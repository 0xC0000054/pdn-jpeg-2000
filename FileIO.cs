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

using Jpeg2000Filetype.Properties;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Security;

namespace Jpeg2000Filetype
{
	internal static class FileIO
	{
		internal enum CodecError : int
		{
			Ok = 1,
			InitFailed = 0,
			OutOfMemory = -1,
			UnknownFormat = -2,
			DecodeFailure = -3,
			TooManyComponents = -4,
			ProfileCreation = -5,
			ProfileConversion = -6,
			ImageBufferWrite = -7,
			EncodeFailure = -8
		}

		[StructLayout(LayoutKind.Sequential)]
		internal struct ImageData
		{
			public int width;
			public int height;
			public int channels;
			[MarshalAs(UnmanagedType.U1)]
			public bool hasAlpha;
			public IntPtr data;
			public double dpcmX;
			public double dpcmY;
		}

		[StructLayout(LayoutKind.Sequential)]
		internal struct EncodeParams
		{
			public int quality;
			public double dpcmX;
			public double dpcmY;
		}

		[UnmanagedFunctionPointer(CallingConvention.StdCall)]
		private delegate int ReadDelegate(IntPtr buffer, int count);
		[UnmanagedFunctionPointer(CallingConvention.StdCall)]
		private delegate int WriteDelegate(IntPtr buffer, int count);
		[UnmanagedFunctionPointer(CallingConvention.StdCall)]
		private delegate int SeekDelegate(int offset, int origin);

		[StructLayout(LayoutKind.Sequential)]
		private sealed class IOCallbacks
		{
			[MarshalAs(UnmanagedType.FunctionPtr)]
			public ReadDelegate Read;
			[MarshalAs(UnmanagedType.FunctionPtr)]
			public WriteDelegate Write;
			[MarshalAs(UnmanagedType.FunctionPtr)]
			public SeekDelegate Seek;
		}

		[SuppressUnmanagedCodeSecurity]
		private static class IO_x86
		{
			[DllImport("Jpeg2000IO_x86.dll", CallingConvention = CallingConvention.StdCall)]
			internal static extern CodecError DecodeFile(IOCallbacks callbacks, out ImageData output);

			[DllImport("Jpeg2000IO_x86.dll", CallingConvention = CallingConvention.StdCall)]
			internal static extern CodecError EncodeFile(
				IntPtr inData,
				int width,
				int height,
				int stride,
				int channelCount,
				EncodeParams parameters,
				IOCallbacks callbacks);

			[DllImport("Jpeg2000IO_x86.dll", CallingConvention = CallingConvention.StdCall)]
			internal static extern void FreeImageData(ref ImageData data);
		}

		[SuppressUnmanagedCodeSecurity]
		private static class IO_x64
		{
			[DllImport("Jpeg2000IO_x64.dll", CallingConvention = CallingConvention.StdCall)]
			internal static extern CodecError DecodeFile(IOCallbacks callbacks, out ImageData output);

			[DllImport("Jpeg2000IO_x64.dll", CallingConvention = CallingConvention.StdCall)]
			internal static extern CodecError EncodeFile(
				IntPtr inData,
				int width,
				int height,
				int stride,
				int channelCount,
				EncodeParams parameters,
				IOCallbacks callbacks);

			[DllImport("Jpeg2000IO_x64.dll", CallingConvention = CallingConvention.StdCall)]
			internal static extern void FreeImageData(ref ImageData data);
		}

		public static unsafe CodecError DecodeFile(Stream input, out ImageData output)
		{
			StreamIOCallbacks streamCallbacks = new StreamIOCallbacks(input);
			IOCallbacks callbacks = new IOCallbacks()
			{
				Read = new ReadDelegate(streamCallbacks.Read),
				Write = new WriteDelegate(streamCallbacks.Write),
				Seek = new SeekDelegate(streamCallbacks.Seek)
			};

			CodecError result;
			if (IntPtr.Size == 8)
			{
				result = IO_x64.DecodeFile(callbacks, out output);
			}
			else
			{
				result = IO_x86.DecodeFile(callbacks, out output);
			}

			GC.KeepAlive(callbacks);
			GC.KeepAlive(streamCallbacks);

			return result;
		}

		public static void FreeImageData(ref ImageData data)
		{
			if (IntPtr.Size == 8)
			{
				IO_x64.FreeImageData(ref data);
			}
			else
			{
				IO_x86.FreeImageData(ref data);
			}
		}

		public static void EncodeFile(IntPtr inData, int width, int height, int stride, int channelCount, EncodeParams parameters, Stream output)
		{
			StreamIOCallbacks streamCallbacks = new StreamIOCallbacks(output);
			IOCallbacks callbacks = new IOCallbacks()
			{
				Read = new ReadDelegate(streamCallbacks.Read),
				Write = new WriteDelegate(streamCallbacks.Write),
				Seek = new SeekDelegate(streamCallbacks.Seek)
			};

			CodecError result;
			if (IntPtr.Size == 8)
			{
				result = IO_x64.EncodeFile(inData, width, height, stride, channelCount, parameters, callbacks);
			}
			else
			{
				result = IO_x86.EncodeFile(inData, width, height, stride, channelCount, parameters, callbacks);
			}
			GC.KeepAlive(callbacks);
			GC.KeepAlive(streamCallbacks);

			if (result != CodecError.Ok)
			{
				string message = string.Empty;
				switch (result)
				{
					case CodecError.InitFailed:
						message = Resources.InitFailed;
						break;
					case CodecError.OutOfMemory:
						throw new OutOfMemoryException();
					case CodecError.EncodeFailure:
						message = Resources.EncodeFailure;
						break;
					case CodecError.ImageBufferWrite:
						message = Resources.ImageBufferWrite;
						break;
				}

				throw new FormatException(message);
			}
		}

		private sealed class StreamIOCallbacks
		{
			private Stream stream;

			public StreamIOCallbacks(Stream stream)
			{
				this.stream = stream;
			}

			public int Read(IntPtr buffer, int count)
			{
				byte[] bytes = new byte[count];

				int bytesRead = stream.Read(bytes, 0, count);
				Marshal.Copy(bytes, 0, buffer, count);

				return bytesRead;
			}

			public int Write(IntPtr buffer, int count)
			{
				byte[] bytes = new byte[count];
				Marshal.Copy(buffer, bytes, 0, count);

				stream.Write(bytes, 0, count);

				return count;
			}

			public int Seek(int offset, int origin)
			{
				return (int)stream.Seek(offset, (SeekOrigin)origin);
			}
		}
	}
}
