/*
 *	Convert pix file to PNG (Portable Network Graphics) format
 *
 *	Author -
 *		John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *      Re-distribution of this software is restricted, as described in
 *      your "Statement of Terms and Conditions for the Release of
 *      The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *      This software is Copyright (C) 1998-2004 by the United States Army
 *      in all countries except the USA.  All rights reserved.
 */


#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>
#include <math.h>
#include "zlib.h"
#include "pngconf.h"
#include "png.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"

static int	file_width = 512;		/* default input width */
static int	file_height = 512;		/* default input height */
static int	autosize = 0;			/* !0 to autosize input */
static int	fileinput = 0;			/* file of pipe on input? */
static char	*file_name;
static FILE	*infp;

#define BYTESPERPIXEL 3

#define ROWSIZE (file_width * BYTESPERPIXEL)
#define SIZE (file_height * ROWSIZE)

static char usage[] = "\
Usage: pix-png [-a] [-w file_width] [-n file_height]\n\
	[-s square_file_size] [file.pix]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "as:w:n:" )) != EOF )  {
		switch( c )  {
		case 'a':
			autosize = 1;
			break;
		case 's':
			/* square file size */
			file_height = file_width = atoi(optarg);
			autosize = 0;
			break;
		case 'w':
			file_width = atoi(optarg);
			autosize = 0;
			break;
		case 'n':
			file_height = atoi(optarg);
			autosize = 0;
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
		infp = stdin;
	} else {
		file_name = argv[optind];
		if( (infp = fopen(file_name, "r")) == NULL )  {
			perror(file_name);
			(void)fprintf( stderr,
				"pix-png: cannot open \"%s\" for reading\n",
				file_name );
			exit(1);
		}
		fileinput++;
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "pix-png: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	int i;
	unsigned char *scanbuf;
	unsigned char **rows;
	png_structp png_p;
	png_infop info_p;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* autosize input? */
	if( fileinput && autosize ) {
		int	w, h;
		if( bn_common_file_size(&w, &h, file_name, 3) ) {
			file_width = w;
			file_height = h;
		} else {
			fprintf(stderr,"pix-png: unable to autosize\n");
		}
	}

	png_p = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if( !png_p )
		bu_bomb( "Could not create PNG write structure\n" );

	info_p = png_create_info_struct( png_p );
	if( !info_p )
		bu_bomb( "Could not create PNG info structure\n" );

	/* allocate space for the image */
	scanbuf = (unsigned char *)bu_calloc( SIZE, sizeof( unsigned char ), "scanbuf" );

	/* create array of pointers to rows for libpng */
	rows = (unsigned char **)bu_calloc( file_height, sizeof( unsigned char *), "rows" );
	for( i=0 ; i<file_height ; i++ )
		rows[i] = scanbuf + ((file_height-i-1)*ROWSIZE);

	/* read the pix file */
	if( fread( scanbuf, SIZE, 1, infp ) != 1 )
		bu_bomb( "pix-png: Short read\n");

	png_init_io( png_p, stdout );
	png_set_filter( png_p, 0, PNG_FILTER_NONE );
	png_set_compression_level( png_p, Z_BEST_COMPRESSION );
	png_set_IHDR( png_p, info_p, file_width, file_height, 8,
		PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );

	/*
	 *  From the PNG 1.0 Specification:
	 *	the gAMA chunk specifies the gamma characteristic
	 *	of the source device.
	 *
	 *  In this interpretation, we set the value to 1.0;
	 *  indicating we hadn't done any gamma correction.
	 *
	 *  From the PNG 1.1 Specification:
	 *
	 *	PNG images can specify, via the gAMA chunk, the
	 *	power function relating the desired display output
	 *	with the image samples.
	 *
	 *  In this interpretation, we set the value to 0.6,
	 *  representing the value needed to un-do the 2.2 correction
	 *  auto-applied by PowerPoint for PC monitors.
	 */
	png_set_gAMA( png_p, info_p, 0.6 );

	png_write_info( png_p, info_p );
	png_write_image( png_p, rows );
	png_write_end( png_p, NULL );

	return 0;
}
