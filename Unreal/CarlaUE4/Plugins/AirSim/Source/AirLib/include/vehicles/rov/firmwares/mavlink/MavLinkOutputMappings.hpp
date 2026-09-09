// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_MavLinkOutputMappings_hpp
#define msr_airlib_MavLinkOutputMappings_hpp

#include <map>
#include <string>
#include <common/Common.hpp>

namespace msr
{
namespace airlib
{

    //these vectors show how to map from Px4 actuator commands
    //{Main1, Main2, ..., Main8} to the inputs that AeroBody is expecting
    //-1 indicates the input is not used
    //These should all be of size 3 + 2*num_rotors
    struct MavLinkOutputMappings
    {
        std::map<std::string, std::vector<int>> mappings = {
            { "Rov", { 6, 7, -1, 1, 5, 0, 4, 2, -1 } },
            { "Generic", { 1, 0, -1, 3, -1 } } //generic fixedwing
        };
    };

}
}; //namespace

#endif