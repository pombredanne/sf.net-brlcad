/* meshoptimization.cpp
 *
 * ----------------------------------------------------------------------
 *
 * Copyright (c) 2014 SURVICE Engineering. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * ----------------------------------------------------------------------
 *
 * Revision Date:
 *  08/26/13
 *
 * Authors(s):
 *  DRH
 *
 * Description:
 *  Mesh optimization routine.
 *
 */


#include "common.h"

#include "meshoptimization.h"

#include "meshoptimizer.h"


void
mesh_optimization(long vertexcount,
		  long tricount,
		  void *indices,
		  int indiceswidth,
		  int optimizationlevel
		 )
{
    const int OPTIMIZATION_FLAGS = MO_FLAGS_DISABLE_LOOK_AHEAD
				   | MO_FLAGS_ENABLE_LAZY_SEARCH
				   | MO_FLAGS_FAST_SEED_SELECT;

    int depth, flags;

    switch (optimizationlevel) {
	case 3:
	    depth = 32;
	    flags = 0;
	    break;

	case 2:
	    depth = 32;
	    flags = OPTIMIZATION_FLAGS;
	    break;

	case 1:
	    depth = 16;
	    flags = OPTIMIZATION_FLAGS;
	    break;

	case 0:
	default:
	    return;
    }

    moOptimizeMesh(vertexcount,
		   tricount,
		   indices, indiceswidth,
		   3 * indiceswidth, 0, 0,
		   depth,
		   flags);
}
