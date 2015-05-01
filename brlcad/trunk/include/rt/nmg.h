/*                          N M G . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
/** @file rt/nmg.h
 *
 */

#ifndef RT_NMG_H
#define RT_NMG_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/ptbl.h"
#include "bn/tol.h"
#include "rt/hit.h"
#include "rt/application.h"
#include "rt/soltab.h"
#include "rt/seg.h"
#include "nmg.h"

__BEGIN_DECLS

/*********************************************************************************
 *      The following section is an exact copy of what was previously "nmg_rt.h" *
 *      (with minor changes to NMG_GET_HITMISS and NMG_FREE_HITLIST              *
 *      moved here to use RTG.rtg_nmgfree freelist for hitmiss structs.         *
 ******************************************************************************* */

#define NMG_HIT_LIST    0
#define NMG_MISS_LIST   1

/* These values are for the hitmiss "in_out" variable and indicate the
 * nature of the hit when known
 */
#define HMG_INBOUND_STATE(_hm) (((_hm)->in_out & 0x0f0) >> 4)
#define HMG_OUTBOUND_STATE(_hm) ((_hm)->in_out & 0x0f)


#define NMG_RAY_STATE_INSIDE    1
#define NMG_RAY_STATE_ON        2
#define NMG_RAY_STATE_OUTSIDE   4
#define NMG_RAY_STATE_ANY       8

#define HMG_HIT_IN_IN   0x11    /**< @brief  hit internal structure */
#define HMG_HIT_IN_OUT  0x14    /**< @brief  breaking out */
#define HMG_HIT_OUT_IN  0x41    /**< @brief  breaking in */
#define HMG_HIT_OUT_OUT 0x44    /**< @brief  edge/vertex graze */
#define HMG_HIT_IN_ON   0x12
#define HMG_HIT_ON_IN   0x21
#define HMG_HIT_ON_ON   0x22
#define HMG_HIT_OUT_ON  0x42
#define HMG_HIT_ON_OUT  0x24
#define HMG_HIT_ANY_ANY 0x88    /**< @brief  hit on non-3-manifold */

#define NMG_VERT_ENTER 1
#define NMG_VERT_ENTER_LEAVE 0
#define NMG_VERT_LEAVE -1
#define NMG_VERT_UNKNOWN -2

#define NMG_HITMISS_SEG_IN 0x696e00     /**< @brief  "in" */
#define NMG_HITMISS_SEG_OUT 0x6f757400  /**< @brief  "out" */

struct hitmiss {
    struct bu_list      l;
    struct hit          hit;
    fastf_t             dist_in_plane;  /**< @brief  distance from plane intersect */
    int                 in_out;         /**< @brief  status of ray as it transitions
                                         * this hit point.
                                         */
    long                *inbound_use;
    vect_t              inbound_norm;
    long                *outbound_use;
    vect_t              outbound_norm;
    int                 start_stop;     /**< @brief  is this a seg_in or seg_out */
    struct hitmiss      *other;         /**< @brief  for keeping track of the other
                                         * end of the segment when we know
                                         * it
                                         */
};


#ifdef NO_BOMBING_MACROS
#  define NMG_CK_HITMISS(hm) BU_IGNORE((hm))
#else
#  define NMG_CK_HITMISS(hm) \
    {\
        switch (hm->l.magic) { \
            case NMG_RT_HIT_MAGIC: \
            case NMG_RT_HIT_SUB_MAGIC: \
            case NMG_RT_MISS_MAGIC: \
                break; \
            case NMG_MISS_LIST: \
                bu_log(BU_FLSTR ": struct hitmiss has NMG_MISS_LIST magic #\n"); \
                bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
            case NMG_HIT_LIST: \
                bu_log(BU_FLSTR ": struct hitmiss has NMG_MISS_LIST magic #\n"); \
                bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
            default: \
                bu_log(BU_FLSTR ": bad struct hitmiss magic: %u:(0x%08x)\n", \
                       hm->l.magic, hm->l.magic); \
                bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
        }\
        if (!hm->hit.hit_private) { \
            bu_log(BU_FLSTR ": NULL hit_private in hitmiss struct\n"); \
            bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
        } \
    }
#endif

#ifdef NO_BOMBING_MACROS
#  define NMG_CK_HITMISS_LISTS(rd) BU_IGNORE((rd))
#else
#  define NMG_CK_HITMISS_LISTS(rd) \
    { \
        struct hitmiss *_a_hit; \
        for (BU_LIST_FOR(_a_hit, hitmiss, &rd->rd_hit)) {NMG_CK_HITMISS(_a_hit);} \
        for (BU_LIST_FOR(_a_hit, hitmiss, &rd->rd_miss)) {NMG_CK_HITMISS(_a_hit);} \
    }
#endif

/**
 * Ray Data structure
 *
 * A) the hitmiss table has one element for each nmg structure in the
 * nmgmodel.  The table keeps track of which elements have been
 * processed before and which haven't.  Elements in this table will
 * either be: (NULL) item not previously processed hitmiss ptr item
 * previously processed
 *
 * the 0th item in the array is a pointer to the head of the "hit"
 * list.  The 1th item in the array is a pointer to the head of the
 * "miss" list.
 *
 * B) If plane_pt is non-null then we are currently processing a face
 * intersection.  The plane_dist and ray_dist_to_plane are valid.  The
 * ray/edge intersector should check the distance from the plane
 * intercept to the edge and update "plane_closest" if the current
 * edge is closer to the intercept than the previous closest object.
 */
struct ray_data {
    uint32_t            magic;
    struct model        *rd_m;
    char                *manifolds; /**< @brief   structure 1-3manifold table */
    vect_t              rd_invdir;
    struct xray         *rp;
    struct application  *ap;
    struct seg          *seghead;
    struct soltab       *stp;
    const struct bn_tol *tol;
    struct hitmiss      **hitmiss;      /**< @brief  1 struct hitmiss ptr per elem. */
    struct bu_list      rd_hit;         /**< @brief  list of hit elements */
    struct bu_list      rd_miss;        /**< @brief  list of missed/sub-hit elements */

/* The following are to support isect_ray_face() */

    /**
     * plane_pt is the intercept point of the ray with the plane of
     * the face.
     */
    point_t             plane_pt;       /**< @brief  ray/plane(face) intercept point */

    /**
     * ray_dist_to_plane is the parametric distance along the ray from
     * the ray origin (rd->rp->r_pt) to the ray/plane intercept point
     */
    fastf_t             ray_dist_to_plane; /**< @brief  ray parametric dist to plane */

    /**
     * the "face_subhit" element is a boolean used by isect_ray_face
     * and [e|v]u_touch_func to record the fact that the
     * ray/(plane/face) intercept point was within tolerance of an
     * edge/vertex of the face.  In such instances, isect_ray_face
     * does NOT need to generate a hit point for the face, as the hit
     * point for the edge/vertex will suffice.
     */
    int                 face_subhit;

    /**
     * the "classifying_ray" flag indicates that this ray is being
     * used to classify a point, so that the "eu_touch" and "vu_touch"
     * functions should not be called.
     */
    int                 classifying_ray;
};

#define NMG_PCA_EDGE    1
#define NMG_PCA_EDGE_VERTEX 2
#define NMG_PCA_VERTEX 3
#define NMG_CK_RD(_rd) NMG_CKMAG(_rd, NMG_RAY_DATA_MAGIC, "ray data");


#define NMG_GET_HITMISS(_p, _ap) { \
        (_p) = BU_LIST_FIRST(hitmiss, &((_ap)->a_resource->re_nmgfree)); \
        if (BU_LIST_IS_HEAD((_p), &((_ap)->a_resource->re_nmgfree))) \
            BU_ALLOC((_p), struct hitmiss); \
        else \
            BU_LIST_DEQUEUE(&((_p)->l)); \
    }


#define NMG_FREE_HITLIST(_p, _ap) { \
        BU_CK_LIST_HEAD((_p)); \
        BU_LIST_APPEND_LIST(&((_ap)->a_resource->re_nmgfree), (_p)); \
    }


#define HIT 1   /**< @brief  a hit on a face */
#define MISS 0  /**< @brief  a miss on the face */


#ifdef NO_BOMBING_MACROS
#  define nmg_bu_bomb(rd, str) BU_IGNORE((rd))
#else
#  define nmg_bu_bomb(rd, str) { \
        bu_log("%s", str); \
        if (RTG.NMG_debug & DEBUG_NMGRT) bu_bomb("End of diagnostics"); \
        BU_LIST_INIT(&rd->rd_hit); \
        BU_LIST_INIT(&rd->rd_miss); \
        RTG.NMG_debug |= DEBUG_NMGRT; \
        nmg_isect_ray_model(rd); \
        (void) nmg_ray_segs(rd); \
        bu_bomb("Should have bombed before this\n"); \
    }
#endif

struct nmg_radial {
    struct bu_list      l;
    struct edgeuse      *eu;
    struct faceuse      *fu;            /**< @brief  Derived from eu */
    struct shell        *s;             /**< @brief  Derived from eu */
    int                 existing_flag;  /**< @brief  !0 if this eu exists on dest edge */
    int                 is_crack;       /**< @brief  This eu is part of a crack. */
    int                 is_outie;       /**< @brief  This crack is an "outie" */
    int                 needs_flip;     /**< @brief  Insert eumate, not eu */
    fastf_t             ang;            /**< @brief  angle, in radians.  0 to 2pi */
};
#define NMG_CK_RADIAL(_p) NMG_CKMAG(_p, NMG_RADIAL_MAGIC, "nmg_radial")

struct nmg_inter_struct {
    uint32_t            magic;
    struct bu_ptbl      *l1;            /**< @brief  vertexuses on the line of */
    struct bu_ptbl      *l2;            /**< @brief  intersection between planes */
    fastf_t             *mag1;          /**< @brief  Distances along intersection line */
    fastf_t             *mag2;          /**< @brief  for each vertexuse in l1 and l2. */
    int                 mag_len;        /**< @brief  Array size of mag1 and mag2 */
    struct shell        *s1;
    struct shell        *s2;
    struct faceuse      *fu1;           /**< @brief  null if l1 comes from a wire */
    struct faceuse      *fu2;           /**< @brief  null if l2 comes from a wire */
    struct bn_tol       tol;
    int                 coplanar;       /**< @brief  a flag */
    struct edge_g_lseg  *on_eg;         /**< @brief  edge_g for line of intersection */
    point_t             pt;             /**< @brief  3D line of intersection */
    vect_t              dir;
    point_t             pt2d;           /**< @brief  2D projection of isect line */
    vect_t              dir2d;
    fastf_t             *vert2d;        /**< @brief  Array of 2d vertex projections [index] */
    int                 maxindex;       /**< @brief  size of vert2d[] */
    mat_t               proj;           /**< @brief  Matrix to project onto XY plane */
    const uint32_t      *twod;          /**< @brief  ptr to face/edge of 2d projection */
};
#define NMG_CK_INTER_STRUCT(_p) NMG_CKMAG(_p, NMG_INTER_STRUCT_MAGIC, "nmg_inter_struct")

/* nmg_class.c */
RT_EXPORT extern int nmg_classify_pt_loop(const point_t pt,
                                          const struct loopuse *lu,
                                          const struct bn_tol *tol);

RT_EXPORT extern int nmg_classify_s_vs_s(struct shell *s,
                                         struct shell *s2,
                                         const struct bn_tol *tol);

RT_EXPORT extern int nmg_classify_lu_lu(const struct loopuse *lu1,
                                        const struct loopuse *lu2,
                                        const struct bn_tol *tol);

RT_EXPORT extern int nmg_class_pt_f(const point_t pt,
                                    const struct faceuse *fu,
                                    const struct bn_tol *tol);
RT_EXPORT extern int nmg_class_pt_s(const point_t pt,
                                    const struct shell *s,
                                    const int in_or_out_only,
                                    const struct bn_tol *tol);

/* From nmg_pt_fu.c */
RT_EXPORT extern int nmg_eu_is_part_of_crack(const struct edgeuse *eu);

RT_EXPORT extern int nmg_class_pt_lu_except(point_t             pt,
                                            const struct loopuse        *lu,
                                            const struct edge           *e_p,
                                            const struct bn_tol *tol);

RT_EXPORT extern int nmg_class_pt_fu_except(const point_t pt,
                                            const struct faceuse *fu,
                                            const struct loopuse *ignore_lu,
                                            void (*eu_func)(struct edgeuse *, point_t, const char *),
                                            void (*vu_func)(struct vertexuse *, point_t, const char *),
                                            const char *priv,
                                            const int call_on_hits,
                                            const int in_or_out_only,
                                            const struct bn_tol *tol);

/* From nmg_plot.c */
RT_EXPORT extern void nmg_pl_shell(FILE *fp,
                                   const struct shell *s,
                                   int fancy);

RT_EXPORT extern void nmg_vu_to_vlist(struct bu_list *vhead,
                                      const struct vertexuse    *vu);
RT_EXPORT extern void nmg_eu_to_vlist(struct bu_list *vhead,
                                      const struct bu_list      *eu_hd);
RT_EXPORT extern void nmg_lu_to_vlist(struct bu_list *vhead,
                                      const struct loopuse      *lu,
                                      int                       poly_markers,
                                      const vectp_t             norm);
RT_EXPORT extern void nmg_snurb_fu_to_vlist(struct bu_list              *vhead,
                                            const struct faceuse        *fu,
                                            int                 poly_markers);
RT_EXPORT extern void nmg_s_to_vlist(struct bu_list             *vhead,
                                     const struct shell *s,
                                     int                        poly_markers);
RT_EXPORT extern void nmg_r_to_vlist(struct bu_list             *vhead,
                                     const struct nmgregion     *r,
                                     int                        poly_markers);
RT_EXPORT extern void nmg_m_to_vlist(struct bu_list     *vhead,
                                     struct model       *m,
                                     int                poly_markers);
RT_EXPORT extern void nmg_offset_eu_vert(point_t                        base,
                                         const struct edgeuse   *eu,
                                         const vect_t           face_normal,
                                         int                    tip);
/* plot */
RT_EXPORT extern void nmg_pl_v(FILE     *fp,
                               const struct vertex *v,
                               long *b);
RT_EXPORT extern void nmg_pl_e(FILE *fp,
                               const struct edge *e,
                               long *b,
                               int red,
                               int green,
                               int blue);
RT_EXPORT extern void nmg_pl_eu(FILE *fp,
                                const struct edgeuse *eu,
                                long *b,
                                int red,
                                int green,
                                int blue);
RT_EXPORT extern void nmg_pl_lu(FILE *fp,
                                const struct loopuse *fu,
                                long *b,
                                int red,
                                int green,
                                int blue);
RT_EXPORT extern void nmg_pl_fu(FILE *fp,
                                const struct faceuse *fu,
                                long *b,
                                int red,
                                int green,
                                int blue);
RT_EXPORT extern void nmg_pl_s(FILE *fp,
                               const struct shell *s);
RT_EXPORT extern void nmg_pl_r(FILE *fp,
                               const struct nmgregion *r);
RT_EXPORT extern void nmg_pl_m(FILE *fp,
                               const struct model *m);
RT_EXPORT extern void nmg_vlblock_v(struct bn_vlblock *vbp,
                                    const struct vertex *v,
                                    long *tab);
RT_EXPORT extern void nmg_vlblock_e(struct bn_vlblock *vbp,
                                    const struct edge *e,
                                    long *tab,
                                    int red,
                                    int green,
                                    int blue);
RT_EXPORT extern void nmg_vlblock_eu(struct bn_vlblock *vbp,
                                     const struct edgeuse *eu,
                                     long *tab,
                                     int red,
                                     int green,
                                     int blue,
                                     int fancy);
RT_EXPORT extern void nmg_vlblock_euleft(struct bu_list                 *vh,
                                         const struct edgeuse           *eu,
                                         const point_t                  center,
                                         const mat_t                    mat,
                                         const vect_t                   xvec,
                                         const vect_t                   yvec,
                                         double                         len,
                                         const struct bn_tol            *tol);
RT_EXPORT extern void nmg_vlblock_around_eu(struct bn_vlblock           *vbp,
                                            const struct edgeuse        *arg_eu,
                                            long                        *tab,
                                            int                 fancy,
                                            const struct bn_tol *tol);
RT_EXPORT extern void nmg_vlblock_lu(struct bn_vlblock *vbp,
                                     const struct loopuse *lu,
                                     long *tab,
                                     int red,
                                     int green,
                                     int blue,
                                     int fancy);
RT_EXPORT extern void nmg_vlblock_fu(struct bn_vlblock *vbp,
                                     const struct faceuse *fu,
                                     long *tab, int fancy);
RT_EXPORT extern void nmg_vlblock_s(struct bn_vlblock *vbp,
                                    const struct shell *s,
                                    int fancy);
RT_EXPORT extern void nmg_vlblock_r(struct bn_vlblock *vbp,
                                    const struct nmgregion *r,
                                    int fancy);
RT_EXPORT extern void nmg_vlblock_m(struct bn_vlblock *vbp,
                                    const struct model *m,
                                    int fancy);

/* visualization helper routines */
RT_EXPORT extern void nmg_pl_edges_in_2_shells(struct bn_vlblock        *vbp,
                                               long                     *b,
                                               const struct edgeuse     *eu,
                                               int                      fancy,
                                               const struct bn_tol      *tol);
RT_EXPORT extern void nmg_pl_isect(const char           *filename,
                                   const struct shell   *s,
                                   const struct bn_tol  *tol);
RT_EXPORT extern void nmg_pl_comb_fu(int num1,
                                     int num2,
                                     const struct faceuse *fu1);
RT_EXPORT extern void nmg_pl_2fu(const char *str,
                                 const struct faceuse *fu1,
                                 const struct faceuse *fu2,
                                 int show_mates);
/* graphical display of classifier results */
RT_EXPORT extern void nmg_show_broken_classifier_stuff(uint32_t *p,
                                                       char     **classlist,
                                                       int      all_new,
                                                       int      fancy,
                                                       const char       *a_string);
RT_EXPORT extern void nmg_face_plot(const struct faceuse *fu);
RT_EXPORT extern void nmg_2face_plot(const struct faceuse *fu1,
                                     const struct faceuse *fu2);
RT_EXPORT extern void nmg_face_lu_plot(const struct loopuse *lu,
                                       const struct vertexuse *vu1,
                                       const struct vertexuse *vu2);
RT_EXPORT extern void nmg_plot_lu_ray(const struct loopuse              *lu,
                                      const struct vertexuse            *vu1,
                                      const struct vertexuse            *vu2,
                                      const vect_t                      left);
RT_EXPORT extern void nmg_plot_ray_face(const char *fname,
                                        point_t pt,
                                        const vect_t dir,
                                        const struct faceuse *fu);
RT_EXPORT extern void nmg_plot_lu_around_eu(const char          *prefix,
                                            const struct edgeuse        *eu,
                                            const struct bn_tol *tol);
RT_EXPORT extern int nmg_snurb_to_vlist(struct bu_list          *vhead,
                                        const struct face_g_snurb       *fg,
                                        int                     n_interior);
RT_EXPORT extern void nmg_cnurb_to_vlist(struct bu_list *vhead,
                                         const struct edgeuse *eu,
                                         int n_interior,
					 int cmd);

/**
 * global nmg animation plot callback
 */
RT_EXPORT extern void (*nmg_plot_anim_upcall)(void);

/**
 * global nmg animation vblock callback
 */
RT_EXPORT extern void (*nmg_vlblock_anim_upcall)(void);

/**
 * global nmg mged display debug callback
 */
RT_EXPORT extern void (*nmg_mged_debug_display_hack)(void);

/**
 * edge use distance tolerance
 */
RT_EXPORT extern double nmg_eue_dist;


/* from nmg_mesh.c */
RT_EXPORT extern int nmg_mesh_two_faces(struct faceuse *fu1,
                                        struct faceuse *fu2,
                                        const struct bn_tol     *tol);
RT_EXPORT extern void nmg_radial_join_eu(struct edgeuse *eu1,
                                         struct edgeuse *eu2,
                                         const struct bn_tol *tol);
RT_EXPORT extern void nmg_mesh_faces(struct faceuse *fu1,
                                     struct faceuse *fu2,
                                     const struct bn_tol *tol);
RT_EXPORT extern int nmg_mesh_face_shell(struct faceuse *fu1,
                                         struct shell *s,
                                         const struct bn_tol *tol);
RT_EXPORT extern int nmg_mesh_shell_shell(struct shell *s1,
                                          struct shell *s2,
                                          const struct bn_tol *tol);
RT_EXPORT extern double nmg_measure_fu_angle(const struct edgeuse *eu,
                                             const vect_t xvec,
                                             const vect_t yvec,
                                             const vect_t zvec);

/* from nmg_bool.c */
RT_EXPORT extern struct nmgregion *nmg_do_bool(struct nmgregion *s1,
                                               struct nmgregion *s2,
                                               const int oper, const struct bn_tol *tol);
RT_EXPORT extern void nmg_shell_coplanar_face_merge(struct shell *s,
                                                    const struct bn_tol *tol,
                                                    const int simplify);
RT_EXPORT extern int nmg_two_region_vertex_fuse(struct nmgregion *r1,
                                                struct nmgregion *r2,
                                                const struct bn_tol *tol);

struct db_tree_state; /* forward declaration */
RT_EXPORT extern union tree *nmg_booltree_leaf_tess(struct db_tree_state *tsp,
                                                    const struct db_full_path *pathp,
                                                    struct rt_db_internal *ip,
                                                    void *client_data);
RT_EXPORT extern union tree *nmg_booltree_leaf_tnurb(struct db_tree_state *tsp,
                                                     const struct db_full_path *pathp,
                                                     struct rt_db_internal *ip,
                                                     void *client_data);

RT_EXPORT extern int nmg_bool_eval_silent;      /* quell output from nmg_booltree_evaluate */
RT_EXPORT extern union tree *nmg_booltree_evaluate(union tree *tp,
                                                   const struct bn_tol *tol,
                                                   struct resource *resp);
RT_EXPORT extern int nmg_boolean(union tree *tp,
                                 struct model *m,
                                 const struct bn_tol *tol,
                                 struct resource *resp);

/* from nmg_class.c */
RT_EXPORT extern void nmg_class_shells(struct shell *sA,
                                       struct shell *sB,
                                       char **classlist,
                                       const struct bn_tol *tol);

/* from nmg_fcut.c */
/* static void ptbl_vsort */
RT_EXPORT extern int nmg_ck_vu_ptbl(struct bu_ptbl      *p,
                                    struct faceuse      *fu);
RT_EXPORT extern double nmg_vu_angle_measure(struct vertexuse   *vu,
                                             vect_t x_dir,
                                             vect_t y_dir,
                                             int assessment,
                                             int in);
RT_EXPORT extern int nmg_wedge_class(int        ass,    /* assessment of two edges forming wedge */
                                     double     a,
                                     double     b);
RT_EXPORT extern void nmg_sanitize_fu(struct faceuse    *fu);
RT_EXPORT extern void nmg_unlist_v(struct bu_ptbl       *b,
                                   fastf_t *mag,
                                   struct vertex        *v);
RT_EXPORT extern struct edge_g_lseg *nmg_face_cutjoin(struct bu_ptbl *b1,
                                                      struct bu_ptbl *b2,
                                                      fastf_t *mag1,
                                                      fastf_t *mag2,
                                                      struct faceuse *fu1,
                                                      struct faceuse *fu2,
                                                      point_t pt,
                                                      vect_t dir,
                                                      struct edge_g_lseg *eg,
                                                      const struct bn_tol *tol);
RT_EXPORT extern void nmg_fcut_face_2d(struct bu_ptbl *vu_list,
                                       fastf_t *mag,
                                       struct faceuse *fu1,
                                       struct faceuse *fu2,
                                       struct bn_tol *tol);
RT_EXPORT extern int nmg_insert_vu_if_on_edge(struct vertexuse *vu1,
                                              struct vertexuse *vu2,
                                              struct edgeuse *new_eu,
                                              struct bn_tol *tol);
/* nmg_face_state_transition */

#define nmg_mev(_v, _u) nmg_me((_v), (struct vertex *)NULL, (_u))

/* From nmg_eval.c */
RT_EXPORT extern void nmg_ck_lu_orientation(struct loopuse *lu,
                                            const struct bn_tol *tolp);
RT_EXPORT extern const char *nmg_class_name(int class_no);
RT_EXPORT extern void nmg_evaluate_boolean(struct shell *sA,
                                           struct shell *sB,
                                           int          op,
                                           char         **classlist,
                                           const struct bn_tol  *tol);

/* The following functions cannot be publicly declared because struct
 * nmg_bool_state is private to nmg_eval.c
 */
/* nmg_eval_shell */
/* nmg_eval_action */
/* nmg_eval_plot */


/* From nmg_rt_isect.c */
RT_EXPORT extern void nmg_rt_print_hitlist(struct bu_list *hd);

RT_EXPORT extern void nmg_rt_print_hitmiss(struct hitmiss *a_hit);

RT_EXPORT extern int nmg_class_ray_vs_shell(struct xray *rp,
                                            const struct shell *s,
                                            const int in_or_out_only,
                                            const struct bn_tol *tol);

RT_EXPORT extern void nmg_isect_ray_model(struct ray_data *rd);

/* From nmg_ck.c */
RT_EXPORT extern void nmg_vvg(const struct vertex_g *vg);
RT_EXPORT extern void nmg_vvertex(const struct vertex *v,
                                  const struct vertexuse *vup);
RT_EXPORT extern void nmg_vvua(const uint32_t *vua);
RT_EXPORT extern void nmg_vvu(const struct vertexuse *vu,
                              const uint32_t *up_magic_p);
RT_EXPORT extern void nmg_veg(const uint32_t *eg);
RT_EXPORT extern void nmg_vedge(const struct edge *e,
                                const struct edgeuse *eup);
RT_EXPORT extern void nmg_veu(const struct bu_list      *hp,
                              const uint32_t *up_magic_p);
RT_EXPORT extern void nmg_vlg(const struct loop_g *lg);
RT_EXPORT extern void nmg_vloop(const struct loop *l,
                                const struct loopuse *lup);
RT_EXPORT extern void nmg_vlu(const struct bu_list      *hp,
                              const uint32_t *up);
RT_EXPORT extern void nmg_vfg(const struct face_g_plane *fg);
RT_EXPORT extern void nmg_vface(const struct face *f,
                                const struct faceuse *fup);
RT_EXPORT extern void nmg_vfu(const struct bu_list      *hp,
                              const struct shell *s);
RT_EXPORT extern void nmg_vsshell(const struct shell *s,
                                 const struct nmgregion *r);
RT_EXPORT extern void nmg_vshell(const struct bu_list *hp,
                                 const struct nmgregion *r);
RT_EXPORT extern void nmg_vregion(const struct bu_list *hp,
                                  const struct model *m);
RT_EXPORT extern void nmg_vmodel(const struct model *m);

/* checking routines */
RT_EXPORT extern void nmg_ck_e(const struct edgeuse *eu,
                               const struct edge *e,
                               const char *str);
RT_EXPORT extern void nmg_ck_vu(const uint32_t *parent,
                                const struct vertexuse *vu,
                                const char *str);
RT_EXPORT extern void nmg_ck_eu(const uint32_t *parent,
                                const struct edgeuse *eu,
                                const char *str);
RT_EXPORT extern void nmg_ck_lg(const struct loop *l,
                                const struct loop_g *lg,
                                const char *str);
RT_EXPORT extern void nmg_ck_l(const struct loopuse *lu,
                               const struct loop *l,
                               const char *str);
RT_EXPORT extern void nmg_ck_lu(const uint32_t *parent,
                                const struct loopuse *lu,
                                const char *str);
RT_EXPORT extern void nmg_ck_fg(const struct face *f,
                                const struct face_g_plane *fg,
                                const char *str);
RT_EXPORT extern void nmg_ck_f(const struct faceuse *fu,
                               const struct face *f,
                               const char *str);
RT_EXPORT extern void nmg_ck_fu(const struct shell *s,
                                const struct faceuse *fu,
                                const char *str);
RT_EXPORT extern int nmg_ck_eg_verts(const struct edge_g_lseg *eg,
                                     const struct bn_tol *tol);
RT_EXPORT extern int nmg_ck_geometry(const struct model *m,
                                     const struct bn_tol *tol);
RT_EXPORT extern int nmg_ck_face_worthless_edges(const struct faceuse *fu);
RT_EXPORT extern void nmg_ck_lueu(const struct loopuse *lu, const char *s);
RT_EXPORT extern int nmg_check_radial(const struct edgeuse *eu, const struct bn_tol *tol);
RT_EXPORT extern int nmg_eu_2s_orient_bad(const struct edgeuse  *eu,
                                          const struct shell    *s1,
                                          const struct shell    *s2,
                                          const struct bn_tol   *tol);
RT_EXPORT extern int nmg_ck_closed_surf(const struct shell *s,
                                        const struct bn_tol *tol);
RT_EXPORT extern int nmg_ck_closed_region(const struct nmgregion *r,
                                          const struct bn_tol *tol);
RT_EXPORT extern void nmg_ck_v_in_2fus(const struct vertex *vp,
                                       const struct faceuse *fu1,
                                       const struct faceuse *fu2,
                                       const struct bn_tol *tol);
RT_EXPORT extern void nmg_ck_vs_in_region(const struct nmgregion *r,
                                          const struct bn_tol *tol);


/* From nmg_inter.c */
RT_EXPORT extern struct vertexuse *nmg_make_dualvu(struct vertex *v,
                                                   struct faceuse *fu,
                                                   const struct bn_tol *tol);
RT_EXPORT extern struct vertexuse *nmg_enlist_vu(struct nmg_inter_struct        *is,
                                                 const struct vertexuse *vu,
                                                 struct vertexuse *dualvu,
                                                 fastf_t dist);
RT_EXPORT extern void nmg_isect2d_prep(struct nmg_inter_struct *is,
                                       const uint32_t *assoc_use);
RT_EXPORT extern void nmg_isect2d_cleanup(struct nmg_inter_struct *is);
RT_EXPORT extern void nmg_isect2d_final_cleanup(void);
RT_EXPORT extern int nmg_isect_2faceuse(point_t pt,
                                        vect_t dir,
                                        struct faceuse *fu1,
                                        struct faceuse *fu2,
                                        const struct bn_tol *tol);
RT_EXPORT extern void nmg_isect_vert2p_face2p(struct nmg_inter_struct *is,
                                              struct vertexuse *vu1,
                                              struct faceuse *fu2);
RT_EXPORT extern struct edgeuse *nmg_break_eu_on_v(struct edgeuse *eu1,
                                                   struct vertex *v2,
                                                   struct faceuse *fu,
                                                   struct nmg_inter_struct *is);
RT_EXPORT extern void nmg_break_eg_on_v(const struct edge_g_lseg        *eg,
                                        struct vertex           *v,
                                        const struct bn_tol     *tol);
RT_EXPORT extern int nmg_isect_2colinear_edge2p(struct edgeuse  *eu1,
                                                struct edgeuse  *eu2,
                                                struct faceuse          *fu,
                                                struct nmg_inter_struct *is,
                                                struct bu_ptbl          *l1,
                                                struct bu_ptbl          *l2);
RT_EXPORT extern int nmg_isect_edge2p_edge2p(struct nmg_inter_struct    *is,
                                             struct edgeuse             *eu1,
                                             struct edgeuse             *eu2,
                                             struct faceuse             *fu1,
                                             struct faceuse             *fu2);
RT_EXPORT extern int nmg_isect_construct_nice_ray(struct nmg_inter_struct       *is,
                                                  struct faceuse                *fu2);
RT_EXPORT extern void nmg_enlist_one_vu(struct nmg_inter_struct *is,
                                        const struct vertexuse  *vu,
                                        fastf_t                 dist);
RT_EXPORT extern int    nmg_isect_line2_edge2p(struct nmg_inter_struct  *is,
                                               struct bu_ptbl           *list,
                                               struct edgeuse           *eu1,
                                               struct faceuse           *fu1,
                                               struct faceuse           *fu2);
RT_EXPORT extern void nmg_isect_line2_vertex2(struct nmg_inter_struct   *is,
                                              struct vertexuse  *vu1,
                                              struct faceuse            *fu1);
RT_EXPORT extern int nmg_isect_two_ptbls(struct nmg_inter_struct        *is,
                                         const struct bu_ptbl   *t1,
                                         const struct bu_ptbl   *t2);
RT_EXPORT extern struct edge_g_lseg     *nmg_find_eg_on_line(const uint32_t *magic_p,
                                                             const point_t              pt,
                                                             const vect_t               dir,
                                                             const struct bn_tol        *tol);
RT_EXPORT extern int nmg_k0eu(struct vertex     *v);
RT_EXPORT extern struct vertex *nmg_repair_v_near_v(struct vertex               *hit_v,
                                                    struct vertex               *v,
                                                    const struct edge_g_lseg    *eg1,
                                                    const struct edge_g_lseg    *eg2,
                                                    int                 bomb,
                                                    const struct bn_tol *tol);
RT_EXPORT extern struct vertex *nmg_search_v_eg(const struct edgeuse    *eu,
                                                int                     second,
                                                const struct edge_g_lseg        *eg1,
                                                const struct edge_g_lseg        *eg2,
                                                struct vertex           *hit_v,
                                                const struct bn_tol     *tol);
RT_EXPORT extern struct vertex *nmg_common_v_2eg(struct edge_g_lseg     *eg1,
                                                 struct edge_g_lseg     *eg2,
                                                 const struct bn_tol    *tol);
RT_EXPORT extern int nmg_is_vertex_on_inter(struct vertex *v,
                                            struct faceuse *fu1,
                                            struct faceuse *fu2,
                                            struct nmg_inter_struct *is);
RT_EXPORT extern void nmg_isect_eu_verts(struct edgeuse *eu,
                                         struct vertex_g *vg1,
                                         struct vertex_g *vg2,
                                         struct bu_ptbl *verts,
                                         struct bu_ptbl *inters,
                                         const struct bn_tol *tol);
RT_EXPORT extern void nmg_isect_eu_eu(struct edgeuse *eu1,
                                      struct vertex_g *vg1a,
                                      struct vertex_g *vg1b,
                                      vect_t dir1,
                                      struct edgeuse *eu2,
                                      struct bu_ptbl *verts,
                                      struct bu_ptbl *inters,
                                      const struct bn_tol *tol);
RT_EXPORT extern void nmg_isect_eu_fu(struct nmg_inter_struct *is,
                                      struct bu_ptbl            *verts,
                                      struct edgeuse            *eu,
                                      struct faceuse          *fu);
RT_EXPORT extern void nmg_isect_fu_jra(struct nmg_inter_struct  *is,
                                       struct faceuse           *fu1,
                                       struct faceuse           *fu2,
                                       struct bu_ptbl           *eu1_list,
                                       struct bu_ptbl           *eu2_list);
RT_EXPORT extern void nmg_isect_line2_face2pNEW(struct nmg_inter_struct *is,
                                                struct faceuse *fu1, struct faceuse *fu2,
                                                struct bu_ptbl *eu1_list,
                                                struct bu_ptbl *eu2_list);
RT_EXPORT extern int    nmg_is_eu_on_line3(const struct edgeuse *eu,
                                           const point_t                pt,
                                           const vect_t         dir,
                                           const struct bn_tol  *tol);
RT_EXPORT extern struct edge_g_lseg     *nmg_find_eg_between_2fg(const struct faceuse   *ofu1,
                                                                 const struct faceuse   *fu2,
                                                                 const struct bn_tol    *tol);
RT_EXPORT extern struct edgeuse *nmg_does_fu_use_eg(const struct faceuse        *fu1,
                                                    const uint32_t      *eg);
RT_EXPORT extern int rt_line_on_plane(const point_t     pt,
                                      const vect_t      dir,
                                      const plane_t     plane,
                                      const struct bn_tol       *tol);
RT_EXPORT extern void nmg_cut_lu_into_coplanar_and_non(struct loopuse *lu,
                                                       plane_t pl,
                                                       struct nmg_inter_struct *is);
RT_EXPORT extern void nmg_check_radial_angles(char *str,
                                              struct shell *s,
                                              const struct bn_tol *tol);
RT_EXPORT extern int nmg_faces_can_be_intersected(struct nmg_inter_struct *bs,
                                                  const struct faceuse *fu1,
                                                  const struct faceuse *fu2,
                                                  const struct bn_tol *tol);
RT_EXPORT extern void nmg_isect_two_generic_faces(struct faceuse                *fu1,
                                                  struct faceuse                *fu2,
                                                  const struct bn_tol   *tol);
RT_EXPORT extern void nmg_crackshells(struct shell *s1,
                                      struct shell *s2,
                                      const struct bn_tol *tol);
RT_EXPORT extern int nmg_fu_touchingloops(const struct faceuse *fu);


/* From nmg_index.c */
RT_EXPORT extern int nmg_index_of_struct(const uint32_t *p);
RT_EXPORT extern void nmg_m_set_high_bit(struct model *m);
RT_EXPORT extern void nmg_m_reindex(struct model *m, long newindex);
RT_EXPORT extern void nmg_vls_struct_counts(struct bu_vls *str,
                                            const struct nmg_struct_counts *ctr);
RT_EXPORT extern void nmg_pr_struct_counts(const struct nmg_struct_counts *ctr,
                                           const char *str);
RT_EXPORT extern uint32_t **nmg_m_struct_count(struct nmg_struct_counts *ctr,
                                                    const struct model *m);
RT_EXPORT extern void nmg_pr_m_struct_counts(const struct model *m,
                                             const char         *str);
RT_EXPORT extern void nmg_merge_models(struct model *m1,
                                       struct model *m2);
RT_EXPORT extern long nmg_find_max_index(const struct model *m);


__END_DECLS

#endif /* RT_NMG_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
