/*
 *	S H _ G R A S S . C
 *
 *  To add a new shader to the "rt" program:
 *
 *	1) Copy this file to sh_shadername.c
 *	2) edit sh_shadername.c:
 *		change "G R A S S" to "S H A D E R N A M E"
 *		change "grass"   to "shadername"
 *		Set a new number for the grass_MAGIC define
 *		define shader specific structure and defaults
 *		edit/build parse table for bu_structparse from grass_parse
 *		edit/build shader_mfuncs tables from grass_mfuncs for
 *			each shader name being built.
 *		edit the grass_setup function to do shader-specific setup
 *		edit the grass_render function to do the actual rendering
 *	3) Edit view.c to add extern for grass_mfuncs and call to mlib_add
 *		to function view_init()
 *	4) Edit Cakefile to add shader file to "FILES" and "RT_OBJ" macros.
 *	5) replace this list with a description of the shader, its parameters
 *		and use.
 */
#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./mathtab.h"
#include "./rdebug.h"

#define grass_MAGIC 0x9    /* make this a unique number for each shader */
#define CK_grass_SP(_p) RT_CKMAG(_p, grass_MAGIC, "grass_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct grass_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	double	lacunarity;
	double	h_val;
	double	octaves;
	double	size;
	point_t	vscale;	/* size of noise coordinate space */
	vect_t	delta;
	double	grass_thresh;	/* variables for shader ... */
	vect_t	grass_delta;	/* offset into noise space */
	point_t grass_min;
	point_t grass_max;
	mat_t	m_to_r;	/* model to region space matrix */
	mat_t	r_to_m;	/* region to model space matrix */
};

/* The default values for the variables in the shader specific structure */
CONST static
struct grass_specific grass_defaults = {
	grass_MAGIC,
	2.1753974,	/* lacunarity */
	1.0,		/* h_val */
	4.0,		/* octaves */
	1.0,		/* size */
	{ 1.0, 1.0, 1.0 },	/* vscale */
	{ 1000.0, 1000.0, 1000.0 },	/* delta into noise space */
	0.75,				/* grass_thresh */
	{ 1.0, 1.0, 1.0 },		/* grass_delta */
	{ 0.0, 0.0, 0.0 },		/* grass_min */
	{ 0.0, 0.0, 0.0 },		/* grass_max */
	{	1.0, 0.0, 0.0, 0.0,	/* grass_m_to_sh */
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0 }
	};

#define SHDR_NULL	((struct grass_specific *)0)
#define SHDR_O(m)	offsetof(struct grass_specific, m)
#define SHDR_AO(m)	offsetofarray(struct grass_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse grass_print_tab[] = {
	{"%f",	1, "lacunarity",	SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "H", 		SHDR_O(h_val),	FUNC_NULL },
	{"%f",	1, "octaves", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "size",		SHDR_O(size),	FUNC_NULL },
	{"%f",  1, "thresh",		SHDR_O(grass_thresh),	FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(grass_delta),	FUNC_NULL },
	{"%f",  3, "max",		SHDR_AO(grass_max),	FUNC_NULL },
	{"%f",  3, "min",		SHDR_AO(grass_min),	FUNC_NULL },
	{"%f",  3, "delta",		SHDR_AO(grass_delta),	FUNC_NULL },
	{"%f",  16, "m_to_r",		SHDR_AO(m_to_r),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct bu_structparse grass_parse_tab[] = {
	{"i",	bu_byteoffset(grass_print_tab[0]), "grass_print_tab", 0, FUNC_NULL },
	{"%f",	1, "l",			SHDR_O(lacunarity),	FUNC_NULL },
	{"%f",	1, "o", 		SHDR_O(octaves),	FUNC_NULL },
	{"%f",  1, "s",			SHDR_O(size),	FUNC_NULL },
	{"%f",  1, "t",			SHDR_O(grass_thresh),	FUNC_NULL },
	{"%f",  3, "d",			SHDR_AO(grass_delta),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	grass_setup(), grass_render();
HIDDEN void	grass_print(), grass_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs grass_mfuncs[] = {
	{"grass",	0,	0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
	grass_setup,	grass_render,	grass_print,	grass_free },

	{(char *)0,	0,		0,		0,		0,
	0,		0,		0,		0 }
};


/*	G R A S S _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
grass_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct grass_specific	*grass_sp;
	mat_t	tmp, mtr;
	vect_t	bb_min, bb_max, v_tmp;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);


	if( rdebug&RDEBUG_SHADE)
		rt_log("grass_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	GETSTRUCT( grass_sp, grass_specific );
	*dpp = (char *)grass_sp;

	/* initialize the default values for the shader */
	memcpy(grass_sp, &grass_defaults, sizeof(struct grass_specific) );

	if (rp->reg_aircode == 0) {
		rt_log("%s\n%s\n",
		"*** WARNING: grass shader applied to non-air region!!! ***",
		"Set air flag with \"edcodes\" in mged");
		rt_bomb("");
	}

	/* parse the user's arguments for this use of the shader. */
	if( bu_struct_parse( matparm, grass_parse_tab, (char *)grass_sp ) < 0 )
		return(-1);

	/* We're going to operate in Region space so we can extract known
	 * distances and sizes in mm for the shader calculations.
	 */
	db_region_mat(mtr, rtip->rti_dbip, rp->reg_name);	 


	mat_idn(tmp);
	if (grass_sp->size != 1.0) {
		/* the user sets "size" to the size of the biggest
		 * noise-space blob in model coordinates
		 */
		tmp[0] = tmp[5] = tmp[10] = 1.0/grass_sp->size;
	} else {
		tmp[0] = 1.0/grass_sp->vscale[0];
		tmp[5] = 1.0/grass_sp->vscale[1];
		tmp[10] = 1.0/grass_sp->vscale[2];
	}

	mat_mul(grass_sp->m_to_r, tmp, mtr);

	/* Add any translation within shader/region space */
	mat_idn(tmp);
	tmp[MDX] = grass_sp->delta[0];
	tmp[MDY] = grass_sp->delta[1];
	tmp[MDZ] = grass_sp->delta[2];
	mat_mul2(tmp, grass_sp->m_to_r);

	mat_inv(grass_sp->r_to_m, grass_sp->m_to_r);

	if( rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", grass_print_tab, (char *)grass_sp );
		mat_print( "m_to_r", grass_sp->m_to_r );
	}

	return(1);
}

/*
 *	G R A S S _ P R I N T
 */
HIDDEN void
grass_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print( rp->reg_name, grass_print_tab, (char *)dp );
}

/*
 *	G R A S S _ F R E E
 */
HIDDEN void
grass_free( cp )
char *cp;
{
	rt_free( cp, "grass_specific" );
}

/*
 *	G R A S S _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
grass_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;	/* defined in material.h */
char			*dp;	/* ptr to the shader-specific struct */
{
	register struct grass_specific *grass_sp =
		(struct grass_specific *)dp;
	point_t in_pt, out_pt;	/* model space in/out points */
	double in_radius, out_radius;	/* beam radius, model space */
	int step_cnt;
	double step_dist, seg_dist;
	vect_t dist_v;
	point_t pt;
	double val, dist, delta;
	double	step_delta;/* distance between sample points, texture space */
	fastf_t	model_step; /* distance between sample points, model space */
	int	steps;	   /* # of samples along ray/solid intersection */
	int	i, octaves;
	double	alt, r, radius;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_grass_SP(grass_sp);

	if( rdebug&RDEBUG_SHADE) {
		bu_struct_print( "grass_render Parameters:", grass_print_tab, (char *)grass_sp );
	}

	/* figure out the in/out points, and the radius of the beam
	 * We can work in model space since grass isn't likely to be moving
	 * around the scene.
	 */
	VJOIN1(in_pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	in_radius = ap->a_rbeam + pp->pt_inhit->hit_dist * ap->a_diverge;

	VJOIN1(out_pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
	out_radius = ap->a_rbeam + pp->pt_outhit->hit_dist * ap->a_diverge;

	if( rdebug&RDEBUG_SHADE) {
		bu_log(" in_pt: %g %g %g  radius: %g\n", V3ARGS(in_pt), in_radius);
		bu_log("out_pt: %g %g %g  radius: %g\n", V3ARGS(out_pt), out_radius);
	}

	/* XXX the radius of the beam should give us an idea 
	 * of how many octaves of noise we need to evaluate.
	 */



	seg_dist = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	step_cnt = seg_dist / 10; /* scale dist to cm units,  1_sample/cm */

	/* get actual length of step */
	step_dist = seg_dist / (double)step_cnt; 

	swp->sw_transmit = 1.0;

	if( rdebug&RDEBUG_SHADE) {
		bu_log("seg_dist: %g\n", seg_dist);
		bu_log("step_cnt %d  step_dist %g\n", step_cnt, step_dist);
	}

	for (i=0 ; i < step_cnt ; i++ ) {
		dist = pp->pt_inhit->hit_dist + i * step_dist;
		VJOIN1(pt, ap->a_ray.r_pt, dist, ap->a_ray.r_dir);
		radius = ap->a_rbeam + dist * ap->a_diverge;

		if ( rdebug&RDEBUG_SHADE) {
			bu_log("pt %g %g %g radius %g\n", 
				V3ARGS(pt), radius);
		}

		octaves = 1;
		for (r = radius ; r < grass_sp->size ; r *= grass_sp->lacunarity)
			octaves ++;

		if ( rdebug&RDEBUG_SHADE) {
			bu_log("octaves %d\n", octaves);
		}

		alt = pt[Z];
		pt[Z] = 0.0;
		VSCALE(pt, pt, 1.0/grass_sp->size);
		val = bn_noise_fbm(pt, grass_sp->h_val,
			grass_sp->lacunarity, octaves);

		if ( rdebug&RDEBUG_SHADE) {
			bu_log("value %g\n", val);
		}

		if (val > grass_sp->grass_thresh) {
			swp->sw_transmit = 0.0;
			break;
		}
		if (alt < 100.0 && val < -grass_sp->grass_thresh) {
			swp->sw_transmit = 0.0;
			break;
		}
	}

	/* grass is basically a green object with transparency */
	if( swp->sw_xmitonly )  return 1;

	if (swp->sw_transmit == 0.0) {
		/* hit a blade of grass */
		/* compute new random normal */
		vect_t N;

		pt[Z] = (alt/grass_sp->size) * 0.125;
		bn_noise_vec(pt, N);
		if ( rdebug&RDEBUG_SHADE) {
			bu_log("Old Normal %g %g %g\n",
				V3ARGS(swp->sw_hit.hit_normal));
		}
		if (VDOT(N, ap->a_ray.r_dir) > 0.0) {
			VREVERSE(swp->sw_hit.hit_normal, N);
		} else {
			VMOVE(swp->sw_hit.hit_normal, N);
		}
		VUNITIZE(swp->sw_hit.hit_normal);
		if ( rdebug&RDEBUG_SHADE) {
			bu_log("New Normal %g %g %g\n",
				V3ARGS(swp->sw_hit.hit_normal));
		}
	} else {
		/* missed everything XXX shoot a ray on through */
		swp->sw_transmit = 1.0;
#if 1
		VSETALL(swp->sw_color, 0.0);
		VSETALL(swp->sw_basecolor, 0.0);
#endif		
		swp->sw_refrac_index = 1.0;
		swp->sw_reflect = 0.0;

	}
	/* shader must perform transmission/reflection calculations
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */
#if 0
	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );
	else {
		/* call plastic here */
	}
#endif
	return(1);
}
