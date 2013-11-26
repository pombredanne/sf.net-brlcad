/*                      B N _ C H U L L . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"


int
main()
{
    int i = 0;
    int retval = 0;

    /* 2D input */
    {
	point2d_t test1_points[4+1] = {{0}};
	point2d_t test1_results[5+1] = {{0}};
	int n = 4;
	point2d_t *hull_polyline = NULL;
	point2d_t *hull_pnts = NULL;

	V2SET(test1_points[0], 1.5, 1.5);
	V2SET(test1_points[1], 3.0, 2.0);
	V2SET(test1_points[2], 2.0, 2.5);
	V2SET(test1_points[3], 1.0, 2.0);

	V2SET(test1_results[0], 1.0, 2.0);
	V2SET(test1_results[1], 1.5, 1.5);
	V2SET(test1_results[2], 3.0, 2.0);
	V2SET(test1_results[3], 2.0, 2.5);
	V2SET(test1_results[4], 1.0, 2.0);

	retval = bn_polyline_2d_chull(&hull_polyline, (const point2d_t *)test1_points, n);
	if (!retval) return -1;
	bu_log("Test #001:  polyline_2d_hull - 4 point test:\n");
	for (i = 0; i < retval; i++) {
	    bu_log("    expected[%d]: (%f, %f)\n", i, test1_results[i][0], test1_results[i][1]);
	    bu_log("      actual[%d]: (%f, %f)\n", i, hull_polyline[i][0], hull_polyline[i][1]);
	    if (!NEAR_ZERO(test1_results[i][0] - hull_polyline[i][0], SMALL_FASTF) ||
		    !NEAR_ZERO(test1_results[i][1] - hull_polyline[i][1], SMALL_FASTF)) {
		retval = 0;
	    }
	}
	if (!retval) {return -1;} else {bu_log("Test #001 Passed!\n");}


	retval = bn_2d_chull(&hull_pnts, (const point2d_t *)test1_points, n);
	if (!retval) return -1;
	bu_log("Test #002:  2d_hull - 4 point test:\n");
	for (i = 0; i < retval; i++) {
	    bu_log("    expected[%d]: (%f, %f)\n", i, test1_results[i][0], test1_results[i][1]);
	    bu_log("      actual[%d]: (%f, %f)\n", i, hull_pnts[i][0], hull_pnts[i][1]);
	    if (!NEAR_ZERO(test1_results[i][0] - hull_pnts[i][0], SMALL_FASTF) ||
		    !NEAR_ZERO(test1_results[i][1] - hull_pnts[i][1], SMALL_FASTF) ) {
		retval = 0;
	    }
	}
	if (!retval) {return -1;} else {bu_log("Test #002 Passed!\n");}
    }

    /* 3D input */
    {
	point_t test3_points[17+1] = {{0}};
	VSET(test3_points[0], -0.5, 0.5, 0.5);
	VSET(test3_points[1], 0.5, -0.5, 0.5);
	VSET(test3_points[2], 0.5, 0.5, 0.5);
	VSET(test3_points[3], -0.5, -0.5, 0.5);
	VSET(test3_points[4], 0.5, 0.5, 0.5);
	VSET(test3_points[5], -0.5, -0.5, 0.5);
	VSET(test3_points[6], -0.5, 0.5, 0.5);
	VSET(test3_points[7], 0.5, -0.5, 0.5);
	VSET(test3_points[8], 0.1666666666666666574148081, 0.1666666666666666574148081, 0.5);
	VSET(test3_points[9], -0.1666666666666666574148081, -0.1666666666666666574148081, 0.5);
	VSET(test3_points[10], -0.3888888888888888950567946, -0.05555555555555555247160271, 0.5);
	VSET(test3_points[11], -0.3518518518518518600757261, 0.09259259259259258745267118, 0.5);
	VSET(test3_points[12], -0.4629629629629629095077803, -0.01851851851851851749053424, 0.5);
	VSET(test3_points[13], 0.05555555555555555247160271, 0.3888888888888888950567946, 0.5);
	VSET(test3_points[14], 0.3888888888888888950567946, 0.05555555555555555247160271, 0.5);
	VSET(test3_points[15], 0.3518518518518518600757261, 0.2407407407407407273769451, 0.5);
	VSET(test3_points[16], -0.05555555555555555247160271, -0.05555555555555555247160271, 0.5);

    }

    {
	point_t test4_points[17+1] = {{0}};
	VSET(test4_points[0],-0.2615997126297299746333636,0.9692719821506950994560725,1.113297221058902053414386);
	VSET(test4_points[1],-0.6960082873702697625617475,-0.3376479821506951362053428,0.791972778941099408989146);
	VSET(test4_points[2],0.08930731496876459507561208,0.2282231541335035251982788,0.5408368359872989250547448);
	VSET(test4_points[3],-1.046915314968769772363544,0.4034008458664972707197194,1.364433164012702537348787);
	VSET(test4_points[4],0.08930731496876459507561208,0.2282231541335035251982788,0.5408368359872989250547448);
	VSET(test4_points[5],-1.046915314968769772363544,0.4034008458664972707197194,1.364433164012702537348787);
	VSET(test4_points[6],-0.2615997126297299746333636,0.9692719821506950994560725,1.113297221058902053414386);
	VSET(test4_points[7],-0.6960082873702697625617475,-0.3376479821506951362053428,0.791972778941099408989146);
	VSET(test4_points[8],-0.2894335616770786212548217,0.2866157180445003671565019,0.8153689453291005362345345);
	VSET(test4_points[9],-0.6681744383229220041187091,0.3450082819554983748489008,1.089901054670902258436627);
	VSET(test4_points[10],-0.6588964886404735654679143,0.5725603699908965449338893,1.189210479914168061554847);
	VSET(test4_points[11],-0.5295568798643752739252477,0.628946878032363931865234,1.13080291854799019901634);
	VSET(test4_points[12],-0.6558038387463227536500199,0.6484110660026961570068238,1.222313621661925475692101);
	VSET(test4_points[13],-0.1539086531126804824332055,0.4947036181095669227225642,0.823167667458433838234555);
	VSET(test4_points[14],-0.2987115113595283921732459,0.05906363000910182931013637,0.7160595200858336228932899);
	VSET(test4_points[15],-0.1662792526892814537475829,0.1913008340623683078973727,0.6907551004674108430236856);
	VSET(test4_points[16],-0.5419274794409739692824246,0.3255440939851655945957987,0.9983903515569694242515197);


    }

    {
	point_t test5_points[9+1] = {{0}};
	VSET(test5_points[0],-0.2894335616770786212548217,0.2866157180445003671565019,0.8153689453291005362345345);
	VSET(test5_points[1],-0.6681744383229220041187091,0.3450082819554983748489008,1.089901054670902258436627);
	VSET(test5_points[2],-0.6588964886404735654679143,0.5725603699908965449338893,1.189210479914168061554847);
	VSET(test5_points[3],-0.5295568798643752739252477,0.628946878032363931865234,1.13080291854799019901634);
	VSET(test5_points[4],-0.6558038387463227536500199,0.6484110660026961570068238,1.222313621661925475692101);
	VSET(test5_points[5],-0.1539086531126804824332055,0.4947036181095669227225642,0.823167667458433838234555);
	VSET(test5_points[6],-0.2987115113595283921732459,0.05906363000910182931013637,0.7160595200858336228932899);
	VSET(test5_points[7],-0.1662792526892814537475829,0.1913008340623683078973727,0.6907551004674108430236856);
	VSET(test5_points[8],-0.5419274794409739692824246,0.3255440939851655945957987,0.9983903515569694242515197);


    }

    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

