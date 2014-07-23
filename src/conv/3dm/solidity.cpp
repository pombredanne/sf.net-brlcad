/*                    S O L I D I T Y . C P P
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
/** @file solidity.cpp
 *
 * Library to determine whether a BoT is closed.
 *
 */


#include "solidity.h"


#include <map>


namespace
{


typedef std::pair<int, int> Edge;


static bool
bot_is_orientable_register(std::map<Edge, int> &edge_order_map, int va, int vb)
{
    enum { ORDER_NONE = 0, ORDER_MIN_MAX, ORDER_MAX_MIN, ORDER_EDGE_FULL };

    const Edge edge(std::min(va, vb), std::max(va, vb));
    int &order = edge_order_map[edge];

    switch (order) {
	case ORDER_NONE:
	    order = va < vb ? ORDER_MIN_MAX : ORDER_MAX_MIN;
	    return va != vb;

	case ORDER_MIN_MAX:
	    order = ORDER_EDGE_FULL;
	    return va > vb;

	case ORDER_MAX_MIN:
	    order = ORDER_EDGE_FULL;
	    return va < vb;

	case ORDER_EDGE_FULL:
	default:
	    return false;
    }
}


}


int bot_is_closed(const rt_bot_internal *bot)
{
    // map edges to number of incident faces
    std::map<Edge, int> edge_incidence_map;

    for (std::size_t i = 0; i < bot->num_faces; ++i) {
	const int v1 = bot->faces[i * 3];
	const int v2 = bot->faces[i * 3 + 1];
	const int v3 = bot->faces[i * 3 + 2];

#define REGISTER_EDGE(va, vb) \
	do { \
	    const Edge edge(std::min((va), (vb)), std::max((va), (vb))); \
	    ++edge_incidence_map[edge]; \
	} while (false)

	REGISTER_EDGE(v1, v2);
	REGISTER_EDGE(v1, v3);
	REGISTER_EDGE(v2, v3);

#undef REGISTER_EDGE
    }

    // a mesh is closed if it has no boundary edges
    for (std::map<Edge, int>::const_iterator it = edge_incidence_map.begin();
	 it != edge_incidence_map.end(); ++it)
	if (it->second == 1) return false;

    return true;
}


int bot_is_orientable(const rt_bot_internal *bot)
{
    std::map<Edge, int> edge_order_map;

    // a mesh is orientable if any two adjacent faces
    // have compatible orientation
    for (std::size_t i = 0; i < bot->num_faces; ++i) {
	const int v1 = bot->faces[i * 3];
	const int v2 = bot->faces[i * 3 + 1];
	const int v3 = bot->faces[i * 3 + 2];

	if (!(bot_is_orientable_register(edge_order_map, v1, v2)
	      && bot_is_orientable_register(edge_order_map, v1, v3)
	      && bot_is_orientable_register(edge_order_map, v2, v3)))
	    return false;
    }

    return true;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
