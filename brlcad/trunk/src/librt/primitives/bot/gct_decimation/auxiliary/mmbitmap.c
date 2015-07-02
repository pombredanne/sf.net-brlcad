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

#include "common.h"


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "cpuconfig.h"
#include "cc.h"
#include "mm.h"
#include "mmbitmap.h"


#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wunused-function"
#endif




/* Friendlier to cache on SMP systems */
#define BP_BITMAP_PREWRITE_CHECK


/*
TODO
If we don't have atomic instruction support, we need a much better mutex locking mechanism!!
*/


void mmBitMapInit(mmBitMap *bitmap, size_t entrycount, int initvalue)
{
    size_t mapsize, vindex;
    long value;
#ifdef MM_ATOMIC_SUPPORT
    mmAtomicL *map;
#else
    long *map;
#endif

    mapsize = (entrycount + CPUCONF_LONG_BITS - 1) >> CPUCONF_LONG_BITSHIFT;
    bitmap->map = (mmAtomic64 *)malloc(mapsize * sizeof(mmAtomicL));
    bitmap->mapsize = mapsize;
    bitmap->entrycount = entrycount;

    map = bitmap->map;
    value = (initvalue & 0x1 ? ~0x0 : 0x0);
#ifdef MM_ATOMIC_SUPPORT

    for (vindex = 0 ; vindex < mapsize ; vindex++)
	mmAtomicWriteL(&map[vindex], value);

#else

    for (vindex = 0 ; vindex < mapsize ; vindex++)
	map[vindex] = value;

    mtMutexInit(&bitmap->mutex);
#endif

    return;
}

void mmBitMapReset(mmBitMap *bitmap, int resetvalue)
{
    size_t vindex;
    long value;
#ifdef MM_ATOMIC_SUPPORT
    mmAtomicL *map;
#else
    long *map;
#endif

    map = bitmap->map;
    value = (resetvalue & 0x1 ? ~(long)0x0 : (long)0x0);
#ifdef MM_ATOMIC_SUPPORT

    for (vindex = 0 ; vindex < bitmap->mapsize ; vindex++)
	mmAtomicWriteL(&map[vindex], value);

#else

    for (vindex = 0 ; vindex < bitmap->mapsize ; vindex++)
	map[vindex] = value;

#endif

    return;
}

void mmBitMapFree(mmBitMap *bitmap)
{
    free(bitmap->map);
    bitmap->map = 0;
    bitmap->mapsize = 0;
#ifndef MM_ATOMIC_SUPPORT
    mtMutexDestroy(&bitmap->mutex);
#endif
    return;
}

int mmBitMapDirectGet(mmBitMap *bitmap, size_t entryindex)
{
    int value;
    size_t vindex, shift;
    vindex = entryindex >> CPUCONF_LONG_BITSHIFT;
    shift = entryindex & (CPUCONF_LONG_BITS - 1);
#ifdef MM_ATOMIC_SUPPORT
    value = (MM_ATOMIC_ACCESS_L(&bitmap->map[vindex]) >> shift) & 0x1;
#else
    value = (bitmap->map[vindex] >> shift) & 0x1;
#endif
    return value;
}

void mmBitMapDirectSet(mmBitMap *bitmap, size_t entryindex)
{
    size_t vindex, shift;
    vindex = entryindex >> CPUCONF_LONG_BITSHIFT;
    shift = entryindex & (CPUCONF_LONG_BITS - 1);
#ifdef MM_ATOMIC_SUPPORT
    MM_ATOMIC_ACCESS_L(&bitmap->map[vindex]) |= (long)1 << shift;
#else
    bitmap->map[vindex] |= (long)1 << shift;
#endif
    return;
}

void mmBitMapDirectClear(mmBitMap *bitmap, size_t entryindex)
{
    size_t vindex, shift;
    vindex = entryindex >> CPUCONF_LONG_BITSHIFT;
    shift = entryindex & (CPUCONF_LONG_BITS - 1);
#ifdef MM_ATOMIC_SUPPORT
    MM_ATOMIC_ACCESS_L(&bitmap->map[vindex]) &= ~((long)1 << shift);
#else
    bitmap->map[vindex] &= ~((long)1 << shift);
#endif
    return;
}

int mmBitMapGet(mmBitMap *bitmap, size_t entryindex)
{
    int value;
    size_t vindex, shift;
    vindex = entryindex >> CPUCONF_LONG_BITSHIFT;
    shift = entryindex & (CPUCONF_LONG_BITS - 1);
#ifdef MM_ATOMIC_SUPPORT
    value = (mmAtomicReadL(&bitmap->map[vindex]) >> shift) & 0x1;
#else
    mtMutexLock(&bitmap->mutex);
    value = (bitmap->map[vindex] >> shift) & 0x1;
    mtMutexUnlock(&bitmap->mutex);
#endif
    return value;
}

void mmBitMapSet(mmBitMap *bitmap, size_t entryindex)
{
    size_t vindex, shift;
    vindex = entryindex >> CPUCONF_LONG_BITSHIFT;
    shift = entryindex & (CPUCONF_LONG_BITS - 1);
#ifdef MM_ATOMIC_SUPPORT
#ifdef BP_BITMAP_PREWRITE_CHECK

    if (!(mmAtomicReadL(&bitmap->map[vindex]) & ((long)1 << shift)))
	mmAtomicOrL(&bitmap->map[vindex], (long)1 << shift);

#else
    mmAtomicOrL(&bitmap->map[vindex], (long)1 << shift);
#endif
#else
    mtMutexLock(&bitmap->mutex);
    bitmap->map[vindex] |= (long)1 << shift;
    mtMutexUnlock(&bitmap->mutex);
#endif
    return;
}

void mmBitMapClear(mmBitMap *bitmap, size_t entryindex)
{
    size_t vindex, shift;
    vindex = entryindex >> CPUCONF_LONG_BITSHIFT;
    shift = entryindex & (CPUCONF_LONG_BITS - 1);
#ifdef MM_ATOMIC_SUPPORT
#ifdef BP_BITMAP_PREWRITE_CHECK

    if (mmAtomicReadL(&bitmap->map[vindex]) & ((long)1 << shift))
	mmAtomicAndL(&bitmap->map[vindex], ~((long)1 << shift));

#else
    mmAtomicAndL(&bitmap->map[vindex], ~((long)1 << shift));
#endif
#else
    mtMutexLock(&bitmap->mutex);
    bitmap->map[vindex] &= ~((long)1 << shift);
    mtMutexUnlock(&bitmap->mutex);
#endif
    return;
}

int mmBitMapMaskGet(mmBitMap *bitmap, size_t entryindex, long mask)
{
    int value;
    size_t vindex, shift;
    vindex = entryindex >> CPUCONF_LONG_BITSHIFT;
    shift = entryindex & (CPUCONF_LONG_BITS - 1);
#ifdef MM_ATOMIC_SUPPORT
    value = (mmAtomicReadL(&bitmap->map[vindex]) >> shift) & mask;
#else
    mtMutexLock(&bitmap->mutex);
    value = (bitmap->map[vindex] >> shift) & mask;
    mtMutexUnlock(&bitmap->mutex);
#endif
    return value;
}

void mmBitMapMaskSet(mmBitMap *bitmap, size_t entryindex, long value, long mask)
{
    size_t vindex, shift;
    long oldvalue, newvalue;
    vindex = entryindex >> CPUCONF_LONG_BITSHIFT;
    shift = entryindex & (CPUCONF_LONG_BITS - 1);
#ifdef MM_ATOMIC_SUPPORT

    for (; ;) {
	oldvalue = mmAtomicReadL(&bitmap->map[vindex]);
	newvalue = (oldvalue & ~(mask << shift)) | (value << shift);

	if (mmAtomicCmpReplaceL(&bitmap->map[vindex], oldvalue, newvalue))
	    break;
    }

#else
    mtMutexLock(&bitmap->mutex);
    bitmap->map[vindex] = (bitmap->map[vindex] & ~(mask << shift)) | (value << shift);
    mtMutexUnlock(&bitmap->mutex);
#endif
    return;
}
