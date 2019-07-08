/*                        C D T . C P P
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
/** @file cdt.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 * Routines for processing 3D edges
 */

#include "common.h"
#include "./cdt.h"

// TODO - need a tree-ish structure for each edge so we can
// take a 3D point and use it as a guide to refine the
// splitting up of the edge, without redoing the whole
// subdivision for the entire edge.  There will almost certainly
// be a variety of situations were we need to locally refine
// the edge approximation, and we don't want to have to start
// over each time.  One already-observed situation is when the
// edge curve is very blocky compared to the mesh of the associated
// surface (large cylinder fillet surface - big circle with large
// linear segments, but curve in the other direction of the surface
// requires very fine triangles - this creates very distorted
// triangles at best and can even create "wrong" triangles depending
// on how the 2D sampling vs 3D projections work out.)  Another
// highly probable situation will be refinement in object overlap
// regions - we'll have to first refine any involved edges before
// we can work on the faces.


BrepTrimPoint *
Add_BrepTrimPoint(
	struct ON_Brep_CDT_State *s_cdt,
	std::map<double, BrepTrimPoint *> *tpp,
	ON_3dPoint *p3d, ON_3dPoint *norm,
	ON_3dVector &tangent, double e, ON_3dPoint &p2d,
	ON_3dVector &trim_normal, double t, int trim_index
	)
{
    BrepTrimPoint *btp = new BrepTrimPoint;
    btp->p3d = p3d;
    btp->n3d = norm;
    btp->tangent = tangent;
    btp->e = e;
    btp->p2d = p2d;
    btp->normal = trim_normal;
    btp->t = t;
    btp->trim_ind = trim_index;
    (*tpp)[btp->t] = btp;
    (*s_cdt->on_brep_edge_pnts)[p3d].insert(btp);
    return btp;
}

double
midpt_binary_search(fastf_t *tmid, const ON_BrepTrim *trim, double tstart, double tend, ON_3dPoint &edge_mid_3d, double tol, int verbose)
{
    double tcmid = (tstart + tend) / 2.0;
    ON_3dPoint trim_mid_2d = trim->PointAt(tcmid);
    const ON_Surface *s = trim->SurfaceOf();
    ON_3dPoint trim_mid_3d = s->PointAt(trim_mid_2d.x, trim_mid_2d.y);
    double dist = edge_mid_3d.DistanceTo(trim_mid_3d);

    if (dist > tol) {
	ON_3dPoint trim_start_2d = trim->PointAt(tstart);
	ON_3dPoint trim_end_2d = trim->PointAt(tend);
	ON_3dPoint trim_start_3d = s->PointAt(trim_start_2d.x, trim_start_2d.y);
	ON_3dPoint trim_end_3d = s->PointAt(trim_end_2d.x, trim_end_2d.y);

	ON_3dVector v1 = edge_mid_3d - trim_start_3d;
	ON_3dVector v2 = edge_mid_3d - trim_end_3d;

	if (verbose) {
	    bu_log("start point (%f %f %f) and end point (%f %f %f)\n", trim_start_3d.x, trim_start_3d.y, trim_start_3d.z, trim_end_3d.x, trim_end_3d.y, trim_end_3d.z);
	    bu_log("Note (%f:%f)%f - edge point (%f %f %f) and trim point (%f %f %f): %f, %f\n", tstart, tend, ON_DotProduct(v1,v2), edge_mid_3d.x, edge_mid_3d.y, edge_mid_3d.z, trim_mid_3d.x, trim_mid_3d.y, trim_mid_3d.z, dist, tol);
	}

	if (ON_DotProduct(v1,v2) < 0) {
	    if (verbose)
		bu_log("(%f - %f - %f (%f): searching left and right subspans\n", tstart, tcmid, tend, ON_DotProduct(v1,v2));
	    double tlmid, trmid;
	    double fldist = midpt_binary_search(&tlmid, trim, tstart, tcmid, edge_mid_3d, tol, 0);
	    double frdist = midpt_binary_search(&trmid, trim, tcmid, tend, edge_mid_3d, tol, 0);
	    if (fldist >= 0 && frdist < -1) {
		if (verbose)
		    bu_log("(%f - %f - %f: going with fldist: %f\n", tstart, tcmid, tend, fldist);
		(*tmid) = tlmid;
		return fldist;
	    }
	    if (frdist >= 0 && fldist < -1) {
		if (verbose)
		    bu_log("(%f - %f - %f: going with frdist: %f\n", tstart, tcmid, tend, frdist);
		(*tmid) = trmid;
		return frdist;
	    }
	    if (fldist < -1 && frdist < -1) {
		if (verbose)
		    bu_log("(%f - %f: point not in either subspan (%f)\n", tstart, tend, ON_DotProduct(v1,v2));
		return -2;
	    }
	} else {
	    // Not in this span
	    if (verbose)
		bu_log("(%f - %f: point not in span (dot-product: %f)\n", tstart, tend, ON_DotProduct(v1,v2));
	    return -2;
	}
    }

    // close enough - this works
    if (verbose)
	bu_log("Workable (%f:%f) - edge point (%f %f %f) and trim point (%f %f %f): %f, %f\n", tstart, tend, edge_mid_3d.x, edge_mid_3d.y, edge_mid_3d.z, trim_mid_3d.x, trim_mid_3d.y, trim_mid_3d.z, dist, tol);

    (*tmid) = tcmid;
    return dist;
}

ON_3dPoint
get_trim_midpt(fastf_t *t, const ON_BrepTrim *trim, double tstart, double tend, ON_3dPoint &edge_mid_3d, double tol, int verbose) {
    double tmid;
    double dist = midpt_binary_search(&tmid, trim, tstart, tend, edge_mid_3d, tol, verbose);
    if (dist < 0) {
	if (verbose)
	    bu_log("Warning - could not find suitable trim point\n");
	tmid = (tstart + tend) / 2.0;
    } else {
	if (verbose) {
	    if (dist > tol)
		bu_log("going with distance %f greater than desired tolerance %f\n", dist, tol);
	}
    }
    ON_3dPoint trim_mid_2d = trim->PointAt(tmid);
    (*t) = tmid;
    return trim_mid_2d;
}

static void
getEdgePoints(
	struct ON_Brep_CDT_State *s_cdt,
	ON_BrepEdge *edge,
	ON_NurbsCurve *nc,
	const ON_BrepTrim &trim,
	BrepTrimPoint *sbtp1,
	BrepTrimPoint *ebtp1,
	BrepTrimPoint *sbtp2,
	BrepTrimPoint *ebtp2,
	const struct brep_cdt_tol *cdt_tol,
	std::map<double, BrepTrimPoint *> *trim1_param_points,
	std::map<double, BrepTrimPoint *> *trim2_param_points,
	double loop_min_dist
	)
{
    ON_3dPoint tmp1, tmp2;
    ON_BrepTrim *trim2 = (edge->Trim(0)->m_trim_index == trim.m_trim_index) ? edge->Trim(1) : edge->Trim(0);
    const ON_Surface *s1 = trim.SurfaceOf();
    const ON_Surface *s2 = trim2->SurfaceOf();

    ON_3dVector trim1_mid_norm = ON_3dVector::UnsetVector;
    ON_3dVector trim2_mid_norm = ON_3dVector::UnsetVector;
    ON_3dPoint edge_mid_3d = ON_3dPoint::UnsetPoint;
    ON_3dVector edge_mid_tang = ON_3dVector::UnsetVector;
    
    fastf_t emid = (sbtp1->e + ebtp1->e) / 2.0;
    bool evtangent_status = nc->EvTangent(emid, edge_mid_3d, edge_mid_tang);
    if (!evtangent_status) {
	// EvTangent call failed, get 3d point
	edge_mid_3d = nc->PointAt(emid);
    }

    // We need the trim points to be pretty close to the edge point, or
    // we get distortions in the mesh.
    
    if (edge->m_edge_index == 790) {
	bu_log("trim fun commencing\n");
    }
    fastf_t t1, t2;
    fastf_t emindist = (cdt_tol->min_dist < 0.5*loop_min_dist) ? cdt_tol->min_dist : 0.5 * loop_min_dist;
    ON_3dPoint trim1_mid_2d, trim2_mid_2d;
    if (edge->m_edge_index == 790) {
	trim1_mid_2d = get_trim_midpt(&t1, &trim, sbtp1->t, ebtp1->t, edge_mid_3d, emindist, 0);
	trim2_mid_2d = get_trim_midpt(&t2, trim2, sbtp2->t, ebtp2->t, edge_mid_3d, emindist, 0);
    } else {
	trim1_mid_2d = get_trim_midpt(&t1, &trim, sbtp1->t, ebtp1->t, edge_mid_3d, emindist, 0);
	trim2_mid_2d = get_trim_midpt(&t2, trim2, sbtp2->t, ebtp2->t, edge_mid_3d, emindist, 0);
    }

    if (!evtangent_status) {
	// If the edge curve evaluation failed, try to average tangents from trims
	ON_3dVector trim1_mid_tang(0.0, 0.0, 0.0);
	ON_3dVector trim2_mid_tang(0.0, 0.0, 0.0);
	int evals = 0;
	evals += (trim.EvTangent(t1, tmp1, trim1_mid_tang)) ? 1 : 0;
	evals += (trim2->EvTangent(t2, tmp2, trim2_mid_tang)) ? 1 : 0;
	if (evals == 2) {
	    edge_mid_tang = (trim1_mid_tang + trim2_mid_tang) / 2;
	} else {
	    edge_mid_tang = ON_3dVector::UnsetVector;
	}
    }

    int dosplit = 0;

    ON_Line line3d(*(sbtp1->p3d), *(ebtp1->p3d));
    double dist3d = edge_mid_3d.DistanceTo(line3d.ClosestPointTo(edge_mid_3d));
    dosplit += (line3d.Length() > cdt_tol->max_dist) ? 1 : 0;
    dosplit += (dist3d > (cdt_tol->within_dist + ON_ZERO_TOLERANCE)) ? 1 : 0;
    dosplit += (dist3d > 2*loop_min_dist) ? 1 : 0;

    if ((dist3d > cdt_tol->min_dist + ON_ZERO_TOLERANCE)) {
	if (!dosplit) {
	    dosplit += ((sbtp1->tangent * ebtp1->tangent) < cdt_tol->cos_within_ang - ON_ZERO_TOLERANCE) ? 1 : 0;
	}

	if (!dosplit && sbtp1->normal != ON_3dVector::UnsetVector && ebtp1->normal != ON_3dVector::UnsetVector) {
	    dosplit += ((sbtp1->normal * ebtp1->normal) < cdt_tol->cos_within_ang - ON_ZERO_TOLERANCE) ? 1 : 0;
	}

	if (!dosplit && sbtp2->normal != ON_3dVector::UnsetVector && ebtp2->normal != ON_3dVector::UnsetVector) {
	    dosplit += ((sbtp2->normal * ebtp2->normal) < cdt_tol->cos_within_ang - ON_ZERO_TOLERANCE) ? 1 : 0;
	}
    }

    if (dosplit) {

	if (!surface_EvNormal(s1, trim1_mid_2d.x, trim1_mid_2d.y, tmp1, trim1_mid_norm)) {
	    trim1_mid_norm = ON_3dVector::UnsetVector;
	} else {
	    if (trim.Face()->m_bRev) {
		trim1_mid_norm = trim1_mid_norm  * -1.0;
	    }
	}

	if (!surface_EvNormal(s2, trim2_mid_2d.x, trim2_mid_2d.y, tmp2, trim2_mid_norm)) {
	    trim2_mid_norm = ON_3dVector::UnsetVector;
	} else {
	    if (trim2->Face()->m_bRev) {
		trim2_mid_norm = trim2_mid_norm  * -1.0;
	    }
	}

    if (edge->m_edge_index == 790) {
	bu_log("Edge %d: %f %f %f\n", edge->m_edge_index, edge_mid_3d.x, edge_mid_3d.y, edge_mid_3d.z);
	bu_log("Trim %d, Face %d, (IsRev: %d): %f %f %f\n", trim.m_trim_index, trim.Face()->m_face_index, trim.m_bRev3d, tmp1.x, tmp1.y, tmp1.z);
	bu_log("Trim %d, Face %d, (IsRev: %d): %f %f %f\n", trim2->m_trim_index, trim2->Face()->m_face_index, trim2->m_bRev3d, tmp2.x, tmp2.y, tmp2.z);
    }


	ON_3dPoint *npt = new ON_3dPoint(edge_mid_3d);
	CDT_Add3DPnt(s_cdt, npt, -1, -1, -1, edge->m_edge_index, emid, 0);
	s_cdt->edge_pnts->insert(npt);

	BrepTrimPoint *nbtp1 = Add_BrepTrimPoint(s_cdt, trim1_param_points, npt, NULL, edge_mid_tang, emid, trim1_mid_2d, trim1_mid_norm, t1, trim.m_trim_index);

	BrepTrimPoint *nbtp2 = Add_BrepTrimPoint(s_cdt, trim2_param_points, npt, NULL, edge_mid_tang, emid, trim2_mid_2d, trim2_mid_norm, t2, trim2->m_trim_index);

	getEdgePoints(s_cdt, edge, nc, trim, sbtp1, nbtp1, sbtp2, nbtp2, cdt_tol, trim1_param_points, trim2_param_points, loop_min_dist);
	getEdgePoints(s_cdt, edge, nc, trim, nbtp1, ebtp1, nbtp2, ebtp2, cdt_tol, trim1_param_points, trim2_param_points, loop_min_dist);
	return;
    }

    int udir = 0;
    int vdir = 0;
    double trim1_seam_t = 0.0;
    double trim2_seam_t = 0.0;
    ON_2dPoint trim1_start = sbtp1->p2d;
    ON_2dPoint trim1_end = ebtp1->p2d;
    ON_2dPoint trim2_start = sbtp2->p2d;
    ON_2dPoint trim2_end = ebtp2->p2d;
    ON_2dPoint trim1_seam_2d, trim2_seam_2d;
    ON_3dPoint trim1_seam_3d, trim2_seam_3d;
    int t1_dosplit = 0;
    int t2_dosplit = 0;

    if (ConsecutivePointsCrossClosedSeam(s1, trim1_start, trim1_end, udir, vdir, BREP_SAME_POINT_TOLERANCE)) {
	ON_2dPoint from = ON_2dPoint::UnsetPoint;
	ON_2dPoint to = ON_2dPoint::UnsetPoint;
	if (FindTrimSeamCrossing(trim, sbtp1->t, ebtp1->t, trim1_seam_t, from, to, BREP_SAME_POINT_TOLERANCE)) {
	    trim1_seam_2d = trim.PointAt(trim1_seam_t);
	    trim1_seam_3d = s1->PointAt(trim1_seam_2d.x, trim1_seam_2d.y);
	    if (trim1_param_points->find(trim1_seam_t) == trim1_param_points->end()) {
		t1_dosplit = 1;
	    }
	}
    }

    if (ConsecutivePointsCrossClosedSeam(s2, trim2_start, trim2_end, udir, vdir, BREP_SAME_POINT_TOLERANCE)) {
	ON_2dPoint from = ON_2dPoint::UnsetPoint;
	ON_2dPoint to = ON_2dPoint::UnsetPoint;
	if (FindTrimSeamCrossing(trim, sbtp2->t, ebtp2->t, trim2_seam_t, from, to, BREP_SAME_POINT_TOLERANCE)) {
	    trim2_seam_2d = trim2->PointAt(trim2_seam_t);
	    trim2_seam_3d = s2->PointAt(trim2_seam_2d.x, trim2_seam_2d.y);
	    if (trim2_param_points->find(trim2_seam_t) == trim2_param_points->end()) {
		t2_dosplit = 1;
	    }
	}
    }

    if (t1_dosplit || t2_dosplit) {
	ON_3dPoint *nsptp = NULL;
	if (t1_dosplit && t2_dosplit) {
	    ON_3dPoint nspt = (trim1_seam_3d + trim2_seam_3d)/2;
	    nsptp = new ON_3dPoint(nspt);
	    CDT_Add3DPnt(s_cdt, nsptp, trim.Face()->m_face_index, -1, trim.m_trim_index, trim.Edge()->m_edge_index, trim1_seam_2d.x, trim1_seam_2d.y);
	    s_cdt->edge_pnts->insert(nsptp);
	} else {
	    // Since the above if test got the both-true case, only one of these at
	    // a time will ever be true.  TODO - could this be a source of degenerate
	    // faces in 3D if we're only splitting one trim?
	    if (!t1_dosplit) {
		trim1_seam_t = (sbtp1->t + ebtp1->t)/2;
		trim1_seam_2d = trim.PointAt(trim1_seam_t);
		nsptp = new ON_3dPoint(trim2_seam_3d);
		CDT_Add3DPnt(s_cdt, nsptp, trim2->Face()->m_face_index, -1, trim2->m_trim_index, trim2->Edge()->m_edge_index, trim1_seam_2d.x, trim1_seam_2d.y);
		s_cdt->edge_pnts->insert(nsptp);
	    }
	    if (!t2_dosplit) {
		trim2_seam_t = (sbtp2->t + ebtp2->t)/2;
		trim2_seam_2d = trim2->PointAt(trim2_seam_t);
		nsptp = new ON_3dPoint(trim1_seam_3d);
		CDT_Add3DPnt(s_cdt, nsptp, trim.Face()->m_face_index, -1, trim.m_trim_index, trim.Edge()->m_edge_index, trim1_seam_2d.x, trim1_seam_2d.y);
		s_cdt->edge_pnts->insert(nsptp);
	    }
	}

	// Note - by this point we shouldn't need tangents and normals...
	ON_3dVector v_unset = ON_3dVector::UnsetVector;
	ON_3dPoint t1s2d(trim1_seam_2d);
	(void)Add_BrepTrimPoint(s_cdt, trim1_param_points, nsptp, NULL, v_unset, ON_UNSET_VALUE, t1s2d, v_unset, trim1_seam_t, trim.m_trim_index);

	ON_3dPoint t2s2d(trim2_seam_2d);
	(void)Add_BrepTrimPoint(s_cdt, trim2_param_points, nsptp, NULL, v_unset, ON_UNSET_VALUE, t2s2d, v_unset, trim2_seam_t, trim2->m_trim_index);
    }

}

std::map<double, BrepTrimPoint *> *
getEdgePoints(
	struct ON_Brep_CDT_State *s_cdt,
	ON_BrepEdge *edge,
	ON_BrepTrim &trim,
	fastf_t max_dist,
	fastf_t loop_min_dist
	)
{
    struct brep_cdt_tol cdt_tol = BREP_CDT_TOL_ZERO;
    std::map<double, BrepTrimPoint *> *trim1_param_points = NULL;
    std::map<double, BrepTrimPoint *> *trim2_param_points = NULL;

    // Get the other trim
    // TODO - this won't work if we don't have a 1->2 edge to trims relationship - in that
    // case we'll have to split up the edge and find the matching sub-trims (possibly splitting
    // those as well if they don't line up at shared 3D points.)
    ON_BrepTrim *trim2 = (edge->Trim(0)->m_trim_index == trim.m_trim_index) ? edge->Trim(1) : edge->Trim(0);

    if (edge->m_edge_index == 790) {
	bu_log("Starting 790\n");
    }

    double dist = 1000.0;

    const ON_Surface *s1 = trim.SurfaceOf();
    const ON_Surface *s2 = trim2->SurfaceOf();

    bool bGrowBox = false;
    ON_3dPoint min, max;

    /* If we're out of sync, bail - something is very very wrong */
    if (trim.m_trim_user.p != NULL && trim2->m_trim_user.p == NULL) {
	return NULL;
    }
    if (trim.m_trim_user.p == NULL && trim2->m_trim_user.p != NULL) {
	return NULL;
    }


    /* If we've already got the points, just return them */
    if (trim.m_trim_user.p != NULL) {
	trim1_param_points = (std::map<double, BrepTrimPoint *> *) trim.m_trim_user.p;
	return trim1_param_points;
    }

    /* Establish tolerances (TODO - get from edge curve...) */
    if (trim.GetBoundingBox(min, max, bGrowBox)) {
	dist = DIST_PT_PT(min, max);
    }
    CDT_Tol_Set(&cdt_tol, dist, max_dist, s_cdt->abs, s_cdt->rel, s_cdt->norm, s_cdt->dist);


    /* Normalize the domain of the curve to the ControlPolygonLength() of the
     * NURBS form of the curve to attempt to minimize distortion in 3D to
     * mirror what we do for the surfaces.  (GetSurfaceSize uses this under the
     * hood for its estimates.)  */
    const ON_Curve* crv = edge->EdgeCurveOf();
    ON_NurbsCurve *nc = crv->NurbsCurve();
    double cplen = nc->ControlPolygonLength();
    nc->SetDomain(0.0, cplen);
    s_cdt->brep->m_T[trim.TrimCurveIndexOf()].SetDomain(0.0, cplen);
    s_cdt->brep->m_T[trim2->TrimCurveIndexOf()].SetDomain(0.0, cplen);
    ON_Interval erange = nc->Domain();


    /* Begin point collection */
    ON_3dPoint tmp1, tmp2;
    int evals = 0;
    ON_3dPoint *edge_start_3d, *edge_end_3d = NULL;
    ON_3dVector edge_start_tang, edge_end_tang = ON_3dVector::UnsetVector;
    ON_3dPoint trim1_start_2d, trim1_end_2d = ON_3dPoint::UnsetPoint;
    ON_3dVector trim1_start_tang, trim1_end_tang = ON_3dVector::UnsetVector;
    ON_3dPoint trim2_start_2d, trim2_end_2d = ON_3dPoint::UnsetPoint;
    ON_3dVector trim2_start_tang, trim2_end_tang = ON_3dVector::UnsetVector;

    trim1_param_points = new std::map<double, BrepTrimPoint *>();
    trim.m_trim_user.p = (void *) trim1_param_points;
    trim2_param_points = new std::map<double, BrepTrimPoint *>();
    trim2->m_trim_user.p = (void *) trim2_param_points;

    ON_Interval range1 = trim.Domain();
    ON_Interval range2 = trim2->Domain();

    // TODO - what is this for?
    if (s1->IsClosed(0) || s1->IsClosed(1)) {
	ON_BoundingBox trim_bbox = ON_BoundingBox::EmptyBoundingBox;
	trim.GetBoundingBox(trim_bbox, false);
    }
    if (s2->IsClosed(0) || s2->IsClosed(1)) {
	ON_BoundingBox trim_bbox2 = ON_BoundingBox::EmptyBoundingBox;
	trim2->GetBoundingBox(trim_bbox2, false);
    }

    /* For the start and end points, use the vertex point */
    edge_start_3d = (*s_cdt->vert_pnts)[edge->Vertex(0)->m_vertex_index];
    edge_end_3d = (*s_cdt->vert_pnts)[edge->Vertex(1)->m_vertex_index];

    /* Populate the 2D points */
    double st1 = (trim.m_bRev3d) ? range1.m_t[1] : range1.m_t[0];
    double et1 = (trim.m_bRev3d) ? range1.m_t[0] : range1.m_t[1];
    double st2 = (trim2->m_bRev3d) ? range2.m_t[1] : range2.m_t[0];
    double et2 = (trim2->m_bRev3d) ? range2.m_t[0] : range2.m_t[1];
    trim1_start_2d = trim.PointAt(st1);
    trim1_end_2d = trim.PointAt(et1);
    trim2_start_2d = trim2->PointAt(st2);
    trim2_end_2d = trim2->PointAt(et2);

    /* Get starting tangent from edge*/
    if (!(nc->EvTangent(erange.m_t[0], tmp1, edge_start_tang))) {
	// If the edge curve failed, average tangents from trims
	evals = 0;
	evals += (trim.EvTangent(st1, tmp1, trim1_start_tang)) ? 1 : 0;
	evals += (trim2->EvTangent(st2, tmp2, trim2_start_tang)) ? 1 : 0;
	if (evals == 2) {
	    edge_start_tang = (trim1_start_tang + trim2_start_tang) / 2;
	} else {
	    edge_start_tang = ON_3dVector::UnsetVector;
	}
    }
    /* Get ending tangent from edge*/
    if (!(nc->EvTangent(erange.m_t[1], tmp2, edge_end_tang))) {
	// If the edge curve failed, average tangents from trims
	evals = 0;
	evals += (trim.EvTangent(et1, tmp1, trim1_end_tang)) ? 1 : 0;
	evals += (trim2->EvTangent(et2, tmp2, trim2_end_tang)) ? 1 : 0;
	if (evals == 2) {
	    edge_end_tang = (trim1_end_tang + trim2_end_tang) / 2;
	} else {
	    edge_end_tang = ON_3dVector::UnsetVector;
	}
    }

    // Get the normals
    ON_3dPoint tmpp;
    ON_3dVector trim1_start_normal, trim1_end_normal = ON_3dVector::UnsetVector;
    ON_3dVector trim2_start_normal, trim2_end_normal = ON_3dVector::UnsetVector;
    ON_3dPoint *t1_sn, *t1_en, *t2_sn, *t2_en = NULL;
    ON_3dPoint *edge_start_3dnorm = (*s_cdt->vert_avg_norms)[edge->Vertex(0)->m_vertex_index];
    ON_3dPoint *edge_end_3dnorm = (*s_cdt->vert_avg_norms)[edge->Vertex(1)->m_vertex_index];

    /* trim 1 */
    if (!surface_EvNormal(s1, trim1_start_2d.x, trim1_start_2d.y, tmpp, trim1_start_normal)) {
	t1_sn = edge_start_3dnorm;
    } else {
	if (trim.Face()->m_bRev) {
	    trim1_start_normal = trim1_start_normal  * -1.0;
	}
	if (edge_start_3dnorm && ON_DotProduct(trim1_start_normal, *edge_start_3dnorm) < -0.5) {
	    t1_sn = edge_start_3dnorm;
	} else {
	    t1_sn = new ON_3dPoint(trim1_start_normal);
	    *t1_sn = trim1_start_normal;
	    s_cdt->w3dnorms->push_back(t1_sn);
	}
    }
    if (!surface_EvNormal(s1, trim1_end_2d.x, trim1_end_2d.y, tmp1, trim1_end_normal)) {
	t1_en = edge_end_3dnorm;
    } else {
	if (trim.Face()->m_bRev) {
	    trim1_end_normal = trim1_end_normal  * -1.0;
	}
	if (edge_end_3dnorm && ON_DotProduct(trim1_end_normal, *edge_end_3dnorm) < -0.5) {
	    t1_en = edge_end_3dnorm;
	} else {
	    t1_en = new ON_3dPoint(trim1_end_normal);
	    *t1_en = trim1_end_normal;
	    s_cdt->w3dnorms->push_back(t1_en);
	}
    }


    /* trim 2 */
    if (!surface_EvNormal(s2, trim2_start_2d.x, trim2_start_2d.y, tmpp, trim2_start_normal)) {
	t2_sn = edge_start_3dnorm;
    } else {
	if (trim2->Face()->m_bRev) {
	    trim2_start_normal = trim2_start_normal  * -1.0;
	}
	if (edge_start_3dnorm && ON_DotProduct(trim2_start_normal, *edge_start_3dnorm) < -0.5) {
	    t2_sn = edge_start_3dnorm;
	} else {
	    t2_sn = new ON_3dPoint(trim2_start_normal);
	    *t2_sn = trim2_start_normal;
	    s_cdt->w3dnorms->push_back(t2_sn);
	}
    }
    if (!surface_EvNormal(s2, trim2_end_2d.x, trim2_end_2d.y, tmpp, trim2_end_normal)) {
	t2_en = edge_end_3dnorm;
    } else {
	if (trim2->Face()->m_bRev) {
	    trim2_end_normal = trim2_end_normal  * -1.0;
	}
	if (edge_end_3dnorm && ON_DotProduct(trim2_end_normal, *edge_end_3dnorm) < -0.5) {
	    t2_en = edge_end_3dnorm;
	} else {
	    t2_en = new ON_3dPoint(trim2_end_normal);
	    *t2_en = trim2_end_normal;
	    s_cdt->w3dnorms->push_back(t2_en);
	}
    }

    /* Start and end points for both trims can now be defined */
    BrepTrimPoint *sbtp1 = Add_BrepTrimPoint(s_cdt, trim1_param_points, edge_start_3d, t1_sn, edge_start_tang, erange.m_t[0], trim1_start_2d, trim1_start_normal, st1, trim.m_trim_index);

    BrepTrimPoint *ebtp1 = Add_BrepTrimPoint(s_cdt, trim1_param_points, edge_end_3d, t1_en, edge_end_tang, erange.m_t[1], trim1_end_2d, trim1_end_normal, et1, trim.m_trim_index);

    BrepTrimPoint *sbtp2 = Add_BrepTrimPoint(s_cdt, trim2_param_points, edge_start_3d, t2_sn, edge_start_tang, erange.m_t[0], trim2_start_2d, trim2_start_normal, st2, trim2->m_trim_index);

    BrepTrimPoint *ebtp2 = Add_BrepTrimPoint(s_cdt, trim2_param_points, edge_end_3d, t2_en, edge_end_tang, erange.m_t[1], trim2_end_2d, trim2_end_normal, et2, trim2->m_trim_index);

    fastf_t emindist = (cdt_tol.min_dist < 0.5*loop_min_dist) ? cdt_tol.min_dist : 0.5 * loop_min_dist;

    if (trim.IsClosed() || trim2->IsClosed()) {

	double edge_mid_range = (erange.m_t[0] + erange.m_t[1]) / 2.0;
	ON_3dVector edge_mid_tang, trim1_mid_norm, trim2_mid_norm = ON_3dVector::UnsetVector;
	ON_3dPoint edge_mid_3d = ON_3dPoint::UnsetPoint;

	int ev_tangent = nc->EvTangent(edge_mid_range, edge_mid_3d, edge_mid_tang);
	if (!ev_tangent) {
	    // EvTangent call failed, get 3d point
	    edge_mid_3d = nc->PointAt(edge_mid_range);
	}

	double trim1_mid_range;
	double trim2_mid_range;
	ON_3dPoint trim1_mid_2d = get_trim_midpt(&trim1_mid_range, &trim, sbtp1->t, ebtp1->t, edge_mid_3d, emindist, 0);
	ON_3dPoint trim2_mid_2d = get_trim_midpt(&trim2_mid_range, trim2, sbtp2->t, ebtp2->t, edge_mid_3d, emindist, 0);

	if (!ev_tangent) {
	    // If the edge curve failed, try to average tangents from trims
	    ON_3dVector trim1_mid_tang(0.0, 0.0, 0.0);
	    ON_3dVector trim2_mid_tang(0.0, 0.0, 0.0);
	    evals = 0;
	    evals += (trim.EvTangent(trim1_mid_range, tmp1, trim1_mid_tang)) ? 1 : 0;
	    evals += (trim2->EvTangent(trim2_mid_range, tmp2, trim2_mid_tang)) ? 1 : 0;
	    if (evals == 2) {
		edge_mid_tang = (trim1_start_tang + trim2_start_tang) / 2;
	    } else {
		edge_mid_tang = ON_3dVector::UnsetVector;
	    }
	}

	evals = 0;
	evals += (surface_EvNormal(s1, trim1_mid_2d.x, trim1_mid_2d.y, tmp1, trim1_mid_norm)) ? 1 : 0;
	if (trim.Face()->m_bRev) {
	    trim1_mid_norm = trim1_mid_norm  * -1.0;
	}

	evals += (surface_EvNormal(s2, trim2_mid_2d.x, trim2_mid_2d.y, tmp2, trim2_mid_norm)) ? 1 : 0;
	if (trim2->Face()->m_bRev) {
	    trim2_mid_norm = trim2_mid_norm  * -1.0;
	}

	if (evals != 2) {
	    bu_log("problem with mid normal evals\n");
	}

	ON_3dPoint *nmp = new ON_3dPoint(edge_mid_3d);
	CDT_Add3DPnt(s_cdt, nmp, -1, -1, -1, edge->m_edge_index, edge_mid_range, 0);
	s_cdt->edge_pnts->insert(nmp);

	BrepTrimPoint *mbtp1 = Add_BrepTrimPoint(s_cdt, trim1_param_points, nmp, NULL, edge_mid_tang, edge_mid_range, trim1_mid_2d, trim1_mid_norm, trim1_mid_range, trim.m_trim_index);

	BrepTrimPoint *mbtp2 = Add_BrepTrimPoint(s_cdt, trim2_param_points, nmp, NULL, edge_mid_tang, edge_mid_range, trim2_mid_2d, trim2_mid_norm, trim2_mid_range, trim2->m_trim_index);

	getEdgePoints(s_cdt, edge, nc, trim, sbtp1, mbtp1, sbtp2, mbtp2, &cdt_tol, trim1_param_points, trim2_param_points, loop_min_dist);
	getEdgePoints(s_cdt, edge, nc, trim, mbtp1, ebtp1, mbtp2, ebtp2, &cdt_tol, trim1_param_points, trim2_param_points, loop_min_dist);

    } else {

	getEdgePoints(s_cdt, edge, nc, trim, sbtp1, ebtp1, sbtp2, ebtp2, &cdt_tol, trim1_param_points, trim2_param_points, loop_min_dist);

    }

    return trim1_param_points;
}

// Untested, need to step through
int
SplitEdgeSegment(
	struct ON_Brep_CDT_State *s_cdt,
	ON_BrepEdge *edge,
	ON_BrepTrim &trim,
	BrepTrimPoint *t1_1,
	BrepTrimPoint *t1_2
	)
{
    int evals = 0;
    ON_3dPoint tmp1, tmp2;

    // Get the other trim
    // TODO - this won't work if we don't have a 1->2 edge to trims relationship - in that
    // case we'll have to split up the edge and find the matching sub-trims (possibly splitting
    // those as well if they don't line up at shared 3D points.)
    ON_BrepTrim *trim2 = (edge->Trim(0)->m_trim_index == trim.m_trim_index) ? edge->Trim(1) : edge->Trim(0);

    // By the time we are calling SplitEdgeSegment, both trims should have param points
    if (trim.m_trim_user.p == NULL || trim2->m_trim_user.p == NULL) {
	return -1;
    }

    std::map<double, BrepTrimPoint *> *trim1_param_points = (std::map<double, BrepTrimPoint *> *) trim.m_trim_user.p;
    std::map<double, BrepTrimPoint *> *trim2_param_points = (std::map<double, BrepTrimPoint *> *) trim2->m_trim_user.p;

    // Using trim 1 points, get corresponding trim 2 points
    BrepTrimPoint *t2_1 = t1_1->other_face_trim_pnt;
    BrepTrimPoint *t2_2 = t1_2->other_face_trim_pnt;

    const ON_Curve* crv = edge->EdgeCurveOf();
    ON_NurbsCurve *nc = crv->NurbsCurve();
    ON_Interval erange = nc->Domain();

    ON_Interval range1 = trim.Domain();
    ON_Interval range2 = trim2->Domain();

    /* Populate the 2D points */
    double st1 = t1_1->t;
    double et1 = t1_2->t;
    double st2 = t2_1->t;
    double et2 = t2_2->t;

    double edge_mid_range = (t1_1->e + t1_2->e) / 2.0;
    ON_3dVector edge_mid_tang, trim1_mid_norm, trim2_mid_norm = ON_3dVector::UnsetVector;
    ON_3dPoint edge_mid_3d = ON_3dPoint::UnsetPoint;
    int ev_tangent = nc->EvTangent(edge_mid_range, edge_mid_3d, edge_mid_tang);
    if (!ev_tangent) {
	// EvTangent call failed, get 3d point
	edge_mid_3d = nc->PointAt(edge_mid_range);
    }

    double trim1_mid_range;
    double trim2_mid_range;
    ON_3dPoint trim1_mid_2d = get_trim_midpt(&trim1_mid_range, &trim, st1, et1, edge_mid_3d, DBL_MAX, 0);
    ON_3dPoint trim2_mid_2d = get_trim_midpt(&trim2_mid_range, trim2, st2, et2, edge_mid_3d, DBL_MAX, 0);

    if (!ev_tangent) {
	// If the edge curve failed, try to average tangents from trims
	ON_3dVector trim1_mid_tang(0.0, 0.0, 0.0);
	ON_3dVector trim2_mid_tang(0.0, 0.0, 0.0);
	evals = 0;
	evals += (trim.EvTangent(trim1_mid_range, tmp1, trim1_mid_tang)) ? 1 : 0;
	evals += (trim2->EvTangent(trim2_mid_range, tmp2, trim2_mid_tang)) ? 1 : 0;
	if (evals == 2) {
	    ON_3dVector trim1_start_tang = t1_1->tangent;
	    ON_3dVector trim2_start_tang = t2_1->tangent;
	    edge_mid_tang = (trim1_start_tang + trim2_start_tang) / 2;
	} else {
	    edge_mid_tang = ON_3dVector::UnsetVector;
	}
    }

    const ON_Surface *s1 = trim.SurfaceOf();
    const ON_Surface *s2 = trim2->SurfaceOf();

    evals = 0;
    evals += (surface_EvNormal(s1, trim1_mid_2d.x, trim1_mid_2d.y, tmp1, trim1_mid_norm)) ? 1 : 0;
    if (trim.Face()->m_bRev) {
	trim1_mid_norm = trim1_mid_norm  * -1.0;
    }

    evals += (surface_EvNormal(s2, trim2_mid_2d.x, trim2_mid_2d.y, tmp2, trim2_mid_norm)) ? 1 : 0;
    if (trim2->Face()->m_bRev) {
	trim2_mid_norm = trim2_mid_norm  * -1.0;
    }

    if (evals != 2) {
	bu_log("problem with mid normal evals\n");
    }

    ON_3dPoint *nmp = new ON_3dPoint(edge_mid_3d);
    CDT_Add3DPnt(s_cdt, nmp, -1, -1, -1, edge->m_edge_index, edge_mid_range, 0);
    s_cdt->edge_pnts->insert(nmp);

    (void)Add_BrepTrimPoint(s_cdt, trim1_param_points, nmp, NULL, edge_mid_tang, edge_mid_range, trim1_mid_2d, trim1_mid_norm, trim1_mid_range, trim.m_trim_index);

    (void)Add_BrepTrimPoint(s_cdt, trim2_param_points, nmp, NULL, edge_mid_tang, edge_mid_range, trim2_mid_2d, trim2_mid_norm, trim2_mid_range, trim2->m_trim_index);

    return 0;
}

bool
build_poly2tri_polylines(struct ON_Brep_CDT_Face_State *f, p2t::CDT **cdt, int init_rtree)
{
    // Process through loops, building Poly2Tri polygons for facetization.
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::map<ON_3dPoint *, std::set<p2t::Point *>> *on3_to_tri = f->on3_to_tri_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
    std::vector<p2t::Point*> polyline;
    ON_BrepFace &face = f->s_cdt->brep->m_F[f->ind];
    int loop_cnt = face.LoopCount();
    ON_SimpleArray<BrepTrimPoint> *brep_loop_points = f->face_loop_points;
    bool outer = true;
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li].Count();
	if (num_loop_points > 2) {
	    for (int i = 1; i < num_loop_points; i++) {
		// map point to last entry to 3d point
		p2t::Point *p = new p2t::Point((brep_loop_points[li])[i].p2d.x, (brep_loop_points[li])[i].p2d.y);
		polyline.push_back(p);
		(*f->p2t_trim_ind)[p] = (brep_loop_points[li])[i].trim_ind;
		(*pointmap)[p] = (brep_loop_points[li])[i].p3d;
		(*on3_to_tri)[(brep_loop_points[li])[i].p3d].insert(p);
		(*normalmap)[p] = (brep_loop_points[li])[i].n3d;
	    }
	    if (init_rtree) {
		for (int i = 1; i < brep_loop_points[li].Count(); i++) {
		    // map point to last entry to 3d point
		    ON_Line *line = new ON_Line((brep_loop_points[li])[i - 1].p2d, (brep_loop_points[li])[i].p2d);
		    ON_BoundingBox bb = line->BoundingBox();

		    bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
		    bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
		    bb.m_max.z = bb.m_max.z + ON_ZERO_TOLERANCE;
		    bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
		    bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
		    bb.m_min.z = bb.m_min.z - ON_ZERO_TOLERANCE;

		    f->rt_trims->Insert2d(bb.Min(), bb.Max(), line);
		}
	    }
	    if (outer) {
		if (f->tris->size() > 0) {
		    ON_Brep_CDT_Face_Reset(f, init_rtree);
		}
		(*cdt) = new p2t::CDT(polyline);
		outer = false;
	    } else {
		(*cdt)->AddHole(polyline);
	    }
	    polyline.clear();
	}
    }
    return outer;
}

void
Process_Loop_Edges(struct ON_Brep_CDT_Face_State *f, int li, fastf_t max_dist)
{
    struct ON_Brep_CDT_State *s_cdt = f->s_cdt;
    ON_SimpleArray<BrepTrimPoint> *points = &(f->face_loop_points[li]);
    const ON_BrepFace &face = s_cdt->brep->m_F[f->ind];
    const ON_BrepLoop *loop = face.Loop(li);
    int trim_count = loop->TrimCount();

    for (int lti = 0; lti < trim_count; lti++) {
	ON_BrepTrim *trim = loop->Trim(lti);
	ON_BrepEdge *edge = trim->Edge();

	/* Provide 2D points for p2d, but we need to be aware that this will
	 * result in (trivially) degenerate 3D faces that we need to filter out
	 * when assembling a mesh */
	if (trim->m_type == ON_BrepTrim::singular) {
	    BrepTrimPoint btp;
	    const ON_BrepVertex& v1 = face.Brep()->m_V[trim->m_vi[0]];
	    ON_3dPoint *p3d = (*s_cdt->vert_pnts)[v1.m_vertex_index];
	    (*s_cdt->faces)[face.m_face_index]->strim_pnts->insert(std::make_pair(trim->m_trim_index, p3d));
	    ON_3dPoint *n3d = (*s_cdt->vert_avg_norms)[v1.m_vertex_index];
	    if (n3d) {
		(*s_cdt->faces)[face.m_face_index]->strim_norms->insert(std::make_pair(trim->m_trim_index, n3d));
	    }
	    double delta =  trim->Domain().Length() / 10.0;
	    ON_Interval trim_dom = trim->Domain();

	    for (int i = 1; i <= 10; i++) {
		btp.p3d = p3d;
		btp.n3d = n3d;
		btp.p2d = v1.Point();
		btp.t = trim->Domain().m_t[0] + (i - 1) * delta;
		btp.p2d = trim->PointAt(btp.t);
		btp.e = ON_UNSET_VALUE;
		points->Append(btp);
	    }
	    // skip last point of trim if not last trim
	    if (lti < trim_count - 1)
		continue;

	    const ON_BrepVertex& v2 = face.Brep()->m_V[trim->m_vi[1]];
	    btp.p3d = p3d;
	    btp.n3d = n3d;
	    btp.p2d = v2.Point();
	    btp.t = trim->Domain().m_t[1];
	    btp.p2d = trim->PointAt(btp.t);
	    btp.e = ON_UNSET_VALUE;
	    points->Append(btp);

	    continue;
	}

	if (!trim->m_trim_user.p) {
	    (void)getEdgePoints(s_cdt, edge, *trim, max_dist, DBL_MAX);
	    if (!trim->m_trim_user.p) {
		//bu_log("Failed to initialize trim->m_trim_user.p: Trim %d (associated with Edge %d) point count: %zd\n", trim->m_trim_index, trim->Edge()->m_edge_index, m->size());
		continue;
	    }
	}

	// If we can bound it, assemble the trim segments in order on the
	// loop array (which will in turn be used to generate the poly2tri
	// polyline for CDT)
	ON_3dPoint boxmin, boxmax;
	if (trim->GetBoundingBox(boxmin, boxmax, false)) {
	    std::map<double, BrepTrimPoint *> *param_points3d = (std::map<double, BrepTrimPoint *> *)trim->m_trim_user.p;
	    std::map<double, BrepTrimPoint*>::const_iterator i, ni;
	    for (i = param_points3d->begin(); i != param_points3d->end();) {
		BrepTrimPoint *btp = (*i).second;
		ni = ++i;
		// skip last point of trim if not last trim
		if (ni == param_points3d->end()) {
		    if (lti < trim_count - 1) {
			continue;
		    }
		}
		points->Append(*btp);
	    }
	}
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

