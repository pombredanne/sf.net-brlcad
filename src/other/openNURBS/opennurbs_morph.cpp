/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2007 Robert McNeel & Associates. All rights reserved.
// Rhinoceros is a registered trademark of Robert McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#include "opennurbs.h"


ON_Localizer::ON_Localizer()
{
  m_nurbs_curve = 0;
  m_nurbs_surface = 0;
  Destroy();
}

ON_Localizer::~ON_Localizer()
{
  Destroy();
}

ON_Localizer::ON_Localizer(const ON_Localizer& src)
{
  m_nurbs_curve = 0;
  m_nurbs_surface = 0;
  Destroy();
  *this = src;
}

ON_Localizer& ON_Localizer::operator=(const ON_Localizer& src)
{
  if ( this != &src )
  {
    Destroy();
    m_type = src.m_type;
    m_d = src.m_d;
    m_P = src.m_P;
    m_V = src.m_V;
    if ( src.m_nurbs_curve )
      m_nurbs_curve = src.m_nurbs_curve->Duplicate();
    if ( src.m_nurbs_surface )
      m_nurbs_surface = src.m_nurbs_surface->Duplicate();
  }
  return *this;
}


void ON_Localizer::Destroy()
{
  m_type = no_type;
  m_P.Set(0.0,0.0,0.0);
  m_V.Set(0.0,0.0,0.0);
  m_d.Set(0.0,0.0);
  if (m_nurbs_curve)
  {
    delete m_nurbs_curve;
    m_nurbs_curve = 0;
  }
  if (m_nurbs_surface)
  {
    delete m_nurbs_surface;
    m_nurbs_surface = 0;
  }
}

bool ON_Localizer::Write(ON_BinaryArchive& archive) const
{
  bool rc = archive.BeginWrite3dmChunk(TCODE_ANONYMOUS_CHUNK,1,0);
  if (!rc)
    return false;

  for(;;)
  {
    rc = archive.WriteInt(m_type);
    if ( !rc ) break;
    rc = archive.WritePoint(m_P);
    if ( !rc ) break;
    rc = archive.WriteVector(m_V);
    if ( !rc ) break;
    rc = archive.WriteInterval(m_d);
    if ( !rc ) break;

    rc = archive.BeginWrite3dmChunk(TCODE_ANONYMOUS_CHUNK,1,0);
    if (!rc) break;
    rc = archive.WriteBool( m_nurbs_curve ? true : false );
    if ( rc && m_nurbs_curve )
      rc = m_nurbs_curve->Write(archive)?true:false;
    if ( !archive.EndWrite3dmChunk() )
      rc = false;
    if (!rc) break;

    rc = archive.BeginWrite3dmChunk(TCODE_ANONYMOUS_CHUNK,1,0);
    if (!rc) break;
    rc = archive.WriteBool( m_nurbs_surface ? true : false );
    if ( rc && m_nurbs_surface )
      rc = m_nurbs_surface->Write(archive)?true:false;
    if ( !archive.EndWrite3dmChunk() )
      rc = false;
    if (!rc) break;

    break;
  }
  
  if ( !archive.EndWrite3dmChunk() )
    rc = false;

  return rc;
}

bool ON_Localizer::Read(ON_BinaryArchive& archive)
{
  Destroy();

  int major_version = 0;
  int minor_version = 0;
  bool rc = archive.BeginRead3dmChunk(TCODE_ANONYMOUS_CHUNK,&major_version,&minor_version);
  if (!rc)
    return false;

  for(;;)
  {
    rc = (1 == major_version);
    if ( !rc ) break;

    int i = no_type;
    rc = archive.ReadInt(&i);
    if ( !rc ) break;

    switch(i)
    {
    case sphere_type:   m_type = sphere_type;   break;
    case plane_type:    m_type = plane_type;    break;
    case cylinder_type: m_type = cylinder_type; break;
    case curve_type:    m_type = curve_type;    break;
    case surface_type:  m_type = surface_type;  break;
    case distance_type: m_type = distance_type; break;
    }

    rc = archive.ReadPoint(m_P);
    if ( !rc ) break;
    rc = archive.ReadVector(m_V);
    if ( !rc ) break;
    rc = archive.ReadInterval(m_d);
    if ( !rc ) break;

    int mjv = 0, mnv = 0;
    rc = archive.BeginRead3dmChunk(TCODE_ANONYMOUS_CHUNK,&mjv,&mnv);
    if (!rc) break;
    rc = (1 == mjv);
    bool bReadCurve = false;
    if (rc)
      rc = archive.ReadBool( &bReadCurve );
    if ( rc && bReadCurve)
    {
      m_nurbs_curve = new ON_NurbsCurve();
      rc = m_nurbs_curve->Read(archive)?true:false;
    }
    if ( !archive.EndRead3dmChunk() )
      rc = false;
    if (!rc) break;

    rc = archive.BeginRead3dmChunk(TCODE_ANONYMOUS_CHUNK,&mjv,&mnv);
    if (!rc) break;
    rc = (1 == mjv);
    bool bReadSurface = false;
    rc = archive.ReadBool( &bReadSurface );
    if ( rc && bReadSurface )
    {
      m_nurbs_surface = new ON_NurbsSurface();
      rc = m_nurbs_surface->Read(archive)?true:false;
    }
    if ( !archive.EndRead3dmChunk() )
      rc = false;
    if (!rc) break;

    break;
  }
  
  if ( !archive.EndRead3dmChunk() )
    rc = false;

  return rc;
}


bool ON_Localizer::CreateCylinderLocalizer( ON_3dPoint P, ON_3dVector V, double r0, double r1 )
{
  Destroy();
  if (    P.IsValid() 
       && V.IsValid() 
       && V.Length() > 0.0 
       && ON_IsValid(r0) 
       && ON_IsValid(r1) 
       && r0 > 0.0
       && r1 > 0.0
       && r0 != r1 )
  {
    m_P = P;
    m_V = V;
    m_V.Unitize();
    m_d.Set(r0,r1);
    m_type = cylinder_type;
  }
  return (cylinder_type == m_type);
}

bool ON_Localizer::CreatePlaneLocalizer( ON_3dPoint P, ON_3dVector N, double h0, double h1 )
{
  Destroy();
  if ( P.IsValid()
       && N.IsValid()
       && N.Length() > 0.0
       && ON_IsValid(h0)
       && ON_IsValid(h1)
       && h0 != h1 )
  {
    m_V = N;
    m_V.Unitize();
    m_P.Set( -(m_V.x*P.x + m_V.y*P.y + m_V.z*P.z), 0.0, 0.0 );
    m_d.Set(h0,h1);
    m_type = plane_type;
  }
  return (plane_type == m_type);
}

bool ON_Localizer::CreateSphereLocalizer( ON_3dPoint P, double r0, double r1 )
{
  Destroy();
  if ( P.IsValid()
       && ON_IsValid(r0)
       && ON_IsValid(r1)
       && r0 > 0.0
       && r1 > 0.0
       && r0 != r1 )
  {
    m_P = P;
    m_V.Zero();
    m_d.Set(r0,r1);
    m_type = sphere_type;
  }
  return (sphere_type == m_type);
}

double ON_Localizer::Value(double t) const
{
  double s = m_d.NormalizedParameterAt(t);
  if ( s <= 0.0 )
    s = 0.0;
  else if ( s >= 1.0 )
    s = 1.0;
  else
    s = s*s*(3.0 - 2.0*s);

  return s;
}

double ON_Localizer::Value(ON_3dPoint P) const
{
  double v,s,t = m_d.m_t[1];

  switch ( m_type )
  {
  case cylinder_type:
    // t = distance from P to axis
    t = ON_CrossProduct( P-m_P, m_V ).Length();
    break;

  case plane_type:
    // t = distance above plane
    t = m_V.x*P.x + m_V.y*P.y + m_V.z*P.z + m_P.x;
    break;

  case sphere_type:
    // t = distance to P
    t = (P-m_P).Length();
    break;

  case curve_type:
    if ( !m_nurbs_curve )
      return 1.0;
    if ( !m_nurbs_curve->GetClosestPoint(P,&v) )
      return 1.0;
    t = P.DistanceTo(m_nurbs_curve->PointAt(v));
    break;

  case surface_type:
    if ( !m_nurbs_surface )
      return 1.0;
    if ( !m_nurbs_surface->GetClosestPoint(P,&s,&v) )
      return 1.0;
    t = P.DistanceTo(m_nurbs_surface->PointAt(s,v));
    break;

  case distance_type:
    // confused user should be calling Value(double)
    return 1.0; // default must be one
    break;

  default:
    return 1.0; // default must be one
  }

  return Value(t);
}


bool ON_Localizer::IsZero( const ON_BoundingBox& bbox ) const
{
  bool rc = false;

  ON_BoundingBox loc_bbox;
  bool bTestLocBox = false;
  double d;

  switch ( m_type )
  {
  case cylinder_type:
    {
      ON_3dPointArray corners;
      bbox.GetCorners(corners);
      int i;
      double t0, t1;
      t0 = t1 = (corners[0]-m_P)*m_V;
      for ( i = 1; i < 8; i++ )
      {
	d = (corners[i]-m_P)*m_V;
	if ( d < t0 )
	  t0 = d;
	else if (d > t1 )
	  t1 = d;
      }
      ON_Line L(m_P+t0*m_V,m_P+t1*m_V);
      if ( m_d[0] > m_d[1] )
      {
	// function is supported along the line
	d = bbox.MinimumDistanceTo(L);
	if ( d >= m_d[0] )
	  rc = true;
      }
      else
      {
	// function is supported outside cylinder
	d = bbox.MaximumDistanceTo(L);
	if ( d <= m_d[0] )
	  rc = true;
      }
    }
    break;

  case plane_type:
    {
      ON_PlaneEquation e;
      e.x = m_V.x; e.y = m_V.y; e.z = m_V.z; e.d = m_P.x;
      e.d -= m_d[0];
      if ( m_d[0] > m_d[1] )
      {
	e.x = -e.x; e.y = -e.y; e.z = -e.z; e.d = -e.d;
      }
      if ( e.MaximumValueAt(bbox) <= 0.0 )
	rc = true;
    }
    break;

  case sphere_type:
    loc_bbox.m_min = m_P;
    loc_bbox.m_max = m_P;
    bTestLocBox = true;
    break;

  case curve_type:
    if ( m_nurbs_curve)
    {
      loc_bbox = m_nurbs_curve->BoundingBox();
      bTestLocBox = true;
    }
    break;

  case surface_type:
    if ( m_nurbs_surface)
    {
      loc_bbox = m_nurbs_surface->BoundingBox();
      bTestLocBox = true;
    }
    break;

  case distance_type:
    rc = false;
    break;

  default:
    rc = true;
  }

  if ( bTestLocBox )
  {
    if ( m_d[1] < m_d[0] && m_d[0] > 0.0 )
    {
      // function is zero outside loc_bbox + m_d[0]
      double d = loc_bbox.MinimumDistanceTo(bbox);
      if ( d > m_d[0] )
	rc = true;
    }
    else if ( m_d[0] > 0.0 )
    {
      // function is zero inside loc_bbox-m_d[0]
      loc_bbox.m_min.x += m_d[0];
      loc_bbox.m_min.y += m_d[0];
      loc_bbox.m_min.z += m_d[0];
      loc_bbox.m_max.x -= m_d[0];
      loc_bbox.m_max.y -= m_d[0];
      loc_bbox.m_max.z -= m_d[0];
      if ( loc_bbox.IsValid() && loc_bbox.Includes(bbox) )
	rc = true;
    }
  }
  return rc;
}

ON_SpaceMorph::ON_SpaceMorph()
{
  m_tolerance = 0.0;
  m_bQuickPreview = false;
  m_bPreserveStructure = false;
}

ON_SpaceMorph::~ON_SpaceMorph()
{
}

ON_3dVector ON_SpaceMorph::MorphVector( 
	  ON_3dPoint tail_point, 
	  ON_3dVector vector 
	  ) const
{
  return (MorphPoint(tail_point+vector) - MorphPoint(tail_point));
}

ON_4dPoint ON_SpaceMorph::MorphPoint( 
	  ON_4dPoint point
	  ) const
{
  ON_4dPoint q = MorphPoint(ON_3dPoint(point));
  q.w = point.w;
  q.x *= q.w;
  q.y *= q.w;
  q.z *= q.w;
  return q;
}

bool ON_SpaceMorph::Ev1Der(
	  ON_3dPoint rst,
	  ON_3dPoint& xyz,
	  ON_3dVector& Dr,
	  ON_3dVector& Ds,
	  ON_3dVector& Dt
	  ) const
{
  // This is a quick estimate of the derivative.
  // Space morphs that are used to deform smooth breps
  // should override this function.
  double d = 1.0e-6;
  double e = 1.0/d;
  ON_3dPoint P;
  ON_3dPoint dr(rst.x+d,rst.y,rst.z);
  ON_3dPoint ds(rst.x,rst.y+d,rst.z);
  ON_3dPoint dt(rst.x,rst.y,rst.z+d);
  P = MorphPoint(rst);
  Dr = e*(MorphPoint(dr) - P);
  Ds = e*(MorphPoint(ds) - P);
  Dt = e*(MorphPoint(dt) - P);
  return true;
}

double ON_SpaceMorph::Tolerance() const
{
  return m_tolerance;
}

void ON_SpaceMorph::SetTolerance(double tolerance)
{
  m_tolerance = (ON_IsValid(tolerance) && tolerance > 0.0 ) 
	      ? tolerance
	      : 0.0;
}

bool ON_SpaceMorph::QuickPreview() const
{
  return m_bQuickPreview;
}

void ON_SpaceMorph::SetQuickPreview( bool bQuickPreview )
{
  m_bQuickPreview = bQuickPreview ? true : false;
}

bool ON_SpaceMorph::IsIdentity( const ON_BoundingBox& bbox ) const
{
  return false;
}

bool ON_SpaceMorph::PreserveStructure() const
{
  return m_bPreserveStructure;
}

void ON_SpaceMorph::SetPreserveStructure( bool bPreserveStructure )
{
  m_bPreserveStructure = bPreserveStructure ? true : false;
}

void ON_SpaceMorph::MorphPointList(
	int dim, 
	int is_rat,
	int count, 
	int stride,
	double* point
	) const
{
  if ( dim > 0 && stride >= (dim+(is_rat)?1:0) && count > 0 && point != 0 )
  {
    int i;
    if ( is_rat )
    {
      ON_4dPoint p(0.0,0.0,0.0,1.0), q;
      for ( i = 0; i < count; i++ )
      {
	p.x = point[0];
	if ( dim > 1 )
	  p.y = point[1];
	if ( dim > 2 )
	  p.z = point[2];
	p.w = point[dim];
	q = MorphPoint(p);
	point[0] = q.x;
	if ( dim > 1 )
	  point[1] = q.y;
	if ( dim > 2 )
	  point[2] = q.z;
	point[dim] = q.w;
	point += stride;
      }
    }
    else
    {
      ON_3dPoint p(0.0,0.0,0.0), q;
      for ( i = 0; i < count; i++ )
      {
	p.x = point[0];
	if ( dim > 1 )
	  p.y = point[1];
	if ( dim > 2 )
	  p.z = point[2];
	q = MorphPoint(p);
	point[0] = q.x;
	if ( dim > 1 )
	  point[1] = q.y;
	if ( dim > 2 )
	  point[2] = q.z;
	point += stride;
      }
    }
  }
}

void ON_SpaceMorph::MorphPointList(
	int dim, 
	int is_rat,
	int count, 
	int stride,
	float* point
	) const
{
  if ( dim > 0 && stride >= (dim+(is_rat)?1:0) && count > 0 && point != 0 )
  {
    int i;
    if ( is_rat )
    {
      ON_4dPoint p(0.0,0.0,0.0,1.0), q;
      for ( i = 0; i < count; i++ )
      {
	p.x = point[0];
	if ( dim > 1 )
	  p.y = point[1];
	if ( dim > 2 )
	  p.z = point[2];
	p.w = point[dim];
	q = MorphPoint(p);
	point[0] = (float)q.x;
	if ( dim > 1 )
	  point[1] = (float)q.y;
	if ( dim > 2 )
	  point[2] = (float)q.z;
	point[dim] = (float)q.w;
	point += stride;
      }
    }
    else
    {
      ON_3dPoint p(0.0,0.0,0.0), q;
      for ( i = 0; i < count; i++ )
      {
	p.x = point[0];
	if ( dim > 1 )
	  p.y = point[1];
	if ( dim > 2 )
	  p.z = point[2];
	q = MorphPoint(p);
	point[0] = (float)q.x;
	if ( dim > 1 )
	  point[1] = (float)q.y;
	if ( dim > 2 )
	  point[2] = (float)q.z;
	point += stride;
      }
    }
  }
}



bool ON_Point::IsMorphable() const
{
  return true;
}

bool ON_PointCloud::IsMorphable() const
{
  return true;
}

bool ON_Mesh::IsMorphable() const
{
  return true;
}

bool ON_Mesh::EvaluatePoint( const class ON_ObjRef& objref, ON_3dPoint& P ) const
{
  // virtual function default
  P = ON_UNSET_POINT;
  ON_COMPONENT_INDEX ci = objref.m_component_index;

  switch ( ci.m_type )
  {
  case ON_COMPONENT_INDEX::mesh_vertex:
    if ( ci.m_index >= 0 && ci.m_index < m_V.Count() )
      P = m_V[ci.m_index];
    break;

  case ON_COMPONENT_INDEX::meshtop_vertex:
    if ( ci.m_index >= 0 && ci.m_index < m_top.m_topv.Count() )
    {
      const ON_MeshTopologyVertex& topv = m_top.m_topv[ci.m_index];
      if ( topv.m_v_count > 0 && topv.m_vi )
      {
	int vi = topv.m_vi[0];
	if ( vi >= 0 && vi < m_V.Count() )
	  P = m_V[vi];
      }
    }
    break;

  case ON_COMPONENT_INDEX::meshtop_edge:
    if ( 5 == objref.m_evp.m_t_type 
	 && fabs(objref.m_evp.m_t[0] + objref.m_evp.m_t[1] - 1.0) <= ON_SQRT_EPSILON )
    {
      ON_Line L = m_top.TopEdgeLine(ci.m_index);
      if ( L.IsValid() )
      {
	P = L.PointAt(objref.m_evp.m_t[0]);
      }
    }
    break;

  case ON_COMPONENT_INDEX::mesh_face:
    if ( 4 == objref.m_evp.m_t_type 
	 && fabs(objref.m_evp.m_t[0] + objref.m_evp.m_t[1] + objref.m_evp.m_t[2] + objref.m_evp.m_t[3] - 1.0) <= ON_SQRT_EPSILON )
    {
      if ( ci.m_index >= 0 && ci.m_index < m_F.Count() )
      {
	const int* fvi = m_F[ci.m_index].vi;
	if ( fvi[0] < 0 || fvi[0] >= m_V.Count() )
	  break;
	if ( fvi[1] < 0 || fvi[1] >= m_V.Count() )
	  break;
	if ( fvi[2] < 0 || fvi[2] >= m_V.Count() )
	  break;
	if ( fvi[3] < 0 || fvi[3] >= m_V.Count() )
	  break;
	ON_3dPoint V[4];
	V[0] = m_V[fvi[0]];
	V[1] = m_V[fvi[1]];
	V[2] = m_V[fvi[2]];
	V[3] = m_V[fvi[3]];
	P = objref.m_evp.m_t[0]*V[0] + objref.m_evp.m_t[1]*V[1] + objref.m_evp.m_t[2]*V[2] + objref.m_evp.m_t[3]*V[3];
      }
    }
    break;

  default:
    // intentionally skipping other ON_COMPONENT_INDEX::TYPE enum values
    break;
  }

  return P.IsValid();
}

bool ON_NurbsCurve::IsMorphable() const
{
  return true;
}

bool ON_NurbsSurface::IsMorphable() const
{
  return true;
}

bool ON_LineCurve::IsMorphable() const
{
  return true;
}

bool ON_Brep::IsMorphable() const
{
  // NOTE: This is virtual function and
  //       TL_Brep::IsMorphable() returns
  //       true.
  bool rc = false;
  if ( m_F.Count() == 1 )
  {
    const ON_Surface* srf = m_F[0].SurfaceOf();
    if ( srf )
      rc = true;
  }
  return rc;
}



bool ON_Point::Morph( const ON_SpaceMorph& morph )
{
  point = morph.MorphPoint(point);
  return true;
}

bool ON_PointCloud::Morph( const ON_SpaceMorph& morph )
{
  if ( m_P.Count() > 0 )
  {
    morph.MorphPointList( 3, 0, m_P.Count(), 3, &m_P[0].x );
    m_bbox.Destroy();
  }
  return true;
}

bool ON_Mesh::Morph( const ON_SpaceMorph& morph )
{
  if ( m_V.Count() > 0 )
  {
    bool bHasFaceNormals = HasFaceNormals();
    bool bHasVertexNormals = HasVertexNormals();

    int i, count = m_V.Count();
    if ( bHasVertexNormals )
    {
      for ( i = 0; i < count; i++ )
      {
	m_N[i] = m_V[i] + 0.0009765625*m_N[i];
      }
      morph.MorphPointList( 3, 0, count, 3, &m_N[0].x );
    }

    morph.MorphPointList( 3, 0, count, 3, &m_V[0].x );

    if ( bHasVertexNormals )
    {
      for ( i = 0; i < count; i++ )
      {
	m_N[i] -= m_V[i];
	m_N[i].Unitize();
      }
    }

    m_FN.SetCount(0);
    m_K.SetCount(0);
    if ( bHasFaceNormals )
    {
      ComputeFaceNormals();
    }

    m_Ctag.Default();
    InvalidateVertexBoundingBox();
    InvalidateVertexNormalBoundingBox();
    InvalidateCurvatureStats();
  }
  return true;
}

bool ON_NurbsCurve::Morph( const ON_SpaceMorph& morph )
{
  DestroyCurveTree();
  BOOL bIsClosed = IsClosed();
  BOOL bIsPeriodic = IsPeriodic();
  morph.MorphPointList( m_dim, m_is_rat, m_cv_count, m_cv_stride, m_cv );
  if ( bIsPeriodic )
  {
    int i, deg = m_order-1;
    for ( i = 0; i < deg; i++ )
      SetCV( m_cv_count-deg+i, ON::intrinsic_point_style, CV(i) );
  }
  else if ( bIsClosed )
  {
    SetCV( m_cv_count-1, ON::intrinsic_point_style, CV(0) );
  }
  return true;
}


bool ON_NurbsSurface::Morph( const ON_SpaceMorph& morph )
{
  DestroySurfaceTree();
  BOOL bIsClosed[2];
  BOOL bIsPeriodic[2];
  BOOL bIsSingular[4];

  int i;
  for ( i = 0; i < 2; i++ )
  {
    bIsClosed[i] = IsClosed(i);
    bIsPeriodic[i] = IsPeriodic(i);
  }

  for ( i = 0; i < 4; i++ )
    bIsSingular[i] = IsSingular(i);

  for ( i = 0; i < m_cv_count[0]; i++ )
  {
    morph.MorphPointList( m_dim, m_is_rat, 
			  m_cv_count[1], m_cv_stride[1], 
			  CV(i,0) );
  }

  for ( i = 0; i < 4; i++ )
  {
    if ( bIsSingular[i] )
      CollapseSide(i);
  }

  // TODO - if input was closed/periodic make output the same

  return true;
}


bool ON_LineCurve::Morph( const ON_SpaceMorph& morph )
{
  DestroyCurveTree();
  m_line.from = morph.MorphPoint(m_line.from);
  m_line.to = morph.MorphPoint(m_line.to);
  return true;
}


bool ON_BezierCurve::Morph( const ON_SpaceMorph& morph )
{
  morph.MorphPointList( m_dim, m_is_rat, m_order, m_cv_stride, m_cv );
  return true;
}


bool ON_BezierSurface::Morph( const ON_SpaceMorph& morph )
{
  int i;
  for ( i = 0; i < m_order[0]; i++ )
  {
    morph.MorphPointList( m_dim, m_is_rat, 
			  m_order[1], m_cv_stride[1], 
			  CV(i,0) );
  }
  return true;
}

bool ON_Brep::Morph( const ON_SpaceMorph& morph )
{
  bool rc = IsMorphable();
  if ( rc )
  {
    ON_Surface* srf = const_cast<ON_Surface*>(m_F[0].SurfaceOf());
    
    if ( srf->IsMorphable() )
    {
      rc = srf->Morph(morph);
    }
    else
    {
      ON_NurbsSurface* new_srf = srf->NurbsSurface();
      if ( !new_srf )
	return false;
      rc = new_srf->Morph(morph);
      if (rc)
      {
	int si = m_F[0].m_si;
	m_F[0].SetProxySurface(new_srf);
	delete srf;
	srf = new_srf;
	m_S[si] = srf;
	DestroyMesh(ON::any_mesh,true);
      }
      else
      {
	delete new_srf;
	new_srf = 0;
      }
    }
    
    if ( rc )
    {
      double tol = 0.01; 
      rc = RebuildEdges( m_F[0], tol, true, true );

      DestroyMesh(ON::analysis_mesh);
      DestroyMesh(ON::preview_mesh);
      ON_Mesh* mesh = const_cast<ON_Mesh*>(m_F[0].Mesh(ON::render_mesh));
      if ( mesh )
	mesh->EvaluateMeshGeometry( *srf );
    }
  }
  return rc;
}

// virtual ON_Geometry override
bool ON_PolyCurve::Morph( const ON_SpaceMorph& morph )
{
  DestroyCurveTree();
  int segment_count = m_segment.Count();
  bool rc = segment_count > 0;
  if ( rc )
  {
    int i;
    ON_NurbsCurve* this_seg = 0;
    for ( i = 0; i < segment_count && rc; i++ )
    {
      if ( 0 == m_segment[i] )
	continue;
      this_seg = ON_NurbsCurve::Cast(m_segment[i]);
      if ( 0 == this_seg )
      {
	this_seg = m_segment[i]->NurbsCurve();
	if ( 0 != this_seg )
	{
	  delete m_segment[i];
	  m_segment[i] = this_seg;
	}
      }
      if ( 0 == this_seg )
	rc = false;
      else
	rc = this_seg->Morph(morph);
    }
  }
  return rc;
}

// virtual ON_Geometry override
bool ON_PolyCurve::IsMorphable() const
{
  return true;
}


// virtual ON_Geometry override
bool ON_PolylineCurve::Morph( const ON_SpaceMorph& morph )
{
  DestroyCurveTree();
  morph.MorphPointList( 3, false, m_pline.Count(), 3, (double*)m_pline.Array() );
  return true;
}

// virtual ON_Geometry override
bool ON_PolylineCurve::IsMorphable() const
{
  return true;
}
