/*                       A N A L Y Z E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file analyze.h
 *
 * Functions provided by the LIBANALYZE geometry analysis library.
 *
 */

#ifndef ANALYZE_H
#define ANALYZE_H

#include "common.h"

#include "raytrace.h"

__BEGIN_DECLS

#ifndef ANALYZE_EXPORT
#  if defined(ANALYZE_DLL_EXPORTS) && defined(ANALYZE_DLL_IMPORTS)
#    error "Only ANALYZE_DLL_EXPORTS or ANALYZE_DLL_IMPORTS can be defined, not both."
#  elif defined(ANALYZE_DLL_EXPORTS)
#    define ANALYZE_EXPORT __declspec(dllexport)
#  elif defined(ANALYZE_DLL_IMPORTS)
#    define ANALYZE_EXPORT __declspec(dllimport)
#  else
#    define ANALYZE_EXPORT
#  endif
#endif

/* Libanalyze return codes */
#define ANALYZE_OK 0x0000
#define ANALYZE_ERROR 0x0001 /**< something went wrong, function not completed */

/*
 *     Density specific structures
 */

#define DENSITY_MAGIC 0xaf0127

/* the entries in the density table */
struct density_entry {
    uint32_t magic;
    double grams_per_cu_mm;
    char *name;
};

/*
 *     Overlap specific structures
 */

struct region_pair {
    struct bu_list l;
    union {
	char *name;
	struct region *r1;
    } r;
    struct region *r2;
    unsigned long count;
    double max_dist;
    vect_t coord;
};

/*
 *      Voxel specific structures
 */

/**
 * This structure is for lists that store region names for each voxel
 */

struct voxelRegion {
    char *regionName;
    fastf_t regionDistance;
    struct voxelRegion *nextRegion;
};

/**
 * This structure stores the information about voxels provided by a single raytrace.
 */

struct rayInfo {
    fastf_t sizeVoxel;
    fastf_t *fillDistances;
    struct voxelRegion *regionList;
};

/**
 *     Routine to parse a .density file
 */
ANALYZE_EXPORT extern int parse_densities_buffer(char *buf,
						 size_t len,
						 struct density_entry *densities,
						 struct bu_vls *result_str,
						 int *num_densities);

/**
 *     region_pair for gqa
 */
ANALYZE_EXPORT extern struct region_pair *add_unique_pair(struct region_pair *list,
							  struct region *r1,
							  struct region *r2,
							  double dist, point_t pt);


/**
 * voxelize function takes raytrace instance and user parameters as inputs
 */
ANALYZE_EXPORT extern void
voxelize(struct rt_i *rtip, fastf_t voxelSize[3], int levelOfDetail, void (*create_boxes)(genptr_t callBackData, int x, int y, int z, const char *regionName, fastf_t percentageFill), genptr_t callBackData);




/**
 * Analyze the difference between two directory objects
 *
 * Returns a pointer to the results structure.  If the first
 * parameter is NULL, diff_dp will create a results structure - else,
 * it uses the one provided.  This allows a calling function to
 * allocate a large number of result containers at once.
 */
ANALYZE_EXPORT void *
diff_dp(void *result_in, struct directory *dp1, struct directory *dp2, struct db_i *dbip1, struct db_i *dbip2);

/**
 * Analyze the difference between two database objects
 *
 * Returns bu_ptbl of results
 */
ANALYZE_EXPORT struct bu_ptbl *
diff_dbip(struct db_i *dbip1, struct db_i *dbip2);

/**
 * Print a summary of the diff results to diff_log
 */
ANALYZE_EXPORT void
diff_summarize(struct bu_vls *diff_log, struct bu_ptbl *results);

/**
 * Free diff results in a bu_ptbl
 */
ANALYZE_EXPORT void
diff_free_ptbl(struct bu_ptbl *results_table);

typedef enum {
    DIFF_NONE = 0,   /* No difference between objects */
    DIFF_REMOVED,    /* Items removed, none added or changed */
    DIFF_ADDED,      /* Items added, none removed or changed */
    DIFF_TYPECHANGE, /* Item underwent a change in type (e.g. sph -> arb8) */
    DIFF_BINARY,     /* Only binary change testing available */
    DIFF             /* Differences present (none of the above cases */
} diff_t;

ANALYZE_EXPORT diff_t
diff_type(void *result);

typedef enum {
    DIFF_SHARED_PARAM = 0,
    DIFF_ORIG_ONLY_PARAM,
    DIFF_NEW_ONLY_PARAM,
    DIFF_CHANGED_ORIG_PARAM,
    DIFF_CHANGED_NEW_PARAM,
    DIFF_SHARED_ATTR,
    DIFF_ORIG_ONLY_ATTR,
    DIFF_NEW_ONLY_ATTR,
    DIFF_CHANGED_ORIG_ATTR,
    DIFF_CHANGED_NEW_ATTR
} diff_result_t;

ANALYZE_EXPORT struct bu_attribute_value_set *
diff_result(void *result, diff_result_t result_type);

typedef enum {
    DIFF_ORIG = 0,
    DIFF_NEW
} diff_obj_t;

ANALYZE_EXPORT struct directory *
diff_info_dp(void *result, diff_obj_t obj_type);

ANALYZE_EXPORT struct rt_db_internal *
diff_info_intern(void *result, diff_obj_t obj_type);



__END_DECLS

#endif /* ANALYZE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
