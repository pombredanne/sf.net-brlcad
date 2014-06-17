/*                     A R B _ E D I T . H
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
/** @file arb_edit.h
 *
 * Editing operations for arb primitives.
 *
 */

/**
 * face definitions for each arb type
 */
const int rt_arb_faces[5][24] = {
    {0,1,2,3, 0,1,4,5, 1,2,4,5, 0,2,4,5, -1,-1,-1,-1, -1,-1,-1,-1},	/* ARB4 */
    {0,1,2,3, 4,0,1,5, 4,1,2,5, 4,2,3,5, 4,3,0,5, -1,-1,-1,-1},		/* ARB5 */
    {0,1,2,3, 1,2,6,4, 0,4,6,3, 4,1,0,5, 6,2,3,7, -1,-1,-1,-1},		/* ARB6 */
    {0,1,2,3, 4,5,6,7, 0,3,4,7, 1,2,6,5, 0,1,5,4, 3,2,6,4},		/* ARB7 */
    {0,1,2,3, 4,5,6,7, 0,4,7,3, 1,2,6,5, 0,1,5,4, 3,2,6,7},		/* ARB8 */
};


/* The following arb editing arrays generally contain the following:
 *
 *	location 	comments
 *-------------------------------------------------------------------
 *	0,1		edge end points
 * 	2,3		bounding planes 1 and 2
 *	4, 5,6,7	plane 1 to recalculate, using next 3 points
 *	8, 9,10,11	plane 2 to recalculate, using next 3 points
 *	12, 13,14,15	plane 3 to recalculate, using next 3 points
 *	16,17		points (vertices) to recalculate
 *
 * Each line is repeated for each edge (or point) to move.
 * See g_arb.c for more details.
 */

/**
 * edit array for arb8's
 */
short earb8[12][18] = {
    {0,1, 2,3, 0,0,1,2, 4,0,1,4, -1,0,0,0, 3,5},	/* edge 12 */
    {1,2, 4,5, 0,0,1,2, 3,1,2,5, -1,0,0,0, 3,6},	/* edge 23 */
    {2,3, 3,2, 0,0,2,3, 5,2,3,6, -1,0,0,0, 1,7},	/* edge 34 */
    {0,3, 4,5, 0,0,1,3, 2,0,3,4, -1,0,0,0, 2,7},	/* edge 14 */
    {0,4, 0,1, 2,0,4,3, 4,0,1,4, -1,0,0,0, 7,5},	/* edge 15 */
    {1,5, 0,1, 4,0,1,5, 3,1,2,5, -1,0,0,0, 4,6},	/* edge 26 */
    {4,5, 2,3, 4,0,5,4, 1,4,5,6, -1,0,0,0, 1,7},	/* edge 56 */
    {5,6, 4,5, 3,1,5,6, 1,4,5,6, -1,0,0,0, 2,7},	/* edge 67 */
    {6,7, 3,2, 5,2,7,6, 1,4,6,7, -1,0,0,0, 3,4},	/* edge 78 */
    {4,7, 4,5, 2,0,7,4, 1,4,5,7, -1,0,0,0, 3,6},	/* edge 58 */
    {2,6, 0,1, 3,1,2,6, 5,2,3,6, -1,0,0,0, 5,7},	/* edge 37 */
    {3,7, 0,1, 2,0,3,7, 5,2,3,7, -1,0,0,0, 4,6},	/* edge 48 */
};

short arb8_edge_vertex_mapping[12][2] = {
    {0,1},	/* edge 12 */
    {1,2},	/* edge 23 */
    {2,3},	/* edge 34 */
    {0,3},	/* edge 14 */
    {0,4},	/* edge 15 */
    {1,5},	/* edge 26 */
    {4,5},	/* edge 56 */
    {5,6},	/* edge 67 */
    {6,7},	/* edge 78 */
    {4,7},	/* edge 58 */
    {2,6},	/* edge 37 */
    {3,7},	/* edge 48 */
};

/**
 * edit array for arb7's
 */
short earb7[12][18] = {
    {0,1, 2,3, 0,0,1,2, 4,0,1,4, -1,0,0,0, 3,5},	/* edge 12 */
    {1,2, 4,5, 0,0,1,2, 3,1,2,5, -1,0,0,0, 3,6},	/* edge 23 */
    {2,3, 3,2, 0,0,2,3, 5,2,3,6, -1,0,0,0, 1,4},	/* edge 34 */
    {0,3, 4,5, 0,0,1,3, 2,0,3,4, -1,0,0,0, 2,-1},	/* edge 41 */
    {0,4, 0,5, 4,0,5,4, 2,0,3,4, 1,4,5,6, 1,-1},	/* edge 15 */
    {1,5, 0,1, 4,0,1,5, 3,1,2,5, -1,0,0,0, 4,6},	/* edge 26 */
    {4,5, 5,3, 2,0,3,4, 4,0,5,4, 1,4,5,6, 1,-1},	/* edge 56 */
    {5,6, 4,5, 3,1,6,5, 1,4,5,6, -1,0,0,0, 2, -1},	/* edge 67 */
    {2,6, 0,1, 5,2,3,6, 3,1,2,6, -1,0,0,0, 4,5},	/* edge 37 */
    {4,6, 4,3, 2,0,3,4, 5,3,4,6, 1,4,5,6, 2,-1},	/* edge 57 */
    {3,4, 0,1, 4,0,1,4, 2,0,3,4, 5,2,3,4, 5,6},		/* edge 45 */
    {-1,-1, -1,-1, 5,2,3,4, 4,0,1,4, 8,2,1,-1, 6,5},	/* point 5 */
};

short arb7_edge_vertex_mapping[12][2] = {
    {0,1},	/* edge 12 */
    {1,2},	/* edge 23 */
    {2,3},	/* edge 34 */
    {0,3},	/* edge 41 */
    {0,4},	/* edge 15 */
    {1,5},	/* edge 26 */
    {4,5},	/* edge 56 */
    {5,6},	/* edge 67 */
    {2,6},	/* edge 37 */
    {4,6},	/* edge 57 */
    {3,4},	/* edge 45 */
    {4,4},	/* point 5 */
};

/**
 * edit array for arb6's
 */
short earb6[10][18] = {
    {0,1, 2,1, 3,0,1,4, 0,0,1,2, -1,0,0,0, 3,-1},	/* edge 12 */
    {1,2, 3,4, 1,1,2,5, 0,0,1,2, -1,0,0,0, 3,4},	/* edge 23 */
    {2,3, 1,2, 4,2,3,5, 0,0,2,3, -1,0,0,0, 1,-1},	/* edge 34 */
    {0,3, 3,4, 2,0,3,5, 0,0,1,3, -1,0,0,0, 4,2},	/* edge 14 */
    {0,4, 0,1, 3,0,1,4, 2,0,3,4, -1,0,0,0, 6,-1},	/* edge 15 */
    {1,4, 0,2, 3,0,1,4, 1,1,2,4, -1,0,0,0, 6,-1},	/* edge 25 */
    {2,6, 0,2, 4,6,2,3, 1,1,2,6, -1,0,0,0, 4,-1},	/* edge 36 */
    {3,6, 0,1, 4,6,2,3, 2,0,3,6, -1,0,0,0, 4,-1},	/* edge 46 */
    {-1,-1, -1,-1, 2,0,3,4, 1,1,2,4, 3,0,1,4, 6,-1},	/* point 5 */
    {-1,-1, -1,-1, 2,0,3,6, 1,1,2,6, 4,2,3,6, 4,-1},	/* point 6 */
};

short arb6_edge_vertex_mapping[10][2] = {
    {0,1},	/* edge 12 */
    {1,2},	/* edge 23 */
    {2,3},	/* edge 34 */
    {0,3},	/* edge 14 */
    {0,4},	/* edge 15 */
    {1,4},	/* edge 25 */
    {2,5},	/* edge 36 */
    {3,5},	/* edge 46 */
    {4,4},	/* point 5 */
    {7,7},	/* point 6 */
};

/**
 * edit array for arb5's
 */
short earb5[9][18] = {
    {0,1, 4,2, 0,0,1,2, 1,0,1,4, -1,0,0,0, 3,-1},	/* edge 12 */
    {1,2, 1,3, 0,0,1,2, 2,1,2,4, -1,0,0,0, 3,-1},	/* edge 23 */
    {2,3, 2,4, 0,0,2,3, 3,2,3,4, -1,0,0,0, 1,-1},	/* edge 34 */
    {0,3, 1,3, 0,0,1,3, 4,0,3,4, -1,0,0,0, 2,-1},	/* edge 14 */
    {0,4, 0,2, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* edge 15 */
    {1,4, 0,3, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* edge 25 */
    {2,4, 0,4, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1}, 	/* edge 35 */
    {3,4, 0,1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* edge 45 */
    {-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 5 */
};

short arb5_edge_vertex_mapping[9][2] = {
    {0,1},	/* edge 12 */
    {1,2},	/* edge 23 */
    {2,3},	/* edge 34 */
    {0,3},	/* edge 14 */
    {0,4},	/* edge 15 */
    {1,4},	/* edge 25 */
    {2,4},	/* edge 35 */
    {3,4},	/* edge 45 */
    {4,4},	/* point 5 */
};

/**
 * edit array for arb4's
 */
short earb4[5][18] = {
    {-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 1 */
    {-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 2 */
    {-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 3 */
    {-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* dummy */
    {-1,-1, -1,-1, 9,0,0,0, 9,0,0,0, 9,0,0,0, -1,-1},	/* point 4 */
};

short arb4_edge_vertex_mapping[5][2] = {
    {0,0},	/* point 1 */
    {1,1},	/* point 2 */
    {2,2},	/* point 3 */
    {3,3},	/* dummy */
    {4,4},	/* point 4 */
};


#if 0
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
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
