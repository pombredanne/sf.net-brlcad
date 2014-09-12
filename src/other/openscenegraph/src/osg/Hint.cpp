/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/

#include <osg/Hint>
#include <osg/StateSet>

using namespace osg;

void Hint::apply(State& /*state*/) const
{
    if (_target==GL_NONE || _mode==GL_NONE) return;

    glHint(_target, _mode);
}

void Hint::setTarget(GLenum target)
{
    if (_target==target) return;

    if (_parents.empty())
    {
        _target = target;
        return;
    }

    // take a reference to this clip plane to prevent it from going out of scope
    // when we remove it temporarily from its parents.
    osg::ref_ptr<Hint> hintRef = this;

    // copy the parents as they _parents list will be changed by the subsequent removeAttributes.
    ParentList parents = _parents;

    // remove this attribute from its parents as its position is being changed
    // and would no longer be valid.
    ParentList::iterator itr;
    for(itr = parents.begin();
        itr != parents.end();
        ++itr)
    {
        osg::StateSet* stateset = *itr;
        stateset->removeAttribute(this);
    }

    // assign the hint target
    _target = target;

    // add this attribute back into its original parents with its new position
    for(itr = parents.begin();
        itr != parents.end();
        ++itr)
    {
        osg::StateSet* stateset = *itr;
        stateset->setAttribute(this);
    }
}

