////////////////////////////////////////////////////////////////////////
//
// This file is part of pdn-jpeg-2000, a FileType plugin for Paint.NET
// that loads and saves JPEG 2000 images.
//
// Copyright (c) 2012-2018 Nicholas Hayes
//
// This file is licensed under the MIT License.
// See LICENSE.txt for complete licensing and attribution information.
//
////////////////////////////////////////////////////////////////////////

#ifndef JPEG2000_H
#define JPEG2000_H

#ifdef JPEG2000IO_EXPORTS
#define JPEG2000IO_API extern "C" __declspec(dllexport)
#else
#define JPEG2000IO_API __declspec(dllimport)
#endif

typedef int (__stdcall *ReadFn)(void* buffer, int count);
typedef int (__stdcall *WriteFn)(void* buffer, int count);
typedef long (__stdcall *SeekFn)(long offset, int origin);

struct IOCallbacks
{
	ReadFn Read;
	WriteFn Write;
	SeekFn Seek;
};

struct ImageData
{
	int width;
	int height;
	int channels;
	bool hasAlpha;
	void* data;
	double dpcmX;
	double dpcmY;
};

struct EncodeParams
{
	int quality;
	double dpcmX;
	double dpcmY;
};

#define errOk 1
#define errInitFailure  0
#define errOutOfMemory  -1
#define errUnknownFormat -2
#define errDecodeFailure -3
#define errTooManyComponents -4
#define errProfileCreation -5
#define errProfileConversion -6
#define errImageBufferWrite -7
#define errEncodeFailed -8

JPEG2000IO_API int __stdcall DecodeFile(IOCallbacks* callbacks, ImageData* output);
JPEG2000IO_API int __stdcall EncodeFile(void* inData, int width, int height, int stride, int channelCount, EncodeParams params, IOCallbacks* callbacks);
JPEG2000IO_API void __stdcall FreeImageData(ImageData * image);

#endif