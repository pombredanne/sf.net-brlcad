#include "common.h"

#include <string.h> /* for memset */

extern "C" {
#include "vmath.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bn/mat.h"
#include "raytrace.h"
#include "analyze.h"
#include "./analyze_private.h"
}

/* Note that we are only looking for gaps within the solid partitions of p - curr_comb is the
 * context we are examining for subtractions, and impacts beyond it are of no concern here.*/
HIDDEN int
find_missing_gaps(struct bu_ptbl *UNUSED(missing), struct bu_ptbl *UNUSED(p_brep), struct bu_ptbl *UNUSED(p_comb), int UNUSED(max_cnt))
{
//1. Set up pointer arrays for both partition lists
//
//2. Iterate over the tbls and assign each array it's minimal partition pointer
//   based on the index stored in the minimal_partitions struct.
//
//3. Look for pointers that are NULL in p_orig array but non-null in p array -
//   that indicates removed material. For rays that have minimal_partitions in
//   both arrays, compare their gaps.  If there is a p_orig gap that exits the
//   gap at less than the smallest hit distance in p->hits or enters a gap at
//   greater than the max in p->hits, it is out of scope.  Otherwise, look for
//   it in the p->gaps array.  If it isn't there, it constitutes a missing
//   gap.  A new minimal_partitions struct is created and all gaps not found
//   are recorded in it (along with the ray information and index) and the
//   process is repeated for all rays.
//
//4. return the length of the missing bu_ptbl.
    return 0;
}

HIDDEN void
plot_min_partitions(struct bu_ptbl *p, const char *cname)
{
    struct bu_vls name;
    bu_vls_init(&name);
    bu_vls_printf(&name, "hits_%s", cname);
    FILE* plot_file = fopen(bu_vls_addr(&name), "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    pl_color(plot_file, r, g, b);
    for (size_t i = 0; i < BU_PTBL_LEN(p); i++) {
	struct minimal_partitions *mp = (struct minimal_partitions *)BU_PTBL_GET(p, i);
	for (int j = 0; j < mp->hit_cnt * 2; j=j+2) {
	    point_t p1, p2;
	    VJOIN1(p1, mp->ray.r_pt, mp->hits[j], mp->ray.r_dir);
	    VJOIN1(p2, mp->ray.r_pt, mp->hits[j+1], mp->ray.r_dir);
	    pdv_3move(plot_file, p1);
	    pdv_3cont(plot_file, p2);
	}
    }
    fclose(plot_file);
}

/* Pass in the parent brep rt prep and non-finalized comb prep to avoid doing them multiple times.
 * */
extern "C" int
analyze_find_subtracted(struct bu_ptbl *UNUSED(results), struct db_i *dbip, const char *pbrep, struct rt_gen_worker_vars *pbrep_rtvars, const char *curr_comb, struct bu_ptbl *candidates, void *data, int pcpus)
{
    struct rt_gen_worker_vars *ccomb_vars;
    struct resource *ccomb_resp;
    struct rt_i *ccomb_rtip;
    //size_t ncpus = bu_avail_cpus();
    size_t ncpus = (size_t)pcpus;
    struct subbrep_object_data *curr_union_data = (struct subbrep_object_data *)data;
    //const ON_Brep *brep = curr_union_data->brep;
    size_t i = 0;
    //struct bn_tol tol = {BN_TOL_MAGIC, 0.5, 0.5 * 0.5, 1.0e-6, 1.0 - 1.0e-6 };

    if (!curr_union_data) return 0;

    // Parent brep prep is worth passing in, but curr_comb should be prepped here, since we're iterating
    // over all candidates.  Might be worth caching *all* preps on all objects for later more efficient
    // diff validation processing, but that needs more thought
    ccomb_vars = (struct rt_gen_worker_vars *)bu_calloc(ncpus+1, sizeof(struct rt_gen_worker_vars ), "ccomb state");
    ccomb_resp = (struct resource *)bu_calloc(ncpus+1, sizeof(struct resource), "ccomb resources");
    ccomb_rtip = rt_new_rti(dbip);
    for (i = 0; i < ncpus+1; i++) {
	ccomb_vars[i].rtip = ccomb_rtip;
	ccomb_vars[i].resp = &ccomb_resp[i];
	rt_init_resource(ccomb_vars[i].resp, i, ccomb_rtip);
    }
    if (rt_gettree(ccomb_rtip, curr_comb) < 0) {
	// TODO - free memory
	return 0;
    }
    rt_prep_parallel(ccomb_rtip, ncpus);

    for (i = 0; i < BU_PTBL_LEN(candidates); i++) {
	point_t bmin, bmax;
	int ray_cnt = 0;
	fastf_t *candidate_rays = NULL;
	struct subbrep_object_data *candidate = (struct subbrep_object_data *)BU_PTBL_GET(candidates, i);

	bu_log("Testing %s against %s\n", bu_vls_addr(candidate->name_root), bu_vls_addr(curr_union_data->name_root));

	// 1. Get the subbrep_bbox.

	if (!candidate->bbox_set) {
	    bu_log("How did we call this a candidate without doing the bbox calculation already?\n");
	    subbrep_bbox(candidate);
	}
	VMOVE(bmin, candidate->bbox->Min());
	VMOVE(bmax, candidate->bbox->Max());
	fastf_t x_dist = fabs(bmax[0] - bmin[0]);
	fastf_t y_dist = fabs(bmax[1] - bmin[1]);
	fastf_t z_dist = fabs(bmax[2] - bmin[2]);
	fastf_t dmin = (x_dist < y_dist) ? x_dist : y_dist;
	dmin = (z_dist < dmin) ? z_dist : dmin;
	struct bn_tol tol = {BN_TOL_MAGIC, dmin/10, dmin/10 * dmin/10, 1.0e-6, 1.0 - 1.0e-6 };

	// Construct the rays to shoot from the bbox  (TODO what tol?)
	ray_cnt = analyze_get_bbox_rays(&candidate_rays, bmin, bmax, &tol);

	// 2. The rays come from the dimensions of the candidate bbox, but
	//    first check to see whether the original and the ccomb have any
	//    disagreement about what should be there in this particular zone.  If
	//    they don't, then we don't need to prep and shoot this candidate at
	//    all.  If they do, we need to know what the disagreement is in order
	//    to determine if this candidate resolves it.

	struct bu_ptbl o_brep_results = BU_PTBL_INIT_ZERO;
	struct bu_ptbl curr_comb_results = BU_PTBL_INIT_ZERO;

	bu_log("Original brep:\n");
	analyze_get_solid_partitions(&o_brep_results, pbrep_rtvars, candidate_rays, ray_cnt, dbip, pbrep, &tol, pcpus);
	struct bu_vls tmp_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&tmp_name, "%s-%s_%s.pl", pbrep, bu_vls_addr(curr_union_data->name_root), bu_vls_addr(candidate->name_root));
	plot_min_partitions(&o_brep_results, bu_vls_addr(&tmp_name));
	bu_log("Control comb: %s\n", curr_comb);
	analyze_get_solid_partitions(&curr_comb_results, ccomb_vars, candidate_rays, ray_cnt, dbip, curr_comb, &tol, pcpus);
	bu_vls_sprintf(&tmp_name, "%s-%s_%s-ccomb.pl", pbrep, bu_vls_addr(curr_union_data->name_root), bu_vls_addr(candidate->name_root));
	plot_min_partitions(&curr_comb_results, bu_vls_addr(&tmp_name));
	//
	// 3. Compare the two partition/gap sets that result.
	struct bu_ptbl missing = BU_PTBL_INIT_ZERO;
	int missing_gaps = find_missing_gaps(&missing, &o_brep_results, &curr_comb_results, ray_cnt);
	
	// 4.  If there are missing gaps in curr_comb, prep candidate and
	// shoot the same rays against it.  If candidate 1) removes either all or a
	// subset of the missing material and 2) does not remove material that is
	// present in the results from the original brep, it is subtracted.  Add it
	// to the results set.
	if (missing_gaps) {
	}
    }
    // Once all candidates are processed, return the BU_PTBL_LEN of results.
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
