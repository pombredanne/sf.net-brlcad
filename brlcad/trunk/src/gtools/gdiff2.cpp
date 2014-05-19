/*                    G D I F F 2 . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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

#include "common.h"

extern "C" {
#include <string.h>

#include "tcl.h"

#include "bu/getopt.h"
#include "raytrace.h"
#include "rt/db_diff.h"
}

/*******************************************************************/
/* Structure and memory management for state management - this holds
 * user supplied options and output logs */
/*******************************************************************/
struct diff_state {
    int return_added;
    int return_removed;
    int return_changed;
    int return_unchanged;
    int return_conflicts;
    int have_search_filter;
    int verbosity;
    int output_mode;
    float diff_tolerance;
    struct bu_vls *diff_log;
    struct bu_vls *search_filter;
};

void diff_state_init(struct diff_state *state) {
    state->return_added = -1;
    state->return_removed = -1;
    state->return_changed = -1;
    state->return_conflicts = 1;
    state->return_unchanged = 0;
    state->have_search_filter = 0;
    state->verbosity = 2;
    state->output_mode = 0;
    state->diff_tolerance = RT_LEN_TOL;
    BU_GET(state->diff_log, struct bu_vls);
    BU_GET(state->search_filter, struct bu_vls);
    bu_vls_init(state->diff_log);
    bu_vls_init(state->search_filter);
}

void diff_state_free(struct diff_state *state) {
    bu_vls_free(state->diff_log);
    bu_vls_free(state->search_filter);
    BU_PUT(state->diff_log, struct bu_vls);
    BU_PUT(state->search_filter, struct bu_vls);
}

/*******************************************************************/
/* Structure and memory management for the container used to hold
 * diff results for related differing objects */
/*******************************************************************/
struct diff_result_container {
    int status;
    struct db_i *dbip_orig;
    struct db_i *dbip_new;
    const struct directory *dp_orig;
    const struct directory *dp_new;
    int internal_diff;
    struct bu_attribute_value_set internal_shared;
    struct bu_attribute_value_set internal_orig_only;
    struct bu_attribute_value_set internal_new_only;
    struct bu_attribute_value_set internal_orig_diff;
    struct bu_attribute_value_set internal_new_diff;
    int attribute_diff;
    struct bu_attribute_value_set additional_shared;
    struct bu_attribute_value_set additional_orig_only;
    struct bu_attribute_value_set additional_new_only;
    struct bu_attribute_value_set additional_orig_diff;
    struct bu_attribute_value_set additional_new_diff;
};


static void
diff_result_init(struct diff_result_container *result)
{
    result->status = 0;
    result->internal_diff = 0;
    result->attribute_diff = 0;
    result->dbip_orig = DBI_NULL;
    result->dbip_new = DBI_NULL;
    result->dp_orig = RT_DIR_NULL;
    result->dp_new = RT_DIR_NULL;
    BU_AVS_INIT(&result->internal_shared);
    BU_AVS_INIT(&result->internal_orig_only);
    BU_AVS_INIT(&result->internal_new_only);
    BU_AVS_INIT(&result->internal_orig_diff);
    BU_AVS_INIT(&result->internal_new_diff);
    BU_AVS_INIT(&result->additional_shared);
    BU_AVS_INIT(&result->additional_orig_only);
    BU_AVS_INIT(&result->additional_new_only);
    BU_AVS_INIT(&result->additional_orig_diff);
    BU_AVS_INIT(&result->additional_new_diff);
}


static void
diff_result_free(void *result)
{
    struct diff_result_container *curr_result = (struct diff_result_container *)result;

    if (!result)
	return;

    bu_avs_free(&curr_result->internal_shared);
    bu_avs_free(&curr_result->internal_orig_only);
    bu_avs_free(&curr_result->internal_new_only);
    bu_avs_free(&curr_result->internal_orig_diff);
    bu_avs_free(&curr_result->internal_new_diff);
    bu_avs_free(&curr_result->additional_shared);
    bu_avs_free(&curr_result->additional_orig_only);
    bu_avs_free(&curr_result->additional_new_only);
    bu_avs_free(&curr_result->additional_orig_diff);
    bu_avs_free(&curr_result->additional_new_diff);
}

struct diff3_result_container {
    int status;
    const struct db_i *ancestor_dbip;
    const struct db_i *left_dbip;
    const struct db_i *right_dbip;
    const struct directory *ancestor_dp;
    const struct directory *left_dp;
    const struct directory *right_dp;
    int internal_diff;
    struct bu_attribute_value_set param_unchanged;
    struct bu_attribute_value_set param_removed_left_only;
    struct bu_attribute_value_set param_removed_right_only;
    struct bu_attribute_value_set param_removed_both;
    struct bu_attribute_value_set param_added_left_only;
    struct bu_attribute_value_set param_added_right_only;
    struct bu_attribute_value_set param_added_both;
    struct bu_attribute_value_set param_added_conflict_left;
    struct bu_attribute_value_set param_added_conflict_right;
    struct bu_attribute_value_set param_changed_left_only;
    struct bu_attribute_value_set param_changed_right_only;
    struct bu_attribute_value_set param_changed_both;
    struct bu_attribute_value_set param_changed_conflict_ancestor;
    struct bu_attribute_value_set param_changed_conflict_left;
    struct bu_attribute_value_set param_changed_conflict_right;
    struct bu_attribute_value_set param_merged;
    int attribute_diff;
    struct bu_attribute_value_set attribute_unchanged;
    struct bu_attribute_value_set attribute_removed_left_only;
    struct bu_attribute_value_set attribute_removed_right_only;
    struct bu_attribute_value_set attribute_removed_both;
    struct bu_attribute_value_set attribute_added_left_only;
    struct bu_attribute_value_set attribute_added_right_only;
    struct bu_attribute_value_set attribute_added_both;
    struct bu_attribute_value_set attribute_added_conflict_left;
    struct bu_attribute_value_set attribute_added_conflict_right;
    struct bu_attribute_value_set attribute_changed_left_only;
    struct bu_attribute_value_set attribute_changed_right_only;
    struct bu_attribute_value_set attribute_changed_both;
    struct bu_attribute_value_set attribute_changed_conflict_ancestor;
    struct bu_attribute_value_set attribute_changed_conflict_left;
    struct bu_attribute_value_set attribute_changed_conflict_right;
    struct bu_attribute_value_set attribute_merged;
};

static void
diff3_result_init(struct diff3_result_container *result)
{
    result->status = 0;
    result->internal_diff = 0;
    result->attribute_diff = 0;
    result->ancestor_dbip = DBI_NULL;
    result->left_dbip = DBI_NULL;
    result->right_dbip = DBI_NULL;
    result->ancestor_dp = RT_DIR_NULL;
    result->left_dp = RT_DIR_NULL;
    result->right_dp = RT_DIR_NULL;
    BU_AVS_INIT(&result->param_unchanged);
    BU_AVS_INIT(&result->param_removed_left_only);
    BU_AVS_INIT(&result->param_removed_right_only);
    BU_AVS_INIT(&result->param_removed_both);
    BU_AVS_INIT(&result->param_added_left_only);
    BU_AVS_INIT(&result->param_added_right_only);
    BU_AVS_INIT(&result->param_added_both);
    BU_AVS_INIT(&result->param_added_conflict_left);
    BU_AVS_INIT(&result->param_added_conflict_right);
    BU_AVS_INIT(&result->param_changed_left_only);
    BU_AVS_INIT(&result->param_changed_right_only);
    BU_AVS_INIT(&result->param_changed_both);
    BU_AVS_INIT(&result->param_changed_conflict_ancestor);
    BU_AVS_INIT(&result->param_changed_conflict_left);
    BU_AVS_INIT(&result->param_changed_conflict_right);
    BU_AVS_INIT(&result->param_merged);
    BU_AVS_INIT(&result->attribute_unchanged);
    BU_AVS_INIT(&result->attribute_removed_left_only);
    BU_AVS_INIT(&result->attribute_removed_right_only);
    BU_AVS_INIT(&result->attribute_removed_both);
    BU_AVS_INIT(&result->attribute_added_left_only);
    BU_AVS_INIT(&result->attribute_added_right_only);
    BU_AVS_INIT(&result->attribute_added_both);
    BU_AVS_INIT(&result->attribute_added_conflict_left);
    BU_AVS_INIT(&result->attribute_added_conflict_right);
    BU_AVS_INIT(&result->attribute_changed_left_only);
    BU_AVS_INIT(&result->attribute_changed_right_only);
    BU_AVS_INIT(&result->attribute_changed_both);
    BU_AVS_INIT(&result->attribute_changed_conflict_ancestor);
    BU_AVS_INIT(&result->attribute_changed_conflict_left);
    BU_AVS_INIT(&result->attribute_changed_conflict_right);
    BU_AVS_INIT(&result->attribute_merged);

}

static void
diff3_result_free(void *curr_result)
{
    struct diff3_result_container *result = (struct diff3_result_container *)curr_result;

    if (!result)
	return;

    bu_avs_free(&result->param_unchanged);
    bu_avs_free(&result->param_removed_left_only);
    bu_avs_free(&result->param_removed_right_only);
    bu_avs_free(&result->param_removed_both);
    bu_avs_free(&result->param_added_left_only);
    bu_avs_free(&result->param_added_right_only);
    bu_avs_free(&result->param_added_both);
    bu_avs_free(&result->param_added_conflict_left);
    bu_avs_free(&result->param_added_conflict_right);
    bu_avs_free(&result->param_changed_left_only);
    bu_avs_free(&result->param_changed_right_only);
    bu_avs_free(&result->param_changed_both);
    bu_avs_free(&result->param_changed_conflict_ancestor);
    bu_avs_free(&result->param_changed_conflict_left);
    bu_avs_free(&result->param_changed_conflict_right);
    bu_avs_free(&result->param_merged);
    bu_avs_free(&result->attribute_unchanged);
    bu_avs_free(&result->attribute_removed_left_only);
    bu_avs_free(&result->attribute_removed_right_only);
    bu_avs_free(&result->attribute_removed_both);
    bu_avs_free(&result->attribute_added_left_only);
    bu_avs_free(&result->attribute_added_right_only);
    bu_avs_free(&result->attribute_added_both);
    bu_avs_free(&result->attribute_added_conflict_left);
    bu_avs_free(&result->attribute_added_conflict_right);
    bu_avs_free(&result->attribute_changed_left_only);
    bu_avs_free(&result->attribute_changed_right_only);
    bu_avs_free(&result->attribute_changed_both);
    bu_avs_free(&result->attribute_changed_conflict_ancestor);
    bu_avs_free(&result->attribute_changed_conflict_left);
    bu_avs_free(&result->attribute_changed_conflict_right);
    bu_avs_free(&result->attribute_merged);

}



/*******************************************************************/
/* Structure and memory management for the container used to hold
 * diff results */
/*******************************************************************/

struct diff_results {
    float diff_tolerance;
    struct bu_ptbl *added;     /* directory pointers */
    struct bu_ptbl *removed;   /* directory pointers */
    struct bu_ptbl *unchanged; /* directory pointers */
    struct bu_ptbl *changed;   /* result containers */
    struct bu_ptbl *changed_ancestor_dbip;   /* directory pointers */
    struct bu_ptbl *changed_new_dbip_1;   /* directory pointers */
};

void diff_results_init(struct diff_results *results){

    BU_GET(results->added, struct bu_ptbl);
    BU_GET(results->removed, struct bu_ptbl);
    BU_GET(results->changed, struct bu_ptbl);
    BU_GET(results->changed_ancestor_dbip, struct bu_ptbl);
    BU_GET(results->changed_new_dbip_1, struct bu_ptbl);
    BU_GET(results->unchanged, struct bu_ptbl);

    BU_PTBL_INIT(results->added);
    BU_PTBL_INIT(results->removed);
    BU_PTBL_INIT(results->changed);
    BU_PTBL_INIT(results->changed_ancestor_dbip);
    BU_PTBL_INIT(results->changed_new_dbip_1);
    BU_PTBL_INIT(results->unchanged);

}


void diff_results_free(struct diff_results *results)
{
    int i = 0;
    bu_ptbl_free(results->added);
    bu_ptbl_free(results->removed);
    bu_ptbl_free(results->unchanged);
    bu_ptbl_free(results->changed_ancestor_dbip);
    bu_ptbl_free(results->changed_new_dbip_1);
    for (i = 0; i < (int)BU_PTBL_LEN(results->changed); i++) {
	struct diff_result_container *result = (struct diff_result_container *)BU_PTBL_GET(results->changed, i);
	diff_result_free((void *)result);
    }
    bu_ptbl_free(results->changed);

    BU_PUT(results->added, struct bu_ptbl);
    BU_PUT(results->removed, struct bu_ptbl);
    BU_PUT(results->changed, struct bu_ptbl);
    BU_PUT(results->changed_ancestor_dbip, struct bu_ptbl);
    BU_PUT(results->changed_new_dbip_1, struct bu_ptbl);
    BU_PUT(results->unchanged, struct bu_ptbl);

}

struct diff3_results {
    float diff_tolerance;
    struct db_i *merged_db;
    struct bu_ptbl *unchanged; /* directory pointers (ancestor dbip) */
    struct bu_ptbl *removed_left_only; /* directory pointers (ancestor dbip) */
    struct bu_ptbl *removed_right_only; /* directory pointers (ancestor dbip) */
    struct bu_ptbl *removed_both; /* directory pointers (ancestor dbip) */
    struct bu_ptbl *added_left_only; /* directory pointers (left dbip) */
    struct bu_ptbl *added_right_only; /* directory pointers (right dbip) */
    struct bu_ptbl *added_both; /* directory pointers (left dbip) */
    struct bu_ptbl *added_merged; /* containers */
    struct bu_ptbl *changed; /* containers */
    struct bu_ptbl *conflict; /* containers */
};

void diff3_results_init(struct diff3_results *results){

    results->merged_db = DBI_NULL;
    results->diff_tolerance = RT_LEN_TOL;
    BU_GET(results->unchanged, struct bu_ptbl);
    BU_GET(results->removed_left_only, struct bu_ptbl);
    BU_GET(results->removed_right_only, struct bu_ptbl);
    BU_GET(results->removed_both, struct bu_ptbl);
    BU_GET(results->added_left_only, struct bu_ptbl);
    BU_GET(results->added_right_only, struct bu_ptbl);
    BU_GET(results->added_both, struct bu_ptbl);
    BU_GET(results->added_merged, struct bu_ptbl);
    BU_GET(results->changed, struct bu_ptbl);
    BU_GET(results->conflict, struct bu_ptbl);

    BU_PTBL_INIT(results->unchanged);
    BU_PTBL_INIT(results->removed_left_only);
    BU_PTBL_INIT(results->removed_right_only);
    BU_PTBL_INIT(results->removed_both);
    BU_PTBL_INIT(results->added_left_only);
    BU_PTBL_INIT(results->added_right_only);
    BU_PTBL_INIT(results->added_both);
    BU_PTBL_INIT(results->added_merged); 
    BU_PTBL_INIT(results->changed);
    BU_PTBL_INIT(results->conflict);

}

void diff3_results_free(struct diff3_results *results)
{
    int i = 0;

    bu_ptbl_free(results->unchanged);
    bu_ptbl_free(results->removed_left_only);
    bu_ptbl_free(results->removed_right_only);
    bu_ptbl_free(results->removed_both);
    bu_ptbl_free(results->added_left_only);
    bu_ptbl_free(results->added_right_only);
    bu_ptbl_free(results->added_both);
    for (i = 0; i < (int)BU_PTBL_LEN(results->added_merged); i++) {
	struct diff3_result_container *result = (struct diff3_result_container *)BU_PTBL_GET(results->added_merged, i);
	diff3_result_free((void *)result);
    } 
    for (i = 0; i < (int)BU_PTBL_LEN(results->changed); i++) {
	struct diff3_result_container *result = (struct diff3_result_container *)BU_PTBL_GET(results->changed, i);
	diff3_result_free((void *)result);
    }
    bu_ptbl_free(results->changed);
    for (i = 0; i < (int)BU_PTBL_LEN(results->conflict); i++) {
	struct diff3_result_container *result = (struct diff3_result_container *)BU_PTBL_GET(results->conflict, i);
	diff3_result_free((void *)result);
    }
    bu_ptbl_free(results->conflict);

    BU_PUT(results->unchanged, struct bu_ptbl);
    BU_PUT(results->removed_left_only, struct bu_ptbl);
    BU_PUT(results->removed_right_only, struct bu_ptbl);
    BU_PUT(results->removed_both, struct bu_ptbl);
    BU_PUT(results->added_left_only, struct bu_ptbl);
    BU_PUT(results->added_right_only, struct bu_ptbl);
    BU_PUT(results->added_both, struct bu_ptbl);
    BU_PUT(results->changed, struct bu_ptbl);
    BU_PUT(results->conflict, struct bu_ptbl);

}


/*******************************************************************/
/* Callback functions for db_compare */
/*******************************************************************/
int
diff_added(const struct db_i *UNUSED(left), const struct db_i *UNUSED(right), const struct directory *added, void *data)
{
    struct diff_results *results;

    if (!data || !added)
	return -1;

    results = (struct diff_results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->added)) BU_PTBL_INIT(results->added);

    bu_ptbl_ins(results->added, (long *)added);

    return 0;
}


int
diff_removed(const struct db_i *UNUSED(left), const struct db_i *UNUSED(right), const struct directory *removed, void *data)
{
    struct diff_results *results;

    if (!data || !removed)
	return -1;

    results = (struct diff_results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->removed)) BU_PTBL_INIT(results->removed);

    bu_ptbl_ins(results->removed, (long *)removed);

    return 0;
}


int
diff_unchanged(const struct db_i *UNUSED(left), const struct db_i *UNUSED(right), const struct directory *unchanged, void *data)
{
    struct diff_results *results;

    if (!data || !unchanged)
	return -1;

    results = (struct diff_results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->unchanged)) BU_PTBL_INIT(results->unchanged);

    bu_ptbl_ins(results->unchanged, (long *)unchanged);

    return 0;
}

int
diff_changed(const struct db_i *left, const struct db_i *right, const struct directory *before, const struct directory *after, void *data)
{
    struct diff_results *results;
    struct diff_result_container *result;
    struct bn_tol diff_tol = BN_TOL_INIT_ZERO;
    struct rt_db_internal intern_orig;
    struct rt_db_internal intern_new;

    if (!left || !right || !before || !after|| !data)
	return -1;

    results = (struct diff_results *)data;
    diff_tol.dist = results->diff_tolerance;

    if (!BU_PTBL_IS_INITIALIZED(results->changed))
	BU_PTBL_INIT(results->changed);
    if (!BU_PTBL_IS_INITIALIZED(results->changed_ancestor_dbip))
	BU_PTBL_INIT(results->changed_ancestor_dbip);
    if (!BU_PTBL_IS_INITIALIZED(results->changed_new_dbip_1))
	BU_PTBL_INIT(results->changed_new_dbip_1);

    bu_ptbl_ins(results->changed_ancestor_dbip, (long *)before);
    bu_ptbl_ins(results->changed_new_dbip_1, (long *)after);

    result = (struct diff_result_container *)bu_calloc(1, sizeof(struct diff_result_container), "diff result struct");
    diff_result_init(result);

    result->dp_orig = before;
    result->dp_new = after;

    RT_DB_INTERNAL_INIT(&intern_orig);
    RT_DB_INTERNAL_INIT(&intern_new);

    /* Get the internal objects */
    if (rt_db_get_internal(&intern_orig, before, left, (fastf_t *)NULL, &rt_uniresource) < 0) {
	result->status = 1;
	return -1;
    }
    if (rt_db_get_internal(&intern_new, after, right, (fastf_t *)NULL, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern_orig);
	result->status = 1;
	return -1;
    }

    result->internal_diff = db_compare(&intern_orig, &intern_new, DB_COMPARE_PARAM,
	    &(result->internal_new_only), &(result->internal_orig_only), &(result->internal_orig_diff),
	    &(result->internal_new_diff), &(result->internal_shared), &diff_tol);

    result->attribute_diff = db_compare(&intern_orig, &intern_new, DB_COMPARE_ATTRS,
	    &(result->additional_new_only), &(result->additional_orig_only), &(result->additional_orig_diff),
	    &(result->additional_new_diff), &(result->additional_shared), &diff_tol);

    if (result->internal_diff || result->attribute_diff) {
	bu_ptbl_ins(results->changed, (long *)result);
    } else {
	if (!BU_PTBL_IS_INITIALIZED(results->unchanged)) BU_PTBL_INIT(results->unchanged);
	bu_ptbl_ins(results->unchanged, (long *)before);
	diff_result_free(result);
    }

    rt_db_free_internal(&intern_orig);
    rt_db_free_internal(&intern_new);

    return 0;
}


/*******************************************************************/
/* Callback functions for db_compare3 */
/*******************************************************************/
int
diff3_added(const struct db_i *left, const struct db_i *ancestor, const struct db_i *right, const struct directory *dp_left, const struct directory *dp_ancestor, const struct directory *dp_right, void *data)
{
    /* The cases are:
     *
     * added in left only
     * added in right only
     * added in left and right, identically
     * added in left and right, mergeably
     * added in left and right, conflict
     */
    struct diff3_results *results;
    if (!data || !dp_ancestor)
	return -1;

    results = (struct diff3_results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->added_left_only)) BU_PTBL_INIT(results->added_left_only);
    if (!BU_PTBL_IS_INITIALIZED(results->added_right_only)) BU_PTBL_INIT(results->added_right_only);
    if (!BU_PTBL_IS_INITIALIZED(results->added_both)) BU_PTBL_INIT(results->added_both);
    if (!BU_PTBL_IS_INITIALIZED(results->changed)) BU_PTBL_INIT(results->changed);
    if (!BU_PTBL_IS_INITIALIZED(results->conflict)) BU_PTBL_INIT(results->conflict);

    if (dp_right == RT_DIR_NULL && dp_left == RT_DIR_NULL) return -1;
    if (dp_left == RT_DIR_NULL) {
	bu_ptbl_ins(results->added_right_only, (long *)dp_right);
    }
    if (dp_right == RT_DIR_NULL) {
	bu_ptbl_ins(results->added_left_only, (long *)dp_left);
    }

    if ((dp_right != RT_DIR_NULL) && (dp_left != RT_DIR_NULL)) {
	struct rt_db_internal intern_left;
	struct rt_db_internal intern_right;
	struct bn_tol diff_tol = BN_TOL_INIT_ZERO;
	struct diff3_result_container *result3 = (struct diff3_result_container *)bu_calloc(1, sizeof(struct diff3_result_container), "diff3 result struct");
	diff3_result_init(result3);
	RT_DB_INTERNAL_INIT(&intern_left);
	RT_DB_INTERNAL_INIT(&intern_right);
	diff_tol.dist = results->diff_tolerance;

	result3->ancestor_dbip = ancestor;
	result3->left_dbip = left;
	result3->right_dbip = right;
	result3->ancestor_dp = dp_ancestor;
	result3->left_dp = dp_left;
	result3->right_dp = dp_right;

	/* Get the internal objects */
	if (rt_db_get_internal(&intern_left, dp_left, left, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    return -1;
	}
	if (rt_db_get_internal(&intern_right, dp_right, right, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    rt_db_free_internal(&intern_left);
	    return -1;
	}

	result3->internal_diff = db_compare3(&intern_left, NULL, &intern_right, DB_COMPARE_PARAM, &(result3->param_unchanged),
		&(result3->param_removed_left_only), &(result3->param_removed_right_only), &(result3->param_removed_both),
		&(result3->param_added_left_only), &(result3->param_added_right_only), &(result3->param_added_both),
		&(result3->param_added_conflict_left), &(result3->param_added_conflict_right),
		&(result3->param_changed_left_only), &(result3->param_changed_right_only),
		&(result3->param_changed_both), &(result3->param_changed_conflict_ancestor),
		&(result3->param_changed_conflict_left),&(result3->param_changed_conflict_right),
		&(result3->param_merged), &diff_tol);

	result3->attribute_diff = db_compare3(&intern_left, NULL, &intern_right, DB_COMPARE_ATTRS, &(result3->attribute_unchanged),
		&(result3->attribute_removed_left_only), &(result3->attribute_removed_right_only), &(result3->attribute_removed_both),
		&(result3->attribute_added_left_only), &(result3->attribute_added_right_only), &(result3->attribute_added_both),
		&(result3->attribute_added_conflict_left), &(result3->attribute_added_conflict_right),
		&(result3->attribute_changed_left_only), &(result3->attribute_changed_right_only),
		&(result3->attribute_changed_both), &(result3->attribute_changed_conflict_ancestor),
		&(result3->attribute_changed_conflict_left),&(result3->attribute_changed_conflict_right),
		&(result3->attribute_merged), &diff_tol);

	result3->status = (result3->internal_diff > result3->attribute_diff) ? result3->internal_diff : result3->attribute_diff;

	switch (result3->status) {
	    case 0:
		/* dp_left == dp_right */
		bu_ptbl_ins(results->added_both, (long *)dp_left);
		diff3_result_free(result3);
		break;
	    case 1:
		/* dp_left != dp_right, but they can be merged */
		bu_ptbl_ins(results->added_merged, (long *)result3);
		break;
	    case 2:
	    case 3:
		/* dp_left != dp_right */
		bu_ptbl_ins(results->conflict, (long *)result3);
		break;
	    default:
		return -1;
		break;
	}
    }

    return 0;
}


int
diff3_removed(const struct db_i *UNUSED(left), const struct db_i *UNUSED(ancestor), const struct db_i *UNUSED(right), const struct directory *UNUSED(dp_left), const struct directory *UNUSED(dp_ancestor), const struct directory *UNUSED(dp_right), void *UNUSED(data))
{
    /* The cases are:
     *
     * removed in left only
     * removed in right only
     * removed in both
     *
     * Cases where a change conflicts with a removal are handled by diff3_changed
     */
    return 0;
}


int
diff3_unchanged(const struct db_i *UNUSED(left), const struct db_i *UNUSED(ancestor), const struct db_i *UNUSED(right), const struct directory *UNUSED(dp_left), const struct directory *dp_ancestor, const struct directory *UNUSED(dp_right), void *data)
{
    struct diff3_results *results;

    if (!data || !dp_ancestor)
	return -1;

    results = (struct diff3_results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->unchanged)) BU_PTBL_INIT(results->unchanged);

    bu_ptbl_ins(results->unchanged, (long *)dp_ancestor);

    return 0;
}

int
diff3_changed(const struct db_i *UNUSED(left), const struct db_i *UNUSED(ancestor), const struct db_i *UNUSED(right), const struct directory *UNUSED(dp_left), const struct directory *UNUSED(dp_ancestor), const struct directory *UNUSED(dp_right), void *UNUSED(data))
{
    /* The cases are:
     *
     * OK:
     * changed in left, right unchanged
     * changed in right, left unchanged
     * changed in left and right, identically
     * changed in left and right, mergeably
     *
     *
     * Conflicts:
     * removed in left, changed in right
     * removed in right, changed in left
     * changed in left, changed in right, conflict
     *
     *
     * Cases where additions conflict with each other are handled by diff3_added
     */

    return 0;
}


/*******************************************************************/
/* Output generators for diff log */
/*******************************************************************/
static void
params_summary(struct bu_vls *attr_log, const struct diff_result_container *result)
{
    struct bu_attribute_value_pair *avpp;
    const struct bu_attribute_value_set *orig_only = &result->internal_orig_only;
    const struct bu_attribute_value_set *new_only = &result->internal_new_only;
    const struct bu_attribute_value_set *orig_diff = &result->internal_orig_diff;
    const struct bu_attribute_value_set *new_diff = &result->internal_new_diff;

    if (orig_only->count > 0) {
	bu_vls_printf(attr_log, "   Parameters removed:\n");
	for (BU_AVS_FOR(avpp, orig_only)) {
	    bu_vls_printf(attr_log, "      %s\n", avpp->name);
	}
    }
    if (new_only->count > 0) {
	bu_vls_printf(attr_log, "   Parameters added:\n");
	for (BU_AVS_FOR(avpp, new_only)) {
	    bu_vls_printf(attr_log, "      %s: %s\n", avpp->name, avpp->value);
	}
    }
    if (orig_diff->count > 0) {
	bu_vls_printf(attr_log, "   Parameters changed:\n");
	for (BU_AVS_FOR(avpp, orig_diff)) {
	    bu_vls_printf(attr_log, "      %s: \"%s\" -> \"%s\"\n", avpp->name, avpp->value, bu_avs_get(new_diff, avpp->name));
	}
    }
}

void
avs_log(struct bu_vls *attr_log, const struct bu_attribute_value_set *avs, const char *title, const char *offset_title, const char *offset_entry)
{
    struct bu_attribute_value_pair *avpp;
    if (avs->count > 0) {
	bu_vls_printf(attr_log, "%s%s:\n", offset_title, title);
	for (BU_AVS_FOR(avpp, avs)) {
	    bu_vls_printf(attr_log, "%s%s\n", offset_entry, avpp->name);
	}
    }
}

#if 0
static void
params3_summary(struct bu_vls *attr_log, const struct diff3_result_container *result)
{
    struct bu_attribute_value_pair *avpp;
    /*const struct bu_attribute_value_set *unchanged                 = &result->param_unchanged;*/
    const struct bu_attribute_value_set *removed_left_only         = &result->param_removed_left_only;
    const struct bu_attribute_value_set *removed_right_only        = &result->param_removed_right_only;
    const struct bu_attribute_value_set *removed_both              = &result->param_removed_both;
    const struct bu_attribute_value_set *added_left_only           = &result->param_added_left_only;
    const struct bu_attribute_value_set *added_right_only          = &result->param_added_right_only;
    const struct bu_attribute_value_set *added_both                = &result->param_added_both;
    const struct bu_attribute_value_set *added_conflict_left       = &result->param_added_conflict_left;
    const struct bu_attribute_value_set *added_conflict_right      = &result->param_added_conflict_right;
    const struct bu_attribute_value_set *changed_left_only         = &result->param_changed_left_only;
    const struct bu_attribute_value_set *changed_right_only        = &result->param_changed_right_only;
    const struct bu_attribute_value_set *changed_both              = &result->param_changed_both;
    const struct bu_attribute_value_set *changed_conflict_ancestor = &result->param_changed_conflict_ancestor;
    const struct bu_attribute_value_set *changed_conflict_left     = &result->param_changed_conflict_left;
    const struct bu_attribute_value_set *changed_conflict_right    = &result->param_changed_conflict_right;
    /*const struct bu_attribute_value_set *merged                    = &result->param_merged;*/

    /* Report removed parameters */
    avs_log(attr_log, removed_left_only, "Parameters removed in left", "   ", "      ");
    avs_log(attr_log, removed_right_only, "Parameters removed in right", "   ", "      ");
    avs_log(attr_log, removed_both, "Parameters removed in both left and right", "   ", "      ");

    /* Report added parameters */
    avs_log(attr_log, added_left_only, "Parameters added in left", "   ", "      ");
    avs_log(attr_log, added_right_only, "Parameters added in right", "   ", "      ");
    avs_log(attr_log, added_both, "Parameters added in both left and right", "   ", "      ");

    /* Parameter conflicts in additions */
    if (added_conflict_left->count > 0) {
	bu_vls_printf(attr_log, "   CONFLICT: Parameters added in both left and right with different values:\n");
	for (BU_AVS_FOR(avpp, added_conflict_left)) {
	    bu_vls_printf(attr_log, "      %s: (%s,%s)\n", avpp->name, bu_avs_get(added_conflict_left, avpp->name), bu_avs_get(added_conflict_right, avpp->name));
	}
    }

    /* Report changed parameters */
    avs_log(attr_log, changed_left_only, "Parameters changed in left", "   ", "      ");
    avs_log(attr_log, changed_right_only, "Parameters changed in right", "   ", "      ");
    avs_log(attr_log, changed_both, "Parameters changed to the same value in both left and right", "   ", "      ");

    /* Parameter conflicts in changes */
    if (changed_conflict_ancestor->count > 0) {
	bu_vls_printf(attr_log, "   CONFLICT: Parameters changed in both left and right with different values:\n");
	for (BU_AVS_FOR(avpp, added_conflict_left)) {
	    bu_vls_printf(attr_log, "      %s: (%s,%s)\n", avpp->name, bu_avs_get(changed_conflict_ancestor, avpp->name), bu_avs_get(changed_conflict_left, avpp->name), bu_avs_get(changed_conflict_right, avpp->name));
	}
    }

}
#endif

static void
attrs_summary(struct bu_vls *attr_log, const struct diff_result_container *result)
{
    struct bu_attribute_value_pair *avpp;
    const struct bu_attribute_value_set *orig_only = &result->additional_orig_only;
    const struct bu_attribute_value_set *new_only = &result->additional_new_only;
    const struct bu_attribute_value_set *orig_diff = &result->additional_orig_diff;
    const struct bu_attribute_value_set *new_diff = &result->additional_new_diff;

    if (orig_only->count > 0) {
	bu_vls_printf(attr_log, "   Attributes removed:\n");
	for (BU_AVS_FOR(avpp, orig_only)) {
	    bu_vls_printf(attr_log, "      %s\n", avpp->name);
	}
    }
    if (new_only->count > 0) {
	bu_vls_printf(attr_log, "   Attributes added:\n");
	for (BU_AVS_FOR(avpp, new_only)) {
	    bu_vls_printf(attr_log, "      %s: %s\n", avpp->name, avpp->value);
	}
    }
    if (orig_diff->count > 0) {
	bu_vls_printf(attr_log, "   Attribute parameters changed:\n");
	for (BU_AVS_FOR(avpp, orig_diff)) {
	    bu_vls_printf(attr_log, "      %s: \"%s\" -> \"%s\"\n", avpp->name, avpp->value, bu_avs_get(new_diff, avpp->name));
	}
    }
}

#if 0
static void
attrs3_summary(struct bu_vls *attr_log, const struct diff3_result_container *result)
{
    struct bu_attribute_value_pair *avpp;
    /*const struct bu_attribute_value_set *unchanged                 = &result->attribute_unchanged;*/
    const struct bu_attribute_value_set *removed_left_only         = &result->attribute_removed_left_only;
    const struct bu_attribute_value_set *removed_right_only        = &result->attribute_removed_right_only;
    const struct bu_attribute_value_set *removed_both              = &result->attribute_removed_both;
    const struct bu_attribute_value_set *added_left_only           = &result->attribute_added_left_only;
    const struct bu_attribute_value_set *added_right_only          = &result->attribute_added_right_only;
    const struct bu_attribute_value_set *added_both                = &result->attribute_added_both;
    const struct bu_attribute_value_set *added_conflict_left       = &result->attribute_added_conflict_left;
    const struct bu_attribute_value_set *added_conflict_right      = &result->attribute_added_conflict_right;
    const struct bu_attribute_value_set *changed_left_only         = &result->attribute_changed_left_only;
    const struct bu_attribute_value_set *changed_right_only        = &result->attribute_changed_right_only;
    const struct bu_attribute_value_set *changed_both              = &result->attribute_changed_both;
    const struct bu_attribute_value_set *changed_conflict_ancestor = &result->attribute_changed_conflict_ancestor;
    const struct bu_attribute_value_set *changed_conflict_left     = &result->attribute_changed_conflict_left;
    const struct bu_attribute_value_set *changed_conflict_right    = &result->attribute_changed_conflict_right;
    /*const struct bu_attribute_value_set *merged                    = &result->attribute_merged;*/

    /* Report removed attributes */
    avs_log(attr_log, removed_left_only, "Attributes removed in left", "   ", "      ");
    avs_log(attr_log, removed_right_only, "Attributes removed in right", "   ", "      ");
    avs_log(attr_log, removed_both, "Attributes removed in both left and right", "   ", "      ");

    /* Report added attributes */
    avs_log(attr_log, added_left_only, "Attributes added in left", "   ", "      ");
    avs_log(attr_log, added_right_only, "Attributes added in right", "   ", "      ");
    avs_log(attr_log, added_both, "Attributes added in both left and right", "   ", "      ");

    /* Attribute conflicts in additions */
    if (added_conflict_left->count > 0) {
	bu_vls_printf(attr_log, "   CONFLICT: Attributes added in both left and right with different values:\n");
	for (BU_AVS_FOR(avpp, added_conflict_left)) {
	    bu_vls_printf(attr_log, "      %s: (%s,%s)\n", avpp->name, bu_avs_get(added_conflict_left, avpp->name), bu_avs_get(added_conflict_right, avpp->name));
	}
    }

    /* Report changed attributes */
    avs_log(attr_log, changed_left_only, "Attributes changed in left", "   ", "      ");
    avs_log(attr_log, changed_right_only, "Attributes changed in right", "   ", "      ");
    avs_log(attr_log, changed_both, "Attributes changed to the same value in both left and right", "   ", "      ");

    /* Attribute conflicts in changes */
    if (changed_conflict_ancestor->count > 0) {
	bu_vls_printf(attr_log, "   CONFLICT: Attributes changed in both left and right with different values:\n");
	for (BU_AVS_FOR(avpp, added_conflict_left)) {
	    bu_vls_printf(attr_log, "      %s: (%s,%s)\n", avpp->name, bu_avs_get(changed_conflict_ancestor, avpp->name), bu_avs_get(changed_conflict_left, avpp->name), bu_avs_get(changed_conflict_right, avpp->name));
	}
    }

}
#endif

static int
print_dp(struct bu_vls *diff_log, const struct bu_ptbl *dptable, const int cnt, const struct directory *dp, const int line_len)
{
    int local_line_len = line_len;

    if (local_line_len + strlen(dp->d_namep) > 80) {
	bu_vls_printf(diff_log, "\n");
	local_line_len = 0;
    }

    bu_vls_printf(diff_log, "%s", dp->d_namep);
    local_line_len += strlen(dp->d_namep);

    if (local_line_len > 79 || (cnt+1) == (int)BU_PTBL_LEN(dptable)) {
	bu_vls_printf(diff_log, "\n");
	local_line_len = 0;
    } else {
	bu_vls_printf(diff_log, ", ");
	local_line_len += 2;
    }

    return local_line_len;
}


static void
diff_summarize(struct bu_vls *diff_log, const struct diff_results *results, struct diff_state *state)
{
    int i = 0;

    struct bu_ptbl *added;
    struct bu_ptbl *removed;
    struct bu_ptbl *changed;
    struct bu_ptbl *unchanged;

    if (state->verbosity < 1 || !results)
	return;

    added = results->added;
    removed = results->removed;
    changed = results->changed;
    unchanged = results->unchanged;

    if (state->verbosity == 1) {
	int line_len = 0;
	if (state->return_added > 0) {
	    if ((int)BU_PTBL_LEN(added) > 0) {
		bu_vls_printf(diff_log, "\nObjects added:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(added); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(added, i);
		line_len = print_dp(diff_log, added, i, dp, line_len);
	    }
	}
	line_len = 0;
	if (state->return_removed > 0) {
	    if ((int)BU_PTBL_LEN(removed) > 0) {
		bu_vls_printf(diff_log, "\nObjects removed:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(removed); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(removed, i);
		line_len = print_dp(diff_log, removed, i, dp, line_len);
	    }
	}
	line_len = 0;
	if (state->return_changed > 0) {
	    if ((int)BU_PTBL_LEN(changed) > 0) {
		bu_vls_printf(diff_log, "\nObjects changed:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(changed); i++) {
		struct diff_result_container *result = (struct diff_result_container *)BU_PTBL_GET(changed, i);
		const struct directory *dp = result->dp_orig;
		if (!dp) dp = result->dp_new;
		line_len = print_dp(diff_log, changed, i, dp, line_len);
	    }
	}
	line_len = 0;
	if (state->return_unchanged > 0) {
	    if ((int)BU_PTBL_LEN(unchanged) > 0) {
		bu_vls_printf(diff_log, "\nObjects unchanged:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(unchanged); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(unchanged, i);
		line_len = print_dp(diff_log, unchanged, i, dp, line_len);
	    }
	}

    }
    if (state->verbosity > 1) {
	if (state->return_added > 0) {
	    for (i = 0; i < (int)BU_PTBL_LEN(added); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(added, i);
		bu_vls_printf(diff_log, "%s was added.\n\n", dp->d_namep);
	    }
	}

	if (state->return_removed > 0) {
	    for (i = 0; i < (int)BU_PTBL_LEN(removed); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(removed, i);
		bu_vls_printf(diff_log, "%s was removed.\n\n", dp->d_namep);
	    }
	}

	if (state->return_changed > 0) {
	    for (i = 0; i < (int)BU_PTBL_LEN(changed); i++) {
		struct diff_result_container *result = (struct diff_result_container *)BU_PTBL_GET(changed, i);
		const struct directory *dp = result->dp_orig;
		if (!dp) dp = result->dp_new;
		bu_vls_printf(diff_log, "%s was changed:\n", dp->d_namep);
		params_summary(diff_log, result);
		attrs_summary(diff_log, result);
		bu_vls_printf(diff_log, "\n");
	    }
	}
	if (state->return_unchanged > 0) {
	    for (i = 0; i < (int)BU_PTBL_LEN(unchanged); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(unchanged, i);
		bu_vls_printf(diff_log, "%s was unchanged.\n\n", dp->d_namep);
	    }
	}


    }
    bu_vls_printf(diff_log, "\n");
}


static void
diff3_summarize(struct bu_vls *diff_log, const struct diff3_results *results, struct diff_state *state)
{
    int i = 0;

    if (state->verbosity < 1 || !results)
	return;

    //struct bu_ptbl *unchanged          = results->unchanged;          /* directory pointers (ancestor dbip) */
    //struct bu_ptbl *removed_left_only  = results->removed_left_only;  /* directory pointers (ancestor dbip) */
    //struct bu_ptbl *removed_right_only = results->removed_right_only; /* directory pointers (ancestor dbip) */
    //struct bu_ptbl *removed_both       = results->removed_both;       /* directory pointers (ancestor dbip) */
    struct bu_ptbl *added_left_only    = results->added_left_only;    /* directory pointers (left dbip) */
    struct bu_ptbl *added_right_only   = results->added_right_only;   /* directory pointers (right dbip) */
    struct bu_ptbl *added_both         = results->added_both;         /* directory pointers (left dbip) */
    struct bu_ptbl *added_merged       = results->added_merged;       /* containers */
    //struct bu_ptbl *changed            = results->changed;            /* containers */
    //struct bu_ptbl *conflict           = results->conflict;           /* containers */

    //if (state->verbosity == 1) {
	int line_len = 0;
	if (state->return_added > 0) {
	    if ((int)BU_PTBL_LEN(added_left_only) > 0 || (int)BU_PTBL_LEN(added_right_only) > 0 || (int)BU_PTBL_LEN(added_both) > 0) {
		bu_vls_printf(diff_log, "\nObjects added:\n");
	    }
	    if ((int)BU_PTBL_LEN(added_left_only) > 0) {
		bu_vls_printf(diff_log, "From left:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(added_left_only); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(added_left_only, i);
		line_len = print_dp(diff_log, added_left_only, i, dp, line_len);
	    }
	    line_len = 0;
	    if ((int)BU_PTBL_LEN(added_right_only) > 0) {
		bu_vls_printf(diff_log, "\nFrom right:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(added_right_only); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(added_right_only, i);
		line_len = print_dp(diff_log, added_right_only, i, dp, line_len);
	    }
	    line_len = 0;
	    if ((int)BU_PTBL_LEN(added_both) > 0) {
		bu_vls_printf(diff_log, "\nFrom both left and right, identically:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(added_both); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(added_both, i);
		line_len = print_dp(diff_log, added_both, i, dp, line_len);
	    }
	    if ((int)BU_PTBL_LEN(added_merged) > 0) {
		bu_vls_printf(diff_log, "\nFrom both left and right, merged information:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(added_merged); i++) {
		struct diff3_result_container *d3 = (struct diff3_result_container *)BU_PTBL_GET(added_merged, i);
		line_len = print_dp(diff_log, added_both, i, d3->left_dp, line_len);
	    }

	}
	line_len = 0;
	if (state->return_removed > 0) {
	}
	line_len = 0;
	if (state->return_changed > 0) {
	}
	line_len = 0;
	if (state->return_unchanged > 0) {
	}

    //}

    bu_vls_printf(diff_log, "\n");
}


static void
filter_dp_ptbl(struct bu_ptbl *filtered_control, struct bu_ptbl *to_be_filtered)
{
    int i = 0;
    struct bu_ptbl tmp_tbl = BU_PTBL_INIT_ZERO;
    if (BU_PTBL_LEN(to_be_filtered) > 0) {
	bu_ptbl_cat(&tmp_tbl, to_be_filtered);
	bu_ptbl_reset(to_be_filtered);
	for (i = 0; i < (int)BU_PTBL_LEN(&tmp_tbl); i++) {
	    if (bu_ptbl_locate(filtered_control, (long *)BU_PTBL_GET(&tmp_tbl, i)) != -1) {
		bu_ptbl_ins(to_be_filtered, (long *)BU_PTBL_GET(&tmp_tbl, i));
	    }
	}
	bu_ptbl_free(&tmp_tbl);
    }
}

static void
filter_diff_container_ptbl(struct bu_ptbl *filtered_ancestor_control, struct bu_ptbl *filtered_new_control, struct bu_ptbl *to_be_filtered)
{
    int i = 0;
    struct bu_ptbl tmp_tbl = BU_PTBL_INIT_ZERO;
    if (BU_PTBL_LEN(to_be_filtered) > 0) {
	bu_ptbl_cat(&tmp_tbl, to_be_filtered);
	bu_ptbl_reset(to_be_filtered);
	for (i = 0; i < (int)BU_PTBL_LEN(&tmp_tbl); i++) {
	    struct diff_result_container *result = (struct diff_result_container *)BU_PTBL_GET(&tmp_tbl, i);
	    if ((result->dp_orig && bu_ptbl_locate(filtered_ancestor_control, (long *)result->dp_orig) != -1) ||
		    (result->dp_new && bu_ptbl_locate(filtered_new_control, (long *)result->dp_orig) != -1)) {
		bu_ptbl_ins(to_be_filtered, (long *)result);
	    } else {
		diff_result_free((void *)result);
	    }
	}
	bu_ptbl_free(&tmp_tbl);
    }
}
/*******************************************************************/
/* Primary function for basic diff operation on two .g files */
/*******************************************************************/
static int
do_diff(struct db_i *ancestor_dbip, struct db_i *new_dbip_1, struct diff_state *state) {
    int have_diff = 0;
    int diff_return = 0;
    struct diff_results *results;

    BU_GET(results, struct diff_results);
    diff_results_init(results);
    results->diff_tolerance = state->diff_tolerance;

    have_diff = db_diff(ancestor_dbip, new_dbip_1, &diff_added, &diff_removed, &diff_changed, &diff_unchanged, (void *)results);

    /* Now we have our diff results, time to filter (if applicable) and report them */
    if (state->have_search_filter) {
	/* In order to respect search filters involving depth, we have to build "allowed"
	 * sets of objects from both databases based on straight-up search filtering of
	 * the original databases.  We then check the filtered diff results against the
	 * search-filtered original databases to make sure they are valid for the final
	 * results-> */
	struct bu_ptbl ancestor_dbip_filtered = BU_PTBL_INIT_ZERO;
	struct bu_ptbl new_dbip_1_filtered = BU_PTBL_INIT_ZERO;
	(void)db_search(&ancestor_dbip_filtered, DB_SEARCH_RETURN_UNIQ_DP, (const char *)bu_vls_addr(state->search_filter), 0, NULL, ancestor_dbip);
	(void)db_search(&new_dbip_1_filtered, DB_SEARCH_RETURN_UNIQ_DP, (const char *)bu_vls_addr(state->search_filter), 0, NULL, new_dbip_1);

	/* Filter added objects */
	filter_dp_ptbl(&new_dbip_1_filtered, results->added);

	/* Filter removed objects */
	filter_dp_ptbl(&ancestor_dbip_filtered, results->removed);

	/* Filter changed objects */
	filter_diff_container_ptbl(&ancestor_dbip_filtered, &new_dbip_1_filtered, results->changed);

	/* Filter unchanged objects */
	filter_dp_ptbl(&ancestor_dbip_filtered, results->removed);

	db_search_free(&ancestor_dbip_filtered);
	db_search_free(&new_dbip_1_filtered);
    }

    if (state->verbosity) {
	if (!have_diff) {
	    bu_log("No differences found.\n");
	} else {
	    diff_summarize(state->diff_log, results, state);
	    bu_log("%s", bu_vls_addr(state->diff_log));
	}
    }
    if (have_diff) {
	if (state->return_added > 0) diff_return += BU_PTBL_LEN(results->added);
	if (state->return_removed > 0) diff_return += BU_PTBL_LEN(results->removed);
	if (state->return_changed > 0) diff_return += BU_PTBL_LEN(results->changed);
	if (state->return_unchanged > 0) diff_return += BU_PTBL_LEN(results->unchanged);
    }

    diff_results_free(results);
    BU_PUT(results, struct diff_results);

    return diff_return;
}

/*******************************************************************/
/* Primary function for diff3 operation on three .g files */
/*******************************************************************/
static int
do_diff3(struct db_i *left, struct db_i *ancestor, struct db_i *right, struct diff_state *state) {
    int have_diff = 0;
    struct diff3_results *results;

    BU_GET(results, struct diff3_results);
    diff3_results_init(results);
    results->diff_tolerance = state->diff_tolerance;

    have_diff = db_diff3(left, ancestor, right, &diff3_added, &diff3_removed, &diff3_changed, &diff3_unchanged, (void *)results);

    if (state->verbosity) {
	if (!have_diff) {
	    bu_log("No differences found.\n");
	} else {
	    diff3_summarize(state->diff_log, results, state);
	    bu_log("%s", bu_vls_addr(state->diff_log));
	}
    }

    diff3_results_free(results);
    BU_PUT(results, struct diff3_results);

    return have_diff;
}

static void
gdiff_usage(const char *str) {
    bu_log("Usage: %s [-acmrv] file1.g file2.g\n", str);
    bu_exit(1, NULL);
}


int
main(int argc, char **argv)
{
    int c;
    int diff_return = 0;
    struct diff_state *state;
    struct db_i *ancestor_dbip = DBI_NULL;
    struct db_i *new_dbip_1 = DBI_NULL;
    struct db_i *new_dbip_2 = DBI_NULL;
    const char *diff_prog_name = argv[0];

    BU_GET(state, struct diff_state);
    diff_state_init(state);

    while ((c = bu_getopt(argc, argv, "aC:cF:mrt:uv:xh?")) != -1) {
	switch (c) {
	    case 'a':
		state->return_added = 1;
		break;
	    case 'r':
		state->return_removed = 1;
		break;
	    case 'c':
		state->return_changed = 1;
		break;
	    case 'u':
		state->return_unchanged = 1;
		break;
	    case 'F':
		state->have_search_filter = 1;
		bu_vls_sprintf(state->search_filter, "%s", bu_optarg);
		break;
	    case 'm':   /* mged readable */
		state->output_mode = 109;  /* use ascii decimal value for 'm' to signify mged mode */
		break;
	    case 't':   /* distance tolerance for same/different decisions (RT_LEN_TOL is default) */
		if (sscanf(bu_optarg, "%f", &(state->diff_tolerance)) != 1) {
		    bu_log("Invalid distance tolerance specification: '%s'\n", bu_optarg);
		    gdiff_usage(diff_prog_name);
		    bu_exit (1, NULL);
		}
		break;
	    case 'v':   /* verbosity (2 is default) */
		if (sscanf(bu_optarg, "%d", &(state->verbosity)) != 1) {
		    bu_log("Invalid verbosity specification: '%s'\n", bu_optarg);
		    gdiff_usage(diff_prog_name);
		    bu_exit (1, NULL);
		}
		break;
	    case 'C':   /* conflict reporting (1 is default) */
		if (sscanf(bu_optarg, "%d", &(state->return_conflicts)) != 1) {
		    bu_log("Invalid verbosity specification: '%s'\n", bu_optarg);
		    gdiff_usage(diff_prog_name);
		    bu_exit (1, NULL);
		}
		break;
	    default:
		gdiff_usage(diff_prog_name);
	}
    }

    if (state->output_mode == 109) {
	bu_log("Error - mged script generation not yet implemented\n");
	bu_exit(EXIT_FAILURE, NULL);
    }

    if (state->return_added == -1 && state->return_removed == -1 && state->return_changed == -1 && state->return_unchanged == 0) {
	state->return_added = 1; state->return_removed = 1; state->return_changed = 1;
    }

    argc -= bu_optind;
    argv += bu_optind;

    if (argc != 2 && argc != 3) {
	bu_log("Error - please specify either two or three .g files\n");
	bu_exit(EXIT_FAILURE, NULL);
    }

    if (!bu_file_exists(argv[0], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[0]);
    }

    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[1]);
    }

    if (argc == 3 && !bu_file_exists(argv[2], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[2]);
    }

    if ((ancestor_dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[0]);
    }
    RT_CK_DBI(ancestor_dbip);
    if (db_dirbuild(ancestor_dbip) < 0) {
	db_close(ancestor_dbip);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
    }

    if ((new_dbip_1 = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
    }
    RT_CK_DBI(new_dbip_1);
    if (db_dirbuild(new_dbip_1) < 0) {
	db_close(ancestor_dbip);
	db_close(new_dbip_1);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }

    /* If we have three files, we're doing a 3 way diff and maybe a 3 way merge */
    if (argc == 3) {
	if ((new_dbip_2 = db_open(argv[2], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(1, "Cannot open geometry database file %s\n", argv[2]);
	}
	RT_CK_DBI(new_dbip_2);
	if (db_dirbuild(new_dbip_2) < 0) {
	    db_close(ancestor_dbip);
	    db_close(new_dbip_2);
	    bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[2]);
	}
    }


    if (argc == 2)
	diff_return = do_diff(ancestor_dbip, new_dbip_1, state);

    if (argc == 3)
	diff_return = do_diff3(ancestor_dbip, new_dbip_1, new_dbip_2, state);

    diff_state_free(state);
    BU_PUT(state, struct diff_state);

    db_close(ancestor_dbip);
    db_close(new_dbip_1);
    if (argc == 3) db_close(new_dbip_2);
    return diff_return;
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
