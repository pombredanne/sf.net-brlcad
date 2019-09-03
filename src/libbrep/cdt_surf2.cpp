/*                        C D T _ S U R F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2019 United States Government as represented by
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
/** @addtogroup libbrep */
/** @{ */
/** @file cdt_surf.cpp
 *
 * Constrained Delaunay Triangulation - Surface Point sampling of NURBS B-Rep
 * objects.
 *
 */

#include "common.h"
#include "bn/rand.h"
#include "./cdt.h"

struct cdt_surf_info_2 {
    std::set<ON_2dPoint *> on_surf_points;
    struct ON_Brep_CDT_State *s_cdt;
    const ON_Surface *s;
    const ON_BrepFace *f;
    RTree<void *, double, 2> *rt_trims;
    RTree<void *, double, 3> *rt_trims_3d;
    std::map<int,ON_3dPoint *> *strim_pnts;
    std::map<int,ON_3dPoint *> *strim_norms;
    double u1, u2, v1, v2;
    fastf_t ulen;
    fastf_t u_lower_3dlen;
    fastf_t u_mid_3dlen;
    fastf_t u_upper_3dlen;
    fastf_t vlen;
    fastf_t v_lower_3dlen;
    fastf_t v_mid_3dlen;
    fastf_t v_upper_3dlen;
    fastf_t min_edge;
    fastf_t max_edge;
    fastf_t min_dist;
    fastf_t within_dist;
    fastf_t cos_within_ang;
    std::set<ON_BoundingBox *> leaf_bboxes;
};


class SPatch {

    public:
	SPatch(double u1, double u2, double v1, double v2)
	{
	    umin = u1;
	    umax = u2;
	    vmin = v1;
	    vmax = v2;
	}

	double umin;
	double umax;
	double vmin;
	double vmax;
};

static double
uline_len_est(struct cdt_surf_info_2 *sinfo, double u1, double u2, double v)
{
    double t, lenfact, lenest;
    int active_half = (fabs(sinfo->v1 - v) < fabs(sinfo->v2 - v)) ? 0 : 1;
    t = (active_half == 0) ? 1 - fabs(sinfo->v1 - v)/fabs((sinfo->v2 - sinfo->v1)*0.5) : 1 - fabs(sinfo->v2 - v)/fabs((sinfo->v2 - sinfo->v1)*0.5);
    if (active_half == 0) {
	lenfact = sinfo->u_lower_3dlen * (1 - (t)) + sinfo->u_mid_3dlen * (t);
	lenest = (u2 - u1)/sinfo->ulen * lenfact;
    } else {
	lenfact = sinfo->u_mid_3dlen * (1 - (t)) + sinfo->u_upper_3dlen * (t);
	lenest = (u2 - u1)/sinfo->ulen * lenfact;
    }
    return lenest;
}


static double
vline_len_est(struct cdt_surf_info_2 *sinfo, double u, double v1, double v2)
{
    double t, lenfact, lenest;
    int active_half = (fabs(sinfo->u1 - u) < fabs(sinfo->u2 - u)) ? 0 : 1;
    t = (active_half == 0) ? 1 - fabs(sinfo->u1 - u)/fabs((sinfo->u2 - sinfo->u1)*0.5) : 1 - fabs(sinfo->u2 - u)/fabs((sinfo->u2 - sinfo->u1)*0.5);
    if (active_half == 0) {
	lenfact = sinfo->v_lower_3dlen * (1 - (t)) + sinfo->v_mid_3dlen * (t);
	lenest = (v2 - v1)/sinfo->vlen * lenfact;
    } else {
	lenfact = sinfo->v_mid_3dlen * (1 - (t)) + sinfo->v_upper_3dlen * (t);
	lenest = (v2 - v1)/sinfo->vlen * lenfact;
    }
    return lenest;
}

static ON_3dPoint *
singular_trim_norm(struct cdt_surf_info_2 *sinfo, fastf_t uc, fastf_t vc)
{
    if (sinfo->strim_pnts->find(sinfo->f->m_face_index) != sinfo->strim_pnts->end()) {
	//bu_log("Face %d has singular trims\n", sinfo->f->m_face_index);
	if (sinfo->strim_norms->find(sinfo->f->m_face_index) == sinfo->strim_norms->end()) {
	    //bu_log("Face %d has no singular trim normal information\n", sinfo->f->m_face_index);
	    return NULL;
	}
	std::map<int, ON_3dPoint *>::iterator m_it;
	// Check the trims to see if uc,vc is on one of them
	for (m_it = sinfo->strim_pnts->begin(); m_it != sinfo->strim_pnts->end(); m_it++) {
	    //bu_log("  trim %d\n", (*m_it).first);
	    ON_Interval trim_dom = sinfo->f->Brep()->m_T[(*m_it).first].Domain();
	    ON_2dPoint p2d1 = sinfo->f->Brep()->m_T[(*m_it).first].PointAt(trim_dom.m_t[0]);
	    ON_2dPoint p2d2 = sinfo->f->Brep()->m_T[(*m_it).first].PointAt(trim_dom.m_t[1]);
	    //bu_log("  points: %f,%f -> %f,%f\n", p2d1.x, p2d1.y, p2d2.x, p2d2.y);
	    int on_trim = 1;
	    if (NEAR_EQUAL(p2d1.x, p2d2.x, ON_ZERO_TOLERANCE)) {
		if (!NEAR_EQUAL(p2d1.x, uc, ON_ZERO_TOLERANCE)) {
		    on_trim = 0;
		}
	    } else {
		if (!NEAR_EQUAL(p2d1.x, uc, ON_ZERO_TOLERANCE) && !NEAR_EQUAL(p2d2.x, uc, ON_ZERO_TOLERANCE)) {
		    if (!((uc > p2d1.x && uc < p2d2.x) || (uc < p2d1.x && uc > p2d2.x))) {
			on_trim = 0;
		    }
		}
	    }
	    if (NEAR_EQUAL(p2d1.y, p2d2.y, ON_ZERO_TOLERANCE)) {
		if (!NEAR_EQUAL(p2d1.y, vc, ON_ZERO_TOLERANCE)) {
		    on_trim = 0;
		}
	    } else {
		if (!NEAR_EQUAL(p2d1.y, vc, ON_ZERO_TOLERANCE) && !NEAR_EQUAL(p2d2.y, vc, ON_ZERO_TOLERANCE)) {
		    if (!((vc > p2d1.y && vc < p2d2.y) || (vc < p2d1.y && vc > p2d2.y))) {
			on_trim = 0;
		    }
		}
	    }

	    if (on_trim) {
		if (sinfo->strim_norms->find((*m_it).first) != sinfo->strim_norms->end()) {
		    ON_3dPoint *vnorm = NULL;
		    vnorm = (*sinfo->strim_norms)[(*m_it).first];
		    //bu_log(" normal: %f, %f, %f\n", vnorm->x, vnorm->y, vnorm->z);
		    return vnorm;
		} else {
		    bu_log("Face %d: on singular trim, but no matching normal: %f, %f\n", sinfo->f->m_face_index, uc, vc);
		    return NULL;
		}
	    }
	}
    }
    return NULL;
}

static bool EdgeSegCallback(void *data, void *a_context) {
    cdt_mesh::cpolyedge_t *eseg = (cdt_mesh::cpolyedge_t *)data;
    std::set<cdt_mesh::cpolyedge_t *> *segs = (std::set<cdt_mesh::cpolyedge_t *> *)a_context;
    segs->insert(eseg);
    return true;
}

struct trim_seg_context {
    const ON_2dPoint *p;
    bool on_edge;
};

static bool TrimSegCallback(void *data, void *a_context) {
    cdt_mesh::cpolyedge_t *pe = (cdt_mesh::cpolyedge_t *)data;
    struct trim_seg_context *sc = (struct trim_seg_context *)a_context;
    ON_2dPoint p2d1(pe->polygon->pnts_2d[pe->v[0]].first, pe->polygon->pnts_2d[pe->v[0]].second);
    ON_2dPoint p2d2(pe->polygon->pnts_2d[pe->v[1]].first, pe->polygon->pnts_2d[pe->v[1]].second);
    ON_Line l(p2d1, p2d2);
    double t;
    if (l.ClosestPointTo(*sc->p, &t)) {
	// If the closest point on the line isn't in the line segment, skip
	if (t < 0 || t > 1) return true;
    }
    double dist = l.MinimumDistanceTo(*sc->p);
    if (NEAR_ZERO(dist, BN_TOL_DIST)) {
	sc->on_edge = true;
    }
    return (sc->on_edge) ? false : true;
}


/* If we've got trimming curves involved, we need to be more careful about respecting
 * the min edge distance. */
static bool involves_trims(double *min_edge, struct cdt_surf_info_2 *sinfo, ON_3dPoint &p1, ON_3dPoint &p2)
{
    double min_edge_dist = sinfo->max_edge;

    // Bump out the intersection box a bit - we want to be aggressive when adapting to local
    // trimming curve segments
    ON_3dPoint wpc = (p1+p2) * 0.5;
    ON_3dVector vec1 = p1 - wpc;
    ON_3dVector vec2 = p2 - wpc;
    ON_3dPoint wp1 = wpc + (vec1 * 1.1);
    ON_3dPoint wp2 = wpc + (vec2 * 1.1);

    ON_BoundingBox uvbb;
    uvbb.Set(wp1, true);
    uvbb.Set(wp2, true);

    //plot_on_bbox(uvbb, "uvbb.plot3");

    //plot_rtree_3d(sinfo->s_cdt->edge_segs_3d[sinfo->f->m_face_index], "rtree.plot3");

    double fMin[3];
    fMin[0] = wp1.x;
    fMin[1] = wp1.y;
    fMin[2] = wp1.z;
    double fMax[3];
    fMax[0] = wp2.x;
    fMax[1] = wp2.y;
    fMax[2] = wp2.z;

    std::set<cdt_mesh::cpolyedge_t *> segs;
    size_t nhits = sinfo->s_cdt->edge_segs_3d[sinfo->f->m_face_index].Search(fMin, fMax, EdgeSegCallback, (void *)&segs);
    //bu_log("new tree found %zu boxes and %zu segments\n", nhits, segs.size());

    if (!nhits) {
	return false;
    }

    if (nhits > 40) {
	// Lot of edges, probably a high level box - just return max_edge
	(*min_edge) = min_edge_dist;
	return true;
    }

    std::set<cdt_mesh::cpolyedge_t *>::iterator s_it;
    for (s_it = segs.begin(); s_it != segs.end(); s_it++) {
	cdt_mesh::cpolyedge_t *seg = *s_it;
	ON_BoundingBox lbb;
	lbb.Set(*seg->eseg->e_start, true);
	lbb.Set(*seg->eseg->e_end, true);
	if (!uvbb.IsDisjoint(lbb)) {
	    fastf_t dist = lbb.Diagonal().Length();
	    if ((dist > BN_TOL_DIST) && (dist < min_edge_dist))  {
		min_edge_dist = dist;
	    }
	}
    }
    (*min_edge) = min_edge_dist;

    return true;
}

/* flags identifying which side of the surface we're calculating
 *
 *                        u_upper
 *                        (2,0)
 *
 *               u1v2---------------- u2v2
 *                |          |         |
 *                |          |         |
 *                |  u_lmid  |         |
 *    v_lower     |----------|---------|     v_upper
 *     (0,1)      |  (1,0)   |         |      (2,1)
 *                |         v|lmid     |
 *                |          |(1,1)    |
 *               u1v1-----------------u2v1
 *
 *                        u_lower
 *                         (0,0)
 */
static double
_cdt_get_uv_edge_3d_len(struct cdt_surf_info_2 *sinfo, int c1, int c2)
{
    int line_set = 0;
    double wu1, wv1, wu2, wv2, umid, vmid = 0.0;

    /* u_lower */
    if (c1 == 0 && c2 == 0) {
	wu1 = sinfo->u1;
	wu2 = sinfo->u2;
	wv1 = sinfo->v1;
	wv2 = sinfo->v1;
	umid = (sinfo->u2 - sinfo->u1)/2.0;
	vmid = sinfo->v1;
	line_set = 1;
    }

    /* u_lmid */
    if (c1 == 1 && c2 == 0) {
	wu1 = sinfo->u1;
	wu2 = sinfo->u2;
	wv1 = (sinfo->v2 - sinfo->v1)/2.0;
	wv2 = (sinfo->v2 - sinfo->v1)/2.0;
	umid = (sinfo->u2 - sinfo->u1)/2.0;
	vmid = (sinfo->v2 - sinfo->v1)/2.0;
	line_set = 1;
    }

    /* u_upper */
    if (c1 == 2 && c2 == 0) {
	wu1 = sinfo->u1;
	wu2 = sinfo->u2;
	wv1 = sinfo->v2;
	wv2 = sinfo->v2;
	umid = (sinfo->u2 - sinfo->u1)/2.0;
	vmid = sinfo->v2;
	line_set = 1;
    }

    /* v_lower */
    if (c1 == 0 && c2 == 1) {
	wu1 = sinfo->u1;
	wu2 = sinfo->u1;
	wv1 = sinfo->v1;
	wv2 = sinfo->v2;
	umid = sinfo->u1;
	vmid = (sinfo->v2 - sinfo->v1)/2.0;
	line_set = 1;
    }

    /* v_lmid */
    if (c1 == 1 && c2 == 1) {
	wu1 = (sinfo->u2 - sinfo->u1)/2.0;
	wu2 = (sinfo->u2 - sinfo->u1)/2.0;
	wv1 = sinfo->v1;
	wv2 = sinfo->v2;
	umid = (sinfo->u2 - sinfo->u1)/2.0;
	vmid = (sinfo->v2 - sinfo->v1)/2.0;
	line_set = 1;
    }

    /* v_upper */
    if (c1 == 2 && c2 == 1) {
	wu1 = sinfo->u2;
	wu2 = sinfo->u2;
	wv1 = sinfo->v1;
	wv2 = sinfo->v2;
	umid = sinfo->u2;
	vmid = (sinfo->v2 - sinfo->v1)/2.0;
	line_set = 1;
    }

    if (!line_set) {
	bu_log("Invalid edge %d, %d specified\n", c1, c2);
	return DBL_MAX;
    }

    // 1st 3d point
    ON_3dPoint p1 = sinfo->s->PointAt(wu1, wv1);

    // middle 3d point
    ON_3dPoint pmid = sinfo->s->PointAt(umid, vmid);

    // last 3d point
    ON_3dPoint p2 = sinfo->s->PointAt(wu2, wv2);

    // length
    double d1 = pmid.DistanceTo(p1);
    double d2 = p2.DistanceTo(pmid);
    return d1+d2;
}

void
filter_surface_edge_pnts_2(struct cdt_surf_info_2 *sinfo)
{

    // Points on 
    
    // TODO - it's looking like a 2D check isn't going to be enough - we probably
    // need BOTH a 2D and a 3D check to make sure none of the points are in a
    // position that will cause trouble.  Will need to build a 3D RTree of the line
    // segments from the edges, as well as 2D rt_trims tree.
    std::set<ON_2dPoint *> rm_pnts;
    std::set<ON_2dPoint *>::iterator osp_it;
    for (osp_it = sinfo->on_surf_points.begin(); osp_it != sinfo->on_surf_points.end(); osp_it++) {
	const ON_2dPoint *p = *osp_it;
	double fMin[2];
	double fMax[2];
	fMin[0] = p->x - ON_ZERO_TOLERANCE;
	fMin[1] = p->y - ON_ZERO_TOLERANCE;
	fMax[0] = p->x + ON_ZERO_TOLERANCE;
	fMax[1] = p->y + ON_ZERO_TOLERANCE;
	struct trim_seg_context sc;
	sc.p = p;
	sc.on_edge = false;
	sinfo->rt_trims->Search(fMin, fMax, TrimSegCallback, (void *)&sc);
	if (sc.on_edge) {
	    rm_pnts.insert((ON_2dPoint *)p);
	}
    }

    for (osp_it = rm_pnts.begin(); osp_it != rm_pnts.end(); osp_it++) {
	const ON_2dPoint *p = *osp_it;
	sinfo->on_surf_points.erase((ON_2dPoint *)p);
    }

    // Next check the face loops with the point in polygon test.  If it's
    // outside the outer loop or inside one of the interior trimming loops,
    // it's out.  For the inner loops, check the bbox of the loop first to see
    // if we need to care... in theory we probably should be using an RTree of
    // the loops to filter this, but I'm guessing that might be premature
    // optimization at this point...  Could combine with the above and have one
    // rtree per loop rather than one per face, and "zero in" for testing per
    // loop as needed.  The tree hierarchy might be worth doing anyway, as
    // those trees may be needed for overlap detection of edge polycurves
    // between breps...



    // TODO - In addition to removing points on the line in 2D, we don't want points that
    // would be outside the edge polygon in projection. Find the "close" trims (if any)
    // for the candidate 3D point, then use the normals of the Brep edge points and
    // the edge direction to do a local "inside/outside" test.  Not sure yet exactly how
    // to do this - possibilities include rtree search for the area around each surface
    // point, or a nanoflann based nearest lookup for the edge points to get candidates
    // near each edge in turn - in the latter case, look for points common to the result
    // sets for both edge points to localize on that particular segment.

    // Populate m_interior_pnts with the final set
    cdt_mesh::cdt_mesh_t *fmesh = &sinfo->s_cdt->fmeshes[sinfo->f->m_face_index];
    for (osp_it = sinfo->on_surf_points.begin(); osp_it != sinfo->on_surf_points.end(); osp_it++) {
	ON_2dPoint n2dp(**osp_it);
	long f_ind2d = fmesh->add_point(n2dp);
	fmesh->m_interior_pnts.insert(f_ind2d);

	// Add new 3D point and normal values to the fmesh as well TODO - store these during
	// the build-down in sinfo and then just look them up here...
    }
}

static bool
getSurfacePoint(
	         struct cdt_surf_info_2 *sinfo,
		 SPatch &sp,
		 std::queue<SPatch> &nq
		 )
{
    fastf_t u1 = sp.umin;
    fastf_t u2 = sp.umax;
    fastf_t v1 = sp.vmin;
    fastf_t v2 = sp.vmax;
    double ldfactor = 2.0;
    int split_u = 0;
    int split_v = 0;
    ON_2dPoint p2d(0.0, 0.0);
    fastf_t u = (u1 + u2) / 2.0;
    fastf_t v = (v1 + v2) / 2.0;
    fastf_t udist = u2 - u1;
    fastf_t vdist = v2 - v1;

    if ((udist < sinfo->min_dist + ON_ZERO_TOLERANCE)
	    || (vdist < sinfo->min_dist + ON_ZERO_TOLERANCE)) {
	return false;
    }

    double est1 = uline_len_est(sinfo, u1, u2, v1);
    double est2 = uline_len_est(sinfo, u1, u2, v2);
    double est3 = vline_len_est(sinfo, u1, v1, v2);
    double est4 = vline_len_est(sinfo, u2, v1, v2);

    double uavg = (est1+est2)/2.0;
    double vavg = (est3+est4)/2.0;

#if 0
    double umin = (est1 < est2) ? est1 : est2;
    double vmin = (est3 < est4) ? est3 : est4;
    double umax = (est1 > est2) ? est1 : est2;
    double vmax = (est3 > est4) ? est3 : est4;

    bu_log("umin,vmin: %f, %f\n", umin, vmin);
    bu_log("umax,vmax: %f, %f\n", umax, vmax);
    bu_log("uavg,vavg: %f, %f\n", uavg, vavg);
    bu_log("min_edge %f\n", sinfo->min_edge);
#endif
    if (est1 < 0.01*sinfo->within_dist && est2 < 0.01*sinfo->within_dist) {
	//bu_log("e12 Small estimates: %f, %f\n", est1, est2);
	return false;
    }
    if (est3 < 0.01*sinfo->within_dist && est4 < 0.01*sinfo->within_dist) {
	//bu_log("e34 Small estimates: %f, %f\n", est3, est4);
	return false;
    }

    if (uavg < sinfo->min_edge && vavg < sinfo->min_edge) {
	return false;
    }


    if (uavg > ldfactor * vavg) {
	split_u = 1;
    }

    if (vavg > ldfactor * uavg) {
	split_v = 1;
    }

    ON_3dPoint p[4] = {ON_3dPoint(), ON_3dPoint(), ON_3dPoint(), ON_3dPoint()};
    ON_3dVector norm[4] = {ON_3dVector(), ON_3dVector(), ON_3dVector(), ON_3dVector()};
    bool ev_success = false;

    if (!split_u || !split_v) {
	// Don't know if we're splitting in at least one direction - check if we're close
	// enough to trims to need to worry about edges

	double min_edge_len = -1.0;

	// If we're dealing with a curved surface, don't get bigger than max_edge
	if (!sinfo->s->IsPlanar(NULL, BN_TOL_DIST)) {
	    min_edge_len = sinfo->max_edge;
	    if (uavg > min_edge_len && vavg > min_edge_len) {
		split_u = 1;
	    }

	    if (uavg > min_edge_len && vavg > min_edge_len) {
		split_v = 1;
	    }
	}

	// If the above test didn't resolve things, keep going
	if (!split_u || !split_v) {
	    if ((surface_EvNormal(sinfo->s, u1, v1, p[0], norm[0]))
		    && (surface_EvNormal(sinfo->s, u2, v1, p[1], norm[1]))
		    && (surface_EvNormal(sinfo->s, u2, v2, p[2], norm[2]))
		    && (surface_EvNormal(sinfo->s, u1, v2, p[3], norm[3]))) {

		ON_BoundingBox uvbb;
		for (int i = 0; i < 4; i++) {
		    uvbb.Set(p[i], true);
		}
		//plot_on_bbox(uvbb, "uvbb.plot3");

		ON_3dPoint pmin = uvbb.Min();
		ON_3dPoint pmax = uvbb.Max();
		if (involves_trims(&min_edge_len, sinfo, pmin, pmax)) {

		    if (min_edge_len > 0 && uavg > min_edge_len && vavg > min_edge_len) {
			split_u = 1;
		    }

		    if (min_edge_len > 0 && uavg > min_edge_len && vavg > min_edge_len) {
			split_v = 1;
		    }
		}

		ev_success = true;
	    }
	}
    }


    if (ev_success && (!split_u || !split_v)) {
	// Don't know if we're splitting in at least one direction - check dot products
	ON_3dPoint mid(0.0, 0.0, 0.0);
	ON_3dVector norm_mid(0.0, 0.0, 0.0);
	if ((surface_EvNormal(sinfo->s, u2, v1, p[1], norm[1])) // for u
		&& (surface_EvNormal(sinfo->s, u1, v2, p[3], norm[3]))
		&& (surface_EvNormal(sinfo->s, u, v, mid, norm_mid))) {
	    double udot;
	    double vdot;
	    ON_Line line1(p[0], p[2]);
	    ON_Line line2(p[1], p[3]);
	    double dist = mid.DistanceTo(line1.ClosestPointTo(mid));
	    V_MAX(dist, mid.DistanceTo(line2.ClosestPointTo(mid)));

	    for (int i = 0; i < 4; i++) {
		fastf_t uc = (i == 0 || i == 3) ? u1 : u2;
		fastf_t vc = (i == 0 || i == 1) ? v1 : v2;
		ON_3dPoint *vnorm = singular_trim_norm(sinfo, uc, vc);
		if (vnorm && ON_DotProduct(*vnorm, norm_mid) > 0) {
		    //bu_log("vert norm %f %f %f works\n", vnorm->x, vnorm->y, vnorm->z);
		    norm[i] = *vnorm;
		}
	    }


	    if (dist < sinfo->min_dist + ON_ZERO_TOLERANCE) {
		return false;
	    }

	    udot = (VNEAR_EQUAL(norm[0], norm[1], ON_ZERO_TOLERANCE)) ? 1.0 : norm[0] * norm[1];
	    vdot = (VNEAR_EQUAL(norm[0], norm[3], ON_ZERO_TOLERANCE)) ? 1.0 : norm[0] * norm[3];
	    if (udot < sinfo->cos_within_ang - ON_ZERO_TOLERANCE) {
		split_u = 1;
	    }
	    if (vdot < sinfo->cos_within_ang - ON_ZERO_TOLERANCE) {
		split_v = 1;
	    }
	}
    }

    if (split_u && split_v) {
	nq.push(SPatch(u1, u, v1, v));
	nq.push(SPatch(u1, u, v, v2));
	nq.push(SPatch(u, u2, v1, v));
	nq.push(SPatch(u, u2, v, v2));
	return true;
    }
    if (split_u) {
	nq.push(SPatch(u1, u, v1, v2));
	nq.push(SPatch(u, u2, v1, v2));
	return true;
    }
    if (split_v) {
	nq.push(SPatch(u1, u2, v1, v));
	nq.push(SPatch(u1, u2, v, v2));
	return true;
    }

    return false;
}

void
GetInteriorPoints(struct ON_Brep_CDT_State *s_cdt, int face_index)
{
    double surface_width, surface_height;

    ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    const ON_Surface *s = face.SurfaceOf();
    const ON_Brep *brep = face.Brep();

    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
	double dist = 0.0;
	double min_dist = 0.0;
	double within_dist = 0.0;
	double cos_within_ang = 0.0;

	if ((surface_width < BN_TOL_DIST) || (surface_height < BN_TOL_DIST)) {
	    return;
	}

	struct cdt_surf_info_2 sinfo;
	sinfo.s_cdt = s_cdt;
	sinfo.s = s;
	sinfo.f = &face;
	sinfo.rt_trims = &(s_cdt->trim_segs[face_index]);
	sinfo.rt_trims_3d = &(s_cdt->edge_segs_3d[face_index]);
	sinfo.strim_pnts = &(s_cdt->strim_pnts[face_index]);
	sinfo.strim_norms = &(s_cdt->strim_norms[face_index]);
	double t1, t2;
	s->GetDomain(0, &t1, &t2);
	sinfo.ulen = fabs(t2 - t1);
	s->GetDomain(1, &t1, &t2);
	sinfo.vlen = fabs(t2 - t1);
	s->GetDomain(0, &sinfo.u1, &sinfo.u2);
	s->GetDomain(1, &sinfo.v1, &sinfo.v2);
	sinfo.u_lower_3dlen = _cdt_get_uv_edge_3d_len(&sinfo, 0, 0);
	sinfo.u_mid_3dlen   = _cdt_get_uv_edge_3d_len(&sinfo, 1, 0);
	sinfo.u_upper_3dlen = _cdt_get_uv_edge_3d_len(&sinfo, 2, 0);
	sinfo.v_lower_3dlen = _cdt_get_uv_edge_3d_len(&sinfo, 0, 1);
	sinfo.v_mid_3dlen   = _cdt_get_uv_edge_3d_len(&sinfo, 1, 1);
	sinfo.v_upper_3dlen = _cdt_get_uv_edge_3d_len(&sinfo, 2, 1);
	sinfo.min_edge = (*s_cdt->min_edge_seg_len)[face_index];
	sinfo.max_edge = (*s_cdt->max_edge_seg_len)[face_index];

	// may be a smaller trimmed subset of surface so worth getting
	// face boundary
	bool bGrowBox = false;
	ON_3dPoint min, max;
	for (int li = 0; li < face.LoopCount(); li++) {
	    for (int ti = 0; ti < face.Loop(li)->TrimCount(); ti++) {
		const ON_BrepTrim *trim = face.Loop(li)->Trim(ti);
		trim->GetBoundingBox(min, max, bGrowBox);
		bGrowBox = true;
	    }
	}

	ON_BoundingBox tight_bbox;
	if (brep->GetTightBoundingBox(tight_bbox)) {
	    // Note: this needs to be based on the smallest dimension of the
	    // box, not the diagonal, in case we've got something really long
	    // and narrow.
	    fastf_t d1 = tight_bbox.m_max[0] - tight_bbox.m_min[0];
	    fastf_t d2 = tight_bbox.m_max[1] - tight_bbox.m_min[1];
	    dist = (d1 < d2) ? d1 : d2;
	}

	if (s_cdt->tol.abs < BN_TOL_DIST + ON_ZERO_TOLERANCE) {
	    min_dist = BN_TOL_DIST;
	} else {
	    min_dist = s_cdt->tol.abs;
	}

	double rel = 0.0;
	if (s_cdt->tol.rel > 0.0 + ON_ZERO_TOLERANCE) {
	    rel = s_cdt->tol.rel * dist;
	    within_dist = rel < min_dist ? min_dist : rel;
	    //if (s_cdt->abs < s_cdt->dist + ON_ZERO_TOLERANCE) {
	    //    min_dist = within_dist;
	    //}
	} else if ((s_cdt->tol.abs > 0.0 + ON_ZERO_TOLERANCE)
		&& (s_cdt->tol.norm < 0.0 + ON_ZERO_TOLERANCE)) {
	    within_dist = min_dist;
	} else if ((s_cdt->tol.abs > 0.0 + ON_ZERO_TOLERANCE)
		|| (s_cdt->tol.norm > 0.0 + ON_ZERO_TOLERANCE)) {
	    within_dist = dist;
	} else {
	    within_dist = 0.01 * dist; // default to 1% minimum surface distance
	}

	if (s_cdt->tol.norm > 0.0 + ON_ZERO_TOLERANCE) {
	    cos_within_ang = cos(s_cdt->tol.norm);
	} else {
	    cos_within_ang = cos(ON_PI / 2.0);
	}

	sinfo.min_dist = min_dist;
	sinfo.within_dist = within_dist;
	sinfo.cos_within_ang = cos_within_ang;

	std::queue<SPatch> spq1, spq2;

	/**
	 * Sample portions of the surface to collect sufficient points
	 * to capture the surface shape according to the settings
	 *
	 * M = max
	 * m = min
	 * o = mid
	 *
	 *    umvM------uovM-------uMvM
	 *     |          |         |
	 *     |          |         |
	 *     |          |         |
	 *    umvo------uovo-------uMvo
	 *     |          |         |
	 *     |          |         |
	 *     |          |         |
	 *    umvm------uovm-------uMvm
	 *
	 * left bottom  = umvm
	 * left midy    = umvo
	 * left top     = umvM
	 * midx bottom  = uovm
	 * midx midy    = uovo
	 * midx top     = uovM
	 * right bottom = uMvm
	 * right midy   = uMvo
	 * right top    = uMvM
	 */

	ON_BOOL32 uclosed = s->IsClosed(0);
	ON_BOOL32 vclosed = s->IsClosed(1);
	double midx = (min.x + max.x) / 2.0;
	double midy = (min.y + max.y) / 2.0;

	if (uclosed && vclosed) {
	    /*
	     *     #--------------------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvo------uovo--------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvm------uovm--------#
	     */
	    spq1.push(SPatch(min.x, midx, min.y, midy));

	    /*
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #--------uovo-------uMvo
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #--------uovm-------uMvm
	     */
	    spq1.push(SPatch(midx, max.x, min.y, midy));

	    /*
	     *    umvM------uovM--------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvo------uovo--------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     */
	    spq1.push(SPatch(min.x, midx, midy, max.y));

	    /*
	     *     #--------uovM------ uMvM
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #--------uovo-------uMvo
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     */
	    spq1.push(SPatch(midx, max.x, midy, max.y));

	} else if (uclosed) {

	    /*
	     *    umvM------uovM--------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvm------uovm--------#
	     */
	    spq1.push(SPatch(min.x, midx, min.y, max.y));

	    /*
	     *     #--------uovM------ uMvM
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #--------uovm-------uMvm
	     */
	    spq1.push(SPatch(midx, max.x, min.y, max.y));

	} else if (vclosed) {

	    /*
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvo--------#--------uMvo
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvm--------#--------uMvm
	     */
	    spq1.push(SPatch(min.x, max.x, min.y, midy));

	    /*
	     *    umvM--------#------- uMvM
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvo--------#--------uMvo
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     */
	    spq1.push(SPatch(min.x, max.x, midy, max.y));

	} else {

	    /*
	     *    umvM--------#------- uMvM
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvm--------#--------uMvm
	     */
	    spq1.push(SPatch(min.x, max.x, min.y, max.y));
	}

	std::queue<SPatch> *wq = &spq1;
	std::queue<SPatch> *nq = &spq2;
	int split_depth = 0;


	while (!wq->empty()) {
	    SPatch sp = wq->front();
	    wq->pop();
	    if (!getSurfacePoint(&sinfo, sp, *nq)) {
		ON_BoundingBox bb(ON_2dPoint(sp.umin,sp.vmin),ON_2dPoint(sp.umax, sp.vmax));
		sinfo.leaf_bboxes.insert(new ON_BoundingBox(bb));
	    }

	    // Once we've processed the current level,
	    // work on the next if we need to
	    if (wq->empty() && !nq->empty()) {
		std::queue<SPatch> *tq = wq;
		wq = nq;
		nq = tq;
		// Let the counter know we're going deeper
		split_depth++;
		std::cout << "split_depth: " << split_depth << "\n";
	    }
	}

	float *prand;
	std::set<ON_BoundingBox *>::iterator b_it;
	/* We want to jitter sampled 2D points out of linearity */
	bn_rand_init(prand, 0);
	for (b_it = sinfo.leaf_bboxes.begin(); b_it != sinfo.leaf_bboxes.end(); b_it++) {
	    ON_3dPoint p3d = (*b_it)->Center();
	    ON_3dPoint pmax = (*b_it)->Max();
	    ON_3dPoint pmin = (*b_it)->Min();
	    double ulen = pmax.x - pmin.x;
	    double vlen = pmax.y - pmin.y;
	    double px = p3d.x + (bn_rand_half(prand) * 0.3*ulen);
	    double py = p3d.y + (bn_rand_half(prand) * 0.3*vlen);
	    sinfo.on_surf_points.insert(new ON_2dPoint(px,py));
	}

	// Strip out points from the surface that are on the trimming curves.  Trim
	// points require special handling for watertightness and introducing them
	// from the surface also runs the risk of adding duplicate 2D points, which
	// aren't allowed for facetization.
	filter_surface_edge_pnts_2(&sinfo);

    }
}


/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

