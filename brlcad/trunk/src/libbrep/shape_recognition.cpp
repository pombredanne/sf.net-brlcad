#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

curve_t
GetCurveType(ON_Curve *curve)
{
    /* First, see if the curve is planar */
/*    if (!curve->IsPlanar()) {
	return CURVE_GENERAL;
    }*/

    /* Check types in order of increasing complexity - simple
     * is better, so try simple first */
    ON_BoundingBox c_bbox;
    curve->GetTightBoundingBox(c_bbox);
    if (c_bbox.IsDegenerate() == 3) return CURVE_POINT;

    if (curve->IsLinear()) return CURVE_LINE;

    ON_Arc arc;
    if (curve->IsArc(NULL, &arc, BREP_CYLINDRICAL_TOL)) {
	if (arc.IsCircle()) return CURVE_CIRCLE;
	ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
	bu_log("arc's circle: center %f, %f, %f   radius %f\n", circ.Center().x, circ.Center().y, circ.Center().z, circ.Radius());
       	return CURVE_ARC;
    }

    // TODO - looks like we need a better test for this...
    if (curve->IsEllipse(NULL, NULL, BREP_ELLIPSOIDAL_TOL)) return CURVE_ELLIPSE;

    return CURVE_GENERAL;
}

surface_t
GetSurfaceType(const ON_Surface *in_surface, struct filter_obj *obj)
{
    surface_t ret = SURFACE_GENERAL;
    ON_Surface *surface = in_surface->Duplicate();
    if (obj) {
	filter_obj_init(obj);
	if (surface->IsPlanar(obj->plane, BREP_PLANAR_TOL)) {
	    ret = SURFACE_PLANE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsSphere(obj->sphere , BREP_SPHERICAL_TOL)) {
	    ret = SURFACE_SPHERE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsCylinder(obj->cylinder , BREP_CYLINDRICAL_TOL)) {
	    ret = SURFACE_CYLINDER;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsCone(obj->cone, BREP_CONIC_TOL)) {
	    ret = SURFACE_CONE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsTorus(obj->torus, BREP_TOROIDAL_TOL)) {
	    ret = SURFACE_TORUS;
	    goto st_done;
	}
    } else {
	if (surface->IsPlanar(NULL, BREP_PLANAR_TOL)) {
	    ret = SURFACE_PLANE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsSphere(NULL, BREP_SPHERICAL_TOL)) {
	    ret = SURFACE_SPHERE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsCylinder(NULL, BREP_CYLINDRICAL_TOL)) {
	    ret = SURFACE_CYLINDER;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsCone(NULL, BREP_CONIC_TOL)) {
	    ret = SURFACE_CONE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsTorus(NULL, BREP_TOROIDAL_TOL)) {
	    ret = SURFACE_TORUS;
	    goto st_done;
	}
    }
st_done:
    delete surface;
    return ret;
}

void
filter_obj_init(struct filter_obj *obj)
{
    if (!obj) return;
    if (!obj->plane) obj->plane = new ON_Plane;
    if (!obj->sphere) obj->sphere = new ON_Sphere;
    if (!obj->cylinder) obj->cylinder = new ON_Cylinder;
    if (!obj->cone) obj->cone = new ON_Cone;
    if (!obj->torus) obj->torus = new ON_Torus;
    obj->type = BREP;
}

void
filter_obj_free(struct filter_obj *obj)
{
    if (!obj) return;
    delete obj->plane;
    delete obj->sphere;
    delete obj->cylinder;
    delete obj->cone;
    delete obj->torus;
}

int
subbrep_is_planar(struct subbrep_object_data *data)
{
    int i = 0;
    // Check surfaces.  If a surface is anything other than a plane the verdict is no.
    // If all surfaces are planes, then the verdict is yes.
    for (i = 0; i < data->faces_cnt; i++) {
	if (GetSurfaceType(data->brep->m_F[data->faces[i]].SurfaceOf(), NULL) != SURFACE_PLANE) return 0;
    }
    data->type = PLANAR_VOLUME;
    return 1;
}


int
subbrep_is_cylinder(struct subbrep_object_data *data, fastf_t cyl_tol)
{
    std::set<int>::iterator f_it;
    std::set<int> planar_surfaces;
    std::set<int> cylindrical_surfaces;
    std::set<int> active_edges;
    // First, check surfaces.  If a surface is anything other than a plane or cylindrical,
    // the verdict is no.  If we don't have at least two planar surfaces and one
    // cylindrical, the verdict is no.
    for (int i = 0; i < data->faces_cnt; i++) {
	int f_ind = data->faces[i];
	ON_BrepFace *used_face = &(data->brep->m_F[f_ind]);
        int surface_type = (int)GetSurfaceType(used_face->SurfaceOf(), NULL);
        switch (surface_type) {
            case SURFACE_PLANE:
                planar_surfaces.insert(f_ind);
                break;
            case SURFACE_CYLINDER:
                cylindrical_surfaces.insert(f_ind);
                break;
            default:
                return 0;
                break;
        }
    }
    if (planar_surfaces.size() < 2) return 0;
    if (cylindrical_surfaces.size() < 1) return 0;

    // Second, check if all cylindrical surfaces share the same axis.
    ON_Cylinder cylinder;
    ON_Surface *cs = data->brep->m_F[*cylindrical_surfaces.begin()].SurfaceOf()->Duplicate();
    cs->IsCylinder(&cylinder);
    delete cs;
    for (f_it = cylindrical_surfaces.begin(); f_it != cylindrical_surfaces.end(); f_it++) {
        ON_Cylinder f_cylinder;
	ON_Surface *fcs = data->brep->m_F[(*f_it)].SurfaceOf()->Duplicate();
        fcs->IsCylinder(&f_cylinder);
	delete fcs;
	if (f_cylinder.circle.Center().DistanceTo(cylinder.circle.Center()) > BREP_CYLINDRICAL_TOL) return 0;
    }
    // Third, see if all planes are coplanar with two and only two planes.
    ON_Plane p1, p2;
    int p2_set = 0;
    data->brep->m_F[*planar_surfaces.begin()].SurfaceOf()->IsPlanar(&p1);
    for (f_it = planar_surfaces.begin(); f_it != planar_surfaces.end(); f_it++) {
        ON_Plane f_p;
        data->brep->m_F[(*f_it)].SurfaceOf()->IsPlanar(&f_p);
        if (!p2_set && f_p != p1) {
            p2 = f_p;
            continue;
        };
        if (f_p != p1 && f_p != p2) return 0;
    }

    // Fourth, check that the two planes are parallel to each other.
    if (p1.Normal().IsParallelTo(p2.Normal(), cyl_tol) == 0) {
        std::cout << "p1 Normal: " << p1.Normal().x << "," << p1.Normal().y << "," << p1.Normal().z << "\n";
        std::cout << "p2 Normal: " << p2.Normal().x << "," << p2.Normal().y << "," << p2.Normal().z << "\n";
        return 0;
    }

    // Fifth, remove degenerate edge sets. A degenerate edge set is defined as two
    // linear segments having the same two vertices.  (To be sure, we should probably
    // check curve directions in loops in some fashion...)
    std::set<int> degenerate;
    for (int i = 0; i < data->edges_cnt; i++) {
	int e_it = data->edges[i];
	if (degenerate.find(e_it) == degenerate.end()) {
	    ON_BrepEdge& edge = data->brep->m_E[e_it];
	    if (edge.EdgeCurveOf()->IsLinear()) {
		for (int j = 0; j < data->edges_cnt; j++) {
		    int f_ind = data->edges[j];
		    ON_BrepEdge& edge2 = data->brep->m_E[f_ind];
		    if (edge2.EdgeCurveOf()->IsLinear()) {
			if ((edge.Vertex(0)->Point() == edge2.Vertex(0)->Point() && edge.Vertex(1)->Point() == edge2.Vertex(1)->Point()) ||
				(edge.Vertex(1)->Point() == edge2.Vertex(0)->Point() && edge.Vertex(0)->Point() == edge2.Vertex(1)->Point()))
			{
			    degenerate.insert(e_it);
			    degenerate.insert(f_ind);
			    break;
			}
		    }
		}
	    }
	}
    }
    for (int i = 0; i < data->edges_cnt; i++) {
	active_edges.insert(data->edges[i]);
    }
    std::set<int>::iterator e_it;
    for (e_it = degenerate.begin(); e_it != degenerate.end(); e_it++) {
        //std::cout << "erasing " << *e_it << "\n";
        active_edges.erase(*e_it);
    }
    //std::cout << "Active Edge set: ";
#if 0
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
        std::cout << (int)(*e_it);
        f_it = e_it;
        f_it++;
        if (f_it != active_edges.end()) std::cout << ",";
    }
    std::cout << "\n";
#endif

    // Sixth, check for any remaining linear segments.  For rpc primitives
    // those are expected, but for a true cylinder the linear segments should
    // all wash out in the degenerate pass.
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
        ON_BrepEdge& edge = data->brep->m_E[*e_it];
        if (edge.EdgeCurveOf()->IsLinear()) return 0;
    }

    // Seventh, sort the curved edges into one of two circles.  Again, in more
    // general cases we might have other curves but a true cylinder should have
    // all of its arcs on these two circles.  We don't need to check for closed
    // loops because we are assuming that in the input Brep; any curve except
    // arc curves that survived the degeneracy test has already resulted in an
    // earlier rejection.
    std::set<int> arc_set_1, arc_set_2;
    ON_Circle set1_c, set2_c;
    int arc1_circle_set= 0;
    int arc2_circle_set = 0;
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
        ON_BrepEdge& edge = data->brep->m_E[*e_it];
        ON_Arc arc;
        if (edge.EdgeCurveOf()->IsArc(NULL, &arc, cyl_tol)) {
            int assigned = 0;
            ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
            //std::cout << "circ " << circ.Center().x << " " << circ.Center().y << " " << circ.Center().z << "\n";
            if (!arc1_circle_set) {
                arc1_circle_set = 1;
                set1_c = circ;
                //std::cout << "center 1 " << set1_c.Center().x << " " << set1_c.Center().y << " " << set1_c.Center().z << "\n";
            } else {
                if (!arc2_circle_set) {
                    if (!(NEAR_ZERO(circ.Center().DistanceTo(set1_c.Center()), cyl_tol))){
                        arc2_circle_set = 1;
                        set2_c = circ;
                        //std::cout << "center 2 " << set2_c.Center().x << " " << set2_c.Center().y << " " << set2_c.Center().z << "\n";
                    }
                }
            }
            if (NEAR_ZERO(circ.Center().DistanceTo(set1_c.Center()), cyl_tol)){
                arc_set_1.insert(*e_it);
                assigned = 1;
            }
            if (arc2_circle_set) {
                if (NEAR_ZERO(circ.Center().DistanceTo(set2_c.Center()), cyl_tol)){
                    arc_set_2.insert(*e_it);
                    assigned = 1;
                }
            }
            if (!assigned) {
                std::cout << "found extra circle - no go\n";
                return 0;
            }
        }
    }

    data->type = CYLINDER;

    ON_3dVector hvect(set2_c.Center() - set1_c.Center());

    data->params->origin[0] = set1_c.Center().x;
    data->params->origin[1] = set1_c.Center().y;
    data->params->origin[2] = set1_c.Center().z;
    data->params->hv[0] = hvect.x;
    data->params->hv[1] = hvect.y;
    data->params->hv[2] = hvect.z;
    data->params->radius = set1_c.Radius();

    //std::cout << "rcc: in " << bu_vls_addr(data->key) << " rcc " << set1_c.Center().x << " " << set1_c.Center().y << " " << set1_c.Center().z << " " << hvect.x << " " << hvect.y << " " << hvect.z << " " << set1_c.Radius() << "\n";
    //mk_rcc(data->wdbp, data->key.c_str(), base, hv, set1_c.Radius());

    // TODO - check for different radius values between the two circles - for a pure cylinder test should reject, but we can easily handle it with the tgc...

    return 1;
}


volume_t
subbrep_shape_recognize(struct subbrep_object_data *data)
{
    if (subbrep_is_planar(data)) return PLANAR_VOLUME;
    //if (BU_STR_EQUAL(bu_vls_addr(data->key), "390_391_418_591_592.s")) {
    if (subbrep_is_cylinder(data, BREP_CYLINDRICAL_TOL)) return CYLINDER;
    if (subbrep_split(data)) return COMB;
    //}
    return BREP;
}

void
subbrep_object_init(struct subbrep_object_data *obj, ON_Brep *brep)
{
    if (!obj) return;
    BU_GET(obj->params, struct csg_object_params);
    BU_GET(obj->key, struct bu_vls);
    BU_GET(obj->children, struct bu_ptbl);
    bu_vls_init(obj->key);
    bu_ptbl_init(obj->children, 8, "children table");
    obj->parent = NULL;
    obj->brep = brep;
    obj->type = BREP;
}

void
subbrep_object_free(struct subbrep_object_data *obj)
{
    if (!obj) return;
    BU_PUT(obj->params, struct csg_object_params);
    bu_vls_free(obj->key);
    BU_PUT(obj->key, struct bu_vls);
    for (unsigned int i = 0; i < BU_PTBL_LEN(obj->children); i++){
	//struct subbrep_object_data *obj = (struct subbrep_object_data *)BU_PTBL_GET(obj->children, i);
	//subbrep_object_free(obj);
    }
    bu_ptbl_free(obj->children);
    BU_PUT(obj->children, struct bu_ptbl);
    if (obj->faces) bu_free(obj->faces, "obj faces");
    if (obj->loops) bu_free(obj->loops, "obj loops");
    if (obj->edges) bu_free(obj->edges, "obj edges");
    if (obj->fol) bu_free(obj->fol, "obj fol");
    if (obj->fil) bu_free(obj->fil, "obj fil");
    obj->parent = NULL;
    obj->brep = NULL;
}


std::string
face_set_key(std::set<int> fset)
{
    std::set<int>::iterator s_it;
    std::set<int>::iterator s_it2;
    std::string key;
    struct bu_vls vls_key = BU_VLS_INIT_ZERO;
    for (s_it = fset.begin(); s_it != fset.end(); s_it++) {
	bu_vls_printf(&vls_key, "%d", (*s_it));
	s_it2 = s_it;
	s_it2++;
	if (s_it2 != fset.end()) bu_vls_printf(&vls_key, "_");
    }
    bu_vls_printf(&vls_key, ".s");
    key.append(bu_vls_addr(&vls_key));
    bu_vls_free(&vls_key);
    return key;
}

void
set_to_array(int **array, int *array_cnt, std::set<int> *set)
{
    std::set<int>::iterator s_it;
    int i = 0;
    (*array_cnt) = set->size();
    if ((*array_cnt) > 0) {
	(*array) = (int *)bu_calloc((*array_cnt), sizeof(int), "array");
	for (s_it = set->begin(); s_it != set->end(); s_it++) {
	    (*array)[i] = *s_it;
	    i++;
	}
    }
}

void
array_to_set(std::set<int> *set, int *array, int array_cnt)
{
    for (int i = 0; i < array_cnt; i++) {
	set->insert(array[i]);
    }
}

struct bu_ptbl *
find_subbreps(ON_Brep *brep)
{
    struct bu_ptbl *subbreps;
    std::set<std::string> subbrep_keys;
    BU_GET(subbreps, struct bu_ptbl);
    bu_ptbl_init(subbreps, 8, "subbrep table");

    /* For each face, build the topologically connected subset.  If that
     * subset has not already been seen, add it to the brep's set of
     * subbreps */
    for (int i = 0; i < brep->m_F.Count(); i++) {
	std::string key;
	std::set<int> faces;
	std::set<int> loops;
	std::set<int> edges;
	std::set<int> fol; /* Faces with outer loops in object loop network */
	std::set<int> fil; /* Faces with only inner loops in object loop network */
	std::queue<int> local_loops;
	std::set<int> processed_loops;
	std::set<int>::iterator s_it;

	ON_BrepFace *face = &(brep->m_F[i]);
	faces.insert(i);
	fol.insert(i);
	local_loops.push(face->OuterLoop()->m_loop_index);
	processed_loops.insert(face->OuterLoop()->m_loop_index);
	while(!local_loops.empty()) {
	    ON_BrepLoop* loop = &(brep->m_L[local_loops.front()]);
	    loops.insert(local_loops.front());
	    local_loops.pop();
	    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
		ON_BrepTrim& trim = face->Brep()->m_T[loop->m_ti[ti]];
		ON_BrepEdge& edge = face->Brep()->m_E[trim.m_ei];
		if (trim.m_ei != -1 && edge.TrimCount() > 1) {
		    edges.insert(trim.m_ei);
		    for (int j = 0; j < edge.TrimCount(); j++) {
			int fio = edge.Trim(j)->FaceIndexOf();
			if (edge.m_ti[j] != ti && fio != -1) {
			    int li = edge.Trim(j)->Loop()->m_loop_index;
			    faces.insert(fio);
			    if (processed_loops.find(li) == processed_loops.end()) {
				local_loops.push(li);
				processed_loops.insert(li);
			    }
			    if (li == brep->m_F[fio].OuterLoop()->m_loop_index) {
				fol.insert(fio);
			    }
			}
		    }
		}
	    }
	}
	for (s_it = faces.begin(); s_it != faces.end(); s_it++) {
	    if (fol.find(*s_it) == fol.end()) {
		fil.insert(*s_it);
	    }
	}
	key = face_set_key(faces);

	/* If we haven't seen this particular subset before, add it */
	if (subbrep_keys.find(key) == subbrep_keys.end()) {
	    subbrep_keys.insert(key);
	    struct subbrep_object_data *new_obj;
	    BU_GET(new_obj, struct subbrep_object_data);
	    subbrep_object_init(new_obj, brep);
	    bu_vls_sprintf(new_obj->key, "%s", key.c_str());
	    set_to_array(&(new_obj->faces), &(new_obj->faces_cnt), &faces);
	    set_to_array(&(new_obj->loops), &(new_obj->loops_cnt), &loops);
	    set_to_array(&(new_obj->edges), &(new_obj->edges_cnt), &edges);
	    set_to_array(&(new_obj->fol), &(new_obj->fol_cnt), &fol);
	    set_to_array(&(new_obj->fil), &(new_obj->fil_cnt), &fil);

	    (void)subbrep_shape_recognize(new_obj);

	    bu_ptbl_ins(subbreps, (long *)new_obj);
	}
    }

    return subbreps;
}

void
print_subbrep_object(struct subbrep_object_data *data, const char *offset)
{
    struct bu_vls log = BU_VLS_INIT_ZERO;
    bu_vls_printf(&log, "\n");
    bu_vls_printf(&log, "%sObject %s:\n", offset, bu_vls_addr(data->key));
    bu_vls_printf(&log, "%sType: ", offset);
    switch (data->type) {
	case COMB:
	    bu_vls_printf(&log, "comb\n");
	    break;
	case PLANAR_VOLUME:
	    bu_vls_printf(&log, "planar\n");
	    break;
	case CYLINDER:
	    bu_vls_printf(&log, "cylinder\n");
	    break;
	case CONE:
	    bu_vls_printf(&log, "cone\n");
	    break;
	case SPHERE:
	    bu_vls_printf(&log, "sphere\n");
	    break;
	case ELLIPSOID:
	    bu_vls_printf(&log, "ellipsoid\n");
	    break;
	case TORUS:
	    bu_vls_printf(&log, "torus\n");
	    break;
	default:
	    bu_vls_printf(&log, "brep\n");
    }
    bu_vls_printf(&log, "%sFace set: ", offset);
    for (int i = 0; i < data->faces_cnt; i++) {
	bu_vls_printf(&log, "%d", data->faces[i]);
	if (i + 1 != data->faces_cnt) bu_vls_printf(&log, ",");
    }
    bu_vls_printf(&log, "\n");
    bu_vls_printf(&log, "%sLoop set: ", offset);
    for (int i = 0; i < data->loops_cnt; i++) {
	bu_vls_printf(&log, "%d", data->loops[i]);
	if (i + 1 != data->loops_cnt) bu_vls_printf(&log, ",");
    }
    bu_vls_printf(&log, "\n");
    bu_vls_printf(&log, "%sEdge set: ", offset);
    for (int i = 0; i < data->edges_cnt; i++) {
	bu_vls_printf(&log, "%d", data->edges[i]);
	if (i + 1 != data->edges_cnt) bu_vls_printf(&log, ",");
    }
    bu_vls_printf(&log, "\n");
    bu_vls_printf(&log, "%sFaces with outer loop set: ", offset);
    for (int i = 0; i < data->fol_cnt; i++) {
	bu_vls_printf(&log, "%d", data->fol[i]);
	if (i + 1 != data->fol_cnt) bu_vls_printf(&log, ",");
    }
    bu_vls_printf(&log, "\n");
    bu_vls_printf(&log, "%sFaces with only inner loops set: ", offset);
    for (int i = 0; i < data->fil_cnt; i++) {
	bu_vls_printf(&log, "%d", data->fil[i]);
	if (i + 1 != data->fil_cnt) bu_vls_printf(&log, ",");
    }
    bu_vls_printf(&log, "\n");

    bu_log("%s\n", bu_vls_addr(&log));
    bu_vls_free(&log);
}





int
subbrep_highest_order_face(struct subbrep_object_data *data)
{
    int planar = 0;
    int spherical = 0;
    int rcylindrical = 0;
    int cone = 0;
    int torus = 0;
    int general = 0;
    int hof = -1;
    int hofo = 0;
    for (int f_it = 0; f_it < data->faces_cnt; f_it++) {
	ON_BrepFace *used_face = &(data->brep->m_F[f_it]);
	int surface_type = (int)GetSurfaceType(used_face->SurfaceOf(), NULL);
	switch (surface_type) {
	    case SURFACE_PLANE:
		planar++;
		if (hofo < 1) {
		    hof = f_it;
		    hofo = 1;
		}
		break;
	    case SURFACE_SPHERE:
		spherical++;
		if (hofo < 2) {
		    hof = f_it;
		    hofo = 2;
		}
		break;
	    case SURFACE_CYLINDER:
		if (!cylindrical_planar_vertices(data, f_it)) {
		    std::cout << "irregular cylindrical: " << f_it << "\n";
		    general++;
		} else {
		    rcylindrical++;
		    if (hofo < 3) {
			hof = f_it;
			hofo = 3;
		    }
		}
		break;
	    case SURFACE_CONE:
		cone++;
		if (hofo < 4) {
		    hof = f_it;
		    hofo = 4;
		}
		break;
	    case SURFACE_TORUS:
		torus++;
		if (hofo < 4) {
		    hof = f_it;
		    hofo = 4;
		}
		break;
	    default:
		general++;
		std::cout << "general surface: " << used_face->SurfaceIndexOf() << "\n";
		break;
	}
    }

    if (!general)
	std::cout << "highest order face: " << hof << "(" << hofo << ")\n";

    std::cout << "\n";
    std::cout << bu_vls_addr(data->key) << ":\n";
    std::cout << "planar_cnt: " << planar << "\n";
    std::cout << "spherical_cnt: " << spherical << "\n";
    std::cout << "regular cylindrical_cnt: " << rcylindrical << "\n";
    std::cout << "cone_cnt: " << cone << "\n";
    std::cout << "torus_cnt: " << torus << "\n";
    std::cout << "general_cnt: " << general << "\n";
    std::cout << "\n";
    return hof;
}

void
set_filter_obj(ON_BrepFace *face, struct filter_obj *obj)
{
    if (!obj) return;
    filter_obj_init(obj);
    obj->stype = GetSurfaceType(face->SurfaceOf(), obj);
    // If we've got a planar face, we can stop now - planar faces
    // are determinative of volume type only when *all* faces are planar,
    // and that case is handled elsewhere - anything is "allowed".
    if (obj->stype == SURFACE_PLANE) obj->type = BREP;

    // If we've got a general surface, anything is allowed
    if (obj->stype == SURFACE_GENERAL) obj->type = BREP;

    if (obj->stype == SURFACE_CYLINDRICAL_SECTION || obj->stype == SURFACE_CYLINDER) obj->type = CYLINDER;

    if (obj->stype == SURFACE_CONE) obj->type = CONE;

    if (obj->stype == SURFACE_SPHERICAL_SECTION || obj->stype == SURFACE_SPHERE) obj->type = SPHERE;

}

int
cylindrical_loop_planar_vertices(ON_BrepFace *face, int loop_index)
{
    std::set<int> verts;
    ON_Brep *brep = face->Brep();
    ON_BrepLoop *loop = &(brep->m_L[loop_index]);
    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	ON_BrepTrim& trim = brep->m_T[loop->m_ti[ti]];
	if (trim.m_ei != -1) {
	    ON_BrepEdge& edge = brep->m_E[trim.m_ei];
	    verts.insert(edge.Vertex(0)->m_vertex_index);
	    verts.insert(edge.Vertex(1)->m_vertex_index);
	}
    }
    if (verts.size() == 3) {
	//std::cout << "Three points - planar.\n";
	return 1;
    } else if (verts.size() >= 3) {
	std::set<int>::iterator v_it = verts.begin();
	ON_3dPoint p1 = brep->m_V[*v_it].Point();
	v_it++;
	ON_3dPoint p2 = brep->m_V[*v_it].Point();
	v_it++;
	ON_3dPoint p3 = brep->m_V[*v_it].Point();
	ON_Plane test_plane(p1, p2, p3);
	for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	    if (!NEAR_ZERO(test_plane.DistanceTo(brep->m_V[*v_it].Point()), BREP_PLANAR_TOL)) {
		//std::cout << "vertex " << *v_it << " too far from plane, not planar: " << test_plane.DistanceTo(brep->m_V[*v_it].Point()) << "\n";
		return 0;
	    }
	}
	//std::cout << verts.size() << " points, planar\n";
	return 1;
    } else {
	//std::cout << "Closed single curve loop - planar only if surface is.";
	return 0;
    }
    return 0;
}

int
cylindrical_planar_vertices(struct subbrep_object_data *data, int face_index)
{
    std::set<int> loops;
    std::set<int>::iterator l_it;
    ON_BrepFace *face = &(data->brep->m_F[face_index]);
    array_to_set(&loops, data->loops, data->loops_cnt);
    for(l_it = loops.begin(); l_it != loops.end(); l_it++) {
	return cylindrical_loop_planar_vertices(face, face->m_li[*l_it]);
    }
    return 0;
}

int
apply_filter_obj(ON_BrepFace *face, int loop_index, struct filter_obj *obj)
{
    int ret = 1;
    struct filter_obj *local_obj;
    BU_GET(local_obj, struct filter_obj);
    int surface_type = (int)GetSurfaceType(face->SurfaceOf(), local_obj);

    std::set<int> allowed;

    if (obj->type == CYLINDER) {
	allowed.insert(SURFACE_CYLINDRICAL_SECTION);
	allowed.insert(SURFACE_CYLINDER);
	allowed.insert(SURFACE_PLANE);
    }

    if (obj->type == CONE) {
	allowed.insert(SURFACE_CONE);
	allowed.insert(SURFACE_PLANE);
    }

    if (obj->type == SPHERE) {
	allowed.insert(SURFACE_SPHERICAL_SECTION);
	allowed.insert(SURFACE_SPHERE);
	allowed.insert(SURFACE_PLANE);
    }

    // If the face's surface type is not part of the allowed set for
    // this object type, we're done
    if (allowed.find(surface_type) == allowed.end()) {
	ret = 0;
	goto filter_done;
    }
    if (obj->type == CYLINDER) {
	if (surface_type == SURFACE_PLANE) {
	    int is_parallel = obj->cylinder->Axis().IsParallelTo(local_obj->plane->Normal(), BREP_PLANAR_TOL);
	    if (is_parallel == 0) {
	       ret = 0;
	       goto filter_done;
	    }
	}
	if (surface_type == SURFACE_CYLINDER || surface_type == SURFACE_CYLINDRICAL_SECTION ) {
	    // Make sure the surfaces are on the same cylinder
	    if (obj->cylinder->circle.Center().DistanceTo(local_obj->cylinder->circle.Center()) > BREP_CYLINDRICAL_TOL) {
	       ret = 0;
	       goto filter_done;
	    }

	    // If something is funny with the loop, we can't (yet) handle it
	    if (!cylindrical_loop_planar_vertices(face, loop_index)) {
	       ret = 0;
	       std::cout << "weird loop in cylindrical face\n";
	       goto filter_done;
	    }
	}
    }
    if (obj->type == CONE) {
    }
    if (obj->type == SPHERE) {
    }
    if (obj->type == TORUS) {
    }

filter_done:
    filter_obj_free(local_obj);
    BU_PUT(local_obj, struct filter_obj);
    return ret;
}

void
add_loops_from_face(ON_BrepFace *face, struct subbrep_object_data *data, std::set<int> *loops, std::queue<int> *local_loops, std::set<int> *processed_loops)
{
    for (int j = 0; j < face->m_li.Count(); j++) {
	int loop_in_set = 0;
	int loop_ind = face->m_li[j];
	int k = 0;
	while (k < data->loops_cnt) {
	    if (data->loops[k] == loop_ind) loop_in_set = 1;
	    k++;
	}
	if (loop_in_set) loops->insert(loop_ind);
	if (loop_in_set && processed_loops->find(loop_ind) == processed_loops->end()) {
	    local_loops->push(loop_ind);
	}
    }
}

/* In order to represent complex shapes, it is necessary to identify
 * subsets of subbreps that can be represented as primitives.  This
 * function will identify such subsets, if possible.  If a subbrep
 * can be successfully decomposed into primitive candidates, the
 * type of the subbrep is set to COMB and the children bu_ptbl is
 * populated with subbrep_object_data sets. */
int
subbrep_split(struct subbrep_object_data *data)
{
    std::set<int> processed_faces;
    std::set<std::string> subbrep_keys;
    /* For each face, identify the candidate solid type.  If that
     * subset has not already been seen, add it to the brep's set of
     * subbreps */
    for (int i = 0; i < data->faces_cnt; i++) {
	std::string key;
	std::set<int> faces;
	std::set<int> loops;
	std::set<int> edges;
	std::queue<int> local_loops;
	std::set<int> processed_loops;
	std::set<int>::iterator s_it;
	struct filter_obj *filters;
	BU_GET(filters, struct filter_obj);
	std::set<int> locally_processed_faces;

	ON_BrepFace *face = &(data->brep->m_F[data->faces[i]]);
	set_filter_obj(face, filters);
	if (filters->type == BREP) {
	    filter_obj_free(filters);
	    BU_PUT(filters, struct filter_obj);
	    continue;
	}
	if (filters->stype == SURFACE_PLANE) {
	    filter_obj_free(filters);
	    BU_PUT(filters, struct filter_obj);
	    continue;
	}
	faces.insert(data->faces[i]);
	std::cout << "working: " << data->faces[i] << "\n";
	locally_processed_faces.insert(data->faces[i]);
	add_loops_from_face(face, data, &loops, &local_loops, &processed_loops);

	while(!local_loops.empty()) {
	    int curr_loop = local_loops.front();
	    local_loops.pop();
	    if (processed_loops.find(curr_loop) == processed_loops.end()) {
		ON_BrepLoop* loop = &(data->brep->m_L[curr_loop]);
		loops.insert(curr_loop);
		processed_loops.insert(curr_loop);
		for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
		    ON_BrepTrim& trim = face->Brep()->m_T[loop->m_ti[ti]];
		    ON_BrepEdge& edge = face->Brep()->m_E[trim.m_ei];
		    if (trim.m_ei != -1 && edge.TrimCount() > 1) {
			edges.insert(trim.m_ei);
			for (int j = 0; j < edge.TrimCount(); j++) {
			    int fio = edge.Trim(j)->FaceIndexOf();
			    if (fio != -1 && locally_processed_faces.find(fio) == locally_processed_faces.end()) {
				std::cout << fio << "\n";
				ON_BrepFace *fface = &(data->brep->m_F[fio]);
				surface_t stype = GetSurfaceType(fface->SurfaceOf(), NULL);
				// If fio meets the criteria for the candidate shape, add it.  Otherwise,
				// it's not part of this shape candidate
				if (apply_filter_obj(fface, curr_loop, filters)) {
				    // TODO - more testing needs to be done here...  get_allowed_surface_types
				    // returns the volume_t, use it to do some testing to evaluate
				    // things like normals and shared axis
				    std::cout << "accept: " << fio << "\n";
				    faces.insert(fio);
				    locally_processed_faces.insert(fio);
				    // The planar faces will always share edges with the non-planar
				    // faces, which drive the primary shape.  Also, planar faces are
				    // more likely to have other edges that relate to other shapes.
				    if (stype != SURFACE_PLANE)
					add_loops_from_face(fface, data, &loops, &local_loops, &processed_loops);
				}
			    }
			}
		    }
		}
	    }
	}
	key = face_set_key(faces);

	/* If we haven't seen this particular subset before, add it */
	if (subbrep_keys.find(key) == subbrep_keys.end()) {
	    subbrep_keys.insert(key);
	    struct subbrep_object_data *new_obj;
	    BU_GET(new_obj, struct subbrep_object_data);
	    subbrep_object_init(new_obj, data->brep);
	    bu_vls_sprintf(new_obj->key, "%s", key.c_str());
	    set_to_array(&(new_obj->faces), &(new_obj->faces_cnt), &faces);
	    set_to_array(&(new_obj->loops), &(new_obj->loops_cnt), &loops);
	    set_to_array(&(new_obj->edges), &(new_obj->edges_cnt), &edges);
	    new_obj->fol_cnt = 0;
	    new_obj->fil_cnt = 0;

	    new_obj->type = filters->type;

	    bu_ptbl_ins(data->children, (long *)new_obj);
	}
	filter_obj_free(filters);
	BU_PUT(filters, struct filter_obj);
    }
    if (subbrep_keys.size() == 0) {
	data->type = BREP;
	return 0;
    }
    data->type = COMB;
    return 1;
}

ON_Brep *
subbrep_make_brep(struct subbrep_object_data *data)
{
    ON_Brep *new_brep = ON_Brep::New();
    // For each edge in data, find the corresponding loop in data and construct
    // a new face in the brep with the surface from the original face and the
    // loop in data as the new outer loop.  Trim down the surface for the new
    // role.  Add the corresponding 3D edges and sync things up.

    // Each edge will map to two faces in the original Brep.  We only want the
    // subset of data that is part of this particular subobject - to do that,
    // we need to map elements from their indices in the old Brep to their
    // locations in the new.
    std::map<int, int> face_map;
    std::map<int, int> surface_map;
    std::map<int, int> edge_map;
    std::map<int, int> vertex_map;
    std::map<int, int> loop_map;
    std::map<int, int> c3_map;
    std::map<int, int> c2_map;
    std::map<int, int> trim_map;

    // Each edge has a trim array, and the trims will tell us which loops
    // are to be included and which faces the trims belong to.  There will
    // be some trims that belong to a face that is not included in the
    // subbrep face list, and those are rejected - those rejections indicate
    // a new face needs to be created to close the Brep.  The edges will drive
    // the population of new_brep initially - for each element pulled in by
    // the edge, it is either added or (if it's already in the map) references
    // are updated.

    for (int i = 0; i < data->edges_cnt; i++) {
	int c3i;
	ON_BrepEdge *old_edge = &(data->brep->m_E[i]);
	// Get vertices, trims, curves, and associated faces;
	if (!c3_map[old_edge->EdgeCurveIndexOf()]) {
	    ON_Curve *nc = old_edge->EdgeCurveOf()->Duplicate();
	    c3i = new_brep->AddEdgeCurve(nc);
	    c3_map[old_edge->EdgeCurveIndexOf()] = c3i;
	} else {
	    c3i = c3_map[old_edge->EdgeCurveIndexOf()];
	}
	int v0i, v1i;
	if (!vertex_map[old_edge->Vertex(0)->m_vertex_index]) {
	    ON_BrepVertex& newv0 = new_brep->NewVertex(old_edge->Vertex(0)->Point(), old_edge->Vertex(0)->m_tolerance);
	    v0i = newv0.m_vertex_index;
	} else {
	    v0i = vertex_map[old_edge->Vertex(0)->m_vertex_index];
	}
	if (!vertex_map[old_edge->Vertex(1)->m_vertex_index]) {
	    ON_BrepVertex& newv1 = new_brep->NewVertex(old_edge->Vertex(1)->Point(), old_edge->Vertex(0)->m_tolerance);
	    v1i = newv1.m_vertex_index;
	} else {
	    v1i = vertex_map[old_edge->Vertex(1)->m_vertex_index];
	}
	ON_BrepEdge& new_edge = new_brep->NewEdge(new_brep->m_V[v0i], new_brep->m_V[v1i], c3i, NULL ,0);
	edge_map[old_edge->m_edge_index] = new_edge.m_edge_index;
	// TODO - handle trims.  Need to create the face associated
	// with the trim if it doesn't already exist - copy the surface
	// if the face is in the subbrep, otherwise deduce it (how?)
	// create loop to hold trim if loop doesn't exist already,
	// add new trim to loop...
    }

    return new_brep;
}

int
subbreps_boolean_tree(struct bu_ptbl *subbreps)
{
    struct subbrep_object_data *top_union = NULL;
    /* The toplevel unioned object in the tree will be the one with no faces
     * that have only inner loops in the object loop network */
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++){
	struct subbrep_object_data *obj = (struct subbrep_object_data *)BU_PTBL_GET(subbreps, i);
	if (obj->fil_cnt == 0) {
	    top_union = obj;
	} else {
	    bu_log("Error - multiple objects appear to qualify as the first union object\n");
	    return 0;
	}
    }
    if (!top_union) {
	bu_log("Error - no object qualifies as the first union object\n");
	return 0;
    }
    /* Once the top level is identified, all other objects are parented to that object.
     * Technically they are not children of that object but of the toplevel comb */
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++){
	struct subbrep_object_data *obj = (struct subbrep_object_data *)BU_PTBL_GET(subbreps, i);
	if (obj != top_union) obj->parent = top_union;
    }

    /* For each child object, we need to ascertain whether the object is subtracted from the
     * top object or unioned to it. The general test for this is to raytrace the original BRep
     * through the child volume in question, and determine from the raytrace results whether
     * the volume adds to or takes away from the solidity along that shotline.  This is a
     * relatively expensive test, so if we have simpler shapes that let us do other tests
     * let's try those first. */

    /* Once we know whether the local shape is a subtraction or addition, we can decide for the
     * individual shapes in the combination whether they are subtractions or unions locally.
     * For example, if a cylinder is subtracted from the toplevel nmg, and a cone is
     * in turn subtracted from that cylinder (in other words, the cone shape contributes volume to the
     * final shape) the cone is subtracted from the local comb containing the cylinder and the cone, which
     * is in turn subtracted from the toplevel nmg.  Likewise, if the cylinder had been unioned to the nmg
     * to add volume and the cone had also added volume to the final shape (i.e. it's surface normals point
     * outward from the cone) then the code would be unioned with the cylinder in the local comb, and the
     * local comb would be unioned into the toplevel. */

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
