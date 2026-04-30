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
#include "URLabEditor.h"
#include "URLabEditorLogging.h"
#include "MjEditorStyle.h"

DEFINE_LOG_CATEGORY(LogURLabEditor);
#include "PropertyEditorModule.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MjComponentDetailCustomizations.h"
#include "MuJoCo/Components/Physics/MjContactPair.h"
#include "MuJoCo/Components/Physics/MjContactExclude.h"
#include "MuJoCo/Components/Constraints/MjEquality.h"

#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MuJoCo/Components/QuickConvert/MjQuickConvertComponent.h"
#include "ScopedTransaction.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "MuJoCo/Components/Sensors/MjSensor.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Geometry/MjSite.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Components/Physics/MjContactPair.h"
#include "MuJoCo/Components/Physics/MjContactExclude.h"
#include "MuJoCo/Components/Constraints/MjEquality.h"
#include "SMjArticulationOutliner.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Framework/Docking/WorkspaceItem.h"

void FURLabEditorModule::StartupModule()
{
    FMjEditorStyle::Initialize();

    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    // Register geom customization on all subclasses
    {
        TArray<UClass*> GeomClasses;
        GetDerivedClasses(UMjGeom::StaticClass(), GeomClasses, true);
        GeomClasses.Add(UMjGeom::StaticClass());
        for (UClass* Class : GeomClasses)
        {
            PropertyModule.RegisterCustomClassLayout(
                Class->GetFName(),
                FOnGetDetailCustomizationInstance::CreateStatic(&FMjGeomDetailCustomization::MakeInstance));
        }
    }
    // Register actuator customization on all subclasses
    {
        TArray<UClass*> ActuatorClasses;
        GetDerivedClasses(UMjActuator::StaticClass(), ActuatorClasses, true);
        ActuatorClasses.Add(UMjActuator::StaticClass());
        for (UClass* Class : ActuatorClasses)
        {
            PropertyModule.RegisterCustomClassLayout(
                Class->GetFName(),
                FOnGetDetailCustomizationInstance::CreateStatic(&FMjActuatorDetailCustomization::MakeInstance));
        }
    }
    // Register sensor customization on all subclasses
    {
        TArray<UClass*> SensorClasses;
        GetDerivedClasses(UMjSensor::StaticClass(), SensorClasses, true);
        SensorClasses.Add(UMjSensor::StaticClass());
        for (UClass* Class : SensorClasses)
        {
            PropertyModule.RegisterCustomClassLayout(
                Class->GetFName(),
                FOnGetDetailCustomizationInstance::CreateStatic(&FMjSensorDetailCustomization::MakeInstance));
        }
    }
    PropertyModule.RegisterCustomClassLayout(
        UMjContactPair::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FMjContactPairDetailCustomization::MakeInstance));
    PropertyModule.RegisterCustomClassLayout(
        UMjContactExclude::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FMjContactExcludeDetailCustomization::MakeInstance));
    PropertyModule.RegisterCustomClassLayout(
        UMjEquality::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FMjEqualityDetailCustomization::MakeInstance));
    // Register joint customization on all subclasses
    {
        TArray<UClass*> JointClasses;
        GetDerivedClasses(UMjJoint::StaticClass(), JointClasses, true);
        JointClasses.Add(UMjJoint::StaticClass());
        for (UClass* Class : JointClasses)
        {
            PropertyModule.RegisterCustomClassLayout(
                Class->GetFName(),
                FOnGetDetailCustomizationInstance::CreateStatic(&FMjJointDetailCustomization::MakeInstance));
        }
    }
    PropertyModule.RegisterCustomClassLayout(
        UMjBody::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FMjBodyDetailCustomization::MakeInstance));
    PropertyModule.RegisterCustomClassLayout(
        UMjDefault::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FMjDefaultDetailCustomization::MakeInstance));
    PropertyModule.RegisterCustomClassLayout(
        UMjSite::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FMjSiteDetailCustomization::MakeInstance));
    PropertyModule.RegisterCustomClassLayout(
        UMjTendon::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FMjTendonDetailCustomization::MakeInstance));
    PropertyModule.NotifyCustomizationModuleChanged();

    // Register viewport actor context menu extender
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors MenuExtender =
        FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(
            &FURLabEditorModule::OnExtendActorContextMenu);
    auto& Extenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
    Extenders.Add(MenuExtender);
    ViewportContextMenuExtenderHandle = Extenders.Last().GetHandle();

    // Register auto-parenting hook for MuJoCo components
    OnObjectModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddRaw(this, &FURLabEditorModule::OnObjectModified);

    // Register MuJoCo Outliner tab (guard against double registration on hot-reload)
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("MjArticulationOutliner"));
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        TEXT("MjArticulationOutliner"),
        FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
        {
            TSharedRef<SMjArticulationOutliner> Outliner = SNew(SMjArticulationOutliner);

            return SNew(SDockTab)
                .TabRole(ETabRole::NomadTab)
                .Label(FText::FromString(TEXT("MuJoCo Outliner")))
                [
                    Outliner
                ];
        }))
        .SetDisplayName(FText::FromString(TEXT("MuJoCo Outliner")))
        .SetTooltipText(FText::FromString(TEXT("Filtered view of MuJoCo articulation components")));

}

void FURLabEditorModule::ShutdownModule()
{
    FMjEditorStyle::Shutdown();

    FCoreUObjectDelegates::OnObjectModified.Remove(OnObjectModifiedHandle);

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("MjArticulationOutliner"));

    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
    {
        FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
        // Unregister all subclass customizations
        auto UnregisterWithSubclasses = [&](UClass* BaseClass)
        {
            TArray<UClass*> Classes;
            GetDerivedClasses(BaseClass, Classes, true);
            Classes.Add(BaseClass);
            for (UClass* Class : Classes)
            {
                PropertyModule.UnregisterCustomClassLayout(Class->GetFName());
            }
        };
        UnregisterWithSubclasses(UMjGeom::StaticClass());
        UnregisterWithSubclasses(UMjActuator::StaticClass());
        UnregisterWithSubclasses(UMjSensor::StaticClass());
        UnregisterWithSubclasses(UMjJoint::StaticClass());
        PropertyModule.UnregisterCustomClassLayout(UMjContactPair::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomClassLayout(UMjContactExclude::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomClassLayout(UMjEquality::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomClassLayout(UMjBody::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomClassLayout(UMjDefault::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomClassLayout(UMjSite::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomClassLayout(UMjTendon::StaticClass()->GetFName());
    }

    if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
    {
        FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
        LevelEditorModule.GetAllLevelViewportContextMenuExtenders().RemoveAll(
            [this](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& Delegate) {
                return Delegate.GetHandle() == ViewportContextMenuExtenderHandle;
            });
    }
}

TSharedRef<FExtender> FURLabEditorModule::OnExtendActorContextMenu(
    const TSharedRef<FUICommandList> CommandList,
    const TArray<AActor*> SelectedActors)
{
    TSharedRef<FExtender> Extender = MakeShared<FExtender>();

    if (SelectedActors.Num() > 0)
    {
        Extender->AddMenuExtension(
            "ActorControl",
            EExtensionHook::After,
            CommandList,
            FMenuExtensionDelegate::CreateLambda([SelectedActors](FMenuBuilder& MenuBuilder)
            {
                MenuBuilder.AddSubMenu(
                    FText::FromString("MuJoCo Quick Convert"),
                    FText::FromString("Add MjQuickConvertComponent with preset configuration"),
                    FNewMenuDelegate::CreateStatic(&FURLabEditorModule::BuildQuickConvertSubMenu, SelectedActors));
            }));
    }

    return Extender;
}

void FURLabEditorModule::BuildQuickConvertSubMenu(FMenuBuilder& MenuBuilder, TArray<AActor*> SelectedActors)
{
    // Capture actors as weak pointers so they survive across frames
    TArray<TWeakObjectPtr<AActor>> WeakActors;
    for (AActor* Actor : SelectedActors)
    {
        WeakActors.Add(Actor);
    }

    MenuBuilder.AddMenuEntry(
        FText::FromString("Simple Static"),
        FText::FromString("Simple collision, fixed in place (no free joint)"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(&FURLabEditorModule::ApplyQuickConvert, WeakActors, true, false)));

    MenuBuilder.AddMenuEntry(
        FText::FromString("Simple Dynamic"),
        FText::FromString("Simple collision, free to move under physics"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(&FURLabEditorModule::ApplyQuickConvert, WeakActors, false, false)));

    MenuBuilder.AddMenuEntry(
        FText::FromString("Complex Static"),
        FText::FromString("CoACD decomposition, fixed in place (no free joint)"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(&FURLabEditorModule::ApplyQuickConvert, WeakActors, true, true)));

    MenuBuilder.AddMenuEntry(
        FText::FromString("Complex Dynamic"),
        FText::FromString("CoACD decomposition, free to move under physics"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(&FURLabEditorModule::ApplyQuickConvert, WeakActors, false, true)));
}

void FURLabEditorModule::ApplyQuickConvert(TArray<TWeakObjectPtr<AActor>> Actors, bool bStatic, bool bComplex)
{
    FScopedTransaction Transaction(FText::FromString("MuJoCo Quick Convert"));

    int32 Applied = 0;
    for (const TWeakObjectPtr<AActor>& WeakActor : Actors)
    {
        AActor* Actor = WeakActor.Get();
        if (!Actor) continue;

        // Skip if already has a QuickConvert component
        if (Actor->FindComponentByClass<UMjQuickConvertComponent>())
        {
            UE_LOG(LogURLabEditor, Warning, TEXT("Actor '%s' already has MjQuickConvertComponent, skipping."), *Actor->GetName());
            continue;
        }

        Actor->Modify();

        // Set mobility to Movable (required for MuJoCo transform sync)
        if (USceneComponent* Root = Actor->GetRootComponent())
        {
            Root->SetMobility(EComponentMobility::Movable);
        }

        // Create and configure the component
        UMjQuickConvertComponent* Comp = NewObject<UMjQuickConvertComponent>(Actor, NAME_None, RF_Transactional);
        Comp->Static = bStatic;
        Comp->ComplexMeshRequired = bComplex;

        Actor->AddInstanceComponent(Comp);
        Comp->RegisterComponent();

        Actor->GetPackage()->MarkPackageDirty();
        Applied++;
    }

    UE_LOG(LogURLabEditor, Log, TEXT("MuJoCo Quick Convert applied to %d actor(s) [Static=%d, Complex=%d]"),
        Applied, bStatic, bComplex);
}

void FURLabEditorModule::OnObjectModified(UObject* Object)
{
    if (bIsAutoParenting) return;

    USimpleConstructionScript* SCS = Cast<USimpleConstructionScript>(Object);
    if (!SCS) return;

    UBlueprint* BP = SCS->GetBlueprint();
    if (!BP || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(AMjArticulation::StaticClass()))
        return;

    // Defer to next tick so the SCS tree is fully updated before we scan
    TWeakObjectPtr<USimpleConstructionScript> WeakSCS = SCS;
    TWeakObjectPtr<UBlueprint> WeakBP = BP;
    GEditor->GetTimerManager()->SetTimerForNextTick([this, WeakSCS, WeakBP]()
    {
        if (bIsAutoParenting) return;
        USimpleConstructionScript* SCSPtr = WeakSCS.Get();
        UBlueprint* BPPtr = WeakBP.Get();
        if (!SCSPtr || !BPPtr) return;

        TArray<USCS_Node*> AllNodes = SCSPtr->GetAllNodes();
        bIsAutoParenting = true;
        bool bMoved = false;

        for (USCS_Node* Node : AllNodes)
        {
            bMoved |= AutoParentSCSNode(Node, SCSPtr);

        }
        bIsAutoParenting = false;

        if (bMoved)
        {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BPPtr);
        }
    });
}

bool FURLabEditorModule::AutoParentSCSNode(USCS_Node* Node, USimpleConstructionScript* SCS)
{
    if (!Node || !Node->ComponentTemplate) return false;

    // Determine the correct parent folder name for this component type
    FString TargetParentName;
    if (Node->ComponentTemplate->IsA<UMjSensor>())        TargetParentName = TEXT("SensorsRoot");
    else if (Node->ComponentTemplate->IsA<UMjActuator>())  TargetParentName = TEXT("ActuatorsRoot");
    else if (Node->ComponentTemplate->IsA<UMjDefault>())   TargetParentName = TEXT("DefaultsRoot");
    else if (Node->ComponentTemplate->IsA<UMjTendon>())    TargetParentName = TEXT("TendonsRoot");
    else if (Node->ComponentTemplate->IsA<UMjContactPair>() || Node->ComponentTemplate->IsA<UMjContactExclude>())
        TargetParentName = TEXT("ContactsRoot");
    else if (Node->ComponentTemplate->IsA<UMjEquality>())  TargetParentName = TEXT("EqualitiesRoot");
    else return false;

    // Check if already correctly parented
    USCS_Node* CurrentParent = SCS->FindParentNode(Node);
    if (CurrentParent && CurrentParent->GetVariableName().ToString() == TargetParentName)
        return false;

    // Skip components that are under DefaultsRoot — they are default templates
    // and must not be moved to organizational folders
    {
        USCS_Node* Ancestor = CurrentParent;
        while (Ancestor)
        {
            if (Ancestor->GetVariableName().ToString() == TEXT("DefaultsRoot"))
                return false;
            Ancestor = SCS->FindParentNode(Ancestor);
        }
    }

    // Find the target parent node
    USCS_Node* TargetParent = nullptr;
    for (USCS_Node* SearchNode : SCS->GetAllNodes())
    {
        if (SearchNode->GetVariableName().ToString() == TargetParentName)
        {
            TargetParent = SearchNode;
            break;
        }
    }

    if (!TargetParent) return false;

    // Reparent
    if (CurrentParent)
    {
        CurrentParent->RemoveChildNode(Node);
    }
    else
    {
        SCS->RemoveNode(Node, /*bNotify=*/false);
    }
    TargetParent->AddChildNode(Node);

    FString CompName = Node->GetVariableName().ToString();
    UE_LOG(LogURLabEditor, Log, TEXT("[Auto-Parent] Moved '%s' to '%s'"), *CompName, *TargetParentName);
    return true;
}

IMPLEMENT_MODULE(FURLabEditorModule, URLabEditor)
