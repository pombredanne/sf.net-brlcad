/*
 *			C H G M O D E L
 *
 * This module contains functions which change particulars of the
 * model, generally on a single solid or combination.
 * Changes to the tree structure of the model are done in chgtree.c
 *
 * Functions -
 *	f_itemair	add/modify item and air codes of a region
 *	f_modify	add/modify material code and los percent
 *	f_mirror	mirror image
 *	f_extrude	"extrude" command -- project an ARB face
 *	f_arbdef	define ARB8 using rot fb angles to define face
 *	f_edcomb	modify combination record info
 *	f_mirface	mirror an ARB face
 *	f_units		change local units of description
 *	f_title		change current title of description
 *	aexists		announce already exists
 *	f_make		create new solid of given type
 *	f_rot_obj	allow precise changes to object rotation
 *	f_sc_obj	allow precise changes to object scaling
 *	f_tr_obj	allow precise changes to object translation
 *
 *  Author -
 *	Michael John Muuss
 *	Keith A. Applin
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include	<math.h>
#include	<signal.h>
#include	<stdio.h>
#include "ged_types.h"
#include "../h/db.h"
#include "sedit.h"
#include "ged.h"
#include "objdir.h"
#include "solid.h"
#include "dm.h"
#include "../h/vmath.h"

extern void	perror();
extern int	atoi(), execl(), fork(), nice(), wait();
extern long	time();
extern char	*strcat();

void	aexists();

int		newedge;		/* new edge for arb editing */

extern int	numargs;	/* number of args */
extern char	*cmd_args[];	/* array of pointers to args */

/* Add/modify item and air codes of a region */
/* Format: I region item <air>	*/
void
f_itemair()
{
	register struct directory *dp;
	int ident, air;
	union record record;

	if( (dp = lookup( cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	air = ident = 0;
	ident = atoi( cmd_args[2] );

	/* If <air> is not included, it is assumed to be zero */
	if( numargs == 4 )  {
		air = atoi( cmd_args[3] );
	}
	db_getrec( dp, &record, 0 );
	if( record.u_id != ID_COMB ) {
		(void)printf("%s: not a combination\n", dp->d_namep );
		return;
	}
	if( record.c.c_flags != 'R' ) {
		(void)printf("%s: not a region\n", dp->d_namep );
		return;
	}
	record.c.c_regionid = ident;
	record.c.c_aircode = air;
	db_putrec( dp, &record, 0 );
}

/* Add/modify material code and los percent of a region */
/* Format: M region mat los	*/
void
f_modify()
{
	register struct directory *dp;
	register int mat, los;
	union record record;

	if( (dp = lookup( cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	mat = los = 0;
	mat = atoi( cmd_args[2] );
	los = atoi( cmd_args[3] );
	/* Should check that los is in valid range */
	db_getrec( dp, &record, 0 );
	if( record.u_id != ID_COMB )  {
		(void)printf("%s: not a combination\n", dp->d_namep );
		return;
	}
	if( record.c.c_flags != 'R' )  {
		(void)printf("%s: not a region\n", dp->d_namep );
		return;
	}
	record.c.c_material = mat;
	record.c.c_los = los;
	db_putrec( dp, &record, 0 );
}

/* Mirror image */
/* Format: m oldobject newobject axis	*/
void
f_mirror()
{
	register struct directory *proto;
	register struct directory *dp;
	register int i, j, k;
	union record record;
	mat_t mirmat;
	mat_t temp;

	if( (proto = lookup( cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	if( lookup( cmd_args[2], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( cmd_args[2] );
		return;
	}
	k = -1;
	if( strcmp( cmd_args[3], "x" ) == 0 )
		k = 0;
	if( strcmp( cmd_args[3], "y" ) == 0 )
		k = 1;
	if( strcmp( cmd_args[3], "z" ) == 0 )
		k = 2;
	if( k < 0 ) {
		(void)printf("axis must be x, y or z\n");
		return;
	}

	db_getrec( proto, &record, 0 );
	if( record.u_id == ID_SOLID ||
		record.u_id == ID_ARS_A ||
		record.u_id == ID_B_SPL_HEAD
	)  {
		if( (dp = dir_add( cmd_args[2], -1, DIR_SOLID, proto->d_len )) == DIR_NULL )
			return;
		db_alloc( dp, proto->d_len );

		/* create mirror image */
		if( record.u_id == ID_ARS_A )  {
			NAMEMOVE( cmd_args[2], record.a.a_name );
			db_putrec( dp, &record, 0 );
			for( i = 1; i < proto->d_len; i++ )  {
				db_getrec( proto, &record, i );
				for( j = k; j < 24; j += 3 )
					record.b.b_values[j] *= -1.0;
				db_putrec( dp, &record, i );
			}
		} else if( record.u_id == ID_B_SPL_HEAD )  {
			NAMEMOVE( cmd_args[2], record.d.d_name );
			db_putrec( dp, &record, 0 );
			for( i = 1; i < proto->d_len; i++ ) {
				db_getrec( proto, &record, i );
				if( record.u_id != ID_B_SPL_CTL )
					continue;
				for( j = k; j < 24; j += 3)
					record.l.l_pts[j] *= -1.0;
				db_putrec( dp, &record, i );
			}
		} else  {
			for( i = k; i < 24; i += 3 )
				record.s.s_values[i] *= -1.0;
			NAMEMOVE( cmd_args[2], record.s.s_name );
			db_putrec( dp, &record, 0 );
		}
	} else if( record.u_id == ID_COMB ) {
		if( (dp = dir_add(
			cmd_args[2], -1, record.c.c_flags == 'R' ?
				DIR_COMB|DIR_REGION :
				DIR_COMB,
			proto->d_len)
		) == DIR_NULL )
			return;
		db_alloc( dp, proto->d_len );
		NAMEMOVE(cmd_args[2], record.c.c_name);
		db_putrec(dp, &record, 0);
		mat_idn( mirmat );
		mirmat[k*5] = -1.0;
		for( i=1; i < proto->d_len; i++) {
			db_getrec(proto, &record, i);
			if(record.u_id != ID_MEMB) {
				(void)printf("f_mirror: bad db record\n");
				return;
			}
			mat_mul(temp, mirmat, record.M.m_mat);
			mat_copy(record.M.m_mat, temp);
			db_putrec(dp, &record, i);
		}
	} else {
		(void)printf("%s: Cannot mirror\n",cmd_args[2]);
		return;
	}

	if( no_memory )  {
		(void)printf(
		"Mirror image (%s) created but NO memory left to draw it\n",
			cmd_args[2] );
		return;
	}
	drawHobj( dp, ROOT, 0, identity, 0 );
	dmp->dmr_colorchange();		/* To color new solid */
	dmaflag = 1;
}

/* Extrude command - project an arb face */
/* Format: extrude face distance	*/
void
f_extrude()
{
	register int i, j;
	static int face;
	static int pt[4];
	static int prod;
	static float dist;
	static struct solidrec lsolid;	/* local copy of solid */

	if( not_state( ST_S_EDIT, "Extrude" ) )
		return;

	if( es_gentype != GENARB8 )  {
		(void)printf("Extrude: solid type must be ARB\n");
		return;
	}
	face = atoi( cmd_args[1] );
	if( face < 1000 || face > 9999 ) {
		(void)printf("ERROR: face must be 4 points\n");
		return;
	}
	/* get distance to project face */
	dist = atof( cmd_args[2] );
	/* apply es_mat[15] to get to real model space */
	/* convert from the local unit (as input) to the base unit */
	dist = dist * es_mat[15] * local2base;

	newedge = 1;

	/* convert to point notation in temporary buffer */
	VMOVE( &lsolid.s_values[0], &es_rec.s.s_values[0] );
	for( i = 3; i <= 21; i += 3 )  {  
		VADD2(&lsolid.s_values[i], &es_rec.s.s_values[i], &lsolid.s_values[0]);
	}

	pt[0] = face / 1000;
	i = face - (pt[0]*1000);
	pt[1] = i / 100;
	i = i - (pt[1]*100);
	pt[2] = i / 10;
	pt[3] = i - (pt[2]*10);

	/* user can input face in any order - will use product of
	 * face points to distinguish faces:
	 *    product       face
	 *       24         1234
	 *     1680         5678
	 *      252         2367
	 *      160         1548
	 *      672         4378
	 *       60         1256
	 */
	prod = 1;
	for( i = 0; i <= 3; i++ )  {
		prod *= pt[i];
		--pt[i];
		if( pt[i] > 7 )  {
			(void)printf("bad face: %d\n",face);
			return;
		}
	}
	/* find plane containing this face */
	if( (j = plane( pt[0], pt[1], pt[2], pt[3], &lsolid )) >= 0 )  {
		(void)printf("face: %d is not a plane\n",face);
		return;
	}
	/* get normal vector of length == dist */
	for( i = 0; i < 3; i++ )
		es_plant[i] *= dist;

	/* protrude the selected face */
	switch( prod )  {

	case 24:   /* protrude face 1234 */
		for( i = 0; i < 4; i++ )  {
			j = i + 4;
			VADD2( &lsolid.s_values[j*3],
				&lsolid.s_values[i*3],
				&es_plant[0]);
		}
		break;

	case 1680:   /* protrude face 5678 */
		for( i = 0; i < 4; i++ )  {
			j = i + 4;
			VADD2( &lsolid.s_values[i*3],
				&lsolid.s_values[j*3],
				&es_plant[0] );
		}
		break;

	case 60:   /* protrude face 1256 */
		VADD2( &lsolid.s_values[9],
			&lsolid.s_values[0],
			&es_plant[0] );
		VADD2( &lsolid.s_values[6],
			&lsolid.s_values[3],
			&es_plant[0] );
		VADD2( &lsolid.s_values[21],
			&lsolid.s_values[12],
			&es_plant[0] );
		VADD2( &lsolid.s_values[18],
			&lsolid.s_values[15],
			&es_plant[0] );
		break;

	case 672:   /* protrude face 4378 */
		VADD2( &lsolid.s_values[0],
			&lsolid.s_values[9],
			&es_plant[0] );
		VADD2( &lsolid.s_values[3],
			&lsolid.s_values[6],
			&es_plant[0] );
		VADD2( &lsolid.s_values[15],
			&lsolid.s_values[18],
			&es_plant[0] );
		VADD2( &lsolid.s_values[12],
			&lsolid.s_values[21],
			&es_plant[0] );
		break;

	case 252:   /* protrude face 2367 */
		VADD2( &lsolid.s_values[0],
			&lsolid.s_values[3],
			&es_plant[0] );
		VADD2( &lsolid.s_values[9],
			&lsolid.s_values[6],
			&es_plant[0] );
		VADD2( &lsolid.s_values[12],
			&lsolid.s_values[15],
			&es_plant[0] );
		VADD2( &lsolid.s_values[21],
			&lsolid.s_values[18],
			&es_plant[0] );
		break;

	case 160:   /* protrude face 1548 */
		VADD2( &lsolid.s_values[3],
			&lsolid.s_values[0],
			&es_plant[0] );
		VADD2( &lsolid.s_values[15],
			&lsolid.s_values[12],
			&es_plant[0] );
		VADD2( &lsolid.s_values[6],
			&lsolid.s_values[9],
			&es_plant[0] );
		VADD2( &lsolid.s_values[18],
			&lsolid.s_values[21],
			&es_plant[0] );
		break;

	default:
		(void)printf("bad face: %d\n", face );
		return;
	}

	/* Convert back to point&vector notation */
	VMOVE( &es_rec.s.s_values[0], &lsolid.s_values[0] );
	for( i = 3; i <= 21; i += 3 )  {  
		VSUB2( &es_rec.s.s_values[i], &lsolid.s_values[i], &lsolid.s_values[0]);
	}

	/* draw the new solid */
	illump = redraw( illump, &es_rec );

	/* Update display information */
	pr_solid( &es_rec.s );
	dmaflag = 1;
}

/* define an arb8 using rot fb angles to define a face */
/* Format: a name rot fb	*/
void
f_arbdef()
{
	register struct directory *dp;
	union record record;
	int i, j;
	float rota, fb;
	vect_t	norm;

	if( lookup( cmd_args[1] , LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( cmd_args[1] );
		return;
	}

	/* get rotation angle */
	rota = atof( cmd_args[2] ) * degtorad;

	/* get fallback angle */
	fb = atof( cmd_args[3] ) * degtorad;

	/* copy arb8 to the new name */
	if( (dp = lookup( "arb8", LOOKUP_NOISY )) == DIR_NULL )
		return;
	db_getrec( dp, &record, 0 );
	if( (dp = dir_add( cmd_args[1], -1, DIR_SOLID, 1 )) == DIR_NULL )
		return;
	db_alloc( dp, 1 );
	NAMEMOVE( cmd_args[1], record.s.s_name );

	/* put vertex of new solid at center of screen */
	record.s.s_values[0] = -toViewcenter[MDX];
	record.s.s_values[1] = -toViewcenter[MDY];
	record.s.s_values[2] = -toViewcenter[MDZ];

	/* calculate normal vector (length = .5) defined by rot,fb */
	norm[0] = cos(fb) * cos(rota) * -0.5;
	norm[1] = cos(fb) * sin(rota) * -0.5;
	norm[2] = sin(fb) * -0.5;

	for( i = 3; i < 24; i++ )
		record.s.s_values[i] = 0.0;

	/* find two perpendicular vectors which are perpendicular to norm */
	j = 0;
	for( i = 0; i < 3; i++ )  {
		if( fabs(norm[i]) < fabs(norm[j]) )
			j = i;
	}
	record.s.s_values[j+3] = 1.0;
	VCROSS( &record.s.s_values[9], &record.s.s_values[3], norm );
	VCROSS( &record.s.s_values[3], &record.s.s_values[9], norm );

	/* create new rpp 5x5x.5 */
	/* the 5x5 faces are in rot,fb plane */
	VUNITIZE( &record.s.s_values[3] );
	VUNITIZE( &record.s.s_values[9] );
	VADD2( &record.s.s_values[6],
		&record.s.s_values[3],
		&record.s.s_values[9] );
	VMOVE( &record.s.s_values[12], norm );
	for( i = 3; i < 12; i += 3 )  {
		j = i + 12;
		VADD2( &record.s.s_values[j], &record.s.s_values[i], norm );
	}

	/* update objfd and draw new arb8 */
	db_putrec( dp, &record, 0 );
	if( no_memory )  {
		(void)printf(
			"ARB8 (%s) created but no memory left to draw it\n",
			cmd_args[1] );
		return;
	}
	drawHobj( dp, ROOT, 0, identity, 0 );
	dmp->dmr_colorchange();		/* To color new solid */
	dmaflag = 1;
}

/* Modify Combination record information */
/* Format: edcomb combname flag item air mat los	*/
void
f_edcomb()
{
	register struct directory *dp;
	union record record;
	int ident, air, mat, los;

	if( (dp = lookup( cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
		return;

	ident = air = mat = los = 0;
	ident = atoi( cmd_args[3] );
	air = atoi( cmd_args[4] );
	mat = atoi( cmd_args[5] );
	los = atoi( cmd_args[6] );

	db_getrec( dp, &record, 0 );
	if( record.u_id != ID_COMB ) {
		(void)printf("%s: not a combination\n", dp->d_namep );
		return;
	}

	if( cmd_args[2][0] == 'R' )
		record.c.c_flags = 'R';
	else
		record.c.c_flags =' ';
	record.c.c_regionid = ident;
	record.c.c_aircode = air;
	record.c.c_material = mat;
	record.c.c_los = los;
	db_putrec( dp, &record, 0 );
}

/* Mirface command - mirror an arb face */
/* Format: mirror face axis	*/
void
f_mirface()
{
	register int i, j, k;
	static int face;
	static int pt[4];
	static int prod;
	static vect_t work;
	static struct solidrec lsolid;	/* local copy of solid */

	if( not_state( ST_S_EDIT, "Mirface" ) )
		return;

	if( es_gentype != GENARB8 )  {
		(void)printf("Mirface: solid type must be ARB\n");
		return;
	}
	face = atoi( cmd_args[1] );
	if( face < 1000 || face > 9999 ) {
		(void)printf("ERROR: face must be 4 points\n");
		return;
	}
	/* check which axis */
	k = -1;
	if( strcmp( cmd_args[2], "x" ) == 0 )
		k = 0;
	if( strcmp( cmd_args[2], "y" ) == 0 )
		k = 1;
	if( strcmp( cmd_args[2], "z" ) == 0 )
		k = 2;
	if( k < 0 ) {
		(void)printf("axis must be x, y or z\n");
		return;
	}

	work[0] = work[1] = work[2] = 1.0;
	work[k] = -1.0;

	/* convert to point notation in temporary buffer */
	VMOVE( &lsolid.s_values[0], &es_rec.s.s_values[0] );
	for( i = 3; i <= 21; i += 3 )  {  
		VADD2(&lsolid.s_values[i], &es_rec.s.s_values[i], &lsolid.s_values[0]);
	}

	pt[0] = face / 1000;
	i = face - (pt[0]*1000);
	pt[1] = i / 100;
	i = i - (pt[1]*100);
	pt[2] = i / 10;
	pt[3] = i - (pt[2]*10);

	/* user can input face in any order - will use product of
	 * face points to distinguish faces:
	 *    product       face
	 *       24         1234
	 *     1680         5678
	 *      252         2367
	 *      160         1548
	 *      672         4378
	 *       60         1256
	 */
	prod = 1;
	for( i = 0; i <= 3; i++ )  {
		prod *= pt[i];
		--pt[i];
		if( pt[i] > 7 )  {
			(void)printf("bad face: %d\n",face);
			return;
		}
	}

	/* mirror the selected face */
	switch( prod )  {

	case 24:   /* mirror face 1234 */
		for( i = 0; i < 4; i++ )  {
			j = i + 4;
			VELMUL( &lsolid.s_values[j*3],
				&lsolid.s_values[i*3],
				work);
		}
		break;

	case 1680:   /* mirror face 5678 */
		for( i = 0; i < 4; i++ )  {
			j = i + 4;
			VELMUL( &lsolid.s_values[i*3],
				&lsolid.s_values[j*3],
				work );
		}
		break;

	case 60:   /* mirror face 1256 */
		VELMUL( &lsolid.s_values[9],
			&lsolid.s_values[0],
			work );
		VELMUL( &lsolid.s_values[6],
			&lsolid.s_values[3],
			work );
		VELMUL( &lsolid.s_values[21],
			&lsolid.s_values[12],
			work );
		VELMUL( &lsolid.s_values[18],
			&lsolid.s_values[15],
			work );
		break;

	case 672:   /* mirror face 4378 */
		VELMUL( &lsolid.s_values[0],
			&lsolid.s_values[9],
			work );
		VELMUL( &lsolid.s_values[3],
			&lsolid.s_values[6],
			work );
		VELMUL( &lsolid.s_values[15],
			&lsolid.s_values[18],
			work );
		VELMUL( &lsolid.s_values[12],
			&lsolid.s_values[21],
			work );
		break;

	case 252:   /* mirror face 2367 */
		VELMUL( &lsolid.s_values[0],
			&lsolid.s_values[3],
			work );
		VELMUL( &lsolid.s_values[9],
			&lsolid.s_values[6],
			work );
		VELMUL( &lsolid.s_values[12],
			&lsolid.s_values[15],
			work );
		VELMUL( &lsolid.s_values[21],
			&lsolid.s_values[18],
			work );
		break;

	case 160:   /* mirror face 1548 */
		VELMUL( &lsolid.s_values[3],
			&lsolid.s_values[0],
			work );
		VELMUL( &lsolid.s_values[15],
			&lsolid.s_values[12],
			work );
		VELMUL( &lsolid.s_values[6],
			&lsolid.s_values[9],
			work );
		VELMUL( &lsolid.s_values[18],
			&lsolid.s_values[21],
			work );
		break;

	default:
		(void)printf("bad face: %d\n", face );
		return;
	}

	/* Convert back to point&vector notation */
	VMOVE( &es_rec.s.s_values[0], &lsolid.s_values[0] );
	for( i = 3; i <= 21; i += 3 )  {  
		VSUB2( &es_rec.s.s_values[i], &lsolid.s_values[i], &lsolid.s_values[0]);
	}

	/* draw the new solid */
	illump = redraw( illump, &es_rec );

	/* Update display information */
	pr_solid( &es_rec.s );
	dmaflag = 1;
}

/*
 * Change the local units of the description.
 * Base unit is fixed so just changing the current local unit.
 */
f_units()
{
	int new_unit = 0;

	if( strcmp(cmd_args[1], "mm") == 0 ) 
		new_unit = ID_MM_UNIT;
	else
	if( strcmp(cmd_args[1], "cm") == 0 ) 
		new_unit = ID_CM_UNIT;
	else
	if( strcmp(cmd_args[1],"m")==0 || strcmp(cmd_args[1],"meters")==0 ) 
		new_unit = ID_M_UNIT;
	else
	if( strcmp(cmd_args[1],"in")==0 || strcmp(cmd_args[1],"inches")==0 ) 
		new_unit = ID_IN_UNIT;
	else
	if( strcmp(cmd_args[1],"ft")==0 || strcmp(cmd_args[1],"feet")==0 ) 
		new_unit = ID_FT_UNIT;

	if( new_unit ) {
		/* change to the new local unit */
		dir_units( new_unit );
		localunit = new_unit;
		if(state == ST_S_EDIT)
			pr_solid( &es_rec.s );

		dmaflag = 1;
		return;
	}

	(void)printf("%s: unrecognized unit\n", cmd_args[1]);
	(void)printf("valid units: <mm|cm|m|in|ft|meters|inches|feet>\n");
}

/*
 *	Change the current title of the description
 */
f_title()
{
	register int i;

	cur_title[0] = '\0';
	for(i=1; i<numargs; i++) {
		(void)strcat(cur_title, cmd_args[i]);
		(void)strcat(cur_title, " ");
	}

	dir_title();
	dmaflag = 1;
}

/* tell him it already exists */
void
aexists( name )
char	*name;
{
	(void)printf( "%s:  already exists\n", name );
}

/*
 *  			F _ M A K E
 *  
 *  Create a new solid of a given type
 *  (Generic, or explicit)
 */
void
f_make()  {
	register struct directory *dp;
	union record record;

	if( lookup( cmd_args[1], LOOKUP_QUIET ) != DIR_NULL )  {
		aexists( cmd_args[1] );
		return;
	}
	/* Position this solid at view center */
	record.s.s_values[0] = -toViewcenter[MDX];
	record.s.s_values[1] = -toViewcenter[MDY];
	record.s.s_values[2] = -toViewcenter[MDZ];
	record.s.s_id = ID_SOLID;

	/* make name <arb8|arb7|arb6|arb5|arb4|ellg|ell|sph|tor|tgc|rec|trc|rcc> */
	if( strcmp( cmd_args[2], "arb8" ) == 0 )  {
		record.s.s_type = GENARB8;
		record.s.s_cgtype = ARB8;
		VSET( &record.s.s_values[0*3],
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		VSET( &record.s.s_values[1*3],  0, (Viewscale*2), 0 );
		VSET( &record.s.s_values[2*3],  0, (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[3*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[4*3],  -(Viewscale*2), 0, 0 );
		VSET( &record.s.s_values[5*3],  -(Viewscale*2), (Viewscale*2), 0 );
		VSET( &record.s.s_values[6*3],  -(Viewscale*2), (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[7*3],  -(Viewscale*2), 0, (Viewscale*2)  );
	} else if( strcmp( cmd_args[2], "arb7" ) == 0 )  {
		record.s.s_type = GENARB8;
		record.s.s_cgtype = ARB7;
		VSET( &record.s.s_values[0*3],
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -(0.5*Viewscale) );
		VSET( &record.s.s_values[1*3],  0, (Viewscale*2), 0 );
		VSET( &record.s.s_values[2*3],  0, (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[3*3],  0, 0, Viewscale );
		VSET( &record.s.s_values[4*3],  -(Viewscale*2), 0, 0 );
		VSET( &record.s.s_values[5*3],  -(Viewscale*2), (Viewscale*2), 0 );
		VSET( &record.s.s_values[6*3],  -(Viewscale*2), (Viewscale*2), Viewscale );
		VSET( &record.s.s_values[7*3],  -(Viewscale*2), 0, 0  );
	} else if( strcmp( cmd_args[2], "arb6" ) == 0 )  {
		record.s.s_type = GENARB8;
		record.s.s_cgtype = ARB6;
		VSET( &record.s.s_values[0*3],
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		VSET( &record.s.s_values[1*3],  0, (Viewscale*2), 0 );
		VSET( &record.s.s_values[2*3],  0, (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[3*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[4*3],  -(Viewscale*2), Viewscale, 0 );
		VSET( &record.s.s_values[5*3],  -(Viewscale*2), Viewscale, 0 );
		VSET( &record.s.s_values[6*3],  -(Viewscale*2), Viewscale, (Viewscale*2) );
		VSET( &record.s.s_values[7*3],  -(Viewscale*2), Viewscale, (Viewscale*2)  );
	} else if( strcmp( cmd_args[2], "arb5" ) == 0 )  {
		record.s.s_type = GENARB8;
		record.s.s_cgtype = ARB5;
		VSET( &record.s.s_values[0*3],
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		VSET( &record.s.s_values[1*3],  0, (Viewscale*2), 0 );
		VSET( &record.s.s_values[2*3],  0, (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[3*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[4*3],  -(Viewscale*2), Viewscale, Viewscale );
		VSET( &record.s.s_values[5*3],  -(Viewscale*2), Viewscale, Viewscale );
		VSET( &record.s.s_values[6*3],  -(Viewscale*2), Viewscale, Viewscale );
		VSET( &record.s.s_values[7*3],  -(Viewscale*2), Viewscale, Viewscale  );
	} else if( strcmp( cmd_args[2], "arb4" ) == 0 )  {
		record.s.s_type = GENARB8;
		record.s.s_cgtype = ARB4;
		VSET( &record.s.s_values[0*3],
			-toViewcenter[MDX] +Viewscale,
			-toViewcenter[MDY] -Viewscale,
			-toViewcenter[MDZ] -Viewscale );
		VSET( &record.s.s_values[1*3],  0, (Viewscale*2), 0 );
		VSET( &record.s.s_values[2*3],  0, (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[3*3],  0, (Viewscale*2), (Viewscale*2) );
		VSET( &record.s.s_values[4*3],  -(Viewscale*2), (Viewscale*2), 0 );
		VSET( &record.s.s_values[5*3],  -(Viewscale*2), (Viewscale*2), 0 );
		VSET( &record.s.s_values[6*3],  -(Viewscale*2), (Viewscale*2), 0 );
		VSET( &record.s.s_values[7*3],  -(Viewscale*2), (Viewscale*2), 0  );
	} else if( strcmp( cmd_args[2], "sph" ) == 0 )  {
		record.s.s_type = GENELL;
		record.s.s_cgtype = SPH;
		VSET( &record.s.s_values[1*3], (0.5*Viewscale), 0, 0 );	/* A */
		VSET( &record.s.s_values[2*3], 0, (0.5*Viewscale), 0 );	/* B */
		VSET( &record.s.s_values[3*3], 0, 0, (0.5*Viewscale) );	/* C */
	} else if( strcmp( cmd_args[2], "ell" ) == 0 )  {
		record.s.s_type = GENELL;
		record.s.s_cgtype = ELL;
		VSET( &record.s.s_values[1*3], (0.5*Viewscale), 0, 0 );	/* A */
		VSET( &record.s.s_values[2*3], 0, (0.25*Viewscale), 0 );	/* B */
		VSET( &record.s.s_values[3*3], 0, 0, (0.25*Viewscale) );	/* C */
	} else if( strcmp( cmd_args[2], "ellg" ) == 0 )  {
		record.s.s_type = GENELL;
		record.s.s_cgtype = ELL;
		VSET( &record.s.s_values[1*3], Viewscale, 0, 0 );	/* A */
		VSET( &record.s.s_values[2*3], 0, (0.5*Viewscale), 0 );	/* B */
		VSET( &record.s.s_values[3*3], 0, 0, (0.25*Viewscale) );	/* C */
	} else if( strcmp( cmd_args[2], "tor" ) == 0 )  {
		record.s.s_type = TOR;
		record.s.s_cgtype = TOR;
		VSET( &record.s.s_values[1*3], (0.5*Viewscale), 0, 0 );	/* N with mag = r2 */
		VSET( &record.s.s_values[2*3], 0, Viewscale, 0 );	/* A == r1 */
		VSET( &record.s.s_values[3*3], 0, 0, Viewscale );	/* B == r1 */
		VSET( &record.s.s_values[4*3], 0, (0.5*Viewscale), 0 );	/* A == r1-r2 */
		VSET( &record.s.s_values[5*3], 0, 0, (0.5*Viewscale) );	/* B == r1-r2 */
		VSET( &record.s.s_values[6*3], 0, (1.5*Viewscale), 0 );	/* A == r1+r2 */
		VSET( &record.s.s_values[7*3], 0, 0, (1.5*Viewscale) );	/* B == r1+r2 */
	} else if( strcmp( cmd_args[2], "tgc" ) == 0 )  {
		record.s.s_type = GENTGC;
		record.s.s_cgtype = TGC;
		VSET( &record.s.s_values[1*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[2*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[3*3],  0, (0.25*Viewscale), 0 );
		VSET( &record.s.s_values[4*3],  (0.25*Viewscale), 0, 0 );
		VSET( &record.s.s_values[5*3],  0, (0.5*Viewscale), 0 );
	} else if( strcmp( cmd_args[2], "tec" ) == 0 )  {
		record.s.s_type = GENTGC;
		record.s.s_cgtype = TEC;
		VSET( &record.s.s_values[1*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[2*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[3*3],  0, (0.25*Viewscale), 0 );
		VSET( &record.s.s_values[4*3],  (0.25*Viewscale), 0, 0 );
		VSET( &record.s.s_values[5*3],  0, 31.75, 0 );
	} else if( strcmp( cmd_args[2], "rec" ) == 0 )  {
		record.s.s_type = GENTGC;
		record.s.s_cgtype = REC;
		VSET( &record.s.s_values[1*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[2*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[3*3],  0, (0.25*Viewscale), 0 );
		VSET( &record.s.s_values[4*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[5*3],  0, (0.25*Viewscale), 0 );
	} else if( strcmp( cmd_args[2], "trc" ) == 0 )  {
		record.s.s_type = GENTGC;
		record.s.s_cgtype = TRC;
		VSET( &record.s.s_values[1*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[2*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[3*3],  0, (0.5*Viewscale), 0 );
		VSET( &record.s.s_values[4*3],  (0.25*Viewscale), 0, 0 );
		VSET( &record.s.s_values[5*3],  0, (0.25*Viewscale), 0 );
	} else if( strcmp( cmd_args[2], "rcc" ) == 0 )  {
		record.s.s_type = GENTGC;
		record.s.s_cgtype = RCC;
		VSET( &record.s.s_values[1*3],  0, 0, (Viewscale*2) );
		VSET( &record.s.s_values[2*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[3*3],  0, (0.5*Viewscale), 0 );
		VSET( &record.s.s_values[4*3],  (0.5*Viewscale), 0, 0 );
		VSET( &record.s.s_values[5*3],  0, (0.5*Viewscale), 0 );
	} else if( strcmp( cmd_args[2], "ars" ) == 0 )  {
		(void)printf("make ars not implimented yet\n");
		return;
	} else {
		(void)printf("make:  %s is not a known primitive\n", cmd_args[2]);
		return;
	}

	/* Add to in-core directory */
	if( (dp = dir_add( cmd_args[1], -1, DIR_SOLID, 0 )) == DIR_NULL )
		return;
	db_alloc( dp, 1 );

	NAMEMOVE( cmd_args[1], record.s.s_name );
	db_putrec( dp, &record, 0 );
	/* draw the "made" solid */
	drawHobj( dp, ROOT, 0, identity, 0 );
	dmp->dmr_colorchange();		/* To color new solid */
	dmaflag = 1;
}

/* allow precise changes to object rotation */
void
f_rot_obj()
{
	mat_t temp;
	vect_t s_point, point, v_work, model_pt;

	if( not_state( ST_O_EDIT, "Object Rotation" ) )
		return;

	if(movedir != ROTARROW) {
		(void)printf("Not in object rotate mode\n");
		return;
	}

	/* find point for rotation to take place wrt */
	MAT4X3PNT(model_pt, es_mat, es_rec.s.s_values);
	MAT4X3PNT(point, modelchanges, model_pt);

	/* Find absolute translation vector to go from "model_pt" to
	 * 	"point" without any of the rotations in "modelchanges"
	 */
	VSCALE(s_point, point, modelchanges[15]);
	VSUB2(v_work, s_point, model_pt);

	/* REDO "modelchanges" such that:
	 *	1. NO rotations (identity)
	 *	2. trans == v_work
	 *	3. same scale factor
	 */
	mat_idn(temp);
	MAT_DELTAS(temp, v_work[X], v_work[Y], v_work[Z]);
	temp[15] = modelchanges[15];
	mat_copy(modelchanges, temp);

	/* build new rotation matrix */
	mat_idn(temp);
	buildHrot(temp, atof(cmd_args[1])*degtorad,
			atof(cmd_args[2])*degtorad,
			atof(cmd_args[3])*degtorad );

	/* Record the new rotation matrix into the revised
	 *	modelchanges matrix wrt "point"
	 */
	wrt_point(modelchanges, temp, modelchanges, point);

	new_mats();
	dmaflag = 1;
}

/* allow precise changes to object scaling */
void
f_sc_obj()
{
	mat_t incr;
	vect_t point, temp;

	if( not_state( ST_O_EDIT, "Object Scale" ) )
		return;

	if(movedir != SARROW) {
		(void)printf("Not in object scale mode\n");
		return;
	}

	mat_idn(incr);
	incr[15] = 1.0 / (atof(cmd_args[1]) * modelchanges[15]);

	/* find point the scaling is to take place wrt */
	MAT4X3PNT(temp, es_mat, es_rec.s.s_values);
	MAT4X3PNT(point, modelchanges, temp);

	wrt_point(modelchanges, incr, modelchanges, point);
	new_mats();
}

/* allow precise changes to object translation */
void
f_tr_obj()
{
	register int i;
	mat_t incr, old;
	vect_t model_sol_pt, model_incr, ed_sol_pt, new_vertex;

	if( not_state( ST_O_EDIT, "Object Translation") )
		return;

	mat_idn(incr);
	mat_idn(old);

	if(movedir & (RARROW|UARROW)) {
		for(i=0; i<3; i++) {
			new_vertex[i] = atof(cmd_args[i+1]) * local2base;
		}
		MAT4X3PNT(model_sol_pt, es_mat, es_rec.s.s_values);
		MAT4X3PNT(ed_sol_pt, modelchanges, model_sol_pt);
		VSUB2(model_incr, new_vertex, ed_sol_pt);
		MAT_DELTAS(incr, model_incr[0], model_incr[1], model_incr[2]);
		mat_copy(old,modelchanges);
		mat_mul(modelchanges, incr, old);
		new_mats();
		return;
	}
	(void)printf("Not in object translate mode\n");
}

/* Change the default region ident codes: item air mat los
 */
void
f_regdef()
{

	dmaflag = 1;
	item_default = atoi(cmd_args[1]);

	if(numargs == 2)
		return;

	air_default = atoi(cmd_args[2]);
	if(air_default) 
		item_default = 0;

	if(numargs == 3)
		return;

	mat_default = atoi(cmd_args[3]);

	if(numargs == 4)
		return;

	los_default = atoi(cmd_args[4]);
}


/* Edgedir command:  define the direction of an arb edge being moved
 *	Format:  edgedir deltax deltay deltaz
	     OR  edgedir rot fb
 */

void
f_edgedir()
{
	int i, point;
	vect_t work;
	float rot, fb;

	if( not_state( ST_S_EDIT, "Edgedir" ) )
		return;

	if( es_edflag != EARB ) {
		(void)printf("Not moving an ARB edge\n");
		return;
	}

	if( es_gentype != GENARB8 ) {
		(void)printf("Edgeslope: solid type must be an ARB\n");
		return;
	}

	VMOVE(work, &es_rec.s.s_values[0]);
	if( (point = es_menu / 10 - 1) ) {
		VADD2(work, &es_rec.s.s_values[0], &es_rec.s.s_values[point*3]);
	}

	newedge = 1;
	editarb( work );

	/* set up slope -
	 *	if 2 values input assume rot, fb used
	 *	else assume delta_x, delta_y, delta_z
	 */
	if( numargs == 3 ) {
		rot = atof( cmd_args[1] ) * degtorad;
		fb = atof( cmd_args[2] ) * degtorad;
		es_m[0] = cos(fb) * cos(rot);
		es_m[1] = cos(fb) * sin(rot);
		es_m[2] = sin(fb);
	}
	else {
		for(i=0; i<3; i++) {
			/* put edge slope in es_m array */
			es_m[i] = atof( cmd_args[i+1] );
		}
	}

	if(MAGNITUDE(es_m) == 0) {
		(void)printf("BAD slope\n");
		return;
	}

	/* get it done */
	editarb( work );
	sedraw++;

}


