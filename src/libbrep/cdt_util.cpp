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

void
plot_polyline(std::vector<p2t::Point *> *pnts, const char *filename)
{
    FILE *plot = fopen(filename, "w");
    int r = 255; int g = 0; int b = 0;
    point_t pc;
    pl_color(plot, r, g, b); 
    for (size_t i = 0; i < pnts->size(); i++) {
	p2t::Point *pt = (*pnts)[i];
	VSET(pc, pt->x, pt->y, 0);
	if (i == 0) {
	    pdv_3move(plot, pc);
	}
	pdv_3cont(plot, pc);
    }
    fclose(plot);
}


void
plot_tri(p2t::Triangle *t, const char *filename)
{
    FILE *plot = fopen(filename, "w");
    int r = 0; int g = 255; int b = 0;
    point_t pc;
    pl_color(plot, r, g, b); 
    for (size_t i = 0; i < 3; i++) {
	p2t::Point *pt = t->GetPoint(i);
	VSET(pc, pt->x, pt->y, 0);
	if (i == 0) {
	    pdv_3move(plot, pc);
	}
	pdv_3cont(plot, pc);
    }
    fclose(plot);
}

static void
plot_tri_3d(p2t::Triangle *t, std::map<p2t::Point *, ON_3dPoint *> *pointmap, int r, int g, int b, FILE *c_plot)
{
    point_t p1, p2, p3;
    ON_3dPoint *on1 = (*pointmap)[t->GetPoint(0)];
    ON_3dPoint *on2 = (*pointmap)[t->GetPoint(1)];
    ON_3dPoint *on3 = (*pointmap)[t->GetPoint(2)];
    p1[X] = on1->x;
    p1[Y] = on1->y;
    p1[Z] = on1->z;
    p2[X] = on2->x;
    p2[Y] = on2->y;
    p2[Z] = on2->z;
    p3[X] = on3->x;
    p3[Y] = on3->y;
    p3[Z] = on3->z;
    pl_color(c_plot, r, g, b);
    pdv_3move(c_plot, p1);
    pdv_3cont(c_plot, p2);
    pdv_3move(c_plot, p1);
    pdv_3cont(c_plot, p3);
    pdv_3move(c_plot, p2);
    pdv_3cont(c_plot, p3);
}


static void
plot_trimesh_tris_3d(std::set<size_t> *faces, std::vector<trimesh::triangle_t> &farray, std::map<p2t::Point *, ON_3dPoint *> *pointmap, const char *filename)
{
    std::set<size_t>::iterator f_it;
    FILE* plot_file = fopen(filename, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    for (f_it = faces->begin(); f_it != faces->end(); f_it++) {
	p2t::Triangle *t = farray[*f_it].t;
	plot_tri_3d(t, pointmap, r, g ,b, plot_file);
    }
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
CDT_Add2DPnt(struct ON_Brep_CDT_Face_State *f, ON_2dPoint *p, int fid, int vid, int tid, int eid, fastf_t tparam)
{
    f->w2dpnts->push_back(p);
    (*f->pnt2d_audit_info)[p] = cdt_ainfo(fid, vid, tid, eid, tid, tparam, 0.0, 0.0, 0.0);
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
CDT_Tol_Set(struct brep_cdt_tol *cdt, double dist, fastf_t md, double t_abs, double t_rel, double t_norm, double t_dist)
{
    fastf_t min_dist, max_dist, within_dist, cos_within_ang;

    max_dist = md;

    if (t_abs < t_dist + ON_ZERO_TOLERANCE) {
	min_dist = t_dist;
    } else {
	min_dist = t_abs;
    }

    double rel = 0.0;
    if (t_rel > 0.0 + ON_ZERO_TOLERANCE) {
	rel = t_rel * dist;
	// TODO - think about what we want maximum distance to mean and how it
	// relates to other tessellation settings...
	if (max_dist < rel * 10.0) {
	    max_dist = rel * 10.0;
	}
	within_dist = rel < min_dist ? min_dist : rel;
    } else if (t_abs > 0.0 + ON_ZERO_TOLERANCE) {
	within_dist = min_dist;
    } else {
	within_dist = 0.01 * dist; // default to 1% of dist
    }

    if (t_norm > 0.0 + ON_ZERO_TOLERANCE) {
	cos_within_ang = cos(t_norm);
    } else {
	cos_within_ang = cos(ON_PI / 2.0);
    }

    cdt->min_dist = min_dist;
    cdt->max_dist = max_dist;
    cdt->within_dist = within_dist;
    cdt->cos_within_ang = cos_within_ang;
}

ON_3dVector
p2tTri_Normal(p2t::Triangle *t, std::map<p2t::Point *, ON_3dPoint *> *pointmap)
{
    ON_3dPoint *p1 = (*pointmap)[t->GetPoint(0)];
    ON_3dPoint *p2 = (*pointmap)[t->GetPoint(1)];
    ON_3dPoint *p3 = (*pointmap)[t->GetPoint(2)];

    ON_3dVector e1 = *p2 - *p1;
    ON_3dVector e2 = *p3 - *p1;
    ON_3dVector tdir = ON_CrossProduct(e1, e2);
    tdir.Unitize();
    return tdir;
}

struct ON_Brep_CDT_Face_State *
ON_Brep_CDT_Face_Create(struct ON_Brep_CDT_State *s_cdt, int ind)
{
    struct ON_Brep_CDT_Face_State *fcdt = new struct ON_Brep_CDT_Face_State;

    fcdt->s_cdt = s_cdt;
    fcdt->ind = ind;
    fcdt->has_singular_trims = 0;

    fcdt->w2dpnts = new std::vector<ON_2dPoint *>;
    fcdt->w3dpnts = new std::vector<ON_3dPoint *>;
    fcdt->w3dnorms = new std::vector<ON_3dPoint *>;

    fcdt->face_loop_points = NULL;
    fcdt->p2t_to_trimpt = new std::map<p2t::Point *, BrepTrimPoint *>;
    fcdt->p2t_trim_ind = new std::map<p2t::Point *, int>;
    fcdt->rt_trims = new ON_RTree;
    fcdt->on_surf_points = new std::set<ON_2dPoint *>;

    fcdt->strim_pnts = new std::map<int,ON_3dPoint *>;
    fcdt->strim_norms = new std::map<int,ON_3dPoint *>;

    fcdt->on2_to_on3_map = new std::map<ON_2dPoint *, ON_3dPoint *>;
    fcdt->on2_to_p2t_map = new std::map<ON_2dPoint *, p2t::Point *>;
    fcdt->p2t_to_on2_map = new std::map<p2t::Point *, ON_2dPoint *>;
    fcdt->p2t_to_on3_map = new std::map<p2t::Point *, ON_3dPoint *>;
    fcdt->p2t_to_on3_norm_map = new std::map<p2t::Point *, ON_3dPoint *>;
    fcdt->on3_to_tri_map = new std::map<ON_3dPoint *, std::set<p2t::Point *>>;

    fcdt->cdt = NULL;
    fcdt->p2t_extra_faces = new std::vector<p2t::Triangle *>;

    fcdt->degen_pnts = new std::set<p2t::Point *>;

    /* Mesh data */
    fcdt->tris_degen = new std::set<p2t::Triangle*>;
    fcdt->tris_zero_3D_area = new std::set<p2t::Triangle*>;
    fcdt->e2f = new EdgeToTri;
    fcdt->ecnt = new std::map<Edge, int>;

    /* Only create halfedge data if we need it */
    fcdt->he_edges = NULL;
    fcdt->he_mesh = NULL;

    return fcdt;
}

/* Clears old triangulation data */
void
ON_Brep_CDT_Face_Reset(struct ON_Brep_CDT_Face_State *fcdt, int full_surface_sample)
{
    fcdt->p2t_to_trimpt->clear();
    fcdt->p2t_trim_ind->clear();

    if (full_surface_sample) {
	fcdt->on_surf_points->clear();

	// I think this is cleanup code for the tree?
	ON_SimpleArray<void*> results;
	ON_BoundingBox bb = fcdt->rt_trims->BoundingBox();
	fcdt->rt_trims->Search2d((const double *) bb.m_min, (const double *) bb.m_max, results);
	if (results.Count() > 0) {
	    for (int ri = 0; ri < results.Count(); ri++) {
		const ON_Line *l = (const ON_Line *)*results.At(ri);
		delete l;
	    }
	}
	fcdt->rt_trims->RemoveAll();
	delete fcdt->rt_trims;
	fcdt->rt_trims = new ON_RTree;
    }

    fcdt->on2_to_p2t_map->clear();
    fcdt->p2t_to_on2_map->clear();
    fcdt->p2t_to_on3_map->clear();
    fcdt->p2t_to_on3_norm_map->clear();
    fcdt->on3_to_tri_map->clear();

    /* Mesh data */
    if (fcdt->cdt) {
	delete fcdt->cdt;
	fcdt->cdt = NULL;
    }
    std::vector<p2t::Triangle *>::iterator trit;
    for (trit = fcdt->p2t_extra_faces->begin(); trit != fcdt->p2t_extra_faces->end(); trit++) {
	p2t::Triangle *t = *trit;
	delete t;
    }
    fcdt->p2t_extra_faces->clear();
    fcdt->tris_degen->clear();
    fcdt->tris_zero_3D_area->clear();
    fcdt->e2f->clear();
    fcdt->ecnt->clear();
}

void
ON_Brep_CDT_Face_Destroy(struct ON_Brep_CDT_Face_State *fcdt)
{
    for (size_t i = 0; i < fcdt->w2dpnts->size(); i++) {
	delete (*(fcdt->w2dpnts))[i];
    }
    for (size_t i = 0; i < fcdt->w3dpnts->size(); i++) {
	delete (*(fcdt->w3dpnts))[i];
    }
    for (size_t i = 0; i < fcdt->w3dnorms->size(); i++) {
	delete (*(fcdt->w3dnorms))[i];
    }

    std::vector<p2t::Triangle *>::iterator trit;
    for (trit = fcdt->p2t_extra_faces->begin(); trit != fcdt->p2t_extra_faces->end(); trit++) {
	p2t::Triangle *t = *trit;
	delete t;
    }

    delete fcdt->w2dpnts;
    delete fcdt->w3dpnts;
    delete fcdt->w3dnorms;
    if (fcdt->face_loop_points) {
	delete fcdt->face_loop_points;
    }
    delete fcdt->p2t_to_trimpt;

    // I think this is cleanup code for the tree?
    ON_SimpleArray<void*> results;
    ON_BoundingBox bb = fcdt->rt_trims->BoundingBox();
    fcdt->rt_trims->Search2d((const double *) bb.m_min, (const double *) bb.m_max, results);
    if (results.Count() > 0) {
	for (int ri = 0; ri < results.Count(); ri++) {
	    const ON_Line *l = (const ON_Line *)*results.At(ri);
	    delete l;
	}
    }
    fcdt->rt_trims->RemoveAll();

    delete fcdt->rt_trims;

    delete fcdt->on_surf_points;
    delete fcdt->strim_pnts;
    delete fcdt->strim_norms;
    delete fcdt->on_surf_points;
    delete fcdt->on2_to_on3_map;
    delete fcdt->on2_to_p2t_map;
    delete fcdt->p2t_to_on2_map;
    delete fcdt->p2t_to_on3_map;
    delete fcdt->p2t_to_on3_norm_map;
    delete fcdt->on3_to_tri_map;
    delete fcdt->p2t_extra_faces;
    delete fcdt->degen_pnts;

    delete fcdt->tris_degen;
    delete fcdt->tris_zero_3D_area;
    delete fcdt->e2f;
    delete fcdt->ecnt;

    delete fcdt;
}


struct ON_Brep_CDT_State *
ON_Brep_CDT_Create(void *bv)
{
    struct ON_Brep_CDT_State *cdt = new struct ON_Brep_CDT_State;
 
    /* Set status to "never evaluated" */
    cdt->status = BREP_CDT_UNTESSELLATED;

    ON_Brep *brep = (ON_Brep *)bv;
    cdt->orig_brep = brep;
    cdt->brep = NULL;

    /* Set sane default tolerances.  May want to do
     * something better (perhaps brep dimension based...) */
    cdt->abs = BREP_CDT_DEFAULT_TOL_ABS;
    cdt->rel = BREP_CDT_DEFAULT_TOL_REL ;
    cdt->norm = BREP_CDT_DEFAULT_TOL_NORM ;
    cdt->dist = BREP_CDT_DEFAULT_TOL_DIST ;


    cdt->w3dpnts = new std::vector<ON_3dPoint *>;
    cdt->w3dnorms = new std::vector<ON_3dPoint *>;

    cdt->vert_pnts = new std::map<int, ON_3dPoint *>;
    cdt->vert_avg_norms = new std::map<int, ON_3dPoint *>;
    cdt->singular_vert_to_norms = new std::map<ON_3dPoint *, ON_3dPoint *>;

    cdt->edge_pnts = new std::set<ON_3dPoint *>;
    cdt->min_edge_seg_len = new std::map<int, double>;
    cdt->max_edge_seg_len = new std::map<int, double>;
    cdt->on_brep_edge_pnts = new std::map<ON_3dPoint *, std::set<BrepTrimPoint *>>;

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

    if (s_cdt->faces) {
	std::map<int, struct ON_Brep_CDT_Face_State *>::iterator f_it;
	for (f_it = s_cdt->faces->begin(); f_it != s_cdt->faces->end(); f_it++) {
	    struct ON_Brep_CDT_Face_State *f = f_it->second;
	    delete f;
	}
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

void
ON_Brep_CDT_Tol_Set(struct ON_Brep_CDT_State *s, const struct ON_Brep_CDT_Tols *t)
{
    if (!s) {
	return;
    }

    if (!t) {
	/* reset to defaults */
	s->abs = BREP_CDT_DEFAULT_TOL_ABS;
	s->rel = BREP_CDT_DEFAULT_TOL_REL ;
	s->norm = BREP_CDT_DEFAULT_TOL_NORM ;
	s->dist = BREP_CDT_DEFAULT_TOL_DIST ;
    }

    s->abs = t->abs;
    s->rel = t->rel;
    s->norm = t->norm;
    s->dist = t->dist;
}

void
ON_Brep_CDT_Tol_Get(struct ON_Brep_CDT_Tols *t, const struct ON_Brep_CDT_State *s)
{
    if (!t) {
	return;
    }

    if (!s) {
	/* set to defaults */
	t->abs = BREP_CDT_DEFAULT_TOL_ABS;
	t->rel = BREP_CDT_DEFAULT_TOL_REL ;
	t->norm = BREP_CDT_DEFAULT_TOL_NORM ;
	t->dist = BREP_CDT_DEFAULT_TOL_DIST ;
    }

    t->abs = s->abs;
    t->rel = s->rel;
    t->norm = s->norm;
    t->dist = s->dist;
}

static int
ON_Brep_CDT_VList_Face(
	struct bu_list *vhead,
	struct bu_list *vlfree,
	int face_index,
	int mode,
	const struct ON_Brep_CDT_State *s)
{
    point_t pt[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    vect_t nv[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;

    p2t::CDT *cdt = (*s->faces)[face_index]->cdt;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = (*s->faces)[face_index]->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = (*s->faces)[face_index]->p2t_to_on3_norm_map;
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();

    switch (mode) {
	case 0:
	    // 3D shaded triangles
	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;
		for (size_t j = 0; j < 3; j++) {
		    p = t->GetPoint(j);
		    ON_3dPoint *op = (*pointmap)[p];
		    ON_3dPoint *onorm = (*normalmap)[p];
		    VSET(pt[j], op->x, op->y, op->z);
		    VSET(nv[j], onorm->x, onorm->y, onorm->z);
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
	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;
		for (size_t j = 0; j < 3; j++) {
		    p = t->GetPoint(j);
		    ON_3dPoint *op = (*pointmap)[p];
		    VSET(pt[j], op->x, op->y, op->z);
		}
		//tri one
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_LINE_MOVE);
		BN_ADD_VLIST(vlfree, vhead, pt[1], BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[2], BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_LINE_DRAW);
	    }
	    break;
	case 2:
	    // 2D wireframe
	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
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
	const struct ON_Brep_CDT_State *s)
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
       if ((*s->faces)[i] && (*s->faces)[i]->cdt) {
	   (void)ON_Brep_CDT_VList_Face(vhead, vlfree, i, mode, s);
       }
   }

   return 0;
}


void
populate_3d_pnts(struct ON_Brep_CDT_Face_State *f)
{
    int face_index = f->ind;
    ON_BrepFace &face = f->s_cdt->brep->m_F[face_index];
    p2t::CDT *cdt = f->cdt;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	(*f->s_cdt->tri_brep_face)[t] = face_index;
	for (size_t j = 0; j < 3; j++) {
	    p2t::Point *p = t->GetPoint(j);
	    if (p) {
		ON_3dPoint *op = (*pointmap)[p];
		ON_3dPoint *onorm = (*normalmap)[p];
		if (!op || !onorm) {
		    /* Don't know the point, normal or both.  Ask the Surface */
		    const ON_Surface *s = face.SurfaceOf();
		    ON_3dPoint pnt;
		    ON_3dVector norm;
		    bool surf_ev = surface_EvNormal(s, p->x, p->y, pnt, norm);
		    if (!surf_ev && !op) {
			// Couldn't get point and normal, go with just point
			pnt = s->PointAt(p->x, p->y);
		    }
		    if (!op) {
			op = new ON_3dPoint(pnt);
			CDT_Add3DPnt(f->s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
		    }
		    (*pointmap)[p] = op;

		    // We can only supply a normal if we either a) already had
		    // it or b) got it from surface_EvNormal - we don't have
		    // any PointAt style fallback
		    if (surf_ev && !onorm) {
			if (face.m_bRev) {
			    norm = norm * -1.0;
			}
			onorm = new ON_3dPoint(norm);
			CDT_Add3DNorm(f->s_cdt, onorm, op, face.m_face_index, -1, -1, -1, p->x, p->y);
		    }
		    if (onorm) {
			(*normalmap)[p] = onorm;
		    }
		}
	    } else {
		bu_log("Face %d: p2t face without proper point info...\n", face.m_face_index);
	    }
	}
    }
}

void
CDT_Face_Build_Halfedge(struct ON_Brep_CDT_Face_State *f)
{
    p2t::CDT *cdt = f->cdt;
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();

    f->he_uniq_p2d.clear();
    f->he_p2dind.clear();
    f->he_ind2p2d.clear();
    f->he_triangles.clear();

    // Assemble the set of unique 2D poly2tri points
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	for (size_t j = 0; j < 3; j++) {
	    f->he_uniq_p2d.insert(t->GetPoint(j));
	}
    }
    // Assign all poly2tri 2D points a trimesh index
    std::set<p2t::Point *>::iterator u_it;
    {
	trimesh::index_t ind = 0;
	for (u_it = f->he_uniq_p2d.begin(); u_it != f->he_uniq_p2d.end(); u_it++) {
	    f->he_p2dind[*u_it] = ind;
	    f->he_ind2p2d[ind] = *u_it;
	    ind++;
	}
    }

    // Assemble the faces array
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	if (f->tris_degen->find(t) != f->tris_degen->end()) {
	    continue;
	}

	trimesh::triangle_t tmt;
	for (size_t j = 0; j < 3; j++) {
	    tmt.v[j] = f->he_p2dind[t->GetPoint(j)];
	}
	tmt.t = t;
	f->he_triangles.push_back(tmt);
    }

    // Build the mesh topology information

    if (f->he_edges) {
	delete f->he_edges;
    }
    if (f->he_mesh) {
	delete f->he_mesh;
    }
    f->he_edges = new std::vector<trimesh::edge_t>;
    f->he_mesh = new trimesh::trimesh_t;

    trimesh::unordered_edges_from_triangles(f->he_triangles.size(), &f->he_triangles[0], *f->he_edges);
    f->he_mesh->build(f->he_uniq_p2d.size(), f->he_triangles.size(), &f->he_triangles[0], f->he_edges->size(), &(*f->he_edges)[0]);


    // Identify any singular points for later reference
    std::map<ON_3dPoint *,std::set<p2t::Point *>> p2t_singular_pnts;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	if (f->tris_degen->find(t) != f->tris_degen->end()) {
	    continue;
	}
	for (size_t j = 0; j < 3; j++) {
	    p2t::Point *p2t = t->GetPoint(j);
	    ON_3dPoint *p3d = (*pointmap)[p2t];
	    if (f->s_cdt->singular_vert_to_norms->find(p3d) != f->s_cdt->singular_vert_to_norms->end()) {
		p2t_singular_pnts[p3d].insert(p2t);
	    }
	}
    }

    // Find all the triangles using a singular point as a vertex - that's our
    // starting triangle set.
    //
    // TODO - initially, the code is going to pretty much assume one singularity per face,
    // but that's not a good assumption in general - ultimately, we should build up sets
    // of faces for each singularity for separate processing.
    if (p2t_singular_pnts.size() != 1) {
	bu_log("odd singularity point count, aborting\n");
    }
    std::map<ON_3dPoint *,std::set<p2t::Point *>>::iterator ap_it;

    // In a multi-singularity version of this code this set will have to be specific to each point
    std::set<size_t> singularity_triangles;

    for (ap_it = p2t_singular_pnts.begin(); ap_it != p2t_singular_pnts.end(); ap_it++) {
	std::set<p2t::Point *>::iterator p_it;
	std::set<p2t::Point *> &p2tspnts = ap_it->second;
	for (p_it = p2tspnts.begin(); p_it != p2tspnts.end(); p_it++) {
	    p2t::Point *p2d = *p_it;
	    trimesh::index_t ind = f->he_p2dind[p2d];
	    std::vector<trimesh::index_t> faces = f->he_mesh->vertex_face_neighbors(ind);
	    for (size_t i = 0; i < faces.size(); i++) {
		std::cout << "face index " << faces[i] << "\n";
		singularity_triangles.insert(faces[i]);
	    }
	}
    }

    bu_file_delete("singularity_triangles.plot3");
    plot_trimesh_tris_3d(&singularity_triangles, f->he_triangles, pointmap, "singularity_triangles.plot3");

    // Find the best fit plane for the vertices of the triangles involved with
    // the singular point

    // Make sure all of the triangles can be projected to the plane
    // successfully, without flipping triangles.  If not, the mess will have be
    // handled in coordinated subsections (doable but hard!) and for now we
    // will punt in that situation.

    // Walk out along the mesh, collecting all the triangles from the face
    // that can be projected onto the plane without undo loss of area or normal
    // issues.  Once the terminating criteria are reached, we have the set
    // of triangles that will provide our "replace and remesh" subset of the
    // original face.

    // Walk around the outer triangles (edge use count internal to the subset will
    // be critical here) and construct the bounding polygon of the subset for
    // poly2Tri.

    // Project all points in the subset into the plane, getting XY coordinates
    // for poly2Tri.  Use the previously constructed loop and assemble the
    // polyline and internal point set for the new triangulation, keeping a
    // map from the new XY points to the original p2t points.

    // Perform the new triangulation, assemble the new p2t Triangle set using
    // the mappings, and check the new 3D mesh thus created for issues.  If it
    // is clean, replace the original subset of the parent face with the new
    // triangle set, else fail.  Note that a consequence of this will be that
    // only one of the original singularity trim points will end up being used.

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

