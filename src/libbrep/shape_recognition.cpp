#include "common.h"

#include <set>
#include <map>
#include <string>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

#define L1_OFFSET 2
#define L2_OFFSET 4

#define WRITE_ISLAND_BREPS 1

// TODO - the topological test by itself is not guaranteed to isolate volumes that
// are uniquely positive or uniquely negative contributions to the overall volume.
// It may be that a candidate subbrep has both positive and
// negative contributions to make to the overall volume.  Hopefully (not sure yet)
// this approach will reduce such situations to planar and general surface volumes,
// but we will need to handle them at those levels.  For example,
//
//                                       *
//                                      * *
//         ---------------------       *   *       ---------------------
//         |                    *     *     *     *                    |
//         |                     * * *       * * *                     |
//         |                                                           |
//         |                                                           |
//         -------------------------------------------------------------
//
// The two faces in the middle contribute both positively and negatively to the
// final surface area, and if this area were extruded would also define both
// positive and negative volume.  Probably convex/concave testing is what
// will be needed to resolve this.

#define TOO_MANY_CSG_OBJS(_obj_cnt, _msgs) \
    if (_obj_cnt > CSG_BREP_MAX_OBJS && _msgs) \
	bu_vls_printf(_msgs, "Error - brep converted to more than %d implicits - not currently a good CSG candidate\n", obj_cnt - 1); \
    if (_obj_cnt > CSG_BREP_MAX_OBJS) \
	goto bail


void
get_face_set_from_loops(struct subbrep_island_data *sb)
{
    const ON_Brep *brep = sb->brep;
    std::set<int> faces;
    std::set<int> fol;
    std::set<int> fil;
    std::set<int> loops;
    std::set<int>::iterator l_it;

    // unpack loop info
    array_to_set(&loops, sb->loops, sb->loops_cnt);

    for (l_it = loops.begin(); l_it != loops.end(); l_it++) {
	const ON_BrepLoop *loop = &(brep->m_L[*l_it]);
	const ON_BrepFace *face = loop->Face();
	bool is_outer = (face->OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	faces.insert(face->m_face_index);
	if (is_outer) {
	    fol.insert(face->m_face_index);
	} else {
	    fil.insert(face->m_face_index);
	}
    }

    // Pack up results
    set_to_array(&(sb->faces), &(sb->faces_cnt), &faces);
    set_to_array(&(sb->fol), &(sb->fol_cnt), &fol);
    set_to_array(&(sb->fil), &(sb->fil_cnt), &fil);
}

void
get_edge_set_from_loops(struct subbrep_island_data *sb)
{
    const ON_Brep *brep = sb->brep;
    std::set<int> edges;
    std::set<int> loops;
    std::set<int>::iterator l_it;

    // unpack loop info
    array_to_set(&loops, sb->loops, sb->loops_cnt);

    for (l_it = loops.begin(); l_it != loops.end(); l_it++) {
	const ON_BrepLoop *loop = &(brep->m_L[*l_it]);
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    if (trim->m_ei != -1 && edge->TrimCount() > 0) {
		edges.insert(trim->m_ei);
	    }
	}
    }

    // Pack up results
    set_to_array(&(sb->edges), &(sb->edges_cnt), &edges);
}


void subbrep_tree_node_init(struct subbrep_tree_node **node)
{
    BU_GET((*node), struct subbrep_tree_node);
    BU_GET((*node)->subtractions, struct bu_ptbl);
    BU_GET((*node)->unions, struct bu_ptbl);
    bu_ptbl_init((*node)->subtractions, 8, "init table");
    bu_ptbl_init((*node)->unions, 8, "init table");
    (*node)->parent = NULL;
    (*node)->island = NULL;
}

/* This is the critical point at which we take a pile of shapes and actually reconstruct a
 * valid boolean tree.  The stages are as follows:
 *
 * 1.  Identify the top level union objects (they will have no fil faces in their structure).
 * 2.  Using the fil faces, build up a queue of subbreps that are topologically connected
 *     by a loop to the top level union objects.
 * 3.  Determine the boolean status of the 2nd layer of objects.
 * 4.  For each sub-object, if that object in turn has fil faces, queue up those objects and goto 3.
 * 5.  Continue iterating until all objects have a boolean operation assigned.
 * 6.  For each unioned object, build a set of all subtraction objects (topologically connected or not.)
 * 7.  add the union pointer to the subbreps_tree, and then add all all topologically connected subtractions
 *     and remove them from the local set.
 * 8.  For the remaining (non-topologically linked) subtractions, get a bounding box and see if it overlaps
 *     with the bounding box of the union object in question.  If yes, we have to stash it for later
 *     evaluation - only solid raytracing will suffice to properly resolve complex cases.
 *     If not, no action is needed.  Once evaluated, remove the subtraction pointer from the set.
 *
 * Initially the test will be axis aligned bounding boxes, but ideally we should use oriented bounding boxes
 * or even tighter tests.  There's a catch here - a positive object overall may be within the bounding box
 * of a subtracting object (like a positive cone at the bottom of a negative cylinder).  First guess at a rule - subtractions
 * may propagate back up the topological tree to unions above the subtraction or in other isolated topological
 * trees, but never to unions below the subtraction
 * object itself.  Which means we'll need an above/below test for union objects relative to a given
 * subtraction object, or an "ignore" list for unions which overrides any bbox tests - that list would include the
 * parent subtraction, that subtraction's parent if it is a subtraction, and so on up to the top level union.  Nested
 * unions and subtractions mean we'll have to go all the way up that particular chain...
 */
void
find_hierarchy(struct bu_vls *UNUSED(msgs), struct subbrep_tree_node *node, struct bu_ptbl *islands)
{
    int depth = 0;
    if (!node) return;

    std::map<int, long*> fol_to_i;
    for (unsigned int i = 0; i < BU_PTBL_LEN(islands); i++) {
	struct subbrep_island_data *id = (struct subbrep_island_data *)BU_PTBL_GET(islands, i);
	for (int j = 0; j < id->fol_cnt; j++) {
	    fol_to_i.insert(std::make_pair(id->fol[j], (long *)id));
	}
    }

    /* Separate out top level unions */
    if (node->parent == NULL) {
	int top_cnt = 0;
	for (unsigned int i = 0; i < BU_PTBL_LEN(islands); i++) {
	    struct subbrep_island_data *obj = (struct subbrep_island_data *)BU_PTBL_GET(islands, i);
	    if (obj->fil_cnt == 0) {
		std::cout << "Top union found: " << bu_vls_addr(obj->key) << "\n";
		top_cnt++;
		node->island = obj;
		obj->nucleus->params->bool_op = 'u';
	    }
	}
	if (top_cnt != 1) {
	    bu_log("Error - incorrect toplevel union count: %d\n", top_cnt);
	    return;
	}
    } else {
	struct subbrep_tree_node *n = node->parent;
	while (n) {
	    depth++;
	    n = n->parent;
	}
    }


    for (unsigned int i = 0; i < BU_PTBL_LEN(islands); i++) {
	struct subbrep_island_data *id = (struct subbrep_island_data *)BU_PTBL_GET(islands, i);
	if (id == node->island) continue;
	bu_log("%*schecking island %s\n", depth, " ", bu_vls_addr(id->key));
	for (int j = 0; j < id->fil_cnt; j++) {
	    long *ip = fol_to_i.find(id->fil[j])->second;
	    if (ip != (long *)id) {
		struct subbrep_island_data *ipd = (struct subbrep_island_data *)ip;
		struct subbrep_tree_node *n = NULL;
		if (ipd->nucleus->params->bool_op == 'u') {
		    bu_log("%*sadding union %s\n", depth, " ", bu_vls_addr(ipd->key));
		    subbrep_tree_node_init(&n);
		    n->island = ipd;
		    n->parent = node;
		    n->fil = id->fil[j]; // Will probably need the plane from this face for later testing.
		    bu_ptbl_ins(node->unions, (long *)n);
		}
		if (ipd->nucleus->params->bool_op == '-') {
		    bu_log("%*sadding subtraction %s\n", depth, " ", bu_vls_addr(ipd->key));
		    subbrep_tree_node_init(&n);
		    n->island = ipd;
		    n->parent = node;
		    n->fil = id->fil[j]; // Will probably need the plane from this face for later testing.
		    bu_ptbl_ins(node->subtractions, (long *)n);
		}
		if (!n) {
		    bu_log("Erk! island without nucleus info: %s\n", bu_vls_addr(ipd->key));
		    return;
		}
	    }
	}
    }

    for (unsigned int i = 0; i < BU_PTBL_LEN(node->unions); i++) {
	struct subbrep_tree_node *n = (struct subbrep_tree_node *)BU_PTBL_GET(node->unions, i);
	(void)find_hierarchy(NULL, n, islands);
    }

    for (unsigned int i = 0; i < BU_PTBL_LEN(node->unions); i++) {
	struct subbrep_tree_node *n = (struct subbrep_tree_node *)BU_PTBL_GET(node->subtractions, i);
	(void)find_hierarchy(NULL, n, islands);
    }
       
#if 0
    // Now that we have the subbreps (or their substructures) and their boolean status we need to
    // construct the top level tree.

    std::set<long *> unions;
    std::set<long *> subtractions;
    std::set<long *>::iterator sb_it, sb_it2;
    std::map<long *, std::set<long *> > subbrep_subobjs;
    std::map<long *, std::set<long *> > subbrep_ignoreobjs;
    struct bu_ptbl *subbreps_tree;

    if (!islands) return NULL;

    for (unsigned int i = 0; i < BU_PTBL_LEN(islands); i++) {
	island_set.insert(BU_PTBL_GET(islands, i));
    }

    /* Separate out top level unions */
    for (sb_it = island_set.begin(); sb_it != island_set.end(); sb_it++) {
	struct subbrep_island_data *obj = (struct subbrep_island_data *)*sb_it;
	//std::cout << bu_vls_addr(obj->key) << " bool: " << obj->params->bool_op << "\n";
	if (obj->fil_cnt == 0) {
	    if (!(obj->nucleus->bool_op == '-')) {
		//std::cout << "Top union found: " << bu_vls_addr(obj->key) << "\n";
		obj->nucleus->bool_op = 'u';
		unions.insert((long *)obj);
		island_set.erase((long *)obj);
	    } else {
		if (obj->nucleus->bool_op == '-') {
		    std::cout << "zero fils, but a negative shape - " << bu_vls_addr(obj->key) << " added to subtractions\n";
		    subtractions.insert((long *)obj);
		}
	    }
	}
    }

    std::set<long *> subbrep_seed_set = unions;

    int iterate = 1;
    while (subbrep_seed_set.size() > 0) {
	//std::cout << "iterate: " << iterate << "\n";
	std::set<long *> subbrep_seed_set_2;
	std::set<long *> island_set_b;
	for (sb_it = subbrep_seed_set.begin(); sb_it != subbrep_seed_set.end(); sb_it++) {
	    std::set<int> tu_fol;
	    struct subbrep_island_data *tu = (struct subbrep_island_data *)*sb_it;
	    array_to_set(&tu_fol, tu->fol, tu->fol_cnt);
	    island_set_b = island_set;
	    for (sb_it2 = island_set.begin(); sb_it2 != island_set.end(); sb_it2++) {
		struct subbrep_island_data *cobj = (struct subbrep_island_data *)*sb_it2;
		for (int i = 0; i < cobj->fil_cnt; i++) {
		    if (tu_fol.find(cobj->fil[i]) != tu_fol.end()) {
			struct subbrep_island_data *up;
			//std::cout << "Object " << bu_vls_addr(cobj->key) << " connected to " << bu_vls_addr(tu->key) << "\n";
			subbrep_seed_set_2.insert(*sb_it2);
			island_set_b.erase(*sb_it2);
			// Determine boolean relationship to parent here, and use parent boolean
			// to decide this object's (cobj) status.  If tu is negative and cobj is unioned
			// relative to tu, then cobj is also negative.

			// This is a make-or-break step of the algorithm - if we cannot determine whether
			// a subbrep is added or subtracted, the whole B-Rep conversion process fails.
			//
			// TODO - In theory a partial conversion might still be achieved by re-inserting the
			// problem subbrep back into the parent B-Rep and proceeding with the rest, but
			// we are not currently set up for that - as of right now, a single failure on
			// this level of logic results in a completely failed csg conversion.

			int bool_test = 0;
			int already_flipped = 0;
			if (cobj->nucleus->bool_op == '\0') {
			    /* First, check the boolean relationship to the parent solid */
			    cobj->parent = tu;
			    bool_test = subbrep_determine_boolean(cobj);
			    //std::cout << "Initial boolean test for " << bu_vls_addr(cobj->key) << ": " << bool_test << "\n";
			    switch (bool_test) {
				case -2:
				    if (msgs) {
					bu_vls_printf(msgs, "%*sGame over - self intersecting shape reported with subbrep %s\n", L1_OFFSET, " ", bu_vls_addr(cobj->key));
					bu_vls_printf(msgs, "%*sUntil breakdown logic for this situation is available, this is a conversion stopper.\n", L1_OFFSET, " ");
				    }
				    return NULL;
				case 2:
				    /* Test relative to parent inconclusive - fall back on surface test, if available */
				    if (cobj->negative_shape == -1) bool_test = -1;
				    if (cobj->negative_shape == 1) bool_test = 1;
				    break;
				default:
				    break;
			    }
			} else {
			    //std::cout << "Boolean status of " << bu_vls_addr(cobj->key) << " already determined\n";
			    bool_test = (cobj->nucleus->bool_op == '-') ? -1 : 1;
			    already_flipped = 1;
			}

			switch (bool_test) {
			    case -1:
				cobj->nucleus->bool_op = '-';
				subtractions.insert((long *)cobj);
				subbrep_subobjs[*sb_it].insert((long *)cobj);
				break;
			    case 1:
				cobj->nucleus->bool_op = 'u';
				unions.insert((long *)cobj);
				/* We've got a union - any subtractions that are parents of this
				 * object go in its ignore list */
				up = cobj->parent;
				while (up) {
				    if (up->nucleus->bool_op == '-') subbrep_ignoreobjs[(long *)cobj].insert((long *)up);
				    up = up->parent;
				}
				break;
			    default:
				if (msgs) bu_vls_printf(msgs, "%*sBoolean status of %s could not be determined - conversion failure.\n", L1_OFFSET, " ", bu_vls_addr(cobj->key));
				return NULL;
				break;
			}

			// If the parent ended up subtracted, we need to propagate the change down the
			// children of this subbrep.
			if (bool_test == -1 && !already_flipped) {
			    for (unsigned int j = 0; j < BU_PTBL_LEN(cobj->children); j++){
				struct subbrep_island_data *cdata = (struct subbrep_island_data *)BU_PTBL_GET(cobj->children,j);
				if (cdata && cdata->nucleus) {
				    cdata->nucleus->bool_op = (cdata->nucleus->bool_op == '-') ? 'u' : '-';
				} else {
				    //std::cout << "Child without params??: " << bu_vls_addr(cdata->key) << ", parent: " << bu_vls_addr(cobj->key) << "\n";
				}
			    }

			    /* If we're a B-Rep and a subtraction, the B-Rep will be inside out */
			    if (cobj->type == BREP && cobj->nucleus->bool_op == '-') {
				for (int l = 0; l < cobj->local_brep->m_F.Count(); l++) {
				    ON_BrepFace &face = cobj->local_brep->m_F[l];
				    cobj->local_brep->FlipFace(face);
				}
			    }
			}
			//std::cout << "Boolean status of " << bu_vls_addr(cobj->key) << ": " << cobj->params->bool_op << "\n";
		    }
		}
	    }
	}
	subbrep_seed_set.clear();
	subbrep_seed_set = subbrep_seed_set_2;
	island_set.clear();
	island_set = island_set_b;
	iterate++;
    }

    BU_GET(subbreps_tree, struct bu_ptbl);
    bu_ptbl_init(subbreps_tree, 8, "subbrep tree table");

    for (sb_it = unions.begin(); sb_it != unions.end(); sb_it++) {
	struct subbrep_island_data *pobj = (struct subbrep_island_data *)(*sb_it);
	bu_ptbl_ins(subbreps_tree, (long *)pobj);
	std::set<long *> topological_subtractions = subbrep_subobjs[(long *)pobj];
	std::set<long *> local_subtraction_queue = subtractions;
	for (sb_it2 = topological_subtractions.begin(); sb_it2 != topological_subtractions.end(); sb_it2++) {
	    bu_ptbl_ins(subbreps_tree, *sb_it2);
	    local_subtraction_queue.erase(*sb_it2);
	}
	std::set<long *> ignore_queue = subbrep_ignoreobjs[(long *)pobj];
	for (sb_it2 = ignore_queue.begin(); sb_it2 != ignore_queue.end(); sb_it2++) {
	    local_subtraction_queue.erase(*sb_it2);
	}

	// Now, whatever is left in the local subtraction queue has to be ruled out based on volumetric
	// intersection testing.
	if (!local_subtraction_queue.empty()) {
	    // Construct bounding box for pobj
	    if (!pobj->bbox_set) subbrep_bbox(pobj);
	    // Iterate over the queue
	    for (sb_it2 = local_subtraction_queue.begin(); sb_it2 != local_subtraction_queue.end(); sb_it2++) {
		struct subbrep_island_data *sobj = (struct subbrep_island_data *)(*sb_it2);
		//std::cout << "Checking subbrep " << bu_vls_addr(sobj->key) << "\n";
		// Construct bounding box for sobj
		if (!sobj->bbox_set) subbrep_bbox(sobj);
		ON_BoundingBox isect;
		bool bbi = isect.Intersection(*pobj->bbox, *sobj->bbox);
		// If there is overlap, we have a possible subtraction object.  It isn't currently possible
		// to sort out reliably in the libbrep context if this object is really a net contributor
		// to negative volume, so we store the candidates for post processing.  Not even surface/surface
		// intersection testing will resolve this - a small arb in the center of a torus will not contribute
		// a net negative volume, but a small arb in the center of a sphere will, and in both cases
		// the bbox tests and the surface/surface tests will give the same answers.  At the moment the
		// only practical approach requires solid raytracing.
		if (bbi) {
		    bu_ptbl_ins(pobj->subtraction_candidates, *sb_it2);
		}
	    }
	}
    }
    return subbreps_tree;
#endif
    return;
}



int cyl_validate_face(const ON_BrepFace *forig, const ON_BrepFace *fcand)
{
    ON_Cylinder corig;
    ON_Surface *csorig = forig->SurfaceOf()->Duplicate();
    csorig->IsCylinder(&corig);
    delete csorig;

    ON_Cylinder ccand;
    ON_Surface *cscand = fcand->SurfaceOf()->Duplicate();
    cscand->IsCylinder(&ccand);
    delete cscand;

    // Make sure the surfaces are on the same cylinder
    if (corig.circle.Center().DistanceTo(ccand.circle.Center()) > BREP_CYLINDRICAL_TOL) return 0;

    // Make sure the radii are the same
    if (!NEAR_ZERO(corig.circle.Radius() - ccand.circle.Radius(), VUNITIZE_TOL)) return 0;

    return 1;
}


int
shoal_filter_loop(int control_loop, int candidate_loop, struct subbrep_island_data *data)
{
    const ON_Brep *brep = data->brep;
    surface_t *face_surface_types = (surface_t *)data->face_surface_types;
    const ON_BrepFace *forig = brep->m_L[control_loop].Face();
    const ON_BrepFace *fcand = brep->m_L[candidate_loop].Face();
    surface_t otype  = face_surface_types[forig->m_face_index];
    surface_t ctype  = face_surface_types[fcand->m_face_index];

    switch (otype) {
	case SURFACE_CYLINDRICAL_SECTION:
	case SURFACE_CYLINDER:
	    if (ctype != SURFACE_CYLINDRICAL_SECTION && ctype != SURFACE_CYLINDER) return 0;
	    if (!cyl_validate_face(forig, fcand)) return 0;
	    break;
	case SURFACE_SPHERICAL_SECTION:
	case SURFACE_SPHERE:
	    if (ctype != SURFACE_SPHERICAL_SECTION && ctype != SURFACE_SPHERE) return 0;
	    break;
	default:
	    if (otype != ctype) return 0;
	    break;
    }

    return 1;
}

int
shoal_build(int **s_loops, int loop_index, struct subbrep_island_data *data)
{
    int s_loops_cnt = 0;
    const ON_Brep *brep = data->brep;
    std::set<int> processed_loops;
    std::set<int> shoal_loops;
    std::queue<int> todo;

    shoal_loops.insert(loop_index);
    todo.push(loop_index);

    while(!todo.empty()) {
	int lc = todo.front();
	const ON_BrepLoop *loop = &(brep->m_L[lc]);
	processed_loops.insert(lc);
	todo.pop();
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    if (trim->m_ei != -1 && edge) {
		for (int j = 0; j < edge->m_ti.Count(); j++) {
		    const ON_BrepTrim *t = &(brep->m_T[edge->m_ti[j]]);
		    int li = t->Loop()->m_loop_index;
		    if (processed_loops.find(li) == processed_loops.end()) {
			if (shoal_filter_loop(lc, li, data)) {
			    shoal_loops.insert(li);
			    todo.push(li);
			} else {
			    processed_loops.insert(li);
			}
		    }
		}
	    }
	}
    }

    set_to_array(s_loops, &(s_loops_cnt), &shoal_loops);
    return s_loops_cnt;
}



/* In order to represent complex shapes, it is necessary to identify
 * subsets of subbreps that can be represented as primitives.  This
 * function will identify such subsets, if possible.  If a subbrep
 * can be successfully decomposed into primitive candidates, the
 * type of the subbrep is set to COMB and the children bu_ptbl is
 * populated with subbrep_island_data sets. */
int
subbrep_split(struct bu_vls *msgs, struct subbrep_island_data *data)
{
    bu_log("splitting\n");
    int csg_fail = 0;
    std::set<int> loops;
    std::set<int> active;
    std::set<int> fully_planar;
    std::set<int> partly_planar;
    std::set<int>::iterator l_it, e_it;

    array_to_set(&loops, data->loops, data->loops_cnt);
    array_to_set(&active, data->loops, data->loops_cnt);

    for (l_it = loops.begin(); l_it != loops.end(); l_it++) {
	const ON_BrepLoop *loop = &(data->brep->m_L[*l_it]);
	const ON_BrepFace *face = loop->Face();
	surface_t surface_type = ((surface_t *)data->face_surface_types)[face->m_face_index];
	// Characterize the planar faces. TODO - see if we actually need this or not...
	if (surface_type != SURFACE_PLANE) {
	    // non-planar face - check to see whether it's already part of a shoal.  If not,
	    // create a new one - otherwise, skip.
	    if (active.find(face->m_face_index) != active.end()) {
		std::set<int> shoal_loops;
		struct subbrep_shoal_data *sh;
		BU_GET(sh, struct subbrep_shoal_data);
		subbrep_shoal_init(sh, data);
		(*(data->obj_cnt))++;
		sh->params->id = (*(data->obj_cnt));
		sh->i = data;
		sh->loops_cnt = shoal_build(&(sh->loops), loop->m_loop_index, data);
		for (int i = 0; i < sh->loops_cnt; i++) {
		    active.erase(sh->loops[i]);
		}
		int local_fail = 0;
		switch (surface_type) {
		    case SURFACE_CYLINDRICAL_SECTION:
		    case SURFACE_CYLINDER:
			if (!cylinder_csg(msgs, sh, BREP_CYLINDRICAL_TOL)) local_fail++;
			break;
		    case SURFACE_CONE:
			local_fail++;
			break;
		    case SURFACE_SPHERICAL_SECTION:
		    case SURFACE_SPHERE:
			local_fail++;
			break;
		    case SURFACE_TORUS:
			local_fail++;
			break;
		    default:
			break;
		}
		if (!local_fail) {
		    bu_ptbl_ins(data->children, (long *)sh);
		} else {
		    csg_fail++;
		}
	    }
	}
    }

    if (csg_fail > 0) {
	goto csg_abort;
    } else {
	// We had a successful conversion - now generate a nucleus.  Need to
	// characterize the nucleus as positive or negative volume.  Can also
	// be degenerate if all planar faces are coplanar or if there are no
	// planar faces at all (perfect cylinder, for example) so will need
	// rules for those situations...
	if (!island_nucleus(msgs, data)) goto csg_abort;

	// Note is is possible to have degenerate *edges*, not just vertices -
	// if a shoal shares part of an outer loop (possible when connected
	// only by a vertex - see example) then the edges in the parent loop
	// that are part of that shoal are degenerate for the purposes of the
	// "parent" shape.  May have to limit our inputs to
	// non-self-intersecting loops for now...
    }
    return 1;
csg_abort:
    /* Clear children - we found something we can't handle, so there will be no
     * CSG conversion of this subbrep. Cleanup is handled one level up. */
    return 0;
}

struct bu_ptbl *
find_subbreps(struct bu_vls *msgs, const ON_Brep *brep)
{
    /* Number of successful conversion operations */
    int successes = 0;
    /* Overall object counter */
    int obj_cnt = 0;
    /* Container to hold island data structures */
    struct bu_ptbl *subbreps;
    BU_GET(subbreps, struct bu_ptbl);
    bu_ptbl_init(subbreps, 8, "subbrep table");

    // Characterize all face surface types once
    std::map<int, surface_t> fstypes;
    surface_t *face_surface_types = (surface_t *)bu_calloc(brep->m_F.Count(), sizeof(surface_t), "surface type array");
    for (int i = 0; i < brep->m_F.Count(); i++) {
	face_surface_types[i] = GetSurfaceType(brep->m_F[i].SurfaceOf());
    }

    // Use the loops to identify subbreps
    std::set<int> brep_loops;
    /* Build a set of loops.  All loops will be assigned to a subbrep */
    for (int i = 0; i < brep->m_L.Count(); i++) {
	brep_loops.insert(i);
    }

    while (!brep_loops.empty()) {
	std::set<int> loops;
	std::queue<int> todo;

	/* For each iteration, we have a new island */
	struct subbrep_island_data *sb = NULL;
	BU_GET(sb, struct subbrep_island_data);
	subbrep_island_init(sb, brep);

	// Seed the queue with the first loop in the set
	std::set<int>::iterator top = brep_loops.begin();
	todo.push(*top);

	// Process the queue
	while(!todo.empty()) {
	    const ON_BrepLoop *loop = &(brep->m_L[todo.front()]);
	    loops.insert(todo.front());
	    brep_loops.erase(todo.front());
	    todo.pop();
	    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
		const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
		const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
		if (trim->m_ei != -1 && edge->TrimCount() > 0) {
		    for (int j = 0; j < edge->TrimCount(); j++) {
			int li = edge->Trim(j)->Loop()->m_loop_index;
			if (loops.find(li) == loops.end()) {
			    todo.push(li);
			    loops.insert(li);
			    brep_loops.erase(li);
			}
		    }
		}
	    }
	}

	// If our object count is growing beyond reason, bail
	TOO_MANY_CSG_OBJS(obj_cnt, msgs);

	// Pass the counter pointer through in case sub-objects need
	// to generate IDs as well.
	sb->obj_cnt = &obj_cnt;

	// Pass along shared data
	sb->brep = brep;
	sb->face_surface_types = (void *)face_surface_types;

	// Assign loop set to island
	set_to_array(&(sb->loops), &(sb->loops_cnt), &loops);

	// Assemble the set of faces
	get_face_set_from_loops(sb);

	// Assemble the set of edges
	get_edge_set_from_loops(sb);

	// Get key based on face indicies
	std::set<int> faces;
	array_to_set(&faces, sb->faces, sb->faces_cnt);
	std::string key = set_key(faces);
	bu_vls_sprintf(sb->key, "%s", key.c_str());

	// Check to see if we have a general surface that precludes conversion
	surface_t hof = highest_order_face(sb);
	if (hof >= SURFACE_GENERAL) {
	    if (msgs) bu_vls_printf(msgs, "Note - general surface present in island %s, representing as B-Rep\n", bu_vls_addr(sb->key));
	    sb->type = BREP;
	    (void)subbrep_make_brep(msgs, sb);
	    bu_ptbl_ins(subbreps, (long *)sb);
	    continue;
	}

	int split = subbrep_split(msgs, sb);
	TOO_MANY_CSG_OBJS(obj_cnt, msgs);
	if (!split) {
	    if (msgs) bu_vls_printf(msgs, "Note - split of %s unsuccessful, making brep\n", bu_vls_addr(sb->key));
	    sb->type = BREP;
	    (void)subbrep_make_brep(msgs, sb);
	} else {
	    // If we did successfully split the brep, do some post-split clean-up
	    sb->type = COMB;
	    //if (sb->planar_obj) subbrep_planar_close_obj(sb);
	    successes++;
#if WRITE_ISLAND_BREPS
	    (void)subbrep_make_brep(msgs, sb);
#endif
	}
	bu_ptbl_ins(subbreps, (long *)sb);
    }

    /* If we didn't do anything to simplify the shape, we're stuck with the original */
    if (!successes) {
	if (msgs) bu_vls_printf(msgs, "%*sNote - no successful simplifications\n", L1_OFFSET, " ");
	goto bail;
    } else {
	bu_log("find us some hierarchy...\n");
	struct subbrep_tree_node *root = NULL;
	subbrep_tree_node_init(&root);
	//find_hierarchy(NULL, root, subbreps);
	if (!root->island) bu_log("no hierarchy found yet...\n");
    }

    return subbreps;

bail:
    // Free memory
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++){
	struct subbrep_island_data *obj = (struct subbrep_island_data *)BU_PTBL_GET(subbreps, i);
	subbrep_island_free(obj);
	BU_PUT(obj, struct subbrep_island_data);
    }
    bu_ptbl_free(subbreps);
    BU_PUT(subbreps, struct bu_ptbl);
    return NULL;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
