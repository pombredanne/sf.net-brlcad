/*            S C R E E N E D _ P O I S S O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file screened_poisson.cpp
 *
 * Brief description
 *
 */
#include "common.h"

#include "vmath.h"
#include "raytrace.h"
#include "gcv_private.h"

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wunused-variable"
#endif
#include "MarchingCubes.h"
#include "Octree.h"
#include "SparseMatrix.h"
#include "PPolynomial.h"
#include "MemoryUsage.h"
#include "PointStream.h"
#include "Factor.cpp"
#include "Geometry.cpp"
#include "MarchingCubes.cpp"
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif


#define DEFAULT_FULL_DEPTH 5
struct gcvpnt {
    point_t p;
    vect_t n;
    int is_set;
};

struct npoints {
    struct gcvpnt in;
    struct gcvpnt out;
};

struct gcv_point_container {
    struct npoints *pts;
    int pnt_cnt;
    int capacity;
};

template< class Real >
class GCVPointStream : public PointStream< Real >
{
    struct gcvpnt **gcvpnts;
    int curr_ind;
    int pnt_cnt;
    public:
    GCVPointStream( struct gcvpnt **pnts, int cnt );
    ~GCVPointStream( void );
    void reset( void );
    bool nextPoint( Point3D< Real >& p , Point3D< Real >& n );

};

template< class Real >
GCVPointStream< Real >::GCVPointStream( struct gcvpnt **pnts, int cnt)
{
    gcvpnts = pnts;
    curr_ind = 0;
    pnt_cnt = cnt;
}
template< class Real >
GCVPointStream< Real >::~GCVPointStream( void )
{
}
template< class Real >
void GCVPointStream< Real >::reset( void ) { curr_ind = 0; }
template< class Real >
bool GCVPointStream< Real >::nextPoint( Point3D< Real >& p , Point3D< Real >& n )
{
    while (curr_ind < pnt_cnt && !gcvpnts[curr_ind]) curr_ind++;
    if (curr_ind == pnt_cnt) return false;
    p[0] = gcvpnts[curr_ind]->p[0] , p[1] = gcvpnts[curr_ind]->p[1] , p[2] = gcvpnts[curr_ind]->p[2];
    n[0] = gcvpnts[curr_ind]->n[0] , n[1] = gcvpnts[curr_ind]->n[1] , n[2] = gcvpnts[curr_ind]->n[2];
    curr_ind++;
    return true;
}

template< class Real >
class PossRecVertex
{
    public:
	const static int Components=3;

	Point3D< Real > point;

	PossRecVertex( void ) { ; }
	PossRecVertex( Point3D< Real > p ) { point=p; }
	PossRecVertex operator + ( PossRecVertex p ) const { return PossRecVertex( point+p.point ); }
	PossRecVertex operator - ( PossRecVertex p ) const { return PossRecVertex( point-p.point ); }
	template< class _Real > PossRecVertex operator * ( _Real s ) const { return PossRecVertex( point*s ); }
	template< class _Real > PossRecVertex operator / ( _Real s ) const { return PossRecVertex( point/s ); }
	PossRecVertex& operator += ( PossRecVertex p ) { point += p.point ; return *this; }
	PossRecVertex& operator -= ( PossRecVertex p ) { point -= p.point ; return *this; }
	template< class _Real > PossRecVertex& operator *= ( _Real s ) { point *= s ; return *this; }
	template< class _Real > PossRecVertex& operator /= ( _Real s ) { point /= s ; return *this; }
};
template< class Real, class _Real >
PossRecVertex< Real > operator * ( XForm4x4< _Real > xForm , PossRecVertex< Real > v ) { return PossRecVertex< Real >( xForm * v.point ); }

template< class Real >
void SetIsoVertexValue( PossRecVertex< float >& vertex , Real value ){ ; }


#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wunused-variable"
#endif
void DumpOutput( const char* format , ... ) {}
void DumpOutput2( char* str , const char* format , ... ) {}
#include "MultiGridOctreeData.h"
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

int
PoissonBuild(int **faces, int *num_faces, point_t **points, int *num_pnts, struct gcvpnt **gcvpnts, int cnt)
{
    XForm4x4< double > xForm;
    xForm = XForm4x4< double >::Identity();
    Octree< double > tree;
    tree.threads = bu_avail_cpus();
    OctNode< TreeNodeData >::SetAllocator( MEMORY_ALLOCATOR_BLOCK_SIZE );
    double maxMemoryUsage;
    Octree< double >::PointInfo* pointInfo = new Octree< double >::PointInfo();
    Octree< double >::NormalInfo* normalInfo = new Octree< double >::NormalInfo();
    std::vector< double >* kernelDensityWeights = new std::vector< double >();
    std::vector< double >* centerWeights = new std::vector< double >();
    PointStream< float >* pointStream = new GCVPointStream< float >( gcvpnts, cnt );
    int pointCount = tree.SetTree< float >( pointStream , 0 , 8 , DEFAULT_FULL_DEPTH , 6 , 1.0 , 1.1, 0 , 0 , 4.0 , 1 , *pointInfo , *normalInfo , *kernelDensityWeights , *centerWeights , 1 , xForm , 0 );
    delete kernelDensityWeights;
    kernelDensityWeights = NULL;
    Pointer( double ) constraints = tree.SetLaplacianConstraints( *normalInfo );
    delete normalInfo;
    Pointer( double ) solution = tree.SolveSystem( *pointInfo , constraints , 0 , 8 , 8 , 0 , float(1e-3) );
    delete pointInfo;
    FreePointer(constraints);
    CoredFileMeshData< PossRecVertex <float> > mesh;
    double isoValue = tree.GetIsoValue( solution , *centerWeights );
    tree.GetMCIsoSurface( NullPointer< double >() , solution , isoValue , mesh , true , 1 , 0 );
    /* mesh to BRL-CAD triangles */
    (*num_pnts) = int(mesh.outOfCorePointCount()+mesh.inCorePoints.size());
    (*num_faces) = mesh.polygonCount();
    (*points) = (point_t *)bu_calloc((*num_pnts) + 1, sizeof(point_t), "final points array");
    (*faces) = (int *)bu_calloc(3*3*(*num_faces) + 1, sizeof(point_t), "final points array");
    bu_log("Point cnt: %d\n", *num_pnts);
    bu_log("Face cnt: %d\n", *num_faces);
    int pnt_ind = 0;
    mesh.resetIterator();
    for (int i = 0 ; i < int(mesh.inCorePoints.size()); i++) {
	PossRecVertex<float> vertex = mesh.inCorePoints[i];
	VSET((*points)[pnt_ind], vertex.point[0], vertex.point[1], vertex.point[2]);
	//bu_log("pt: %f, %f, %f\n", (*points)[pnt_ind][0], (*points)[pnt_ind][1], (*points)[pnt_ind][2]);
	pnt_ind++;
    }
    for (int i = 0 ; i < mesh.outOfCorePointCount(); i++) {
	PossRecVertex<float> vertex;
	mesh.nextOutOfCorePoint(vertex);
	VSET((*points)[pnt_ind], vertex.point[0], vertex.point[1], vertex.point[2]);
	//bu_log("pt: %f, %f, %f\n", (*points)[pnt_ind][0], (*points)[pnt_ind][1], (*points)[pnt_ind][2]);
	pnt_ind++;
    }
    std::vector< CoredVertexIndex > polygon;
    for (int i = 0; i < (*num_faces); i++) {
	mesh.nextPolygon(polygon);
	(*faces)[i*3] = (polygon[0].inCore) ? polygon[0].idx : polygon[0].idx + int(mesh.inCorePoints.size());
	(*faces)[i*3+1] = (polygon[1].inCore) ? polygon[1].idx : polygon[1].idx + int(mesh.inCorePoints.size());
	(*faces)[i*3+2] = (polygon[2].inCore) ? polygon[2].idx : polygon[2].idx + int(mesh.inCorePoints.size());
	//bu_log("face: %d, %d, %d\n", (*faces)[i*3], (*faces)[i*3+1], (*faces)[i*3+2]);
    }
}

struct rt_parallel_container {
    struct rt_i *rtip;
    struct resource *resp;
    struct bu_vls *logs;
    struct gcv_point_container *npts;
    int ray_dir;
    int ncpus;
    fastf_t delta;
};

/* add all hit point info to info list */
HIDDEN int
add_hit_pnts(struct application *app, struct partition *partH, struct seg *UNUSED(segs))
{

    struct partition *pp;
    struct soltab *stp;
    /*point_t hit_pnt;
    vect_t hit_normal;*/
    struct gcv_point_container *c = (struct gcv_point_container *)(app->a_uptr);
    struct npoints *npt;

    if (c->pnt_cnt > c->capacity-1) {
	c->capacity *= 4;
	c->pts = (struct npoints *)bu_realloc((char *)c->pts, c->capacity * sizeof(struct npoints), "enlarge results array");
    }

    RT_CK_APPLICATION(app);
    /*struct bu_vls *fp = (struct bu_vls *)(app->a_uptr);*/

    /* add all hit points */
    for (pp = partH->pt_forw; pp != partH; pp = pp->pt_forw) {

	npt = &(c->pts[c->pnt_cnt]);

	/* add "in" hit point info */
	stp = pp->pt_inseg->seg_stp;

	/* hack fix for bad tgc surfaces */
	if (bu_strncmp("rec", stp->st_meth->ft_label, 3) == 0 || bu_strncmp("tgc", stp->st_meth->ft_label, 3) == 0) {

	    /* correct invalid surface number */
	    if (pp->pt_inhit->hit_surfno < 1 || pp->pt_inhit->hit_surfno > 3) {
		pp->pt_inhit->hit_surfno = 2;
	    }
	    if (pp->pt_outhit->hit_surfno < 1 || pp->pt_outhit->hit_surfno > 3) {
		pp->pt_outhit->hit_surfno = 2;
	    }
	}


	VJOIN1(npt->in.p, app->a_ray.r_pt, pp->pt_inhit->hit_dist, app->a_ray.r_dir);
	RT_HIT_NORMAL(npt->in.n, pp->pt_inhit, stp, &(app->a_ray), pp->pt_inflip);
	npt->in.is_set = 1;
	//bu_vls_printf(fp, "%f %f %f %f %f %f\n", hit_pnt[0], hit_pnt[1], hit_pnt[2], hit_normal[0], hit_normal[1], hit_normal[2]);
	/* add "out" hit point info (unless half-space) */
	stp = pp->pt_inseg->seg_stp;
	if (bu_strncmp("half", stp->st_meth->ft_label, 4) != 0) {
	    VJOIN1(npt->out.p, app->a_ray.r_pt, pp->pt_outhit->hit_dist, app->a_ray.r_dir);
	    RT_HIT_NORMAL(npt->out.n, pp->pt_outhit, stp, &(app->a_ray), pp->pt_outflip);
	    npt->out.is_set = 1;
	    //bu_vls_printf(fp, "%f %f %f %f %f %f\n", hit_pnt[0], hit_pnt[1], hit_pnt[2], hit_normal[0], hit_normal[1], hit_normal[2]);
	}
	c->pnt_cnt++;
    }
    return 1;
}

/* don't care about misses */
HIDDEN int
ignore_miss(struct application *app)
{
    RT_CK_APPLICATION(app);
    //bu_log("miss!\n");
    return 0;
}

void
_rt_gen_worker(int cpu, void *ptr)
{
    int i, j;
    struct application ap;
    struct rt_parallel_container *state = (struct rt_parallel_container *)ptr;
    point_t min, max;
    int ymin, ymax;
    int dir1, dir2, dir3;
    fastf_t d[3];
    int n[3];
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = state->rtip;
    ap.a_hit = add_hit_pnts;
    ap.a_miss = ignore_miss;
    ap.a_onehit = 0;
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_resource = &state->resp[cpu];
    ap.a_uptr = (void *)(&state->npts[cpu]);

    /* get min and max points of bounding box */
    VMOVE(min, state->rtip->mdl_min);
    VMOVE(max, state->rtip->mdl_max);

    /* Make sure we've got at least 10 steps in all 3 dimensions,
     * regardless of delta */
    for (i = 0; i < 3; i++) {
	n[i] = (max[i] - min[i])/state->delta;
	if(n[i] < 10) n[i] = 10;
	d[i] = (max[i] - min[i])/n[i];
    }

    dir1 = state->ray_dir;
    dir2 = (state->ray_dir+1)%3;
    dir3 = (state->ray_dir+2)%3;

    if (state->ncpus == 1) {
	ymin = 0;
	ymax = n[dir3];
    } else {
	ymin = n[dir3]/state->ncpus * (cpu - 1) + 1;
	ymax = n[dir3]/state->ncpus * (cpu);
	if (cpu == 1) ymin = 0;
	if (cpu == state->ncpus) ymax = n[dir3];
    }

    ap.a_ray.r_dir[state->ray_dir] = -1;
    ap.a_ray.r_dir[dir2] = 0;
    ap.a_ray.r_dir[dir3] = 0;
    VMOVE(ap.a_ray.r_pt, min);
    ap.a_ray.r_pt[state->ray_dir] = max[state->ray_dir] + 100;

    for (i = 0; i < n[dir2]; i++) {
	ap.a_ray.r_pt[dir3] = min[dir3] + d[dir3] * ymin;
	for (j = ymin; j < ymax; j++) {
	    rt_shootray(&ap);
	    ap.a_ray.r_pt[dir3] += d[dir3];
	}
	ap.a_ray.r_pt[dir2] += d[dir2];
    }
}


HIDDEN int
_rt_generate_points(int **faces, int *num_faces, point_t **points, int *num_pnts, struct bu_ptbl *hit_pnts, struct db_i *dbip, const char *obj, fastf_t delta)
{
    int i, dir1, j;
    point_t min, max;
    int ncpus = bu_avail_cpus();
    struct rt_parallel_container *state;
    struct bu_vls vlsstr;
    bu_vls_init(&vlsstr);

    if (!hit_pnts || !dbip || !obj) return -1;

    BU_GET(state, struct rt_parallel_container);

    state->rtip = rt_new_rti(dbip);
    state->delta = delta;

    if (rt_gettree(state->rtip, obj) < 0) return -1;
    rt_prep_parallel(state->rtip, 1);

    state->resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "resources");
    for (i = 0; i < ncpus+1; i++) {
	rt_init_resource(&(state->resp[i]), i, state->rtip);
    }

    state->npts = (struct gcv_point_container *)bu_calloc(ncpus+1, sizeof(struct gcv_point_container), "point container arrays");
    int n[3];
    VMOVE(min, state->rtip->mdl_min);
    VMOVE(max, state->rtip->mdl_max);
    for (i = 0; i < 3; i++) {
	n[i] = (int)((max[i] - min[i])/state->delta) + 2;
	if(n[i] < 12) n[i] = 12;
    }
    int total = 0;
    for (i = 0; i < 3; i++) total += n[i]*n[(i+1)%3];
    if (total > 1e6) total = 1e6;
    for (i = 0; i < ncpus+1; i++) {
	state->npts[i].pts = (struct npoints *)bu_calloc(total, sizeof(struct npoints), "npoints arrays");
	state->npts[i].pnt_cnt = 0;
	state->npts[i].capacity = total;
    }

    for (dir1 = 0; dir1 < 3; dir1++) {
	state->ray_dir = dir1;
	state->ncpus = ncpus;
	state->delta = delta;
	bu_parallel(_rt_gen_worker, ncpus, (void *)state);
    }
    struct bu_vls log = BU_VLS_INIT_ZERO;

    int out_cnt = 0;
    for (i = 0; i < ncpus+1; i++) {
	bu_log("%d, pnt_cnt: %d\n", i, state->npts[i].pnt_cnt);
	for (j = 0; j < state->npts[i].pnt_cnt; j++) {
	    struct npoints *npt = &(state->npts[i].pts[j]);
	    if (npt->in.is_set) out_cnt++;
	    if (npt->out.is_set) out_cnt++;
	}
    }

    struct gcvpnt **gcvpnts = (struct gcvpnt **)bu_calloc(out_cnt, sizeof(struct gcvpnt *), "output array");
    int curr_ind = 0;
    for (i = 0; i < ncpus+1; i++) {
	for (j = 0; j < state->npts[i].pnt_cnt; j++) {
	    struct npoints *npt = &(state->npts[i].pts[j]);
	    if (npt->in.is_set) {
		gcvpnts[curr_ind] = &(npt->in);
		curr_ind++;
	    }
	    if (npt->out.is_set) {
		gcvpnts[curr_ind] = &(npt->out);
		curr_ind++;
	    }
	}
    }

    (void)PoissonBuild(faces, num_faces, points, num_pnts, gcvpnts, out_cnt);

    return 0;
}

extern "C" void
gcv_generate_mesh(int **faces, int *num_faces, point_t **points, int *num_pnts,
	struct db_i *dbip, const char *obj, fastf_t delta)
{
    fastf_t d = delta;
    struct bu_ptbl *hit_pnts;
    if (!faces || !num_faces || !points || !num_pnts) return;
    if (!dbip || !obj) return;
    BU_GET(hit_pnts, struct bu_ptbl);
    bu_ptbl_init(hit_pnts, 64, "hit pnts");
    if (NEAR_ZERO(d, SMALL_FASTF)) d = 1;
    if (_rt_generate_points(faces, num_faces, points, num_pnts, hit_pnts, dbip, obj, d)) {
	(*num_faces) = 0;
	(*num_pnts) = 0;
	return;
    }
}
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

