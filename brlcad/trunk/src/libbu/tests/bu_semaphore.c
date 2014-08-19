/*                  B U _ S E M A P H O R E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file bu_semaphore.c
 *
 * Tests libbu semaphore locking.
 *
 */

#include "common.h"

#include "bu.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#  include <signal.h>
#else
#  include <windows.h>
#endif


const int SEM = BU_SEM_LAST+1;


struct increment_thread_args {
    size_t ncpu;
    int *parallel;
    int *running;
    size_t reps;
    size_t *counter;
};


#ifdef HAVE_UNISTD_H
static void
_exit_alarm_handler(int sig)
{
    if (sig == SIGALRM)
	exit(0);
}
#else
static void CALLBACK
_exit_alarm_handler(UINT UNUSED(uTimerID), UINT UNUSED(uMsg), DWORD_PTR UNUSED(dwUser),	DWORD_PTR UNUSED(dw1), DWORD_PTR UNUSED(dw2))
{
    exit(0);
}
#endif


static int
set_exit_alarm(unsigned seconds)
{
#ifdef HAVE_UNISTD_H
    signal(SIGALRM, _exit_alarm_handler);
    alarm(seconds);
    return 1;
#else
    return !!timeSetEvent(seconds*1000, 100, (LPTIMECALLBACK)_exit_alarm_handler, (DWORD_PTR)NULL, TIME_ONESHOT);
#endif
}


static int
repeat_test(size_t reps)
{
    size_t i;

    for (i = 0; i < reps; i++)
	bu_semaphore_init(SEM+1);

    for (i = 0; i < reps; i++)
	bu_semaphore_free();

    return 1;
}



static int
single_thread_test(void)
{
    if (!set_exit_alarm(1)) {
	bu_log("failed to start alarm; skipping single-thread bu_semaphore test");
	return 1;
    }

    bu_semaphore_init(SEM+1);
    bu_semaphore_acquire(SEM);
    bu_semaphore_acquire(SEM);
    bu_semaphore_free();

    bu_log("single-thread bu_semaphore test failed");
    return 0;
}


static void
increment_thread(int cpu, void *pargs)
{
    struct increment_thread_args *args = (struct increment_thread_args *)pargs;
    size_t i = 0;

    bu_semaphore_acquire(SEM);
    if (*args->running)
	*args->parallel = 1;
    *args->running = cpu+1;
    bu_semaphore_release(SEM);

    for (i = 0; i < args->reps; i++) {
	bu_semaphore_acquire(SEM);
	++*args->counter;
	bu_semaphore_release(SEM);
    }

    while (args->ncpu > 1 && i++ < UINT32_MAX-1 && !*args->parallel) {
	bu_semaphore_acquire(SEM);
	++*args->counter;
	--*args->counter;
	bu_semaphore_release(SEM);
    }

    bu_semaphore_acquire(SEM);
    *args->running = 0;
    bu_semaphore_release(SEM);

    return;
}


static int
parallel_test(size_t ncpu, size_t reps)
{
    int parallel = 0;
    int running = 0;

    size_t counter = 0;
    size_t expected = reps * ncpu;

    struct increment_thread_args args;

    args.ncpu = ncpu;
    args.parallel = &parallel;
    args.running = &running;
    args.reps = reps;
    args.counter = &counter;

    bu_semaphore_init(SEM+1);
    bu_parallel(increment_thread, ncpu, &args);
    bu_semaphore_free();

    if (counter != expected) {
	bu_log("bu_semaphore parallel increment test:  counter is %lu, expected %lu\n [FAIL]", counter, expected);
	return 0;
    }

    if ((ncpu > 1) && !parallel) {
	bu_log("bu_semaphore parallel thread test:  did not run in parallel [FAIL]\n");
	return 0;
    }

    return 1;
}


int
main(int argc, char **argv)
{
    const char * const USAGE = "Usage: %s [-P ncpu] [-n reps]\n";

    int c;
    int success;
    size_t nreps = 1000;
    unsigned long nreps_opt;
    size_t ncpu = bu_avail_cpus();
    unsigned long ncpu_opt;

    while ((c = bu_getopt(argc, argv, "n:P:")) != -1) {
	switch (c) {
	    case 'n':
		nreps_opt = (uint32_t)strtoul(bu_optarg, NULL, 0);
		if (nreps_opt > 0 && nreps_opt < UINT32_MAX-1)
		    nreps = (size_t)nreps_opt;
		break;
	    case 'P':
		ncpu_opt = (size_t)strtoul(bu_optarg, NULL, 0);
		if (ncpu_opt > 0 && ncpu_opt < ncpu)
		    ncpu = ncpu_opt;
		break;
	    default:
		bu_exit(1, USAGE, argv[0]);
	}
    }

    /* nreps is a minimum */
    success = repeat_test(nreps) && parallel_test(ncpu, nreps);

    /* single_thread_test should lock up and be killed by alarm */
    return !(success && single_thread_test());

}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
