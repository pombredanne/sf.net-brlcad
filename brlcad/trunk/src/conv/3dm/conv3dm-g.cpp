/*                         C O N V 3 D M - G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file conv3dm-g.cpp
 *
 * Program to convert a Rhino model (in a .3dm file) to a BRL-CAD .g
 * file.
 *
 */




#include "common.h"


/* disabled if OBJ_BREP is not available */
#ifdef OBJ_BREP

#include "conv3dm-g.hpp"


#include <sstream>
#include <stdexcept>

#include "bu/getopt.h"
#include "vmath.h"
#include "wdb.h"




namespace
{




static const bool verbose_mode = false;


static const std::string ROOT_UUID = "00000000-0000-0000-0000-000000000000";
static const std::string DEFAULT_LAYER_NAME = "Default";
static const std::string DEFAULT_SHADER = "plastic";

/* UUID buffers must be >= 37 chars per openNURBS API */
static const std::size_t UUID_LEN = 37;


typedef std::map<std::string, conv3dm::RhinoConverter::ModelObject> OBJ_MAP;


static struct _InitOpenNURBS {
    _InitOpenNURBS()
    {
	ON::Begin();
    }


    ~_InitOpenNURBS()
    {
	ON::End();
    }
} _init_opennurbs;




static inline std::string
UUIDstr(const ON_UUID &uuid)
{
    char buf[UUID_LEN];
    return ON_UuidToString(uuid, buf);
}




static inline std::string
w2string(const ON_wString &source)
{
    if (!source) return "";
    return ON_String(source).Array();
}




static inline bool
is_toplevel(const ON_Layer &layer)
{
    return UUIDstr(layer.m_parent_layer_id) == ROOT_UUID;
}




static void
xform2mat_t(const ON_Xform &source, mat_t dest)
{
    const int dmax = 4;
    for (int row = 0; row < dmax; ++row)
	for (int col = 0; col < dmax; ++col)
	    dest[row*dmax + col] = source[row][col];
}




std::string
get_shader_args(const ON_Material &material)
{
    std::ostringstream args;
    args << "{";
    args << " tr " << material.m_transparency;
    args << " re " << material.m_reflectivity;
    args << " sp " << material.m_specular;
    args << " di " << material.m_diffuse;
    args << " ri " << material.m_index_of_refraction;
    args << " sh " << material.m_shine;
    // args << " ex " << ??;
    args << " em " << material.m_emission;
    args << " }";

    return args.str();
}




// Removes all leading and trailing non alpha-numeric characters,
// then replaces all remaining non alpha-numeric characters with
// the '_' character. The allow string is an exception list where
// these characters are allowed, but not leading or trailing, in
// the name.
static std::string
CleanName(ON_wString name)
{
    ON_wString allow(".-_");
    ON_wString new_name;
    bool was_cleaned = false;

    bool found_first = false;
    int idx_first = 0, idx_last = 0;

    for (int j = 0; j < name.Length(); j++) {
	wchar_t c = name.GetAt(j);

	bool ok_char = false;
	if (isalnum(c)) {
	    if (!found_first) {
		idx_first = idx_last = j;
		found_first = true;
	    } else {
		idx_last = j;
	    }
	    ok_char = true;
	} else {
	    for (int k = 0 ; k < allow.Length() ; k++) {
		if (c == allow.GetAt(k)) {
		    ok_char = true;
		    break;
		}
	    }
	}
	if (!ok_char) {
	    c = L'_';
	    was_cleaned = true;
	}
	new_name += c;
    }
    if (idx_first != 0 || name.Length() != ((idx_last - idx_first) + 1)) {
	new_name = new_name.Mid(idx_first, (idx_last - idx_first) + 1);
	was_cleaned = true;
    }
    if (was_cleaned) {
	name.Destroy();
	name = new_name;
    }

    if (name)
	return w2string(name);
    else
	return "noname";
}




static bool
is_name_taken(const OBJ_MAP &obj_map, const std::string &name)
{
    for (OBJ_MAP::const_iterator it = obj_map.begin();
	 it != obj_map.end(); ++it)
	if (name == it->second.m_name)
	    return true;


    return false;
}




static std::string
unique_name(const OBJ_MAP &obj_map,
	    const std::string &name,
	    const std::string &suffix)
{
    std::string new_name = name + suffix;
    int counter = 0;
    while (is_name_taken(obj_map, new_name)) {
	std::ostringstream s;
	s << ++counter;
	new_name = name + s.str() + suffix;
    }

    return new_name;
}
}




namespace conv3dm
{



class RhinoConverter::Color
{
public:
    static Color random();


    Color();
    Color(int red, int green, int blue);
    Color(const ON_Color &src);

    bool operator==(const Color &rhs) const;

    const unsigned char *get_rgb() const;


private:
    void set_rgb(int red, int green, int blue);

    unsigned char m_rgb[3];
};




RhinoConverter::Color
RhinoConverter::Color::random()
{
    int red = static_cast<int>(256 * drand48() + 1);
    int green = static_cast<int>(256 * drand48() + 1);
    int blue = static_cast<int>(256 * drand48() + 1);

    return Color(red, green, blue);
}




RhinoConverter::Color::Color()
{
    set_rgb(0, 0, 0);
}




RhinoConverter::Color::Color(int red, int green, int blue)
{
    set_rgb(red, green, blue);
}



RhinoConverter::Color::Color(const ON_Color &src)
{
    set_rgb(src.Red(), src.Green(), src.Blue());
}




bool
RhinoConverter::Color::operator==(const Color &rhs) const
{
    return m_rgb[0] == rhs.m_rgb[0]
	   && m_rgb[1] == rhs.m_rgb[1]
	   && m_rgb[2] == rhs.m_rgb[2];
}




const unsigned char *
RhinoConverter::Color::get_rgb() const
{
    return m_rgb;
}




void
RhinoConverter::Color::set_rgb(int red, int green, int blue)
{
    if (red < 0 || red > 255
	|| green < 0 || green > 255
	|| blue < 0 || blue > 255)
	throw std::out_of_range("invalid color");

    m_rgb[0] = static_cast<unsigned char>(red);
    m_rgb[1] = static_cast<unsigned char>(green);
    m_rgb[2] = static_cast<unsigned char>(blue);
}




RhinoConverter::RhinoConverter(const std::string &output_path) :
    m_use_uuidnames(false),
    m_random_colors(false),
    m_obj_map(),
    m_log(new ON_TextLog),
    m_model(new ONX_Model),
    m_db(NULL)
{
    m_db = wdb_fopen(output_path.c_str());
    if (!m_db || mk_id(m_db, "3dm -> g conversion")) {
	wdb_close(m_db);
	throw std::runtime_error("failed to open database");
    }
}




RhinoConverter::~RhinoConverter()
{
    wdb_close(m_db);
}




void
RhinoConverter::write_model(const std::string &path, bool use_uuidnames,
			    bool random_colors)
{
    m_use_uuidnames = use_uuidnames;
    m_random_colors = random_colors;

    if (!m_model->Read(path.c_str(), m_log.get()))
	throw std::runtime_error("failed to read 3dm file");

    m_log->Print("Loaded 3dm model '%s'\n", path.c_str());
    clean_model();
    m_log->Print("Number of NURBS objects read: %d\n\n", m_model->m_object_table.Count());

    char *basename = new char[path.size() + 1];
    bu_basename(basename, path.c_str());
    m_obj_map[ROOT_UUID].m_name = basename;
    delete[] basename;

    map_uuid_names();
    create_all_idefs();
    create_all_geometry();
    nest_all_layers();
    create_all_layers();

    m_model->Destroy();

    m_log->Print("Done.\n");
}




void
RhinoConverter::clean_model()
{
    if (!m_model->IsValid(m_log.get())) {
	m_log->Print("Model is NOT valid. Attempting repairs.\n");

	m_model->Polish(); // fill in defaults

	int repair_count = 0;
	ON_SimpleArray<int> warnings;
	m_model->Audit(true, &repair_count, m_log.get(), &warnings); // repair

	m_log->Print("%d objects were repaired.\n", repair_count);
	for (int warn_i = 0; warn_i < warnings.Count(); ++warn_i)
	    m_log->Print("%s\n", warnings[warn_i]);

	if (m_model->IsValid(m_log.get()))
	    m_log->Print("Repair successful, model is now valid.\n");
	else
	    m_log->Print("Repair unsuccessful, model is still NOT valid.\n");
    }
}




void
RhinoConverter::map_uuid_names()
{
    for (int i = 0; i < m_model->m_object_table.Count(); ++i) {
	const ON_Object *object = m_model->m_object_table[i].m_object;
	const ON_3dmObjectAttributes &myAttributes = m_model->m_object_table[i].m_attributes;
	const std::string obj_uuid = UUIDstr(myAttributes.m_uuid);

	std::string suffix = ".s";
	if (object->ObjectType() == ON::instance_reference)
	    suffix = ".c";

	if (m_use_uuidnames)
	    m_obj_map[obj_uuid].m_name = obj_uuid + suffix;
	else
	    m_obj_map[obj_uuid].m_name = unique_name(m_obj_map,
					 CleanName(myAttributes.m_name), suffix);

    }


    for (int i = 0; i < m_model->m_idef_table.Count(); ++i) {
	const ON_InstanceDefinition &idef = m_model->m_idef_table[i];
	const std::string idef_uuid = UUIDstr(idef.m_uuid);

	if (m_use_uuidnames)
	    m_obj_map[idef_uuid].m_name = idef_uuid + ".c";
	else
	    m_obj_map[idef_uuid].m_name =
		unique_name(m_obj_map, CleanName(idef.Name()), ".c");
    }


    for (int i = 0; i < m_model->m_layer_table.Count(); ++i) {
	const ON_Layer &layer = m_model->m_layer_table[i];
	const std::string layer_uuid = UUIDstr(layer.m_layer_id);

	std::string suffix = ".c";
	if (!m_random_colors && is_toplevel(layer))
	    suffix = ".r";

	if (m_use_uuidnames)
	    m_obj_map[layer_uuid].m_name = layer_uuid + suffix;
	else
	    m_obj_map[layer_uuid].m_name =
		unique_name(m_obj_map, CleanName(layer.m_name), suffix);
    }
}




void
RhinoConverter::nest_all_layers()
{
    for (int i = 0; i < m_model->m_layer_table.Count(); ++i) {
	const ON_Layer &layer = m_model->m_layer_table[i];
	const std::string layer_uuid = UUIDstr(layer.m_layer_id);
	const std::string parent_layer_uuid = UUIDstr(layer.m_parent_layer_id);

	m_obj_map.at(parent_layer_uuid).m_children.push_back(layer_uuid);
    }
}




void
RhinoConverter::create_all_layers()
{
    for (int i = 0; i < m_model->m_layer_table.Count(); ++i) {
	const ON_Layer &layer = m_model->m_layer_table[i];
	const std::string &layer_name = m_obj_map.at(UUIDstr(layer.m_layer_id)).m_name;
	m_log->Print("Creating layer '%s'\n", layer_name.c_str());
	create_layer(layer);
    }
}




void
RhinoConverter::create_layer(const ON_Layer &layer)
{
    const std::string layer_uuid = UUIDstr(layer.m_layer_id);
    const std::vector<std::string> &child_vec = m_obj_map.at(layer_uuid).m_children;
    const bool is_region = !m_random_colors && is_toplevel(layer);
    const bool do_inherit = false;

    const ON_Material &material = m_model->m_material_table[layer.m_material_index];
    const std::string shader_args = get_shader_args(material);


    std::string layer_name = m_obj_map.at(layer_uuid).m_name;
    if (layer_name == DEFAULT_LAYER_NAME + ".c" || layer_name == DEFAULT_LAYER_NAME + ".r")
	layer_name = m_obj_map.at(ROOT_UUID).m_name;

    wmember members;
    BU_LIST_INIT(&members.l);
    for (std::vector<std::string>::const_iterator it = child_vec.begin();
	 it != child_vec.end(); ++it) {
	const ModelObject &obj = m_obj_map.at(*it);
	if (obj.is_in_idef) continue;
	mk_addmember(obj.m_name.c_str(), &members.l, NULL, WMOP_UNION);
    }

    // FIXME code duplication
    Color color;
    if (m_random_colors)
	color = Color::random();
    else
	color = layer.m_color;

    if (color == Color(0, 0, 0)) {
	m_log->Print("Object has no color; setting color to red\n");
	color = Color(255, 0, 0);
    }

    mk_comb(m_db, layer_name.c_str(), &members.l, is_region,
	    DEFAULT_SHADER.c_str(), shader_args.c_str(), color.get_rgb(),
	    0, 0, 0, 0, do_inherit, false, false);

}




void
RhinoConverter::create_all_idefs()
{
    m_log->Print("Creating ON_InstanceDefinitions...\n");

    for (int i = 0; i < m_model->m_idef_table.Count(); ++i) {
	const ON_InstanceDefinition &idef = m_model->m_idef_table[i];
	create_idef(idef);
    }
}




void
RhinoConverter::create_idef(const ON_InstanceDefinition &idef)
{
    wmember members;
    BU_LIST_INIT(&members.l);

    for (int i = 0; i < idef.m_object_uuid.Count(); ++i) {
	const std::string member_uuid = UUIDstr(idef.m_object_uuid[i]);

	const int geom_index = m_model->ObjectIndex(idef.m_object_uuid[i]);
	if (geom_index == -1) {
	    m_log->Print("referenced uuid=%s does not exist\n", member_uuid.c_str());
	    continue;
	}

	std::string &member_name = m_obj_map.at(member_uuid).m_name;
	mk_addmember(member_name.c_str(), &members.l, NULL, WMOP_UNION);
	m_obj_map.at(member_uuid).is_in_idef = true;
    }

    const std::string &idef_name = m_obj_map.at(UUIDstr(idef.m_uuid)).m_name;
    mk_lfcomb(m_db, idef_name.c_str(), &members, false);
}




void
RhinoConverter::create_iref(const ON_InstanceRef &iref,
			    const ON_3dmObjectAttributes &iref_attrs)
{
    mat_t matrix;
    xform2mat_t(iref.m_xform, matrix);

    wmember members;
    BU_LIST_INIT(&members.l);

    const std::string iref_uuid = UUIDstr(iref_attrs.m_uuid);
    const std::string &iref_name = m_obj_map.at(iref_uuid).m_name;
    const bool do_inherit = false;

    const ON_Material &material = m_model->m_material_table[iref_attrs.m_material_index];
    const std::string shader_args = get_shader_args(material);

    const std::string member_uuid = UUIDstr(iref.m_instance_definition_uuid);
    const std::string &member_name = m_obj_map.at(member_uuid).m_name;

    mk_addmember(member_name.c_str(), &members.l, matrix, WMOP_UNION);
    mk_comb(m_db, iref_name.c_str(), &members.l, false,
	    DEFAULT_SHADER.c_str(), shader_args.c_str(),
	    get_color(iref_attrs).get_rgb(),
	    0, 0, 0, 0, do_inherit, false, false);

    const std::string parent_uuid =
	UUIDstr(m_model->m_layer_table[iref_attrs.m_layer_index].m_layer_id);
    m_obj_map.at(parent_uuid).m_children.push_back(iref_uuid);
}




RhinoConverter::Color
RhinoConverter::get_color(const ON_3dmObjectAttributes &obj_attrs)
{
    Color color;
    if (m_random_colors)
	return Color::random();
    else
	switch (obj_attrs.ColorSource()) {
	    case ON::color_from_parent:
	    case ON::color_from_layer:
		color = m_model->m_layer_table[obj_attrs.m_layer_index].m_color;
		break;

	    case ON::color_from_object:
		color = obj_attrs.m_color;
		break;

	    case ON::color_from_material:
		color = m_model->m_material_table[obj_attrs.m_material_index].m_ambient;
		break;

	    default:
		m_log->Print("ERROR: unknown color source\n");
		return Color(255, 0, 0);
	}


    if (color == Color(0, 0, 0)) {
	m_log->Print("Object has no color; setting color to red\n");
	return Color(255, 0, 0);
    } else
	return color;
}




void
RhinoConverter::create_brep(const ON_Brep &brep, const ON_3dmObjectAttributes &brep_attrs)
{
    const std::string brep_uuid = UUIDstr(brep_attrs.m_uuid);
    const std::string &brep_name = m_obj_map.at(brep_uuid).m_name;

    m_log->Print("Creating BREP '%s'\n", brep_name.c_str());

    mk_brep(m_db, brep_name.c_str(), const_cast<ON_Brep *>(&brep));
    const std::string parent_uuid =
	UUIDstr(m_model->m_layer_table[brep_attrs.m_layer_index].m_layer_id);

    m_obj_map.at(parent_uuid).m_children.push_back(brep_uuid);
}




void
RhinoConverter::create_all_geometry()
{
    for (int i = 0; i < m_model->m_object_table.Count(); ++i) {
	const ON_3dmObjectAttributes &obj_attrs = m_model->m_object_table[i].m_attributes;
	const std::string obj_uuid = UUIDstr(obj_attrs.m_uuid);
	const std::string &obj_name = m_obj_map.at(obj_uuid).m_name;

	m_log->Print("Object %d of %d...", i + 1, m_model->m_object_table.Count());

	const ON_Geometry *pGeometry = ON_Geometry::Cast(m_model->m_object_table[i].m_object);
	if (pGeometry)
	    create_geometry(pGeometry, obj_attrs);
	else
	    m_log->Print("WARNING: Skipping non-Geometry entity: %s\n", obj_name.c_str());
    }
}




void
RhinoConverter::create_geometry(const ON_Geometry *pGeometry,
				const ON_3dmObjectAttributes &obj_attrs)
{
    const ON_Brep *brep;
    const ON_Curve *curve;
    const ON_Surface *surface;
    const ON_Mesh *mesh;
    const ON_RevSurface *revsurf;
    const ON_PlaneSurface *planesurf;
    const ON_InstanceDefinition *instdef;
    const ON_InstanceRef *instref;
    const ON_Layer *layer;
    const ON_Light *light;
    const ON_NurbsCage *nurbscage;
    const ON_MorphControl *morphctrl;
    const ON_Group *group;
    const ON_Geometry *geom;

    if ((brep = ON_Brep::Cast(pGeometry))) {
	create_brep(*brep, obj_attrs);
    } else if (pGeometry->HasBrepForm()) {
	ON_Brep *new_brep = pGeometry->BrepForm();
	create_brep(*new_brep, obj_attrs);
	delete new_brep;
    } else if ((curve = ON_Curve::Cast(pGeometry))) {
	m_log->Print("Skipping: Type: ON_Curve\n");
	if (verbose_mode) curve->Dump(*m_log);
    } else if ((surface = ON_Surface::Cast(pGeometry))) {
	m_log->Print("Skipping: Type: ON_Surface\n");
	if (verbose_mode) surface->Dump(*m_log);
    } else if ((mesh = ON_Mesh::Cast(pGeometry))) {
	m_log->Print("Skipping: Type: ON_Mesh\n");
	if (verbose_mode) mesh->Dump(*m_log);
    } else if ((revsurf = ON_RevSurface::Cast(pGeometry))) {
	m_log->Print("Skipping: Type: ON_RevSurface\n");
	if (verbose_mode) revsurf->Dump(*m_log);
    } else if ((planesurf = ON_PlaneSurface::Cast(pGeometry))) {
	m_log->Print("Skipping: Type: ON_PlaneSurface\n");
	if (verbose_mode) planesurf->Dump(*m_log);
    } else if ((instdef = ON_InstanceDefinition::Cast(pGeometry))) {
	m_log->Print("Skipping: Type: ON_InstanceDefinition\n");
	if (verbose_mode) instdef->Dump(*m_log);
    } else if ((instref = ON_InstanceRef::Cast(pGeometry))) {
	m_log->Print("Type: ON_InstanceRef\n");
	if (verbose_mode) instref->Dump(*m_log);

	create_iref(*instref, obj_attrs);
    } else if ((layer = ON_Layer::Cast(pGeometry))) {
	m_log->Print("Skipping: Type: ON_Layer\n");
	if (verbose_mode) layer->Dump(*m_log);
    } else if ((light = ON_Light::Cast(pGeometry))) {
	m_log->Print("Skipping: Type: ON_Light\n");
	if (verbose_mode) light->Dump(*m_log);
    } else if ((nurbscage = ON_NurbsCage::Cast(pGeometry))) {
	m_log->Print("Skipping: Type: ON_NurbsCage\n");
	if (verbose_mode) nurbscage->Dump(*m_log);
    } else if ((morphctrl = ON_MorphControl::Cast(pGeometry))) {
	m_log->Print("Skipping: Type: ON_MorphControl\n");
	if (verbose_mode) morphctrl->Dump(*m_log);
    } else if ((group = ON_Group::Cast(pGeometry))) {
	m_log->Print("Skipping:Type: ON_Group\n");
	if (verbose_mode) group->Dump(*m_log);
    } else if ((geom = ON_Geometry::Cast(pGeometry))) {
	m_log->Print("Skipping: Type: ON_Geometry\n");
	if (verbose_mode) geom->Dump(*m_log);
    } else {
	m_log->Print("WARNING: Encountered an unexpected kind of object.\n"
		     "Please report to devs@brlcad.org\n");
    }

    if (verbose_mode) {
	m_log->PopIndent();
	m_log->Print("\n\n");
    } else
	m_log->Print("\n");
}




}




#endif //OBJ_BREP




/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
