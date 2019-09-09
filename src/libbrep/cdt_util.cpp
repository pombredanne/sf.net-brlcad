/*                        C D T _ U T I L . C P P
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
 */

#include "common.h"
#include "./cdt.h"

/***************************************************
 * debugging routines
 ***************************************************/

#define BB_PLOT_2D(min, max) {         \
    fastf_t pt[4][3];                  \
    VSET(pt[0], max[X], min[Y], 0);    \
    VSET(pt[1], max[X], max[Y], 0);    \
    VSET(pt[2], min[X], max[Y], 0);    \
    VSET(pt[3], min[X], min[Y], 0);    \
    pdv_3move(plot_file, pt[0]); \
    pdv_3cont(plot_file, pt[1]); \
    pdv_3cont(plot_file, pt[2]); \
    pdv_3cont(plot_file, pt[3]); \
    pdv_3cont(plot_file, pt[0]); \
}

#define TREE_LEAF_FACE_3D(valp, a, b, c, d)  \
    pdv_3move(plot_file, pt[a]); \
    pdv_3cont(plot_file, pt[b]); \
    pdv_3cont(plot_file, pt[c]); \
    pdv_3cont(plot_file, pt[d]); \
    pdv_3cont(plot_file, pt[a]); \

#define BB_PLOT(min, max) {                 \
    fastf_t pt[8][3];                       \
    VSET(pt[0], max[X], min[Y], min[Z]);    \
    VSET(pt[1], max[X], max[Y], min[Z]);    \
    VSET(pt[2], max[X], max[Y], max[Z]);    \
    VSET(pt[3], max[X], min[Y], max[Z]);    \
    VSET(pt[4], min[X], min[Y], min[Z]);    \
    VSET(pt[5], min[X], max[Y], min[Z]);    \
    VSET(pt[6], min[X], max[Y], max[Z]);    \
    VSET(pt[7], min[X], min[Y], max[Z]);    \
    TREE_LEAF_FACE_3D(pt, 0, 1, 2, 3);      \
    TREE_LEAF_FACE_3D(pt, 4, 0, 3, 7);      \
    TREE_LEAF_FACE_3D(pt, 5, 4, 7, 6);      \
    TREE_LEAF_FACE_3D(pt, 1, 5, 6, 2);      \
}

void
plot_rtree_2d(ON_RTree *rtree, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    ON_RTreeIterator rit(*rtree);
    const ON_RTreeBranch *rtree_leaf;
    for (rit.First(); 0 != (rtree_leaf = rit.Value()); rit.Next())
    {
	BB_PLOT_2D(rtree_leaf->m_rect.m_min, rtree_leaf->m_rect.m_max);
    }

    fclose(plot_file);
}

void
plot_rtree_2d2(RTree<void *, double, 2> &rtree, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    RTree<void *, double, 2>::Iterator tree_it;
    rtree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	double m_min[2];
	double m_max[2];
	tree_it.GetBounds(m_min, m_max);
	BB_PLOT_2D(m_min, m_max);
	++tree_it;
    }

    rtree.Save("test.rtree");

    fclose(plot_file);
}
void
plot_rtree_3d(RTree<void *, double, 3> &rtree, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);


    RTree<void *, double, 3>::Iterator tree_it;
    rtree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	double m_min[3];
	double m_max[3];
	tree_it.GetBounds(m_min, m_max);
	BB_PLOT(m_min, m_max);
	++tree_it;
    }

    fclose(plot_file);
}

void
plot_bbox(point_t m_min, point_t m_max, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    BB_PLOT(m_min, m_max);

    fclose(plot_file);
}

void
plot_on_bbox(ON_BoundingBox &bb, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    BB_PLOT(bb.m_min, bb.m_max);

    fclose(plot_file);
}

void
plot_ce_bbox(struct ON_Brep_CDT_State *s_cdt, cdt_mesh::cpolyedge_t *pe, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    ON_2dPoint p2d1 = trim.PointAt(pe->trim_start);
    ON_2dPoint p2d2 = trim.PointAt(pe->trim_end);
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

    point_t m_min, m_max;
    m_min[0] = bb.Min().x;
    m_min[1] = bb.Min().y;
    m_min[2] = 0;
    m_max[0] = bb.Max().x;
    m_max[1] = bb.Max().y;
    m_max[2] = 0;

    BB_PLOT(m_min, m_max);

    fclose(plot_file);
}


struct cdt_audit_info *
cdt_ainfo(int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d, fastf_t px, fastf_t py, fastf_t pz)
{
    struct cdt_audit_info *a = new struct cdt_audit_info;
    a->face_index = fid;
    a->vert_index = vid;
    a->trim_index = tid;
    a->edge_index = eid;
    a->surf_uv = ON_2dPoint(x2d, y2d);
    a->vert_pnt = ON_3dPoint(px, py, pz);
    return a;
}

void
CDT_Add3DPnt(struct ON_Brep_CDT_State *s, ON_3dPoint *p, int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d)
{
    s->w3dpnts->push_back(p);
    (*s->pnt_audit_info)[p] = cdt_ainfo(fid, vid, tid, eid, x2d, y2d, 0.0, 0.0, 0.0);
}

void
CDT_Add3DNorm(struct ON_Brep_CDT_State *s, ON_3dPoint *normal, ON_3dPoint *vert, int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d)
{
    s->w3dnorms->push_back(normal);
    (*s->pnt_audit_info)[normal] = cdt_ainfo(fid, vid, tid, eid, x2d, y2d, vert->x, vert->y, vert->z);
}

// Digest tessellation tolerances...
void
CDT_Tol_Set(struct brep_cdt_tol *cdt, double dist, fastf_t md, double t_abs, double t_rel, double t_dist)
{
    fastf_t max_dist = md;
    fastf_t min_dist = (t_abs < t_dist + ON_ZERO_TOLERANCE) ? t_dist : t_abs;
    fastf_t within_dist = 0.01 * dist; // default to 1% of dist

    if (t_rel > 0.0 + ON_ZERO_TOLERANCE) {
	double rel = t_rel * dist;
	// TODO - think about what we want maximum distance to mean and how it
	// relates to other tessellation settings...
	if (max_dist < rel * 10.0) {
	    max_dist = rel * 10.0;
	}
	within_dist = rel < min_dist ? min_dist : rel;
    } else {
	if (t_abs > 0.0 + ON_ZERO_TOLERANCE) {
	    within_dist = min_dist;
	}
    }

    cdt->min_dist = min_dist;
    cdt->max_dist = max_dist;
    cdt->within_dist = within_dist;
}

struct ON_Brep_CDT_State *
ON_Brep_CDT_Create(void *bv, const char *objname)
{
    struct ON_Brep_CDT_State *cdt = new struct ON_Brep_CDT_State;

    cdt->brep_root = BU_VLS_INIT_ZERO;
    if (objname) {
	bu_vls_sprintf(&cdt->brep_root, "%s_", objname);
    } else {
	bu_vls_trunc(&cdt->brep_root, 0);
    }

    /* Set status to "never evaluated" */
    cdt->status = BREP_CDT_UNTESSELLATED;

    ON_Brep *brep = (ON_Brep *)bv;
    cdt->orig_brep = brep;
    cdt->brep = NULL;

    /* By default, all tolerances are unset */
    cdt->tol.abs = -1;
    cdt->tol.rel = -1;
    cdt->tol.norm = -1;
    cdt->tol.absmax = -1;
    cdt->tol.absmin = -1;
    cdt->tol.relmax = -1;
    cdt->tol.relmin = -1;
    cdt->tol.rel_lmax = -1;
    cdt->tol.rel_lmin = -1;

    cdt->w3dpnts = new std::vector<ON_3dPoint *>;
    cdt->w3dnorms = new std::vector<ON_3dPoint *>;

    cdt->vert_pnts = new std::map<int, ON_3dPoint *>;
    cdt->vert_avg_norms = new std::map<int, ON_3dPoint *>;
    cdt->singular_vert_to_norms = new std::map<ON_3dPoint *, ON_3dPoint *>;

    cdt->edge_pnts = new std::set<ON_3dPoint *>;
    cdt->min_edge_seg_len = new std::map<int, double>;
    cdt->max_edge_seg_len = new std::map<int, double>;
    cdt->on_brep_edge_pnts = new std::map<ON_3dPoint *, std::set<BrepTrimPoint *>>;
    cdt->etrees = new std::map<int, struct BrepEdgeSegment *>;

    cdt->pnt_audit_info = new std::map<ON_3dPoint *, struct cdt_audit_info *>;

    cdt->faces = new std::map<int, struct ON_Brep_CDT_Face_State *>;

    cdt->tri_brep_face = new std::map<p2t::Triangle*, int>;
    cdt->bot_pnt_to_on_pnt = new std::map<int, ON_3dPoint *>;

    return cdt;
}


void
ON_Brep_CDT_Destroy(struct ON_Brep_CDT_State *s_cdt)
{
    for (size_t i = 0; i < s_cdt->w3dpnts->size(); i++) {
	delete (*(s_cdt->w3dpnts))[i];
    }
    for (size_t i = 0; i < s_cdt->w3dnorms->size(); i++) {
	delete (*(s_cdt->w3dnorms))[i];
    }

    if (s_cdt->brep) {
	delete s_cdt->brep;
    }

    delete s_cdt->vert_pnts;
    delete s_cdt->vert_avg_norms;
    delete s_cdt->singular_vert_to_norms;

    delete s_cdt->edge_pnts;
    delete s_cdt->min_edge_seg_len;
    delete s_cdt->max_edge_seg_len;
    delete s_cdt->on_brep_edge_pnts;

    // TODO - free segment trees in etrees
    delete s_cdt->etrees;

    delete s_cdt->pnt_audit_info;
    delete s_cdt->faces;

    delete s_cdt->tri_brep_face;
    delete s_cdt->bot_pnt_to_on_pnt;

    delete s_cdt;
}

int
ON_Brep_CDT_Status(struct ON_Brep_CDT_State *s_cdt)
{
    return s_cdt->status;
}


void *
ON_Brep_CDT_Brep(struct ON_Brep_CDT_State *s_cdt)
{
    return (void *)s_cdt->brep;
}

// Rules of precedence:
//
// 1.  absmax >= absmin
// 2.  absolute over relative
// 3.  relative local over relative global 
// 4.  normal (curvature)
void
cdt_tol_global_calc(struct ON_Brep_CDT_State *s)
{
    if (s->tol.abs < ON_ZERO_TOLERANCE && s->tol.rel < ON_ZERO_TOLERANCE && s->tol.norm < ON_ZERO_TOLERANCE &&
	    s->tol.absmax < ON_ZERO_TOLERANCE && s->tol.absmin < ON_ZERO_TOLERANCE && s->tol.relmax < ON_ZERO_TOLERANCE &&
	    s->tol.relmin < ON_ZERO_TOLERANCE && s->tol.rel_lmax < ON_ZERO_TOLERANCE && s->tol.rel_lmin < ON_ZERO_TOLERANCE) {
    }

    if (s->tol.abs > ON_ZERO_TOLERANCE) {
	s->absmax = s->tol.abs;
    }

    if (s->tol.absmax > ON_ZERO_TOLERANCE && s->tol.abs < ON_ZERO_TOLERANCE) {
	s->absmax = s->tol.absmax;
    }

    if (s->tol.absmin > ON_ZERO_TOLERANCE) {
	s->absmin = s->tol.absmin;
    }

    // If we don't have hard set limits globally, see if we have global relative
    // settings
    if (s->absmax < ON_ZERO_TOLERANCE || s->absmin < ON_ZERO_TOLERANCE) {

	/* If we've got nothing, set a default global relative tolerance */
	if (s->tol.rel < ON_ZERO_TOLERANCE && s->tol.relmax < ON_ZERO_TOLERANCE &&
		s->tol.relmin < ON_ZERO_TOLERANCE && s->tol.rel_lmax < ON_ZERO_TOLERANCE && s->tol.rel_lmin < ON_ZERO_TOLERANCE) {
	    s->tol.rel = BREP_CDT_DEFAULT_TOL_REL ;
	}

	// Need some bounding box information
	ON_BoundingBox bbox;
	if (!s->brep->GetTightBoundingBox(bbox)) {
	   bbox = s->brep->BoundingBox();
	}
	double len = bbox.Diagonal().Length();

	if (s->tol.rel > ON_ZERO_TOLERANCE) {
	    // Largest dimension, unless set by abs, is tol.rel * the bbox diagonal length
	    if ( s->tol.relmax < ON_ZERO_TOLERANCE) {
		s->absmax = len * s->tol.rel;
	    }

	    // Smallest dimension, unless otherwise set, is  0.01 * tol.rel * the bbox diagonal length.
	    if (s->tol.relmin < ON_ZERO_TOLERANCE) {
		s->absmin = len * s->tol.rel * 0.01;
	    }
	}

	if (s->absmax < ON_ZERO_TOLERANCE && s->tol.relmax > ON_ZERO_TOLERANCE) {
	    s->absmax = len * s->tol.relmax;
	}

	if (s->absmin < ON_ZERO_TOLERANCE && s->tol.relmin > ON_ZERO_TOLERANCE) {
	    s->absmin = len * s->tol.relmin;
	}

    }

    // Sanity
    s->absmax = (s->absmin > s->absmax) ? s->absmin : s->absmax;
    s->absmin = (s->absmax < s->absmin) ? s->absmax : s->absmin;

    // Get the normal based parameter as well
    if (s->tol.norm > ON_ZERO_TOLERANCE) {
	s->cos_within_ang = cos(s->tol.norm);
    } else {
	s->cos_within_ang = -1.0;
    }
}

void
ON_Brep_CDT_Tol_Set(struct ON_Brep_CDT_State *s, const struct bg_tess_tol *t)
{
    if (!s) {
	return;
    }

    if (!t) {
	/* reset to defaults */
	s->tol.abs = -1;
	s->tol.rel = -1;
	s->tol.norm = -1;
	s->tol.absmax = -1;
	s->tol.absmin = -1;
	s->tol.relmax = -1;
	s->tol.relmin = -1;
	s->tol.rel_lmax = -1;
	s->tol.rel_lmin = -1;
	s->absmax = -1;
	s->absmin = -1;
	s->cos_within_ang = -1;
	if (s->brep) {
	    cdt_tol_global_calc(s);
	}
	return;
    }

    s->tol = *t;
    s->absmax = -1;
    s->absmin = -1;
    s->cos_within_ang = -1;
    if (s->brep) {
	cdt_tol_global_calc(s);
    }
}

void
ON_Brep_CDT_Tol_Get(struct bg_tess_tol *t, const struct ON_Brep_CDT_State *s)
{
    if (!t) {
	return;
    }

    if (!s) {
	/* set to defaults */
	t->abs = -1;
	t->rel = -1;
	t->norm = -1;
    	t->absmax = -1;
	t->absmin = -1;
	t->relmax = -1;
	t->relmin = -1;
	t->rel_lmax = -1;
	t->rel_lmin = -1;
    }

    *t = s->tol;
}

static int
ON_Brep_CDT_VList_Face(
	struct bu_list *vhead,
	struct bu_list *vlfree,
	int face_index,
	int mode,
	struct ON_Brep_CDT_State *s)
{
    point_t pt[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    vect_t nv[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
#if 0
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;
#endif

    cdt_mesh::cdt_mesh_t *fmesh = &(s->fmeshes[face_index]);
    std::set<cdt_mesh::triangle_t>::iterator tr_it;
    std::set<cdt_mesh::triangle_t> tris = fmesh->tris;

    switch (mode) {
	case 0:
	    // 3D shaded triangles
	    for (tr_it = tris.begin(); tr_it != tris.end(); tr_it++) {
		for (size_t j = 0; j < 3; j++) {
		    ON_3dPoint *p3d = fmesh->pnts[(*tr_it).v[j]];
		    ON_3dPoint onorm;
		    if (s->singular_vert_to_norms->find(p3d) != s->singular_vert_to_norms->end()) {
			// Use calculated normal for singularity points
			onorm = *(*s->singular_vert_to_norms)[p3d];
		    } else {
			onorm = *fmesh->normals[fmesh->nmap[(*tr_it).v[j]]];
		    }
		    VSET(pt[j], p3d->x, p3d->y, p3d->z);
		    VSET(nv[j], onorm.x, onorm.y, onorm.z);
		}
		//tri one
		BN_ADD_VLIST(vlfree, vhead, nv[0], BN_VLIST_TRI_START);
		BN_ADD_VLIST(vlfree, vhead, nv[0], BN_VLIST_TRI_VERTNORM);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_TRI_MOVE);
		BN_ADD_VLIST(vlfree, vhead, nv[1], BN_VLIST_TRI_VERTNORM);
		BN_ADD_VLIST(vlfree, vhead, pt[1], BN_VLIST_TRI_DRAW);
		BN_ADD_VLIST(vlfree, vhead, nv[2], BN_VLIST_TRI_VERTNORM);
		BN_ADD_VLIST(vlfree, vhead, pt[2], BN_VLIST_TRI_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_TRI_END);
		//bu_log("Face %d, Tri %zd: %f/%f/%f-%f/%f/%f -> %f/%f/%f-%f/%f/%f -> %f/%f/%f-%f/%f/%f\n", face_index, i, V3ARGS(pt[0]), V3ARGS(nv[0]), V3ARGS(pt[1]), V3ARGS(nv[1]), V3ARGS(pt[2]), V3ARGS(nv[2]));
	    }
	    break;
	case 1:
	    // 3D wireframe
	    for (tr_it = tris.begin(); tr_it != tris.end(); tr_it++) {
		for (size_t j = 0; j < 3; j++) {
		    ON_3dPoint *p3d = fmesh->pnts[(*tr_it).v[j]];
		    VSET(pt[j], p3d->x, p3d->y, p3d->z);
		}
		//tri one
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_LINE_MOVE);
		BN_ADD_VLIST(vlfree, vhead, pt[1], BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[2], BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_LINE_DRAW);
	    }
	    break;
	case 2:
#if 0
	    // 2D wireframe
	    for (tr_it = tris.begin(); tr_it != tris.end(); tr_it++) {
		p2t::Triangle *t = *tr_it;
		p2t::Point *p = NULL;

		for (size_t j = 0; j < 3; j++) {
		    if (j == 0) {
			p = t->GetPoint(2);
		    } else {
			p = t->GetPoint(j - 1);
		    }
		    pt1[0] = p->x;
		    pt1[1] = p->y;
		    pt1[2] = 0.0;
		    p = t->GetPoint(j);
		    pt2[0] = p->x;
		    pt2[1] = p->y;
		    pt2[2] = 0.0;
		    BN_ADD_VLIST(vlfree, vhead, pt1, BN_VLIST_LINE_MOVE);
		    BN_ADD_VLIST(vlfree, vhead, pt2, BN_VLIST_LINE_DRAW);
		}
	    }
#endif
	    break;
	default:
	    return -1;
    }

    return 0;
}

int ON_Brep_CDT_VList(
	struct bn_vlblock *vbp,
	struct bu_list *vlfree,
	struct bu_color *c,
	int mode,
	struct ON_Brep_CDT_State *s)
{
    int r, g, b;
    struct bu_list *vhead = NULL;
    int have_color = 0;

   if (UNLIKELY(!c) || mode < 0) {
       return -1;
   }

   have_color = bu_color_to_rgb_ints(c, &r, &g, &b);

   if (UNLIKELY(!have_color)) {
       return -1;
   }

   vhead = bn_vlblock_find(vbp, r, g, b);

   if (UNLIKELY(!vhead)) {
       return -1;
   }

   for (int i = 0; i < s->brep->m_F.Count(); i++) {
       if (s->fmeshes[i].tris.size()) {
	   (void)ON_Brep_CDT_VList_Face(vhead, vlfree, i, mode, s);
       }
   }

   return 0;
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

