/*                        C D T . H 
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
/** @file cdt.h
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"

#include <vector>
#include <list>
#include <map>
#include <stack>
#include <iostream>
#include <algorithm>
#include <set>
#include <utility>

#include "poly2tri/poly2tri.h"

#include "assert.h"

#include "vmath.h"

#include "bu/color.h"
#include "bu/cv.h"
#include "bu/opt.h"
#include "bu/time.h"
#include "bn/plot3.h"
#include "bn/tol.h"
#include "bn/vlist.h"
#include "bg/polygon.h"
#include "bg/trimesh.h"
#include "brep/defines.h"
#include "brep/cdt.h"
#include "brep/pullback.h"
#include "brep/util.h"

#define BREP_PLANAR_TOL 0.05

/***************************************************/
typedef std::pair<ON_3dPoint *, ON_3dPoint *> Edge;
typedef std::map<Edge, std::set<p2t::Triangle*> > EdgeToTri;

#define BREP_CDT_FAILED -3
#define BREP_CDT_NON_SOLID -2
#define BREP_CDT_UNTESSELLATED -1
#define BREP_CDT_SOLID 0

/* Note - these tolerance values are based on the default
 * values from the GED 'tol' command */
#define BREP_CDT_DEFAULT_TOL_ABS 0.0
#define BREP_CDT_DEFAULT_TOL_REL 0.01
#define BREP_CDT_DEFAULT_TOL_NORM 0.0
#define BREP_CDT_DEFAULT_TOL_DIST BN_TOL_DIST

/* this is a debugging structure - it holds origin information for
 * a point added to the CDT state */
struct cdt_audit_info {
    int face_index;
    int vert_index;
    int trim_index;
    int edge_index;
    ON_2dPoint surf_uv;
    ON_3dPoint vert_pnt; // For auditing normals
};

struct ON_Brep_CDT_State;

struct ON_Brep_CDT_Face_State {
    struct ON_Brep_CDT_State *s_cdt;
    int ind;
    std::map<ON_2dPoint *, struct cdt_audit_info *> *pnt2d_audit_info;

    /* 3D data specific to this face (i.e. not shared at an edge) */
    std::vector<ON_2dPoint *> *w2dpnts;
    std::vector<ON_3dPoint *> *w3dpnts;
    std::vector<ON_3dPoint *> *w3dnorms;

    /* loop points */
    ON_SimpleArray<BrepTrimPoint> *face_loop_points;
    std::map<p2t::Point *, BrepTrimPoint *> *p2t_to_trimpt;
    std::map<p2t::Point *, int> *p2t_trim_ind;
    std::set<ON_2dPoint *> *on_surf_points;
    ON_RTree *rt_trims;

    /* singular trim info */
    std::map<int,ON_3dPoint *> *strim_pnts;
    std::map<int,ON_3dPoint *> *strim_norms;

    /* mappings */
    std::map<ON_2dPoint *, ON_3dPoint *> *on2_to_on3_map;
    std::map<ON_2dPoint *, p2t::Point *> *on2_to_p2t_map;
    std::map<p2t::Point *, ON_2dPoint *> *p2t_to_on2_map;
    std::map<p2t::Point *, ON_3dPoint *> *p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *p2t_to_on3_norm_map;
    std::map<ON_3dPoint *, std::set<p2t::Point *>> *on3_to_tri_map;

    /* Poly2Tri information */
    p2t::CDT *cdt;
    std::vector<p2t::Triangle *> *p2t_extra_faces;
    std::set<p2t::Point *> *degen_pnts;
    std::set<p2t::Triangle*> *tris_degen;
    std::set<p2t::Triangle*> *tris_zero_3D_area;
    EdgeToTri *e2f;
    std::map<Edge, int> *ecnt;
};


struct ON_Brep_CDT_State {

    int status;
    ON_Brep *orig_brep;
    ON_Brep *brep;

    /* Tolerances */
    fastf_t abs;
    fastf_t rel;
    fastf_t norm;
    fastf_t dist;

    /* 3D data */
    std::vector<ON_3dPoint *> *w3dpnts;
    std::vector<ON_3dPoint *> *w3dnorms;

    /* Vertices */
    std::map<int, ON_3dPoint *> *vert_pnts;
    std::map<int, ON_3dPoint *> *vert_avg_norms;
    std::map<ON_3dPoint *, ON_3dPoint *> *vert_to_norms;

    /* Edges */
    std::set<ON_3dPoint *> *edge_pnts;
    std::map<int, double> *min_edge_seg_len;
    std::map<int, double> *max_edge_seg_len;
    std::map<ON_3dPoint *, std::set<BrepTrimPoint *>> *on_brep_edge_pnts;

    /* Audit data */
    std::map<ON_3dPoint *, struct cdt_audit_info *> *pnt_audit_info;

    /* Face specific data */
    std::map<int, struct ON_Brep_CDT_Face_State *> *faces;


    /* Mesh data */
    std::map<int, ON_3dPoint *> *bot_pnt_to_on_pnt;
    std::map<p2t::Triangle*, int> *tri_brep_face;
};

struct brep_cdt_tol {
    fastf_t min_dist;
    fastf_t max_dist;
    fastf_t within_dist;
    fastf_t cos_within_ang;
};
#define BREP_CDT_TOL_ZERO {0.0, 0.0, 0.0, 0.0}

struct cdt_surf_info {
    const ON_Surface *s;
    const ON_BrepFace *f;
    ON_RTree *rt_trims;
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
    std::set<ON_BoundingBox *> leaf_bboxes;
};

void
PerformClosedSurfaceChecks(
	struct ON_Brep_CDT_State *s_cdt,
	const ON_Surface *s,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> *brep_loop_points,
	double same_point_tolerance);

std::map<double, BrepTrimPoint *> *
getEdgePoints(
	struct ON_Brep_CDT_State *s_cdt,
	ON_BrepEdge *edge,
	ON_BrepTrim &trim,
	fastf_t max_dist,
	fastf_t min_dist
	);

void
getSurfacePoints(struct ON_Brep_CDT_Face_State *f);


struct ON_Brep_CDT_Face_State *
ON_Brep_CDT_Face_Create(struct ON_Brep_CDT_State *s_cdt, int ind);
void
ON_Brep_CDT_Face_Reset(struct ON_Brep_CDT_Face_State *fcdt, int full_surface_sample);
void
ON_Brep_CDT_Face_Destroy(struct ON_Brep_CDT_Face_State *fcdt);

void
plot_polyline(std::vector<p2t::Point *> *pnts, const char *filename);
void
plot_tri(p2t::Triangle *t, const char *filename);

struct cdt_audit_info *
cdt_ainfo(int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d, double px, double py, double pz);

void
add_tri_edges(EdgeToTri *e2f, p2t::Triangle *t,
    std::map<p2t::Point *, ON_3dPoint *> *pointmap);

void
CDT_Add2DPnt(struct ON_Brep_CDT_Face_State *f, ON_2dPoint *p, int fid, int vid, int tid, int eid, fastf_t tparam);
void
CDT_Add3DPnt(struct ON_Brep_CDT_State *s, ON_3dPoint *p, int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d);
void
CDT_Add3DNorm(struct ON_Brep_CDT_State *s, ON_3dPoint *norm, ON_3dPoint *vert, int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d);


void
CDT_Tol_Set(struct brep_cdt_tol *cdt, double dist, fastf_t md, double t_abs, double t_rel, double t_norm, double t_dist);

void populate_3d_pnts(struct ON_Brep_CDT_Face_State *f);


void triangles_degenerate_trivial(struct ON_Brep_CDT_Face_State *f);
void triangles_degenerate_area(struct ON_Brep_CDT_Face_State *f);
int triangles_incorrect_normals(struct ON_Brep_CDT_Face_State *f);

int triangles_check_edges(struct ON_Brep_CDT_Face_State *f);

void triangles_rebuild_involved(struct ON_Brep_CDT_Face_State *f);

void
trimesh_error_report(struct ON_Brep_CDT_State *s_cdt, int valid_fcnt, int valid_vcnt, int *valid_faces, fastf_t *valid_vertices, struct bg_trimesh_solid_errors *se);

bool build_poly2tri_polylines(struct ON_Brep_CDT_Face_State *f, int init_rtree);

void
Process_Loop_Edges(struct ON_Brep_CDT_Face_State *f, int li, fastf_t max_dist);


/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

