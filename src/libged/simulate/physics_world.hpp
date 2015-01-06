/*               P H Y S I C S _ W O R L D . H P P
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file physics_world.hpp
 *
 * Brief description
 *
 */


#ifndef PHYSICS_WORLD_H
#define PHYSICS_WORLD_H


#include <vector>

#include <btBulletDynamicsCommon.h>

#include "vmath.h"


namespace simulate
{


class PhysicsWorld
{
public:
    PhysicsWorld();
    ~PhysicsWorld();

    void step(btScalar seconds);
    void add_object(matp_t matrix, const vect_t &bounding_box_dimensions,
		    fastf_t mass, const vect_t &linear_velocity, const vect_t &angular_velocity);


private:
    class WorldObject;


    PhysicsWorld(const PhysicsWorld &source);
    PhysicsWorld &operator=(const PhysicsWorld &source);

    btDbvtBroadphase m_broadphase;
    btDefaultCollisionConfiguration m_collision_config;
    btCollisionDispatcher m_collision_dispatcher;
    btSequentialImpulseConstraintSolver m_constraint_solver;
    btDiscreteDynamicsWorld m_world;

    std::vector<WorldObject *> m_objects;
};


}


#endif


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
