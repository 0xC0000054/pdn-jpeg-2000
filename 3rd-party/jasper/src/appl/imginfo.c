/*
 * Copyright (c) 2001-2003 Michael David Adams.
 * All rights reserved.
 */

/* __START_OF_JASPER_LICENSE__
 * 
 * JasPer License Version 2.0
 * 
 * Copyright (c) 2001-2006 Michael David Adams
 * Copyright (c) 1999-2000 Image Power, Inc.
 * Copyright (c) 1999-2000 The University of British Columbia
 * 
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person (the
 * "User") obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 * 
 * 1.  The above copyright notices and this permission notice (which
 * includes the disclaimer below) shall be included in all copies or
 * substantial portions of the Software.
 * 
 * 2.  The name of a copyright holder shall not be used to endorse or
 * promote products derived from the Software without specific prior
 * written permission.
 * 
 * THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL PART OF THIS
 * LICENSE.  NO USE OF THE SOFTWARE IS AUTHORIZED HEREUNDER EXCEPT UNDER
 * THIS DISCLAIMER.  THE SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 * INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  NO ASSURANCES ARE
 * PROVIDED BY THE COPYRIGHT HOLDERS THAT THE SOFTWARE DOES NOT INFRINGE
 * THE PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF ANY OTHER ENTITY.
 * EACH COPYRIGHT HOLDER DISCLAIMS ANY LIABILITY TO THE USER FOR CLAIMS
 * BROUGHT BY ANY OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL
 * PROPERTY RIGHTS OR OTHERWISE.  AS A CONDITION TO EXERCISING THE RIGHTS
 * GRANTED HEREUNDER, EACH USER HEREBY ASSUMES SOLE RESPONSIBILITY TO SECURE
 * ANY OTHER INTELLECTUAL PROPERTY RIGHTS NEEDED, IF ANY.  THE SOFTWARE
 * IS NOT FAULT-TOLERANT AND IS NOT INTENDED FOR USE IN MISSION-CRITICAL
 * SYSTEMS, SUCH AS THOSE USED IN THE OPERATION OF NUCLEAR FACILITIES,
 * AIRCRAFT NAVIGATION OR COMMUNICATION SYSTEMS, AIR TRAFFIC CONTROL
 * SYSTEMS, DIRECT LIFE SUPPORT MACHINES, OR WEAPONS SYSTEMS, IN WHICH
 * THE FAILURE OF THE SOFTWARE OR SYSTEM COULD LEAD DIRECTLY TO DEATH,
 * PERSONAL INJURY, OR SEVERE PHYSICAL OR ENVIRONMENTAL DAMAGE ("HIGH
 * RISK ACTIVITIES").  THE COPYRIGHT HOLDERS SPECIFICALLY DISCLAIM ANY
 * EXPRESS OR IMPLIED WARRANTY OF FITNESS FOR HIGH RISK ACTIVITIES.
 * 
 * __END_OF_JASPER_LICENSE__
 */

/*
 * Image Information Program
 *
 * $Id$
 */

/******************************************************************************\
* Includes.
\******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <assert.h>

#include <jasper/jasper.h>
#include "jp2_cod.h"


/******************************************************************************\
*
\******************************************************************************/

typedef enum {
	OPT_HELP,
	OPT_VERSION,
	OPT_VERBOSE,
	OPT_INFILE
} optid_t;

/******************************************************************************\
*
\******************************************************************************/

static void usage(void);
static void cmdinfo(void);

/******************************************************************************\
*
\******************************************************************************/

static jas_opt_t opts[] = {
	{OPT_HELP, "help", 0},
	{OPT_VERSION, "version", 0},
	{OPT_VERBOSE, "verbose", 0},
	{OPT_INFILE, "f", JAS_OPT_HASARG},
	{-1, 0, 0}
};

static char *cmdname = 0;

/******************************************************************************\
* Main program.
\******************************************************************************/

int main(int argc, char **argv)
{
	int fmtid;
	int id;
	char *infile;
	jas_stream_t *instream;
	jas_image_t *image;
	int width;
	int height;
	int depth;
	int numcmpts;
	int verbose;
	char *fmtname;
	jp2_box_t *box = NULL;
	double vres, hres, powv, powh, resv, resh;


	if (jas_init()) {
		abort();
	}

	cmdname = argv[0];

	infile = 0;
	verbose = 0;

	/* Parse the command line options. */
	while ((id = jas_getopt(argc, argv, opts)) >= 0) {
		switch (id) {
		case OPT_VERBOSE:
			verbose = 1;
			break;
		case OPT_VERSION:
			printf("%s\n", JAS_VERSION);
			exit(EXIT_SUCCESS);
			break;
		case OPT_INFILE:
			infile = jas_optarg;
			break;
		case OPT_HELP:
		default:
			usage();
			break;
		}
	}

	/* Open the image file. */
	if (infile) {
		/* The image is to be read from a file. */
		wchar_t file[300];
		mbstowcs(file, infile, 300);
		if (!(instream = jas_stream_fopen(file, "rb"))) {
			fprintf(stderr, "cannot open input image file %s\n", infile);
			exit(EXIT_FAILURE);
		}
	} else {
		/* The image is to be read from standard input. */
		if (!(instream = jas_stream_fdopen(0, "rb"))) {
			fprintf(stderr, "cannot open standard input\n");
			exit(EXIT_FAILURE);
		}
	}

	if ((fmtid = jas_image_getfmt(instream)) < 0) {
		fprintf(stderr, "unknown image format\n");
	}

	/* Decode the image. */
	if (!(image = jas_image_decode(instream, fmtid, 0))) {
		fprintf(stderr, "cannot load image\n");
		return EXIT_FAILURE;
	}

	hres = vres = resh = resv = powh = powv = 0.0;
	if (strcmp(jas_image_fmttostr(fmtid), "jp2") == 0)
	{	
		jas_stream_rewind(instream);
	
		while ((box = jp2_box_get(instream)) != 0 && box->type != JP2_BOX_JP2C)
		{
			if (box->type == JP2_BOX_RESC)
			{
				jp2_resc_t res = box->data.resc;

				if (res.VRcE < 0)
				{
					powv = 10.0 / (double)res.VRcE;
				}
				else
				{
					powv = pow(10.0, (double)res.VRcE);
				}
			
				if (res.HRcE < 0)
				{
					powh = 10.0 / (double)res.HRcE;
				}
				else
				{
					powh = pow(10.0, (double)res.HRcE);
				}

				vres = ((double)res.VRcN / (double)res.VRcD) * powv;
				hres = ((double)res.HRcN / (double)res.HRcD) * powh;
			}
			else if (box->type == JP2_BOX_RESD)
			{
				jp2_resd_t res = box->data.resd;

				if (res.VRcE < 0)
				{
					powv = 10.0 / (double)res.VRcE;
				}
				else
				{
					powv = pow(10.0, (double)res.VRcE);
				}
			
				if (res.HRcE < 0)
				{
					powh = 10.0 / (double)res.HRcE;
				}
				else
				{
					powh = pow(10.0, (double)res.HRcE);
				}

				vres = ((double)res.VRcN / (double)res.VRcD) * powv;
				hres = ((double)res.HRcN / (double)res.HRcD) * powh;
			}
		}
	}

		resv = vres / 100;
		resh = hres / 100;

		depth = 8;
			// <LD> 01/Jan/2005: Always force conversion to sRGB. Seems to be required for many types of JPEG2000 file.
		// if (depth!=1 && depth!=4 && depth!=8)
		if (image->numcmpts_>=3 && depth <=8 && image->clrspc_ != JAS_CLRSPC_SRGB && image->cmprof_ != NULL)
		{
			jas_image_t *newimage;
			jas_cmprof_t *inprof = NULL;
			jas_cmprof_t *outprof;
			//jas_eprintf("forcing conversion to sRGB\n");
			outprof = jas_cmprof_createfromclrspc(JAS_CLRSPC_SRGB);
			if (!outprof) {
				//throw((int)errProfileCreation);
			}

			newimage = jas_image_chclrspc(image, outprof, JAS_CMXFORM_INTENT_PER);
			if (!newimage) {
				jas_cmprof_destroy(outprof); // <LD> 01/Jan/2005: Destroy color profile on error.
				//throw((int)errProfileConversion);
			}
			jas_image_destroy(image);
			jas_cmprof_destroy(outprof);
			image = newimage;
		}


	/* Close the image file. */
	jas_stream_close(instream);

	numcmpts = jas_image_numcmpts(image);
	width = jas_image_cmptwidth(image, 0);
	height = jas_image_cmptheight(image, 0);
	depth = jas_image_cmptprec(image, 0);

	if (!(fmtname = jas_image_fmttostr(fmtid))) {
		abort();
	}
	
	printf("%s %d %d %d %d %3.2f %3.2f %ld\n", fmtname, numcmpts, width, height, depth, resh, resv, (long) jas_image_rawsize(image));

	jas_image_destroy(image);
	jas_image_clearfmts();

	return EXIT_SUCCESS;
}

/******************************************************************************\
*
\******************************************************************************/

static void cmdinfo()
{
	fprintf(stderr, "Image Information Utility (Version %s).\n",
	  JAS_VERSION);
	fprintf(stderr,
	  "Copyright (c) 2001 Michael David Adams.\n"
	  "All rights reserved.\n"
	  );
}

static void usage()
{
	cmdinfo();
	fprintf(stderr, "usage:\n");
	fprintf(stderr,"%s ", cmdname);
	fprintf(stderr, "[-f image_file]\n");
	exit(EXIT_FAILURE);
}
