/*              F A S T G E N 4 _ W R I T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file fastgen4_write.cpp
 *
 * FASTGEN4 export plugin for libgcv.
 *
 */


#include "common.h"

#include <fstream>
#include <iomanip>
#include <stdexcept>

#include "../../plugin.h"


namespace
{


class FastgenWriter
{
public:
    FastgenWriter(const std::string &path);
    ~FastgenWriter();

    void write_comment(const std::string &value);


private:
    friend class Section;

    class Record;

    static const std::size_t MAX_GROUPS = 50;
    static const std::size_t MAX_COMPONENTS = 999;

    std::size_t m_next_section_id;
    bool m_record_open;
    std::ofstream m_ostream;
};


class FastgenWriter::Record
{
public:
    Record(FastgenWriter &writer);
    ~Record();

    template <typename T> Record &operator<<(const T &value);
    Record &text(const std::string &value);


private:
    static const std::size_t FIELD_WIDTH = 8;
    static const std::size_t RECORD_WIDTH = 10;

    std::size_t m_width;
    FastgenWriter &m_writer;
};


FastgenWriter::Record::Record(FastgenWriter &writer) :
    m_width(0),
    m_writer(writer)
{
    if (m_writer.m_record_open)
	throw std::logic_error("record open");

    m_writer.m_record_open = true;
}


FastgenWriter::Record::~Record()
{
    if (m_width) m_writer.m_ostream.put('\n');

    m_writer.m_record_open = false;
}


template <typename T> FastgenWriter::Record &
FastgenWriter::Record::operator<<(const T &value)
{
    if (++m_width > RECORD_WIDTH)
	throw std::logic_error("invalid record width");

    m_writer.m_ostream << std::left << std::setw(FIELD_WIDTH) << std::showpoint;
    m_writer.m_ostream << value;
    return *this;
}


FastgenWriter::Record &
FastgenWriter::Record::text(const std::string &value)
{
    m_width = RECORD_WIDTH;
    m_writer.m_ostream << value;
    return *this;
}


FastgenWriter::FastgenWriter(const std::string &path) :
    m_next_section_id(1),
    m_record_open(false),
    m_ostream(path.c_str(), std::ofstream::out)
{
    m_ostream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
}


FastgenWriter::~FastgenWriter()
{
    Record(*this) << "ENDDATA";
}


void
FastgenWriter::write_comment(const std::string &value)
{
    (Record(*this) << "$COMMENT").text(value);
}


class Section
{
public:
    Section(FastgenWriter &writer, const std::string &name, std::size_t group_id,
	    bool volume_mode);

    std::size_t add_grid_point(double x, double y, double z);

    void add_sphere(std::size_t g1, double thickness, double radius);

    void add_cone(std::size_t g1, std::size_t g2, double ro1, double ro2,
		  double ri1, double ri2);

    void add_line(std::size_t g1, std::size_t g2, double thickness, double radius);

    void add_triangle(std::size_t g1, std::size_t g2, std::size_t g3,
		      double thickness, bool grid_centered = false);

    void add_quad(std::size_t g1, std::size_t g2, std::size_t g3, std::size_t g4,
		  double thickness, bool grid_centered = false);

    void add_hexahedron(const std::size_t *g);


private:
    static const std::size_t MAX_GRID_POINTS = 50000;
    static const std::size_t MAX_HOLES = 40000;
    static const std::size_t MAX_WALLS = 40000;

    std::size_t m_next_grid_id;
    std::size_t m_next_element_id;
    FastgenWriter &m_writer;
};


Section::Section(FastgenWriter &writer, const std::string &name,
		 std::size_t group_id, bool volume_mode) :
    m_next_grid_id(1),
    m_next_element_id(1),
    m_writer(writer)
{
    if (group_id > FastgenWriter::MAX_GROUPS
	|| m_writer.m_next_section_id > FastgenWriter::MAX_COMPONENTS)
	throw std::invalid_argument("invalid id");

    {
	FastgenWriter::Record record(m_writer);
	record << "$NAME" << group_id << m_writer.m_next_section_id;
	record << "" << "" << "" << "";
	record.text(name);
    }

    FastgenWriter::Record(m_writer)
	    << "SECTION" << group_id << m_writer.m_next_section_id << (volume_mode ? 2 : 1);
    ++m_writer.m_next_section_id;
}


std::size_t
Section::add_grid_point(double x, double y, double z)
{
    if (m_next_grid_id > MAX_GRID_POINTS)
	throw std::length_error("maximum GRID records");

    FastgenWriter::Record record(m_writer);
    record << "GRID" << m_next_grid_id << "";
    record << x << y << z;
    return m_next_grid_id++;
}


void
Section::add_sphere(std::size_t g1, double thickness, double radius)
{
    if (thickness <= 0.0 || radius <= 0.0)
	throw std::invalid_argument("invalid value");

    FastgenWriter::Record(m_writer) << "CSPHERE" << m_next_element_id++ << 0 << g1
				    << "" << "" << "" << thickness << radius;
}


void
Section::add_cone(std::size_t g1, std::size_t g2, double ro1, double ro2,
		  double ri1, double ri2)
{
    if (g1 == g2 || g1 >= m_next_grid_id || g2 >= m_next_grid_id)
	throw std::invalid_argument("invalid grid id");

    if (ri1 <= 0.0 || ri2 <= 0.0 || ro1 <= ri2 || ro2 <= ri2)
	throw std::invalid_argument("invalid radius");

    FastgenWriter::Record(m_writer) << "CCONE2" << m_next_element_id << 0 << g1 <<
				    g2 << "" << "" << "" << ro1 << m_next_element_id;
    FastgenWriter::Record(m_writer) << m_next_element_id << ro2 << ri1 << ri2;
    ++m_next_element_id;
}


void
Section::add_line(std::size_t g1, std::size_t g2, double thickness,
		  double radius)
{
    if (thickness <= 0.0 || radius <= 0.0)
	throw std::invalid_argument("invalid value");

    if (g1 == g2 || g1 >= m_next_grid_id || g2 >= m_next_grid_id)
	throw std::invalid_argument("invalid grid id");

    FastgenWriter::Record(m_writer) << "CLINE" << m_next_element_id++ << 0 << g1 <<
				    g2 << thickness << radius;
}


void
Section::add_triangle(std::size_t g1, std::size_t g2, std::size_t g3,
		      double thickness, bool grid_centered)
{
    if (thickness <= 0.0)
	throw std::invalid_argument("invalid thickness");

    if (g1 == g2 || g1 == g3 || g2 == g3)
	throw std::invalid_argument("invalid grid id");

    if (g1 >= m_next_grid_id || g2 >= m_next_grid_id || g3 >= m_next_grid_id)
	throw std::invalid_argument("invalid grid id");

    FastgenWriter::Record(m_writer) << "CTRI" << m_next_element_id++ << 0 << g1 <<
				    g2 << g3 << thickness << (grid_centered ? 1 : 2);
}


void
Section::add_quad(std::size_t g1, std::size_t g2, std::size_t g3,
		  std::size_t g4, double thickness, bool grid_centered)
{
    if (thickness <= 0.0)
	throw std::invalid_argument("invalid thickness");

    if (g1 == g2 || g1 == g3 || g1 == g4 || g2 == g3 || g2 == g4 || g3 == g4)
	throw std::invalid_argument("repeated grid id");

    if (g1 >= m_next_grid_id || g2 >= m_next_grid_id || g3 >= m_next_grid_id
	|| g4 >= m_next_grid_id)
	throw std::invalid_argument("invalid grid id");

    FastgenWriter::Record(m_writer) << "CQUAD" << m_next_element_id++ << 0 << g1 <<
				    g2 << g3 << g4 << thickness << (grid_centered ? 1 : 2);
}


void
Section::add_hexahedron(const std::size_t *g)
{
    for (int i = 0; i < 8; ++i) {
	if (g[i] >= m_next_grid_id)
	    throw std::invalid_argument("invalid grid id");

	for (int j = i + 1; j < 8; ++j)
	    if (g[i] == g[j])
		throw std::invalid_argument("repeated grid id");
    }

    {
	FastgenWriter::Record record1(m_writer);
	record1 << "CHEX2" << m_next_element_id << 0;

	for (int i = 0; i < 6; ++i)
	    record1 << g[i];

	record1 << m_next_element_id;
    }

    FastgenWriter::Record(m_writer) << m_next_element_id << g[6] << g[7];
    ++m_next_element_id;
}


HIDDEN void
write_bot(FastgenWriter &writer, const rt_bot_internal &bot)
{
    Section section(writer, "bot", 0, bot.mode == RT_BOT_SOLID);

    for (std::size_t i = 0; i < bot.num_vertices; ++i) {
	const fastf_t * const vertex = &bot.vertices[i * 3];
	section.add_grid_point(vertex[0], vertex[1], vertex[2]);
    }

    for (std::size_t i = 0; i < bot.num_faces; ++i) {
	double thickness = 1.0;
	bool grid_centered = false;

	if (bot.mode == RT_BOT_PLATE) {
	    if (bot.thickness) thickness = bot.thickness[i];

	    if (bot.face_mode) grid_centered = !BU_BITTEST(bot.face_mode, i);
	}

	const int * const face = &bot.faces[i * 3];
	section.add_triangle(face[0] + 1, face[1] + 1, face[2] + 1, thickness,
			     grid_centered);
    }
}


HIDDEN bool
ell_is_sphere(const rt_ell_internal &ell)
{
    // based on rt_sph_prep()

    fastf_t magsq_a, magsq_b, magsq_c;
    vect_t Au, Bu, Cu; // A, B, C with unit length
    fastf_t f;

    // Validate that |A| > 0, |B| > 0, |C| > 0
    magsq_a = MAGSQ(ell.a);
    magsq_b = MAGSQ(ell.b);
    magsq_c = MAGSQ(ell.c);

    if (ZERO(magsq_a) || ZERO(magsq_b) || ZERO(magsq_c))
	return false;

    // Validate that |A|, |B|, and |C| are nearly equal
    if (!EQUAL(magsq_a, magsq_b) || !EQUAL(magsq_a, magsq_c))
	return false;

    // Create unit length versions of A, B, C
    f = 1.0 / sqrt(magsq_a);
    VSCALE(Au, ell.a, f);
    f = 1.0 / sqrt(magsq_b);
    VSCALE(Bu, ell.b, f);
    f = 1.0 / sqrt(magsq_c);
    VSCALE(Cu, ell.c, f);

    // Validate that A.B == 0, B.C == 0, A.C == 0 (check dir only)
    f = VDOT(Au, Bu);

    if (!ZERO(f))
	return false;

    f = VDOT(Bu, Cu);

    if (!ZERO(f))
	return false;

    f = VDOT(Au, Cu);

    if (!ZERO(f))
	return false;

    return true;
}


HIDDEN void
write_nmg_region(nmgregion *nmg_region, const db_full_path *path, int region_id,
		 int material_id, float color[3], void *client_data)
{
    (void)nmg_region;
    (void)path;
    (void)region_id;
    (void)material_id;
    (void)color;
    (void)client_data;
}


HIDDEN tree *
convert_primitive(db_tree_state *tree_state, const db_full_path *path,
		  rt_db_internal *internal, void *client_data)
{
    RT_CK_DBTS(tree_state);

    if (internal->idb_major_type != DB5_MAJORTYPE_BRLCAD)
	return NULL;

    gcv_region_end_data &region_end_data = *static_cast<gcv_region_end_data *>
					   (client_data);
    FastgenWriter &writer = *static_cast<FastgenWriter *>
			    (region_end_data.client_data);

    char *name = db_path_to_string(path);

    switch (internal->idb_type) {
	case ID_ELL:
	case ID_SPH: {
	    const rt_ell_internal &ell = *static_cast<rt_ell_internal *>(internal->idb_ptr);
	    RT_ELL_CK_MAGIC(&ell);

	    if (internal->idb_type != ID_SPH && !ell_is_sphere(ell))
		goto tesselate;

	    Section section(writer, name, 0, true);
	    std::size_t center = section.add_grid_point(ell.v[0], ell.v[1], ell.v[2]);
	    section.add_sphere(center, 1.0, ell.a[0]);
	    break;
	}

	case ID_ARB8: {
	    const rt_arb_internal &arb = *static_cast<rt_arb_internal *>(internal->idb_ptr);
	    RT_ARB_CK_MAGIC(&arb);
	    Section section(writer, name, 0, true);

	    for (int i = 0; i < 8; ++i)
		section.add_grid_point(arb.pt[i][0], arb.pt[i][1], arb.pt[i][2]);

	    const std::size_t points[] = {1, 2, 3, 4, 5, 6, 7, 8};
	    section.add_hexahedron(points);
	    break;
	}

	case ID_BOT: {
	    const rt_bot_internal &bot = *static_cast<rt_bot_internal *>(internal->idb_ptr);
	    RT_BOT_CK_MAGIC(&bot);
	    write_bot(writer, bot);
	    break;
	}

	default: // handle any primitives that can't be directly expressed in fg4
	tesselate:
	    break;
    }

    bu_free(name, "name");
    return NULL;
}


}


extern "C" {


    HIDDEN int
    gcv_fastgen4_write(const char *path, struct db_i *dbip,
		       const struct gcv_opts *UNUSED(options))
    {
	FastgenWriter writer(path);
	writer.write_comment("g -> fastgen4 conversion");

	{
	    directory **results;
	    db_update_nref(dbip, &rt_uniresource);
	    std::size_t num_objects = db_ls(dbip, DB_LS_TOPS, NULL, &results);
	    char **object_names = db_dpv_to_argv(results);
	    bu_free(results, "tops");

	    const bn_tol tol = {BN_TOL_MAGIC, 5e-4, 5e-4 * 5e-4, 1e-6, 1 - 1e-6};
	    const rt_tess_tol ttol = {RT_TESS_TOL_MAGIC, 0.0, 0.01, 0.0};
	    model *vmodel = nmg_mm();
	    db_tree_state initial_tree_state = rt_initial_tree_state;
	    initial_tree_state.ts_tol = &tol;
	    initial_tree_state.ts_ttol = &ttol;
	    initial_tree_state.ts_m = &vmodel;

	    gcv_region_end_data region_end_data = {write_nmg_region, &writer};
	    db_walk_tree(dbip, num_objects, const_cast<const char **>(object_names), 1,
			 &initial_tree_state, NULL, gcv_region_end, convert_primitive, &region_end_data);

	    nmg_km(vmodel);
	    bu_free(object_names, "object_names");
	}


	return 1;
    }


    static const struct gcv_converter converters[] = {
	{"fg4", NULL, gcv_fastgen4_write},
	{NULL, NULL, NULL}
    };


    struct gcv_plugin_info gcv_plugin_conv_fastgen4_write = {converters};


}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
