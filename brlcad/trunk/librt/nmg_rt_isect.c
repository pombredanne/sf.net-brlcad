/*
 *			N M G _ R T _ I S E C T . C
 *
 *	Support routines for raytracing an NMG.
 *
 *  Author -
 *	Lee A. Butler
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "./nmg_rt.h"

static void 	vertex_neighborhood RT_ARGS((struct ray_data *rd, struct vertexuse *vu_p, struct hitmiss *myhit));
RT_EXTERN(void	nmg_isect_ray_model, (struct ray_data *rd));

/* XXX move to ./nmg_rt.h */
#define NMG_RAY_STATE_ANY	8

CONST char *
nmg_rt_inout_str( code )
int	code;
{
	switch(code) {
	case HMG_HIT_IN_IN:
		return("IN_IN");
	case HMG_HIT_IN_OUT:
		return("IN_OUT");
	case HMG_HIT_OUT_IN:
		return("OUT_IN");
	case HMG_HIT_OUT_OUT:
		return("OUT_OUT");
	case HMG_HIT_IN_ON:
		return("IN_ON");
	case HMG_HIT_ON_IN:
		return("ON_IN");
	case HMG_HIT_OUT_ON:
		return("OUT_ON");
	case HMG_HIT_ON_OUT:
		return("ON_OUT");
	case HMG_HIT_ANY_ANY:
		return("ANY_ANY");
	}
	return("?_?\n");
}

CONST char *
nmg_rt_state_str( code )
int	code;
{
	switch(code) {
	case NMG_RAY_STATE_INSIDE:
		return "RS_INSIDE";
	case NMG_RAY_STATE_ON:
		return "RS_ON";
	case NMG_RAY_STATE_OUTSIDE:
		return "RS_OUTSIDE";
	case NMG_RAY_STATE_ANY:
		return "RS_ANY";
	}
	return "RS_UNKNOWN";
}

/*
 *				N M G _ C K _ H I T M I S S _ L I S T
 *
 *  Ensure that the ray makes a valid set of state transitions.
 */
void
nmg_ck_hitmiss_list( hd )
CONST struct rt_list	*hd;
{
	struct hitmiss	*hmp;
	int		state = NMG_RAY_STATE_OUTSIDE;
	int		istate;
	int		ostate;

	for( RT_LIST_FOR( hmp, hitmiss, hd ) )  {
		NMG_CK_HITMISS(hmp);

		/* Skip hits on non-3-manifolds */
		if( hmp->in_out == HMG_HIT_ANY_ANY )  continue;

		istate = HMG_INBOUND_STATE(hmp);
		ostate = HMG_OUTBOUND_STATE(hmp);
		if( state != istate )  {
			rt_log("ray state was=%s, transition=%s (istate=%s)\n",
				nmg_rt_state_str(state),
				nmg_rt_inout_str(hmp->in_out),
				nmg_rt_state_str(istate) );
			rt_bomb("nmg_ck_hitmiss_list() NMG ray-tracer bad in/out state transition\n");
		}
		state = ostate;
	}
	if( state != NMG_RAY_STATE_OUTSIDE )  {
		rt_log("ray ending state was %s, should have been RS_OUT\n", nmg_rt_state_str(state));
		rt_bomb("nmg_ck_hitmiss_list() NMG ray-tracer bad ending state\n");
	}
}

/* Plot a faceuse and a line between pt and plane_pt */
static int plot_file_number=0;

static void
plfu( fu, pt, plane_pt )
struct faceuse *fu;
point_t pt;
point_t plane_pt;
{
	FILE *fd;
	char name[25];
	long *b;

	NMG_CK_FACEUSE(fu);
	
	sprintf(name, "ray%02d.pl", plot_file_number++);
	if ((fd=fopen(name, "w")) == (FILE *)NULL) {
		perror(name);
		abort();
	}

	rt_log("overlay %s\n", name);
	b = (long *)rt_calloc( fu->s_p->r_p->m_p->maxindex,
		sizeof(long), "bit vec"),

	pl_erase(fd);
	pd_3space(fd,
		fu->f_p->min_pt[0]-1.0,
		fu->f_p->min_pt[1]-1.0,
		fu->f_p->min_pt[2]-1.0,
		fu->f_p->max_pt[0]+1.0,
		fu->f_p->max_pt[1]+1.0,
		fu->f_p->max_pt[2]+1.0);
	
	nmg_pl_fu(fd, fu, b, 255, 255, 255);

	pl_color(fd, 255, 50, 50);
	pdv_3line(fd, pt, plane_pt);

	rt_free((char *)b, "bit vec");
	fclose(fd);
}

static void
pleu( eu, pt, plane_pt)
struct edgeuse *eu;
point_t pt;
point_t plane_pt;
{
        FILE *fd;
        char name[25];
	long *b;
	point_t min_pt, max_pt;
	int i;
	struct model *m;
        
        sprintf(name, "ray%02d.pl", plot_file_number++);
        if ((fd=fopen(name, "w")) == (FILE *)NULL) {
        	perror(name);
        	abort();
        }

	rt_log("overlay %s\n", name);
	m = nmg_find_model( eu->up.magic_p );
	b = (long *)rt_calloc( m->maxindex, sizeof(long), "bit vec");

	pl_erase(fd);

	VMOVE(min_pt, eu->vu_p->v_p->vg_p->coord);
	
	for (i=0 ; i < 3 ; i++) {
		if (eu->eumate_p->vu_p->v_p->vg_p->coord[i] < min_pt[i]) {
			max_pt[i] = min_pt[i];
			min_pt[i] = eu->eumate_p->vu_p->v_p->vg_p->coord[i];
		} else {
			max_pt[i] = eu->eumate_p->vu_p->v_p->vg_p->coord[i];
		}
	}
	pd_3space(fd,
		min_pt[0]-1.0, min_pt[1]-1.0, min_pt[2]-1.0,
		max_pt[0]+1.0, max_pt[1]+1.0, max_pt[2]+1.0);

	nmg_pl_eu(fd, eu, b, 255, 255, 255);
	pl_color(fd, 255, 50, 50);
	pdv_3line(fd, pt, plane_pt);
	rt_free((char *)b, "bit vec");
	fclose(fd);
}
static void
plvu(vu)
struct vertexuse *vu;
{
}

void
nmg_rt_print_hitmiss(a_hit)
struct hitmiss *a_hit;
{
	NMG_CK_HITMISS(a_hit);
	rt_log("   dist:%12g pt=(%g %g %g) %s=x%x\n",
		a_hit->hit.hit_dist,
		a_hit->hit.hit_point[0],
		a_hit->hit.hit_point[1],
		a_hit->hit.hit_point[2],
		rt_identify_magic(*(long *)a_hit->hit.hit_private),
		a_hit->hit.hit_private
	);
	rt_log("\tstate:%s", nmg_rt_inout_str(a_hit->in_out));

	switch (a_hit->start_stop) {
	case NMG_HITMISS_SEG_IN:	rt_log(" SEG_START"); break;
	case NMG_HITMISS_SEG_OUT:	rt_log(" SEG_STOP"); break;
	}

	VPRINT("\n\tin_normal", a_hit->inbound_norm);
	VPRINT("\tout_normal", a_hit->outbound_norm);
}
static void print_hitlist(hl)
struct hitmiss *hl;
{
	struct hitmiss *a_hit;

	if (!(rt_g.NMG_debug & DEBUG_RT_ISECT))
		return;

	rt_log("nmg/ray hit list:\n");

	for (RT_LIST_FOR(a_hit, hitmiss, &(hl->l))) {
		nmg_rt_print_hitmiss(a_hit);
	}
}




/*
 *	H I T _ I N S
 *
 *	insert the new hit point in the correct place in the list of hits
 *	so that the list is always in sorted order
 */
static void
hit_ins(rd, newhit)
struct ray_data *rd;
struct hitmiss *newhit;
{
	struct hitmiss *a_hit;

	RT_LIST_MAGIC_SET(&newhit->l, NMG_RT_HIT_MAGIC);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log("hit_ins()\n  inserting:");
		nmg_rt_print_hitmiss(newhit);
	}

	for (RT_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)) {
		if (newhit->hit.hit_dist < a_hit->hit.hit_dist)
			break;
	}

	/* a_hit now points to the item before which we should insert this
	 * hit in the hit list.
	 */

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		if (RT_LIST_NOT_HEAD(a_hit, &rd->rd_hit)) {
			rt_log("   prior to:");
			nmg_rt_print_hitmiss(a_hit);
		} else {
			rt_log("\tat end of list\n");
		}
	}

	RT_LIST_INSERT(&a_hit->l, &newhit->l);

	print_hitlist((struct hitmiss *)&rd->rd_hit);

}


/*
 *  The ray missed this vertex.  Build the appropriate miss structure.
 */
static struct hitmiss *
ray_miss_vertex(rd, vu_p)
struct ray_data *rd;
struct vertexuse *vu_p;
{
	struct hitmiss *myhit;

	NMG_CK_VERTEXUSE(vu_p);
	NMG_CK_VERTEX(vu_p->v_p);
	NMG_CK_VERTEX_G(vu_p->v_p->vg_p);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log("ray_miss_vertex(%g %g %g)\n",
			vu_p->v_p->vg_p->coord[0],
			vu_p->v_p->vg_p->coord[1],
			vu_p->v_p->vg_p->coord[2]);
	}

	if (myhit=NMG_INDEX_GET(rd->hitmiss, vu_p->v_p)) {
		/* vertex previously processed */
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("ray_miss_vertex( vertex previously HIT!!!! )\n");
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_SUB_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("ray_miss_vertex( vertex previously HIT_SUB!?!? )\n");
		}
		return(myhit);
	}
	if (myhit=NMG_INDEX_GET(rd->hitmiss, vu_p->v_p)) {
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("ray_miss_vertex( vertex previously missed )\n");
		return(myhit);
	}

	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, vu_p->v_p, myhit);
	myhit->outbound_use = (long *)vu_p;
	myhit->inbound_use = (long *)vu_p;
	myhit->hit.hit_private = (genptr_t)vu_p->v_p;

	/* get build_vertex_miss() to compute this */
	myhit->dist_in_plane = -1.0;

	/* add myhit to the list of misses */
	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
	RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
	NMG_CK_HITMISS(myhit);
	NMG_CK_HITMISS_LISTS(myhit, rd);

 	return myhit;
}

/*
 * Support routine for vertex_neighborhood()
 *
 */
static void
get_pole_dist_to_face(rd, vu,
	Pole, Pole_prj_pt, Pole_dist, Pole_pca,
	pointA, leftA, pointB, leftB, polar_height_vect, Pole_name)
struct ray_data *rd;
struct vertexuse *vu;
point_t Pole;
point_t Pole_prj_pt;
double *Pole_dist;
point_t Pole_pca;
point_t pointA;
vect_t leftA;
point_t pointB;
vect_t leftB;
vect_t polar_height_vect;
char *Pole_name;
{
	vect_t pca_to_pole_vect;
	vect_t VtoPole_prj;
	point_t pcaA, pcaB;
	double distA, distB;
	int code, status;

/* find the points of closest approach
 *  There are six distinct return values from rt_dist_pt3_lseg3():
 *
 *    Value	Condition
 *    	-----------------------------------------------------------------
 *	0	P is within tolerance of lseg AB.  *dist isn't 0: (SPECIAL!!!)
 *		  *dist = parametric dist = |PCA-A| / |B-A|.  pca=computed.
 *	1	P is within tolerance of point A.  *dist = 0, pca=A.
 *	2	P is within tolerance of point B.  *dist = 0, pca=B.
 *
 *	3	P is to the "left" of point A.  *dist=|P-A|, pca=A.
 *	4	P is to the "right" of point B.  *dist=|P-B|, pca=B.
 *	5	P is "above/below" lseg AB.  *dist=|PCA-P|, pca=computed.
 */
	code = rt_dist_pt3_lseg3( &distA, pcaA, vu->v_p->vg_p->coord, pointA,
			Pole_prj_pt, rd->tol );
	if (code < 3) {
		/* Point is on line */
		*Pole_dist = MAGNITUDE(polar_height_vect);
		VMOVE(Pole_pca, Pole_prj_pt);
		return;
	}

	status = code << 4;
	code = rt_dist_pt3_lseg3( &distB, pcaB, vu->v_p->vg_p->coord, pointB,
			Pole_prj_pt, rd->tol );
	if (code < 3) {
		/* Point is on line */
		*Pole_dist = MAGNITUDE(polar_height_vect);
		VMOVE(Pole_pca, Pole_prj_pt);
		return;
	}

	status |= code;

	/*	  Status
	 *	codeA CodeB
	 *	 3	3	Do the Tanenbaum patch thing
	 *	 3	4	This should not happen.
	 *	 3	5	compute dist from pole to pcaB
	 *	 4	3	This should not happen.
	 *	 4	4	This should not happen.
	 *	 4	5	This should not happen.
	 *	 5	3	compute dist from pole to pcaA
	 *	 5	4	This should not happen.
	 *	 5	5	pick the edge with smallest "dist"
	 *	 		and compute pole-pca distance.
	 */
   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("get_pole_dist_to_face(%s) status from dist_pt_lseg == 0x%02x\n", Pole_name, status);

	switch (status) {
	case 0x35: /* compute dist from pole to pcaB */

		/* if plane point is "inside" edge B, plane point is PCA */
		VSUB2(VtoPole_prj, Pole_prj_pt, vu->v_p->vg_p->coord);
		if (VDOT(leftB, VtoPole_prj) >= 0.0) {
			/* plane point is "inside" edge B */
		   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tplane point inside face\n");
			VMOVE(Pole_pca, Pole_prj_pt);
			VSUB2(pca_to_pole_vect, Pole_prj_pt, Pole);
			*Pole_dist = MAGNITUDE(pca_to_pole_vect);
			return;
		}

	   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tplane point outside face\n");
		VSUB2(pca_to_pole_vect, pcaB, Pole);
		VMOVE(Pole_pca, pcaB);
		break;
	case 0x53: /* compute dist from pole to pcaA */

		/* if plane point is "inside" edge A, plane point is PCA */
		VSUB2(VtoPole_prj, Pole_prj_pt, vu->v_p->vg_p->coord);
		if (VDOT(leftA, VtoPole_prj) >= 0.0) {
			/* plane point is "inside" edge A */
		   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tplane point inside face\n");
			VMOVE(Pole_pca, Pole_prj_pt);
			VSUB2(pca_to_pole_vect, Pole_prj_pt, Pole);
			*Pole_dist = MAGNITUDE(pca_to_pole_vect);
			return;
		}

	   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tplane point outside face\n");
		VSUB2(pca_to_pole_vect, pcaA, Pole);
		VMOVE(Pole_pca, pcaA);
		break;
	case 0x55:/* pick the edge with smallest "dist"
		   * and compute pole-pca distance.
		   */
		VSUB2(VtoPole_prj, Pole_prj_pt, vu->v_p->vg_p->coord);

		if (distA < distB) {
			VUNITIZE(VtoPole_prj);
			VUNITIZE(leftA);
			if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
				VPRINT("LeftA", leftA);
				VPRINT("VtoPole_prj", VtoPole_prj);
			}

			if (VDOT(leftA, VtoPole_prj) >= 0.0) {
				/* plane point is "inside" edge A */
			   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
					rt_log("\tplane point inside face\n");
				VSUB2(pca_to_pole_vect, Pole_prj_pt, Pole);
				*Pole_dist = MAGNITUDE(pca_to_pole_vect);
				VMOVE(Pole_pca, Pole_prj_pt);
				return;
			}
		   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
					rt_log("\tplane point outside face\n");
			VSUB2(pca_to_pole_vect, pcaA, Pole);
			VMOVE(Pole_pca, pcaA);
		} else {
			VUNITIZE(VtoPole_prj);
			VUNITIZE(leftB);
			if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
				VPRINT("LeftB", leftB);
				VPRINT("VtoPole_prj", VtoPole_prj);
			}
			if (VDOT(leftB, VtoPole_prj) >= 0.0) {
				/* plane point is "inside" edge B */
			   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
					rt_log("\tplane point inside face\n");
				VMOVE(Pole_pca, Pole_prj_pt);
				VSUB2(pca_to_pole_vect, Pole_prj_pt, Pole);
				*Pole_dist = MAGNITUDE(pca_to_pole_vect);
				return;
			}
		   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
					rt_log("\tplane point outside face\n");
			VSUB2(pca_to_pole_vect, pcaB, Pole);
			VMOVE(Pole_pca, pcaB);
		}
		break;
	case 0x33: {
		/* The point is over the vertex shared by the points,
		 * and not over one edge or the other.  We now need to
		 * figure out which edge to classify this point against.
		 *
		 * Time to do the Tanenbaum algorithm.
		 */
		double dotA;
		double dotB;

	   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("\tplane point beyond both edges.... Doing the Tanenbaum algorithm.\n");

		VSUB2(VtoPole_prj, Pole_prj_pt, vu->v_p->vg_p->coord);

		dotA = VDOT(leftA, VtoPole_prj);
		dotB = VDOT(leftB, VtoPole_prj);
		
		if (dotA > dotB) {
			if (dotA >= 0.0) {
				/* Point is "inside" face,
				 * PCA is plane projection point.
				 */
			   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
					rt_log("\tpoint inside face\n");
				VMOVE(Pole_pca, Pole_prj_pt);
				VSUB2(pca_to_pole_vect, Pole, Pole_prj_pt);
			} else {
				/* Point is "outside" face, PCA is at vertex. */
			   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
					rt_log("\tpoint outside face, PCA is vertex\n");

				VSUB2(pca_to_pole_vect, pcaA, Pole);
				VMOVE(Pole_pca, pcaA);
			}
		} else {
			if (dotB >= 0.0) {
				/* Point is "inside" face,
				 * PCA is plane projection point
				 */
			   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
					rt_log("\tpoint is inside face\n");
				VMOVE(Pole_pca, Pole_prj_pt);
				VSUB2(pca_to_pole_vect, Pole, Pole_prj_pt);
			} else {

				/* Point is "outside" face, PCA is at vertex. */
			   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
					rt_log("\tpoint is outside face, PCA is vertex\n");
				VSUB2(pca_to_pole_vect, pcaB, Pole);
				VMOVE(Pole_pca, pcaB);
			}
		}
	   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			VPRINT("\tpca_to_pole_vect", pca_to_pole_vect);
		*Pole_dist = MAGNITUDE(pca_to_pole_vect);
		return;
	}
	case 0x34: /* fallthrough */
	case 0x54: /* fallthrough */
	case 0x43: /* fallthrough */
	case 0x44: /* fallthrough */
	case 0x45: /* fallthrough */
	default:
		rt_log("%s %d: So-called 'Impossible' status codes\n",
			__FILE__, __LINE__);
		rt_bomb("Pretending NOT to bomb\n");
		break;
	}

	*Pole_dist = MAGNITUDE(pca_to_pole_vect);		

	return;
}

static void
plot_neighborhood( North_Pole, North_pl_pt, North_pca,
		South_Pole, South_pl_pt, South_pca,
		pointA, pointB, norm, 
		pt, leftA, leftB)
point_t North_Pole;
point_t North_pl_pt;
point_t North_pca;
point_t South_Pole;
point_t South_pl_pt;
point_t South_pca;
point_t pointA;
point_t pointB;
vect_t norm;
point_t pt;
vect_t leftA;
vect_t leftB;
{
	static int plotnum=0;
	FILE *pfd;
	char name[64];
	point_t my_pt;
	vect_t ray;

	sprintf(name, "vert%03d.pl", plotnum++);
	if ((pfd=fopen(name, "w")) == (FILE *)NULL) {
		rt_log("Error opening %s\n", name);
		return;
	} else
		rt_log("overlay %s\n", name);


	/* draw the ray */
	pl_color(pfd, 255, 55, 55);
	pdv_3line(pfd, North_Pole, South_Pole);

	/* draw the area of the face */
	pl_color(pfd, 55, 255, 55);
	pdv_3move(pfd, pt);
	pdv_3cont(pfd, pointA);
	pdv_3cont(pfd, pointB);
	pdv_3cont(pfd, pt);

	/* draw the projections of the pole points */
	pl_color(pfd, 255, 255, 55);
	pdv_3line(pfd, North_Pole, North_pl_pt);
	if ( ! VEQUAL(North_pca, North_pl_pt) ) {
		pdv_3line(pfd, North_Pole, North_pca);
		pdv_3line(pfd, North_pl_pt, North_pca);
	}
	VSUB2(ray, South_Pole, North_Pole);
	VSCALE(ray, ray, -0.125);
	VADD2(my_pt, North_Pole, ray);
	pdv_3move(pfd, my_pt);
	pl_label(pfd, "N");

	pl_color(pfd, 55, 255, 255);
	pdv_3line(pfd, South_Pole, South_pl_pt);
	if ( ! VEQUAL(South_pca, South_pl_pt) ) {
		pdv_3line(pfd, South_Pole, South_pca);
		pdv_3line(pfd, South_pl_pt, South_pca);
	}
	VREVERSE(ray, ray);
	VADD2(my_pt, South_Pole, ray);
	pdv_3move(pfd, my_pt);
	pl_label(pfd, "S");

	pl_color(pfd, 128, 128, 128);
	VADD2(my_pt, pt, norm);
	pdv_3line(pfd, pt, my_pt);

	pl_color(pfd, 192, 192, 192);
	VADD2(my_pt, pointA, leftA);
	pdv_3line(pfd, pointA, my_pt);

	VADD2(my_pt, pointB, leftB);
	pdv_3line(pfd, pointB, my_pt);


	fclose(pfd);
}
				







/*	V E R T E X _ N E I G H B O R H O O D
 *
 *	Examine the vertex neighborhood to classify the ray as IN/ON/OUT of
 *	the NMG both before and after hitting the vertex.
 *
 *
 *	There is a conceptual sphere about the vertex.  For reasons associated
 *	with tolerancing, the sphere has a radius equal to the magnitude of
 *	the maximum dimension of the NMG bounding RPP.
 *
 *	The point where the ray enters this sphere is called the "North Pole"
 *	and the point where it exits the sphere is called the "South Pole"  
 *
 *	For each face which uses this vertex we compute 2 "distance" metrics:
 *
 *	project the "north pole" and "south pole" down onto the plane of the
 *	face:
 *
 *
 *
 *			    		    /
 *				  	   /
 *				     	  /North Pole
 *				     	 / |
 *			Face Normal  ^	/  |
 *			 	     | /   | Projection of North Pole
 *	Plane of face	    	     |/    V to plane
 *  ---------------------------------*-------------------------------
 *     		  Projection of ^   / Vertex		
 *		     South Pole |  /
 *		       to plane | /
 *			    	|/
 *			    	/South Pole
 *			       /
 *			      /
 *			     /
 *			    /
 *			   /
 *			  /
 *			|/_
 *			Ray
 *
 *	If the projected polar point is inside the two edgeuses at this
 *		vertexuse, then
 *		 	the distance to the face is the projection distance.
 *
 *		else
 *			Each of the two edgeuses at this vertexuse 
 *			implies an infinite ray originating at the vertex.
 *			Find the point of closest approach (PCA) of each ray
 *			to the projection point.  For whichever ray passes
 *			closer to the projection point, find the distance
 *			from the original pole point to the PCA.  This is
 *			the "distance to the face" metric for this face from
 *			the given pole point.
 *
 *	We compute this metric for the North and South poles for all faces at
 *	the vertex.  The face with the smallest distance to the north (south)
 * 	pole is used to determine the state of the ray before (after) it hits
 *	the vertex.  
 *
 *	If the north (south) pole is "outside" the plane of the closest face,
 *	then the ray state is "outside" before (after) the ray hits the
 *	vertex.
 */
static void
vertex_neighborhood(rd, vu_p, myhit)
struct ray_data *rd;
struct vertexuse *vu_p;
struct hitmiss *myhit;
{
	struct vertexuse *vu;
	struct faceuse *fu;
	point_t	South_Pole, North_Pole;
	struct faceuse *North_fu, *South_fu;
	struct vertexuse *North_vu, *South_vu;
	point_t North_pl_pt, South_pl_pt;
	point_t North_pca, South_pca;
	double North_dist, South_dist;
	double North_min, South_min;
	double cos_angle;
	vect_t VtoPole;
	double scalar_dist;
	double dimen, t;
	point_t min_pt, max_pt;
	int i;
	vect_t norm, anti_norm;
	vect_t polar_height_vect;
	vect_t leftA, leftB;
	point_t pointA, pointB;
	struct edgeuse *eu;
	vect_t edge_vect;
	int found_faces;

	NMG_CK_VERTEXUSE(vu_p);
	NMG_CK_VERTEX(vu_p->v_p);
	NMG_CK_VERTEX_G(vu_p->v_p->vg_p);

   	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("vertex_neighborhood\n");

	nmg_model_bb( min_pt, max_pt, nmg_find_model( vu_p->up.magic_p ));
	for (dimen= -MAX_FASTF,i=3 ; i-- ; ) {
		t = max_pt[i]-min_pt[i];
		if (t > dimen) dimen = t;
	}

  	VJOIN1(North_Pole, vu_p->v_p->vg_p->coord, -dimen, rd->rp->r_dir);
	VJOIN1(South_Pole, vu_p->v_p->vg_p->coord, dimen, rd->rp->r_dir);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		VPRINT("\tNorth Pole", North_Pole);
		VPRINT("\tSouth Pole", South_Pole);
	}

    	/* There is a conceptual sphere around the vertex
	 * The max dimension of the bounding box for the NMG defines the size.
    	 * The point where the ray enters this sphere is
    	 *  called the North Pole, and the point where it
    	 *  exits is called the South Pole.
    	 */

	South_min = North_min = MAX_FASTF;
	found_faces = 0;

	/* for every use of this vertex */
	for ( RT_LIST_FOR(vu, vertexuse, &vu_p->v_p->vu_hd) ) {
		/* if the parent use is an (edge/loop)use of an 
		 * OT_SAME faceuse that we haven't already processed...
		 */
		NMG_CK_VERTEXUSE(vu);
		NMG_CK_VERTEX(vu->v_p);
		NMG_CK_VERTEX_G(vu->v_p->vg_p);

		fu = nmg_find_fu_of_vu( vu );
		if (fu) {
		  found_faces = 1;
		  if(*vu->up.magic_p == NMG_EDGEUSE_MAGIC &&
		    fu->orientation == OT_SAME ) {

			/* The distance from each "Pole Point" to the faceuse
			 *  is the commodity in which we are interested.
			 *
			 * A pole point is projected
			 * This is either the distance to the plane (if the
			 * projection of the point into the plane lies within
			 * the angle of the two edgeuses at this vertex) or
			 * we take the distance from the pole to the PCA of
			 * the closest edge.
			 */
			NMG_GET_FU_NORMAL( norm, fu );
			VREVERSE(anti_norm, norm);
		
		    	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				VPRINT("\tchecking face", norm);

			/* project the north pole onto the plane */
			VSUB2(polar_height_vect, vu->v_p->vg_p->coord, North_Pole);
			scalar_dist = VDOT(norm, polar_height_vect);
		
			/* project the poles down onto the plane */
			VJOIN1(North_pl_pt, North_Pole, scalar_dist, norm);
			VJOIN1(South_pl_pt, South_Pole, scalar_dist, anti_norm);
		    	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		    		VPRINT("\tNorth Plane Point", North_pl_pt);
		    		VPRINT("\tSouth Plane Point", South_pl_pt);
		    	}
			/* Find points on sphere in direction of edges
			 * (away from vertex along edge)
			 */
			do
				eu = RT_LIST_PNEXT_CIRC(edgeuse, vu->up.eu_p);
			while (eu->vu_p->v_p == vu->v_p);
			nmg_find_eu_leftvec(leftA, vu->up.eu_p);
			VSUB2(edge_vect, eu->vu_p->v_p->vg_p->coord,
				vu->v_p->vg_p->coord);
			VUNITIZE(edge_vect);
			VJOIN1(pointA, vu->v_p->vg_p->coord, dimen, edge_vect)

			do
				eu = RT_LIST_PLAST_CIRC(edgeuse, vu->up.eu_p);
			while (eu->vu_p->v_p == vu->v_p);
			nmg_find_eu_leftvec(leftB, eu);
			VSUB2(edge_vect, eu->vu_p->v_p->vg_p->coord,
				vu->v_p->vg_p->coord);
			VUNITIZE(edge_vect);
			VJOIN1(pointB, vu->v_p->vg_p->coord, dimen, edge_vect)


		    	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		    		VPRINT("\tLeftA", leftA);
		    		VPRINT("\tLeftB", leftB);
		    	}
		    	/* find distance of face to North Pole */
			get_pole_dist_to_face(rd, vu,
				North_Pole, North_pl_pt, &North_dist, North_pca,
				pointA, leftA, pointB, leftB,
				polar_height_vect, "North");

			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tDist north pole<=>face %g\n", North_dist);

			if (North_min > North_dist) {
				North_min = North_dist;
				North_fu = fu;
				North_vu = vu;
				if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			    		rt_log("New North Pole Min: %g\n", North_min);
			}


		    	/* find distance of face to South Pole */
			get_pole_dist_to_face(rd, vu,
				South_Pole, South_pl_pt, &South_dist, South_pca,
				pointA, leftA, pointB, leftB,
				polar_height_vect, "South");

			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tDist south pole<=>face %g\n", South_dist);

			if (South_min > South_dist) {
				South_min = South_dist;
				South_fu = fu;
				South_vu = vu;
				if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			    		rt_log("New South Pole Min: %g\n", South_min);
			}


			if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
				plot_neighborhood( North_Pole, North_pl_pt, North_pca,
						   South_Pole, South_pl_pt, South_pca,
						   pointA, pointB, norm, 
						   vu_p->v_p->vg_p->coord,
						   leftA, leftB);
			}
		    }
		} else {
			if (!found_faces)
				South_vu = North_vu = vu;
		}
	}

	if (!found_faces) {
		/* we've found a vertex floating in space */
		myhit->outbound_use = myhit->inbound_use = (long *)North_vu;
		myhit->in_out = HMG_HIT_ANY_ANY;
		VREVERSE(myhit->hit.hit_normal, rd->rp->r_dir);
		return;
	}


	NMG_GET_FU_NORMAL( norm, North_fu );
	VMOVE(myhit->inbound_norm, norm);
	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("North Pole Min: %g to %g %g %g\n", North_min,
			norm[0], norm[1],norm[2]);

	/* compute status of ray as it is in-bound on the vertex */
	VSUB2(VtoPole, North_Pole, vu_p->v_p->vg_p->coord);
	cos_angle = VDOT(norm, VtoPole);
	if (RT_VECT_ARE_PERP(cos_angle, rd->tol))
		myhit->in_out |= NMG_RAY_STATE_ON << 4;
	else if (cos_angle > 0.0)
		myhit->in_out |= NMG_RAY_STATE_OUTSIDE << 4;
	else
		myhit->in_out |= NMG_RAY_STATE_INSIDE << 4;


	/* compute status of ray as it is out-bound from the vertex */
	NMG_GET_FU_NORMAL( norm, South_fu );
	VMOVE(myhit->outbound_norm, norm);
	VSUB2(VtoPole, South_Pole, vu_p->v_p->vg_p->coord);
	cos_angle = VDOT(norm, VtoPole);
	if (RT_VECT_ARE_PERP(cos_angle, rd->tol))
		myhit->in_out |= NMG_RAY_STATE_ON;
	else if (cos_angle > 0.0)
		myhit->in_out |= NMG_RAY_STATE_OUTSIDE;
	else
		myhit->in_out |= NMG_RAY_STATE_INSIDE;


	myhit->inbound_use = (long *)North_vu;
	myhit->outbound_use = (long *)South_vu;

	switch (myhit->in_out) {
	case HMG_HIT_IN_IN:	/* fallthrough */
	case HMG_HIT_OUT_OUT:	/* fallthrough */
	case HMG_HIT_IN_ON:	/* fallthrough */
	case HMG_HIT_ON_IN:	/* two hits */
		myhit->hit.hit_private = (genptr_t)North_vu;
		break;
	case HMG_HIT_IN_OUT:	/* one hit - outbound */
	case HMG_HIT_ON_OUT:	/* one hit - outbound */
		myhit->hit.hit_private = (genptr_t)South_vu;
		break;
	case HMG_HIT_OUT_IN:	/* one hit - inbound */
	case HMG_HIT_OUT_ON:	/* one hit - inbound */
		myhit->hit.hit_private = (genptr_t)North_vu;
		break;
	default:
		rt_log("%s %d: Bad vertex in/out state?\n",
			__FILE__, __LINE__);
		rt_bomb("bombing\n");
		break;

	}
}






/*
 *  Once it has been decided that the ray hits the vertex, 
 *  this routine takes care of recording that fact.
 */
static void
ray_hit_vertex(rd, vu_p, status)
struct ray_data *rd;
struct vertexuse *vu_p;
int status;
{
	struct hitmiss *myhit;
	vect_t v;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("ray_hit_vertex\n");

	if (myhit = NMG_INDEX_GET(rd->hitmiss, vu_p->v_p)) {
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_MAGIC))
			return;
		/* oops, we have to change a MISS into a HIT */
		RT_LIST_DEQUEUE(&myhit->l);
	} else {
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, vu_p->v_p, myhit);
		myhit->outbound_use = (long *)vu_p;
		myhit->inbound_use = (long *)vu_p;
	}

	/* v = vector from ray point to hit vertex */
	VSUB2( v, rd->rp->r_pt, vu_p->v_p->vg_p->coord);
	myhit->hit.hit_dist = MAGNITUDE(v);	/* distance along ray */
	VMOVE(myhit->hit.hit_point, vu_p->v_p->vg_p->coord);
	myhit->hit.hit_private = (genptr_t) vu_p->v_p;

	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_MAGIC);

	/* XXX need to compute neighborhood of vertex so that ray
	 * can be classified as in/on/out before and after the vertex.
	 *
	 */
	vertex_neighborhood(rd, vu_p, myhit);

	/* XXX we re really should temper the results of vertex_neighborhood()
	 * with the knowledge of "status"
	 */

	hit_ins(rd, myhit);
	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		plvu(vu_p);

	NMG_CK_HITMISS(myhit);
}


/*	I S E C T _ R A Y _ V E R T E X U S E
 *
 *	Called in one of the following situations:
 *		1)	Zero length edge
 *		2)	Vertexuse child of Loopuse
 *		3)	Vertexuse child of Shell
 *
 *	return:
 *		1 vertex was hit
 *		0 vertex was missed
 */
static int
isect_ray_vertexuse(rd, vu_p)
struct ray_data *rd;
struct vertexuse *vu_p;
{
	struct hitmiss *myhit;
	double ray_vu_dist;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("isect_ray_vertexuse\n");

	NMG_CK_VERTEXUSE(vu_p);
	NMG_CK_VERTEX(vu_p->v_p);
	NMG_CK_VERTEX_G(vu_p->v_p->vg_p);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("nmg_isect_ray_vertexuse %g %g %g", 
			vu_p->v_p->vg_p->coord[0], 
			vu_p->v_p->vg_p->coord[1], 
			vu_p->v_p->vg_p->coord[2]);

	if (myhit = NMG_INDEX_GET(rd->hitmiss, vu_p->v_p)) {
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_MAGIC)) {
			/* we've previously hit this vertex */
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously hit\n");
			return(1);
		} else {
			/* we've previously missed this vertex */
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously missed\n");
			return 0;
		}
	}

	/* intersect ray with vertex */
	ray_vu_dist = rt_dist_line_point(rd->rp->r_pt, rd->rp->r_dir,
					 vu_p->v_p->vg_p->coord);

	if (ray_vu_dist > rd->tol->dist) {
		/* ray misses vertex */
		ray_miss_vertex(rd, vu_p);
		return 0;
	}

	/* ray hits vertex */
	ray_hit_vertex(rd, vu_p, NMG_VERT_UNKNOWN);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log(" Ray hits vertex, dist %g (priv=x%x, v magic=x%x)\n",
			myhit->hit.hit_dist,
			myhit->hit.hit_private,
			vu_p->v_p->magic);

		print_hitlist(rd->hitmiss[NMG_HIT_LIST]);
	}

	return 1;
}


/*
 * As the name implies, this routine is called when the ray and an edge are
 * colinear.  It handles marking the verticies as hit, remembering that this
 * is a seg_in/seg_out pair, and builds the hit on the edge.
 *
 */
static void
colinear_edge_ray(rd, eu_p)
struct ray_data *rd;
struct edgeuse *eu_p;
{
	struct hitmiss *vhit1, *vhit2, *myhit;


	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("\t - colinear_edge_ray\n");

	vhit1 = NMG_INDEX_GET(rd->hitmiss, eu_p->vu_p->v_p);
	vhit2 = NMG_INDEX_GET(rd->hitmiss, eu_p->eumate_p->vu_p->v_p);

	/* record the hit on each vertex, and remember that these two hits
	 * should be kept together.
	 */
	if (vhit1->hit.hit_dist > vhit2->hit.hit_dist) {
		ray_hit_vertex(rd, eu_p->vu_p, NMG_VERT_ENTER);
		vhit1->start_stop = NMG_HITMISS_SEG_OUT;

		ray_hit_vertex(rd, eu_p->eumate_p->vu_p, NMG_VERT_LEAVE);
		vhit2->start_stop = NMG_HITMISS_SEG_IN;
	} else {
		ray_hit_vertex(rd, eu_p->vu_p, NMG_VERT_LEAVE);
		vhit1->start_stop = NMG_HITMISS_SEG_IN;

		ray_hit_vertex(rd, eu_p->eumate_p->vu_p, NMG_VERT_ENTER);
		vhit2->start_stop = NMG_HITMISS_SEG_OUT;
	}
	vhit1->other = vhit2;
	vhit2->other = vhit1;

	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);
	myhit->hit.hit_private = (genptr_t)eu_p->e_p;
	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC);
	RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
	NMG_CK_HITMISS(myhit);
	NMG_CK_HITMISS_LISTS(myhit, rd);

	return;
}

/*
 *  When a vertex at an end of an edge gets hit by the ray, this macro
 *  is used to build the hit structures for the vertex and the edge.
 */
#define HIT_EDGE_VERTEX(rd, eu_p, vu_p) {\
	if (rt_g.NMG_debug & DEBUG_RT_ISECT) rt_log("hit_edge_vertex\n"); \
	if (*eu_p->up.magic_p == NMG_SHELL_MAGIC || \
	    (*eu_p->up.magic_p == NMG_LOOPUSE_MAGIC && \
	     *eu_p->up.lu_p->up.magic_p == NMG_SHELL_MAGIC)) \
		ray_hit_vertex(rd, vu_p, NMG_VERT_ENTER_LEAVE); \
	else \
		ray_hit_vertex(rd, vu_p, NMG_VERT_UNKNOWN); \
	GET_HITMISS(myhit); \
	NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit); \
	myhit->hit.hit_private = (genptr_t)eu_p->e_p; \
	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC); \
	RT_LIST_INSERT(&rd->rd_miss, &myhit->l); \
	NMG_CK_HITMISS(myhit); \
	NMG_CK_HITMISS_LISTS(myhit, rd); }



/*
 *	Compute the "ray state" before and after the ray encounters a
 *	hit-point on an edge.
 */
static void
edge_hit_ray_state(rd, eu, myhit)
struct ray_data *rd;
struct edgeuse *eu;
struct hitmiss *myhit;
{
	double cos_angle;
	double inb_cos_angle = 2.0;
	double outb_cos_angle = -1.0;
	struct faceuse *fu;
	struct faceuse *inb_fu;
	struct faceuse *outb_fu;
	struct edgeuse *inb_eu;
	struct edgeuse *outb_eu;
	struct edgeuse *eu_p;
	struct edgeuse *fu_eu;
	vect_t edge_left;
	vect_t eu_vec;
	vect_t norm;
	int faces_found;
	struct hitmiss *a_hit;

	NMG_CK_HITMISS_LISTS(a_hit, rd);
	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		eu_p = RT_LIST_PNEXT_CIRC(edgeuse, eu);
		rt_log("edge_hit_ray_state(%g %g %g -> %g %g %g _vs_ %g %g %g)\n",
			eu->vu_p->v_p->vg_p->coord[0],
			eu->vu_p->v_p->vg_p->coord[1],
			eu->vu_p->v_p->vg_p->coord[2],
			eu_p->vu_p->v_p->vg_p->coord[0],
			eu_p->vu_p->v_p->vg_p->coord[1],
			eu_p->vu_p->v_p->vg_p->coord[2],
			rd->rp->r_dir[0],
			rd->rp->r_dir[1],
			rd->rp->r_dir[2]);
	}

	myhit->in_out = 0;


	faces_found = 0;
	eu_p = eu->e_p->eu_p;
	do {
		if (fu=nmg_find_fu_of_eu(eu_p)) {
			fu_eu = eu_p;
			faces_found = 1;
			if (fu->orientation == OT_OPPOSITE &&
			    fu->fumate_p->orientation == OT_SAME) {
				fu = fu->fumate_p;
			    	fu_eu = eu_p->eumate_p;
			    }
			if (fu->orientation != OT_SAME) {
				rt_log("%s[%d]: I can't seem to find an OT_SAME faceuse\nThis must be a `dangling' face  I'll skip it\n", __FILE__, __LINE__);
				goto next_edgeuse;
			}

			if (nmg_find_eu_leftvec(edge_left, eu_p)) {
				rt_log("edgeuse not part of faceuse");
				goto next_edgeuse;
			}

			if (! (NMG_3MANIFOLD & 
			       NMG_MANIFOLDS(rd->manifolds, fu->f_p)) ){
				rt_log("This is not a 3-Manifold face.  I'll skip it\n");
				goto next_edgeuse;
			}

			cos_angle = VDOT(edge_left, rd->rp->r_dir);

			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("left_vect:(%g %g %g)  cos_angle:%g\n",
					edge_left[0], edge_left[1],
					edge_left[2], cos_angle);

			if (cos_angle < inb_cos_angle) {
				inb_cos_angle = cos_angle;
				inb_fu = fu;
				inb_eu = fu_eu;
				if (rt_g.NMG_debug & DEBUG_RT_ISECT)
					rt_log("New inb cos_angle %g\n", inb_cos_angle);
			}
			if (cos_angle > outb_cos_angle) {
				outb_cos_angle = cos_angle;
				outb_fu = fu;
				outb_eu = fu_eu;
				if (rt_g.NMG_debug & DEBUG_RT_ISECT)
					rt_log("New outb cos_angle %g\n", outb_cos_angle);
			}
		}
next_edgeuse:	eu_p = eu_p->eumate_p->radial_p;
	} while (eu_p != eu->e_p->eu_p);

	NMG_CK_HITMISS_LISTS(a_hit, rd);

	if (!faces_found) {
		/* we hit a wire edge */
		myhit->in_out = HMG_HIT_ANY_ANY;
		myhit->hit.hit_private = (genptr_t)eu;
		myhit->inbound_use = myhit->outbound_use = (long *)eu;

		eu_p = RT_LIST_PNEXT_CIRC(edgeuse, eu);
		VSUB2(eu_vec, eu->vu_p->v_p->vg_p->coord,
			eu_p->vu_p->v_p->vg_p->coord);
		VCROSS(edge_left, eu_vec, rd->rp->r_dir);
		VCROSS(myhit->inbound_norm, eu_vec, edge_left);
		if (VDOT(myhit->inbound_norm, rd->rp->r_dir) > 0.0) {
			VREVERSE(myhit->inbound_norm,myhit->inbound_norm);
		}
		VMOVE(myhit->outbound_norm, myhit->inbound_norm);

		return;
	}

	/* inb_fu is closest to ray on outbound side
	 * outb_fu is closest to ray on inbound side
	 */

	/* Compute the ray state on the inbound side */
	NMG_GET_FU_NORMAL(norm, inb_fu);
	VMOVE(myhit->inbound_norm, norm);
	if (MAGSQ(norm) < VDIVIDE_TOL)
		rt_bomb("null normal!\n");

	cos_angle = VDOT(norm, rd->rp->r_dir);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		VPRINT("\ninb face normal", norm);
		rt_log("cos_angle wrt ray direction: %g\n", cos_angle);
	}

	if (RT_VECT_ARE_PERP(cos_angle, rd->tol))
		myhit->in_out |= NMG_RAY_STATE_ON << 4;
	else if (cos_angle < 0.0)
		myhit->in_out |= NMG_RAY_STATE_OUTSIDE << 4;
	else /* (cos_angle > 0.0) */
		myhit->in_out |= NMG_RAY_STATE_INSIDE << 4;


	/* Compute the ray state on the outbound side */
	NMG_GET_FU_NORMAL(norm, outb_fu);
	VMOVE(myhit->outbound_norm, norm);
	cos_angle = VDOT(norm, rd->rp->r_dir);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		VPRINT("\noutb face normal", norm);
		rt_log("cos_angle wrt ray direction: %g\n", cos_angle);
	}

	if (RT_VECT_ARE_PERP(cos_angle, rd->tol))
		myhit->in_out |= NMG_RAY_STATE_ON;
	else if (cos_angle > 0.0)
		myhit->in_out |= NMG_RAY_STATE_OUTSIDE;
	else /* (cos_angle < 0.0) */
		myhit->in_out |= NMG_RAY_STATE_INSIDE;


	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log("myhit->in_out: 0x%02x/", myhit->in_out);
		switch(myhit->in_out) {
		case HMG_HIT_IN_IN:
			rt_log("IN_IN\n"); break;
		case HMG_HIT_IN_OUT:
			rt_log("IN_OUT\n"); break;
		case HMG_HIT_OUT_IN:
			rt_log("OUT_IN\n"); break;
		case HMG_HIT_OUT_OUT:
			rt_log("OUT_OUT\n"); break;
		case HMG_HIT_IN_ON:
			rt_log("IN_ON\n"); break;
		case HMG_HIT_ON_IN:
			rt_log("ON_IN\n"); break;
		case HMG_HIT_OUT_ON:
			rt_log("OUT_ON\n"); break;
		case HMG_HIT_ON_OUT:
			rt_log("ON_OUT\n"); break;
		default:
			rt_log("?_?\n"); break;
		}
	}


	switch(myhit->in_out) {
	case HMG_HIT_IN_IN:	/* fallthrough */
	case HMG_HIT_OUT_OUT:	/* fallthrough */
	case HMG_HIT_IN_ON:	/* fallthrough */
	case HMG_HIT_ON_IN:	/* two hits */
		myhit->inbound_use = (long *)inb_eu;
		myhit->outbound_use = (long *)outb_eu;
		myhit->hit.hit_private = (genptr_t)inb_eu;
		break;
	case HMG_HIT_IN_OUT:	/* one hit - outbound */
	case HMG_HIT_ON_OUT:	/* one hit - outbound */
		myhit->inbound_use = (long *)outb_eu;
		myhit->outbound_use = (long *)outb_eu;
		myhit->hit.hit_private = (genptr_t)outb_eu;
		break;
	case HMG_HIT_OUT_IN:	/* one hit - inbound */
	case HMG_HIT_OUT_ON:	/* one hit - inbound */
		myhit->inbound_use = (long *)inb_eu;
		myhit->outbound_use = (long *)inb_eu;
		myhit->hit.hit_private = (genptr_t)inb_eu;
		break;
	default:
		rt_log("%s %d: Bad edge in/out state?\n", __FILE__, __LINE__);
		rt_bomb("bombing\n");
		break;
	}
	NMG_CK_HITMISS_LISTS(a_hit, rd);
}

/*
 *
 * record a hit on an edge.
 *
 */
static void
ray_hit_edge(rd, eu_p, dist_along_ray, pt)
struct ray_data *rd;
struct edgeuse *eu_p;
double dist_along_ray;
point_t pt;
{
	struct hitmiss *myhit;
	struct hitmiss *a_hit;
	ray_miss_vertex(rd, eu_p->vu_p);
	ray_miss_vertex(rd, eu_p->eumate_p->vu_p);
	NMG_CK_HITMISS_LISTS(a_hit, rd);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) rt_log("\t - HIT edge 0x%08x\n", eu_p->e_p);

	if (myhit = NMG_INDEX_GET(rd->hitmiss, eu_p->e_p)) {
		switch (((struct rt_list *)myhit)->magic) {
		case NMG_RT_MISS_MAGIC:
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tedge previously missed, changing to hit\n");
			RT_LIST_DEQUEUE(&myhit->l);
			NMG_CK_HITMISS_LISTS(a_hit, rd);
			break;
		case NMG_RT_HIT_SUB_MAGIC:
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tedge vertex previously hit\n");
			return;
		case NMG_RT_HIT_MAGIC:
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tedge previously hit\n");
			return;
		default:
			break;
		}
	} else {
		GET_HITMISS(myhit);
	}

	/* create hit structure for this edge */
	NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);
	myhit->outbound_use = (long *)eu_p;
	myhit->inbound_use = (long *)eu_p;

	/* build the hit structure */
	myhit->hit.hit_dist = dist_along_ray;
	VMOVE(myhit->hit.hit_point, pt)
	myhit->hit.hit_private = (genptr_t) eu_p->e_p;

	edge_hit_ray_state(rd, eu_p, myhit);

	NMG_CK_HITMISS_LISTS(a_hit, rd);
	hit_ins(rd, myhit);
	NMG_CK_HITMISS_LISTS(a_hit, rd);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		register struct faceuse *fu;
		if ((fu=nmg_find_fu_of_eu( eu_p )))
			plfu(fu, rd->rp->r_pt, myhit->hit.hit_point);
		else
			pleu(eu_p, rd->rp->r_pt, myhit->hit.hit_point);
	}

	NMG_CK_HITMISS(myhit);
	NMG_CK_HITMISS_LISTS(a_hit, rd);
}
isect_ray_cnurb(rd, eu_p)
struct ray_data *rd;
struct edgeuse *eu_p;
{
}
isect_ray_lseg(rd, eu_p)
struct ray_data *rd;
struct edgeuse *eu_p;
{
	int status;
	struct hitmiss *myhit;
	int vhit1, vhit2;
	double dist_along_ray;

	status = rt_isect_line_lseg( &dist_along_ray,
			rd->rp->r_pt, rd->rp->r_dir, 
			eu_p->vu_p->v_p->vg_p->coord,
			eu_p->eumate_p->vu_p->v_p->vg_p->coord,
			rd->tol);

	switch (status) {
	case -4 :
		/* Zero length edge.  The routine rt_isect_line_lseg() can't
		 * help us.  Intersect the ray with each vertex.  If either
		 * vertex is hit, then record that the edge has sub-elements
		 * which where hit.  Otherwise, record the edge as being
		 * missed.
		 */

		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("\t - Zero length edge\n");

		vhit1 = isect_ray_vertexuse(rd, eu_p->vu_p);
		vhit2 = isect_ray_vertexuse(rd, eu_p->eumate_p->vu_p);

		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);

		myhit->hit.hit_private = (genptr_t)eu_p->e_p;

		if (vhit1 || vhit2) {
			/* we hit the vertex */
			RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC);
		} else {
			/* both vertecies were missed, so edge is missed */
			RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		}
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		NMG_CK_HITMISS(myhit);
		NMG_CK_HITMISS_LISTS(myhit, rd);
		break;
	case -3 :	/* fallthrough */
	case -2 :
		/* The ray misses the edge segment, but hits the infinite
		 * line of the edge to one end of the edge segment.  This
		 * is an exercise in tabulating the nature of the miss.
		 */

		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("\t - Miss edge, \"hit\" line\n");
		/* record the fact that we missed each vertex */
		(void)ray_miss_vertex(rd, eu_p->vu_p);
		(void)ray_miss_vertex(rd, eu_p->eumate_p->vu_p);

		/* record the fact that we missed the edge */
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);
		myhit->hit.hit_private = (genptr_t)eu_p->e_p;

		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		NMG_CK_HITMISS(myhit);
		NMG_CK_HITMISS_LISTS(myhit, rd);
		break;
	case -1 : /* just plain missed the edge/line */
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("\t - Miss edge/line\n");

		ray_miss_vertex(rd, eu_p->vu_p);
		ray_miss_vertex(rd, eu_p->eumate_p->vu_p);

		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, eu_p->e_p, myhit);
		myhit->hit.hit_private = (genptr_t)eu_p->e_p;

		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		NMG_CK_HITMISS(myhit);
		NMG_CK_HITMISS_LISTS(myhit, rd);

		break;
	case 0 :  /* oh joy.  Lines are co-linear */
		colinear_edge_ray(rd, eu_p);
		break;
	case 1 :
		HIT_EDGE_VERTEX(rd, eu_p, eu_p->vu_p);
		break;
	case 2 :
		HIT_EDGE_VERTEX(rd, eu_p, eu_p->eumate_p->vu_p);
		break;
	case 3 : /* a hit on an edge */
		{
			point_t pt;

			VJOIN1(pt, rd->rp->r_pt, dist_along_ray, rd->rp->r_dir);

			ray_hit_edge(rd, eu_p, dist_along_ray, pt);

			break;
		}
	}
}
/*	I S E C T _ R A Y _ E D G E U S E
 *
 *	Intersect ray with edgeuse.  If they pass within tolerance, a hit
 *	is generated.
 *
 */
static void
isect_ray_edgeuse(rd, eu_p)
struct ray_data *rd;
struct edgeuse *eu_p;
{
	int status;
	struct hitmiss *myhit;
	int vhit1, vhit2;

	NMG_CK_EDGEUSE(eu_p);
	NMG_CK_EDGEUSE(eu_p->eumate_p);
	NMG_CK_EDGE(eu_p->e_p);
	NMG_CK_VERTEXUSE(eu_p->vu_p);
	NMG_CK_VERTEXUSE(eu_p->eumate_p->vu_p);
	NMG_CK_VERTEX(eu_p->vu_p->v_p);
	NMG_CK_VERTEX(eu_p->eumate_p->vu_p->v_p);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log("isect_ray_edgeuse (%g %g %g) -> (%g %g %g)",
			eu_p->vu_p->v_p->vg_p->coord[0],
			eu_p->vu_p->v_p->vg_p->coord[1],
			eu_p->vu_p->v_p->vg_p->coord[2],
			eu_p->eumate_p->vu_p->v_p->vg_p->coord[0],
			eu_p->eumate_p->vu_p->v_p->vg_p->coord[1],
			eu_p->eumate_p->vu_p->v_p->vg_p->coord[2]);
	}

	if (eu_p->e_p != eu_p->eumate_p->e_p)
		rt_bomb("edgeuse mate has step-father\n");

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("\n\tLooking for previous hit on edge 0x%08x ...\n",
			eu_p->e_p);

	if (myhit = NMG_INDEX_GET(rd->hitmiss, eu_p->e_p)) {
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_MAGIC)) {
			/* previously hit vertex or edge */
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tedge previously hit\n");
			return;
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_HIT_SUB_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tvertex of edge previously hit\n");
			return;
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit, NMG_RT_MISS_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("\tedge previously missed\n");
			return;
		} else {
			nmg_rt_bomb(rd, "what happened?\n");
		}
	}

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("\t No previous hit\n");

	switch (*eu_p->g.magic_p) {
	case NMG_EDGE_G_LSEG_MAGIC:
		isect_ray_lseg(rd, eu_p);
		break;
	case NMG_EDGE_G_CNURB_MAGIC:
		isect_ray_cnurb(rd, eu_p);
		break;
	}
}






/*	I S E C T _ R A Y _ L O O P U S E
 *
 */
static void
isect_ray_loopuse(rd, lu_p)
struct ray_data *rd;
struct loopuse *lu_p;
{
	struct edgeuse *eu_p;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("isect_ray_loopuse( 0x%08x, loop:0x%08x)\n", rd, lu_p->l_p);

	NMG_CK_LOOPUSE(lu_p);
	NMG_CK_LOOP(lu_p->l_p);
	NMG_CK_LOOP_G(lu_p->l_p->lg_p);

	if (RT_LIST_FIRST_MAGIC(&lu_p->down_hd) == NMG_EDGEUSE_MAGIC) {
		for (RT_LIST_FOR(eu_p, edgeuse, &lu_p->down_hd)) {
			isect_ray_edgeuse(rd, eu_p);
		}
		return;

	} else if (RT_LIST_FIRST_MAGIC(&lu_p->down_hd)!=NMG_VERTEXUSE_MAGIC) {
		rt_log("in %s at %d", __FILE__, __LINE__);
		nmg_rt_bomb(rd, " bad loopuse child magic");
	}

	/* loopuse child is vertexuse */

	(void) isect_ray_vertexuse(rd,
		RT_LIST_FIRST(vertexuse, &lu_p->down_hd));
}



#define FACE_MISS(rd, f_p) {\
	struct hitmiss *myhit; \
	GET_HITMISS(myhit); \
	NMG_INDEX_ASSIGN(rd->hitmiss, f_p, myhit); \
	myhit->hit.hit_private = (genptr_t)f_p; \
	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC); \
	RT_LIST_INSERT(&rd->rd_miss, &myhit->l); \
	NMG_CK_HITMISS(myhit); \
	NMG_CK_HITMISS_LISTS(myhit, rd); }



static void
eu_touch_func(eu, fpi)
struct edgeuse *eu;
struct fu_pt_info *fpi;
{
	struct edgeuse *eu_next;
	struct ray_data *rd;
	struct hitmiss *myhit;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_EDGE(eu->e_p);
	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);

	eu_next = RT_LIST_PNEXT_CIRC(edgeuse, eu);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("eu_touch(%g %g %g -> %g %g %g\n",
			eu->vu_p->v_p->vg_p->coord[0],
			eu->vu_p->v_p->vg_p->coord[1],
			eu->vu_p->v_p->vg_p->coord[2],
			eu_next->vu_p->v_p->vg_p->coord[0],
			eu_next->vu_p->v_p->vg_p->coord[1],
			eu_next->vu_p->v_p->vg_p->coord[2]);

	rd = (struct ray_data *)fpi->priv;
	rd->face_subhit = 1;

	NMG_CK_HITMISS_LISTS(myhit, rd);
	ray_hit_edge(rd, eu, rd->ray_dist_to_plane, fpi->pt);
	NMG_CK_HITMISS_LISTS(myhit, rd);

}


static void
vu_touch_func(vu, fpi)
struct vertexuse *vu;
struct fu_pt_info *fpi;
{
	struct ray_data *rd;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("vu_touch(%g %g %g)\n",
			vu->v_p->vg_p->coord[0],
			vu->v_p->vg_p->coord[1],
			vu->v_p->vg_p->coord[2]);

	rd = (struct ray_data *)fpi->priv;

	rd->face_subhit = 1;
	ray_hit_vertex(rd, vu, NMG_VERT_UNKNOWN);
}

static void
record_face_hit(rd, myhit, plane_pt, dist, fu_p, norm, a_hit)
struct ray_data *rd;
struct hitmiss *myhit;
point_t plane_pt;
double dist;
struct faceuse *fu_p;
plane_t norm;
{
	double cos_angle;

	RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_MAGIC);
	myhit->outbound_use = (long *)fu_p;
	myhit->inbound_use = (long *)fu_p;


	VMOVE(myhit->hit.hit_point, plane_pt);

	/* also rd->ray_dist_to_plane */
	myhit->hit.hit_dist = dist; 
	myhit->hit.hit_private = (genptr_t)fu_p->f_p;


	/* compute what the ray-state is before and after this
	 * encountering this hit point.
	 */
	cos_angle = VDOT(norm, rd->rp->r_dir);
	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		VPRINT("face Normal", norm);
		rt_log("cos_angle wrt ray direction: %g\n", cos_angle);
	}


	if ( ! (NMG_MANIFOLDS(rd->manifolds, fu_p) & NMG_3MANIFOLD) ) {
		myhit->in_out = HMG_HIT_ANY_ANY;

		if (cos_angle < rd->tol->perp) {
			VMOVE(myhit->inbound_norm, norm);
			VREVERSE(myhit->outbound_norm, norm);
			myhit->inbound_use = (long *)fu_p;
			myhit->outbound_use = (long *)fu_p->fumate_p;
		} else {
			VREVERSE(myhit->inbound_norm, norm);
			VMOVE(myhit->outbound_norm, norm);
			myhit->inbound_use = (long *)fu_p->fumate_p;
			myhit->outbound_use = (long *)fu_p;
		}

		return;
	}


	switch (fu_p->orientation) {
	case OT_SAME:
		if (RT_VECT_ARE_PERP(cos_angle, rd->tol)) {
			/* perpendicular? */
			rt_log("%s[%d]: Ray is in plane of face?\n",
				__FILE__, __LINE__);
				rt_bomb("I quit\n");
		} else if (cos_angle > 0.0) {
			myhit->in_out = HMG_HIT_IN_OUT;
			VREVERSE(myhit->outbound_norm, norm);
			myhit->outbound_use = (long *)fu_p;
			myhit->inbound_use = (long *)fu_p;
		} else {
			myhit->in_out = HMG_HIT_OUT_IN;
			VMOVE(myhit->inbound_norm, norm);
			myhit->inbound_use = (long *)fu_p;
			myhit->outbound_use = (long *)fu_p;
		}
		break;
	case OT_OPPOSITE:
		if (RT_VECT_ARE_PERP(cos_angle, rd->tol)) {
			/* perpendicular? */
			rt_log("%s[%d]: Ray is in plane of face?\n",
				__FILE__, __LINE__);
				rt_bomb("I quit\n");
		} else if (cos_angle > 0.0) {
			myhit->in_out = HMG_HIT_OUT_IN;
			VREVERSE(myhit->inbound_norm, norm);
			myhit->inbound_use = (long *)fu_p;
			myhit->outbound_use = (long *)fu_p;
		} else {
			myhit->in_out = HMG_HIT_IN_OUT;
			VMOVE(myhit->outbound_norm, norm);
			myhit->inbound_use = (long *)fu_p;
			myhit->outbound_use = (long *)fu_p;
		}
		break;
	default:
		rt_log("%s %d:face orientation not SAME/OPPOSITE\n",
			__FILE__, __LINE__);
		rt_bomb("Crash and burn\n");
	}

	hit_ins(rd, myhit);
	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		plfu(fu_p, rd->rp->r_pt, myhit->hit.hit_point);

	NMG_CK_HITMISS(myhit);

}

void
isect_ray_snurb_face(rd, fu_p, fg_p)
struct ray_data *rd;
struct faceuse *fu_p;
struct face_g_snurb *fg_p;
{
	rt_bomb("isect_ray_snurb_face()\n");
}

void
isect_ray_planar_face(rd, fu_p, fg_p)
struct ray_data *rd;
struct faceuse *fu_p;
struct face_g_plane *fg_p;
{
	plane_t			norm;
	int			code;
	fastf_t			dist;
	struct hitmiss		*myhit;
	struct fu_pt_info	*fpi;
	point_t			plane_pt;
	struct loopuse		*lu_p;

	/* perform the geometric intersection of the ray with the plane 
	 * of the face.
	 */
	NMG_GET_FU_PLANE( norm, fu_p );
	code = rt_isect_line3_plane(&dist, rd->rp->r_pt, rd->rp->r_dir,
			norm, rd->tol);


	if (code < 0) {
		/* ray is parallel to halfspace and (-1)inside or (-2)outside
		 * the halfspace.
		 */
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("\tray misses halfspace of plane\n");

		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
		myhit->hit.hit_private = (genptr_t)fu_p->f_p;
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		NMG_CK_HITMISS(myhit);

		return;
	} else if (code == 0) {
		/* XXX gack!  ray lies in plane.  
		 * In leiu of doing 2D intersection we define such rays
		 * as "missing" the face.  We rely on other faces to generate
		 * hit points.
		 */
		rt_log("\tWarning:  Ignoring ray in plane of face (NOW A MISS) XXX\n");
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
		myhit->hit.hit_private = (genptr_t)fu_p->f_p;
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		NMG_CK_HITMISS(myhit);

		return;
	}


	/* ray hits plane:
	 *
	 * Compute the ray/plane intersection point.
	 * Then compute whether this point lies within the area of the face.
	 */

	VJOIN1(plane_pt, rd->rp->r_pt, dist, rd->rp->r_dir);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		rt_log("\tray (%g %g %g) (-> %g %g %g)\n",
			rd->rp->r_pt[0],
			rd->rp->r_pt[1],
			rd->rp->r_pt[2],
			rd->rp->r_dir[0],
			rd->rp->r_dir[1],
			rd->rp->r_dir[2]);

		VPRINT("\tplane/ray intersection point", plane_pt);
		rt_log("\tdistance along ray to intersection point %g\n", dist);
	}


	/* determine if the plane point is in or out of the face, and
	 * if it is within tolerance of any of the elements of the faceuse.
	 *
	 * The value of "rd->face_subhit" will be set non-zero if either
	 * eu_touch_func or vu_touch_func is called.  They will be called
	 * when an edge/vertex of the face is within tolerance of plane_pt.
	 */
	rd->face_subhit = 0;
	rd->ray_dist_to_plane = dist;
	fpi = nmg_class_pt_fu_except(plane_pt, fu_p, (struct loopuse *)NULL,
		eu_touch_func, vu_touch_func, (char *)rd, NMG_FPI_PERGEOM,
		rd->tol);


	GET_HITMISS(myhit);
	NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
	myhit->hit.hit_private = (genptr_t)fu_p->f_p;
	myhit->inbound_use = myhit->outbound_use = &fu_p->l.magic;



	switch (fpi->pt_class) {
	case NMG_CLASS_Unknown	:
		rt_log("%s[line:%d] ray/plane intercept point cannot be classified wrt face\n",
			__FILE__, __LINE__);
		rt_bomb("bombing");
		break;
	case NMG_CLASS_AinB	:
	case NMG_CLASS_AonBshared :
		/* if a sub-element of the face was within tolerance of the
		 * plane intercept, then a hit has already been recorded for
		 * that element, and we do NOT need to generate one for the
		 * face.
		 */
		if (rd->face_subhit) {
			RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_HIT_SUB_MAGIC);
			VMOVE(myhit->hit.hit_point, plane_pt);
			/* also rd->ray_dist_to_plane */
			myhit->hit.hit_dist = dist; 

			myhit->hit.hit_private = (genptr_t)fu_p->f_p;
			RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
			NMG_CK_HITMISS(myhit);
		} else {
			/* The plane_pt was NOT within tolerance of a 
			 * sub-element, but it WAS within the area of the 
			 * face.  We need to record a hit on the face
			 */
			record_face_hit(rd, myhit, plane_pt, dist,
				fu_p, norm);
		}
		break;
	case NMG_CLASS_AoutB	:
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
		NMG_CK_HITMISS(myhit);

		break;
	default	:
		rt_log("%s[line:%d] BIZZARE ray/plane intercept point classification\n",
			__FILE__, __LINE__);
		rt_bomb("bombing");
	}




	/* intersect the ray with the edges/verticies of the face */
	for ( RT_LIST_FOR(lu_p, loopuse, &fu_p->lu_hd) )
		isect_ray_loopuse(rd, lu_p);

	NMG_FPI_FREE( fpi );
}
/*	I S E C T _ R A Y _ F A C E U S E
 *
 *	check to see if ray hits face.
 */
static void
isect_ray_faceuse(rd, fu_p)
struct ray_data *rd;
struct faceuse *fu_p;
{

	struct hitmiss		*myhit;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("isect_ray_faceuse(0x%08x, faceuse:0x%08x/face:0x%08x)\n",
			rd, fu_p, fu_p->f_p);

	NMG_CK_FACEUSE(fu_p);
	NMG_CK_FACEUSE(fu_p->fumate_p);
	NMG_CK_FACE(fu_p->f_p);
	NMG_CK_FACE_G_PLANE(fu_p->f_p->g.plane_p);


	/* if this face already processed, we are done. */
	if (myhit = NMG_INDEX_GET(rd->hitmiss, fu_p->f_p)) {
		if (RT_LIST_MAGIC_OK((struct rt_list *)myhit,
		    NMG_RT_HIT_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously hit\n");
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit,
		    NMG_RT_HIT_SUB_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously hit sub-element\n");
		} else if (RT_LIST_MAGIC_OK((struct rt_list *)myhit,
		    NMG_RT_MISS_MAGIC)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log(" previously missed\n");
		} else {
			rt_log("%s %d:\n\tBad magic %ld (0x%08x) for hitmiss struct for faceuse 0x%08x\n",
				__FILE__, __LINE__,
				myhit->l.magic, myhit->l.magic, fu_p);
			nmg_rt_bomb(rd, "Was I hit or not?\n");
		}
		return;
	}


	/* bounding box intersection */
	if (!rt_in_rpp(rd->rp, rd->rd_invdir,
	    fu_p->f_p->min_pt, fu_p->f_p->max_pt) ) {
		GET_HITMISS(myhit);
		NMG_INDEX_ASSIGN(rd->hitmiss, fu_p->f_p, myhit);
		myhit->hit.hit_private = (genptr_t)fu_p->f_p;
		RT_LIST_MAGIC_SET(&myhit->l, NMG_RT_MISS_MAGIC);
		RT_LIST_INSERT(&rd->rd_miss, &myhit->l);
	    	NMG_CK_HITMISS(myhit);

		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("missed bounding box\n");
		return;
	}

	if (rt_g.NMG_debug & DEBUG_RT_ISECT) rt_log(" hit bounding box \n");


	switch (*fu_p->f_p->g.magic_p) {
	case NMG_FACE_G_PLANE_MAGIC:
		isect_ray_planar_face(rd, fu_p, fu_p->f_p->g.plane_p);
		break;
	case NMG_FACE_G_SNURB_MAGIC:
		isect_ray_snurb_face(rd, fu_p, fu_p->f_p->g.snurb_p);
		break;
	}
}


/*	I S E C T _ R A Y _ S H E L L
 *
 *	Implicit return:
 *		adds hit points to the hit-list "hl"
 */
static void
nmg_isect_ray_shell(rd, s_p)
struct ray_data *rd;
struct shell *s_p;
{
	struct faceuse *fu_p;
	struct loopuse *lu_p;
	struct edgeuse *eu_p;

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("nmg_isect_ray_shell( 0x%08x, 0x%08x)\n", rd, s_p);

	NMG_CK_SHELL(s_p);
	NMG_CK_SHELL_A(s_p->sa_p);

	/* does ray isect shell rpp ?
	 * if not, we can just return.  there is no need to record the
	 * miss for the shell, as there is only one "use" of a shell.
	 */
	if (!rt_in_rpp(rd->rp, rd->rd_invdir,
	    s_p->sa_p->min_pt, s_p->sa_p->max_pt) ) {
		if (rt_g.NMG_debug & DEBUG_RT_ISECT)
			rt_log("nmg_isect_ray_shell( no RPP overlap)\n",
				 rd, s_p);
		return;
	}

	/* ray intersects shell, check sub-objects */

	for (RT_LIST_FOR(fu_p, faceuse, &(s_p->fu_hd)) )
		isect_ray_faceuse(rd, fu_p);

	for (RT_LIST_FOR(lu_p, loopuse, &(s_p->lu_hd)) )
		isect_ray_loopuse(rd, lu_p);

	for (RT_LIST_FOR(eu_p, edgeuse, &(s_p->eu_hd)) )
		isect_ray_edgeuse(rd, eu_p);

	if (s_p->vu_p)
		(void)isect_ray_vertexuse(rd, s_p->vu_p);

	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("nmg_isect_ray_shell( done )\n", rd, s_p);
}


/*	N M G _ I S E C T _ R A Y _ M O D E L
 *
 */
void
nmg_isect_ray_model(rd)
struct ray_data *rd;
{
	struct nmgregion *r_p;
	struct shell *s_p;
	struct hitmiss *a_hit;


	if (rt_g.NMG_debug & DEBUG_RT_ISECT)
		rt_log("isect_ray_nmg: Pnt(%g %g %g) Dir(%g %g %g)\n", 
			rd->rp->r_pt[0],
			rd->rp->r_pt[1],
			rd->rp->r_pt[2],
			rd->rp->r_dir[0],
			rd->rp->r_dir[1],
			rd->rp->r_dir[2]);

	NMG_CK_MODEL(rd->rd_m);

	/* Caller has assured us that the ray intersects the nmg model,
	 * check ray for intersecion with rpp's of nmgregion's
	 */
	for ( RT_LIST_FOR(r_p, nmgregion, &rd->rd_m->r_hd) ) {
		NMG_CK_REGION(r_p);
		NMG_CK_REGION_A(r_p->ra_p);

		/* does ray intersect nmgregion rpp? */
		if (! rt_in_rpp(rd->rp, rd->rd_invdir,
		    r_p->ra_p->min_pt, r_p->ra_p->max_pt) )
			continue;

		/* ray intersects region, check shell intersection */
		for (RT_LIST_FOR(s_p, shell, &r_p->s_hd)) {
			nmg_isect_ray_shell(rd, s_p);
		}
	}

	NMG_CK_HITMISS_LISTS(a_hit, rd);


	if (rt_g.NMG_debug & DEBUG_RT_ISECT) {
		if (RT_LIST_IS_EMPTY(&rd->rd_hit)) {
			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				rt_log("ray missed NMG\n");
		} else {
			print_hitlist((struct hitmiss*)&rd->rd_hit);
		}
	}
}

/*
 *				N M G _ P L _ H I T M I S S _ L I S T
 */
void
nmg_pl_hitmiss_list( str, num, hd, rp )
CONST char		*str;
int			num;
CONST struct rt_list	*hd;
CONST struct xray	*rp;
{
	FILE		*fp;
	char		buf[128];
	struct hitmiss	*hmp;
	int		count = 0;

	sprintf( buf, "%s%d.pl", str, num );

	if( rt_list_len(hd) <= 0 )  {
		rt_log("nmg_pl_hitmiss_list(): empty list, no %s written\n", buf);
		return;
	}

	if( (fp = fopen(buf, "w")) == (FILE *)NULL )  {
		perror(buf);
		return;
	}

	pl_color( fp, 210, 210, 210 );		/* grey ray */

	for( RT_LIST_FOR( hmp, hitmiss, hd ) )  {
		point_t		pt;
		NMG_CK_HITMISS(hmp);
		VJOIN1( pt, rp->r_pt, hmp->hit.hit_dist, rp->r_dir );
		if( count++ )
			pdv_3cont( fp, pt );
		else
			pdv_3move( fp, pt );
	}
	fclose(fp);
	rt_log("overlay %s\n", buf);
}

static int
guess_class_from_hitlist_max(rd, hari_kari)
struct ray_data *rd;
int *hari_kari;
{
	struct hitmiss *a_hit;
	struct hitmiss *plus_hit = (struct hitmiss *)NULL;
	int pt_class;

	*hari_kari = 0;	

	NMG_CK_RD(rd);
	if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
		rt_log("plus guessing\n");
	for (RT_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)) {

		NMG_CK_HITMISS(a_hit);
		/* if we've got a zero distance hit, that clinches it */
		if (a_hit->hit.hit_dist <= rd->tol->dist &&
	    	    a_hit->hit.hit_dist >= -rd->tol->dist) {
			if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
				rt_log("guess_class_from_hitlist_min() returns NMG_CLASS_AonBshared for 0 dist hit\n");
	    	    
	    		return NMG_CLASS_AonBshared;
	    	}

		if (a_hit->hit.hit_dist < -rd->tol->dist)
			continue;

		if (a_hit->in_out == HMG_HIT_ANY_ANY)
			continue;

		if (plus_hit == (struct hitmiss *)NULL) {
			if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
				rt_log("plus hit = %g (%s)\n", a_hit->hit.hit_dist,
					nmg_rt_inout_str(a_hit->in_out));
			plus_hit = a_hit;
			*hari_kari = 0;
		} else if (a_hit->hit.hit_dist < plus_hit->hit.hit_dist) {
			if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
				rt_log("plus hit = %g (%s)\n", a_hit->hit.hit_dist,
					nmg_rt_inout_str(a_hit->in_out));
			plus_hit = a_hit;
			*hari_kari = 0;
		} else if ( a_hit->hit.hit_dist == plus_hit->hit.hit_dist) {
			*hari_kari = 1;
		}
	}

	/* XXX This needs to be resolved with parity */
	if (*hari_kari) {
		if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
			rt_log("Contemplating Hari Kari\n");
		return NMG_CLASS_Unknown;
	}
	/* figure out what the status is from plus_hit */
	if (plus_hit) {
		switch (plus_hit->in_out) {
		case HMG_HIT_IN_IN:
		case HMG_HIT_IN_OUT:
		case HMG_HIT_IN_ON:
			pt_class = NMG_CLASS_AinB;
			break;
		case HMG_HIT_OUT_IN:
		case HMG_HIT_OUT_OUT:
		case HMG_HIT_OUT_ON:
			pt_class = NMG_CLASS_AoutB;
			break;
		case HMG_HIT_ON_IN:
		case HMG_HIT_ON_ON:
		case HMG_HIT_ON_OUT:
			pt_class = NMG_CLASS_AonBshared;
			break;
		default:
			rt_bomb("no-class hitpoint\n");
			break;
		}
	} else {
		/* since we didn't hit anything in the positive direction,
		 * we've got to be outside, since we don't allow infinite
		 * NMG's
		 */
		if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
			rt_log("Nothing in the plus direction\n");
		pt_class = NMG_CLASS_AoutB;
	}

	return pt_class;
}

static int
guess_class_from_hitlist_min(rd, hari_kari)
struct ray_data *rd;
int *hari_kari;
{
	struct hitmiss *a_hit;
	struct hitmiss *minus_hit = (struct hitmiss *)NULL;
	int pt_class;

	*hari_kari = 0;
	
	NMG_CK_RD(rd);
	if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
		rt_log("minus guessing\n");

	for (RT_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)) {

		NMG_CK_HITMISS(a_hit);
		/* if we've got a zero distance hit, that clinches it */
		if (a_hit->hit.hit_dist <= rd->tol->dist &&
	    	    a_hit->hit.hit_dist >= -rd->tol->dist) {
			if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
				rt_log("guess_class_from_hitlist_min() returns NMG_CLASS_AonBshared for 0 dist hit\n");

	    		return NMG_CLASS_AonBshared;
	    	}

		if (a_hit->hit.hit_dist > rd->tol->dist)
			continue;

		if (a_hit->in_out == HMG_HIT_ANY_ANY)
			continue;

		if (minus_hit == (struct hitmiss *)NULL) {
			if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
				rt_log("minus hit = %g (%s)\n", a_hit->hit.hit_dist,
					nmg_rt_inout_str(a_hit->in_out));
			minus_hit = a_hit;
			*hari_kari = 0;
		} else if (a_hit->hit.hit_dist > minus_hit->hit.hit_dist) {
			if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
				rt_log("minus hit = %g (%s)\n", a_hit->hit.hit_dist,
					nmg_rt_inout_str(a_hit->in_out));
			minus_hit = a_hit;
			*hari_kari = 0;
		} else if ( a_hit->hit.hit_dist == minus_hit->hit.hit_dist) {
			*hari_kari = 1;
		}
	}

	/* XXX This needs to be resolved with parity */
	if (*hari_kari) {
		if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
			rt_log("Contemplating Hari Kari\n");
		return NMG_CLASS_Unknown;
	}

	/* figure out what the status is from plus_hit */
	if (minus_hit) {
		switch (minus_hit->in_out) {
		case HMG_HIT_IN_IN:
		case HMG_HIT_OUT_IN:
		case HMG_HIT_ON_IN:
			pt_class = NMG_CLASS_AinB;
			break;
		case HMG_HIT_OUT_OUT:
		case HMG_HIT_IN_OUT:
		case HMG_HIT_ON_OUT:
			pt_class = NMG_CLASS_AoutB;
			break;
		case HMG_HIT_ON_ON:
		case HMG_HIT_OUT_ON:
		case HMG_HIT_IN_ON:
			pt_class = NMG_CLASS_AonBshared;
			break;
		default:
			rt_bomb("no-class hitpoint\n");
			break;
		}
	} else {
		/* since we didn't hit anything in the positive direction,
		 * we've got to be outside, since we don't allow infinite
		 * NMG's
		 */
		if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
			rt_log("Nothing in the minus direction\n");
		pt_class = NMG_CLASS_Unknown; /* XXX not AoutB? */
	}

	return pt_class;
}


/*
 *	N M G _ R A Y _ I S E C T _ S H E L L
 *
 *	Intended as a support routine for nmg_class_pt_s() in nmg_class.c
 *
 *	Intersect a ray with a shell and return the number of hitpoints with
 *	positive distance values.
 *
 */
int
nmg_ray_vs_shell(rp, s, tol)
struct xray *rp;
struct shell *s;
struct rt_tol *tol;
{
	struct ray_data rd;
	struct application ap;
	struct hitmiss *a_hit;
	int hit_count = 0;
	struct hitmiss *minus_hit = (struct hitmiss *)NULL;
	struct hitmiss *plus_hit = (struct hitmiss *)NULL;
	int minus_class, plus_class;
	int hari_kari_minus, hari_kari_plus;


	VUNITIZE(rp->r_dir);
	NMG_CK_SHELL(s);
	RT_CK_TOL(tol);

	if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT)) {
		rt_log("nmg_ray_vs_shell(pt(%g %g %g), dir(%g %g %g))\n",
			V3ARGS(rp->r_pt), V3ARGS(rp->r_dir));
	}

	memset(&ap, 0, sizeof(struct application));

	ap.a_resource = &rt_uniresource;
	rt_uniresource.re_magic = RESOURCE_MAGIC;

	rd.rd_m = nmg_find_model( &s->l.magic );
	rd.manifolds = nmg_manifolds(rd.rd_m);

	/* Compute the inverse of the direction cosines */
	if( !NEAR_ZERO( rp->r_dir[X], SQRT_SMALL_FASTF ) )  {
		rd.rd_invdir[X]=1.0/rp->r_dir[X];
	} else {
		rd.rd_invdir[X] = INFINITY;
		rp->r_dir[X] = 0.0;
	}
	if( !NEAR_ZERO( rp->r_dir[Y], SQRT_SMALL_FASTF ) )  {
		rd.rd_invdir[Y]=1.0/rp->r_dir[Y];
	} else {
		rd.rd_invdir[Y] = INFINITY;
		rp->r_dir[Y] = 0.0;
	}
	if( !NEAR_ZERO( rp->r_dir[Z], SQRT_SMALL_FASTF ) )  {
		rd.rd_invdir[Z]=1.0/rp->r_dir[Z];
	} else {
		rd.rd_invdir[Z] = INFINITY;
		rp->r_dir[Z] = 0.0;
	}

	rd.rp = rp;
	rd.tol = tol;
	rd.ap = &ap;
	rd.stp = (struct soltab *)NULL;
	rd.seghead = (struct seg *)NULL;
	rd.magic = NMG_RAY_DATA_MAGIC;
	rd.hitmiss = (struct hitmiss **)rt_calloc( rd.rd_m->maxindex,
		sizeof(struct hitmiss *), "nmg geom hit list");

	/* initialize the lists of things that have been hit/missed */
	RT_LIST_INIT(&rd.rd_hit);
	RT_LIST_INIT(&rd.rd_miss);

	nmg_isect_ray_shell( &rd, s);

	while (RT_LIST_WHILE(a_hit, hitmiss, &rd.rd_miss)) {
		RT_LIST_DEQUEUE( &a_hit->l );
		rt_free( (char *)a_hit, "Miss list hitmiss struct" );
	}

	/* count the number of hits */
	if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT)) {
		rt_log("%s[%d]: shell Hits:\n", __FILE__, __LINE__);
		for( RT_LIST_FOR( a_hit, hitmiss, &rd.rd_hit ) )  {
			if (a_hit->hit.hit_dist >= 0.0)
				rt_log("Positive dist hit\n");
			else
				rt_log("Negative dist hit\n");
			nmg_rt_print_hitmiss(a_hit);
		}
	}

	if( (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT)) &&
	    (rt_g.NMG_debug & (DEBUG_PLOTEM)) )  {
	    	static int	num=0;
		nmg_pl_hitmiss_list( "shell-ray", num++, &rd.rd_hit, rp );
	}

	minus_class = guess_class_from_hitlist_min(&rd, &hari_kari_minus);
	plus_class = guess_class_from_hitlist_max(&rd, &hari_kari_plus);

	if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT)) {
		rt_log("minus_class = (%s)\n", nmg_class_name(minus_class));
		rt_log("plus_class = (%s)\n", nmg_class_name(plus_class));
	}

	while (RT_LIST_WHILE(a_hit, hitmiss, &rd.rd_hit)) {
		RT_LIST_DEQUEUE( &a_hit->l );
		rt_free( (char *)a_hit, "hit list hitmiss struct" );
	}

	/* free the hitmiss table */
	rt_free( (char *)rd.hitmiss, "free nmg geom hit list");



	/* XXX This should be fixed in the guess_* routines
	 * instead of being fudged here.
	 */
	if (hari_kari_minus) {
		if(hari_kari_plus)
			rt_bomb("double hari kari");
		if (plus_class == NMG_CLASS_Unknown)
			rt_bomb("minus hari kari & plus unknown");
		minus_class = plus_class;

	} else if (minus_class == NMG_CLASS_Unknown) {
		if (plus_class == NMG_CLASS_Unknown)
			rt_bomb("minus hari kari & plus unknown");
		minus_class = plus_class;
	} else if (plus_class == NMG_CLASS_Unknown || hari_kari_plus) {
		plus_class = minus_class;
	}


	if (plus_class != minus_class) {
		rt_log("%s:%d plus_class (%s) != minus_class (%s)\n",
			__FILE__, __LINE__,
			nmg_class_name(plus_class),
			nmg_class_name(minus_class) );
		rt_bomb("");
	}

	if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_RT_ISECT))
		rt_log("nmg_ray_vs_shell() returns %s(%d)\n",
			nmg_class_name(plus_class), plus_class);
	return plus_class;
}


