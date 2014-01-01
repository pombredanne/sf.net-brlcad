/*                 BoundaryCurve.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file step/BoundaryCurve.cpp
 *
 * Routines to convert STEP "BoundaryCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "BoundaryCurve.h"

#define CLASSNAME "BoundaryCurve"
#define ENTITYNAME "Boundary_Curve"
string BoundaryCurve::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)BoundaryCurve::Create);

BoundaryCurve::BoundaryCurve()
{
    step = NULL;
    id = 0;
}

BoundaryCurve::BoundaryCurve(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

BoundaryCurve::~BoundaryCurve()
{
}

bool
BoundaryCurve::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!CompositeCurveOnSurface::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::CompositeCurve." << std::endl;
	return false;
    }

    return true;
}

void

BoundaryCurve::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    CompositeCurveOnSurface::Print(level + 1);
}

STEPEntity *
BoundaryCurve::GetInstance(STEPWrapper *sw, int id)
{
    return new BoundaryCurve(sw, id);
}

STEPEntity *
BoundaryCurve::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
BoundaryCurve::LoadONBrep(ON_Brep *brep)
{
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
