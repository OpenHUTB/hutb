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

#include "CoreMinimal.h"
#include "XmlNode.h"

/**
 * @class MjXmlUtils
 * @brief Static utility functions for parsing MuJoCo XML nodes.
 */
class URLAB_API MjXmlUtils
{
public:
    /**
     * @brief Parses an FVector from a space-separated string "x y z".
     * @param S The input string.
     * @return The parsed FVector, or ZeroVector if invalid.
     */
    static FVector ParseVector(const FString& S);

    /**
     * @brief Parses an FVector2D from a space-separated string "x y".
     * @param S The input string.
     * @return The parsed FVector2D, or ZeroVector if invalid.
     */
    static FVector2D ParseVector2D(const FString& S);

    /**
     * @brief Parses a list of floats from a space-separated string.
     * @param S The input string.
     * @param OutArray The resulting array of floats.
     */
    static void ParseFloatArray(const FString& S, TArray<float>& OutArray);

    /**
     * @brief Parses a boolean from an XML attribute ("true"/"false" or "1"/"0").
     * @param S The input string.
     * @param bDefault The default value if the string is empty.
     * @return The parsed boolean.
     */
    static bool ParseBool(const FString& S, bool bDefault = false);

    // ---- Attribute-from-Node helpers ----
    // Each reads an attribute by name, sets bOverride=true and writes the value
    // if the attribute is present. Returns true if the attribute was found.
    // These eliminate the repeated GetAttribute→IsEmpty→Parse→bOverride pattern.

    /** @brief Read a float attribute. Returns true if present. */
    static bool ReadAttrFloat(const FXmlNode* Node, const TCHAR* Attr, float& Out, bool& bOverride);

    /** @brief Read a double attribute. Returns true if present. */
    static bool ReadAttrDouble(const FXmlNode* Node, const TCHAR* Attr, double& Out, bool& bOverride);

    /** @brief Read an int32 attribute. Returns true if present. */
    static bool ReadAttrInt(const FXmlNode* Node, const TCHAR* Attr, int32& Out, bool& bOverride);

    /** @brief Read a space-separated float array attribute (partial OK — MuJoCo style). Returns true if present. */
    static bool ReadAttrFloatArray(const FXmlNode* Node, const TCHAR* Attr, TArray<float>& Out, bool& bOverride);

    /** @brief Read a boolean attribute ("true"/"false"/"1"/"0"). Returns true if present. */
    static bool ReadAttrBool(const FXmlNode* Node, const TCHAR* Attr, bool& Out, bool& bOverride);

    /** @brief Read a string attribute. Returns true if present (non-empty). */
    static bool ReadAttrString(const FXmlNode* Node, const TCHAR* Attr, FString& Out);
};
