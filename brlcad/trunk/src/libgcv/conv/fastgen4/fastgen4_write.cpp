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
#include <set>
#include <sstream>
#include <stdexcept>
#include <map>

#include "../../plugin.h"


namespace
{


template <typename T> HIDDEN inline void
autofreeptr_wrap_bu_free(T *ptr)
{
    bu_free(ptr, "AutoFreePtr");
}


template <typename T, void free_fn(T *) = autofreeptr_wrap_bu_free>
struct AutoFreePtr {
    AutoFreePtr(T *vptr) : ptr(vptr) {}

    ~AutoFreePtr()
    {
	if (ptr) free_fn(ptr);
    }


    T * const ptr;


private:
    AutoFreePtr(const AutoFreePtr &source);
    AutoFreePtr &operator=(const AutoFreePtr &source);
};


class FastgenWriter
{
public:
    class Record;

    static const fastf_t INCHES_PER_MM;

    FastgenWriter(const std::string &path);
    ~FastgenWriter();

    void write_comment(const std::string &value);
    void write_section_color(std::size_t section_id, const unsigned char *color);

    std::pair<std::size_t, std::size_t> take_next_section_id();


private:
    static const std::size_t MAX_GROUP_ID = 49;
    static const std::size_t MAX_SECTION_ID = 999;

    std::size_t m_next_section_id[MAX_GROUP_ID + 1];
    bool m_record_open;
    std::ofstream m_ostream, m_colors_ostream;
};


class FastgenWriter::Record
{
public:
    Record(FastgenWriter &writer);
    ~Record();

    template <typename T> Record &operator<<(const T &value);
    Record &operator<<(fastf_t value);
    Record &non_zero(fastf_t value);
    Record &text(const std::string &value);

    static std::string truncate_float(fastf_t value);


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
	throw std::logic_error("logic error: record open");

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
	throw std::logic_error("logic error: invalid record width");

    std::ostringstream sstream;

    if (!(sstream << value))
	throw std::invalid_argument("failed to convert value");

    const std::string str_val = sstream.str();

    if (str_val.size() > FIELD_WIDTH)
	throw std::invalid_argument("length exceeds field width");

    m_writer.m_ostream << std::left << std::setw(FIELD_WIDTH) << str_val;
    return *this;
}


inline
FastgenWriter::Record &
FastgenWriter::Record::operator<<(fastf_t value)
{
    return operator<<(truncate_float(value));
}


// ensure that the truncated value != 0.0
FastgenWriter::Record &
FastgenWriter::Record::non_zero(fastf_t value)
{
    std::string result = truncate_float(value);

    if (result.find_first_not_of("-0.") == std::string::npos) {
	result.resize(FIELD_WIDTH, '0');
	result.at(result.size() - 1) = '1';
    }

    return operator<<(result);
}


FastgenWriter::Record &
FastgenWriter::Record::text(const std::string &value)
{
    m_width = RECORD_WIDTH;
    m_writer.m_ostream << value;
    return *this;
}


std::string
FastgenWriter::Record::truncate_float(fastf_t value)
{
    std::ostringstream sstream;
    sstream << std::fixed << std::showpoint << value;
    return sstream.str().substr(0, FIELD_WIDTH);
}


const fastf_t FastgenWriter::INCHES_PER_MM = 1.0 / 25.4;


FastgenWriter::FastgenWriter(const std::string &path) :
    m_record_open(false),
    m_ostream(path.c_str(), std::ofstream::out),
    m_colors_ostream((path + ".colors").c_str(), std::ofstream::out)
{
    m_ostream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    m_colors_ostream.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    for (std::size_t i = 0; i <= MAX_GROUP_ID; ++i)
	m_next_section_id[i] = 1;
}


inline
FastgenWriter::~FastgenWriter()
{
    Record(*this) << "ENDDATA";
}


std::pair<std::size_t, std::size_t>
FastgenWriter::take_next_section_id()
{
    std::size_t group_id = 0;

    while (m_next_section_id[group_id] > MAX_SECTION_ID)
	if (++group_id > MAX_GROUP_ID)
	    throw std::length_error("maximum number of sections");

    return std::make_pair(group_id, m_next_section_id[group_id]++);
}


inline void
FastgenWriter::write_comment(const std::string &value)
{
    (Record(*this) << "$COMMENT").text(" ").text(value);
}


void
FastgenWriter::write_section_color(std::size_t section_id,
				   const unsigned char *color)
{
    m_colors_ostream << section_id << ' '
		     << section_id << ' '
		     << static_cast<unsigned>(color[0]) << ' '
		     << static_cast<unsigned>(color[1]) << ' '
		     << static_cast<unsigned>(color[2]) << '\n';
}


class Point
{
public:
    Point()
    {
	VSETALL(*this, 0.0);
    }


    Point(const fastf_t *values)
    {
	VMOVE(*this, values);
    }


    operator const fastf_t *() const
    {
	return m_point;
    }


    operator fastf_t *()
    {
	return m_point;
    }


private:
    point_t m_point;
};


class GridManager
{
public:
    GridManager();

    std::size_t get_max_id() const;
    std::size_t get_grid(const Point &point);

    // return a vector of grid IDs corresponding to the given points,
    // with no duplicate indices in the returned vector.
    std::vector<std::size_t> get_unique_grids(const std::vector<Point> &points);

    void write(FastgenWriter &writer) const;


private:
    struct PointComparator {
	bool operator()(const Point &lhs, const Point &rhs) const;
    };

    typedef std::pair<std::map<Point, std::vector<std::size_t>, PointComparator>::iterator, bool>
    FindResult;

    static const std::size_t MAX_GRID_POINTS = 50000;

    FindResult find_grid(const Point &point);

    std::size_t m_next_grid_id;
    std::map<Point, std::vector<std::size_t>, PointComparator> m_grids;
};


bool GridManager::PointComparator::operator()(const Point &lhs,
	const Point &rhs) const
{
#define COMPARE(a, b) do { if (!EQUAL((a), (b))) return (a) < (b); } while (false)

    COMPARE(lhs[X], rhs[X]);
    COMPARE(lhs[Y], rhs[Y]);
    COMPARE(lhs[Z], rhs[Z]);
    return false;

#undef COMPARE
}


inline
GridManager::GridManager() :
    m_next_grid_id(1),
    m_grids()
{}


GridManager::FindResult
GridManager::find_grid(const Point &point)
{
    std::vector<std::size_t> temp(1);
    temp.at(0) = m_next_grid_id;
    return m_grids.insert(std::make_pair(point, temp));
}


inline std::size_t
GridManager::get_max_id() const
{
    return m_next_grid_id - 1;
}


std::size_t
GridManager::get_grid(const Point &point)
{
    FindResult found = find_grid(point);

    if (found.second)
	++m_next_grid_id;

    return found.first->second.at(0);
}


std::vector<std::size_t>
GridManager::get_unique_grids(const std::vector<Point> &points)
{
    std::vector<std::size_t> results(points.size());

    for (std::size_t i = 0; i < points.size(); ++i) {
	FindResult found = find_grid(points[i]);

	if (found.second) {
	    ++m_next_grid_id;
	    results[i] = found.first->second.at(0);
	    continue;
	}

	std::size_t n = 0;
	results[i] = found.first->second.at(n);

	for (std::size_t j = 0; j < i; ++j)
	    if (results[j] == results[i]) {
		if (++n < found.first->second.size()) {
		    results[i] = found.first->second.at(n);
		    j = 0;
		} else {
		    found.first->second.push_back(m_next_grid_id);
		    results[i] = m_next_grid_id++;
		    break;
		}
	    }

	if (results[i] > MAX_GRID_POINTS)
	    throw std::length_error("invalid grid ID");
    }

    return results;
}


void
GridManager::write(FastgenWriter &writer) const
{
    for (std::map<Point, std::vector<std::size_t>, PointComparator>::const_iterator
	 it = m_grids.begin(); it != m_grids.end(); ++it)
	for (std::vector<std::size_t>::const_iterator id_it = it->second.begin();
	     id_it != it->second.end(); ++id_it) {
	    FastgenWriter::Record(writer) << "GRID" << *id_it << "" << it->first[X] *
					  FastgenWriter::INCHES_PER_MM << it->first[Y] * FastgenWriter::INCHES_PER_MM <<
					  it->first[Z] * FastgenWriter::INCHES_PER_MM;
	}
}


class Section
{
public:
    class Geometry;
    class Line;
    class Sphere;
    class Cone;
    class Triangle;
    class Hexahedron;

    Section(const std::string &name, bool volume_mode);
    ~Section();

    void add(Geometry *geometry);
    void write(FastgenWriter &writer, const unsigned char *color = NULL) const;


private:
    static const std::size_t MAX_NAME_SIZE = 25;

    const std::string m_name;
    const bool m_volume_mode;

    GridManager m_grid_manager;
    std::vector<Geometry *> m_geometry;
};


class Section::Geometry
{
public:
    Geometry(Section &section, const std::string &name);
    virtual ~Geometry();

    void write(FastgenWriter &writer, std::size_t &id) const;


protected:
    virtual void write_to_section(FastgenWriter &writer, std::size_t &id) const = 0;

    Section &m_section;


private:
    const std::string m_name;
};


inline
Section::Geometry::Geometry(Section &section, const std::string &name) :
    m_section(section),
    m_name(name)
{}


inline
Section::Geometry::~Geometry()
{}


void
Section::Geometry::write(FastgenWriter &writer, std::size_t &id) const
{
    if (!m_name.empty())
	writer.write_comment(m_name);

    write_to_section(writer, id);
}


inline
Section::Section(const std::string &name, bool volume_mode) :
    m_name(name),
    m_volume_mode(volume_mode)
{}


Section::~Section()
{
    for (std::vector<Geometry *>::iterator it = m_geometry.begin();
	 it != m_geometry.end(); ++it)
	delete *it;
}


inline void
Section::add(Geometry *geometry)
{
    m_geometry.push_back(geometry);
}


void
Section::write(FastgenWriter &writer, const unsigned char *color) const
{
    const std::pair<std::size_t, std::size_t> id = writer.take_next_section_id();

    {
	std::string record_name;

	if (m_name.size() > MAX_NAME_SIZE) {
	    writer.write_comment(m_name);
	    record_name = "..." + m_name.substr(m_name.size() - MAX_NAME_SIZE + 3);
	} else {
	    record_name = m_name;
	}

	FastgenWriter::Record record(writer);
	record << "$NAME" << id.first << id.second;
	record << "" << "" << "" << "";
	record.text(record_name);
    }

    FastgenWriter::Record(writer) << "SECTION" << id.first << id.second <<
				  (m_volume_mode ? 2 : 1);

    if (color)
	writer.write_section_color(id.second, color);

    m_grid_manager.write(writer);
    std::size_t component_id = m_grid_manager.get_max_id() + 1;

    for (std::vector<Geometry *>::const_iterator it = m_geometry.begin();
	 it != m_geometry.end(); ++it) {
	(*it)->write(writer, component_id);
	++component_id;
    }
}


class Section::Line : public Section::Geometry
{
public:
    Line(Section &section, const std::string &name, const Point &point_a,
	 const Point &point_b, fastf_t thickness, fastf_t radius);

protected:
    virtual void write_to_section(FastgenWriter &writer, std::size_t &id) const;

private:
    std::size_t m_grid1, m_grid2;
    const fastf_t m_thickness, m_radius;
};


Section::Line::Line(Section &section, const std::string &name,
		    const Point &point_a, const Point &point_b, fastf_t thickness, fastf_t radius) :
    Geometry(section, name),
    m_grid1(0),
    m_grid2(0),
    m_thickness(thickness),
    m_radius(radius)
{
    if ((!m_section.m_volume_mode && m_thickness <= 0.0) || m_thickness < 0.0)
	throw std::invalid_argument("invalid thickness");

    if (m_radius <= 0.0)
	throw std::invalid_argument("invalid radius");

    std::vector<Point> points(2);
    points.at(0) = point_a;
    points.at(1) = point_b;
    const std::vector<std::size_t> grids =
	m_section.m_grid_manager.get_unique_grids(
	    points);
    m_grid1 = grids.at(0);
    m_grid2 = grids.at(1);
}


void
Section::Line::write_to_section(FastgenWriter &writer, std::size_t &id) const
{
    FastgenWriter::Record record(writer);
    record << "CLINE" << id << 0 << m_grid1 << m_grid2 << "" << "";

    if (m_section.m_volume_mode)
	record << m_thickness * FastgenWriter::INCHES_PER_MM;
    else
	record.non_zero(m_thickness * FastgenWriter::INCHES_PER_MM);

    record << m_radius * FastgenWriter::INCHES_PER_MM;
}


class Section::Sphere : public Section::Geometry
{
public:
    Sphere(Section &section, const std::string &name, const Point &center,
	   fastf_t thickness, fastf_t radius);

protected:
    virtual void write_to_section(FastgenWriter &writer, std::size_t &id) const;

private:
    const std::size_t m_grid1;
    const fastf_t m_thickness, m_radius;
};


Section::Sphere::Sphere(Section &section, const std::string &name,
			const Point &center, fastf_t thickness, fastf_t radius) :
    Geometry(section, name),
    m_grid1(m_section.m_grid_manager.get_grid(center)),
    m_thickness(thickness),
    m_radius(radius)
{
    if (m_thickness <= 0.0 || m_radius <= 0.0)
	throw std::invalid_argument("invalid value");
}


void
Section::Sphere::write_to_section(FastgenWriter &writer, std::size_t &id) const
{
    FastgenWriter::Record record(writer);
    record << "CSPHERE" << id << 0 << m_grid1 << "" << "" << "";
    record.non_zero(m_thickness * FastgenWriter::INCHES_PER_MM).non_zero(
	m_radius * FastgenWriter::INCHES_PER_MM);
}


class Section::Cone : public Section::Geometry
{
public:
    Cone(Section &section, const std::string &name, const Point &point_a,
	 const Point &point_b, fastf_t radius_outer1, fastf_t radius_outer2,
	 fastf_t radius_inner1, fastf_t radius_inner2);

protected:
    virtual void write_to_section(FastgenWriter &writer, std::size_t &id) const;

private:
    std::size_t m_grid1, m_grid2;
    const fastf_t m_ro1, m_ro2, m_ri1, m_ri2;
};


Section::Cone::Cone(Section &section, const std::string &name,
		    const Point &point_a, const Point &point_b, fastf_t radius_outer1,
		    fastf_t radius_outer2, fastf_t radius_inner1, fastf_t radius_inner2) :
    Geometry(section, name),
    m_grid1(0),
    m_grid2(0),
    m_ro1(radius_outer1),
    m_ro2(radius_outer2),
    m_ri1(radius_inner1),
    m_ri2(radius_inner2)
{
    if (m_ri1 < 0.0 || m_ri2 < 0.0 || m_ri1 >= m_ro1 || m_ri2 >= m_ro2)
	throw std::invalid_argument("invalid radius");

    std::vector<Point> points(2);
    points.at(0) = point_a;
    points.at(1) = point_b;
    const std::vector<std::size_t> grids = section.m_grid_manager.get_unique_grids(
	    points);
    m_grid1 = grids.at(0);
    m_grid2 = grids.at(1);
}


void
Section::Cone::write_to_section(FastgenWriter &writer, std::size_t &id) const
{
    FastgenWriter::Record(writer) << "CCONE2" << id << 0 << m_grid1 << m_grid2 << ""
				  << "" << "" << m_ro1 * FastgenWriter::INCHES_PER_MM << ++id;
    FastgenWriter::Record(writer) << id << m_ro2 * FastgenWriter::INCHES_PER_MM <<
				  m_ri1 * FastgenWriter::INCHES_PER_MM << m_ri2 * FastgenWriter::INCHES_PER_MM;
}


class Section::Triangle : public Section::Geometry
{
public:
    Triangle(Section &section, const std::string &name, const Point &point_a,
	     const Point &point_b, const Point &point_c, fastf_t thickness,
	     bool grid_centered);

protected:
    virtual void write_to_section(FastgenWriter &writer, std::size_t &id) const;

private:
    std::size_t m_grid1, m_grid2, m_grid3;
    const fastf_t m_thickness;
    const bool m_grid_centered;
};


Section::Triangle::Triangle(Section &section, const std::string &name,
			    const Point &point_a, const Point &point_b, const Point &point_c,
			    fastf_t thickness, bool grid_centered) :
    Geometry(section, name),
    m_grid1(0),
    m_grid2(0),
    m_grid3(0),
    m_thickness(thickness),
    m_grid_centered(grid_centered)
{
    if (thickness <= 0.0)
	throw std::invalid_argument("invalid thickness");

    std::vector<Point> points(3);
    points.at(0) = point_a;
    points.at(1) = point_b;
    points.at(2) = point_c;
    const std::vector<std::size_t> grids = section.m_grid_manager.get_unique_grids(
	    points);
    m_grid1 = grids.at(0);
    m_grid2 = grids.at(1);
    m_grid3 = grids.at(2);
}


void
Section::Triangle::write_to_section(FastgenWriter &writer,
				    std::size_t &id) const
{
    FastgenWriter::Record record(writer);
    record << "CTRI" << id << 0 << m_grid1 << m_grid2 << m_grid3;
    record.non_zero(m_thickness * FastgenWriter::INCHES_PER_MM);
    record << (m_grid_centered ? 1 : 2);
}


class Section::Hexahedron : public Section::Geometry
{
public:
    Hexahedron(Section &section, const std::string &name,
	       const fastf_t points[8][3]);

protected:
    virtual void write_to_section(FastgenWriter &writer, std::size_t &id) const;

private:
    std::vector<std::size_t> m_grids;
};


Section::Hexahedron::Hexahedron(Section &section, const std::string &name,
				const fastf_t points[8][3]) :
    Geometry(section, name),
    m_grids()
{
    std::vector<Point> vpoints(8);

    for (int i = 0; i < 8; ++i)
	vpoints.at(i) = points[i];

    m_grids = section.m_grid_manager.get_unique_grids(vpoints);
}


void
Section::Hexahedron::write_to_section(FastgenWriter &writer,
				      std::size_t &id) const
{
    {
	FastgenWriter::Record record1(writer);
	record1 << "CHEX2" << id << 0;

	for (int i = 0; i < 6; ++i)
	    record1 << m_grids.at(i);

	record1 << ++id;
    }

    FastgenWriter::Record record2(writer);
    record2 << id << m_grids.at(6) << m_grids.at(7);
}


HIDDEN void
write_bot(Section &section, const rt_bot_internal &bot)
{
    RT_BOT_CK_MAGIC(&bot);

    for (std::size_t i = 0; i < bot.num_faces; ++i) {
	fastf_t thickness = 1.0;
	bool grid_centered = false;

	if (bot.mode == RT_BOT_PLATE) {
	    // fg4 does not allow zero thickness
	    // set a very small thickness if face thickness is zero
	    if (bot.thickness)
		thickness = !ZERO(bot.thickness[i]) ? bot.thickness[i] : 2 * SMALL_FASTF;

	    if (bot.face_mode)
		grid_centered = !BU_BITTEST(bot.face_mode, i);
	}

	const int * const face = &bot.faces[i * 3];
	const fastf_t * const v1 = &bot.vertices[face[0] * 3];
	const fastf_t * const v2 = &bot.vertices[face[1] * 3];
	const fastf_t * const v3 = &bot.vertices[face[2] * 3];
	section.add(new Section::Triangle(section, "", v1, v2, v3, thickness,
					  grid_centered));
    }
}


HIDDEN bool
ell_is_sphere(const rt_ell_internal &ell)
{
    RT_ELL_CK_MAGIC(&ell);

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


// Determines whether a tgc can be represented by a CCONE2 object.
// Assumes that tgc is a valid rt_tgc_internal.
HIDDEN bool
tgc_is_ccone(const rt_tgc_internal &tgc)
{
    RT_TGC_CK_MAGIC(&tgc);

    if (VZERO(tgc.a) || VZERO(tgc.b))
	return false;

    if (!ZERO(VDOT(tgc.h, tgc.a)) || !ZERO(VDOT(tgc.h, tgc.b)))
	return false;

    {
	vect_t a_norm, b_norm, c_norm, d_norm;
	VMOVE(a_norm, tgc.a);
	VMOVE(b_norm, tgc.b);
	VMOVE(c_norm, tgc.c);
	VMOVE(d_norm, tgc.d);
	VUNITIZE(a_norm);
	VUNITIZE(b_norm);
	VUNITIZE(c_norm);
	VUNITIZE(d_norm);

	if (!VEQUAL(a_norm, c_norm) || !VEQUAL(b_norm, d_norm))
	    return false;
    }

    return true;
}


// Determines whether the object with the given name is a member of a
// CCONE-compatible region (typically created by the fastgen4 importer).
//
// Sets `new_name` to the name of the CCONE.
// Sets `ro1`, `ro2`, `ri1`, and `ri2` to the values of
// the lower/upper outer and inner radii.
HIDDEN bool
find_ccone_cutout(const std::string &name, db_i &db, std::string &new_name,
		  fastf_t &ro1, fastf_t &ro2, fastf_t &ri1, fastf_t &ri2)
{
    db_full_path path;

    if (db_string_to_path(&path, &db, name.c_str()))
	throw std::logic_error("logic error: invalid path");

    AutoFreePtr<db_full_path, db_free_full_path> autofree_path(&path);

    if (path.fp_len < 2)
	return false;

    rt_db_internal comb_db_internal;

    if (rt_db_get_internal(&comb_db_internal, path.fp_names[path.fp_len - 2], &db,
			   NULL, &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    AutoFreePtr<rt_db_internal, rt_db_free_internal> autofree_comb_db_internal(
	&comb_db_internal);

    rt_comb_internal &comb_internal = *static_cast<rt_comb_internal *>
				      (comb_db_internal.idb_ptr);
    RT_CK_COMB(&comb_internal);

    const directory *outer_directory, *inner_directory;
    {
	const tree::tree_node &t = comb_internal.tree->tr_b;

	if (t.tb_op != OP_SUBTRACT || !t.tb_left || !t.tb_right
	    || t.tb_left->tr_op != OP_DB_LEAF || t.tb_right->tr_op != OP_DB_LEAF)
	    return false;

	outer_directory = db_lookup(&db, t.tb_left->tr_l.tl_name, false);
	inner_directory = db_lookup(&db, t.tb_right->tr_l.tl_name, false);

	// check for nonexistent members
	if (!outer_directory || !inner_directory)
	    return false;
    }

    rt_db_internal outer_db_internal, inner_db_internal;

    if (rt_db_get_internal(&outer_db_internal, outer_directory, &db, NULL,
			   &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    AutoFreePtr<rt_db_internal, rt_db_free_internal> autofree_outer_db_internal(
	&outer_db_internal);

    if (rt_db_get_internal(&inner_db_internal, inner_directory, &db, NULL,
			   &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    AutoFreePtr<rt_db_internal, rt_db_free_internal> autofree_inner_db_internal(
	&inner_db_internal);

    if ((outer_db_internal.idb_minor_type != ID_TGC
	 && outer_db_internal.idb_minor_type != ID_REC)
	|| (inner_db_internal.idb_minor_type != ID_TGC
	    && inner_db_internal.idb_minor_type != ID_REC))
	return false;

    const rt_tgc_internal &outer_tgc = *static_cast<rt_tgc_internal *>
				       (outer_db_internal.idb_ptr);
    const rt_tgc_internal &inner_tgc = *static_cast<rt_tgc_internal *>
				       (inner_db_internal.idb_ptr);
    RT_TGC_CK_MAGIC(&outer_tgc);
    RT_TGC_CK_MAGIC(&inner_tgc);

    // check cone geometry
    if (!tgc_is_ccone(outer_tgc) || !tgc_is_ccone(inner_tgc))
	return false;

    if (!VEQUAL(outer_tgc.v, inner_tgc.v) || !VEQUAL(outer_tgc.h, inner_tgc.h))
	return false;

    // store results
    --path.fp_len;
    new_name = AutoFreePtr<char>(db_path_to_string(&path)).ptr;
    ro1 = MAGNITUDE(outer_tgc.a);
    ro2 = MAGNITUDE(outer_tgc.b);
    ri1 = MAGNITUDE(inner_tgc.a);
    ri2 = MAGNITUDE(inner_tgc.b);

    // check radii
    if (ri1 >= ro1 || ri2 >= ro2)
	return false;

    return true;
}


struct ConversionData {
    FastgenWriter &m_writer;
    const bn_tol &m_tol;
    const bool m_convert_primitives;

    // for find_ccone_cutout()
    db_i &m_db;
    std::set<std::string> m_recorded_ccones;
};


HIDDEN bool
convert_primitive(ConversionData &data, const rt_db_internal &internal,
		  const std::string &name)
{
    switch (internal.idb_type) {
	case ID_CLINE: {
	    const rt_cline_internal &cline = *static_cast<rt_cline_internal *>
					     (internal.idb_ptr);
	    RT_CLINE_CK_MAGIC(&cline);

	    point_t v2;
	    VADD2(v2, cline.v, cline.h);

	    Section section(name, true);
	    section.add(new Section::Line(section, name, cline.v, v2, cline.thickness,
					  cline.radius));
	    section.write(data.m_writer);
	    break;
	}

	case ID_ELL:
	case ID_SPH: {
	    const rt_ell_internal &ell = *static_cast<rt_ell_internal *>(internal.idb_ptr);
	    RT_ELL_CK_MAGIC(&ell);

	    if (internal.idb_type != ID_SPH && !ell_is_sphere(ell))
		return false;

	    Section section(name, true);
	    section.add(new Section::Sphere(section, name, ell.v, 1.0, MAGNITUDE(ell.a)));
	    section.write(data.m_writer);
	    break;
	}

	case ID_TGC:
	case ID_REC: {
	    const rt_tgc_internal &tgc = *static_cast<rt_tgc_internal *>(internal.idb_ptr);
	    RT_TGC_CK_MAGIC(&tgc);

	    if (internal.idb_type != ID_REC && !tgc_is_ccone(tgc))
		return false;

	    std::string new_name;
	    fastf_t ro1, ro2, ri1, ri2;

	    if (find_ccone_cutout(name, data.m_db, new_name, ro1, ro2, ri1, ri2)) {
		// an imported CCONE with cutout
		if (!data.m_recorded_ccones.insert(new_name).second)
		    break; // already written
	    } else {
		new_name = name;
		ro1 = MAGNITUDE(tgc.a);
		ro2 = MAGNITUDE(tgc.b);
		ri1 = ri2 = 0.0;
	    }

	    point_t v2;
	    VADD2(v2, tgc.v, tgc.h);

	    Section section(new_name, true);
	    section.add(new Section::Cone(section, name, tgc.v, v2, ro1, ro2, ri1, ri2));
	    section.write(data.m_writer);
	    break;
	}

	case ID_ARB8: {
	    const rt_arb_internal &arb = *static_cast<rt_arb_internal *>(internal.idb_ptr);
	    RT_ARB_CK_MAGIC(&arb);

	    Section section(name, true);
	    section.add(new Section::Hexahedron(section, name, arb.pt));
	    section.write(data.m_writer);
	    break;
	}

	case ID_BOT: {
	    const rt_bot_internal &bot = *static_cast<rt_bot_internal *>(internal.idb_ptr);
	    RT_BOT_CK_MAGIC(&bot);

	    Section section(name, bot.mode == RT_BOT_SOLID);
	    write_bot(section, bot);
	    section.write(data.m_writer);
	    break;
	}

	default:
	    return false;
    }

    return true;
}


HIDDEN void
write_nmg_region(nmgregion *nmg_region, const db_full_path *path,
		 int UNUSED(region_id), int UNUSED(material_id), float *color, void *client_data)
{
    NMG_CK_REGION(nmg_region);
    NMG_CK_MODEL(nmg_region->m_p);
    RT_CK_FULL_PATH(path);

    ConversionData &data = *static_cast<ConversionData *>(client_data);
    const std::string name = AutoFreePtr<char>(db_path_to_string(path)).ptr;

    shell *vshell;

    for (BU_LIST_FOR(vshell, shell, &nmg_region->s_hd)) {
	NMG_CK_SHELL(vshell);

	rt_bot_internal *bot = nmg_bot(vshell, &data.m_tol);

	// fill in an rt_db_internal with our new bot so we can free it
	rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_minor_type = ID_BOT;
	internal.idb_meth = &OBJ[ID_BOT];
	internal.idb_ptr = bot;

	try {
	    unsigned char char_color[3];
	    char_color[0] = static_cast<unsigned char>(color[0] * 255.0 + 0.5);
	    char_color[1] = static_cast<unsigned char>(color[1] * 255.0 + 0.5);
	    char_color[2] = static_cast<unsigned char>(color[2] * 255.0 + 0.5);
	    Section section(name, bot->mode == RT_BOT_SOLID);
	    write_bot(section, *bot);
	    section.write(data.m_writer, char_color);
	} catch (const std::runtime_error &e) {
	    bu_log("FAILURE: write_bot() failed on object '%s': %s\n", name.c_str(),
		   e.what());
	}

	internal.idb_meth->ft_ifree(&internal);
    }
}


HIDDEN tree *
convert_leaf(db_tree_state *tree_state, const db_full_path *path,
	     rt_db_internal *internal, void *client_data)
{
    RT_CK_DBTS(tree_state);
    RT_CK_FULL_PATH(path);
    RT_CK_DB_INTERNAL(internal);

    if (internal->idb_major_type != DB5_MAJORTYPE_BRLCAD)
	return NULL;

    ConversionData &data = *static_cast<ConversionData *>(client_data);
    const std::string name = AutoFreePtr<char>(db_path_to_string(path)).ptr;
    bool converted = false;

    try {
	if (data.m_convert_primitives)
	    converted = convert_primitive(data, *internal, name);
    } catch (const std::runtime_error &e) {
	bu_log("FAILURE: convert_primitive() failed on object '%s': %s\n", name.c_str(),
	       e.what());
    }

    if (!converted)
	return nmg_booltree_leaf_tess(tree_state, path, internal, client_data);

    tree *result;
    RT_GET_TREE(result, tree_state->ts_resp);
    result->tr_op = OP_NOP;
    return result;
}


HIDDEN tree *
convert_region(db_tree_state *tree_state, const db_full_path *path,
	       tree *current_tree, void *client_data)
{
    RT_CK_DBTS(tree_state);
    RT_CK_FULL_PATH(path);
    RT_CK_TREE(current_tree);

    ConversionData &data = *static_cast<ConversionData *>(client_data);
    const std::string name = AutoFreePtr<char>(db_path_to_string(path)).ptr;

    gcv_region_end_data gcv_data = {write_nmg_region, &data};
    return gcv_region_end(tree_state, path, current_tree, &gcv_data);
}


HIDDEN int
gcv_fastgen4_write(const char *path, struct db_i *dbip,
		   const struct gcv_opts *UNUSED(options))
{
    // Set to true to directly translate any primitives that can be represented by fg4.
    // Due to limitations in the fg4 format, boolean operations can not be represented.
    const bool convert_primitives = false;

    FastgenWriter writer(path);
    writer.write_comment(dbip->dbi_title);
    writer.write_comment("g -> fastgen4 conversion");

    const rt_tess_tol ttol = {RT_TESS_TOL_MAGIC, 0.0, 0.01, 0.0};
    const bn_tol tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1 - 1e-6};
    ConversionData conv_data = {writer, tol, convert_primitives, *dbip, std::set<std::string>()};

    {
	model *vmodel;
	db_tree_state initial_tree_state = rt_initial_tree_state;
	initial_tree_state.ts_tol = &tol;
	initial_tree_state.ts_ttol = &ttol;
	initial_tree_state.ts_m = &vmodel;

	db_update_nref(dbip, &rt_uniresource);
	directory **results;
	std::size_t num_objects = db_ls(dbip, DB_LS_TOPS, NULL, &results);
	AutoFreePtr<char *> object_names(db_dpv_to_argv(results));
	bu_free(results, "tops");

	vmodel = nmg_mm();
	db_walk_tree(dbip, num_objects, const_cast<const char **>(object_names.ptr), 1,
		     &initial_tree_state, NULL, convert_region, convert_leaf, &conv_data);
	nmg_km(vmodel);
    }

    return 1;
}


static const struct gcv_converter converters[] = {
    {"fg4", NULL, gcv_fastgen4_write},
    {NULL, NULL, NULL}
};


}


extern "C" {


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
