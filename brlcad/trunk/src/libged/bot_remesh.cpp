/*                     B O T _ R E M E S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019 United States Government as represented by
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
/** @file libged/bot_remesh.cpp
 *
 * The bot "remesh" sub-command.
 *
 */

#include "common.h"

#ifdef OPENVDB_ABI_VERSION_NUMBER
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/VolumeToMesh.h>
#  include <openvdb/tools/MeshToVolume.h>
#endif /* OPENVDB_ABI_VERSION_NUMBER */

#include "vmath.h"
#include "bu/str.h"
#include "rt/db5.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/geom.h"
#include "rt/wdb.h"
#include "ged/commands.h"
#include "ged/database.h"
#include "ged/objects.h"


#ifdef OPENVDB_ABI_VERSION_NUMBER

struct botDataAdapter {
    struct rt_bot_internal *bot;

    size_t polygonCount() const {
	return bot->num_faces;
    };
    size_t pointCount() const {
	return bot->num_vertices;
    };
    size_t vertexCount(size_t) const {
	return 3;
    };
    void getIndexSpacePoint(size_t n, size_t v, openvdb::Vec3d& pos) const {
	int idx = bot->faces[n*3+v];
	pos[X] = bot->vertices[idx+X];
	pos[Y] = bot->vertices[idx+Y];
	pos[Z] = bot->vertices[idx+Z];
	return;
    };

    /* constructor */
    botDataAdapter(struct rt_bot_internal *bip) : bot(bip) {}
};


static bool
bot_remesh(struct ged *UNUSED(gedp), struct rt_bot_internal *bot)
{
    const float exteriorBandWidth = 3.0;
    const float interiorBandWidth = std::numeric_limits<float>::max();

    struct botDataAdapter bda(bot);

    openvdb::initialize();

    openvdb::math::Transform::Ptr xform = openvdb::math::Transform::createLinearTransform();
    openvdb::FloatGrid::Ptr bot2vol = openvdb::tools::meshToVolume<openvdb::FloatGrid, botDataAdapter>(bda, *xform, exteriorBandWidth, interiorBandWidth);

    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::Vec3I> triangles;
    std::vector<openvdb::Vec4I> quadrilaterals;
    openvdb::tools::volumeToMesh<openvdb::FloatGrid>(*bot2vol, points, triangles, quadrilaterals);

    if (bot->vertices) {
	bu_free(bot->vertices, "vertices");
	bot->num_vertices = 0;
    }
    if (bot->faces) {
	bu_free(bot->faces, "faces");
	bot->num_faces = 0;
    }
    if (bot->normals) {
	bu_free(bot->normals, "normals");
    }
    if (bot->face_normals) {
	bu_free(bot->face_normals, "face normals");
    }

    bot->num_vertices = points.size();
    bot->vertices = (fastf_t *)bu_malloc(bot->num_vertices * ELEMENTS_PER_POINT * sizeof(fastf_t), "vertices");
    for (size_t i = 0; i < points.size(); i++) {
	bot->vertices[i+X] = points[i].x();
	bot->vertices[i+Y] = points[i].y();
	bot->vertices[i+Z] = points[i].z();
    }
    bot->num_faces = triangles.size() + (quadrilaterals.size() * 2);
    bot->faces = (int *)bu_malloc(bot->num_faces * 3 * sizeof(int), "triangles");
    for (size_t i = 0; i < triangles.size(); i++) {
	bot->faces[i+X] = triangles[i].x();
	bot->faces[i+Y] = triangles[i].y();
	bot->faces[i+Z] = triangles[i].z();
    }
    size_t ntri = triangles.size();
    for (size_t i = 0; i < quadrilaterals.size(); i++) {
	bot->faces[ntri+i+X] = quadrilaterals[i][0];
	bot->faces[ntri+i+Y] = quadrilaterals[i][1];
	bot->faces[ntri+i+Z] = quadrilaterals[i][2];

	bot->faces[ntri+i+1+X] = quadrilaterals[i][0];
	bot->faces[ntri+i+1+Y] = quadrilaterals[i][2];
	bot->faces[ntri+i+1+Z] = quadrilaterals[i][3];
    }

    bu_log("npoints = %zu\n"
	   "ntris = %zu\n"
	   "nquads = %zu\n", points.size(), triangles.size(), quadrilaterals.size());

    return (points.size() > 0);
}

#else /* OPENVDB_ABI_VERSION_NUMBER */

static bool
bot_remesh(struct ged *gedp, struct rt_bot_internal *UNUSED(bot))
{
    bu_vls_printf(gedp->ged_result_str,
		  "WARNING: BoT remeshing is unavailable.\n"
		  "BRL-CAD needs to be compiled with OpenVDB support.\n"
		  "(cmake -DBRLCAD_ENABLE_OPENVDB=ON)\n");
    return false;
}

#endif /* OPENVDB_ABI_VERSION_NUMBER */


int
ged_bot_remesh(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "input.bot [output.bot]";

    char *input_bot_name;
    char *output_bot_name;
    struct directory *dp_input;
    struct directory *dp_output;
    struct rt_bot_internal *input_bot;
    struct rt_db_internal intern;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    dp_input = dp_output = RT_DIR_NULL;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* check that we are using a version 5 database */
    if (db_version(gedp->ged_wdbp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str,
		      "ERROR: Unable to remesh the current (v%d) database.\n"
		      "Use \"dbupgrade\" to upgrade this database to the current version.\n",
		      db_version(gedp->ged_wdbp->dbip));
	return GED_ERROR;
    }

    if (argc > 3) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: unexpected arguments encountered\n");
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    input_bot_name = output_bot_name = (char *)argv[1];
    if (argc > 2)
	output_bot_name = (char *)argv[2];

    if (!BU_STR_EQUAL(input_bot_name, output_bot_name)) {
	GED_CHECK_EXISTS(gedp, output_bot_name, LOOKUP_QUIET, GED_ERROR);
    }

    GED_DB_LOOKUP(gedp, dp_input, input_bot_name, LOOKUP_QUIET, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp_input, NULL, gedp->ged_wdbp->wdb_resp, GED_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a BOT primitive\n", input_bot_name);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    input_bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(input_bot);

    /* TODO: stash a backup if overwriting the original */

    bool ok = bot_remesh(gedp, input_bot);
    if (!ok) {
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(input_bot_name, output_bot_name)) {
	dp_output = dp_input;
    } else {
	GED_DB_DIRADD(gedp, dp_output, output_bot_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type, GED_ERROR);
    }

    GED_DB_PUT_INTERNAL(gedp, dp_output, &intern, gedp->ged_wdbp->wdb_resp, GED_ERROR);
    rt_db_free_internal(&intern);

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
