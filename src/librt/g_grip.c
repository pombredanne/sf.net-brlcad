/*
 *			G _ G R I P . C
 *  
 *  Function -
 *  	Intersect a ray with a "grip" and return nothing.
 *
 *
 *  A GRIP is defiend by a direction normal, a center and a
 *  height/magnitued vector.  The center is the control point used
 *  for all grip movements.
 *
 *  All Ray intersections return "missed"
 *
 *  The bounding box for a grip is emtpy.
 *  
 *  Authors -
 *	Christopher T. Johnson
 *  
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by Geometric Solutions, Inc.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSgrip[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nmg.h"
#include "db.h"
#include "rtgeom.h"
#include "./debug.h"

struct grip_specific {
	long	grip_magic;
	vect_t	grip_center;
	vect_t	grip_normal;
	fastf_t	grip_mag;
};
#define	GRIP_NULL	((struct grip_specific *)0)

const struct bu_structparse rt_grp_parse[] = {
	{ "%f", 3, "V", offsetof(struct rt_grip_internal, center[X]), BU_STRUCTPARSE_FUNC_NULL },
	{ "%f", 3, "N", offsetof(struct rt_grip_internal, normal[X]), BU_STRUCTPARSE_FUNC_NULL },
	{ "%f", 1, "L", offsetof(struct rt_grip_internal, mag), BU_STRUCTPARSE_FUNC_NULL },
	{ {'\0','\0','\0','\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL }
};

/*
 *  			R T _ G R P _ P R E P
 */
int
rt_grp_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	struct rt_grip_internal *gip;
	register struct grip_specific *gripp;

	gip = (struct rt_grip_internal *)ip->idb_ptr;
	RT_GRIP_CK_MAGIC(gip);

	BU_GETSTRUCT( gripp, grip_specific);
	stp->st_specific = (genptr_t)gripp;

	VMOVE(gripp->grip_normal, gip->normal);
	VMOVE(gripp->grip_center, gip->center);
	gripp->grip_mag = gip->mag;

	/* No bounding sphere or bounding RPP is possible */
	VSETALL( stp->st_min, 0.0);
	VSETALL( stp->st_max, 0.0);

	stp->st_aradius = 0.0;
	stp->st_bradius = 0.0;
	return 0;		/* OK */
}

/*
 *  			R T _ G R P _ P R I N T
 */
void
rt_grp_print(register const struct soltab *stp)
{
	register const struct grip_specific *gripp =
		(struct grip_specific *)stp->st_specific;

	if( gripp == GRIP_NULL )  {
		bu_log("grip(%s):  no data?\n", stp->st_name);
		return;
	}
	VPRINT( "Center", gripp->grip_center);
	VPRINT( "Normal", gripp->grip_normal);
	bu_log( "mag = %f\n", gripp->grip_mag);
}

/*
 *			R T _ G R P _ S H O T
 *  
 * Function -
 *	Shoot a ray at a GRIP
 *
 * Algorithm -
 * 	The intersection distance is computed.
 *  
 * Returns -
 *	0	MISS
 *	>0	HIT
 */
int
rt_grp_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	return 0;	/* this has got to be the easiest */
			/* RT routine I've written */
}

#define RT_HALF_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ G R P _ V S H O T
 *
 *  Vectorizing version.
 */
void
rt_grp_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
             	               /* An array of solid pointers */
           		       /* An array of ray pointers */
                               /* array of segs (results returned) */
   		  	       /* Number of ray/object pairs */
                  	    
{
	return;
}

/*
 *  			R T _ G R P _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 *  The normal is already filled in.
 */
void
rt_grp_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
	rt_bomb("rt_grp_norm: grips should never be hit.\n");
}
/*
 *			R T _ G R P _ C U R V E
 *
 *  Return the "curvature" of the grip.
 */
void
rt_grp_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
	rt_bomb("rt_grp_curve: nobody should be asking for curve of a grip.\n");
}

/*
 *  			R T _ G R P _ U V
 *  
 *  For a hit on a face of an HALF, return the (u,v) coordinates
 *  of the hit point.  0 <= u,v <= 1.
 *  u extends along the Xbase direction
 *  v extends along the "Ybase" direction
 *  Note that a "toroidal" map is established, varying each from
 *  0 up to 1 and then back down to 0 again.
 */
void
rt_grp_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
	rt_bomb("rt_grp_uv: nobody should be asking for UV of a grip.\n");
}

/*
 *			R T _ G R P _ F R E E
 */
void
rt_grp_free(struct soltab *stp)
{
	register struct grip_specific *gripp =
		(struct grip_specific *)stp->st_specific;

	bu_free( (char *)gripp, "grip_specific");
}

int
rt_grp_class(void)
{
	return(0);
}

/*
 *			R T _ G R P _ P L O T
 *
 * We represent a GRIP as a pyramid.  The center describes where
 * the center of the base is.  The normal describes which direction
 * the tip of the pyramid is.  Mag describes the distence from the
 * center to the tip. 1/4 of the width is the length of a base side.
 *
 */
int
rt_grp_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	struct rt_grip_internal	*gip;
	vect_t xbase, ybase;	/* perpendiculars to normal */
	vect_t x1, x2;
	vect_t y1, y2;
	vect_t tip;

	RT_CK_DB_INTERNAL(ip);
	gip = (struct rt_grip_internal *)ip->idb_ptr;
	RT_GRIP_CK_MAGIC(gip);

	/* The use of "x" and "y" here is not related to the axis */
	bn_vec_perp( xbase, gip->normal );
	VCROSS( ybase, xbase, gip->normal );

	/* Arrange for the cross to be 2 meters across */
	VUNITIZE( xbase );
	VUNITIZE( ybase);
	VSCALE( xbase, xbase, gip->mag/4.0 );
	VSCALE( ybase, ybase, gip->mag/4.0 );

	VADD2( x1, gip->center, xbase );
	VSUB2( x2, gip->center, xbase );
	VADD2( y1, gip->center, ybase );
	VSUB2( y2, gip->center, ybase );

	RT_ADD_VLIST( vhead, x1, BN_VLIST_LINE_MOVE );	/* the base */
	RT_ADD_VLIST( vhead, y1, BN_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, x2, BN_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, y2, BN_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, x1, BN_VLIST_LINE_DRAW );

	VSCALE( tip, gip->normal, gip->mag );
	VADD2( tip, gip->center, tip );

	RT_ADD_VLIST( vhead, x1,  BN_VLIST_LINE_MOVE ); /* the sides */
	RT_ADD_VLIST( vhead, tip, BN_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, x2,  BN_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, y1,  BN_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vhead, tip, BN_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, y2,  BN_VLIST_LINE_DRAW );
	return(0);
}

/*
 *			R T _ G R P _ I M P O R T
 *
 *  Returns -
 *	-1	failure
 *	 0	success
 */
int
rt_grp_import(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
	struct rt_grip_internal	*gip;
	union record	*rp;

	fastf_t		orig_eqn[3*3];
	register double	f,t;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != ID_SOLID )  {
		bu_log("rt_grp_import: defective record, id=x%x\n", rp->u_id);
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_GRIP;
	ip->idb_meth = &rt_functab[ID_GRIP];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_grip_internal), "rt_grip_internal");
	gip = (struct rt_grip_internal *)ip->idb_ptr;
	gip->magic = RT_GRIP_INTERNAL_MAGIC;

	rt_fastf_float( orig_eqn, rp->s.s_values, 3 );	/* 2 floats to many */

	/* Transform the point, and the normal */
	MAT4X3PNT( gip->center, mat, &orig_eqn[0] );
	MAT4X3VEC( gip->normal, mat, &orig_eqn[3] );
	if ( NEAR_ZERO(mat[15], 0.001) ) {
		rt_bomb("rt_grip_import, scale factor near zero.");
	}
	gip->mag = orig_eqn[6]/mat[15];

	/* Verify that normal has unit length */
	f = MAGNITUDE( gip->normal );
	if( f < SMALL )  {
		bu_log("rt_grp_import:  bad normal, len=%g\n", f );
		return(-1);		/* BAD */
	}
	t = f - 1.0;
	if( !NEAR_ZERO( t, 0.001 ) )  {
		/* Restore normal to unit length */
		f = 1/f;
		VSCALE( gip->normal, gip->normal, f );
	}
	return 0;			/* OK */
}

/*
 *			R T _ G R P _ E X P O R T
 */
int
rt_grp_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_grip_internal	*gip;
	union record		*rec;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_GRIP )  return(-1);
	gip = (struct rt_grip_internal *)ip->idb_ptr;
	RT_GRIP_CK_MAGIC(gip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "grip external");
	rec = (union record *)ep->ext_buf;

	rec->s.s_id = ID_SOLID;
	rec->s.s_type = GRP;
	VMOVE(&rec->s.s_grip_N, gip->normal);
	VMOVE(&rec->s.s_grip_C, gip->center);
	rec->s.s_grip_m = gip->mag;

	return(0);
}

int
rt_grp_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	struct rt_grip_internal *gip;
	fastf_t			vec[7];
	register double		f,t;

	RT_CK_DB_INTERNAL( ip );
	BU_CK_EXTERNAL( ep );

	BU_ASSERT_LONG( ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 7 );

	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_GRIP;
	ip->idb_meth = &rt_functab[ID_GRIP];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_grip_internal), "rt_grip_internal");

	gip = (struct rt_grip_internal *)ip->idb_ptr;
	gip->magic = RT_GRIP_INTERNAL_MAGIC;

	/* Convert from database (network) to internal (host) format */
	ntohd( (unsigned char *)vec, ep->ext_buf, 7 );

	/* Transform the point, and the normal */
	MAT4X3PNT( gip->center, mat, &vec[0] );
	MAT4X3VEC( gip->normal, mat, &vec[3] );
	if ( NEAR_ZERO(mat[15], 0.001) ) {
		rt_bomb("rt_grip_import5, scale factor near zero.");
	}
	gip->mag = vec[6]/mat[15];

	/* Verify that normal has unit length */
	f = MAGNITUDE( gip->normal );
	if( f < SMALL )  {
		bu_log("rt_grp_import:  bad normal, len=%g\n", f );
		return(-1);		/* BAD */
	}
	t = f - 1.0;
	if( !NEAR_ZERO( t, 0.001 ) )  {
		/* Restore normal to unit length */
		f = 1/f;
		VSCALE( gip->normal, gip->normal, f );
	}
	return 0;		/* OK */
}

/*
 *			R T _ G R I P _ E X P O R T 5
 *
 */
int
rt_grp_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_grip_internal *gip;
	fastf_t			vec[7];

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_GRIP )  return(-1);
	gip = (struct rt_grip_internal *)ip->idb_ptr;
	RT_GRIP_CK_MAGIC(gip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * 7;
	ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes, "grip external");

	VSCALE( &vec[0], gip->center, local2mm );
	VMOVE( &vec[3], gip->normal );
	vec[6] = gip->mag * local2mm;

	/* Convert from internal (host) to database (network) format */
	htond( ep->ext_buf, (unsigned char *)vec, 7 );

	return 0;
}

/*
 *			R T _ G R P _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_grp_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	register struct rt_grip_internal	*gip =
		(struct rt_grip_internal *)ip->idb_ptr;
	char	buf[256];

	RT_GRIP_CK_MAGIC(gip);
	bu_vls_strcat( str, "grip\n");

	sprintf(buf, "\tN (%g, %g, %g)\n",
		V3ARGS(gip->normal));		/* should have unit length */

	bu_vls_strcat( str, buf );

	sprintf(buf, "\tC (%g %g %g) mag=%g\n",
		gip->center[0]*mm2local, gip->center[1]*mm2local,
		gip->center[2]*mm2local, gip->mag*mm2local);

	bu_vls_strcat( str, buf);
	return(0);
}

/*
 *			R T _ G R P _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_grp_ifree(struct rt_db_internal *ip)
{
	RT_CK_DB_INTERNAL(ip);
	bu_free( ip->idb_ptr, "grip ifree" );
	ip->idb_ptr = GENPTR_NULL;
}

/*
 *			R T _ G R P _ T E S S
 */
int
rt_grp_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	struct rt_grip_internal	*gip;

	RT_CK_DB_INTERNAL(ip);
	gip = (struct rt_grip_internal *)ip->idb_ptr;
	RT_GRIP_CK_MAGIC(gip);

	/* XXX tess routine needed */
	return(-1);
}
