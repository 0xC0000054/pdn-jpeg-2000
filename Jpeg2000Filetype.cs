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

using System;
using System.Collections.Generic;
using System.IO;
using Jpeg2000Filetype.Properties;
using PaintDotNet;
using PaintDotNet.IndirectUI;
using PaintDotNet.PropertySystem;

namespace Jpeg2000Filetype
{
	[PluginSupportInfo(typeof(PluginSupportInfo))]
	public sealed class Jpeg2000Filetype : PropertyBasedFileType, IFileTypeFactory
	{
		internal static string StaticName
		{
			get
			{
				return "Jpeg 2000";
			}
		}

		public Jpeg2000Filetype() :
			base(StaticName, FileTypeFlags.SupportsLoading | FileTypeFlags.SupportsSaving, new string[] {".jp2", ".j2c", ".jpg2", ".jpf"})
		{
		}

		public FileType[] GetFileTypeInstances()
		{
			return new FileType[] { new Jpeg2000Filetype() };
		}

		protected unsafe override Document OnLoad(Stream input)
		{

			FileIO.ImageData image;

			FileIO.CodecError result = FileIO.DecodeFile(input, out image);

			if (result == FileIO.CodecError.Ok)
			{
				Document doc = new Document(image.width, image.height);
				BitmapLayer layer = new BitmapLayer(image.width, image.height) { IsBackground = true };

				Surface surf = layer.Surface;

				if (!image.hasAlpha)
				{
					new UnaryPixelOps.SetAlphaChannelTo255().Apply(surf, surf.Bounds);
				}

				byte* scan0 = (byte*)image.data.ToPointer();

				if (image.channels >= 3)
				{
					int stride = image.width * image.channels;
					for (int y = 0; y < image.height; y++)
					{
						ColorBgra* p = surf.GetRowAddressUnchecked(y);
						byte* src = scan0 + (y * stride);

						for (int x = 0; x < image.width; x++)
						{
							p->R = src[0]; // JP2 uses RGB pixel order.
							p->G = src[1];
							p->B = src[2];

							if (image.hasAlpha)
							{
								p->A = src[3];
							}

							p++;
							src += image.channels;
						}
					}
				}
				else
				{
					for (int y = 0; y < image.height; y++)
					{
						ColorBgra* p = surf.GetRowAddressUnchecked(y);
						byte* src = scan0 + (y * image.width);

						for (int x = 0; x < image.width; x++)
						{
							p->R = p->G = p->B = src[0]; // gray scale image

							if (image.hasAlpha)
							{
								p->A = src[1];
							}

							p++;
							src += image.channels;
						}
					}
				}

				if (image.dpcmX > 0.0 && image.dpcmY > 0.0)
				{
					doc.DpuUnit = MeasurementUnit.Centimeter;
					doc.DpuX = image.dpcmX;
					doc.DpuY = image.dpcmY;
				}

				FileIO.FreeImageData(ref image);

				doc.Layers.Add(layer);

				return doc;
			}
			else
			{
				string error = string.Empty;
				switch (result)
				{
					case FileIO.CodecError.InitFailed:
						error = Resources.InitFailed;
						break;
					case FileIO.CodecError.OutOfMemory:
						throw new OutOfMemoryException();
					case FileIO.CodecError.UnknownFormat:
						error = Resources.UnknownFormat;
						break;
					case FileIO.CodecError.DecodeFailure:
						error = Resources.DecodeFailure;
						break;
					case FileIO.CodecError.TooManyComponents:
						error = Resources.TooManyComponets;
						break;
					case FileIO.CodecError.ProfileCreation:
						error = Resources.ProfileCreation;
						break;
					case FileIO.CodecError.ProfileConversion:
						error = Resources.ProfileConversion;
						break;
				}

				throw new FormatException(error);
			}

		}

		private enum PropertyNames
		{
			Quality
		}

		public override PropertyCollection OnCreateSavePropertyCollection()
		{
			List<Property> props = new List<Property> { new Int32Property(PropertyNames.Quality, 95, 0, 100)};


			return new PropertyCollection(props);
		}

		public override ControlInfo OnCreateSaveConfigUI(PropertyCollection props)
		{
			ControlInfo info = PropertyBasedFileType.CreateDefaultSaveConfigUI(props);
			info.SetPropertyControlValue(PropertyNames.Quality, ControlInfoPropertyNames.DisplayName, "Quality");

			return info;
		}

		private static unsafe int CountChannels(Surface scratchSurface)
		{
			HashSet<ColorBgra> uniqueColors = new HashSet<ColorBgra>();

			int width = scratchSurface.Width;
			int height = scratchSurface.Height;

			for (int y = 0; y < height; ++y)
			{
				ColorBgra* srcPtr = scratchSurface.GetRowAddress(y);
				ColorBgra* endPtr = srcPtr + width;

				while (srcPtr < endPtr)
				{
					if (srcPtr->A < 255)
					{
						return 4;
					}

					if (!uniqueColors.Contains(*srcPtr) && uniqueColors.Count < 300)
					{
						uniqueColors.Add(*srcPtr);
					}

					++srcPtr;
				}
			}


			if (uniqueColors.Count <= 256)
			{
				for (int y = 0; y < height; y++)
				{
					ColorBgra* srcPtr = scratchSurface.GetRowAddress(y);
					ColorBgra* endPtr = srcPtr + width;

					while (srcPtr < endPtr)
					{

						if (!(srcPtr->B == srcPtr->G && srcPtr->G == srcPtr->R))
						{
							// The image is RGB.
							return 3;
						}


						++srcPtr;
					}
				}

				return 1;
			}

			return 3;
		}

		protected override void OnSaveT(Document input, Stream output, PropertyBasedSaveConfigToken token, Surface scratchSurface, ProgressEventHandler progressCallback)
		{
			int quality = (int)token.GetProperty(PropertyNames.Quality).Value;

			FileIO.EncodeParams parameters = new FileIO.EncodeParams();
			parameters.quality = quality;

			switch (input.DpuUnit)
			{
				case MeasurementUnit.Centimeter:
					parameters.dpcmX = input.DpuX;
					parameters.dpcmY = input.DpuY;
					break;
				case MeasurementUnit.Inch:
					parameters.dpcmX = Document.DotsPerInchToDotsPerCm(input.DpuX);
					parameters.dpcmY = Document.DotsPerInchToDotsPerCm(input.DpuY);
					break;
				case MeasurementUnit.Pixel:
					parameters.dpcmX = Document.DefaultDpcm;
					parameters.dpcmY = Document.DefaultDpcm;
					break;
			}

			using (RenderArgs ra = new RenderArgs(scratchSurface))
			{
				input.Render(ra, true);
			}

			FileIO.EncodeFile(
				scratchSurface.Scan0.Pointer,
				scratchSurface.Width,
				scratchSurface.Height,
				scratchSurface.Stride,
				CountChannels(scratchSurface),
				parameters,
				output);
		}
	}
}
