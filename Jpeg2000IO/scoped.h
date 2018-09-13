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

#pragma once

#include "jasper\jasper.h"
#include <memory>

struct jas_image_deleter
{
	void operator()(jas_image_t* image)
	{
		if (image)
		{
			jas_image_destroy(image);
		}
	}
};

typedef std::unique_ptr<jas_image_t, jas_image_deleter> ScopedJasPerImage;

struct jas_matrix_deleter
{
	void operator()(jas_matrix_t* matrix)
	{
		if (matrix)
		{
			jas_matrix_destroy(matrix);
		}
	}
};

typedef std::unique_ptr<jas_matrix_t, jas_matrix_deleter> ScopedJasPerMatrix;

struct jas_stream_deleter
{
	void operator()(jas_stream_t* stream)
	{
		if (stream)
		{
			jas_stream_close(stream);
		}
	}
};

typedef std::unique_ptr<jas_stream_t, jas_stream_deleter> ScopedJasPerStream;

struct jas_cmprof_deleter
{
	void operator()(jas_cmprof_t* profile)
	{
		if (profile)
		{
			jas_cmprof_destroy(profile);
		}
	}
};

typedef std::unique_ptr<jas_cmprof_t, jas_cmprof_deleter> ScopedJasPerColorProfile;
