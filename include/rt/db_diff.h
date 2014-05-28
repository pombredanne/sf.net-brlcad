/*                       D B _ D I F F . H
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
/** @file db_diff.h
 *
 * Diff interface for comparing geometry.
 *
 */

/**
 * DIFF bit flags to select various types of results
 */
#define DIFF_EMPTY	0      /* Nothing to diff */
#define DIFF_UNCHANGED	1      /* (left == right) */
#define DIFF_REMOVED	2      /* (right == NULL) */
#define DIFF_ADDED	4      /* (left == NULL)  */
#define DIFF_CHANGED	8      /* (left != right) */

/**
 * DIFF3 bit flags to select various types of results
 */
#define DIFF3_EMPTY                            0          /* Nothing to diff */
#define DIFF3_UNCHANGED			       1          /* (ancestor == left) && (ancestor == right)                    */
#define DIFF3_REMOVED_BOTH_IDENTICALLY 	       2          /* (ancestor) && (right == NULL) && (left == NULL)              */
#define DIFF3_REMOVED_LEFT_ONLY 	       4          /* (ancestor == right) && (left == NULL)                        */
#define DIFF3_REMOVED_RIGHT_ONLY 	       8          /* (ancestor == left) && (right == NULL)                        */
#define DIFF3_ADDED_BOTH_IDENTICALLY 	       16         /* (ancestor == NULL) && (left == right)                        */
#define DIFF3_ADDED_LEFT_ONLY 		       32         /* (ancestor == NULL) && (right == NULL) && (left)              */
#define DIFF3_ADDED_RIGHT_ONLY 		       64         /* (ancestor == NULL) && (left == NULL) && (right)              */
#define DIFF3_CHANGED_BOTH_IDENTICALLY         128        /* (ancestor != left) && (ancestor != right) && (left == right) */
#define DIFF3_CHANGED_LEFT_ONLY 	       256        /* (ancestor != left) && (ancestor == right)                    */
#define DIFF3_CHANGED_RIGHT_ONLY 	       1024       /* (ancestor == left) && (ancestor != right)                    */
#define DIFF3_CHANGED_CLEAN_MERGE 	       2048       /* ((ancestor == NULL) || ((ancestor != left) && (ancestor != right))) && (left != right) && (clean_merge)  */
#define DIFF3_CONFLICT_LEFT_CHANGE_RIGHT_DEL   4096       /* (ancestor != left) && (right== NULL)                         */
#define DIFF3_CONFLICT_RIGHT_CHANGE_LEFT_DEL   8192       /* (ancestor != right) && (left == NULL)                        */
#define DIFF3_CONFLICT_ADDED_BOTH	      16384       /* (ancestor == NULL) && (left != right) && (!clean_merge)      */
#define DIFF3_CONFLICT_CHANGED_BOTH	      32768       /* ((ancestor != left) && (ancestor != right)) && (left != right) && (!clean_merge)  */

/**
 * The flags parameter is a bitfield is used to specify whether
 * to process internal object parameter differences
 * (DB_COMPARE_PARAM), attribute differences (DB_COMPARE_ATTRS), or
 * both (DB_COMPARE_ALL).
 */
typedef enum {
    DB_COMPARE_ALL=0x00,
    DB_COMPARE_PARAM=0x01,
    DB_COMPARE_ATTRS=0x02
} db_compare_criteria_t;



/**
 * Compare two attribute sets.
 *
 * This function is useful for comparing the contents
 * of two attribute/value sets. */
RT_EXPORT extern int
db_avs_diff(const struct bu_attribute_value_set *left_set,
	    const struct bu_attribute_value_set *right_set,
            const struct bn_tol *diff_tol,
	    int (*add_func)(const char *attr_name, const char *attr_val, void *data),
	    int (*del_func)(const char *attr_name, const char *attr_val, void *data),
	    int (*chgd_func)(const char *attr_name, const char *attr_val_left, const char *attr_val_right, void *data),
	    int (*unchgd_func)(const char *attr_name, const char *attr_val, void *data),
	    void *client_data);

/**
 * Compare three attribute sets.
 */
RT_EXPORT extern int
db_avs_diff3(const struct bu_attribute_value_set *left_set,
	     const struct bu_attribute_value_set *ancestor_set,
	     const struct bu_attribute_value_set *right_set,
	     const struct bn_tol *diff_tol,
	     /* DIFF3_ADDED_BOTH_IDENTICALLY, DIFF3_ADDED_LEFT_ONLY, DIFF3_ADDED_RIGHT_ONLY */
	     int (*add_func)(const char *attr_name,
		                const char *attr_val_left,
			       	const char *attr_val_right,
			       	int state,
			       	void *data),
	     /* DIFF3_REMOVED_BOTH_IDENTICALLY, DIFF3_REMOVED_LEFT_ONLY, DIFF3_REMOVED_RIGHT_ONLY */
	     int (*del_func)(const char *attr_name,
			       	const char *attr_val_ancestor,
			       	int state,
			       	void *data),
	     /* DIFF3_CHANGED_BOTH_IDENTICALLY, DIFF3_CHANGED_LEFT_ONLY, DIFF3_CHANGED_RIGHT_ONLY, DIFF3_CHANGED_CLEAN_MERGE */
	     int (*chgd_func)(const char *attr_name,
		                const char *attr_val_left,
			       	const char *attr_val_ancestor,
			       	const char *attr_val_right,
			       	int state,
			       	void *data),
	     /* DIFF3_CONFLICT_LEFT_CHANGE_RIGHT_DEL, DIFF3_CONFLICT_RIGHT_CHANGE_LEFT_DEL, DIFF3_CONFLICT_BOTH_CHANGED */
	     int (*conflict_func)(const char *attr_name,
		                const char *attr_val_left,
			       	const char *attr_val_ancestor,
			       	const char *attr_val_right,
			       	int state,
			       	void *data),
	     /* DIFF3_UNCHANGED */
	     int (*unchgd_func)(const char *attr_name,
			       	const char *attr_val_ancestor,
			       	void *data),
	     void *client_data);

/*
 * Results for a diff between two objects are held in a set
 * of avs structures.
 */
struct diff_result {
    char *obj_name;
    int param_state;  /* results of diff for parameters */
    int attr_state;   /* results of diff for attributes */
    struct bn_tol *diff_tol;
    struct bu_attribute_value_set *left_param_avs;
    struct bu_attribute_value_set *right_param_avs;
    struct bu_attribute_value_set *left_attr_avs;
    struct bu_attribute_value_set *right_attr_avs;
};
RT_EXPORT extern void diff_init_result(struct diff_result **result, const struct bn_tol *curr_diff_tol, const char *object_name);
RT_EXPORT extern void diff_free_result(struct diff_result *result);

/**
 * Compare two database objects.
 *
 * This function is useful for comparing two geometry objects and
 * inspecting their differences.  Differences are recorded in
 * key=value form based on whether there is a difference added in the
 * right, removed in the right, changed going from left to right, or
 * unchanged.
 *
 * The flags parameter is a bitfield specifying what type of
 * comparisons to perform.  The default is to compare everything and
 * report on any differences encountered.
 *
 * The same bu_attribute_value_set container may be passed to any of
 * the provided containers to aggregate results.  NULL may be passed
 * to not inspect or record information for that type of comparison.
 *
 * Returns an int with bit flags set according to the above
 * four diff categories.
 *
 * Negative returns indicate an error.
 */
RT_EXPORT extern int
db_diff_dp(const struct db_i *left_dbip,
	   const struct db_i *right_dbip,
	   const struct directory *left_dp,
	   const struct directory *right_dp,
	   const struct bn_tol *diff_tol,
	   db_compare_criteria_t flags,
	   struct diff_result *result);


/*
 * Results for a diff3 between three objects are held in a set
 * of avs structures.
 */
struct diff3_result {
    char *obj_name;
    int param_state;  /* results of diff for parameters */
    int attr_state;   /* results of diff for attributes */
    struct bn_tol *diff_tol;
    struct bu_attribute_value_set *left_param_avs;
    struct bu_attribute_value_set *ancestor_param_avs;
    struct bu_attribute_value_set *right_param_avs;
    struct bu_attribute_value_set *left_attr_avs;
    struct bu_attribute_value_set *ancestor_attr_avs;
    struct bu_attribute_value_set *right_attr_avs;
};
RT_EXPORT extern void diff3_init_result(struct diff3_result **result, const struct bn_tol *curr_diff_tol, const char *object_name);
RT_EXPORT extern void diff3_free_result(struct diff3_result *result);

/**
 * Compare three database objects.
 *
 * The flags parameter is a bitfield specifying what type of
 * comparisons to perform.  The default is to compare everything and
 * report on any differences encountered.
 *
 * The same bu_attribute_value_set container may be passed to any of
 * the provided containers to aggregate results.  NULL may be passed
 * to not inspect or record information for that type of comparison.
 *
 * Returns an int with bit flags set according to the above
 * diff3 categories.
 *
 * Negative returns indicate an error.
 *
 */

RT_EXPORT extern int
db_diff3_dp(const struct db_i *left,
	    const struct db_i *ancestor,
	    const struct db_i *right,
	    const struct directory *left_dp,
	    const struct directory *ancestor_dp,
	    const struct directory *right_dp,
	    const struct bn_tol *diff_tol,
	    db_compare_criteria_t flags,
	    struct diff3_result *result);

/**
 * Compare two database instances.
 *
 * All objects in dbip_left are compared against the objects in
 * dbip_right.  Every object results in one of four callback functions
 * getting called.  Any objects in dbip_right but not in dbip_left
 * cause add_func() to get called.  Any objects in dbip_left but not
 * in dbip_right cause del_func() to get called.  Objects existing in
 * both (i.e., with the same name) but differing in some fashion
 * cause chgd_func() to get called. If the object exists in both
 * but is unchanged, unch_func() is called.  NULL may be
 * passed to skip any callback.
 *
 * Returns an int with bit flags set according to the above
 * four diff categories.
 *
 * Negative returns indicate an error.
 */
RT_EXPORT extern int
db_diff(const struct db_i *dbip_left,
	const struct db_i *dbip_right,
	const struct bn_tol *diff_tol,
	db_compare_criteria_t flags,
	struct bu_ptbl *diff_results);


/**
 * Compare three database instances.
 *
 * This does a "3-way" diff to identify changes in the left and
 * right databases relative to the ancestor database, and provides
 * functional hooks for the various cases.
 *
 * Returns an int with bit flags set according to the above
 * diff3 categories.
 *
 * Negative returns indicate an error.
 */
RT_EXPORT extern int
db_diff3(const struct db_i *dbip_left,
	const struct db_i *dbip_ancestor,
	const struct db_i *dbip_right,
	const struct bn_tol *diff_tol,
	db_compare_criteria_t flags,
	struct bu_ptbl *diff3_results);


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
