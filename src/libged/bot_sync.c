/*                         B O T _ S Y N C . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file bot_sync.c
 *
 * The bot_sync command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "rtgeom.h"

#include "./ged_private.h"


int
ged_bot_sync(struct ged *gedp, int argc, const char *argv[])
{
    register int i;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    static const char *usage = "bot [bot2 bot3 ...]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    for (i=1; i < argc; ++i) {
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_QUIET)) == DIR_NULL) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: db_lookup(%s) error\n", argv[0], argv[i]);
	    continue;
	}

	if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, bn_mat_identity, &rt_uniresource) < 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: rt_db_get_internal(%s) error\n", argv[0], argv[i]);
	    return GED_ERROR;
	}

	if (intern.idb_type != ID_BOT) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: %s is not a BOT solid!!!\n", argv[0], argv[i]);
	    continue;
	}

	bot = (struct rt_bot_internal *)intern.idb_ptr;
	rt_bot_sync(bot);

	if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, gedp->ged_wdbp->wdb_resp)) {
	    bu_vls_printf(&gedp->ged_result_str,
			  "Failed to write BOT (%s) to database!!!",
			  dp->d_namep);
	    rt_db_free_internal(&intern, gedp->ged_wdbp->wdb_resp);
	    return GED_ERROR;
	}
    }

    return GED_OK;
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
