/*
 *			D B _ O P E N . C
 *
 * Functions -
 *	db_open		Open the database
 *	db_create	Create a new database
 *	db_close	Close a database, releasing dynamic memory
 *
 *
 *  Authors -
 *	Michael John Muuss
 *	Robert Jon Reschly Jr.
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <fcntl.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#ifdef unix
# include <sys/types.h>
# include <sys/stat.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "db.h"

#include "./debug.h"

/*
 *  This constant determines what the maximum size database is that
 *  will be buffered entirely in memory.
 *  Architecture constraints suggest different values for each vendor.
 */
#ifdef CRAY1
#	define	INMEM_LIM	1*1024*1024	/* includes XMP */
#endif
#ifdef CRAY2
#	define	INMEM_LIM	32*8*1024*1024
#endif
#ifdef sun
#	define	INMEM_LIM	1*1024*1024
#endif
#ifdef gould
#	define	INMEM_LIM	1*1024*1024
#endif
#ifdef vax
#	define	INMEM_LIM	8*1024*1024
#endif
#ifdef mips
#	define	INMEM_LIM	2*1024*1024
#endif
#if !defined(INMEM_LIM)
#	define	INMEM_LIM	1*1024*1024	/* default */
#endif

/*
 *  			D B _ O P E N
 *
 *  Open the named database.
 *  The 'mode' parameter specifies read-only or read-write mode.
 *
 *  Returns:
 *	DBI_NULL	error
 *	db_i *		success
 */
struct db_i *
db_open( name, mode )
char	*name;
char	*mode;
{
	register struct db_i	*dbip;
	register int		i;
#if unix
	struct stat		sb;
#endif

	if(rt_g.debug&DEBUG_DB) rt_log("db_open(%s, %s)\n", name, mode );

	GETSTRUCT( dbip, db_i );
	dbip->dbi_magic = DBI_MAGIC;

#if unix
	if( stat( name, &sb ) < 0 )
		goto fail;
#endif

	if( mode[0] == 'r' && mode[1] == '\0' )  {
		/* Read-only mode */
#		if unix
			if( (dbip->dbi_fd = open( name, O_RDONLY )) < 0 )
				goto fail;
			if( (dbip->dbi_fp = fdopen( dbip->dbi_fd, "r" )) == NULL )
				goto fail;
			if( sb.st_size <= INMEM_LIM )  {
				dbip->dbi_inmem = rt_malloc( sb.st_size,
					"in-memory database" );
				if( read( dbip->dbi_fd, dbip->dbi_inmem,
				    sb.st_size ) != sb.st_size )
					goto fail;
			}
#		else
			if( (dbip->dbi_fp = fopen( name, "r")) == NULL )
				goto fail;
			dbip->dbi_fd = -1;
#		endif
		dbip->dbi_read_only = 1;
	}  else  {
		/* Read-write mode */
#		if unix
			if( (dbip->dbi_fd = open( name, O_RDWR )) < 0 )
				goto fail;
			if( (dbip->dbi_fp = fdopen( dbip->dbi_fd, "r+w" )) == NULL )
				goto fail;
#		else
			if( (dbip->dbi_fp = fopen( name, "r+w")) == NULL )
				goto fail;
			dbip->dbi_fd = -1;
#		endif
		dbip->dbi_read_only = 0;
	}

	for( i=0; i<RT_DBNHASH; i++ )
		dbip->dbi_Head[i] = DIR_NULL;

	dbip->dbi_eof = -1L;
	dbip->dbi_localunit = 0;		/* mm */
	dbip->dbi_local2base = 1.0;
	dbip->dbi_base2local = 1.0;
	dbip->dbi_title = (char *)0;
	dbip->dbi_filename = rt_strdup(name);

	return(dbip);
fail:
	rt_free( (char *)dbip, "struct db_i" );
	return( DBI_NULL );
}

/*
 *			D B _ C R E A T E
 *
 *  Create a new database containing just an IDENT record,
 *  regardless of whether it previously existed or not,
 *  and open it for reading and writing.
 *
 *
 *  Returns:
 *	DBI_NULL	error
 *	db_i *		success
 */
struct db_i *
db_create( name )
char *name;
{
	union record new;

	if(rt_g.debug&DEBUG_DB) rt_log("db_create(%s, %s)\n", name );

	/* Prepare the IDENT record */
	bzero( (char *)&new, sizeof(new) );
	new.i.i_id = ID_IDENT;
	new.i.i_units = ID_MM_UNIT;
	strncpy( new.i.i_version, ID_VERSION, sizeof(new.i.i_version) );
	strcpy( new.i.i_title, "Untitled MGED Database" );

#	if unix
	{
		int	fd;
		if( (fd = creat(name, 0644)) < 0 ||
		    write( fd, (char *)&new, sizeof(new) ) != sizeof(new) )
			return(DBI_NULL);
		(void)close(fd);
	}
#	else
	{
		FILE	*fp;
		if( (fp = fopen( name, "w" )) == NULL )
			return(DBI_NULL);
		(void)fwrite( (char *)&new, 1, sizeof(new), fp );
		(void)fclose(fp);
	}
#	endif

	return( db_open( name, "r+w" ) );
}

/*
 *			D B _ C L O S E
 *
 *  Close a database, releasing dynamic memory
 */
void
db_close( dbip )
register struct db_i	*dbip;
{
	register int		i;
	register struct directory *dp, *nextdp;

	if( dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_close:  bad dbip\n");
	if(rt_g.debug&DEBUG_DB) rt_log("db_close(%s) x%x\n",
		dbip->dbi_filename, dbip );

#if unix
	(void)close( dbip->dbi_fd );
#endif
	fclose( dbip->dbi_fp );
	if( dbip->dbi_title )
		rt_free( dbip->dbi_title, "dbi_title" );
	if( dbip->dbi_filename )
		rt_free( dbip->dbi_filename, "dbi_filename" );

	/* Free all directory entries */
	for( i=0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; )  {
			nextdp = dp->d_forw;
			rt_free( (char *)dp, "dir");
			dp = nextdp;
		}
		dbip->dbi_Head[i] = DIR_NULL;	/* sanity*/
	}

	rt_free( (char *)dbip, "struct db_i" );
}
