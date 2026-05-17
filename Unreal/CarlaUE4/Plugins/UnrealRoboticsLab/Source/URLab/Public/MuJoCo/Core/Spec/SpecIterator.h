// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// --- LEGAL DISCLAIMER ---
// UnrealRoboticsLab is an independent software plugin. It is NOT affiliated with, 
// endorsed by, or sponsored by Epic Games, Inc. "Unreal" and "Unreal Engine" are 
// trademarks or registered trademarks of Epic Games, Inc. in the US and elsewhere.
//
// This plugin incorporates third-party software: MuJoCo (Apache 2.0), 
// CoACD (MIT), and libzmq (MPL 2.0). See ThirdPartyNotices.txt for details.

#pragma once
#include <mujoco/mujoco.h>
#include <string>
#include <iterator>

namespace mj {

// Forward declarations
struct Body;
struct Joint;
struct Geom;
struct Site;

// -----------------------------------------------------------------------------
// 1. Generic Iterator
//    Uses mjs_firstChild and mjs_nextChild to traverse specific types efficiently.
// -----------------------------------------------------------------------------
template <typename WrapperT, mjtObj TypeEnum>
class ChildIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = WrapperT;
    using difference_type = std::ptrdiff_t;
    using pointer = WrapperT*;
    using reference = WrapperT&;

    // End iterator constructor
    ChildIterator() : parent_(nullptr), current_(nullptr) {}

    // Begin iterator constructor
    ChildIterator(mjsBody* parent) : parent_(parent) {
        // 0 for recurse means immediate children only
        current_ = mjs_firstChild(parent, TypeEnum, 0); 
    }

    WrapperT operator*() const {
        return WrapperT(current_);
    }

    ChildIterator& operator++() {
        if (current_ && parent_) {
            // 0 for recurse
            current_ = mjs_nextChild(parent_, current_, 0);
        }
        return *this;
    }

    ChildIterator operator++(int) {
        ChildIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const ChildIterator& other) const {
        return current_ == other.current_;
    }
    
    bool operator!=(const ChildIterator& other) const {
        return current_ != other.current_;
    }

private:
    mjsBody* parent_;
    mjsElement* current_;
};

// -----------------------------------------------------------------------------
// 2. Range Helper (Allow for-each loops)
// -----------------------------------------------------------------------------
template <typename WrapperT, mjtObj TypeEnum>
class ChildRange {
    mjsBody* parent_;
public:
    ChildRange(mjsBody* parent) : parent_(parent) {}
    auto begin() const { return ChildIterator<WrapperT, TypeEnum>(parent_); }
    auto end() const { return ChildIterator<WrapperT, TypeEnum>(); }
};

// -----------------------------------------------------------------------------
// 3. Object Wrappers
// -----------------------------------------------------------------------------

// -- Joint Wrapper --
struct Joint {
    mjsJoint* ptr;
    explicit Joint(mjsElement* e) : ptr(mjs_asJoint(e)) {}

    // Safe accessors using header helpers
    
    FString name() const { 
        
        const char* name = mjs_getString(mjs_getName(ptr->element));
        return (name && name[0]) ? UTF8_TO_TCHAR(name) : TEXT("(unnamed)");
    }
    mjtJoint type() const { return ptr->type; }
};

// -- Geom Wrapper --
struct Geom {
    mjsGeom* ptr;
    explicit Geom(mjsElement* e) : ptr(mjs_asGeom(e)) {}

    FString name() const { 
        
        const char* name = mjs_getString(mjs_getName(ptr->element));
        return (name && name[0]) ? UTF8_TO_TCHAR(name) : TEXT("(unnamed)");
    }
    mjtGeom type() const { return ptr->type; }
    
    // Example of using vector helpers from mujoco.h
    void setPos(double x, double y, double z) {
        ptr->pos[0] = x; ptr->pos[1] = y; ptr->pos[2] = z;
    }
};

// -- Site Wrapper --
struct Site {
    mjsSite* ptr;
    explicit Site(mjsElement* e) : ptr(mjs_asSite(e)) {}

    FString name() const { 
        const char* name = mjs_getString(mjs_getName(ptr->element));
        return (name && name[0]) ? UTF8_TO_TCHAR(name) : TEXT("(unnamed)");
    }
    mjtGeom type() const { return ptr->type; }
};

// -- Camera Wrapper --
struct Camera {
    mjsCamera* ptr;
    explicit Camera(mjsElement* e) : ptr(mjs_asCamera(e)) {}

    FString name() const { 
        const char* name = mjs_getString(mjs_getName(ptr->element));
        return (name && name[0]) ? UTF8_TO_TCHAR(name) : TEXT("(unnamed)");
    }
};

// -- Body Wrapper --
struct Body {
    mjsBody* ptr;
    
    // Constructor handles casting from generic element if necessary
    explicit Body(mjsElement* e) : ptr(mjs_asBody(e)) {}
    explicit Body(mjsBody* b) : ptr(b) {} // Direct constructor

    FString name() const { 
        
        const char* name = mjs_getString(mjs_getName(ptr->element));
        return (name && name[0]) ? UTF8_TO_TCHAR(name) : TEXT("(unnamed)");
    }

    // -- The Iterators --
    // These create temporary range objects that define begin()/end()
    
    auto bodies() const { return ChildRange<Body, mjOBJ_BODY>(ptr); }
    auto joints() const { return ChildRange<Joint, mjOBJ_JOINT>(ptr); }
    auto geoms()  const { return ChildRange<Geom, mjOBJ_GEOM>(ptr); }
    auto sites()  const { return ChildRange<struct Site, mjOBJ_SITE>(ptr); } 
    auto cameras() const { return ChildRange<struct Camera, mjOBJ_CAMERA>(ptr); }

    // Check if valid
    operator bool() const { return ptr != nullptr; }
};

} // namespace mj