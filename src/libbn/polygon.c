/*                     P O L Y G O N . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "limits.h" /* for INT_MAX */
#include "bu/list.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/sort.h"
#include "bn/polygon.h"
#include "bn/plane_struct.h"
#include "bn/plane_calc.h"


int
bn_3d_polygon_area(fastf_t *area, size_t npts, const point_t *pts)
{
    size_t i;
    vect_t v1, v2, tmp, tot = VINIT_ZERO;
    plane_t plane_eqn;
    struct bn_tol tol;

    if (!pts || !area || npts < 3)
	return 1;
    BN_TOL_INIT(&tol);
    tol.dist_sq = BN_TOL_DIST * BN_TOL_DIST;
    if (bn_mk_plane_3pts(plane_eqn, pts[0], pts[1], pts[2], &tol) == -1)
	return 1;

    switch (npts) {
	case 3:
	    /* Triangular Face - for triangular face T:V0, V1, V2,
	     * area = 0.5 * [(V2 - V0) x (V1 - V0)] */
	    VSUB2(v1, pts[1], pts[0]);
	    VSUB2(v2, pts[2], pts[0]);
	    VCROSS(tot, v2, v1);
	    break;
	case 4:
	    /* Quadrilateral Face - for planar quadrilateral
	     * Q:V0, V1, V2, V3 with unit normal N,
	     * area = N/2 ⋅ [(V2 - V0) x (V3 - V1)] */
	    VSUB2(v1, pts[2], pts[0]);
	    VSUB2(v2, pts[3], pts[1]);
	    VCROSS(tot, v2, v1);
	    break;
	default:
	    /* N-Sided Face - compute area using Green's Theorem */
	    for (i = 0; i < npts; i++) {
		VCROSS(tmp, pts[i], pts[i + 1 == npts ? 0 : i + 1]);
		VADD2(tot, tot, tmp);
	    }
	    break;
    }
    *area = fabs(VDOT(plane_eqn, tot)) * 0.5;
    return 0;
}


int
bn_3d_polygon_centroid(point_t *cent, size_t npts, const point_t *pts)
{
    size_t i;
    fastf_t x_0 = 0.0;
    fastf_t x_1 = 0.0;
    fastf_t y_0 = 0.0;
    fastf_t y_1 = 0.0;
    fastf_t z_0 = 0.0;
    fastf_t z_1 = 0.0;
    fastf_t a = 0.0;
    fastf_t signedArea = 0.0;

    if (!pts || !cent || npts < 3)
	return 1;
    /* Calculate Centroid projection for face for x-y-plane */
    for (i = 0; i < npts-1; i++) {
	x_0 = pts[i][0];
	y_0 = pts[i][1];
	x_1 = pts[i+1][0];
	y_1 = pts[i+1][1];
	a = x_0 *y_1 - x_1*y_0;
	signedArea += a;
	*cent[0] += (x_0 + x_1)*a;
	*cent[1] += (y_0 + y_1)*a;
    }
    x_0 = pts[i][0];
    y_0 = pts[i][1];
    x_1 = pts[0][0];
    y_1 = pts[0][1];
    a = x_0 *y_1 - x_1*y_0;
    signedArea += a;
    *cent[0] += (x_0 + x_1)*a;
    *cent[1] += (y_0 + y_1)*a;

    signedArea *= 0.5;
    *cent[0] /= (6.0*signedArea);
    *cent[1] /= (6.0*signedArea);

    /* calculate Centroid projection for face for x-z-plane */

    signedArea = 0.0;
    for (i = 0; i < npts-1; i++) {
	x_0 = pts[i][0];
	z_0 = pts[i][2];
	x_1 = pts[i+1][0];
	z_1 = pts[i+1][2];
	a = x_0 *z_1 - x_1*z_0;
	signedArea += a;
	*cent[2] += (z_0 + z_1)*a;
    }
    x_0 = pts[i][0];
    z_0 = pts[i][2];
    x_1 = pts[0][0];
    z_0 = pts[0][2];
    a = x_0 *z_1 - x_1*z_0;
    signedArea += a;
    *cent[2] += (z_0 + z_1)*a;

    signedArea *= 0.5;
    *cent[2] /= (6.0*signedArea);
    return 0;
}


int
bn_3d_polygon_mk_pts_planes(size_t *npts, point_t **pts, size_t neqs, const plane_t *eqs)
{
    size_t i, j, k, l;
    if (!npts || !pts || neqs < 4 || !eqs)
	return 1;
    /* find all vertices */
    for (i = 0; i < neqs - 2; i++) {
	for (j = i + 1; j < neqs - 1; j++) {
	    for (k = j + 1; k < neqs; k++) {
		point_t pt;
		int keep_point = 1;
		if (bn_mkpoint_3planes(pt, eqs[i], eqs[j], eqs[k]) < 0)
		    continue;
		/* discard pt if it is outside the polyhedron */
		for (l = 0; l < neqs; l++) {
		    if (l == i || l == j || l == k)
			continue;
		    if (DIST_PT_PLANE(pt, eqs[l]) > BN_TOL_DIST) {
			keep_point = 0;
			break;
		    }
		}
		/* found a good point, add it to each of the intersecting faces */
		if (keep_point) {
		    VMOVE(pts[i][npts[i]], (pt)); npts[i]++;
		    VMOVE(pts[j][npts[j]], (pt)); npts[j]++;
		    VMOVE(pts[k][npts[k]], (pt)); npts[k]++;
		}
	    }
	}
   }
    return 0;
}


HIDDEN int
sort_ccw_3d(const void *x, const void *y, void *cmp)
{
    vect_t tmp;
    VCROSS(tmp, ((fastf_t *)x), ((fastf_t *)y));
    return VDOT(*((point_t *)cmp), tmp);
}


int
bn_3d_polygon_sort_ccw(size_t npts, point_t *pts, plane_t cmp)
{
    if (!pts || npts < 3)
	return 1;
    bu_sort(pts, npts, sizeof(point_t), sort_ccw_3d, &cmp);
    return 0;
}

int
bn_polygon_clockwise(size_t npts, const point2d_t *pts)
{
    size_t i;
    int sum = 0;
    for (i = 0; i < npts; i++) {
	if (i + 1 == npts) {
	    sum += (pts[0][0] - pts[i][0]) * (pts[0][1] + pts[i][1]);
	} else {
	    sum += (pts[i+1][0] - pts[i][0]) * (pts[i+1][1] + pts[i][1]);
	}
    }
    if (sum == 0) return 0;
    return (sum > 0) ? 1 : -1;
}

/*
 * Translation to libbn data types of Franklin's point-in-polygon test.
 * See http://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/pnpoly.html
 * for a discussion of the subtleties involved with the inequality tests.
 * The below copyright applies to just the function bn_pt_in_polygon,
 * not the whole of polygon.c
 *
 * Copyright (c) 1970-2003, Wm. Randolph Franklin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimers.  Redistributions
 * in binary form must reproduce the above copyright notice in the
 * documentation and/or other materials provided with the distribution.
 * The name of W. Randolph Franklin may not be used to endorse or promote
 * products derived from this Software without specific prior written
 * permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
int
bn_pt_in_polygon(size_t nvert, const point2d_t *pnts, const point2d_t *test)
{
    size_t i = 0;
    size_t j = 0;
    int c = 0;
    for (i = 0, j = nvert-1; i < nvert; j = i++) {
	if ( ((pnts[i][1] > (*test)[1]) != (pnts[j][1] > (*test)[1])) &&
		((*test)[0] < (pnts[j][0]-pnts[i][0]) * ((*test)[1]-pnts[i][1]) / (pnts[j][1]-pnts[i][1]) + pnts[i][0]) )
	    c = !c;
    }
    return c;
}


/**
 * Implementation of Ear Clipping Polygon Triangulation
 *
 * Input polygon points must be in a CCW direction.
 *
 * Based off of David Eberly's documentation of the algorithm in Triangulation
 * by Ear Clipping, section 2.
 * http://www.geometrictools.com/Documentation/TriangulationByEarClipping.pdf
 *
 * Helpful reference implementations included
 * https://code.google.com/p/polypartition/ and
 * http://www.geometrictools.com/GTEngine/Include/GteTriangulateEC.h
 */


typedef struct pt_vertex_ref pt_vr;

struct pt_vertex {
    struct bu_list l;
    int index;
    int isConvex;
    int isEar;
    double angle;
    pt_vr *convex_ref;
    pt_vr *reflex_ref;
    pt_vr *ear_ref;
};

struct pt_vertex_ref {
    struct bu_list l;
    struct pt_vertex *v;
};

struct pt_lists {
    struct pt_vertex *vertex_list;
    struct pt_vertex_ref *convex_list;
    struct pt_vertex_ref *reflex_list;
    struct pt_vertex_ref *ear_list;
};

#define PT_ADD_CONVEX_VREF(_list, vert) {\
    struct pt_vertex_ref *n_ref; \
    BU_GET(n_ref, struct pt_vertex_ref); \
    n_ref->v = vert; \
    BU_LIST_APPEND(&(_list->l), &(n_ref->l)); \
    vert->convex_ref = n_ref; \
    vert->isConvex = 1; \
}

#define PT_ADD_REFLEX_VREF(_list, vert) {\
    struct pt_vertex_ref *n_ref; \
    BU_GET(n_ref, struct pt_vertex_ref); \
    n_ref->v = vert; \
    BU_LIST_APPEND(&(_list->l), &(n_ref->l)); \
    vert->reflex_ref = n_ref; \
    vert->isConvex = 0; \
}

#define PT_ADD_EAR_VREF(_list, vert, pts) {\
    struct pt_vertex_ref *n_ref; \
    BU_GET(n_ref, struct pt_vertex_ref); \
    n_ref->v = vert; \
    BU_LIST_APPEND(&(_list->l), &(n_ref->l)); \
    vert->ear_ref = n_ref; \
    vert->isConvex = 1; \
    vert->isEar = 1; \
}

#define PT_DEQUEUE_CONVEX_VREF(_list, vert) {\
    BU_LIST_DEQUEUE(&(vert->convex_ref->l)); \
    BU_PUT(vert->convex_ref, struct pt_vertex_ref); \
    vert->convex_ref = NULL; \
    vert->isConvex = 0; \
}

#define PT_DEQUEUE_REFLEX_VREF(_list, vert) {\
    BU_LIST_DEQUEUE(&(vert->reflex_ref->l)); \
    BU_PUT(vert->reflex_ref, struct pt_vertex_ref); \
    vert->reflex_ref = NULL; \
}

#define PT_DEQUEUE_EAR_VREF(_list, vert) {\
    BU_LIST_DEQUEUE(&(vert->ear_ref->l)); \
    BU_PUT(vert->ear_ref, struct pt_vertex_ref); \
    vert->ear_ref = NULL; \
    vert->isEar = 0; \
}

#define PT_ADD_TRI(ear) { \
    struct pt_vertex *p = PT_NEXT(ear); \
    struct pt_vertex *n = PT_PREV(ear); \
    local_faces[offset+0] = p->index; \
    local_faces[offset+1] = ear->index; \
    local_faces[offset+2] = n->index; \
    offset = offset + 3; \
    face_cnt++; \
}

#define PT_NEXT(v) BU_LIST_PNEXT_CIRC(pt_vertex, &(v->l))
#define PT_PREV(v) BU_LIST_PPREV_CIRC(pt_vertex, &(v->l))

#define PT_NEXT_REF(v) BU_LIST_PNEXT_CIRC(pt_vertex_ref, &(v->l))
#define PT_PREV_REF(v) BU_LIST_PPREV_CIRC(pt_vertex_ref, &(v->l))

HIDDEN int
is_inside(const point2d_t p1, const point2d_t p2, const point2d_t p3, const point2d_t *test) {
    point2d_t tri[3];
    V2MOVE(tri[0], p1);
    V2MOVE(tri[1], p2);
    V2MOVE(tri[2], p3);
    return bn_pt_in_polygon(3, (const point2d_t *)tri, test);
}


HIDDEN int
is_ear(int p_ind, int p_prev_ind, int p_next_ind, struct pt_vertex_ref *reflex_list, const point2d_t *pts)
{
    int ear = 1;
    struct pt_vertex_ref *vref = NULL;
    for (BU_LIST_FOR_BACKWARDS(vref, pt_vertex_ref, &(reflex_list->l))) {
	int vrind = vref->v->index;
	if (vrind == p_ind || vrind == p_prev_ind || vrind == p_next_ind) continue;
	if (is_inside(pts[p_ind], pts[p_prev_ind], pts[p_next_ind], (const point2d_t *)&pts[vrind])) {
	    ear = 0;
	    break;
	}
    }
    return ear;
}

/* Use the convexity test from polypartition */
HIDDEN int
is_convex(const point2d_t test, const point2d_t p1, const point2d_t p2) {

    double testval;
    testval = (p2[1]-p1[1])*(test[0]-p1[0])-(p2[0]-p1[0])*(test[1]-p1[1]);
    return (testval > 0) ? 1 : 0;
}

/* 2D angle */
HIDDEN double
pt_angle(const point2d_t test, const point2d_t p1, const point2d_t p2) {

    double dot, det;
    vect_t v1;
    vect_t v2;
    V2MOVE(v1, p1);
    V2MOVE(v2, p2);
    V2SUB2(v1, v1, test);
    V2SUB2(v2, v2, test);
    dot = v1[0]*v2[0] + v1[1]*v2[1];
    det = v1[0]*v2[1] - v1[1]*v2[0];
    return -1*atan2(det, dot) * 180/M_PI;
}

HIDDEN void
pt_v_get(struct bu_list *l, int i)
{
    struct pt_vertex *v;
    BU_GET(v, struct pt_vertex);
    v->index = i;
    v->isConvex = -1;
    v->isEar = -1;
    v->convex_ref = NULL;
    v->reflex_ref = NULL;
    v->ear_ref = NULL;
    BU_LIST_PUSH(l, &(v->l));
}

HIDDEN void
pt_v_put(struct pt_vertex *v)
{
    if (v->convex_ref) {
	BU_LIST_DEQUEUE(&(v->convex_ref->l));
	BU_PUT(v->convex_ref, struct pt_vertex_ref);
    }
    if (v->reflex_ref) {
	bu_log("Warning - trying to remove a reflex point! %d\n", v->index);
	BU_LIST_DEQUEUE(&(v->reflex_ref->l));
	BU_PUT(v->reflex_ref, struct pt_vertex_ref);
    }
    if (v->ear_ref) {
	BU_LIST_DEQUEUE(&(v->ear_ref->l));
	BU_PUT(v->ear_ref, struct pt_vertex_ref);
    }
    BU_LIST_DEQUEUE(&(v->l));
    BU_PUT(v, struct pt_vertex);
}

HIDDEN void
remove_ear(struct pt_vertex *ear, struct pt_lists *lists, const point2d_t *pts)
{
    struct pt_vertex *vp;
    struct pt_vertex *vn;
    if (!ear) return;

    /* First, make a note of the two vertices whose status might change */
    vp = PT_NEXT(ear);
    vn = PT_PREV(ear);

    /* Remove the ear vertex */
    pt_v_put(ear);

    /* Update the status of the two neighbor points */
    if (vp->isConvex) {
	/* Check ear status */
	int prev_ear_status = vp->isEar;
	struct pt_vertex *p = PT_NEXT(vp);
	struct pt_vertex *n = PT_PREV(vp);
	vp->angle = pt_angle(pts[vp->index], pts[p->index], pts[n->index]);
	vp->isEar = is_ear(vp->index, p->index, n->index, lists->reflex_list, pts);
	if (prev_ear_status != vp->isEar) {
	    if (vp->isEar) {
		PT_ADD_EAR_VREF(lists->ear_list, vp, pts);
	    } else {
		PT_DEQUEUE_EAR_VREF(lists->ear_list, vp);
	    }
	}
    } else {
	struct pt_vertex *p = PT_NEXT(vp);
	struct pt_vertex *n = PT_PREV(vp);
	/* Check if it became convex */
	vp->isConvex = is_convex(pts[vp->index], pts[p->index], pts[n->index]);
	/* Check if it became an ear */
	if (vp->isConvex) {
	    vp->angle = pt_angle(pts[vp->index], pts[p->index], pts[n->index]);
	    PT_DEQUEUE_REFLEX_VREF(lists->reflex_list, vp);
	    PT_ADD_CONVEX_VREF(lists->convex_list, vp);
	    vp->isEar = is_ear(vp->index, p->index, n->index, lists->reflex_list, pts);
	    if (vp->isEar) {
		PT_ADD_EAR_VREF(lists->ear_list, vp, pts);
	    }
	}
    }
    if (vn->isConvex) {
	int prev_ear_status = vn->isEar;
	struct pt_vertex *p = PT_NEXT(vn);
	struct pt_vertex *n = PT_PREV(vn);
	vn->angle = pt_angle(pts[vn->index], pts[p->index], pts[n->index]);
	vn->isEar = is_ear(vn->index, p->index, n->index, lists->reflex_list, pts);
	if (prev_ear_status != vn->isEar) {
	    if (vn->isEar) {
		PT_ADD_EAR_VREF(lists->ear_list, vn, pts);
	    } else {
		PT_DEQUEUE_EAR_VREF(lists->ear_list, vn);
	    }
	}
    } else {
	struct pt_vertex *p = PT_NEXT(vn);
	struct pt_vertex *n = PT_PREV(vn);
	/* Check if it became convex */
	vn->isConvex = is_convex(pts[vn->index], pts[p->index], pts[n->index]);
	/* Check if it became an ear */
	if (vn->isConvex) {
	    vn->angle = pt_angle(pts[vn->index], pts[p->index], pts[n->index]);
	    PT_DEQUEUE_REFLEX_VREF(lists->reflex_list, vn);
	    PT_ADD_CONVEX_VREF(lists->convex_list, vn);
	    vn->isEar = is_ear(vn->index, p->index, n->index, lists->reflex_list, pts);
	    if (vn->isEar) {
		PT_ADD_EAR_VREF(lists->ear_list, vn, pts);
	    }
	}
    }

    return;
}

int bn_polygon_triangulate(int **faces, int *num_faces, const point2d_t *pts, size_t npts)
{
    size_t i = 0;
    size_t face_cnt = 0;
    int offset = 0;
    int ret = 0;
    int *local_faces;
    struct pt_lists *lists = NULL;
    struct pt_vertex *v = NULL;
    struct pt_vertex_ref *vref = NULL;
    struct pt_vertex *vertex_list = NULL;
    struct pt_vertex_ref *convex_list = NULL;
    struct pt_vertex_ref *reflex_list = NULL;
    struct pt_vertex_ref *ear_list = NULL;
    int ccw = bn_polygon_clockwise(npts, pts);

    if (ccw != -1) {
	bu_log("Warning - non-CCW point loop!\n");
    }


    BU_GET(lists, struct pt_lists);
    if(npts < 3) return 1;
    if (!faces || !num_faces || !pts) return 1;

    BU_GET(lists->vertex_list, struct pt_vertex);
    BU_LIST_INIT(&(lists->vertex_list->l));
    lists->vertex_list->index = -1;

    BU_GET(lists->ear_list, struct pt_vertex_ref);
    BU_LIST_INIT(&(lists->ear_list->l));
    lists->ear_list->v = NULL;

    BU_GET(lists->reflex_list, struct pt_vertex_ref);
    BU_LIST_INIT(&(lists->reflex_list->l));
    lists->reflex_list->v = NULL;

    BU_GET(lists->convex_list, struct pt_vertex_ref);
    BU_LIST_INIT(&(lists->convex_list->l));
    lists->convex_list->v = NULL;

    vertex_list = lists->vertex_list;
    convex_list = lists->convex_list;
    reflex_list = lists->reflex_list;
    ear_list = lists->ear_list;

    local_faces = (int *)bu_calloc(3*3*npts, sizeof(int), "triangles");

    /* Initialize vertex list. */
    for (i = 0; i < npts; i++) {
	pt_v_get(&(vertex_list->l), i);
    }

    /* Point ordering ends up opposite to that of the points in the array, so
     * everything is backwards */

    /* Find the initial convex and reflex point sets */
    for (BU_LIST_FOR_BACKWARDS(v, pt_vertex, &(vertex_list->l)))
    {
	struct pt_vertex *prev = PT_NEXT(v);
	struct pt_vertex *next = PT_PREV(v);
	v->isConvex = is_convex(pts[v->index], pts[prev->index], pts[next->index]);
	if (v->isConvex) {
	    v->angle = pt_angle(pts[v->index], pts[prev->index], pts[next->index]);
	    PT_ADD_CONVEX_VREF(convex_list, v);
	} else {
	    v->angle = 0;
	    v->isEar = 0;
	    PT_ADD_REFLEX_VREF(reflex_list, v);
	}
    }
    /* Now that we know which are the convex and reflex verts, find the initial ears */
    for (BU_LIST_FOR_BACKWARDS(vref, pt_vertex_ref, &(convex_list->l)))
    {
	struct pt_vertex *p = PT_NEXT(vref->v);
	struct pt_vertex *n = PT_PREV(vref->v);
	vref->v->isEar = is_ear(vref->v->index, p->index, n->index, reflex_list, pts);
	if (vref->v->isEar) PT_ADD_EAR_VREF(ear_list, vref->v, pts);
    }

    /* If we didn't find any ears, something is wrong */
    if (BU_LIST_IS_EMPTY(&(lists->ear_list->l))) {
	ret = 1;
	goto cleanup;
    }

    /* We know what we need to begin - remove ears, build triangles and update accordingly */
    {
	struct pt_vertex *one_vert = PT_NEXT(vertex_list);
	struct pt_vertex *four_vert = PT_NEXT(PT_NEXT(PT_NEXT(one_vert)));
	while(one_vert->index != four_vert->index) {
	    struct pt_vertex_ref *ear_ref;
	    int min_angle = INT_MAX;
	    for (BU_LIST_FOR_BACKWARDS(vref, pt_vertex_ref, &(ear_list->l))){
		if (vref->v->angle < min_angle && vref->v) {
		    min_angle = vref->v->angle;
		    ear_ref = vref;
		}
	    }
	    if (!ear_ref || !ear_ref->v) {
		bu_log("ear list error!\n");
		ret = 1;
		goto cleanup;
	    }
	    PT_ADD_TRI(ear_ref->v);
	    remove_ear(ear_ref->v, lists, pts);
	    one_vert = PT_NEXT(vertex_list);
	    four_vert = PT_NEXT(PT_NEXT(PT_NEXT(one_vert)));
	}
    }

    /* Last triangle */
    for (BU_LIST_FOR_BACKWARDS(v, pt_vertex, &(vertex_list->l))){
	if (!v->isConvex) PT_DEQUEUE_REFLEX_VREF(lists->reflex_list, v);
    }
    PT_ADD_TRI(PT_NEXT(vertex_list));

    /* Finalize the face set */
    (*num_faces) = face_cnt;
    (*faces) = (int *)bu_calloc(face_cnt*3, sizeof(int), "final faces array");
    for (i = 0; i < face_cnt; i++) {
	(*faces)[i*3] = local_faces[i*3];
	(*faces)[i*3+1] = local_faces[i*3+1];
	(*faces)[i*3+2] = local_faces[i*3+2];
    }

cleanup:
    bu_free(local_faces, "free local faces array");

    /* Make sure the lists are empty */

    while (BU_LIST_WHILE(v, pt_vertex , &(vertex_list->l))) {
	/*bu_log("clear vert: %d\n", v->index);*/
	pt_v_put(v);
    }

    /* TODO - this should always be empty by this point? */
    while (BU_LIST_WHILE(vref, pt_vertex_ref , &(reflex_list->l))) {
	BU_LIST_DEQUEUE(&(vref->l));
	BU_PUT(vref, struct pt_vertex_ref);
    }

    while (BU_LIST_WHILE(vref, pt_vertex_ref , &(convex_list->l))) {
	BU_LIST_DEQUEUE(&(vref->l));
	BU_PUT(vref, struct pt_vertex_ref);
    }
    /* TODO - this should always be empty by this point? */
    while (BU_LIST_WHILE(vref, pt_vertex_ref , &(ear_list->l))) {
	BU_LIST_DEQUEUE(&(vref->l));
	BU_PUT(vref, struct pt_vertex_ref);
    }

    BU_PUT(ear_list, struct pt_vertex_ref);
    BU_PUT(reflex_list, struct pt_vertex_ref);
    BU_PUT(convex_list, struct pt_vertex_ref);
    BU_PUT(vertex_list, struct pt_vertex);
    BU_PUT(lists, struct pt_lists);

    return ret;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
