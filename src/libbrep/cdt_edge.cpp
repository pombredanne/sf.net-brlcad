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
/** @file cdt_edge.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"
#include <queue>
#include <numeric>
#include "bg/chull.h"
#include "./cdt.h"

#define BREP_PLANAR_TOL 0.05
#define MAX_TRIANGULATION_ATTEMPTS 5

#if 0
int debug_ecnt;

static void
debug_plot(struct ON_Brep_CDT_State *s_cdt, cdt_mesh::cpolygon_t *cpoly, int m_face_index, int m_loop_index, int m_edge_index, int m_trim_index, int step_cnt, int *d_cnt) {
    if (m_face_index > 0 && m_face_index != 34) return;
    if (m_edge_index > 0 && (m_edge_index < 93 || m_edge_index > 96)) return;
    cdt_mesh::cdt_mesh_t *fmesh = &s_cdt->fmeshes[34];
    std::cout << "\n";
    if (m_loop_index > 0) {
	std::cout << step_cnt << "-" << (*d_cnt) << ": generating plots for loop " << m_loop_index << "...\n";
    }
    if (m_edge_index > 0) {
	std::cout << step_cnt << "-" << (*d_cnt) << ": generating plots for edge " << m_edge_index << "...\n";
    }
    if (m_trim_index > 0) {
	std::cout << step_cnt << "-" << (*d_cnt) << ": generating plots for trim " << m_trim_index << "...\n";
    }
    cpoly->print();
    fmesh->polygon_print_3d(cpoly);
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%d-%d-%d-poly2d.p3", m_face_index, step_cnt, (*d_cnt));
    fmesh->polygon_plot_2d(cpoly, bu_vls_cstr(&fname));
    bu_vls_sprintf(&fname, "%d-%d-%d-poly3d.p3", m_face_index, step_cnt, (*d_cnt));
    fmesh->polygon_plot_3d(cpoly, bu_vls_cstr(&fname));
    fmesh->cdt();
    bu_vls_sprintf(&fname, "%d-%d-%d-tris_2d.p3", m_face_index, step_cnt, (*d_cnt));
    fmesh->tris_plot_2d(bu_vls_cstr(&fname));
    bu_vls_sprintf(&fname, "%d-%d-%d-tris.p3", m_face_index, step_cnt, (*d_cnt));
    fmesh->tris_plot(bu_vls_cstr(&fname));
    (*d_cnt)++;
    std::cout << "\n";
}
#endif
#if 0
static void
debug_bseg(cdt_mesh::bedge_seg_t *bseg, int seg_id)
{
#if 0
    int face_index = 34;
    if (bseg->edge_ind < 93 && bseg->edge_ind > 96) return;
#endif
    ON_BrepEdge& edge = bseg->brep->m_E[bseg->edge_ind];
    ON_BrepTrim *trim1 = edge.Trim(0);
    ON_BrepTrim *trim2 = edge.Trim(1);
    if (!trim1 || !trim2) return ;

#if 0
    if (trim1->Face()->m_face_index != face_index && trim2->Face()->m_face_index != face_index) return;
    ON_BrepTrim *ftrim = (trim1->Face()->m_face_index == face_index) ? trim1 : trim2;
    cdt_mesh::cpolyedge_t *tseg = (bseg->tseg1->trim_ind == ftrim->m_trim_index) ? bseg->tseg1 : bseg->tseg2;

    std::cout << "bseg " << bseg->edge_ind << "-" << seg_id << ", trim " << tseg->trim_ind << " (" << bseg->brep->m_T[tseg->trim_ind].m_bRev3d << "):\n";
    std::cout << "   edge point start (x,y,z): (" << bseg->e_start->x << "," << bseg->e_start->y << "," << bseg->e_start->z << ")\n";
    ON_3dPoint es = bseg->nc->PointAt(bseg->edge_start);
    std::cout << "   edge_start (t)(x,y,z): (" << bseg->edge_start << ") (" << es.x << "," << es.y << "," << es.z << ")\n";
    std::cout << "   edge point end   (x,y,z): (" << bseg->e_end->x << "," << bseg->e_end->y << "," << bseg->e_end->z << ")\n";
    ON_3dPoint ee = bseg->nc->PointAt(bseg->edge_end);
    std::cout << "   edge_end (t)(x,y,z): (" << bseg->edge_end << ") (" << ee.x << "," << ee.y << "," << ee.z << ")\n";

    ON_2dPoint p2s = ftrim->PointAt(tseg->trim_start);
    ON_2dPoint p2e = ftrim->PointAt(tseg->trim_end);
    ON_3dPoint p3s = ftrim->SurfaceOf()->PointAt(p2s.x, p2s.y);
    ON_3dPoint p3e = ftrim->SurfaceOf()->PointAt(p2e.x, p2e.y);
    std::cout << "   start point: p2d(x,y): " << p2s.x << "," << p2s.y << ") p3d(x,y,z): (" << p3s.x << "," << p3s.y << "," << p3s.z << ")\n";
    std::cout << "   end point  : p2d(x,y): " << p2e.x << "," << p2e.y << ") p3d(x,y,z): (" << p3e.x << "," << p3e.y << "," << p3e.z << ")\n";

    if (ftrim->m_bRev3d) {
	if (bseg->e_start->DistanceTo(p3e) > BN_TOL_DIST) {
	    std::cout << "          WARNING - bseg edge and trim start points don't match\n";
	}
	if (bseg->e_end->DistanceTo(p3s) > BN_TOL_DIST) {
	    std::cout << "          WARNING - bseg edge and trim end points don't match\n";
	}
    } else {
	if (bseg->e_start->DistanceTo(p3s) > BN_TOL_DIST) {
	    std::cout << "          WARNING - bseg edge and trim start points don't match\n";
	}
	if (bseg->e_end->DistanceTo(p3e) > BN_TOL_DIST) {
	    std::cout << "          WARNING - bseg edge and trim end points don't match\n";
	}
    }
#endif

    std::cout << "bseg " << bseg->edge_ind << "-" << seg_id << ":\n";
    std::cout << "tseg1(" << bseg->tseg1->v[0] << "," << bseg->tseg1->v[1] << ") polygon: ";
    bseg->tseg1->polygon->print();
    std::cout << "tseg2(" << bseg->tseg2->v[0] << "," << bseg->tseg2->v[1] << ") polygon: ";
    bseg->tseg2->polygon->print();
    std::cout << "\n";

}
#endif

// TODO - can we base the width of this box on how far the 3D edge curve midpoint is from
// the midpoint of the 3D line segment?  That might relate the 2D box size to how far away
// we need to be to avoid problematic surface points...
//
// Probably still want some minimum distance to avoid extremely slim triangles, even if they
// are "valid"...
void
rtree_bbox_2d(struct ON_Brep_CDT_State *s_cdt, cdt_mesh::cpolyedge_t *pe, int tight)
{
    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    ON_2dPoint p2d1(pe->polygon->pnts_2d[pe->v[0]].first, pe->polygon->pnts_2d[pe->v[0]].second);
    ON_2dPoint p2d2(pe->polygon->pnts_2d[pe->v[1]].first, pe->polygon->pnts_2d[pe->v[1]].second);
    double dist = p2d1.DistanceTo(p2d2);
    double bdist = 0.5*dist;

    // If we have an associated 3D edge, we need a surface point that will
    // result in a sensible triangle near that edge.
    if (!tight && pe->eseg) {
	ON_3dPoint p13d(p2d1.x, p2d1.y, 0);
	ON_3dPoint p23d(p2d2.x, p2d2.y, 0);
	ON_3dPoint p1norm(p2d1.x, p2d1.y, 1);
	ON_3dVector vtrim = p23d - p13d;
	ON_3dVector vnorm = p1norm - p13d;
	vtrim.Unitize();
	vnorm.Unitize();
	ON_3dVector ndir = ON_CrossProduct(vnorm, vtrim);
	ndir.Unitize();
	ndir = ndir * bdist;
	ON_2dVector ndir2d(ndir.x, ndir.y);
	ON_2dPoint p2mid = (p2d1 + p2d2) * 0.5;
	pe->spnt = p2mid + ndir2d;
	pe->defines_spnt = true;
    }

    ON_Line line(p2d1, p2d2);
    pe->bb = line.BoundingBox();
    pe->bb.m_max.x = pe->bb.m_max.x + ON_ZERO_TOLERANCE;
    pe->bb.m_max.y = pe->bb.m_max.y + ON_ZERO_TOLERANCE;
    pe->bb.m_min.x = pe->bb.m_min.x - ON_ZERO_TOLERANCE;
    pe->bb.m_min.y = pe->bb.m_min.y - ON_ZERO_TOLERANCE;

    if (!tight) {
	double xdist = pe->bb.m_max.x - pe->bb.m_min.x;
	double ydist = pe->bb.m_max.y - pe->bb.m_min.y;
	// If we're close to the edge, we want to know - the Search callback will
	// check the precise distance and make a decision on what to do.
	if (xdist < bdist) {
	    pe->bb.m_min.x = pe->bb.m_min.x - 0.5*bdist;
	    pe->bb.m_max.x = pe->bb.m_max.x + 0.5*bdist;
	}
	if (ydist < bdist) {
	    pe->bb.m_min.y = pe->bb.m_min.y - 0.5*bdist;
	    pe->bb.m_max.y = pe->bb.m_max.y + 0.5*bdist;
	}

	if (pe->eseg) {
	    pe->bb.Set(pe->spnt, true);
	}
    }

    double p1[2];
    p1[0] = pe->bb.Min().x;
    p1[1] = pe->bb.Min().y;
    double p2[2];
    p2[0] = pe->bb.Max().x;
    p2[1] = pe->bb.Max().y;
    s_cdt->face_rtrees_2d[trim.Face()->m_face_index].Insert(p1, p2, (void *)pe);
}

void
rtree_bbox_2d_remove(struct ON_Brep_CDT_State *s_cdt, cdt_mesh::cpolyedge_t *pe)
{
    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    ON_2dPoint p2d1(pe->polygon->pnts_2d[pe->v[0]].first, pe->polygon->pnts_2d[pe->v[0]].second);
    ON_2dPoint p2d2(pe->polygon->pnts_2d[pe->v[1]].first, pe->polygon->pnts_2d[pe->v[1]].second);
    ON_Line line(p2d1, p2d2);
    ON_BoundingBox bb = line.BoundingBox();
    bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
    bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
    bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
    bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;

    double dist = p2d1.DistanceTo(p2d2);
    double bdist = 0.5*dist;
    double xdist = bb.m_max.x - bb.m_min.x;
    double ydist = bb.m_max.y - bb.m_min.y;
   // Be slightly more aggressive in the size of this bbox than when adding,
   // since we want to avoid floating point weirdness when it comes to the
   // RTree Remove routine looking for this box
    if (xdist < bdist) {
	bb.m_min.x = bb.m_min.x - 0.51*bdist;
	bb.m_max.x = bb.m_max.x + 0.51*bdist;
    }
    if (ydist < bdist) {
	bb.m_min.y = bb.m_min.y - 0.51*bdist;
	bb.m_max.y = bb.m_max.y + 0.51*bdist;
    }

    double p1[2];
    p1[0] = bb.Min().x;
    p1[1] = bb.Min().y;
    double p2[2];
    p2[0] = bb.Max().x;
    p2[1] = bb.Max().y;
    s_cdt->face_rtrees_2d[trim.Face()->m_face_index].Remove(p1, p2, (void *)pe);
}

void
rtree_bbox_3d(struct ON_Brep_CDT_State *s_cdt, cdt_mesh::cpolyedge_t *pe)
{
    if (!pe->eseg) return;
    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    double tcparam = (pe->trim_start + pe->trim_end) / 2.0;
    ON_3dPoint trim_2d = trim.PointAt(tcparam);
    const ON_Surface *s = trim.SurfaceOf();
    ON_3dPoint trim_3d = s->PointAt(trim_2d.x, trim_2d.y);

    ON_3dPoint *p3d1 = pe->eseg->e_start;
    ON_3dPoint *p3d2 = pe->eseg->e_end;
    ON_Line line(*p3d1, *p3d2);

    double arc_dist = 2*trim_3d.DistanceTo(line.ClosestPointTo(trim_3d));

    ON_BoundingBox bb = line.BoundingBox();
    bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
    bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
    bb.m_max.z = bb.m_max.z + ON_ZERO_TOLERANCE;
    bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
    bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
    bb.m_min.z = bb.m_min.z - ON_ZERO_TOLERANCE;

    double dist = p3d1->DistanceTo(*p3d2);
    double bdist = (0.5*dist > arc_dist) ? 0.5*dist : arc_dist;
    double xdist = bb.m_max.x - bb.m_min.x;
    double ydist = bb.m_max.y - bb.m_min.y;
    double zdist = bb.m_max.z - bb.m_min.z;
    // If we're close to the edge, we want to know - the Search callback will
    // check the precise distance and make a decision on what to do.
    if (xdist < bdist) {
	bb.m_min.x = bb.m_min.x - 0.5*bdist;
	bb.m_max.x = bb.m_max.x + 0.5*bdist;
    }
    if (ydist < bdist) {
	bb.m_min.y = bb.m_min.y - 0.5*bdist;
	bb.m_max.y = bb.m_max.y + 0.5*bdist;
    }
    if (zdist < bdist) {
	bb.m_min.z = bb.m_min.z - 0.5*bdist;
	bb.m_max.z = bb.m_max.z + 0.5*bdist;
    }

    double p1[3];
    p1[0] = bb.Min().x;
    p1[1] = bb.Min().y;
    p1[2] = bb.Min().z;
    double p2[3];
    p2[0] = bb.Max().x;
    p2[1] = bb.Max().y;
    p2[2] = bb.Max().z;
    s_cdt->face_rtrees_3d[trim.Face()->m_face_index].Insert(p1, p2, (void *)pe);
}

struct rtree_minsplit_context {
    struct ON_Brep_CDT_State *s_cdt;
    cdt_mesh::cpolyedge_t *cseg;
};

static bool MinSplit2dCallback(void *data, void *a_context) {
    cdt_mesh::cpolyedge_t *tseg = (cdt_mesh::cpolyedge_t *)data;
    struct rtree_minsplit_context *context= (struct rtree_minsplit_context *)a_context;


    plot_ce_bbox(context->s_cdt, tseg, "l.p3");

    // Intersecting with oneself isn't cause for splitting
    if (tseg == context->cseg || tseg == context->cseg->prev || tseg == context->cseg->next) return true;

    // Someone needs to split - figure out if it's us
    ON_2dPoint cp2d1(context->cseg->polygon->pnts_2d[context->cseg->v[0]].first, context->cseg->polygon->pnts_2d[context->cseg->v[0]].second);
    ON_2dPoint cp2d2(context->cseg->polygon->pnts_2d[context->cseg->v[1]].first, context->cseg->polygon->pnts_2d[context->cseg->v[1]].second);
    ON_2dPoint tp2d1(tseg->polygon->pnts_2d[tseg->v[0]].first, tseg->polygon->pnts_2d[tseg->v[0]].second);
    ON_2dPoint tp2d2(tseg->polygon->pnts_2d[tseg->v[1]].first, tseg->polygon->pnts_2d[tseg->v[1]].second);
    double cdist = cp2d1.DistanceTo(cp2d2);
    double tdist = tp2d1.DistanceTo(tp2d2);

    // If tseg is longer, it should be the one to split, but we'll need
    // to re-check this trim in the next pass
    if (cdist < tdist && !NEAR_EQUAL(cdist, tdist, ON_ZERO_TOLERANCE)) {
	context->cseg->split_status = 1;
	return true;
    }

    // Mark this segment down as a segment to split
    context->cseg->split_status = 2;

    // No need to keep checking if we already know we're going to split
    return false;
}

double
median_seg_len(std::vector<double> &lsegs)
{
    // Get the median segment length (https://stackoverflow.com/a/42791986)
    double median, e1, e2;
    std::vector<double>::iterator v1, v2;

    if (!lsegs.size()) return -DBL_MAX;
    if (lsegs.size() % 2 == 0) {
	v1 = lsegs.begin() + lsegs.size() / 2 - 1;
	v2 = lsegs.begin() + lsegs.size() / 2;
	std::nth_element(lsegs.begin(), v1, lsegs.end());
	e1 = *v1;
	std::nth_element(lsegs.begin(), v2, lsegs.end());
	e2 = *v2;
	median = (e1+e2)*0.5;
    } else {
	v2 = lsegs.begin() + lsegs.size() / 2;
	std::nth_element(lsegs.begin(), v2, lsegs.end());
	median = *v2;
    }

    return median;
}

double
edge_median_seg_len(struct ON_Brep_CDT_State *s_cdt, int m_edge_index)
{
    std::vector<double> lsegs;
    std::vector<cdt_mesh::bedge_seg_t *> &epsegs = s_cdt->e2polysegs[m_edge_index];
    std::vector<cdt_mesh::bedge_seg_t *>::iterator e_it;
    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
	cdt_mesh::bedge_seg_t *b = *e_it;
	double seg_dist = b->e_start->DistanceTo(*b->e_end);
	lsegs.push_back(seg_dist);
    }
    return median_seg_len(lsegs);
}

ON_3dVector
trim_normal(ON_BrepTrim *trim, ON_2dPoint &cp)
{
    ON_3dVector norm = ON_3dVector::UnsetVector;
    if (trim->m_type != ON_BrepTrim::singular) {
	// 3D points are globally unique, but normals are not - the same edge point may
	// have different normals from two faces at a sharp edge.  Calculate the
	// face normal for this point on this surface.
	ON_Plane fplane;
	const ON_Surface *s = trim->SurfaceOf();
	double ptol = s->BoundingBox().Diagonal().Length()*0.001;
	ptol = (ptol < BREP_PLANAR_TOL) ? ptol : BREP_PLANAR_TOL;
	if (s->IsPlanar(&fplane, ptol)) {
	    norm = fplane.Normal();
	} else {
	    ON_3dPoint tmp1;
	    surface_EvNormal(trim->SurfaceOf(), cp.x, cp.y, tmp1, norm);
	}
	if (trim->Face()->m_bRev) {
	    norm = -1 * norm;
	}
	//std::cout << "Face " << trim->Face()->m_face_index << ", Loop " << trim->Loop()->m_loop_index << " norm: " << norm.x << "," << norm.y << "," << norm.z << "\n";
    }
    return norm;
}


double
pnt_binary_search(fastf_t *tparam, const ON_BrepTrim &trim, double tstart, double tend, ON_3dPoint &edge_3d, double tol, int verbose, int depth, int force)
{
    double tcparam = (tstart + tend) / 2.0;
    ON_3dPoint trim_2d = trim.PointAt(tcparam);
    const ON_Surface *s = trim.SurfaceOf();
    ON_3dPoint trim_3d = s->PointAt(trim_2d.x, trim_2d.y);
    double dist = edge_3d.DistanceTo(trim_3d);

    if (dist > tol && !force) {
	ON_3dPoint trim_start_2d = trim.PointAt(tstart);
	ON_3dPoint trim_end_2d = trim.PointAt(tend);
	ON_3dPoint trim_start_3d = s->PointAt(trim_start_2d.x, trim_start_2d.y);
	ON_3dPoint trim_end_3d = s->PointAt(trim_end_2d.x, trim_end_2d.y);

	ON_3dVector v1 = edge_3d - trim_start_3d;
	ON_3dVector v2 = edge_3d - trim_end_3d;
	double sedist = trim_start_3d.DistanceTo(trim_end_3d);

	if (verbose) {
	    //bu_log("start point (%f %f %f) and end point (%f %f %f)\n", trim_start_3d.x, trim_start_3d.y, trim_start_3d.z, trim_end_3d.x, trim_end_3d.y, trim_end_3d.z);
	}

	double vdot = ON_DotProduct(v1,v2);

	if (vdot < 0 && dist > ON_ZERO_TOLERANCE) {
	    //if (verbose)
	    //	bu_log("(%f - %f - %f (%f): searching left and right subspans\n", tstart, tcparam, tend, ON_DotProduct(v1,v2));
	    double tlparam, trparam;
	    double fldist = pnt_binary_search(&tlparam, trim, tstart, tcparam, edge_3d, tol, verbose, depth+1, 0);
	    double frdist = pnt_binary_search(&trparam, trim, tcparam, tend, edge_3d, tol, verbose, depth+1, 0);
	    if (fldist >= 0 && frdist < -1) {
		//	if (verbose)
		//	    bu_log("(%f - %f - %f: going with fldist: %f\n", tstart, tcparam, tend, fldist);
		(*tparam) = tlparam;
		return fldist;
	    }
	    if (frdist >= 0 && fldist < -1) {
		//	if (verbose)
		//	    bu_log("(%f - %f - %f: going with frdist: %f\n", tstart, tcparam, tend, frdist);
		(*tparam) = trparam;
		return frdist;
	    }
	    if (fldist < -1 && frdist < -1) {
		fldist = pnt_binary_search(&tlparam, trim, tstart, tcparam, edge_3d, tol, verbose, depth+1, 1);
		frdist = pnt_binary_search(&trparam, trim, tcparam, tend, edge_3d, tol, verbose, depth+1, 1);
		if (verbose) {
		    bu_log("Trim %d: point not in either subspan according to dot product (distances are %f and %f, distance between sampling segment ends is %f), forcing the issue\n", trim.m_trim_index, fldist, frdist, sedist);
		}

		if ((fldist < frdist) && (fldist < dist)) {
		    (*tparam) = tlparam;
		    return fldist;
		}
		if ((frdist < fldist) && (frdist < dist)) {
		    (*tparam) = trparam;
		    return frdist;
		}
		(*tparam) = tcparam;
		return dist;

	    }
	} else if (NEAR_ZERO(vdot, ON_ZERO_TOLERANCE)) {
	    (*tparam) = tcparam;
	    return dist;
	} else {
	    // Not in this span
	    if (verbose && depth < 2) {
		//bu_log("Trim %d: (%f:%f)%f - edge point (%f %f %f) and trim point (%f %f %f): distance between them is %f, tol is %f, search seg length: %f\n", trim.m_trim_index, tstart, tend, ON_DotProduct(v1,v2), edge_3d.x, edge_3d.y, edge_3d.z, trim_3d.x, trim_3d.y, trim_3d.z, dist, tol, sedist);
	    }
	    if (depth == 0) {
		(*tparam) = tcparam;
		return dist;
	    } else {
		return -2;
	    }
	}
    }

    // close enough - this works
    //if (verbose)
    //	bu_log("Workable (%f:%f) - edge point (%f %f %f) and trim point (%f %f %f): %f, %f\n", tstart, tend, edge_3d.x, edge_3d.y, edge_3d.z, trim_3d.x, trim_3d.y, trim_3d.z, dist, tol);

    (*tparam) = tcparam;
    return dist;
}

ON_2dPoint
get_trim_midpt(fastf_t *t, struct ON_Brep_CDT_State *s_cdt, cdt_mesh::cpolyedge_t *pe, ON_3dPoint &edge_mid_3d, double elen, double brep_edge_tol)
{
    int verbose = 1;
    double tol;
    if (!NEAR_EQUAL(brep_edge_tol, ON_UNSET_VALUE, ON_ZERO_TOLERANCE)) {
	tol = brep_edge_tol;
    } else {
	tol = (elen < BN_TOL_DIST) ? 0.01*elen : 0.1*BN_TOL_DIST;
    }
    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];

    double tmid;
    double dist = pnt_binary_search(&tmid, trim, pe->trim_start, pe->trim_end, edge_mid_3d, tol, 0, 0, 0);
    if (dist < 0) {
	if (verbose) {
	    bu_log("Warning - could not find suitable trim point\n");
	}
	tmid = (pe->trim_start + pe->trim_end) / 2.0;
    } else {
	if (verbose && (dist > BN_TOL_DIST) && (dist > tol)) {
	    if (trim.m_bRev3d) {
		//bu_log("Reversed trim: going with distance %f greater than desired tolerance %f\n", dist, tol);
	    } else {
		//bu_log("Non-reversed trim: going with distance %f greater than desired tolerance %f\n", dist, tol);
	    }
	    if (dist > 10*tol) {
		dist = pnt_binary_search(&tmid, trim, pe->trim_start, pe->trim_end, edge_mid_3d, tol, 0, 0, 0);
	    }
	}
    }

    ON_2dPoint trim_mid_2d = trim.PointAt(tmid);
    (*t) = tmid;
    return trim_mid_2d;
}

bool
tol_need_split(struct ON_Brep_CDT_State *s_cdt, cdt_mesh::bedge_seg_t *bseg, ON_3dPoint &edge_mid_3d)
{
    ON_Line line3d(*(bseg->e_start), *(bseg->e_end));
    double seg_len = line3d.Length();

    double max_allowed = (s_cdt->tol.absmax > ON_ZERO_TOLERANCE) ? s_cdt->tol.absmax : 1.1*bseg->cp_len;
    double min_allowed = (s_cdt->tol.rel > ON_ZERO_TOLERANCE) ? s_cdt->tol.rel * bseg->cp_len : 0.0;
    double max_edgept_dist_from_edge = (s_cdt->tol.abs > ON_ZERO_TOLERANCE) ? s_cdt->tol.abs : seg_len;
    ON_BrepLoop *l1 = s_cdt->brep->m_T[bseg->tseg1->trim_ind].Loop();
    ON_BrepLoop *l2 = s_cdt->brep->m_T[bseg->tseg2->trim_ind].Loop();
    const ON_Surface *s1= l1->SurfaceOf();
    const ON_Surface *s2= l2->SurfaceOf();
    double len_1 = -1;
    double len_2 = -1;
    double s_len;

    switch (bseg->edge_type) {
	case 0:
	    // singularity splitting is handled in a separate step, since it isn't based
	    // on 3D information
	    return false;
	case 1:
	    // Curved edge - default assigned values are correct.
	    break;
	case 2:
	    // Linear edge on non-planar surface - use the median segment lengths
	    // from the trims from non-planar faces associated with this edge
	    len_1 = (!s1->IsPlanar(NULL, BN_TOL_DIST)) ? s_cdt->l_median_len[l1->m_loop_index] : -1;
	    len_2 = (!s2->IsPlanar(NULL, BN_TOL_DIST)) ? s_cdt->l_median_len[l2->m_loop_index] : -1;
	    if (len_1 < 0 && len_2 < 0) {
		bu_log("Error - both loops report invalid median lengths\n");
		return false;
	    }
	    s_len = (len_1 > 0) ? len_1 : len_2;
	    s_len = (len_2 > 0 && len_2 < s_len) ? len_2 : s_len;
	    max_allowed = 5*s_len;
	    min_allowed = 0.2*s_len;
	    break;
	case 3:
	    // Linear edge connected to one or more non-linear edges.  If the start or end points
	    // are the same as the root start or end points, use the median edge length of the
	    // connected edge per the vert lookup.
	    if (bseg->e_start == bseg->e_root_start || bseg->e_end == bseg->e_root_start) {
		len_1 = s_cdt->v_min_seg_len[bseg->e_root_start];
	    }
	    if (bseg->e_start == bseg->e_root_end || bseg->e_end == bseg->e_root_end) {
		len_2 = s_cdt->v_min_seg_len[bseg->e_root_end];
	    }
	    if (bseg->e_start == bseg->e_root_start || bseg->e_end == bseg->e_root_start) {
		if (len_1 < 0 && len_2 < 0) {
		    bu_log("Error - verts report invalid lengths on type 3 line segment\n");
		    return false;
		}
	    }
	    s_len = (len_1 > 0) ? len_1 : len_2;
	    s_len = (len_2 > 0 && len_2 < s_len) ? len_2 : s_len;
	    if (s_len > 0) {
		max_allowed = 2*s_len;
		min_allowed = 0.5*s_len;
	    }
	    break;
	case 4:
	    // Linear segment, no curves involved
	    break;
	default:
	    bu_log("Error - invalid edge type: %d\n", bseg->edge_type);
	    return false;
    }


    if (seg_len > max_allowed) return true;

    if (seg_len < min_allowed) return false;

    // If we're linear and not already split, tangents and normals won't change that
    if (bseg->edge_type > 1) return false;

    double dist3d = edge_mid_3d.DistanceTo(line3d.ClosestPointTo(edge_mid_3d));

    if (dist3d > max_edgept_dist_from_edge) return true;

    if ((bseg->tan_start * bseg->tan_end) < s_cdt->cos_within_ang) return true;

    ON_3dPoint *n1, *n2;

    ON_BrepEdge& edge = s_cdt->brep->m_E[bseg->edge_ind];
    ON_BrepTrim *trim1 = edge.Trim(0);
    ON_BrepFace *face1 = trim1->Face();
    cdt_mesh::cdt_mesh_t *fmesh1 = &s_cdt->fmeshes[face1->m_face_index];
    n1 = fmesh1->normals[fmesh1->nmap[fmesh1->p2ind[bseg->e_start]]];
    n2 = fmesh1->normals[fmesh1->nmap[fmesh1->p2ind[bseg->e_end]]];

    if (ON_3dVector(*n1) != ON_3dVector::UnsetVector && ON_3dVector(*n2) != ON_3dVector::UnsetVector) {
	if ((ON_3dVector(*n1) * ON_3dVector(*n2)) < s_cdt->cos_within_ang - VUNITIZE_TOL) return true;
    }

    ON_BrepTrim *trim2 = edge.Trim(1);
    ON_BrepFace *face2 = trim2->Face();
    cdt_mesh::cdt_mesh_t *fmesh2 = &s_cdt->fmeshes[face2->m_face_index];
    n1 = fmesh2->normals[fmesh2->nmap[fmesh2->p2ind[bseg->e_start]]];
    n2 = fmesh2->normals[fmesh2->nmap[fmesh2->p2ind[bseg->e_end]]];

    if (ON_3dVector(*n1) != ON_3dVector::UnsetVector && ON_3dVector(*n2) != ON_3dVector::UnsetVector) {
	if ((ON_3dVector(*n1) * ON_3dVector(*n2)) < s_cdt->cos_within_ang - VUNITIZE_TOL) return true;
    }

    return false;
}

std::set<cdt_mesh::bedge_seg_t *>
split_edge_seg(struct ON_Brep_CDT_State *s_cdt, cdt_mesh::bedge_seg_t *bseg, int force, int update_rtrees)
{
    std::set<cdt_mesh::bedge_seg_t *> nedges;

    // If we don't have associated segments, we can't do anything
    if (!bseg->tseg1 || !bseg->tseg2 || !bseg->nc) return nedges;

    ON_BrepEdge& edge = s_cdt->brep->m_E[bseg->edge_ind];
    ON_BrepTrim *trim1 = &s_cdt->brep->m_T[bseg->tseg1->trim_ind];
    ON_BrepTrim *trim2 = &s_cdt->brep->m_T[bseg->tseg2->trim_ind];

    // If we don't have associated trims, we can't do anything
    if (!trim1 || !trim2) return nedges;

    ON_BrepFace *face1 = trim1->Face();
    ON_BrepFace *face2 = trim2->Face();
    cdt_mesh::cdt_mesh_t *fmesh1 = &s_cdt->fmeshes[face1->m_face_index];
    cdt_mesh::cdt_mesh_t *fmesh2 = &s_cdt->fmeshes[face2->m_face_index];


    // Get the 3D midpoint (and tangent, if we can) from the edge curve
    ON_3dPoint edge_mid_3d = ON_3dPoint::UnsetPoint;
    ON_3dVector edge_mid_tan = ON_3dVector::UnsetVector;
    fastf_t emid = (bseg->edge_start + bseg->edge_end) / 2.0;
    bool evtangent_status = bseg->nc->EvTangent(emid, edge_mid_3d, edge_mid_tan);
    if (!evtangent_status) {
	// EvTangent call failed, get 3d point
	edge_mid_3d = bseg->nc->PointAt(emid);
	edge_mid_tan = ON_3dVector::UnsetVector;
    }

    // Unless we're forcing a split this is the point at which we do tolerance
    // based testing to determine whether to proceed with the split or halt.
    if (!force && !tol_need_split(s_cdt, bseg, edge_mid_3d)) {
	return nedges;
    }

    // edge_mid_3d is a new point in the cdt and the fmesh, as well as a new
    // edge point - add it to the appropriate containers
    ON_3dPoint *mid_3d = new ON_3dPoint(edge_mid_3d);
    CDT_Add3DPnt(s_cdt, mid_3d, -1, -1, -1, edge.m_edge_index, 0, 0);
    s_cdt->edge_pnts->insert(mid_3d);

    // Find the 2D points
    double elen1 = (bseg->nc->PointAt(bseg->edge_start)).DistanceTo(bseg->nc->PointAt(emid));
    double elen2 = (bseg->nc->PointAt(emid)).DistanceTo(bseg->nc->PointAt(bseg->edge_end));
    double elen = (elen1 + elen2) * 0.5;
    fastf_t t1mid, t2mid;
    ON_2dPoint trim1_mid_2d, trim2_mid_2d;
    trim1_mid_2d = get_trim_midpt(&t1mid, s_cdt, bseg->tseg1, edge_mid_3d, elen, edge.m_tolerance);
    trim2_mid_2d = get_trim_midpt(&t2mid, s_cdt, bseg->tseg2, edge_mid_3d, elen, edge.m_tolerance);

    // Update the 2D and 2D->3D info in the fmeshes
    long f1_ind2d = fmesh1->add_point(trim1_mid_2d);
    long f1_ind3d = fmesh1->add_point(mid_3d);
    fmesh1->p2d3d[f1_ind2d] = f1_ind3d;
    long f2_ind2d = fmesh2->add_point(trim2_mid_2d);
    long f2_ind3d = fmesh2->add_point(mid_3d);
    fmesh2->p2d3d[f2_ind2d] = f2_ind3d;

    // Trims get their own normals
    ON_3dVector norm1 = trim_normal(trim1, trim1_mid_2d);
    fmesh1->normals.push_back(new ON_3dPoint(norm1));
    long f1_nind = fmesh1->normals.size() - 1;
    fmesh1->nmap[f1_ind3d] = f1_nind;
    ON_3dVector norm2 = trim_normal(trim2, trim2_mid_2d);
    fmesh2->normals.push_back(new ON_3dPoint(norm2));
    long f2_nind = fmesh2->normals.size() - 1;
    fmesh2->nmap[f2_ind3d] = f2_nind;

    // From the existing polyedge, make the two new polyedges that will replace the old one
    cdt_mesh::bedge_seg_t *bseg1 = new cdt_mesh::bedge_seg_t(bseg);
    bseg1->edge_start = bseg->edge_start;
    bseg1->edge_end = emid;
    bseg1->e_start = bseg->e_start;
    bseg1->e_end = mid_3d;
    bseg1->tan_start = bseg->tan_start;
    bseg1->tan_end = edge_mid_tan;

    cdt_mesh::bedge_seg_t *bseg2 = new cdt_mesh::bedge_seg_t(bseg);
    bseg2->edge_start = emid;
    bseg2->edge_end = bseg->edge_end;
    bseg2->e_start = mid_3d;
    bseg2->e_end = bseg->e_end;
    bseg2->tan_start = edge_mid_tan;
    bseg2->tan_end = bseg->tan_end;

    // Remove the old segments from their respective rtrees
    if (update_rtrees) {
	rtree_bbox_2d_remove(s_cdt, bseg->tseg1);
	rtree_bbox_2d_remove(s_cdt, bseg->tseg2);
    }

    // Using the 2d mid points, update the polygons associated with tseg1 and tseg2.
    cdt_mesh::cpolyedge_t *poly1_ne1, *poly1_ne2, *poly2_ne1, *poly2_ne2;
    {
	cdt_mesh::cpolygon_t *poly1 = bseg->tseg1->polygon;
	int v[2];
	v[0] = bseg->tseg1->v[0];
	v[1] = bseg->tseg1->v[1];
	int trim_ind = bseg->tseg1->trim_ind;
	double old_trim_start = bseg->tseg1->trim_start;
	double old_trim_end = bseg->tseg1->trim_end;
	poly1->remove_ordered_edge(cdt_mesh::edge_t(v[0], v[1]));
	long poly1_2dind = poly1->add_point(trim1_mid_2d, f1_ind2d);
	struct cdt_mesh::edge_t poly1_edge1(v[0], poly1_2dind);
	poly1_ne1 = poly1->add_ordered_edge(poly1_edge1);
	poly1_ne1->trim_ind = trim_ind;
	poly1_ne1->trim_start = old_trim_start;
	poly1_ne1->trim_end = t1mid;
	struct cdt_mesh::edge_t poly1_edge2(poly1_2dind, v[1]);
	poly1_ne2 = poly1->add_ordered_edge(poly1_edge2);
	poly1_ne2->trim_ind = trim_ind;
	poly1_ne2->trim_start = t1mid;
	poly1_ne2->trim_end = old_trim_end;
    }
    {
	cdt_mesh::cpolygon_t *poly2 = bseg->tseg2->polygon;
	int v[2];
	v[0] = bseg->tseg2->v[0];
	v[1] = bseg->tseg2->v[1];
	int trim_ind = bseg->tseg2->trim_ind;
	double old_trim_start = bseg->tseg2->trim_start;
	double old_trim_end = bseg->tseg2->trim_end;
	poly2->remove_ordered_edge(cdt_mesh::edge_t(v[0], v[1]));
	long poly2_2dind = poly2->add_point(trim2_mid_2d, f2_ind2d);
	struct cdt_mesh::edge_t poly2_edge1(v[0], poly2_2dind);
	poly2_ne1 = poly2->add_ordered_edge(poly2_edge1);
	poly2_ne1->trim_ind = trim_ind;
	poly2_ne1->trim_start = old_trim_start;
	poly2_ne1->trim_end = t2mid;
	struct cdt_mesh::edge_t poly2_edge2(poly2_2dind, v[1]);
	poly2_ne2 = poly2->add_ordered_edge(poly2_edge2);
	poly2_ne2->trim_ind = trim_ind;
	poly2_ne2->trim_start = t2mid;
	poly2_ne2->trim_end = old_trim_end;
    }

    // The new trim segments are then associated with the new bounding edge
    // segments.
    // NOTE: the m_bRev3d logic below is CRITICALLY important when it comes to
    // associating the correct portion of the edge curve with the correct part
    // of the polygon in parametric space.  If this is NOT correct, the 3D
    // polycurves manifested by the 2D polygon will be self intersecting, as
    // will the 3D triangles generated from the 2D CDT.
    bseg1->tseg1 = (trim1->m_bRev3d) ? poly1_ne2 : poly1_ne1;
    bseg1->tseg2 = (trim2->m_bRev3d) ? poly2_ne2 : poly2_ne1;
    bseg2->tseg1 = (trim1->m_bRev3d) ? poly1_ne1 : poly1_ne2;
    bseg2->tseg2 = (trim2->m_bRev3d) ? poly2_ne1 : poly2_ne2;

    // Associated the trim segments with the edge segment they actually
    // wound up assigned to
    bseg1->tseg1->eseg = bseg1;
    bseg1->tseg2->eseg = bseg1;
    bseg2->tseg1->eseg = bseg2;
    bseg2->tseg2->eseg = bseg2;

    nedges.insert(bseg1);
    nedges.insert(bseg2);

    // Update the rtrees with the new segments
    if (update_rtrees) {
	rtree_bbox_2d(s_cdt, bseg1->tseg1, 0);
	rtree_bbox_2d(s_cdt, bseg1->tseg2, 0);
	rtree_bbox_2d(s_cdt, bseg2->tseg1, 0);
	rtree_bbox_2d(s_cdt, bseg2->tseg2, 0);
#if 0
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	int face_index = s_cdt->brep->m_T[bseg1->tseg1->trim_ind].Face()->m_face_index;
	bu_vls_sprintf(&fname, "%d-rtree_2d_split_update.plot3", face_index);
	plot_rtree_2d2(s_cdt->face_rtrees_2d[face_index], bu_vls_cstr(&fname));
	face_index = s_cdt->brep->m_T[bseg2->tseg1->trim_ind].Face()->m_face_index;
	bu_vls_sprintf(&fname, "%d-rtree_2d_split_update.plot3", face_index);
	plot_rtree_2d2(s_cdt->face_rtrees_2d[face_index], bu_vls_cstr(&fname));
	bu_vls_free(&fname);
#endif
    }

    delete bseg;
    return nedges;
}

#if 0
// Calculate loop avg segment length
static double
loop_avg_seg_len(struct ON_Brep_CDT_State *s_cdt, int loop_index)
{
    const ON_BrepLoop &loop = s_cdt->brep->m_L[loop_index];
    std::vector<double> lsegs;
    for (int lti = 0; lti < loop.TrimCount(); lti++) {
	ON_BrepTrim *trim = loop.Trim(lti);
	ON_BrepEdge *edge = trim->Edge();
	if (!edge) continue;
	const ON_Curve* crv = edge->EdgeCurveOf();
	if (!crv) continue;
	std::vector<cdt_mesh::bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge->m_edge_index];
	if (!epsegs.size()) continue;
	std::vector<cdt_mesh::bedge_seg_t *>::iterator e_it;
	for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
	    cdt_mesh::bedge_seg_t *b = *e_it;
	    double seg_dist = b->e_start->DistanceTo(*b->e_end);
	    lsegs.push_back(seg_dist);
	}
    }
    return (std::accumulate(lsegs.begin(), lsegs.end(), 0.0)/lsegs.size());
}

static void
split_long_edges(struct ON_Brep_CDT_State *s_cdt, int face_index)
{
    ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    int loop_cnt = face.LoopCount();

    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	double avg_seg_len = loop_avg_seg_len(s_cdt, loop->m_loop_index);
	int trim_count = loop->TrimCount();
	for (int lti = 0; lti < trim_count; lti++) {
	    ON_BrepTrim *trim = loop->Trim(lti);
	    ON_BrepEdge *edge = trim->Edge();
	    if (!edge) continue;
	    const ON_Curve* crv = edge->EdgeCurveOf();
	    if (!crv || crv->IsLinear(BN_TOL_DIST)) {
		continue;
	    }
	    std::vector<cdt_mesh::bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge->m_edge_index];
	    if (!epsegs.size()) continue;
	    std::vector<cdt_mesh::bedge_seg_t *>::iterator e_it;
	    std::set<cdt_mesh::bedge_seg_t *> new_segs;
	    std::set<cdt_mesh::bedge_seg_t *> ws1, ws2;
	    std::set<cdt_mesh::bedge_seg_t *> *ws = &ws1;
	    std::set<cdt_mesh::bedge_seg_t *> *ns = &ws2;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		cdt_mesh::bedge_seg_t *b = *e_it;
		ws->insert(b);
	    }
	    while (ws->size()) {
		cdt_mesh::bedge_seg_t *b = *ws->begin();
		ws->erase(ws->begin());
		double seg_dist = b->e_start->DistanceTo(*b->e_end);
		if (seg_dist > 0.5*avg_seg_len) {
		    std::set<cdt_mesh::bedge_seg_t *> esegs_split = split_edge_seg(s_cdt, b, 1);
		    if (esegs_split.size()) {
			ns->insert(esegs_split.begin(), esegs_split.end());
		    } else {
			new_segs.insert(b);
		    }
		} else {
		    new_segs.insert(b);
		}
		if (!ws->size() && ns->size()) {
		    std::set<cdt_mesh::bedge_seg_t *> *tmp = ws;
		    ws = ns;
		    ns = tmp;
		}
	    }
	    s_cdt->e2polysegs[edge->m_edge_index].clear();
	    s_cdt->e2polysegs[edge->m_edge_index].insert(new_segs.begin(), new_segs.end());
	}
    }
}
#endif

std::set<cdt_mesh::cpolyedge_t *>
split_singular_seg(struct ON_Brep_CDT_State *s_cdt, cdt_mesh::cpolyedge_t *ce, int update_rtrees)
{
    std::set<cdt_mesh::cpolyedge_t *> nedges;
    cdt_mesh::cpolygon_t *poly = ce->polygon;
    int trim_ind = ce->trim_ind;

    ON_BrepTrim& trim = s_cdt->brep->m_T[ce->trim_ind];
    double tcparam = (ce->trim_start + ce->trim_end) / 2.0;
    ON_3dPoint trim_mid_2d_ev = trim.PointAt(tcparam);
    ON_2dPoint trim_mid_2d(trim_mid_2d_ev.x, trim_mid_2d_ev.y);

    ON_BrepFace *face = trim.Face();
    cdt_mesh::cdt_mesh_t *fmesh = &s_cdt->fmeshes[face->m_face_index];
    long f_ind2d = fmesh->add_point(trim_mid_2d);

    // Singularity - new 2D point points to the same 3D point as both of the existing
    // vertices 
    fmesh->p2d3d[f_ind2d] = fmesh->p2d3d[poly->p2o[ce->v[0]]];

    if (update_rtrees) {
	rtree_bbox_2d_remove(s_cdt, ce);
    }

    // Using the 2d mid points, update the polygons associated with tseg1 and tseg2.
    cdt_mesh::cpolyedge_t *poly_ne1, *poly_ne2;
    int v[2];
    v[0] = ce->v[0];
    v[1] = ce->v[1];
    double old_trim_start = ce->trim_start;
    double old_trim_end = ce->trim_end;
    poly->remove_edge(cdt_mesh::edge_t(v[0], v[1]));
    long poly_2dind = poly->add_point(trim_mid_2d, f_ind2d);
    struct cdt_mesh::edge_t poly_edge1(v[0], poly_2dind);
    poly_ne1 = poly->add_edge(poly_edge1);
    poly_ne1->trim_ind = trim_ind;
    poly_ne1->trim_start = old_trim_start;
    poly_ne1->trim_end = tcparam;
    poly_ne1->eseg = NULL;
    struct cdt_mesh::edge_t poly_edge2(poly_2dind, v[1]);
    poly_ne2 = poly->add_edge(poly_edge2);
    poly_ne2->trim_ind = trim_ind;
    poly_ne2->trim_start = tcparam;
    poly_ne2->trim_end = old_trim_end;
    poly_ne2->eseg = NULL;

    nedges.insert(poly_ne1);
    nedges.insert(poly_ne2);

    if (update_rtrees) {
	rtree_bbox_2d(s_cdt, poly_ne1, 0);
	rtree_bbox_2d(s_cdt, poly_ne2, 0);
    }

    return nedges;
}


// There are a couple of edge splitting operations that have to happen in the
// beginning regardless of tolerance settings.  Do them up front so the subsequent
// working set has consistent properties.
bool
initialize_edge_segs(struct ON_Brep_CDT_State *s_cdt)
{
    std::map<int, std::vector<cdt_mesh::bedge_seg_t *>>::iterator epoly_it;
    for (epoly_it = s_cdt->e2polysegs.begin(); epoly_it != s_cdt->e2polysegs.end(); epoly_it++) {
	std::vector<cdt_mesh::bedge_seg_t *>::iterator seg_it;
	std::vector<cdt_mesh::bedge_seg_t *> wsegs = epoly_it->second;
	for (seg_it = wsegs.begin(); seg_it != wsegs.end(); seg_it++) {
	    cdt_mesh::bedge_seg_t *e = *seg_it;


	    ON_BrepEdge& edge = s_cdt->brep->m_E[e->edge_ind];
	    ON_BrepTrim *trim1 = edge.Trim(0);
	    ON_BrepTrim *trim2 = edge.Trim(1);
	    std::set<cdt_mesh::bedge_seg_t *> esegs_closed;

	    if (!trim1 || !trim2) return false;

	    if (trim1->m_type == ON_BrepTrim::singular || trim1->m_type == ON_BrepTrim::singular) return false;

	    // 1.  Any edges with at least 1 closed trim are split.
	    if (trim1->IsClosed() || trim2->IsClosed()) {
		esegs_closed = split_edge_seg(s_cdt, e, 1, 1);
		if (!esegs_closed.size()) {
		    // split failed??  On a closed edge this is fatal - we must split it
		    // to work with it at all
		    return false;
		}
	    } else {
		esegs_closed.insert(e);
	    }

	    // 2.  Any edges with a non-linear edge curve are split.
	    std::set<cdt_mesh::bedge_seg_t *> esegs_csplit;
	    const ON_Curve* crv = edge.EdgeCurveOf();
	    if (!crv->IsLinear(BN_TOL_DIST)) {
		std::set<cdt_mesh::bedge_seg_t *>::iterator e_it;
		for (e_it = esegs_closed.begin(); e_it != esegs_closed.end(); e_it++) {
		    std::set<cdt_mesh::bedge_seg_t *> efirst = split_edge_seg(s_cdt, *e_it, 1, 1);
		    if (!efirst.size()) {
			// split failed??  On a curved edge we must split at least once to
			// avoid potentially degenerate polygons (if we had to split a closed
			// loop from step 1, for example;
			return false;
		    } else {
			// To avoid representing circles with squares, split curved segments
			// one additional time
			std::set<cdt_mesh::bedge_seg_t *>::iterator s_it;
			for (s_it = efirst.begin(); s_it != efirst.end(); s_it++) {
			    std::set<cdt_mesh::bedge_seg_t *> etmp = split_edge_seg(s_cdt, *s_it, 1, 1);
			    if (!etmp.size()) {
				// split failed??  This isn't good and shouldn't
				// happen, but it's not fatal the way the previous two
				// failure cases are...
				esegs_csplit.insert(*s_it);
			    } else {
				esegs_csplit.insert(etmp.begin(), etmp.end());
			    }
			}
		    }
		}
	    } else {
		esegs_csplit = esegs_closed;
	    }

	    s_cdt->e2polysegs[edge.m_edge_index].clear();
	    std::copy(esegs_csplit.begin(), esegs_csplit.end(), std::back_inserter(s_cdt->e2polysegs[edge.m_edge_index]));
	}
    }

#if 0
    for (int face_index = 0; face_index < s_cdt->brep->m_F.Count(); face_index++) {
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "%d-rtree_2d_after_initial_splits.plot3", face_index);
	plot_rtree_2d2(s_cdt->face_rtrees_2d[face_index], bu_vls_cstr(&fname));
	bu_vls_free(&fname);
    }
#endif

    return true;
}

// Charcterize the edges.  Five possibilities:
//
// 0.  Singularity
// 1.  Curved edge
// 2.  Linear edge, associated with at least 1 non-planar surface
// 3.  Linear edge, associated with planar surfaces but sharing one or more vertices with
//     curved edges.
// 4.  Linear edge, associated only with planar faces and linear edges.
std::vector<int>
characterize_edges(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;

    // Characterize the vertices - are they used by non-linear edges?
    std::vector<int> vert_type;
    for (int i = 0; i < brep->m_V.Count(); i++) {
	int has_curved_edge = 0;
	for (int j = 0; j < brep->m_V[i].m_ei.Count(); j++) {
	    ON_BrepEdge &edge = brep->m_E[brep->m_V[i].m_ei[j]];
	    const ON_Curve* crv = edge.EdgeCurveOf();
	    if (crv && !crv->IsLinear(BN_TOL_DIST)) {
		has_curved_edge = 1;
		break;
	    }
	}
	vert_type.push_back(has_curved_edge);
    }

    std::vector<int> edge_type;
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	const ON_Curve* crv = edge.EdgeCurveOf();

	// Singularity
	if (!crv) {
	    edge_type.push_back(0);
	    continue;
	}

	// Curved edge
	if (!crv->IsLinear(BN_TOL_DIST)) {
	    edge_type.push_back(1);
	    continue;
	}

	// Linear edge, at least one non-planar surface
	const ON_Surface *s1= edge.Trim(0)->SurfaceOf();
	const ON_Surface *s2= edge.Trim(1)->SurfaceOf();
	if (!s1->IsPlanar(NULL, BN_TOL_DIST) || !s2->IsPlanar(NULL, BN_TOL_DIST)) {
	    edge_type.push_back(2);
	    continue;
	}

	// Linear edge, at least one associated non-linear edge
	if (vert_type[edge.Vertex(0)->m_vertex_index] || vert_type[edge.Vertex(1)->m_vertex_index]) {
	    edge_type.push_back(3);
	    continue;
	}

	// Linear edge, only associated with linear edges and planar faces
	edge_type.push_back(4);
    }
    return edge_type;
}

// Set up the edge containers that will manage the edge subdivision.  Loop
// ordering is not the job of these containers - that's handled by the trim loop
// polygons.  These containers maintain the association between trims in different
// faces and the 3D edge curve information used to drive shared points.
void
initialize_edge_containers(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;

    // Charcterize the edges.
    std::vector<int> edge_type = characterize_edges(s_cdt);

    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	cdt_mesh::bedge_seg_t *bseg = new cdt_mesh::bedge_seg_t;
	bseg->edge_ind = edge.m_edge_index;
	bseg->brep = s_cdt->brep;

	// Provide a normalize edge NURBS curve
	const ON_Curve* crv = edge.EdgeCurveOf();
	bseg->nc = crv->NurbsCurve();
	bseg->cp_len = bseg->nc->ControlPolygonLength();
	bseg->nc->SetDomain(0.0, bseg->cp_len);

	// Set the initial edge curve t parameter values
	bseg->edge_start = 0.0;
	bseg->edge_end = bseg->cp_len;

	// Get the trims and normalize their domains as well.
	// NOTE - another point where this won't work if we don't have a 1->2 edge to trims relationship
	ON_BrepTrim *trim1 = edge.Trim(0);
	ON_BrepTrim *trim2 = edge.Trim(1);
	s_cdt->brep->m_T[trim1->TrimCurveIndexOf()].SetDomain(bseg->edge_start, bseg->edge_end);
	s_cdt->brep->m_T[trim2->TrimCurveIndexOf()].SetDomain(bseg->edge_start, bseg->edge_end);

	// The 3D start and endpoints will be vertex points (they are shared with other edges).
	bseg->e_start = (*s_cdt->vert_pnts)[edge.Vertex(0)->m_vertex_index];
	bseg->e_end = (*s_cdt->vert_pnts)[edge.Vertex(1)->m_vertex_index];

	// These are also the root start and end points - type 3 edges will need this information later
	bseg->e_root_start = bseg->e_start;
	bseg->e_root_end = bseg->e_end;

	// Stash the edge type - we will need it during refinement
	bseg->edge_type = edge_type[edge.m_edge_index];

	s_cdt->e2polysegs[edge.m_edge_index].push_back(bseg);
    }
}

// For each face and each loop in each face define the initial
// loop polygons.  Note there is no splitting of edges at this point -
// we are simply establishing the initial closed polygons.
bool
initialize_loop_polygons(struct ON_Brep_CDT_State *s_cdt, std::set<cdt_mesh::cpolyedge_t *> *singular_edges)
{
    ON_Brep* brep = s_cdt->brep;
    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	int loop_cnt = face.LoopCount();
	cdt_mesh::cdt_mesh_t *fmesh = &s_cdt->fmeshes[face_index];
	fmesh->f_id = face_index;
	fmesh->m_bRev = face.m_bRev;
	fmesh->has_singularities = false;
	cdt_mesh::cpolygon_t *cpoly = NULL;

	for (int li = 0; li < loop_cnt; li++) {
	    const ON_BrepLoop *loop = face.Loop(li);
	    bool is_outer = (face.OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	    if (is_outer) {
		cpoly = &fmesh->outer_loop;
	    } else {
		cpoly = new cdt_mesh::cpolygon_t;
		fmesh->inner_loops[li] = cpoly;
	    }
	    int trim_count = loop->TrimCount();

	    ON_2dPoint cp(0,0);

	    long cv = -1;
	    long pv = -1;
	    long fv = -1;

	    for (int lti = 0; lti < trim_count; lti++) {
		ON_BrepTrim *trim = loop->Trim(lti);
		ON_Interval range = trim->Domain();
		if (lti == 0) {
		    // Get the 2D point, add it to the mesh and current polygon
		    cp = trim->PointAt(range.m_t[0]);
		    long find = fmesh->add_point(cp);
		    pv = cpoly->add_point(cp, find);
		    fv = pv;

		    // Let cdt_mesh know about new 3D information
		    ON_3dPoint *op3d = (*s_cdt->vert_pnts)[trim->Vertex(0)->m_vertex_index];
		    ON_3dVector norm = ON_3dVector::UnsetVector;
		    if (trim->m_type != ON_BrepTrim::singular) {
			// 3D points are globally unique, but normals are not - the same edge point may
			// have different normals from two faces at a sharp edge.  Calculate the
			// face normal for this point on this surface.
			norm = calc_trim_vnorm(*trim->Vertex(0), trim);
			//std::cout << "Face " << face.m_face_index << ", Loop " << loop->m_loop_index << ", Vert " << trim->Vertex(0)->m_vertex_index << " norm: " << norm.x << "," << norm.y << "," << norm.z << "\n";
		    } else {
			// Surface sampling will need some information about singularities
			s_cdt->strim_pnts[face_index][trim->m_trim_index] = op3d;
			ON_3dPoint *sn3d = (*s_cdt->vert_avg_norms)[trim->Vertex(0)->m_vertex_index];
			if (sn3d) {
			    s_cdt->strim_norms[face_index][trim->m_trim_index] = sn3d;
			}
		    }
		    long f3ind = fmesh->add_point(op3d);
		    long fnind = fmesh->add_normal(new ON_3dPoint(norm));
		    fmesh->p2d3d[find] = f3ind;
		    fmesh->nmap[f3ind] = fnind;

		} else {
		    pv = cv;
		}

		// Get the 2D point, add it to the mesh and current polygon
		cp = trim->PointAt(range.m_t[1]);
		if (lti == trim_count - 1) {
		    cv = fv;
		} else {
		    long find;
		    find = fmesh->add_point(cp);
		    cv = cpoly->add_point(cp, find);

		    // Let cdt_mesh know about the 3D information
		    ON_3dPoint *cp3d = (*s_cdt->vert_pnts)[trim->Vertex(1)->m_vertex_index];
		    ON_3dVector norm = ON_3dVector::UnsetVector;
		    if (trim->m_type != ON_BrepTrim::singular) {
			// 3D points are globally unique, but normals are not - the same edge point may
			// have different normals from two faces at a sharp edge.  Calculate the
			// face normal for this point on this surface.
			norm = calc_trim_vnorm(*trim->Vertex(1), trim);
			//std::cout << "Face " << face.m_face_index << ", Loop " << loop->m_loop_index << ", Vert " << trim->Vertex(1)->m_vertex_index << " norm: " << norm.x << "," << norm.y << "," << norm.z << "\n";
		    } else {
			// Surface sampling will need some information about singularities
			s_cdt->strim_pnts[face_index][trim->m_trim_index] = cp3d;
			ON_3dPoint *sn3d = (*s_cdt->vert_avg_norms)[trim->Vertex(1)->m_vertex_index];
			if (sn3d) {
			    s_cdt->strim_norms[face_index][trim->m_trim_index] = sn3d;
			}
		    }

		    long f3ind = fmesh->add_point(cp3d);
		    long fnind = fmesh->add_normal(new ON_3dPoint(norm));
		    fmesh->p2d3d[find] = f3ind;
		    fmesh->nmap[f3ind] = fnind;
		}

		struct cdt_mesh::edge_t lseg(pv, cv);
		cdt_mesh::cpolyedge_t *ne = cpoly->add_ordered_edge(lseg);

		ne->trim_ind = trim->m_trim_index;
		ne->loop_type = (is_outer) ? 1 : 2;
		ne->trim_start = range.m_t[0];
		ne->trim_end = range.m_t[1];

		rtree_bbox_2d(s_cdt, ne, 0);

		if (trim->m_ei >= 0) {
		    cdt_mesh::bedge_seg_t *eseg = s_cdt->e2polysegs[trim->m_ei][0];
		    // Associate the edge segment with the trim segment and vice versa
		    ne->eseg = eseg;
		    if (eseg->tseg1 && eseg->tseg2) {
			bu_log("error - more than two trims associated with an edge\n");
			return false;
		    }
		    if (eseg->tseg1) {
			eseg->tseg2 = ne;
		    } else {
			eseg->tseg1 = ne;
		    }
		} else {
		    // A null eseg will indicate a singularity and a need for special case
		    // splitting of the 2D edge only
		    ne->eseg = NULL;
		    singular_edges->insert(ne);
		    fmesh->has_singularities = true;
		}
	    }
	}

#if 0
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "%d-rtree_2d_initial.plot3", face_index);
	plot_rtree_2d2(s_cdt->face_rtrees_2d[face_index], bu_vls_cstr(&fname));
	bu_vls_free(&fname);
#endif
    }
    return true;
}

// Split curved edges per tolerance settings
void
tol_curved_edges_split(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	const ON_Curve* crv = edge.EdgeCurveOf();
	// TODO - BN_TOL_DIST will be too large for very small trims - need to do
	// something similar to the ptol calculation for these edge curves...
	if (crv && !crv->IsLinear(BN_TOL_DIST)) {
	    std::vector<cdt_mesh::bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge.m_edge_index];
	    std::vector<cdt_mesh::bedge_seg_t *>::iterator e_it;
	    std::set<cdt_mesh::bedge_seg_t *> new_segs;
	    std::set<cdt_mesh::bedge_seg_t *> ws1, ws2;
	    std::set<cdt_mesh::bedge_seg_t *> *ws = &ws1;
	    std::set<cdt_mesh::bedge_seg_t *> *ns = &ws2;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		cdt_mesh::bedge_seg_t *b = *e_it;
		ws->insert(b);
	    }
	    while (ws->size()) {
		cdt_mesh::bedge_seg_t *b = *ws->begin();
		ws->erase(ws->begin());
		std::set<cdt_mesh::bedge_seg_t *> esegs_split = split_edge_seg(s_cdt, b, 0, 0);
		if (esegs_split.size()) {
		    ns->insert(esegs_split.begin(), esegs_split.end());
		} else {
		    new_segs.insert(b);
		}
		if (!ws->size() && ns->size()) {
		    std::set<cdt_mesh::bedge_seg_t *> *tmp = ws;
		    ws = ns;
		    ns = tmp;
		}
	    }
	    s_cdt->e2polysegs[edge.m_edge_index].clear();
	    std::copy(new_segs.begin(), new_segs.end(), std::back_inserter(s_cdt->e2polysegs[edge.m_edge_index]));
	}
    }
}

// Calculate for each vertex involved with curved edges the minimum individual bedge_seg
// length involved.
void
update_vert_edge_seg_lengths(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;
    for (int i = 0; i < brep->m_V.Count(); i++) {
	ON_3dPoint *p3d = (*s_cdt->vert_pnts)[i];
	double emin = DBL_MAX;
	for (int j = 0; j < brep->m_V[i].m_ei.Count(); j++) {
	    ON_BrepEdge &edge = brep->m_E[brep->m_V[i].m_ei[j]];
	    std::vector<cdt_mesh::bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge.m_edge_index];
	    std::vector<cdt_mesh::bedge_seg_t *>::iterator e_it;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		cdt_mesh::bedge_seg_t *b = *e_it;
		if (b->e_start == p3d || b->e_end == p3d) {
		    ON_Line line3d(*(b->e_start), *(b->e_end));
		    double seg_len = line3d.Length();
		    if (seg_len < emin) {
			emin = seg_len;
		    }
		}
	    }
	}
	s_cdt->v_min_seg_len[p3d] = emin;
	//std::cout << "Minimum vert seg length, vert " << i << ": " << s_cdt->v_min_seg_len[p3d] << "\n";
    }
}

// Calculate loop median segment lengths contributed from the curved edges
void
update_loop_median_curved_edge_seg_lengths(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;
    for (int index = 0; index < brep->m_L.Count(); index++) {
	const ON_BrepLoop &loop = brep->m_L[index];
	std::vector<double> lsegs;
	for (int lti = 0; lti < loop.TrimCount(); lti++) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    ON_BrepEdge *edge = trim->Edge();
	    if (!edge) continue;
	    const ON_Curve* crv = edge->EdgeCurveOf();
	    if (!crv || crv->IsLinear(BN_TOL_DIST)) {
		continue;
	    }
	    std::vector<cdt_mesh::bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge->m_edge_index];
	    if (!epsegs.size()) continue;
	    std::vector<cdt_mesh::bedge_seg_t *>::iterator e_it;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		cdt_mesh::bedge_seg_t *b = *e_it;
		double seg_dist = b->e_start->DistanceTo(*b->e_end);
		lsegs.push_back(seg_dist);
	    }
	}
	if (!lsegs.size()) {
	    // No non-linear edges, so no segments to use
	    s_cdt->l_median_len[index] = -1;
	} else {
	    s_cdt->l_median_len[index] = median_seg_len(lsegs);
	    //std::cout << "Median loop seg length, loop " << index << ": " << s_cdt->l_median_len[index] << "\n";
	}
    }
}

// After the initial curve split, make another pass looking for curved
// edges sharing a vertex.  We want larger curves to refine close to the
// median segment length of the smaller ones, since this situation can be a
// sign that the surface will generate small triangles near large ones.
void curved_edges_refine(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;

    // Calculate for each vertex involved with curved edges the minimum individual bedge_seg
    // length involved.
    update_vert_edge_seg_lengths(s_cdt);


    // Calculate loop median segment lengths contributed from the curved edges
    update_loop_median_curved_edge_seg_lengths(s_cdt);

    std::map<int, double> refine_targets;
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	const ON_Curve* crv = edge.EdgeCurveOf();
	if (!crv || crv->IsLinear(BN_TOL_DIST)) continue;
	bool refine = false;
	double target_len = DBL_MAX;
	double lmed = edge_median_seg_len(s_cdt, edge.m_edge_index);
	for (int i = 0; i < 2; i++) {
	    int vert_ind = edge.Vertex(i)->m_vertex_index;
	    for (int j = 0; j < brep->m_V[vert_ind].m_ei.Count(); j++) {
		ON_BrepEdge &e2= brep->m_E[brep->m_V[vert_ind].m_ei[j]];
		const ON_Curve* crv2 = e2.EdgeCurveOf();
		if (crv2 && !crv2->IsLinear(BN_TOL_DIST)) {
		    double emed = edge_median_seg_len(s_cdt, e2.m_edge_index);
		    if (emed < lmed) {
			target_len = (2*emed < target_len) ? 2*emed : target_len;
			refine = true;
		    }
		}
	    }
	}
	if (refine) {
	    refine_targets[index] = target_len;
	}
    }
    std::map<int, double>::iterator r_it;
    for (r_it = refine_targets.begin(); r_it != refine_targets.end(); r_it++) {
	ON_BrepEdge& edge = brep->m_E[r_it->first];
	double split_tol = r_it->second;
	std::vector<cdt_mesh::bedge_seg_t *> &epsegs = s_cdt->e2polysegs[r_it->first];
	std::vector<cdt_mesh::bedge_seg_t *>::iterator e_it;
	std::set<cdt_mesh::bedge_seg_t *> new_segs;
	std::set<cdt_mesh::bedge_seg_t *> ws1, ws2;
	std::set<cdt_mesh::bedge_seg_t *> *ws = &ws1;
	std::set<cdt_mesh::bedge_seg_t *> *ns = &ws2;
	for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
	    cdt_mesh::bedge_seg_t *b = *e_it;
	    ws->insert(b);
	}
	while (ws->size()) {
	    cdt_mesh::bedge_seg_t *b = *ws->begin();
	    ws->erase(ws->begin());
	    bool split_edge = (b->e_start->DistanceTo(*b->e_end) > split_tol);
	    if (split_edge) {
		// If we need to split, do so
		std::set<cdt_mesh::bedge_seg_t *> esegs_split = split_edge_seg(s_cdt, b, 1, 0);
		if (esegs_split.size()) {
		    ws->insert(esegs_split.begin(), esegs_split.end());
		} else {
		    new_segs.insert(b);
		}
	    } else {
		new_segs.insert(b);
	    }
	    if (!ws->size() && ns->size()) {
		std::set<cdt_mesh::bedge_seg_t *> *tmp = ws;
		ws = ns;
		ns = tmp;
	    }
	}
	s_cdt->e2polysegs[edge.m_edge_index].clear();
	std::copy(new_segs.begin(), new_segs.end(), std::back_inserter(s_cdt->e2polysegs[edge.m_edge_index]));
    }

}

// Split linear edges according to tolerance information
void
tol_linear_edges_split(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;

    // Calculate loop median segment lengths contributed from the curved edges
    update_loop_median_curved_edge_seg_lengths(s_cdt);

    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	const ON_Curve* crv = edge.EdgeCurveOf();
	if (crv && crv->IsLinear(BN_TOL_DIST)) {
	    std::vector<cdt_mesh::bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge.m_edge_index];
	    std::vector<cdt_mesh::bedge_seg_t *>::iterator e_it;
	    std::set<cdt_mesh::bedge_seg_t *> new_segs;
	    std::set<cdt_mesh::bedge_seg_t *> ws1, ws2;
	    std::set<cdt_mesh::bedge_seg_t *> *ws = &ws1;
	    std::set<cdt_mesh::bedge_seg_t *> *ns = &ws2;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		cdt_mesh::bedge_seg_t *b = *e_it;
		ws->insert(b);
	    }
	    while (ws->size()) {
		cdt_mesh::bedge_seg_t *b = *ws->begin();
		ws->erase(ws->begin());
		std::set<cdt_mesh::bedge_seg_t *> esegs_split = split_edge_seg(s_cdt, b, 0, 0);
		if (esegs_split.size()) {
		    ns->insert(esegs_split.begin(), esegs_split.end());
		} else {
		    new_segs.insert(b);
		}
		if (!ws->size() && ns->size()) {
		    std::set<cdt_mesh::bedge_seg_t *> *tmp = ws;
		    ws = ns;
		    ns = tmp;
		}
	    }
	    s_cdt->e2polysegs[edge.m_edge_index].clear();
	    std::copy(new_segs.begin(), new_segs.end(), std::back_inserter(s_cdt->e2polysegs[edge.m_edge_index]));
	}
    }

}

void
refine_close_edges(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;

    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	std::cout << "Face " << face_index << " close edge check...\n";

	std::vector<cdt_mesh::cpolyedge_t *> ws = cdt_face_polyedges(s_cdt, face_index);

#if 0
	{
	    struct bu_vls fname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&fname, "%d-rtree_2d_split_update_0.plot3", face.m_face_index);
	    plot_rtree_2d2(s_cdt->face_rtrees_2d[face_index], bu_vls_cstr(&fname));
	    bu_vls_free(&fname);
	}
#endif


	// Check all the edge segments associated with the loop to see if our bounding box overlaps with boxes
	// that aren't our neighbor boxes.  For any that do, split and check again.  Keep refining until we
	// don't have any non-neighbor overlaps.
	int split_cnt = 0;
	while (ws.size() && split_cnt < 10) {
	    std::vector<cdt_mesh::cpolyedge_t *> current_trims;
	    std::map<int, std::vector<cdt_mesh::bedge_seg_t *>> curr_edge_segs;

	    bool split_check = false;

#if 1
	    if (split_cnt) {
		std::cout << "Face " << face_index << " split_cnt " << split_cnt << "\n";
	    }
#endif
	    // TODO - with the status determination being recorded in the cpolyedge_t structure
	    // itself, this loop should be (in principle) suitable for bu_parallel - we're not
	    // doing any splitting at this point - searching is a read only activity once
	    // the initial data containers are set up
	    std::vector<cdt_mesh::cpolyedge_t *>::iterator w_it;
	    for (w_it = ws.begin(); w_it != ws.end(); w_it++) {
		cdt_mesh::cpolyedge_t *tseg = *w_it;
		ON_2dPoint p2d1(tseg->polygon->pnts_2d[tseg->v[0]].first, tseg->polygon->pnts_2d[tseg->v[0]].second);
		ON_2dPoint p2d2(tseg->polygon->pnts_2d[tseg->v[1]].first, tseg->polygon->pnts_2d[tseg->v[1]].second);

		// Trim 2D bbox
		ON_Line line(p2d1, p2d2);
		ON_BoundingBox bb = line.BoundingBox();
		bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
		bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
		bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
		bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
		double dist = p2d1.DistanceTo(p2d2);
		double bdist = 0.5*dist;
		double xdist = bb.m_max.x - bb.m_min.x;
		double ydist = bb.m_max.y - bb.m_min.y;
		if (xdist < bdist) {
		    bb.m_min.x = bb.m_min.x - 0.51*bdist;
		    bb.m_max.x = bb.m_max.x + 0.51*bdist;
		}
		if (ydist < bdist) {
		    bb.m_min.y = bb.m_min.y - 0.51*bdist;
		    bb.m_max.y = bb.m_max.y + 0.51*bdist;
		}

		double tMin[2];
		tMin[0] = bb.Min().x;
		tMin[1] = bb.Min().y;
		double tMax[2];
		tMax[0] = bb.Max().x;
		tMax[1] = bb.Max().y;

		plot_ce_bbox(s_cdt, tseg, "c.p3");

		// Edge context info
		struct rtree_minsplit_context a_context;
		a_context.s_cdt = s_cdt;
		a_context.cseg = tseg;

		// Do the search
		s_cdt->face_rtrees_2d[face.m_face_index].Search(tMin, tMax, MinSplit2dCallback, (void *)&a_context);
	    }

	    // If we need to split, do so.  We need to process as a set,
	    // because an edge split on a closed face may end up removing more
	    // than one cpolyedge_t in ws at the same time.
	    std::set<cdt_mesh::cpolyedge_t *> ws_s(ws.begin(), ws.end());
	    while (ws_s.size()) {
		cdt_mesh::cpolyedge_t *pe = *ws_s.begin();
		if (pe->eseg) {
		    cdt_mesh::bedge_seg_t *b = pe->eseg;
		    // Get both of them in case they're both in ws (closed face)
		    ws_s.erase(b->tseg1);
		    ws_s.erase(b->tseg2);
		    int edge_ind = b->edge_ind;
		    if (pe->split_status == 2) {
			std::set<cdt_mesh::bedge_seg_t *> esegs_split = split_edge_seg(s_cdt, b, 1, 1);
			if (esegs_split.size()) {
			    split_check = true;
			    std::copy(esegs_split.begin(), esegs_split.end(), std::back_inserter(curr_edge_segs[edge_ind]));
			    // Pick up the new trim segments from the edges for the next iteration.  Only
			    // want the ones associated with the current face.
			    std::set<cdt_mesh::bedge_seg_t *>::iterator b_it;
			    for (b_it = esegs_split.begin(); b_it != esegs_split.end(); b_it++) {
				cdt_mesh::bedge_seg_t *bn = *b_it;
				cdt_mesh::cpolyedge_t *ce = (s_cdt->brep->m_T[bn->tseg1->trim_ind].Face()->m_face_index == face_index) ? bn->tseg1 : bn->tseg2;
				current_trims.push_back(ce);
			    }
			} else {
			    // This is probably fatal...
			    std::cerr << "Forced edge split failed???\n";
			    curr_edge_segs[edge_ind].push_back(b);
			    current_trims.push_back(pe);
			}
		    } else if (pe->split_status == 1) {
			curr_edge_segs[edge_ind].push_back(b);
			current_trims.push_back(pe);
		    } else {
			curr_edge_segs[edge_ind].push_back(b);
		    }
		} else {
		    // Trim only, no edge.
		    ws_s.erase(pe);
		    if (pe->split_status == 2) {
			std::set<cdt_mesh::cpolyedge_t *> ntrims = split_singular_seg(s_cdt, pe, 1);
			if (ntrims.size()) {
			    std::copy(ntrims.begin(), ntrims.end(), std::back_inserter(current_trims));
			    split_check = true;
			} else {
			    // This is probably fatal...
			    std::cerr << "Forced trim split failed???\n";
			    current_trims.push_back(pe);
			}
		    } else if (pe->split_status == 1) {
			current_trims.push_back(pe);
		    }
		}
	    }

	    ws.clear();

	    split_cnt++;

	    if (split_check) {
		ws = current_trims;
		for (w_it = ws.begin(); w_it != ws.end(); w_it++) {
		    // We don't want to zero this status information if this is
		    // our last iteration before bailing and we've still got
		    // unresolved inputs - we will want to know about any edges
		    // that are still overlapping with non-neighbors when doing
		    // surface points.
		    if (split_cnt < 10) {
			(*w_it)->split_status = 0;
		    }
		}
	    }

	    // Once we're done with this round of splitting, update the e2polysegs sets
	    std::map<int, std::vector<cdt_mesh::bedge_seg_t *>>::iterator m_it;
	    for (m_it = curr_edge_segs.begin(); m_it != curr_edge_segs.end(); m_it++) {
		int m_edge_index = m_it->first;
		s_cdt->e2polysegs[m_edge_index].clear();
		std::copy(m_it->second.begin(), m_it->second.end(), std::back_inserter(s_cdt->e2polysegs[m_edge_index]));
	    }

#if 0
	    struct bu_vls fname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&fname, "%d-rtree_2d_split_update_%d.plot3", face.m_face_index, split_cnt);
	    plot_rtree_2d2(s_cdt->face_rtrees_2d[face_index], bu_vls_cstr(&fname));
	    bu_vls_free(&fname);
#endif
	}

    }
}

void
finalize_rtrees(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;
    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	s_cdt->face_rtrees_2d[face.m_face_index].RemoveAll();
	cdt_mesh::cdt_mesh_t *fmesh = &s_cdt->fmeshes[face.m_face_index];

	std::vector<cdt_mesh::cpolyedge_t *> ws;
	std::vector<cdt_mesh::cpolyedge_t *>::iterator w_it;

	int loop_cnt = face.LoopCount();
	for (int li = 0; li < loop_cnt; li++) {
	    const ON_BrepLoop *loop = face.Loop(li);
	    bool is_outer = (face.OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	    cdt_mesh::cpolygon_t *cpoly = NULL;
	    if (is_outer) {
		cpoly = &fmesh->outer_loop;
	    } else {
		cpoly = fmesh->inner_loops[li];
	    }

	    size_t ecnt = 1;
	    cdt_mesh::cpolyedge_t *pe = (*cpoly->poly.begin());
	    cdt_mesh::cpolyedge_t *first = pe;
	    cdt_mesh::cpolyedge_t *next = pe->next;
	    rtree_bbox_2d(s_cdt, first, 1);
	    // Walk the loop
	    while (first != next) {
		ecnt++;
		if (!next) break;
		rtree_bbox_2d(s_cdt, next, 1);
		next = next->next;
		if (ecnt > cpoly->poly.size()) {
		    std::cerr << "\nfinalize_2d_rtrees: ERROR! encountered infinite loop\n";
		    return;
		}
	    }
	}
#if 0
	    struct bu_vls fname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&fname, "%d-rtree_2d_final.plot3", face.m_face_index);
	    plot_rtree_2d2(s_cdt->face_rtrees_2d[face_index], bu_vls_cstr(&fname));
	    bu_vls_free(&fname);
#endif


    }

    for (int index = 0; index < brep->m_E.Count(); index++) {
	std::vector<cdt_mesh::bedge_seg_t *> &epsegs = s_cdt->e2polysegs[index];
	std::vector<cdt_mesh::bedge_seg_t *>::iterator e_it;
	for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
	    cdt_mesh::bedge_seg_t *b = *e_it;
	    rtree_bbox_3d(s_cdt, b->tseg1);
	    rtree_bbox_3d(s_cdt, b->tseg2);
	}
    }
}

void
cpolyedge_ndists(cdt_mesh::cpolyedge_t *pe) {
    ON_2dPoint p2d1(pe->polygon->pnts_2d[pe->v[0]].first, pe->polygon->pnts_2d[pe->v[0]].second);
    ON_2dPoint p2d2(pe->polygon->pnts_2d[pe->v[1]].first, pe->polygon->pnts_2d[pe->v[1]].second);

    ON_2dPoint p2d1_n(pe->polygon->pnts_2d[pe->prev->v[0]].first, pe->polygon->pnts_2d[pe->prev->v[0]].second);
    ON_2dPoint p2d2_n(pe->polygon->pnts_2d[pe->next->v[1]].first, pe->polygon->pnts_2d[pe->next->v[1]].second);

    pe->v1_dist = p2d1.DistanceTo(p2d1_n);
    pe->v2_dist = p2d2.DistanceTo(p2d2_n);
}

void
cpolyedge_nearest_dists(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;
    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	cdt_mesh::cdt_mesh_t *fmesh = &s_cdt->fmeshes[face.m_face_index];

	std::vector<cdt_mesh::cpolyedge_t *> ws;
	std::vector<cdt_mesh::cpolyedge_t *>::iterator w_it;

	int loop_cnt = face.LoopCount();
	for (int li = 0; li < loop_cnt; li++) {
	    const ON_BrepLoop *loop = face.Loop(li);
	    bool is_outer = (face.OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	    cdt_mesh::cpolygon_t *cpoly = NULL;
	    if (is_outer) {
		cpoly = &fmesh->outer_loop;
	    } else {
		cpoly = fmesh->inner_loops[li];
	    }

	    size_t ecnt = 1;
	    cdt_mesh::cpolyedge_t *pe = (*cpoly->poly.begin());
	    cdt_mesh::cpolyedge_t *first = pe;
	    cdt_mesh::cpolyedge_t *next = pe->next;

	    cpolyedge_ndists(first);
	    
	    // Walk the loop
	    while (first != next) {
		ecnt++;
		if (!next) break;
		cpolyedge_ndists(next);
		next = next->next;
		if (ecnt > cpoly->poly.size()) {
		    std::cerr << "\ncpolyedge_nearest_dists: ERROR! encountered infinite loop\n";
		    return;
		}
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

