/*               O P E N N U R B S _ E X T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2009 United States Government as represented by
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
/** @file opennurbs_ext.cpp
 *
 * Implementation of routines openNURBS left out.
 *
 * Author -
 *   Jason Owens
 */

#include "common.h"

#include "bio.h"
#include <assert.h>
#include <vector>
#include <limits>

#include "tnt.h"
#include "jama_lu.h"

#include "vmath.h"
#include "bu.h"
#include "vector.h"
#include "opennurbs_ext.h"


#define RANGE_HI 0.55
#define RANGE_LO 0.45
#define UNIVERSAL_SAMPLE_COUNT 1001

#define BBOX_GROW 0.1


namespace brlcad {

	//--------------------------------------------------------------------------------
	// CurveTree
	CurveTree::CurveTree(ON_BrepFace* face)
	: m_face(face)
	{
		m_root = initialLoopBBox();
		
		for (int i = 0; i < face->LoopCount(); i++) {
			bool innerLoop = (i > 0) ? true : false;
			ON_BrepLoop* loop = face->Loop(i);
			// for each trim
			for (int j = 0; j < loop->m_ti.Count(); j++) {
				ON_BrepTrim& trim = face->Brep()->m_T[loop->m_ti[j]];
				const ON_Curve* trimCurve = trim.TrimCurveOf();
				double min,max;
				(void)trimCurve->GetDomain(&min, &max);
				ON_Interval t(min,max);
				
				if (false) { // (trimCurve->IsLinear()) {
					ON_3dPoint points[2];
					points[0] = trimCurve->PointAtStart();
					points[1] = trimCurve->PointAtEnd();
					point_t minpt, maxpt;
					VSETALL(minpt, MAX_FASTF);
					VSETALL(maxpt, -MAX_FASTF);
					for (int i = 0; i < 2; i++)
						VMINMAX(minpt,maxpt,((double*)points[i]));
					points[0]=ON_3dPoint(minpt);
					points[1]=ON_3dPoint(maxpt);
					ON_BoundingBox bb(points[0],points[1]);
					TRACE("linear no need to subdivide");
					m_root->addChild(curveBBox(trimCurve,t,true,innerLoop,bb));
				} else {
					TRACE("need to subdivide");
					// divide on param interval
#define KTANGENTBREAK
#ifdef KTANGENTBREAK
					if (!trimCurve->IsLinear()) {
					    int knotcnt = trimCurve->SpanCount();
					    double *knots = new double[knotcnt+1];

					    trimCurve->GetSpanVector(knots);
					    list<double> splitlist;
					    for(int i=1;i<=knotcnt;i++) {
						ON_Interval t(knots[i-1],knots[i]);

						getHVTangents(trimCurve,t,splitlist);
					    }
					    for( list<double>::iterator l=splitlist.begin();l != splitlist.end();l++) {
						double xmax = *l;
						if (!NEAR_ZERO(xmax-min,0.000001)) {
						    m_root->addChild(subdivideCurve(trimCurve,min,xmax,innerLoop,0));
						}
						min = xmax;
					    }
					}
#endif
					if (!NEAR_ZERO(max-min,0.000001)) {
						m_root->addChild(subdivideCurve(trimCurve,min,max,innerLoop,0));
					}
				}
				}
			}
		getLeaves(m_sortedX);
		m_sortedX.sort(sortX);
		getLeaves(m_sortedY);
		m_sortedY.sort(sortY);
		
		return;
	}
	
	CurveTree::~CurveTree() {
		delete m_root;
	}

	BRNode*
	CurveTree::getRootNode() const {
	return m_root;
	}

	int
	CurveTree::depth() {
	return m_root->depth();
	}

	ON_2dPoint
	CurveTree::getClosestPointEstimate(const ON_3dPoint& pt) {
	return m_root->getClosestPointEstimate(pt);
	}

	ON_2dPoint
	CurveTree::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v) {
	return m_root->getClosestPointEstimate(pt, u, v);
	}

	void
	CurveTree::getLeaves(list<BRNode*>& out_leaves) {
	m_root->getLeaves(out_leaves);
	}

	void
	CurveTree::getLeavesAbove(list<BRNode*>& out_leaves, const ON_Interval& u, const ON_Interval& v) {
	    point_t bmin,bmax;
	    double dist;
	    for (list<BRNode*>::iterator i = m_sortedX.begin(); i != m_sortedX.end(); i++) {
		SubcurveBRNode* br = dynamic_cast<SubcurveBRNode*>(*i);
		br->GetBBox(bmin,bmax);
		dist = 0.000001;//0.03*DIST_PT_PT(bmin,bmax);
		if (bmax[X]+dist < u[0])
		    continue;
		if (bmin[X]-dist < u[1]) {
		    if (bmax[Y]+dist > v[0]) {
			out_leaves.push_back(br);
		    }
		}
	    }
	}

	void
	CurveTree::getLeavesRight(list<BRNode*>& out_leaves, const ON_Interval& u, const ON_Interval& v) {
	    point_t bmin,bmax;
	    double dist;
	    for (list<BRNode*>::iterator i = m_sortedX.begin(); i != m_sortedX.end(); i++) {
		SubcurveBRNode* br = dynamic_cast<SubcurveBRNode*>(*i);
		br->GetBBox(bmin,bmax);
	        dist = 0.000001; //0.03*DIST_PT_PT(bmin,bmax);
		if (bmax[Y]+dist < v[0])
		    continue;
		if (bmin[Y]-dist < v[1]) {
		    if (bmax[X]+dist > u[0]) {
			out_leaves.push_back(br);
		    }
		}
	    }
	}


fastf_t
CurveTree::getVerticalTangent(const ON_Curve *curve,fastf_t min,fastf_t max) {
    fastf_t mid;
    ON_3dVector tangent;
    bool tanmin;

	// first lets check end points
    tangent = curve->TangentAt(max);
	if (NEAR_ZERO(tangent.x,0.00001) )
		return max;
    tangent = curve->TangentAt(min);
	if (NEAR_ZERO(tangent.x,0.00001) )
		return min;
		
    tanmin = (tangent[X] < 0.0);
    while ( (max-min) > 0.00001 ) {
	mid = (max + min)/2.0;
	tangent = curve->TangentAt(mid);
	if (NEAR_ZERO(tangent[X], 0.00001)) {
	    return mid;
	}
	if ( (tangent[X] < 0.0) == tanmin ) {
	    min = mid;
	} else {
	    max = mid;
	}
    }
    return min;
}

fastf_t
CurveTree::getHorizontalTangent(const ON_Curve *curve,fastf_t min,fastf_t max) {
    fastf_t mid;
    ON_3dVector tangent;
    bool tanmin;

	// first lets check end points
    tangent = curve->TangentAt(max);
	if (NEAR_ZERO(tangent.y,0.00001) )
		return max;
	tangent = curve->TangentAt(min);
	if (NEAR_ZERO(tangent.y,0.00001) )
		return min;
	
    tanmin = (tangent[Y] < 0.0);
    while ( (max-min) > 0.00001 ) {
		mid = (max + min)/2.0;
		tangent = curve->TangentAt(mid);
		if (NEAR_ZERO(tangent[Y], 0.00001)) {
			return mid;
		}
		if ( (tangent[Y] < 0.0) == tanmin ) {
			min = mid;
		} else {
			max = mid;
		}
    }
    return min;
}

bool
CurveTree::getHVTangents(const ON_Curve* curve, ON_Interval& t, list<fastf_t>& list) {
    bool tanx1,tanx2,tanx_changed;
    bool tany1,tany2,tany_changed;
    bool tan_changed;
    ON_3dVector tangent1,tangent2;
    ON_3dPoint p1,p2;

    tangent1 = curve->TangentAt(t[0]);
    tangent2 = curve->TangentAt(t[1]);
    
    tanx1 = (tangent1[X] < 0.0);
    tanx2 = (tangent2[X] < 0.0);
    tany1 = (tangent1[Y] < 0.0);
    tany2 = (tangent2[Y] < 0.0);

    tanx_changed =(tanx1 != tanx2);
    tany_changed =(tany1 != tany2);

    tan_changed = tanx_changed || tany_changed;
    
    if ( tan_changed ) {
	if (tanx_changed && tany_changed) {//horz & vert simply split
	    double midpoint = (t[1]+t[0])/2.0;
	    ON_Interval left(t[0],midpoint);
	    ON_Interval right(midpoint,t[1]);
	    getHVTangents(curve, left, list);
	    getHVTangents(curve, right, list);
	    return true;
	} else if (tanx_changed) {//find horz
	    double x = getVerticalTangent(curve,t[0],t[1]);
	    list.push_back(x);
	} else { //find vert
	    double x = getHorizontalTangent(curve,t[0],t[1]);
	    list.push_back(x);
	}
    } else { // check point slope for change
	bool slopex,slopex_changed;
	bool slopey,slopey_changed;
	bool slope_changed;
	fastf_t xdelta,ydelta;
	
	p1 = curve->PointAt(t[0]);
	p2 = curve->PointAt(t[1]);
	
	xdelta = (p2[X] - p1[X]);
	slopex = (xdelta < 0.0);
	ydelta = (p2[Y] - p1[Y]);
	slopey = (ydelta < 0.0);
	
	if ( NEAR_ZERO(xdelta, 0.000001) ||
			NEAR_ZERO(ydelta, 0.000001)) {
			return true;
			}
	
	slopex_changed = (slopex != tanx1);
	slopey_changed = (slopey != tany1);

	slope_changed = slopex_changed || slopey_changed;
			
	if (slope_changed) {  //2 horz or 2 vert changes simply split
	    double midpoint = (t[1]+t[0])/2.0;
	    ON_Interval left(t[0],midpoint);
	    ON_Interval right(midpoint,t[1]);
	    getHVTangents(curve, left, list);
	    getHVTangents(curve, right, list);
	    return true;
	}
    }
    return true;
}
	BRNode*
	CurveTree::curveBBox(const ON_Curve* curve, ON_Interval& t, bool isLeaf, bool innerTrim, const ON_BoundingBox& bb)
	{
	BRNode* node;
	fastf_t vdot = 1.0;
	
	if (isLeaf) {
		TRACE("creating leaf: u(" << u.Min() << "," << u.Max() <<
		  ") v(" << v.Min() << "," << v.Max() << ")");
		node = new SubcurveBRNode(curve,bb,m_face,t,vdot,innerTrim);
	}
	else
		node = new BRNode(bb);


	return node;

	}

	BRNode* CurveTree::initialLoopBBox()
	{
	double min[3];
	double max[3];
	ON_BoundingBox bb;
    for (int i = 0; i < m_face->LoopCount(); i++) {
		ON_BrepLoop* loop = m_face->Loop(i);
		if (loop->m_type == ON_BrepLoop::outer ) {
		 if (loop->GetBBox(bb[0],bb[1],FALSE)) {
			TRACE("BBox for Loop min<" << bb[0][0] << "," << bb[0][1] "," << bb[0][2] << ">" );
			TRACE("BBox for Loop max<" << bb[1][0] << "," << bb[1][1] "," << bb[1][2] << ">" );
		 }
		 break;
		}
	}
	BRNode* node = new BRNode(bb);
	return node;
	}

	BRNode*
	CurveTree::subdivideCurve(const ON_Curve* curve,double min,
				  double max, bool innerTrim,
				  int depth)
	{
		ON_Interval dom = curve->Domain();
		ON_3dPoint points[2];
		points[0] = curve->PointAt(min);
		points[1] = curve->PointAt(max);
        point_t minpt, maxpt;
		VSETALL(minpt, MAX_FASTF);
		VSETALL(maxpt, -MAX_FASTF);
		for (int i = 0; i < 2; i++)
			VMINMAX(minpt,maxpt,((double*)points[i]));
		points[0]=ON_3dPoint(minpt);
		points[1]=ON_3dPoint(maxpt);
		ON_BoundingBox bb(points[0],points[1]);
		
        ON_Interval t(min,max);
		if (isLinear(curve, min, max) || depth >= BREP_MAX_LN_DEPTH) {
			//	    ON_3dPoint mid = curve->PointAt((max + min)/2.0);
			double delta = (max - min)/(BREP_BB_CRV_PNT_CNT-1);
			point_t points[BREP_BB_CRV_PNT_CNT];
			ON_3dPoint pnt;
			for(int i=0;i<BREP_BB_CRV_PNT_CNT-1;i++){
				pnt = curve->PointAt(min + delta*i);
				VSET(points[i],pnt[0],pnt[1],pnt[2]);
			}
			pnt = curve->PointAt(max);
			VSET(points[BREP_BB_CRV_PNT_CNT-1],pnt[0],pnt[1],pnt[2]);
			
			point_t minpt, maxpt;
			VSETALL(minpt, MAX_FASTF);
			VSETALL(maxpt, -MAX_FASTF);
			for (int i = 0; i < BREP_BB_CRV_PNT_CNT; i++)
				VMINMAX(minpt,maxpt,((double*)points[i]));
			double dd = DIST_PT_PT(minpt,maxpt);
			
			//vect_t grow;
			//VSETALL(grow,0.0); // grow the box a bit
			//grow[0]= grow[1] = 0.03*dd; // grow the box a bit
			//VSUB2(minpt, minpt, grow);
			//VADD2(maxpt, maxpt, grow);
			VMOVE(pnt,minpt);
			bb.Set(pnt,false);
			VMOVE(pnt,maxpt);
			bb.Set(pnt,true);
			//bb = ON_PointListBoundingBox(3,0,BREP_BB_CRV_PNT_CNT,3,(double*)points);
			//for(int i=1; i<4; i++) {
			//    bb.Set(points[i],true); //grow bound to encompass midpoint
			//}
			return curveBBox(curve,t,true,innerTrim,bb);
		} else {
			//subdivide
			BRNode* parent = curveBBox(curve,t,false,innerTrim,bb);
			ON_Curve* left;
			ON_Curve* right;
			double mid = (max+min)/2.0;
			BRNode* l = subdivideCurve(curve,min,mid,innerTrim,depth+1);
			BRNode* r = subdivideCurve(curve,mid,max,innerTrim,depth+1);
			parent->addChild(l);
			parent->addChild(r);
			return parent;
		}
		
		return NULL;
	}

	/**
	 * Determine whether a given curve segment is linear 
     */

	bool
	CurveTree::isLinear(const ON_Curve* curve, double min, double max)
	{
		double mid = (max + min)/2.0;
		double qtr = (max - min)/4.0;
		ON_3dPoint  pmin = curve->PointAt(min);
		ON_3dPoint  pmax = curve->PointAt(max);
		
		const ON_Surface* surf = m_face->SurfaceOf();
		ON_Interval u = surf->Domain(0);
		ON_Interval v = surf->Domain(1);
		point_t a,b;
		VSET(a,u[0],v[0],0.0);
		VSET(b,u[1],v[1],0.0);
		double dd = DIST_PT_PT(a,b);
		double cd = DIST_PT_PT(pmin,pmax);
		
		if (cd > BREP_TRIM_SUB_FACTOR*dd )
			return false;
		
		double delta = (max - min)/(BREP_BB_CRV_PNT_CNT-1);
		ON_3dPoint points[BREP_BB_CRV_PNT_CNT];
		for(int i=0;i<BREP_BB_CRV_PNT_CNT-1;i++){
			points[i] = curve->PointAt(min + delta*i);
		}
		points[BREP_BB_CRV_PNT_CNT-1] = curve->PointAt(max);

		//double l2w = fabs((pmax.x - pmin.x)/(pmax.y - pmin.y));
		
		//	if ((l2w > 1.5) || (l2w < 1/.75))
		//	    return false;
		
		ON_3dVector A;
		ON_3dVector B;
		double vdot = 1.0;
		A = points[BREP_BB_CRV_PNT_CNT-1] - points[0];
		A.Unitize();
		for(int i=1;i<BREP_BB_CRV_PNT_CNT-1;i++){
			B = points[i] - points[0];
			B.Unitize();
			vdot = vdot * ( A * B );
			if ( vdot < BREP_CURVE_FLATNESS )
				return false; //already failed
		}
		
		//ON_3dVector tmin = curve->TangentAt(min);
		//ON_3dVector tmax = curve->TangentAt(max);
		//ON_3dVector tmid = curve->TangentAt(mid);
		
		//	if ( ((l2w > 1.5) || (l2w < 1/1.5)) &&
		//	     ( SIGN(tmin.y/tmin.x) != SIGN(tmax.y/tmax.x)))
		//	    return false;
		
		//double tangdot = (tmin * tmax) * (tmin * tmid) * (tmid * tmax);
		
		//return  (tangdot >= 0.9 ) && (vdot >= 0.9);
		return  (vdot >= BREP_CURVE_FLATNESS);
	}


    //--------------------------------------------------------------------------------
    // SurfaceTree
    SurfaceTree::SurfaceTree(ON_BrepFace* face)
	: m_face(face)
    {
	// build the surface bounding volume hierarchy
	const ON_Surface* surf = face->SurfaceOf();
	TRACE("Creating surface tree for: " << face->m_face_index);
	ON_Interval u = surf->Domain(0);
	ON_Interval v = surf->Domain(1);
	m_root = subdivideSurface(u, v, 0);
	TRACE("u: [" << u[0] << "," << u[1] << "]");
	TRACE("v: [" << v[0] << "," << v[1] << "]");
	TRACE("m_root: " << m_root);
    }

    SurfaceTree::~SurfaceTree() {
	delete m_root;
    }

    BBNode*
    SurfaceTree::getRootNode() const {
	return m_root;
    }

    int
    SurfaceTree::depth() {
	return m_root->depth();
    }

    ON_2dPoint
    SurfaceTree::getClosestPointEstimate(const ON_3dPoint& pt) {
	return m_root->getClosestPointEstimate(pt);
    }

    ON_2dPoint
    SurfaceTree::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v) {
	return m_root->getClosestPointEstimate(pt, u, v);
    }

    void
    SurfaceTree::getLeaves(list<BBNode*>& out_leaves) {
	m_root->getLeaves(out_leaves);
    }

    BBNode*
    SurfaceTree::surfaceBBox(bool isLeaf, const ON_Interval& u, const ON_Interval& v)
    {
	ON_3dPoint corners[9];
	const ON_Surface* surf = m_face->SurfaceOf();

	double uq = u.Length()*0.25;
	double vq = v.Length()*0.25;
	if (!surf->EvPoint(u.Min()-BBOX_GROW,v.Min()-BBOX_GROW,corners[0]) ||
	    !surf->EvPoint(u.Max()+BBOX_GROW,v.Min()-BBOX_GROW,corners[1]) ||
	    !surf->EvPoint(u.Max()+BBOX_GROW,v.Max()+BBOX_GROW,corners[2]) ||
	    !surf->EvPoint(u.Min()-BBOX_GROW,v.Max()+BBOX_GROW,corners[3]) ||

	    !surf->EvPoint(u.Mid(),v.Mid(),corners[4]) ||
	    !surf->EvPoint(u.Mid()-uq,v.Mid()-vq,corners[5]) ||
	    !surf->EvPoint(u.Mid()-uq,v.Mid()+vq,corners[6]) ||
	    !surf->EvPoint(u.Mid()+uq,v.Mid()-vq,corners[7]) ||
	    !surf->EvPoint(u.Mid()+uq,v.Mid()+vq,corners[8])) {
	    bu_bomb("Could not evaluate a point on surface"); // XXX fix this message
	}

	point_t min, max;
	VSETALL(min, MAX_FASTF);
	VSETALL(max, -MAX_FASTF);
	for (int i = 0; i < 9; i++)
	    VMINMAX(min,max,((double*)corners[i]));
//	vect_t grow;
//	VSETALL(grow,0.5); // grow the box a bit
//	VSUB2(min, min, grow);
//	VADD2(max, max, grow);

	// calculate the estimate point on the surface: i.e. use the point
	// on the surface defined by (u.Mid(), v.Mid()) as a heuristic for
	// finding the uv domain bounding a portion of the surface close
	// to an arbitrary model-space point (see
	// getClosestPointEstimate())
	ON_3dPoint estimate;
	if (!surf->EvPoint(u.Mid(),v.Mid(),estimate)) {
	    bu_bomb("Could not evaluate estimate point on surface");
	}
	BBNode* node;
	if (isLeaf) {
	    TRACE("creating leaf: u(" << u.Min() << "," << u.Max() <<
		  ") v(" << v.Min() << "," << v.Max() << ")");
	    node = new SubsurfaceBBNode(ON_BoundingBox(ON_3dPoint(min),
						       ON_3dPoint(max)),
					m_face,
					u, v);
	}
	else
	    node = new BBNode(ON_BoundingBox(ON_3dPoint(min),
					     ON_3dPoint(max)));
	node->m_estimate = estimate;
	return node;
    }

    BBNode* initialBBox(const ON_Surface* surf)
    {
	ON_BoundingBox bb = surf->BoundingBox();
	BBNode* node = new BBNode(bb);
	ON_3dPoint estimate;
	if (!surf->EvPoint(surf->Domain(0).Mid(),surf->Domain(1).Mid(),estimate)) {
	    bu_bomb("Could not evaluate estimate point on surface");
	}
	node->m_estimate = estimate;
	return node;
    }

    BBNode*
    SurfaceTree::subdivideSurface(const ON_Interval& u,
				  const ON_Interval& v,
				  int depth)
    {
	const ON_Surface* surf = m_face->SurfaceOf();
	ON_Interval usurf = surf->Domain(0);
	ON_Interval vsurf = surf->Domain(1);
	point_t a,b;
	VSET(a,usurf[0],vsurf[0],0.0);
	VSET(b,usurf[1],vsurf[1],0.0);
	double dsurf = DIST_PT_PT(a,b);
	VSET(a,u[0],v[0],0.0);
	VSET(b,u[1],v[1],0.0);
	double dsubsurf = DIST_PT_PT(a,b);

	if ( (dsubsurf/dsurf < BREP_SURF_SUB_FACTOR) && (isFlat(surf, u, v) || depth >= BREP_MAX_FT_DEPTH)) {
	    return surfaceBBox(true, u, v);
	} else {
	    BBNode* parent = (depth == 0) ? initialBBox(surf) : surfaceBBox(false, u, v);
	    BBNode* quads[4];
	    ON_Interval first(0,0.5);
	    ON_Interval second(0.5,1.0);
	    quads[0] = subdivideSurface(u.ParameterAt(first),  v.ParameterAt(first),  depth+1);
	    quads[1] = subdivideSurface(u.ParameterAt(second), v.ParameterAt(first),  depth+1);
	    quads[2] = subdivideSurface(u.ParameterAt(second), v.ParameterAt(second), depth+1);
	    quads[3] = subdivideSurface(u.ParameterAt(first),  v.ParameterAt(second), depth+1);

	    for (int i = 0; i < 4; i++)
		parent->addChild(quads[i]);

	    for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 3; j++) {
		    if (parent->m_node.m_min[j] > quads[i]->m_node.m_min[j]) {
    			parent->m_node.m_min[j] = quads[i]->m_node.m_min[j];
    		    }
    		    if (parent->m_node.m_max[j] < quads[i]->m_node.m_max[j]) {
    			parent->m_node.m_max[j] = quads[i]->m_node.m_max[j];
    		    }
		}
	    }

	    return parent;
	}
    }

#define NE 1
#define NW 2
#define SW 3
#define SE 4

    /**
     * Determine whether a given surface is flat enough, i.e. it falls
     * beneath our simple flatness constraints. The flatness constraint in
     * this case is a sampling of normals across the surface such that
     * the product of their combined dot products is close to 1.
     *
     * @f[ \prod_{i=1}^{7} n_i \dot n_{i+1} = 1 @f]
     *
     * Would be a perfectly flat surface. Generally something in the range
     * 0.8-0.9 should suffice (according to Abert, 2005).
     *
     *   +-------------------------+
     *	 |           	           |
     *	 |            +            |
     *	 |                         |
     *  V|       +         +       |
     *	 |                         |
     *	 |            +            |
     *	 |                         |
     *	 +-------------------------+
     *                  U
     *
     * The "+" indicates the normal sample.
     */

    bool
    SurfaceTree::isFlat(const ON_Surface* surf, const ON_Interval& u, const ON_Interval& v)
    {
	ON_3dVector normals[8];

	if (surf->IsAtSingularity(u.Min(), v.Min()) ||
	    surf->IsAtSingularity(u.Max(), v.Min()) ||
	    surf->IsAtSingularity(u.Max(), v.Max()) ||
	    surf->IsAtSingularity(u.Min(), v.Max())) {
	    TRACE("singularity! --------------------------");
	    TRACE("umin: " << u.Min());
	    TRACE("umax: " << u.Max());
	    TRACE("vmin: " << v.Min());
	    TRACE("vmax: " << v.Max());
	}

	// corners
	if (!surf->EvNormal(u.Min(),v.Min(),normals[0], SW) ||
	    !surf->EvNormal(u.Max(),v.Min(),normals[1], SE) ||
	    !surf->EvNormal(u.Max(),v.Max(),normals[2], NE) ||
	    !surf->EvNormal(u.Min(),v.Max(),normals[3], NW) ||

	    // interior
	    !surf->EvNormal(u.ParameterAt(0.5),v.ParameterAt(0.25),normals[4], SW) ||
	    !surf->EvNormal(u.ParameterAt(0.75),v.ParameterAt(0.5),normals[5], NE) ||
	    !surf->EvNormal(u.ParameterAt(0.5),v.ParameterAt(0.75),normals[6], NE) ||
	    !surf->EvNormal(u.ParameterAt(0.25),v.ParameterAt(0.5),normals[7], SW)) {
	    bu_bomb("Could not evaluate a normal on the surface"); // XXX fix this
	}

	double product = 1.0;

#ifdef DO_VECTOR
	double ax[4] VEC_ALIGN;
	double ay[4] VEC_ALIGN;
	double az[4] VEC_ALIGN;

	double bx[4] VEC_ALIGN;
	double by[4] VEC_ALIGN;
	double bz[4] VEC_ALIGN;

	distribute(4, normals, ax, ay, az);
	distribute(4, &normals[1], bx, by, bz);

	// how to get the normals in here?
	{
	    dvec<4> xa(ax);
	    dvec<4> ya(ay);
	    dvec<4> za(az);
	    dvec<4> xb(bx);
	    dvec<4> yb(by);
	    dvec<4> zb(bz);
	    dvec<4> dots = xa * xb + ya * yb + za * zb;
	    product *= dots.foldr(1,dvec<4>::mul());
	    if (product < 0.0) return false;
	}
	// try the next set of normals
	{
	    distribute(3, &normals[4], ax, ay, az);
	    distribute(3, &normals[5], bx, by, bz);
	    dvec<4> xa(ax);
	    dvec<4> xb(bx);
	    dvec<4> ya(ay);
	    dvec<4> yb(by);
	    dvec<4> za(az);
	    dvec<4> zb(bz);
	    dvec<4> dots = xa * xb + ya * yb + za * zb;
	    product *= dots.foldr(1,dvec<4>::mul(),3);
	}
#else
	for (int i = 0; i < 7; i++) {
	    product *= (normals[i] * normals[i+1]);
	}
#endif

	return product >= BREP_SURFACE_FLATNESS;
    }

    //--------------------------------------------------------------------------------
    // get_closest_point implementation

    typedef struct _gcp_data {
	const ON_Surface* surf;
	ON_3dPoint  pt;

	// for newton iteration
	ON_3dPoint  S;
	ON_3dVector du;
	ON_3dVector dv;
	ON_3dVector duu;
	ON_3dVector dvv;
	ON_3dVector duv;
    } GCPData;

    bool
    gcp_gradient(pt2d_t out_grad, GCPData& data, pt2d_t uv) {
	bool evaluated = data.surf->Ev2Der(uv[0],
					   uv[1],
					   data.S,
					   data.du,
					   data.dv,
					   data.duu,
					   data.duv,
					   data.dvv); // calc S(u,v) dS/du dS/dv d2S/du2 d2S/dv2 d2S/dudv
	if (!evaluated) return false;
	out_grad[0] = 2 * (data.du * (data.S - data.pt));
	out_grad[1] = 2 * (data.dv * (data.S - data.pt));
	return true;
    }

    bool
    gcp_newton_iteration(pt2d_t out_uv, GCPData& data, pt2d_t grad, pt2d_t in_uv) {
	ON_3dVector delta = data.S - data.pt;
	double g1du = 2 * (data.duu  * delta) + 2 * (data.du * data.du);
	double g2du = 2 * (data.duv  * delta) + 2 * (data.dv * data.du);
	double g1dv = g2du;
	double g2dv = 2 * (data.dvv  * delta) + 2 * (data.dv * data.dv);
	mat2d_t jacob = { g1du, g1dv,
			  g2du, g2dv };
	mat2d_t inv_jacob;
	if (mat2d_inverse(inv_jacob, jacob)) {
	    pt2d_t tmp;
	    mat2d_pt2d_mul(tmp, inv_jacob, grad);
	    pt2dsub(out_uv, in_uv, tmp);
	    return true;
	} else {
	    std::cerr << "inverse failed!" << std::endl; // XXX fix the error handling
	    return false;
	}
    }

    bool
    get_closest_point(ON_2dPoint& outpt,
		      ON_BrepFace* face,
		      const ON_3dPoint& point,
		      SurfaceTree* tree,
		      double tolerance) {

	int try_count = 0;
	bool delete_tree = false;
	bool found = false;
	double d_last = real.infinity();
	pt2d_t curr_grad;
	pt2d_t new_uv;
	GCPData data;
	data.surf = face->SurfaceOf();
	data.pt = point;

	TRACE("get_closest_point: " << PT(point));

	// get initial estimate
	SurfaceTree* a_tree = tree;
	if (a_tree == NULL) {
	    a_tree = new SurfaceTree(face);
	    delete_tree = true;
	}
	ON_Interval u, v;
	ON_2dPoint est = a_tree->getClosestPointEstimate(point, u, v);
	pt2d_t uv = { est[0], est[1] };

	// do the newton iterations
	// terminates on 1 of 3 conditions:
	// 1. if the gradient falls below an epsilon (preferred :-)
	// 2. if the gradient diverges
	// 3. iterated MAX_FCP_ITERATIONS
    try_again:
	int diverge_count = 0;
	for (int i = 0; i < BREP_MAX_FCP_ITERATIONS; i++) {
	    assert(gcp_gradient(curr_grad, data, uv));

	    ON_3dPoint p = data.surf->PointAt(uv[0],uv[1]);
	    double d = p.DistanceTo(point);
	    TRACE("dist: " << d);

	    if (NEAR_ZERO((d-d_last),BREP_FCP_ROOT_EPSILON)) {
		found = true; break;
	    } else if (d > d_last) {
		TRACE("diverged!");
		diverge_count++;
	    }
	    gcp_newton_iteration(new_uv, data, curr_grad, uv);
	    move(uv, new_uv);
	    d_last = d;
	}
	if (found) {
	    // check to see if we've left the surface domain
	    double l, h;
	    data.surf->GetDomain(0,&l,&h);
	    if (uv[0] < l) uv[0] = l; // clamp if out of range!
	    if (uv[0] > h) uv[0] = h;
	    data.surf->GetDomain(1,&l,&h);
	    if (uv[1] < l) uv[1] = l;
	    if (uv[1] > h) uv[1] = h;

	    outpt[0] = uv[0];
	    outpt[1] = uv[1];
	} else {
	    TRACE("FAILED TO FIND CLOSEST POINT!");
	    // XXX: try the mid point of the domain -- HACK! but it seems to work!?
	    if (try_count == 0) {
		uv[0] = u.Mid();
		uv[1] = v.Mid();
		++try_count;
		goto try_again;
	    }
	}

	if (delete_tree) delete a_tree;
	return found;
    }


    //--------------------------------------------------------------------------------
    // pullback_curve implementation
    typedef struct pbc_data {
	double tolerance;
	double flatness;
	const ON_Curve* curve;
	const ON_Surface* surf;
	ON_BrepFace* face;
	SurfaceTree* tree;
	ON_2dPointArray samples;
    } PBCData;

    typedef struct _bspline {
	int p; // degree
	int m; // num_knots-1
	int n; // num_samples-1 (aka number of control points)
	vector<double> params;
	vector<double> knots;
	ON_2dPointArray controls;
    } BSpline;

    bool
    isFlat(const ON_2dPoint& p1, const ON_2dPoint& m, const ON_2dPoint& p2, double flatness) {
	ON_Line line = ON_Line(ON_3dPoint(p1), ON_3dPoint(p2));
	return line.DistanceTo(ON_3dPoint(m)) <= flatness;
    }

    bool
    toUV(PBCData& data, ON_2dPoint& out_pt, double t) {
	ON_3dPoint pointOnCurve = data.curve->PointAt(t);
	return get_closest_point(out_pt, data.face, pointOnCurve, data.tree);
    }

    double
    randomPointFromRange(PBCData& data, ON_2dPoint& out, double lo, double hi)
    {
	assert(lo < hi);

#ifdef HAVE_DRAND48
	double random_pos = drand48() * (RANGE_HI - RANGE_LO) + RANGE_LO;
#else
	double random_pos = rand() * (RANGE_HI - RANGE_LO) / (RAND_MAX + 1.) + RANGE_LO;
#endif

	double newt = random_pos * (hi - lo) + lo;
	assert(newt >= lo && newt <= hi);
	assert(toUV(data, out, newt));
	return newt;
    }

    bool
    sample(PBCData& data,
	   double t1,
	   double t2,
	   const ON_2dPoint& p1,
	   const ON_2dPoint& p2)
    {
	ON_2dPoint m;
	double t = randomPointFromRange(data, m, t1, t2);
	if (isFlat(p1, m, p2, data.flatness)) {
	    data.samples.Append(p2);
	} else {
	    sample(data, t1, t, p1, m);
	    sample(data, t, t2, m, p2);
	}
	return true;
    }

    // this is uniform knot generation, not recommended for use with
    // chord-length parameter method... but we're not using
    // that. considering using the average method described at
    // http://www.cs.mtu.edu/~shene/COURSES/cs3621/NOTES/
    void
    generateKnots(BSpline& bspline) {
	int num_knots = bspline.m + 1;
	bspline.knots.resize(num_knots);
	for (int i = 0; i <= bspline.p; i++) {
	    bspline.knots[i] = 0.0;
	    TRACE("knot: " << bspline.knots[i]);
	}
	for (int i = 1; i <= bspline.n-bspline.p; i++) {
	    bspline.knots[bspline.p+i] = ((double)i) / (bspline.n-bspline.p+1.0);
	    TRACE("knot: " << bspline.knots[bspline.p+i]);
	}
	for (int i = bspline.m-bspline.p; i <= bspline.m; i++) {
	    bspline.knots[i] = 1.0;
	    TRACE("knot: " << bspline.knots[i]);
	}
	TRACE("knot size: " << bspline.knots.size());
    }

    int
    getKnotInterval(BSpline& bspline, double u) {
	int k = 0;
	while (u >= bspline.knots[k]) k++;
	k = (k == 0) ? k : k-1;
	return k;
    }

    int
    getCoefficients(BSpline& bspline, Array1D<double>& N, double u) {
	// evaluate the b-spline basis function for the given parameter u
	// place the results in N[]
	N = 0.0;
	if (u == bspline.knots[0]) {
	    N[0] = 1.0;
	    return 0;
	} else if (u == bspline.knots[bspline.m]) {
	    N[bspline.n] = 1.0;
	    return bspline.n;
	}
	int k = getKnotInterval(bspline, u);
	N[k] = 1.0;
	for (int d = 1; d <= bspline.p; d++) {
	    double uk_1 = bspline.knots[k+1];
	    double uk_d_1 = bspline.knots[k-d+1];
	    N[k-d] = ((uk_1 - u)/(uk_1 - uk_d_1)) * N[k-d+1];
	    for (int i = k-d+1; i <= k-1; i++) {
		double ui = bspline.knots[i];
		double ui_1 = bspline.knots[i+1];
		double ui_d = bspline.knots[i+d];
		double ui_d_1 = bspline.knots[i+d+1];
		N[i] = ((u - ui)/(ui_d - ui)) * N[i] + ((ui_d_1 - u)/(ui_d_1 - ui_1))*N[i+1];
	    }
	    double uk = bspline.knots[k];
	    double uk_d = bspline.knots[k+d];
	    N[k] = ((u - uk)/(uk_d - uk)) * N[k];
	}
	return k;
    }

    // XXX: this function sucks...
    void
    generateParameters(BSpline& bspline) {
	TRACE("generateParameters");
	double lastT = 0.0;
	bspline.params.resize(bspline.n+1);
	Array2D<double> N(UNIVERSAL_SAMPLE_COUNT, bspline.n+1);
	for (int i = 0; i < UNIVERSAL_SAMPLE_COUNT; i++) {
	    double t = (double)i / (UNIVERSAL_SAMPLE_COUNT-1);
	    Array1D<double> n = Array1D<double>(N.dim2(), N[i]);
	    getCoefficients(bspline, n, t);
	}
	for (int i = 0; i < bspline.n+1; i++) {
	    double max = -real.max();
	    for (int j = 0; j < UNIVERSAL_SAMPLE_COUNT; j++) {
		double f = N[j][i];
		double t = ((double)j)/(UNIVERSAL_SAMPLE_COUNT-1);
		if (f > max) {
		    max = f;
		    if (j == UNIVERSAL_SAMPLE_COUNT-1) bspline.params[i] = t;
		} else if (f < max) {
		    bspline.params[i] = lastT;
		    break;
		}
		lastT = t;
	    }
	}
	for (int i = 0; i < bspline.n+1; i++) {
	}
    }

    void
    printMatrix(Array2D<double>& m) {
	printf("---\n");
	for (int i = 0; i < m.dim1(); i++) {
	    for (int j = 0; j < m.dim2(); j++) {
		printf("% 5.5f ", m[i][j]);
	    }
	    printf("\n");
	}
    }

    void
    generateControlPoints(BSpline& bspline, PBCData& data)
    {
	Array2D<double> bigN(bspline.n+1, bspline.n+1);
	for (int i = 0; i < bspline.n+1; i++) {
	    Array1D<double> n = Array1D<double>(bigN.dim2(), bigN[i]);
	    getCoefficients(bspline, n, bspline.params[i]);
	}
	Array2D<double> bigD(bspline.n+1,2);
	for (int i = 0; i < bspline.n+1; i++) {
	    bigD[i][0] = data.samples[i].x;
	    bigD[i][1] = data.samples[i].y;
	}

	printMatrix(bigD);
	printMatrix(bigN);

	JAMA::LU<double> lu(bigN);
	assert(lu.isNonsingular() > 0);
	Array2D<double> bigP = lu.solve(bigD); // big linear algebra black box here...

	// extract the control points
	for (int i = 0; i < bspline.n+1; i++) {
	    ON_2dPoint& p = bspline.controls.AppendNew();
	    p.x = bigP[i][0];
	    p.y = bigP[i][1];
	}
    }

    ON_NurbsCurve*
    newNURBSCurve(BSpline& spline) {
	TRACE("newNURBSCurve!");

	TRACE("n: " << spline.n);
	TRACE("p: " << spline.p);

	// we now have everything to complete our spline
	ON_NurbsCurve* c = ON_NurbsCurve::New(2,
					      false,
					      spline.p+1,
					      spline.n+1);

	// truly - i don't know WTF openNURBS is doing here
	// when it prints out the knots, they only have multiplicity 3,
	// but yet the order of the curve is 4!!!
	int num_knots = spline.knots.size() - 2;
	for (int i = 0; i < num_knots; i++) {
	    double knot = spline.knots[i+1];
	    TRACE("knot: " << knot);
	    c->SetKnot(i,knot);
	}
	//c->ClampEnd(2);

	for (int i = 0; i < spline.controls.Count(); i++) {
	    c->SetCV(i, ON_3dPoint(spline.controls[i]));
	}

	return c;
    }

    ON_Curve*
    interpolateCurve(PBCData& data) {
	ON_Curve* curve;
	if (data.samples.Count() == 2) {
	    // build a line
	    curve = new ON_LineCurve(data.samples[0], data.samples[1]);
	    curve->SetDomain(0.0,1.0);
	} else {
	    // build a NURBS curve, then see if it can be simplified!
	    BSpline spline;
	    spline.p = 3;
	    spline.n = data.samples.Count()-1;
	    spline.m = spline.n + spline.p + 1;
	    generateKnots(spline);
	    generateParameters(spline);
	    generateControlPoints(spline, data);
	    assert(spline.controls.Count() >= 4);
	    curve = newNURBSCurve(spline);
	    // XXX - attempt to simplify here!
	}
	ON_TextLog tl;
	TRACE("************** interpolateCurve");
	curve->Dump(tl);
	assert(curve->IsValid(&tl));
	return curve;
    }

    ON_Curve*
    pullback_curve(ON_BrepFace* face,
		   const ON_Curve* curve,
		   SurfaceTree* tree,
		   double tolerance,
		   double flatness) {
	PBCData data;
	data.tolerance = tolerance;
	double len;
	curve->GetLength(&len);
	data.flatness = (len < 1.0) ? flatness : flatness * len;

	data.curve = curve;
	data.face = face;
	data.surf = face->SurfaceOf();
	data.tree = tree;

	double u[2], v[2];
	data.surf->GetDomain(0, &u[0], &u[1]);
	data.surf->GetDomain(1, &v[0], &v[1]);
	TRACE("pullback_curve: " << PT2(u) << " | " << PT2(v));
	TRACE("pullback_curve: ");
	ON_TextLog tl;
	curve->Dump(tl);
	data.surf->Dump(tl);

	// Step 1 - adaptively sample the curve
	double tmin, tmax;
	data.curve->GetDomain(&tmin, &tmax);
	ON_2dPoint& start = data.samples.AppendNew(); // new point is added to samples and returned
	assert(toUV(data,start,tmin));

	ON_2dPoint p1, p2;
	toUV(data, p1, tmin);
	toUV(data, p2, tmax);
	assert(sample(data, tmin, tmax, p1, p2));

	// step 2 - interpolate the samples
	return interpolateCurve(data);
    }
    bool sortX (BRNode* first, BRNode* second)
    {
	point_t first_min,second_min;
	point_t first_max,second_max;

	first->GetBBox(first_min,first_max);
	second->GetBBox(second_min,second_max);

	if ( first_min[X] < second_min[X] )
	    return true;
	else 
	    return false;
    }
    bool sortY (BRNode* first, BRNode* second)
    {
	point_t first_min,second_min;
	point_t first_max,second_max;

	first->GetBBox(first_min,first_max);
	second->GetBBox(second_min,second_max);

	if ( first_min[Y] < second_min[Y] )
	    return true;
	else 
	    return false;
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
