////////////////////////////////////////////////////////////////////////
//
// This file is part of pdn-jpeg-2000, a FileType plugin for Paint.NET
// that loads and saves JPEG 2000 images.
//
// Copyright (c) 2012-2017 Nicholas Hayes
//
// This file is licensed under the MIT License.
// See LICENSE.txt for complete licensing and attribution information.
//
////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Jpeg2000IO.h"
#include "jasper\jasper.h"
#include "jp2_cod.h"

namespace
{
	int ReadOp(jas_stream_obj_t* obj, unsigned char* buf, int cnt)
	{
		IOCallbacks* callbacks = reinterpret_cast<IOCallbacks*>(obj);

		return callbacks->Read(buf, cnt);
	}

	int WriteOp(jas_stream_obj_t* obj, unsigned char* buf, int cnt)
	{
		IOCallbacks* callbacks = reinterpret_cast<IOCallbacks*>(obj);

		return callbacks->Write(buf, cnt);
	}

	long SeekOp(jas_stream_obj_t* obj, long offset, int origin)
	{
		IOCallbacks* callbacks = reinterpret_cast<IOCallbacks*>(obj);

		return callbacks->Seek(offset, origin);
	}

	int CloseOp(jas_stream_obj_t* obj)
	{
		return 0;
	}
}

int __stdcall DecodeFile(unsigned char *input, int inLen, ImageData* output)
{
	jas_image_t* image = nullptr;
	jas_matrix_t** bufs = nullptr;
	int i, x, y;

	int err = errOk;

	if (jas_init())
		return errInitFailure;

	jas_stream_t* in = jas_stream_memopen(input, inLen);
	if (!in)
		return errOutOfMemory;

	try
	{
		int format = jas_image_getfmt(in);
		if (format < 0)
		{
			throw((int)errUnknownFormat);
		}

		jas_image_fmtinfo_t* info = jas_image_lookupfmtbyid(format);
		if (!info)
		{
			throw((int)errUnknownFormat);
		}

		/* Decode the image. */
		image = (*info->ops.decode)(in, nullptr);
		if (!image)
		{
			throw((int)errDecodeFailure);
		}

		output->dpcmX = output->dpcmY = 0.0;

		if (strcmp(info->name, "jp2") == 0)
		{
			jas_stream_rewind(in);
			bool done = false;

			jp2_box_t *box = nullptr;

			while(box = jp2_box_get(in))
			{
				switch(box->type)
				{
				case JP2_BOX_RESC:
					if (box->data.resc.VRcE >= 0 && box->data.resc.HRcE >= 0) // pow does not work with negative exponents.
					{
						jp2_resc_t* resc = &box->data.resc;

						double hres = (static_cast<double>(resc->HRcN) / static_cast<double>(resc->HRcD)) * pow(10.0, static_cast<double>(resc->HRcE));
						double vres = (static_cast<double>(resc->VRcN) / static_cast<double>(resc->VRcD)) * pow(10.0, static_cast<double>(resc->VRcE));

						// convert pixels per meter to pixels per centimeter
						output->dpcmX = hres / 100.0;
						output->dpcmY = vres / 100.0;
					}
					break;
				case JP2_BOX_JP2C:
					done = true;
					break;
				}

				if (box)
				{
					jp2_box_destroy(box);
					box = nullptr;
				}

				if (done)
				{
					break;
				}
			}
		}

		/* Create a color profile if needed. */
		if (!jas_clrspc_isunknown(image->clrspc_) &&
		    !jas_clrspc_isgeneric(image->clrspc_) &&
			!image->cmprof_)
		{
			image->cmprof_ = jas_cmprof_createfromclrspc(jas_image_clrspc(image));
			if (!image->cmprof_)
			{
				throw((int)errProfileCreation);
			}
		}

		int width = jas_image_cmptwidth(image, 0);
		int height = jas_image_cmptheight(image, 0);
		int depth = jas_image_cmptprec(image, 0);

		if (image->numcmpts_ > 64 || image->numcmpts_ < 0)
		{
			throw((int)errTooManyComponents);
		}

		output->width = width;
		output->height = height;

		// <LD> 01/Jan/2005: Always force conversion to sRGB. Seems to be required for many types of JPEG2000 file.
		if (image->numcmpts_ >= 3 && depth <= 8 && image->clrspc_ != JAS_CLRSPC_SRGB && image->cmprof_ != nullptr)
		{
			jas_cmprof_t* outprof = jas_cmprof_createfromclrspc(JAS_CLRSPC_SRGB);
			if (!outprof)
			{
				throw((int)errProfileCreation);
			}

			jas_image_t* newimage = jas_image_chclrspc(image, outprof, JAS_CMXFORM_INTENT_PER);
			if (!newimage)
			{
				jas_cmprof_destroy(outprof); // <LD> 01/Jan/2005: Destroy color profile on error.
				throw((int)errProfileConversion);
			}
			jas_image_destroy(image);
			jas_cmprof_destroy(outprof);
			image = newimage;
		}

		output->channels = image->numcmpts_;

		bufs = reinterpret_cast<jas_matrix_t**>(calloc(image->numcmpts_, sizeof(jas_matrix_t**)));
		if (!bufs)
		{
			throw((int)errOutOfMemory);
		}

		for (int i = 0; i < image->numcmpts_; ++i) {
			bufs[i] = jas_matrix_create(1, width);
			if (!bufs[i]) {
				throw((int)errOutOfMemory);
			}
		}

		int shift = 0;
		if (depth > 8)
		{
			shift = depth - 8;
		}

		int outLen, stride, bpp, index0, index1, index2;

		const int alphaIndex = jas_image_getcmptbytype(image, JAS_IMAGE_CT_OPACITY);

		const bool hasAlpha = alphaIndex >= 0;

		output->hasAlpha = hasAlpha;

		switch (jas_clrspc_fam(image->clrspc_))
		{
			case JAS_CLRSPC_FAM_RGB:

				bpp = hasAlpha ? 4 : 3;

				stride = width * bpp;
				outLen = stride * height;
				output->data = HeapAlloc(GetProcessHeap(), 0, outLen);

				if (!output->data)
				{
					throw((int)errOutOfMemory);
				}

				index0 = jas_image_getcmptbytype(image, JAS_IMAGE_CT_RGB_R);
				index1 = jas_image_getcmptbytype(image, JAS_IMAGE_CT_RGB_G);
				index2 = jas_image_getcmptbytype(image, JAS_IMAGE_CT_RGB_B);

				for (y = 0; y < height; y++)
				{
					jas_image_readcmpt(image, index0, 0, y, width, 1, bufs[0]);
					jas_image_readcmpt(image, index1, 0, y, width, 1, bufs[1]);
					jas_image_readcmpt(image, index2, 0, y, width, 1, bufs[2]);

					if (hasAlpha)
					{
						jas_image_readcmpt(image, alphaIndex, 0, y, width, 1, bufs[3]);
					}

					BYTE* data = reinterpret_cast<BYTE*>(output->data) + (y * stride);
					for (x = 0; x < width; x++){
						data[0] = static_cast<BYTE>((jas_matrix_getv(bufs[0], x)>>shift));
						data[1] = static_cast<BYTE>((jas_matrix_getv(bufs[1], x)>>shift));
						data[2] = static_cast<BYTE>((jas_matrix_getv(bufs[2], x)>>shift));

						if (hasAlpha)
						{
							data[3] = static_cast<BYTE>((jas_matrix_getv(bufs[3], x)>>shift));
						}

						data += bpp;
					}
				}

				break;
			case JAS_CLRSPC_FAM_GRAY:

				bpp = hasAlpha ? 2 : 1;

				stride = width * bpp;
				outLen = stride * height;
				output->data = HeapAlloc(GetProcessHeap(), 0, outLen);

				if (!output->data)
				{
					throw((int)errOutOfMemory);
				}

				width = jas_image_cmptwidth(image, 0);
				height = jas_image_cmptheight(image, 0);
				depth = jas_image_cmptprec(image, 0);

				index0 = jas_image_getcmptbytype(image, JAS_IMAGE_CT_GRAY_Y);

				for (y = 0; y < height; y++) {
					jas_image_readcmpt(image, index0, 0, y, width, 1, bufs[0]);

					if (hasAlpha)
					{
						jas_image_readcmpt(image, alphaIndex, 0, y, width, 1, bufs[1]);
					}

					BYTE* data = reinterpret_cast<BYTE*>(output->data) + (y * width);
					for (x = 0; x < width; x++){
						data[0] = static_cast<BYTE>((jas_matrix_getv(bufs[0], x)>>shift));

						if (hasAlpha)
						{
							data[1] = static_cast<BYTE>((jas_matrix_getv(bufs[1], x)>>shift));
						}

						data++;
					}
				}

				break;
			default:
				err = errUnknownFormat;
				break;
		}
	}
	catch (int error)
	{
		err = error;
	}

	if (bufs) {
		for (i = 0; i < image->numcmpts_; ++i){	if (bufs[i]) jas_matrix_destroy(bufs[i]);}
		free(bufs);
	}
	jas_cleanup();
	if (image) jas_image_destroy(image);
	if (in) jas_stream_close(in);

	return err;
}

void __stdcall FreeImageData(ImageData* image)
{
	if (image != nullptr && image->data != nullptr)
	{
		HeapFree(GetProcessHeap(), 0, image->data);
		image->data = nullptr;
		image->channels = 0;
		image->width = 0;
		image->height = 0;
		image->hasAlpha = false;
		image->dpcmX = 0.0;
		image->dpcmY = 0.0;
	}
}

int __stdcall EncodeFile(void* inData, int width, int height, int stride, int channelCount, EncodeParams params, IOCallbacks* callbacks)
{
	jas_image_t* image = nullptr;
	jas_stream_t* out = nullptr;
	jas_matrix_t* cmpts[4] = { nullptr, nullptr, nullptr, nullptr };
	jas_image_cmptparm_t cmptparms[4];
	int i, x, y;

	int format = 0, error = errOk;

	if (jas_init())
		return errInitFailure;

	jas_stream_ops_t ops;
	ops.read_ = &ReadOp;
	ops.write_ = &WriteOp;
	ops.seek_ = &SeekOp;
	ops.close_ = &CloseOp;

	out = jas_stream_create_ops(&ops, callbacks);
	if (!out)
		return errOutOfMemory;

	try
	{
		for (i = 0; i < channelCount; i++)
		{
			cmptparms[i].tlx = 0;
			cmptparms[i].tly = 0;
			cmptparms[i].hstep = 1;
			cmptparms[i].vstep = 1;
			cmptparms[i].width = width;
			cmptparms[i].height = height;
			cmptparms[i].prec = 8;
			cmptparms[i].sgnd = false;
		}

		image = jas_image_create(channelCount, cmptparms, JAS_CLRSPC_UNKNOWN);
		if (!image)
			throw((int)errOutOfMemory);

		if (channelCount >= 3)
		{
			jas_image_setclrspc(image, JAS_CLRSPC_SRGB);
			jas_image_setcmpttype(image, 0, JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_R));
			jas_image_setcmpttype(image, 1, JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_G));
			jas_image_setcmpttype(image, 2, JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_B));

			if (channelCount == 4)
			{
				jas_image_setcmpttype(image, 3, JAS_IMAGE_CT_OPACITY);
			}
		}
		else
		{
			jas_image_setclrspc(image, JAS_CLRSPC_SGRAY);
			jas_image_setcmpttype(image, 0,	JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_GRAY_Y));
		}

		for (i = 0; i < channelCount; i++)
		{
			cmpts[i] = jas_matrix_create(1, width);
			if (!cmpts[i])
			{
				throw((int)errOutOfMemory);
			}
		}

		BYTE* scan0 = reinterpret_cast<BYTE*>(inData);

		for (y = 0; y < height; y++)
		{
			BYTE* src = scan0 + (y * stride);
			for (x = 0; x < width; x++)
			{
				if (channelCount >= 3)
				{
					jas_matrix_setv(cmpts[0], x, src[2]); // Paint.NET uses BGR order
					jas_matrix_setv(cmpts[1], x, src[1]);
					jas_matrix_setv(cmpts[2], x, src[0]);

					if (channelCount == 4)
					{
						jas_matrix_setv(cmpts[3], x, src[3]);
					}
				}
				else
				{
					jas_matrix_setv(cmpts[0], x, src[0]);
				}

				src += 4;
			}

			for (i = 0; i < channelCount; i++)
			{
				if (jas_image_writecmpt(image, i, 0, y, width, 1, cmpts[i]))
				{
					throw((int)errImageBufferWrite);
				}
			}

		}

		ZeroMemory(&image->captureRes, sizeof(jas_image_resolution_t));
		if (params.dpcmX > 0.0 && params.dpcmY > 0.0)
		{
			const double dotsPerMeterX = params.dpcmX * 100.0;
			const double dotsPerMeterY = params.dpcmY * 100.0;

			jas_image_resolution_t* res = &image->captureRes;

			uint_fast32_t vRes = static_cast<uint_fast32_t>(floor(dotsPerMeterY * 1000.0));

			res->vNumerator = vRes;
			res->vDenomerator = 1000;
			res->vExponent = 0;

			while (res->vNumerator > UINT_FAST16_MAX)
			{
				res->vNumerator /= 10;
				res->vExponent += 1;
			}

			uint_fast32_t hRes = static_cast<uint_fast32_t>(floor(dotsPerMeterX * 1000.0));

			res->hNumerator = hRes;
			res->hDenomerator = 1000;
			res->hExponent = 0;

			while (res->hNumerator > UINT_FAST16_MAX)
			{
				res->hNumerator /= 10;
				res->hExponent += 1;
			}
		}

		int outFmt = jas_image_strtofmt("jp2");

		char encOps[32];
		ZeroMemory(encOps, sizeof(encOps));

		// JasPer uses lossless compression by default when the rate parameter is not specified.
		if (params.quality < 100)
		{
			sprintf_s(encOps, sizeof(encOps), "rate=%.3f", static_cast<float>(params.quality) / 100.0f);
		}

		if (jas_image_encode(image, out, outFmt, encOps))
		{
			throw((int)errEncodeFailed);
		}

		jas_stream_flush(out);
	}
	catch (int errorCode)
	{
		error = errorCode;
	}

	for (i = 0; i < channelCount; i++)
	{
		if (cmpts[i])
		{
			jas_matrix_destroy(cmpts[i]);
		}
	}

	if (image)
		jas_image_destroy(image);

	if (out)
		jas_stream_close(out);

	return error;
}