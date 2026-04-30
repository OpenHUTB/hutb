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

#include "MuJoCo/Utils/MjXmlUtils.h"

FVector MjXmlUtils::ParseVector(const FString& S)
{
    TArray<FString> Parts;
    S.ParseIntoArray(Parts, TEXT(" "), true);
    if (Parts.Num() >= 3)
    {
        return FVector(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), FCString::Atod(*Parts[2]));
    }
    return FVector::ZeroVector;
}

FVector2D MjXmlUtils::ParseVector2D(const FString& S)
{
    TArray<FString> Parts;
    S.ParseIntoArray(Parts, TEXT(" "), true);
    if (Parts.Num() >= 2)
    {
        return FVector2D(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]));
    }
    return FVector2D::ZeroVector;
}

void MjXmlUtils::ParseFloatArray(const FString& S, TArray<float>& OutArray)
{
    TArray<FString> Parts;
    S.ParseIntoArray(Parts, TEXT(" "), true);
    OutArray.Empty(Parts.Num());
    for (const FString& P : Parts)
    {
        OutArray.Add(FCString::Atof(*P));
    }
}

bool MjXmlUtils::ParseBool(const FString& S, bool bDefault)
{
    if (S.IsEmpty()) return bDefault;
    return S.Equals(TEXT("true"), ESearchCase::IgnoreCase) || S.Equals(TEXT("1"));
}

// ---- Attribute-from-Node helpers ----

bool MjXmlUtils::ReadAttrFloat(const FXmlNode* Node, const TCHAR* Attr, float& Out, bool& bOverride)
{
    FString Str = Node->GetAttribute(Attr);
    if (Str.IsEmpty()) return false;
    bOverride = true;
    Out = FCString::Atof(*Str);
    return true;
}

bool MjXmlUtils::ReadAttrDouble(const FXmlNode* Node, const TCHAR* Attr, double& Out, bool& bOverride)
{
    FString Str = Node->GetAttribute(Attr);
    if (Str.IsEmpty()) return false;
    bOverride = true;
    Out = FCString::Atod(*Str);
    return true;
}

bool MjXmlUtils::ReadAttrInt(const FXmlNode* Node, const TCHAR* Attr, int32& Out, bool& bOverride)
{
    FString Str = Node->GetAttribute(Attr);
    if (Str.IsEmpty()) return false;
    bOverride = true;
    Out = FCString::Atoi(*Str);
    return true;
}

bool MjXmlUtils::ReadAttrFloatArray(const FXmlNode* Node, const TCHAR* Attr, TArray<float>& Out, bool& bOverride)
{
    FString Str = Node->GetAttribute(Attr);
    if (Str.IsEmpty()) return false;
    bOverride = true;
    ParseFloatArray(Str, Out);
    return true;
}

bool MjXmlUtils::ReadAttrBool(const FXmlNode* Node, const TCHAR* Attr, bool& Out, bool& bOverride)
{
    FString Str = Node->GetAttribute(Attr);
    if (Str.IsEmpty()) return false;
    bOverride = true;
    Out = ParseBool(Str);
    return true;
}

bool MjXmlUtils::ReadAttrString(const FXmlNode* Node, const TCHAR* Attr, FString& Out)
{
    FString Str = Node->GetAttribute(Attr);
    if (Str.IsEmpty()) return false;
    Out = Str;
    return true;
}
