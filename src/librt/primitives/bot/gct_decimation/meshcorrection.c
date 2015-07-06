/* *****************************************************************************
 *
 * Copyright (c) 2014 Alexis Naveros. All rights reserved.
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
 * *****************************************************************************
 */


#if defined(__GNUC__) && (__GNUC__ == 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wunused-function"
#endif


#include "common.h"

#include "meshcorrection.h"

#include "auxiliary/cpuconfig.h"
#include "auxiliary/cpuinfo.h"
#include "auxiliary/cc.h"
#include "auxiliary/mm.h"
#include "auxiliary/mmhash.h"
#include "auxiliary/mmbitmap.h"
#include "auxiliary/math3d.h"
#include "auxiliary/mmbinsort.h"

#include "bu/log.h"
#include "vmath.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>

/****/


typedef int mci;
#define MC_SIZEOF_MCI (CPUCONF_INT_SIZE)


/* Double precision internal math is recommended */
/*
typedef float mcf;
*/
typedef double mcf;


/* Maximum count of attempts to try avoiding inexact results */
#define MC_SEED_ATTEMPT_COUNT_MAX (64)

/* Threshold of attempt count beyond which test only yet unprocessed triangles */
#define MC_SEED_ATTEMPT_SAFETY_THRESHOLD (MC_SEED_ATTEMPT_COUNT_MAX>>2)

/* Thresholds to flag operations as inexact, triggering a fresh new attempt */
#define MC_ENABLE_INEXACTFLAG_SUPPORT
#define MC_INEXACT_VECTOR_THRESHOLD (0.01)
#define MC_INEXACT_SEGMENT_THRESHOLD (0.001)

/* Multiply thresholds by this factor on each pass */
#define MC_INEXACT_THRESHOLD_ATTEMPT_FACTOR (0.9)


typedef struct {
    mci v[2];
    mci triindex;
} mcEdge;

typedef struct {
    mci v0, v1;
    mci triindex;
    mmListNode list;
} mcOp;


/****/


static void mcEdgeHashClearEntry(void *entry)
{
    mcEdge *edge;
    edge = (mcEdge *)entry;
    edge->v[0] = -1;
    return;
}

static int mcEdgeHashEntryValid(void *entry)
{
    mcEdge *edge;
    edge = (mcEdge *)entry;
    return (edge->v[0] >= 0 ? 1 : 0);
}

static uint32_t mcEdgeHashEntryKey(void *entry)
{
    uint32_t hashkey;
    mcEdge *edge;
    edge = (mcEdge *)entry;

#ifdef MC_CONFIG_FAST_HASH

    hashkey  = edge->v[0];
    hashkey += hashkey << 10;
    hashkey ^= hashkey >> 6;
    hashkey += edge->v[1];
    hashkey += hashkey << 10;
    hashkey ^= hashkey >> 6;
    hashkey += hashkey << 6;
    hashkey ^= hashkey >> 11;
    hashkey += hashkey << 15;

#else

#if MC_SIZEOF_MCI == 4
#if CPUCONF_WORD_SIZE == 64
    {
	uint64_t *v = (uint64_t *)edge->v;
	hashkey = ccHash32Int64Inline(*v);
    }
#else
    hashkey = ccHash32Data4Inline((void *)edge->v);
#endif
#elif MC_SIZEOF_MCI == 8
#if CPUCONF_WORD_SIZE == 64
    hashkey = ccHash32Array64((uint64_t *)edge->v, 2);
#else
    hashkey = ccHash32Array32((uint32_t *)edge->v, 4);
#endif
#else
    hashkey = ccHash32Data(edge->v, 2 * sizeof(mci));
#endif

#endif

    return hashkey;
}

static int mcEdgeHashEntryCmp(void *entry, void *entryref)
{
    mcEdge *edge, *edgeref;
    edge = (mcEdge *)entry;
    edgeref = (mcEdge *)entryref;

    if (edge->v[0] == -1)
	return MM_HASH_ENTRYCMP_INVALID;

    if ((edge->v[0] == edgeref->v[0]) && (edge->v[1] == edgeref->v[1]))
	return MM_HASH_ENTRYCMP_FOUND;

    return MM_HASH_ENTRYCMP_SKIP;
}

static mmHashAccess mcEdgeHashEdge = {
    mcEdgeHashClearEntry,
    mcEdgeHashEntryValid,
    mcEdgeHashEntryKey,
    mcEdgeHashEntryCmp,
    NULL
};


/****/


typedef struct {
    size_t vertexcount;
    void *vertex;
    int vertexwidth;
    size_t vertexstride;
    size_t tricount;
    void *indices;
    int indiceswidth;
    size_t indicesstride;
    long attemptcount;
    int flags;

    void *edgehashtable;
    mmBitMap tribitmap;
    int processcount;

    mmBlockHead opblock;

    void (*indicesUserToNative)(mci *dst, void *src);
    void (*indicesNativeToUser)(void *dst, mci *src);
    void (*vertexUserToNative)(mcf *dst, void *src);
    void (*vertexNativeToUser)(void *dst, mcf *src);

    ccQuickRandState32 randstate;

} mcMesh;


static void mcIndicesInt8ToNative(mci *dst, void *src)
{
    uint8_t *s = (uint8_t *)src;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
    return;
}

static void mcIndicesInt16ToNative(mci *dst, void *src)
{
    uint16_t *s = (uint16_t *)src;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
    return;
}

static void mcIndicesInt32ToNative(mci *dst, void *src)
{
    uint32_t *s = (uint32_t *)src;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
    return;
}

static void mcIndicesInt64ToNative(mci *dst, void *src)
{
    uint64_t *s = (uint64_t *)src;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
    return;
}


static void mcIndicesNativeToInt8(void *dst, mci *src)
{
    uint8_t *d = (uint8_t *)dst;
    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
    return;
}

static void mcIndicesNativeToInt16(void *dst, mci *src)
{
    uint16_t *d = (uint16_t *)dst;
    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
    return;
}

static void mcIndicesNativeToInt32(void *dst, mci *src)
{
    uint32_t *d = (uint32_t *)dst;
    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
    return;
}

static void mcIndicesNativeToInt64(void *dst, mci *src)
{
    uint64_t *d = (uint64_t *)dst;
    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
    return;
}


static void mcVertexFloatToNative(mcf *dst, void *src)
{
    float *s = (float *)src;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
    return;
}

static void mcVertexDoubleToNative(mcf *dst, void *src)
{
    double *s = (double *)src;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
    return;
}

static void mcVertexNativeToFloat(void *dst, mcf *src)
{
    float *d = (float *)dst;
    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
    return;
}

static void mcVertexNativeToDouble(void *dst, mcf *src)
{
    double *d = (double *)dst;
    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
    return;
}


/****/


static void *mcMeshHashInit(size_t trianglecount, mcf hashextrabits, uint32_t lockpageshift)
{
    size_t edgecount, hashmemsize;
    uint32_t hashbits, hashbitsmin, hashbitsmax;
    void *edgehashtable;

    edgecount = trianglecount * 3;

    /* Hard minimum count of hash table bits */
#if CPUCONF_WORD_SIZE == 64
    hashbitsmin = ccLog2Int64(edgecount) + 1;
#else
    hashbitsmin = ccLog2Int32(edgecount) + 1;
#endif
    hashbitsmax = hashbitsmin + 4;

    hashbits = (uint32_t)(log2((mcf)edgecount) + hashextrabits + 0.5);

    if (hashbits < hashbitsmin)
	hashbits = hashbitsmin;
    else if (hashbits > hashbitsmax)
	hashbits = hashbitsmax;

    if (hashbits < 12)
	hashbits = 12;

    /* lockpageshift = 7; works great, 128 hash entries per lock page */
    if (lockpageshift < 3)
	lockpageshift = 3;
    else if (lockpageshift > 16)
	lockpageshift = 16;

    hashmemsize = mmHashRequiredSize(sizeof(mcEdge), hashbits, lockpageshift);

    edgehashtable = malloc(hashmemsize);

    if (!(edgehashtable))
	return 0;

    mmHashInit(edgehashtable, &mcEdgeHashEdge, sizeof(mcEdge), hashbits, lockpageshift, MM_HASH_FLAGS_NO_COUNT);

    return edgehashtable;
}


void mcHashBuild(mcMesh *mesh)
{
    mci triindex, indices[3];
    void *indsrc;
    mcEdge edge;

    /* Build triangles, allow duplicate edges */
    indsrc = mesh->indices;

    for (triindex = 0 ; triindex < (mci)mesh->tricount ; triindex++, indsrc = ADDRESS(indsrc, mesh->indicesstride)) {
	mesh->indicesUserToNative(indices, indsrc);
	edge.v[0] = indices[0];
	edge.v[1] = indices[1];
	edge.triindex = triindex;

	if (mmHashDirectAddEntry(mesh->edgehashtable, &mcEdgeHashEdge, &edge, 0) != MM_HASH_SUCCESS)
	    bu_bomb("SHOULD NOT HAPPEN");

	edge.v[0] = indices[1];
	edge.v[1] = indices[2];
	edge.triindex = triindex;

	if (mmHashDirectAddEntry(mesh->edgehashtable, &mcEdgeHashEdge, &edge, 0) != MM_HASH_SUCCESS)
	    bu_bomb("SHOULD NOT HAPPEN");

	edge.v[0] = indices[2];
	edge.v[1] = indices[0];
	edge.triindex = triindex;

	if (mmHashDirectAddEntry(mesh->edgehashtable, &mcEdgeHashEdge, &edge, 0) != MM_HASH_SUCCESS)
	    bu_bomb("SHOULD NOT HAPPEN");
    }

    return;
}


/****/


typedef struct {
    mcf vectorthreshold;
    mcf segmentthreshold;
    int inexactflag;
} mcIntersectInexact;


static int mcIntersectRayTriangle(mcf *src, mcf *vector, mcf *v0, mcf *v1, mcf *v2, mcf *hit, mcIntersectInexact *inexact)
{
    mcf u[3], v[3], normal[3], sv0[3], planehit[3], w[3];
    mcf distance, num, denom;
    mcf uu, uv, vv, wu, wv, d;
    mcf s, t;
#ifdef MC_ENABLE_INEXACTFLAG_SUPPORT
    int inexactflag;
    mcf accuracy;
#endif

    M3D_VectorSubStore(u, v1, v0);
    M3D_VectorSubStore(v, v2, v0);
    M3D_VectorCrossProduct(normal, u, v);

    denom = M3D_VectorDotProduct(normal, vector);

    /* Dotproduct of normal,vector  should be near mag(normal)*mag(vector) for high accuracy */
#ifdef MC_ENABLE_INEXACTFLAG_SUPPORT
    accuracy = fabs(denom) / (M3D_VectorMagnitude(normal) * M3D_VectorMagnitude(vector));

    if (accuracy < inexact->vectorthreshold)
	inexact->inexactflag = 1;

#endif

    if (ZERO(denom))    /* Ray is parallel to triangle, what about a robust check for near parallel? */
	return 0;

    M3D_VectorSubStore(sv0, src, v0);
    num = -M3D_VectorDotProduct(normal, sv0);
    distance = num / denom;


#ifdef MC_ENABLE_INEXACTFLAG_SUPPORT

    if (distance < -inexact->segmentthreshold)  /* Ray aimed away from triangle plane */
	return 0;

    inexactflag = 0;

    if ((distance > -inexact->segmentthreshold) && (distance < inexact->segmentthreshold))
	inexactflag = 1;

#else

    if (distance < 0.0)  /* Ray aimed away from triangle plane */
	return 0;

#endif

    uu = M3D_VectorDotProduct(u, u);
    uv = M3D_VectorDotProduct(u, v);
    vv = M3D_VectorDotProduct(v, v);
    d = (uv * uv) - (uu * vv);


    if (ZERO(d))
	return 0;

    d = 1.0 / d;

    planehit[0] = src[0] + (distance * vector[0]);
    planehit[1] = src[1] + (distance * vector[1]);
    planehit[2] = src[2] + (distance * vector[2]);


    M3D_VectorSubStore(w, planehit, v0);
    wu = M3D_VectorDotProduct(w, u);
    wv = M3D_VectorDotProduct(w, v);
    s = ((uv * wv) - (vv * wu)) * d;


    if ((s < 0.0) || (s > 1.0))
	return 0;

    t = ((uv * wu) - (uu * wv)) * d;


    if ((t < 0.0) || ((s + t) > 1.0))
	return 0;

#ifdef MC_ENABLE_INEXACTFLAG_SUPPORT
    inexact->inexactflag = inexactflag;

    if (distance < 0.0)  /* Ray aimed away from triangle plane */
	return 0;

#endif
    M3D_VectorCopy(hit, planehit);
    return 1;
}


static int mcIntersectSegmentTriangle(mcf *src, mcf *dst, mcf *v0, mcf *v1, mcf *v2, mcf *hit, mcIntersectInexact *inexact)
{
    mcf u[3], v[3], normal[3], vector[3], sv0[3], planehit[3], w[3];
    mcf distance, num, denom;
    mcf uu, uv, vv, wu, wv, d;
    mcf s, t;
#ifdef MC_ENABLE_INEXACTFLAG_SUPPORT
    int inexactflag;
    mcf accuracy;
#endif

    M3D_VectorSubStore(u, v1, v0);
    M3D_VectorSubStore(v, v2, v0);
    M3D_VectorCrossProduct(normal, u, v);


    M3D_VectorSubStore(vector, dst, src);
    denom = M3D_VectorDotProduct(normal, vector);

    /* Dotproduct of normal,vector  should be near mag(normal)*mag(vector) for high accuracy */
#ifdef MC_ENABLE_INEXACTFLAG_SUPPORT
    accuracy = fabs(denom) / (M3D_VectorMagnitude(normal) * M3D_VectorMagnitude(vector));

    if (accuracy < inexact->vectorthreshold)
	inexact->inexactflag = 1;

#endif

    if (ZERO(denom))    /* Ray is parallel to triangle, what about a robust check for near parallel? */
	return 0;

    M3D_VectorSubStore(sv0, src, v0);
    num = -M3D_VectorDotProduct(normal, sv0);
    distance = num / denom;


#ifdef MC_ENABLE_INEXACTFLAG_SUPPORT

    if (distance < -inexact->segmentthreshold)  /* Ray aimed away from triangle plane */
	return 0;

    if (distance > (1.0 + inexact->segmentthreshold)) /* Ray/Plane intersection is beyond segment */
	return 0;

    inexactflag = 0;

    if ((distance > -inexact->segmentthreshold) && (distance < inexact->segmentthreshold))
	inexactflag = 1;

    if ((distance > (1.0 - inexact->segmentthreshold)) && (distance < (1.0 + inexact->segmentthreshold)))
	inexactflag = 1;

#else

    if (distance < 0.0)  /* Ray aimed away from triangle plane */
	return 0;

    if (distance > 1.0)  /* Ray/Plane intersection is beyond segment */
	return 0;

#endif

    uu = M3D_VectorDotProduct(u, u);
    uv = M3D_VectorDotProduct(u, v);
    vv = M3D_VectorDotProduct(v, v);
    d = (uv * uv) - (uu * vv);


    if (ZERO(d))
	return 0;

    d = 1.0 / d;

    planehit[0] = src[0] + (distance * vector[0]);
    planehit[1] = src[1] + (distance * vector[1]);
    planehit[2] = src[2] + (distance * vector[2]);


    M3D_VectorSubStore(w, planehit, v0);
    wu = M3D_VectorDotProduct(w, u);
    wv = M3D_VectorDotProduct(w, v);
    s = ((uv * wv) - (vv * wu)) * d;


    if ((s < 0.0) || (s > 1.0))
	return 0;

    t = ((uv * wu) - (uu * wv)) * d;


    if ((t < 0.0) || ((s + t) > 1.0))
	return 0;

#ifdef MC_ENABLE_INEXACTFLAG_SUPPORT
    inexact->inexactflag = inexactflag;

    if (distance < 0.0)  /* Ray aimed away from triangle plane */
	return 0;

    if (distance > 1.0)  /* Ray/Plane intersection is beyond segment */
	return 0;

#endif
    M3D_VectorCopy(hit, planehit);
    return 1;
}


/****/


#define MC_SEED_TRIBUFFER_SIZE (256)

static int mcGetSeedtriangle(mcMesh *mesh)
{
    int attemptcount, tribuffercount, bufferoffset;
    mci tribuffer[MC_SEED_TRIBUFFER_SIZE];
    mci triindex, bufferindex, seedindex;
    mci indices[3], indtemp, bestvertexindex;
    mcf refplane[4];
    mcf distance, bestdistance, worstdistance, midpoint[3], signfactor, middistance, dispdistance;
    mcf raysrc[3], raydst[3], rayvector[3];
    void *indsrc;
    void *vertex0, *vertex1, *vertex2;
    mcf v0f[3], v1f[3], v2f[3];
    mcf vecta[3], vectb[3], normal[3];
    mcIntersectInexact inexact;


    /* Set up initial exactitude thresholds */
    inexact.vectorthreshold = MC_INEXACT_VECTOR_THRESHOLD;
    inexact.segmentthreshold = MC_INEXACT_SEGMENT_THRESHOLD;

    /* Attempt multiple times in the rare case where we would detect numerical accuracy issues with the reference plane picked */
    tribuffercount = 0;

    for (attemptcount = 0 ; attemptcount < MC_SEED_ATTEMPT_COUNT_MAX ; attemptcount++) {
	inexact.inexactflag = 0;

	/* Give up after a certain count of global attempts */
	if (--mesh->attemptcount < 0)
	    return -1;


	for (; ;) {
	    refplane[0] = ((mcf)(ccQuickRand32(&mesh->randstate) & 0xffff) / 65535.0) - 0.5;
	    refplane[1] = ((mcf)(ccQuickRand32(&mesh->randstate) & 0xffff) / 65535.0) - 0.5;
	    refplane[2] = ((mcf)(ccQuickRand32(&mesh->randstate) & 0xffff) / 65535.0) - 0.5;

	    if (M3D_VectorMagnitude(refplane) > 0.01)
		break;
	}

	M3D_VectorNormalize(mcf, refplane);
	refplane[3] = 0.0;


	/* Loop through all unused triangles, find the right-most vertex and buffer up all triangles that connect to it */
	bestdistance = -FLT_MAX;
	worstdistance = FLT_MAX;
	bestvertexindex = -1;

	for (triindex = 0 ; triindex < (mci)mesh->tricount ; triindex++) {
	    /* Skip triangles that have already been checked */
	    if (mmBitMapDirectGet(&mesh->tribitmap, triindex))
		continue;

	    /* Get triangle indices */
	    indsrc = ADDRESS(mesh->indices, triindex * mesh->indicesstride);
	    mesh->indicesUserToNative(indices, indsrc);
	    vertex0 = ADDRESS(mesh->vertex, indices[0] * mesh->vertexstride);
	    vertex1 = ADDRESS(mesh->vertex, indices[1] * mesh->vertexstride);
	    vertex2 = ADDRESS(mesh->vertex, indices[2] * mesh->vertexstride);

	    /* Find most distant vertex from plane */
	    mesh->vertexUserToNative(v0f, vertex0);
	    mesh->vertexUserToNative(v1f, vertex1);
	    mesh->vertexUserToNative(v2f, vertex2);
	    distance = M3D_PlanePoint(refplane, v0f);

	    if (distance > bestdistance) {
		bestdistance = distance;
		bestvertexindex = indices[0];
		tribuffercount = 0;
	    }

	    worstdistance = FMIN(worstdistance, distance);
	    distance = M3D_PlanePoint(refplane, v1f);

	    if (distance > bestdistance) {
		bestdistance = distance;
		bestvertexindex = indices[1];
		tribuffercount = 0;
	    }

	    worstdistance = FMIN(worstdistance, distance);
	    distance = M3D_PlanePoint(refplane, v2f);

	    if (distance > bestdistance) {
		bestdistance = distance;
		bestvertexindex = indices[2];
		tribuffercount = 0;
	    }

	    worstdistance = FMIN(worstdistance, distance);

	    /* If triangle doesn't connect our bestvertex, do not buffer up */
	    if ((indices[0] != bestvertexindex) && (indices[1] != bestvertexindex) && (indices[2] != bestvertexindex))
		continue;

	    /* Accumulate triangles sharing the vertex */
	    if (tribuffercount >= MC_SEED_TRIBUFFER_SIZE)
		break;

	    tribuffer[tribuffercount++] = triindex;

	}


	/* No buffered triangle means all triangles have been processed */
	if (!(tribuffercount))
	    return -1;

	/* Trace a ray from outside towards the center of a buffer triangle */
	seedindex = -1;
	bufferoffset = ccQuickRand32(&mesh->randstate) % tribuffercount;

	for (bufferindex = 0 ; bufferindex < tribuffercount ; bufferindex++) {
	    triindex = tribuffer[(bufferindex + bufferoffset) % tribuffercount ];
	    indsrc = ADDRESS(mesh->indices, triindex * mesh->indicesstride);
	    mesh->indicesUserToNative(indices, indsrc);
	    vertex0 = ADDRESS(mesh->vertex, indices[0] * mesh->vertexstride);
	    vertex1 = ADDRESS(mesh->vertex, indices[1] * mesh->vertexstride);
	    vertex2 = ADDRESS(mesh->vertex, indices[2] * mesh->vertexstride);
	    mesh->vertexUserToNative(v0f, vertex0);
	    mesh->vertexUserToNative(v1f, vertex1);
	    mesh->vertexUserToNative(v2f, vertex2);

	    /* Set the ray origin to midpoint of triangle displaced on target axis */
	    midpoint[0] = (1.0 / 3.0) * (v0f[0] + v1f[0] + v2f[0]);
	    midpoint[1] = (1.0 / 3.0) * (v0f[1] + v1f[1] + v2f[1]);
	    midpoint[2] = (1.0 / 3.0) * (v0f[2] + v1f[2] + v2f[2]);


	    /* Define ray origin as beyond plane, perpendicular to plane normal, heading towards midpoint */
	    middistance = M3D_PlanePoint(refplane, midpoint);
	    dispdistance = bestdistance - middistance;

	    /* Displace origin a little beyond the plane */
	    dispdistance += FMAX((1.0 / 16.0) * (bestdistance - worstdistance), 0.000001);

	    raysrc[0] = midpoint[0] + (dispdistance * refplane[0]);
	    raysrc[1] = midpoint[1] + (dispdistance * refplane[1]);
	    raysrc[2] = midpoint[2] + (dispdistance * refplane[2]);

	    /* Trace ray, store intersection point into raydst */
	    rayvector[0] = midpoint[0] - raysrc[0];
	    rayvector[1] = midpoint[1] - raysrc[1];
	    rayvector[2] = midpoint[2] - raysrc[2];


	    if (mcIntersectRayTriangle(raysrc, rayvector, v0f, v1f, v2f, raydst, &inexact)) {
		seedindex = triindex;
		break;
	    }
	}

	/* If no triangle was hit by our ray, try again with a new reference plane */
	if (seedindex == -1) {
	    continue;
	}


	/* Find the first hit along ray */
	for (triindex = 0 ; triindex < (mci)mesh->tricount ; triindex++) {
	    /* Skip triangles that have already been checked */
	    if ((attemptcount >= MC_SEED_ATTEMPT_SAFETY_THRESHOLD) && (mmBitMapDirectGet(&mesh->tribitmap, triindex)))
		continue;

	    indsrc = ADDRESS(mesh->indices, triindex * mesh->indicesstride);
	    mesh->indicesUserToNative(indices, indsrc);
	    vertex0 = ADDRESS(mesh->vertex, indices[0] * mesh->vertexstride);
	    vertex1 = ADDRESS(mesh->vertex, indices[1] * mesh->vertexstride);
	    vertex2 = ADDRESS(mesh->vertex, indices[2] * mesh->vertexstride);
	    mesh->vertexUserToNative(v0f, vertex0);
	    mesh->vertexUserToNative(v1f, vertex1);
	    mesh->vertexUserToNative(v2f, vertex2);

	    /* Check if that triangle is on top of our current target */
	    if (mcIntersectSegmentTriangle(raysrc, raydst, v0f, v1f, v2f, raydst, &inexact)) {
		seedindex = triindex;
	    }
	}

	/* If seedindex has already been processed, try again without modifying thresholds */
	if (mmBitMapDirectGet(&mesh->tribitmap, seedindex))
	    continue;


	/* We may hit floating point inaccuracy issues somewhere in our calculations */
	/* If inexactflag is raised, do NOT accept the result, pick a different reference plane and try again */
	if (!(inexact.inexactflag))
	    break;

	/* For the next pass, reduce accuracy thresholds a little */
	inexact.vectorthreshold *= MC_INEXACT_THRESHOLD_ATTEMPT_FACTOR;
	inexact.segmentthreshold *= MC_INEXACT_THRESHOLD_ATTEMPT_FACTOR;
    }

    if (seedindex == -1)
	return -1;

    /* We now know that our target triangle should have a normal looking away from our rayvector */
    mesh->indicesUserToNative(indices, ADDRESS(mesh->indices, seedindex * mesh->indicesstride));
    vertex0 = ADDRESS(mesh->vertex, indices[0] * mesh->vertexstride);
    vertex1 = ADDRESS(mesh->vertex, indices[1] * mesh->vertexstride);
    vertex2 = ADDRESS(mesh->vertex, indices[2] * mesh->vertexstride);
    mesh->vertexUserToNative(v0f, vertex0);
    mesh->vertexUserToNative(v1f, vertex1);
    mesh->vertexUserToNative(v2f, vertex2);

    /* Compute normal of triangle to decide if we need to flip */
    M3D_VectorSubStore(vecta, v1f, v0f);
    M3D_VectorSubStore(vectb, v2f, v0f);
    M3D_VectorCrossProduct(normal, vectb, vecta);
    signfactor = 1.0;

    if (mesh->flags & MC_FLAGS_TRIANGLE_WINDING_CCW)
	signfactor = -1.0;


    if ((signfactor * M3D_VectorDotProduct(normal, rayvector)) > 0.0) {
	/* If facing towards ray, seed triangle requires flipping */
	indtemp = indices[2];
	indices[2] = indices[1];
	indices[1] = indtemp;
	mesh->indicesNativeToUser(ADDRESS(mesh->indices, seedindex * mesh->indicesstride), indices);
    } else {
    }

    mmBitMapDirectSet(&mesh->tribitmap, seedindex);

    mesh->processcount++;
    return seedindex;
}


/****/


typedef struct {
    int tricount;
    int trialloc;
    mci *trilist;
} mcEdgeTriBuffer;

static int mcEdgeTriBufferInit(mcEdgeTriBuffer *tribuffer)
{
    tribuffer->tricount = 0;
    tribuffer->trialloc = 256;
    tribuffer->trilist = (mci *)malloc(tribuffer->trialloc * sizeof(mci));
    return 1;
}

static void mcEdgeTriBufferReset(mcEdgeTriBuffer *tribuffer)
{
    tribuffer->tricount = 0;
    return;
}

static int mcEdgeTriBufferAdd(mcEdgeTriBuffer *tribuffer, mci triindex)
{
    if (tribuffer->tricount >= tribuffer->trialloc) {
	tribuffer->trialloc <<= 1;
	tribuffer->trilist = (mci *)realloc(tribuffer->trilist, tribuffer->trialloc * sizeof(mci));
    }

    tribuffer->trilist[tribuffer->tricount++] = triindex;
    return 1;
}

static void mcEdgeTriBufferFree(mcEdgeTriBuffer *tribuffer)
{
    free(tribuffer->trilist);
    return;
}


/****/


static int mcEdgeHashListBufferAdd(void *opaque, void *entry, void *entryref)
{
    mcEdge *edge, *edgeref;
    mcEdgeTriBuffer *tribuffer;

    edge = (mcEdge *)entry;
    edgeref = (mcEdge *)entryref;

    if (edge->v[0] == -1)
	return MM_HASH_ENTRYLIST_BREAK;

    if ((edge->v[0] == edgeref->v[0]) && (edge->v[1] == edgeref->v[1]) && (edge->triindex != edgeref->triindex)) {
	tribuffer = (mcEdgeTriBuffer *)opaque;
	mcEdgeTriBufferAdd(tribuffer, edge->triindex);
    }

    return MM_HASH_ENTRYLIST_CONTINUE;
}


static mmHashAccess mcEdgeHashBufferAdd = {
    mcEdgeHashClearEntry,
    mcEdgeHashEntryValid,
    mcEdgeHashEntryKey,
    mcEdgeHashEntryCmp,
    mcEdgeHashListBufferAdd
};

static void mcQueueNewOp(mcMesh *mesh, void **oplist, mci v0, mci v1, mci triindex)
{
    mcOp *op;
    op = (mcOp *)mmBlockAlloc(&mesh->opblock);
    op->v0 = v0;
    op->v1 = v1;
    op->triindex = triindex;


    mmListAdd(oplist, op, offsetof(mcOp, list));
    return;
}


static void mcPerformCorrection(mcMesh *mesh)
{
    int flipflag;
    mci seedindex, triindex;
    mcEdge edge;
    mcEdgeTriBuffer tribuffer;
    mci indices[3], indtemp;
    mcOp *op;
    void *oplist;

    mcEdgeTriBufferInit(&tribuffer);

    mesh->processcount = 0;
    oplist = 0;

    for (; ;) {
	/* Find new seed triangles */
	seedindex = mcGetSeedtriangle(mesh);

	if (seedindex == -1)
	    break;

	/* Add the 3 edges of seed to op list */
	mesh->indicesUserToNative(indices, ADDRESS(mesh->indices, seedindex * mesh->indicesstride));
	mcQueueNewOp(mesh, &oplist, indices[0], indices[1], seedindex);
	mcQueueNewOp(mesh, &oplist, indices[1], indices[2], seedindex);
	mcQueueNewOp(mesh, &oplist, indices[2], indices[0], seedindex);


	/* Process all pending edges */
	for (op = (mcOp *)oplist ; op ; op = (mcOp *)oplist) {

	    /* Accumulate results for edge in both direction */
	    mcEdgeTriBufferReset(&tribuffer);
	    edge.triindex = op->triindex;
	    edge.v[0] = op->v0;
	    edge.v[1] = op->v1;
	    mmHashDirectListEntry(mesh->edgehashtable, &mcEdgeHashBufferAdd, &edge, &tribuffer);
	    edge.v[0] = op->v1;
	    edge.v[1] = op->v0;
	    mmHashDirectListEntry(mesh->edgehashtable, &mcEdgeHashBufferAdd, &edge, &tribuffer);


	    /* If edge is uncertain due to multiple linked edges, do not process */
	    if (tribuffer.tricount != 1)
		goto next;

	    /* If linked triangle has already been processed, check */
	    triindex = tribuffer.trilist[0];

	    /* Check if we have already processed that edge */
	    if (mmBitMapDirectGet(&mesh->tribitmap, triindex)) {
		goto next;
	    }


	    /* Check the linked triangle, flip if it doesn't face the same way */
	    flipflag = 0;
	    mesh->indicesUserToNative(indices, ADDRESS(mesh->indices, triindex * mesh->indicesstride));

	    if (indices[0] == op->v0) {
		if (indices[1] == op->v1)
		    flipflag = 1;
	    } else if (indices[1] == op->v0) {
		if (indices[2] == op->v1)
		    flipflag = 1;
	    } else if (indices[2] == op->v0) {
		if (indices[0] == op->v1)
		    flipflag = 1;
	    } else
		bu_bomb("SHOULD NOT HAPPEN");


	    /* If necessary, flip the indices and write them back */
	    mmBitMapDirectSet(&mesh->tribitmap, triindex);
	    mesh->processcount++;

	    if (flipflag) {
		indtemp = indices[2];
		indices[2] = indices[1];
		indices[1] = indtemp;
		mesh->indicesNativeToUser(ADDRESS(mesh->indices, triindex * mesh->indicesstride), indices);
	    }

	    /* Add all edges except the one we come from */
	    if (((indices[0] != op->v0) || (indices[1] != op->v1)) && ((indices[0] != op->v1) || (indices[1] != op->v0)))
		mcQueueNewOp(mesh, &oplist, indices[0], indices[1], triindex);

	    if (((indices[1] != op->v0) || (indices[2] != op->v1)) && ((indices[1] != op->v1) || (indices[2] != op->v0)))
		mcQueueNewOp(mesh, &oplist, indices[1], indices[2], triindex);

	    if (((indices[2] != op->v0) || (indices[0] != op->v1)) && ((indices[2] != op->v1) || (indices[0] != op->v0)))
		mcQueueNewOp(mesh, &oplist, indices[2], indices[0], triindex);

	next:
	    /* Remove op from list and free */
	    mmListRemove(op, offsetof(mcOp, list));
	    mmBlockRelease(&mesh->opblock, op);
	}
    }

    mcEdgeTriBufferFree(&tribuffer);

    return;
}


int mcMeshCorrection(size_t vertexcount, void *vertex, int vertexwidth, size_t vertexstride, size_t tricount, void *indices, int indiceswidth, size_t indicesstride, long attemptcount, int flags)
{
    mcMesh mesh;

    mesh.vertexcount = vertexcount;
    mesh.vertex = vertex;
    mesh.vertexwidth = vertexwidth;
    mesh.vertexstride = vertexstride;
    mesh.tricount = tricount;
    mesh.indices = indices;
    mesh.indiceswidth = indiceswidth;
    mesh.indicesstride = indicesstride;
    mesh.attemptcount = attemptcount;
    mesh.flags = flags;

    switch (indiceswidth) {
	case sizeof(uint8_t):
	    mesh.indicesUserToNative = mcIndicesInt8ToNative;
	    mesh.indicesNativeToUser = mcIndicesNativeToInt8;
	    break;

	case sizeof(uint16_t):
	    mesh.indicesUserToNative = mcIndicesInt16ToNative;
	    mesh.indicesNativeToUser = mcIndicesNativeToInt16;
	    break;

	case sizeof(uint32_t):
	    mesh.indicesUserToNative = mcIndicesInt32ToNative;
	    mesh.indicesNativeToUser = mcIndicesNativeToInt32;
	    break;

	case sizeof(uint64_t):
	    mesh.indicesUserToNative = mcIndicesInt64ToNative;
	    mesh.indicesNativeToUser = mcIndicesNativeToInt64;
	    break;

	default:
	    return 0;
    }

    switch (vertexwidth) {
	case sizeof(float):
	    mesh.vertexUserToNative = mcVertexFloatToNative;
	    mesh.vertexNativeToUser = mcVertexNativeToFloat;
	    break;

	case sizeof(double):
	    mesh.vertexUserToNative = mcVertexDoubleToNative;
	    mesh.vertexNativeToUser = mcVertexNativeToDouble;
	    break;

	default:
	    return 0;
    }

    if (tricount < 1)
	return 0;

    ccQuickRand32Seed(&mesh.randstate, time(0) + tricount);

    mesh.edgehashtable = mcMeshHashInit(tricount, 1.5, 7);

    if (!(mesh.edgehashtable))
	return 0;

    mmBitMapInit(&mesh.tribitmap, tricount, 0);
    mmBlockInit(&mesh.opblock, sizeof(mcOp), 16384, 16384, 0x10);

    mcHashBuild(&mesh);

    mcPerformCorrection(&mesh);

    free(mesh.edgehashtable);
    mmBitMapFree(&mesh.tribitmap);
    mmBlockFreeAll(&mesh.opblock);

    return 1;
}
