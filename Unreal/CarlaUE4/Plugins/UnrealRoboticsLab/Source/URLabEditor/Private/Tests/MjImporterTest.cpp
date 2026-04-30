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

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MujocoGenerationAction.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "mujoco/mujoco.h"
#include "XmlFile.h"
#include "Misc/Paths.h"
#include "Internationalization/Regex.h"
#include "Kismet2/KismetEditorUtilities.h"

// Define a test with flags to run in Editor
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjImporterValidationTest, "URLab.MuJoCo.Importer.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

FString SanitizeXmlForComparison(const FString& InXml, const FString& PrefixToStrip)
{
    FString OutXml = InXml;
    
    // 1. Strip the specific actor prefix surgically (e.g. TempBP_0_C_0_)
    if (!PrefixToStrip.IsEmpty())
    {
        OutXml = OutXml.Replace(*PrefixToStrip, TEXT(""));
    }

    // 2. Strip auto-generated names produced by the importer for unnamed XML elements.
    // Covers new contextual names (Geom_Box, HingeJoint, etc.) and legacy AUTONAME_*.
    TArray<FRegexPattern> AutoNamePatterns;
    AutoNamePatterns.Emplace(TEXT(" name=\"AUTONAME_[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"Geom_[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"[A-Z][a-z]*Joint[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"FreeJoint[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"Site[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"Inertial[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"Camera[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"[A-Z][a-z]*Sensor[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"[A-Z][a-z]*Actuator[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"[A-Z][a-z]*Tendon[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"[a-zA-Z_]*_Body[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"[a-zA-Z_]*_Frame[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"ContactPair_[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"ContactExclude_[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"Eq_[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"Keyframe[^\"]*\""));
    AutoNamePatterns.Emplace(TEXT(" name=\"Default[A-Z][^\"]*\""));

    for (const FRegexPattern& Pattern : AutoNamePatterns)
    {
        FRegexMatcher Matcher(Pattern, OutXml);
        while (Matcher.FindNext())
        {
            OutXml = OutXml.Replace(*Matcher.GetCaptureGroup(0), TEXT(""));
            Matcher = FRegexMatcher(Pattern, OutXml);
        }
    }

    // 3. Collapse whitespace and newlines for robust comparison
    OutXml = OutXml.Replace(TEXT("\n"), TEXT(" "));
    OutXml = OutXml.Replace(TEXT("\r"), TEXT(" "));
    OutXml = OutXml.Replace(TEXT("\t"), TEXT(" "));
    while (OutXml.Contains(TEXT("  ")))
    {
        OutXml = OutXml.Replace(TEXT("  "), TEXT(" "));
    }

    // 4. Remove empty default tags strictly (e.g. <default class="foo"></default> or <default></default>)
    // Also handle self-closing ones: <default class="foo" />
    OutXml = OutXml.Replace(TEXT("<default> </default>"), TEXT("<default></default>"));
    
    // First, remove self-closing empty defaults
    {
        const FRegexPattern EmptySelfClosingPattern(TEXT("<default[^>]*/>"));
        FRegexMatcher Matcher(EmptySelfClosingPattern, OutXml);
        while (Matcher.FindNext())
        {
            OutXml = OutXml.Replace(*Matcher.GetCaptureGroup(0), TEXT(""));
            Matcher = FRegexMatcher(EmptySelfClosingPattern, OutXml);
        }
    }

    // Then, remove empty tag pairs
    const FRegexPattern EmptyDefaultPattern(TEXT("<default[^>]*></default>"));
    FRegexMatcher EmptyDefaultMatcher(EmptyDefaultPattern, OutXml);
    while (EmptyDefaultMatcher.FindNext())
    {
        OutXml = OutXml.Replace(*EmptyDefaultMatcher.GetCaptureGroup(0), TEXT(""));
        EmptyDefaultMatcher = FRegexMatcher(EmptyDefaultPattern, OutXml);
    }
    
    // 5. Promote children of <default> and <default class="main">
    auto StripTagPair = [&](const FString& TagToStrip) {
        int32 TagPos = OutXml.Find(TagToStrip);
        while (TagPos != INDEX_NONE)
        {
            // Find matching closure </default>
            int32 OpenCount = 0;
            int32 ClosurePos = INDEX_NONE;
            
            // Search from TagPos
            for (int32 i = TagPos; i <= OutXml.Len() - 10; ++i)
            {
                if (OutXml.Mid(i, 8) == TEXT("<default"))
                {
                    OpenCount++;
                    // Skip to end of tag
                    while (i < OutXml.Len() && OutXml[i] != '>') i++;
                }
                else if (OutXml.Mid(i, 10) == TEXT("</default>"))
                {
                    OpenCount--;
                    if (OpenCount == 0)
                    {
                        ClosurePos = i;
                        break;
                    }
                    i += 9;
                }
            }

            if (ClosurePos != INDEX_NONE)
            {
                OutXml.RemoveAt(ClosurePos, 10);
                OutXml.RemoveAt(TagPos, TagToStrip.Len());
            }
            else
            {
                break; // Safety break
            }
            TagPos = OutXml.Find(TagToStrip);
        }
    };

    StripTagPair(TEXT("<default class=\"main\">"));
    StripTagPair(TEXT("<default>"));

    // Cleanup any resulting double spaces
    while (OutXml.Contains(TEXT("  ")))
    {
        OutXml = OutXml.Replace(TEXT("  "), TEXT(" "));
    }
    
    return OutXml.TrimStartAndEnd();
}

bool FMjImporterValidationTest::RunTest(const FString& Parameters)
{
    struct FTestCase
    {
        FString Name;
        FString XmlString;
    };

    TArray<FTestCase> TestCases = {
        {
            TEXT("Simple Geom and Joint"),
            TEXT(R"(
<mujoco>
  <worldbody>
    <body name="b1">
      <joint name="j1" type="hinge" axis="0 1 0"/>
      <geom type="box" size="1 1 1" rgba="1 0 0 1"/>
    </body>
  </worldbody>
</mujoco>
            )")
        	
        },
    	
{
	TEXT("Simple Geom with mass and joint with ref"),
	TEXT(R"(
<mujoco>
 <compiler angle="radian"/>
  <worldbody>
    <body name="b1">
      <joint name="j1" type="hinge" axis="0 1 0" ref="0.2"/>
      <geom type="box" size="1 1 1" rgba="1 0 0 1" mass="1000"/>
    </body>
  </worldbody>
</mujoco>
            )")
},
{
            TEXT("Partial Actuator Arrays"),
            TEXT(R"(
<mujoco>
  <worldbody>
    <body name="b1">
      <joint name="j1" type="hinge"/>
      <geom name="g1" size="1"/>
    </body>
  </worldbody>
  <actuator>
    <motor joint="j1" ctrlrange="-1 1"/>
  </actuator>
</mujoco>
            )")
        },
        {
            TEXT("Allows Spaces"),
            TEXT(R"(
<mujoco>
  <worldbody>
    <body>
      <geom size="1" axisangle="1 0   0 0 "/>
    </body>
  </worldbody>
</mujoco>
            )")
        },
        {
            TEXT("Parse Polycoef"),
            TEXT(R"(
<mujoco>
  <worldbody>
    <body>
      <joint name="0"/>
      <geom size="1"/>
    </body>
    <body>
      <joint name="1"/>
      <geom size="1"/>
    </body>
  </worldbody>
  <equality>
    <joint joint1="0" joint2="1"/>
    <joint joint1="0" joint2="1" polycoef="2"/>
    <joint joint1="0" joint2="1" polycoef="3 4"/>
    <joint joint1="0" joint2="1" polycoef="5 6 7 8 9"/>
  </equality>
</mujoco>
            )")
        },
       
    };

    char szError[1000];
    const int N_XML_BUFFER = 10000;
    char szGtXml[N_XML_BUFFER];
    char szUeXml[N_XML_BUFFER];

    for (const FTestCase& TestCase : TestCases)
    {
        AddInfo(TEXT("=================================================="));
        AddInfo(FString::Printf(TEXT("TEST CASE: %s"), *TestCase.Name));

        // Clear buffers
        FMemory::Memzero(szGtXml, N_XML_BUFFER);
        FMemory::Memzero(szUeXml, N_XML_BUFFER);
        FMemory::Memzero(szError, sizeof(szError));

        // 1. Ground Truth Generation
        mjSpec* gt_spec = mj_parseXMLString(TCHAR_TO_UTF8(*TestCase.XmlString), nullptr, szError, sizeof(szError));
        if (!gt_spec)
        {
            AddError(FString::Printf(TEXT("Ground Truth Parse Failed: %hs"), szError));
            continue;
        }

        mj_compile(gt_spec,nullptr);
        int gt_save_res = mj_saveXMLString(gt_spec, szGtXml, N_XML_BUFFER, szError, sizeof(szError));
        AddInfo(FString::Printf(TEXT("mj_saveXMLString(GT) returned: %d. Error: %hs"), gt_save_res, szError));
        
        FString GtXmlString = FString(UTF8_TO_TCHAR(szGtXml));

        // 2. Unreal Generation
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        WorldContext.SetCurrentWorld(World);

        UBlueprint* TempBP = FKismetEditorUtilities::CreateBlueprint(
            AMjArticulation::StaticClass(),
            GetTransientPackage(),
            MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("TempBP")),
            BPTYPE_Normal,
            UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass()
        );

        if (!TempBP)
        {
            AddError(TEXT("Failed to create temporary blueprint."));
            mj_deleteSpec(gt_spec);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
            continue;
        }

        FXmlFile* XmlFile = new FXmlFile(TestCase.XmlString, EConstructMethod::ConstructFromBuffer);
        if (!XmlFile->IsValid())
        {
            AddError(TEXT("FXmlFile Failed to parse test string buffer"));
            mj_deleteSpec(gt_spec);
            delete XmlFile;
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
            continue;
        }

        UMujocoGenerationAction* GenAction = NewObject<UMujocoGenerationAction>();
        GenAction->GenerateForBlueprintXml(TempBP, TEXT(""), XmlFile);
        FKismetEditorUtilities::CompileBlueprint(TempBP);

        if (!TempBP->GeneratedClass)
        {
            AddError(TEXT("Blueprint compilation failed."));
            mj_deleteSpec(gt_spec);
            delete XmlFile;
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
            continue;
        }

        FActorSpawnParameters SpawnParams;
        AMjArticulation* SpawnedRobot = World->SpawnActor<AMjArticulation>(TempBP->GeneratedClass, SpawnParams);

        if (!SpawnedRobot)
        {
            AddError(TEXT("Failed to spawn generated TempBP articulation"));
        }
        else
        {
            mjVFS* TempVfs = new mjVFS();
            mj_defaultVFS(TempVfs);
            
            mjSpec* UeSpec = mj_makeSpec();
            UeSpec->compiler.degree = false;
            
            SpawnedRobot->Setup(UeSpec, TempVfs);
            FString ActorPrefix = SpawnedRobot->GetName() + TEXT("_");
            
            mj_compile(UeSpec, TempVfs);
            FMemory::Memzero(szError, sizeof(szError));
            int ue_save_res = mj_saveXMLString(UeSpec, szUeXml, N_XML_BUFFER, szError, sizeof(szError));
            AddInfo(FString::Printf(TEXT("mj_saveXMLString(UE) returned: %d. Error: %hs"), ue_save_res, szError));

            FString UeXmlString = FString(UTF8_TO_TCHAR(szUeXml));

            // 3. Comparison
            FString SanitizedGt = SanitizeXmlForComparison(GtXmlString, TEXT("")); // GT doesn't have the TempBP prefix
            FString SanitizedUe = SanitizeXmlForComparison(UeXmlString, ActorPrefix);

            AddInfo(TEXT("--- Sanitized Ground Truth ---"));
            AddInfo(SanitizedGt.IsEmpty() ? TEXT("[EMPTY]") : SanitizedGt);
            AddInfo(TEXT("--- Sanitized Unreal Export ---"));
            AddInfo(SanitizedUe.IsEmpty() ? TEXT("[EMPTY]") : SanitizedUe);

            if (SanitizedUe != SanitizedGt)
            {
                AddError(FString::Printf(TEXT("XML Comparison Failed for %s. Lengths: SanitizedGT=%d, SanitizedUE=%d"), *TestCase.Name, SanitizedGt.Len(), SanitizedUe.Len()));
            }
            else
            {
                AddInfo(FString::Printf(TEXT("XML Comparison Passed for %s"), *TestCase.Name));
            }
            
            mj_deleteSpec(UeSpec);
            mj_deleteVFS(TempVfs);
            delete TempVfs;
        }

        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
        mj_deleteSpec(gt_spec);
        delete XmlFile;
        AddInfo(TEXT("=================================================="));
    }

    return true;
}
