/*
	SCCS id:	@(#) sgi_dep.c	2.2
	Modified: 	12/29/86 at 11:20:24
	Retrieved: 	12/30/86 at 17:02:29
	SCCS archive:	/vld/moss/src/fbed/s.sgi_dep.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) sgi_dep.c 2.2, modified 12/29/86 at 11:20:24, archive /vld/moss/src/fbed/s.sgi_dep.c";
#endif
#ifdef sgi
#include <device.h>
#include "fb.h"
void
sgi_Init()
	{
	qdevice( KEYBD );
	}

int
sgi_Getchar()
	{	extern FBIO	*fbp;
	if( fbp != FBIO_NULL && strncmp( fbp->if_name, "/dev/sgi", 8 ) == 0 )
		{	short	val;
		winattach();
		while( qread( &val ) != KEYBD )
			;
		return	(int) val;
		}
	else
		return	getchar();
	}
#endif
