#include "common.h"

#include <set>
#include <map>
#include <limits>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

#define L3_OFFSET 6


int
cone_validate_face(const ON_BrepFace *forig, const ON_BrepFace *fcand)
{
    ON_Cone corig;
    ON_Surface *csorig = forig->SurfaceOf()->Duplicate();
    csorig->IsCone(&corig, BREP_CONIC_TOL);
    delete csorig;
    ON_Line lorig(corig.BasePoint(), corig.ApexPoint());

    ON_Cone ccand;
    ON_Surface *cscand = fcand->SurfaceOf()->Duplicate();
    cscand->IsCone(&ccand, BREP_CONIC_TOL);
    delete cscand;
    double d1 = lorig.DistanceTo(ccand.BasePoint());
    double d2 = lorig.DistanceTo(ccand.ApexPoint());

    // Make sure the cone axes are colinear
    if (corig.Axis().IsParallelTo(ccand.Axis(), VUNITIZE_TOL) == 0) return 0;
    if (fabs(d1) > BREP_CONIC_TOL) return 0;
    if (fabs(d2) > BREP_CONIC_TOL) return 0;

    // Make sure the AngleInRadians parameter is the same for both cones
    if (!NEAR_ZERO(corig.AngleInRadians() - ccand.AngleInRadians(), VUNITIZE_TOL)) return 0;

    // Make sure the ApexPoint parameter is the same for both cones
    if (corig.ApexPoint().DistanceTo(ccand.ApexPoint()) > BREP_CONIC_TOL) return 0;

    // TODO - make sure negative/positive status for both cyl faces is the same.

    return 1;
}



/* Return -1 if the cone face is pointing in toward the axis,
 * 1 if it is pointing out, and 0 if there is some other problem */
int
negative_cone(const ON_Brep *brep, int face_index, double cone_tol) {
    int ret = 0;
    const ON_Surface *surf = brep->m_F[face_index].SurfaceOf();
    ON_Cone cone;
    ON_Surface *cs = surf->Duplicate();
    cs->IsCone(&cone, cone_tol);
    delete cs;

    ON_3dPoint pnt;
    ON_3dVector normal;
    double u = surf->Domain(0).Mid();
    double v = surf->Domain(1).Mid();
    if (!surf->EvNormal(u, v, pnt, normal)) return 0;
    ON_3dPoint base_pnt = cone.BasePoint();

    ON_3dVector base_vect = pnt - base_pnt;
    double dotp = ON_DotProduct(base_vect, normal);

    if (NEAR_ZERO(dotp, VUNITIZE_TOL)) return 0;
    ret = (dotp < 0) ? -1 : 1;
    if (brep->m_F[face_index].m_bRev) ret = -1 * ret;
    return ret;
}


int
cone_implicit_plane(const ON_Brep *brep, int lc, int *le, ON_SimpleArray<ON_Plane> *cone_planes)
{
    return cyl_implicit_plane(brep, lc, le, cone_planes);
}


int
cone_implicit_params(struct subbrep_shoal_data *data, ON_SimpleArray<ON_Plane> *cone_planes, int implicit_plane_ind, int ndc, int *nde, int shoal_nonplanar_face, int UNUSED(nonlinear_edge))
{

    const ON_Brep *brep = data->i->brep;
    std::set<int> nondegen_edges;
    std::set<int>::iterator c_it;
    array_to_set(&nondegen_edges, nde, ndc);

    // Make a starting cone from one of the cylindrical surfaces and construct the axis line
    ON_Cone cone;
    ON_Surface *cs = brep->m_L[data->shoal_loops[0]].Face()->SurfaceOf()->Duplicate();
    cs->IsCone(&cone, BREP_CONIC_TOL);
    delete cs;
    ON_Line l(cone.BasePoint(), cone.ApexPoint());

    int need_arbn = 1;
    if ((*cone_planes).Count() <= 2) {
	int perpendicular = 0;
	for (int i = 0; i < 2; i++) {
	    ON_Plane p;
	    if ((*cone_planes)[i].Normal().IsParallelTo(cone.Axis(), VUNITIZE_TOL) == 0) perpendicular++;
	}
	if (perpendicular == (*cone_planes).Count()) need_arbn = 0;
    }

    // We'll be using a truncated cone to represent this, and we don't want to make it any
    // larger than we have to.  Find all the limit points, starting with the apex point and
    // adding in the planes and edges.
    ON_3dPointArray axis_pts_init;

    // It may be trimmed away by planes, but not in the simplest case.  Add in apex point.
    axis_pts_init.Append(cone.ApexPoint());

    // Add in all the nondegenerate edge vertices
    for (c_it = nondegen_edges.begin(); c_it != nondegen_edges.end(); c_it++) {
        const ON_BrepEdge *edge = &(brep->m_E[*c_it]);
        axis_pts_init.Append(edge->Vertex(0)->Point());
        axis_pts_init.Append(edge->Vertex(1)->Point());
    }

    // Use the intersection, cone angle, and the projection of the cone axis onto the plane
    // to calculate min and max points on the cone from this plane.
    for (int i = 0; i < (*cone_planes).Count(); i++) {
        // Note - don't intersect the implicit plane, since it doesn't play a role in defining the main cone
        if (!(*cone_planes)[i].Normal().IsPerpendicularTo(cone.Axis(), VUNITIZE_TOL) && i != implicit_plane_ind) {
            ON_3dPoint ipoint = ON_LinePlaneIntersect(l, (*cone_planes)[i]);
            if ((*cone_planes)[i].Normal().IsParallelTo(cone.Axis(), VUNITIZE_TOL)) {
                axis_pts_init.Append(ipoint);
            } else {

		ON_3dVector av = ipoint - cone.ApexPoint();
		double side = av.Length();
		av.Unitize();
                double dpc = ON_DotProduct(av, (*cone_planes)[i].Normal());
		double angle_plane_axis = (dpc < 0) ? M_PI/2 - acos(dpc) : acos(dpc);

		double C = M_PI/2 - (M_PI/4 - angle_plane_axis) - cone.AngleInRadians();
		double a = side * sin(cone.AngleInRadians()) / sin(C);
		C = M_PI/2 - (M_PI/4 + angle_plane_axis) - cone.AngleInRadians();
		double b = side * sin(cone.AngleInRadians()) / sin(C);

		ON_3dVector avect, bvect;
                ON_3dVector pvect = cone.Axis() - ((*cone_planes)[i].Normal() * dpc);
                pvect.Unitize();
		double abdp = ON_DotProduct(pvect, av);
		avect = (abdp < 0) ? pvect : -1 * pvect;
		bvect = (abdp > 0) ? pvect : -1 * pvect;

                if (!NEAR_ZERO(dpc, VUNITIZE_TOL)) {
                    ON_3dPoint p1 = ipoint + avect * a;
		    bu_log("in p1.s sph %f %f %f 1\n", p1.x, p1.y, p1.z);
                    ON_3dPoint p2 = ipoint + bvect * b;
		    bu_log("in p2.s sph %f %f %f 1\n", p2.x, p2.y, p2.z);
                    axis_pts_init.Append(p1);
                    axis_pts_init.Append(p2);
                }
            }
        }
    }

    // Trim out points that are above the bounding planes
    ON_3dPointArray axis_pts_2nd;
    for (int i = 0; i < axis_pts_init.Count(); i++) {
        int trimmed = 0;
        for (int j = 0; j < (*cone_planes).Count(); j++) {
            // Don't trim with the implicit plane - the implicit plane is defined "after" the
            // primary shapes are formed, so it doesn't get a vote in removing capping points
            if (j != implicit_plane_ind) {
                ON_3dVector v = axis_pts_init[i] - (*cone_planes)[j].origin;
                double dotp = ON_DotProduct(v, (*cone_planes)[j].Normal());
                if (dotp > 0 && !NEAR_ZERO(dotp, VUNITIZE_TOL)) {
                    trimmed = 1;
                    break;
                }
            }
        }
        if (!trimmed) axis_pts_2nd.Append(axis_pts_init[i]);
    }

    // For everything that's left, project it back onto the central axis line and see
    // if it's further up or down the line than anything previously checked.  We want
    // the min and the max points on the centeral axis.
    double tmin = std::numeric_limits<double>::max();
    double tmax = std::numeric_limits<double>::min();
    for (int i = 0; i < axis_pts_2nd.Count(); i++) {
        double t;
        if (l.ClosestPointTo(axis_pts_2nd[i], &t)) {
            if (t < tmin) tmin = t;
            if (t > tmax) tmax = t;
        }
    }

    // Populate the CSG implicit primitive data
    data->params->csg_type = CONE;
    // Flag the cone according to the negative or positive status of the
    // cone surface.
    data->params->negative = negative_cone(brep, shoal_nonplanar_face, BREP_CONIC_TOL);
    data->params->bool_op = (data->params->negative == -1) ? '-' : 'u';

    if (l.PointAt(tmin).DistanceTo(cone.ApexPoint()) > BREP_CONIC_TOL) {
	fastf_t hdelta = cone.BasePoint().DistanceTo(l.PointAt(tmin));
	hdelta = (cone.height < 0) ? -1*hdelta : hdelta;
	data->params->radius = cone.CircleAt(cone.height - hdelta).Radius();
	BN_VMOVE(data->params->origin, l.PointAt(tmin));
    }

    if (l.PointAt(tmax).DistanceTo(cone.ApexPoint()) > BREP_CONIC_TOL) {
	fastf_t hdelta = cone.BasePoint().DistanceTo(l.PointAt(tmax));
	hdelta = (cone.height < 0) ? -1*hdelta : hdelta;
	data->params->r2 = cone.CircleAt(cone.height - hdelta).Radius();
    } else {
	data->params->r2 = 0.000001;
    }

    ON_3dVector hvect(l.PointAt(tmax) - l.PointAt(tmin));
    BN_VMOVE(data->params->hv, hvect);
    data->params->height = hvect.Length();

    return need_arbn;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
