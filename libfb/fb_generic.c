/*
 *			F B _ G E N E R I C
 *
 *  Authors -
 *	Phil Dykstra
 *	Gary S. Moss
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "fb.h"
#include "./fblocal.h"

extern char *getenv();
static int fb_totally_numeric();

/*
 * Disk interface enable flag.  Used so the the remote daemon
 * can turn off the disk interface.
 */
int _fb_disk_enable = 1;

/*
 *		f b _ n u l l
 *
 *  Filler for FBIO function slots not used by a particular device
 */
int fb_null()
{
	return	0;
}

#ifdef IF_REMOTE
extern FBIO remote_interface;	/* not in list[] */
#endif

#ifdef IF_ADAGE
extern FBIO adage_interface;
#endif
#ifdef IF_SUN
extern FBIO sun_interface;
#endif
#if defined(IF_SGI) || defined(IF_4D)
extern FBIO sgi_interface;
#endif
#ifdef IF_RAT
extern FBIO rat_interface;
#endif
#ifdef IF_UG
extern FBIO ug_interface;
#endif
#ifdef IF_X
extern FBIO X_interface;
#endif
#ifdef IF_PTTY
extern FBIO ptty_interface;
#endif
#ifdef IF_AB
extern FBIO abekas_interface;
#endif
#ifdef IF_TS
extern FBIO ts_interface;
#endif

/* Always included */
extern FBIO debug_interface, disk_interface, stk_interface;
extern FBIO memory_interface, null_interface;

/* First element of list is default device when no name given */
static
FBIO *_if_list[] = {
#ifdef IF_ADAGE
	&adage_interface,
#endif
#ifdef IF_SUN
	&sun_interface,
#endif
#if defined(IF_SGI) || defined(IF_4D)
	&sgi_interface,
#endif
#ifdef IF_RAT
	&rat_interface,
#endif
#ifdef IF_UG
	&ug_interface,
#endif
	&debug_interface,
/* never get the following by default */
#ifdef IF_X
	&X_interface,
#endif
#ifdef IF_AB
	&abekas_interface,
#endif
#ifdef IF_TS
	&ts_interface,
#endif
#ifdef IF_PTTY
	&ptty_interface,
#endif
	&stk_interface,
	&memory_interface,
	&null_interface,
	(FBIO *) 0
};

FBIO *
fb_open( file, width, height )
char	*file;
int	width, height;
{
	register FBIO	*ifp;
	int	i;

	if( (ifp = (FBIO *) calloc( sizeof(FBIO), 1 )) == FBIO_NULL ) {
		Malloc_Bomb( sizeof(FBIO) );
		return	FBIO_NULL;
	}
	if( file == NULL || *file == '\0' )  {
		/* No name given, check environment variable first.	*/
		if( (file = getenv( "FB_FILE" )) == NULL || *file == '\0' )  {
			/* None set, use first device as default */
			*ifp = *(_if_list[0]);	/* struct copy */
			file = ifp->if_name;
			goto found_interface;
		}
	}
	/*
	 *  Determine what type of hardware the device name refers to.
	 *
	 *  "file" can in general look like: hostname:/pathname/devname#
	 *  If we have a ':' assume the remote interface
	 *    (We don't check to see if it's us. Good for debugging.)
	 *  else strip out "/path/devname" and try to look it up in the
	 *    device array.  If we don't find it assume it's a file.
	 */
	i = 0;
	while( _if_list[i] != (FBIO *)NULL ) {
		if( strncmp( file, _if_list[i]->if_name,
		    strlen(_if_list[i]->if_name) ) == 0 ) {
		    	/* found it, copy its struct in */
			*ifp = *(_if_list[i]);
			goto found_interface;
		}
		i++;
	}
	/* Not in list, check special interfaces or disk files */
	/* "/dev/" protection! */
	if( strncmp(file, "/dev/", 5) == 0 ) {
		fb_log(	"fb_open: no such device \"%s\".\n", file );
		free( (void *) ifp );
		return	FBIO_NULL;
	}
#ifdef IF_REMOTE
	if( fb_totally_numeric(file) || strchr( file, ':' ) != NULL ) {
		/* We have a remote file name of the form <host>:<file>
		 * or a port number (which assumes localhost) */
		*ifp = remote_interface;
		goto found_interface;
	}
#endif /* IF_REMOTE */
	/* Assume it's a disk file */
	if( _fb_disk_enable ) {
		*ifp = disk_interface;
	} else {
		fb_log(	"fb_open: no such device \"%s\".\n", file );
		free( (void *) ifp );
		return	FBIO_NULL;
	}

found_interface:
	/* Copy over the name it was opened by. */
	if( (ifp->if_name = malloc( (unsigned) strlen( file ) + 1 ))
	    == (char *) NULL
	    )
	{
		Malloc_Bomb( strlen( file ) + 1 );
		free( (void *) ifp );
		return	FBIO_NULL;
	}
	(void) strcpy( ifp->if_name, file );

	if( (i=(*ifp->if_open)( ifp, file, width, height )) <= -1 )  {
		fb_log(	"fb_open: can not open device \"%s\", ret=%d.\n",
			file, i );
		free( (void *) ifp->if_name );
		free( (void *) ifp );
		return	FBIO_NULL;
	}
	return	ifp;
}

int
fb_close( ifp )
FBIO	*ifp;
{
	int	i;

	_fb_pgflush( ifp );
	if( (i=(*ifp->if_close)( ifp )) <= -1 )  {
		fb_log(	"fb_close: can not close device \"%s\", ret=%d.\n",
			ifp->if_name, i );
		return	-1;
	}
	if( ifp->if_pbase != PIXEL_NULL )
		free( (void *) ifp->if_pbase );
	free( (void *) ifp->if_name );
	free( (void *) ifp );
	return	0;
}

/*
 *  Generic Help.
 *  Print out the list of available frame buffers.
 */
int
fb_genhelp()
{
	int	i;

	i = 0;
	while( _if_list[i] != (FBIO *)NULL ) {
		fb_log( "%-12s  %s\n",
			_if_list[i]->if_name,
			_if_list[i]->if_type );
		i++;
	}

	/* Print the ones not in the device list */
#ifdef IF_REMOTE
	fb_log( "%-12s  %s\n",
		remote_interface.if_name,
		remote_interface.if_type );
#endif
	if( _fb_disk_enable ) {
		fb_log( "%-12s  %s\n",
			disk_interface.if_name,
			disk_interface.if_type );
	}

	return	0;
}

/* True if the non-null string s is all digits */
static int
fb_totally_numeric( s )
register char *s;
{
	if( s == (char *)0 || *s == 0 )
		return	0;

	while( *s ) {
		if( *s < '0' || *s > '9' )
			return 0;
		s++;
	}

	return 1;
}

/*
 *			F B _ I S _ L I N E A R _ C M A P
 *
 *  Check for a color map being linear in the upper 8 bits of
 *  R, G, and B.
 *  Returns 1 for linear map, 0 for non-linear map
 *  (ie, non-identity map).
 */
int
fb_is_linear_cmap(cmap)
register ColorMap	*cmap;
{
	register int i;

	for( i=0; i<256; i++ )  {
		if( cmap->cm_red[i]>>8 != i )  return(0);
		if( cmap->cm_green[i]>>8 != i )  return(0);
		if( cmap->cm_blue[i]>>8 != i )  return(0);
	}
	return(1);
}

/*
 *			F B _ M A K E _ L I N E A R _ C M A P
 */
void
fb_make_linear_cmap(cmap)
register ColorMap	*cmap;
{
	register int i;

	for( i=0; i<256; i++ )  {
		cmap->cm_red[i] = i<<8;
		cmap->cm_green[i] = i<<8;
		cmap->cm_blue[i] = i<<8;
	}
}
