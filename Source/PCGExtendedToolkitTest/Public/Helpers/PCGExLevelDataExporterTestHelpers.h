// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

// Exporter under test
#include "Helpers/PCGExDefaultLevelDataExporter.h"

// UE types
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/Material.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

// PCG types
#include "PCGDataAsset.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

/**
 * Shared RAII scope and helpers for UPCGExDefaultLevelDataExporter tests.
 * Used by PCGExLevelDataExporterTests.cpp and PCGExPropertyDeltaTests.cpp.
 */
namespace PCGExLevelDataExporterTestHelpers
{
	const FName TestTag = FName(TEXT("PCGExTest_LDE"));

	/** RAII scope that manages test actors and cleanup */
	struct FExporterTestScope
	{
		UWorld* World = nullptr;
		TArray<AActor*> SpawnedActors;
		UPCGDataAsset* OutputAsset = nullptr;
		UPCGExDefaultLevelDataExporter* Exporter = nullptr;

		bool Initialize(bool bGenerateCollections = false, bool bCaptureMaterials = true)
		{
#if WITH_EDITOR
			if (!GEditor) { return false; }
			World = GEditor->GetEditorWorldContext().World();
			if (!World) { return false; }

			OutputAsset = NewObject<UPCGDataAsset>(GetTransientPackage(), NAME_None, RF_Transient);
			if (!OutputAsset) { return false; }

			Exporter = NewObject<UPCGExDefaultLevelDataExporter>(GetTransientPackage(), NAME_None, RF_Transient);
			if (!Exporter) { return false; }

			// Filter to only test actors
			Exporter->IncludeTags.Add(TestTag);
			Exporter->bGenerateCollections = bGenerateCollections;
			Exporter->bCaptureMaterialOverrides = bCaptureMaterials;

			return true;
#else
			return false;
#endif
		}

		AActor* SpawnTestActor(const FTransform& Transform = FTransform::Identity)
		{
			if (!World) { return nullptr; }

			FActorSpawnParameters SpawnParams;
			SpawnParams.Name = MakeUniqueObjectName(World, AActor::StaticClass(), FName(TEXT("PCGExTestLDEActor")));
			SpawnParams.ObjectFlags = RF_Transient;
			SpawnParams.bHideFromSceneOutliner = true;
			// Note: NOT setting bTemporaryEditorActor — it sets bIsEditorOnlyActor which the exporter skips
			AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), Transform, SpawnParams);
			if (!Actor) { return nullptr; }

			USceneComponent* Root = NewObject<USceneComponent>(Actor, NAME_None, RF_Transient);
			Actor->SetRootComponent(Root);
			Root->RegisterComponent();

			Actor->Tags.Add(TestTag);
			SpawnedActors.Add(Actor);
			return Actor;
		}

		/** Load an engine built-in mesh. Uses /Engine/BasicShapes/ cube/sphere/cylinder. */
		static UStaticMesh* LoadEngineMesh(const TCHAR* Shape = TEXT("Cube"))
		{
			const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape);
			return LoadObject<UStaticMesh>(nullptr, *Path);
		}

		UMaterial* CreateTestMaterial(const FString& Name = TEXT("TestMaterial"))
		{
			return NewObject<UMaterial>(GetTransientPackage(), *Name, RF_Transient);
		}

		UStaticMeshComponent* AddStaticMeshComponent(AActor* Actor, UStaticMesh* Mesh)
		{
			if (!Actor || !Mesh) { return nullptr; }
			UStaticMeshComponent* SMC = NewObject<UStaticMeshComponent>(Actor, NAME_None, RF_Transient);
			SMC->SetStaticMesh(Mesh);
			SMC->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			SMC->RegisterComponent();
			return SMC;
		}

		UInstancedStaticMeshComponent* AddISMComponent(AActor* Actor, UStaticMesh* Mesh, const TArray<FTransform>& InstanceTransforms)
		{
			if (!Actor || !Mesh) { return nullptr; }
			UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(Actor, NAME_None, RF_Transient);
			ISM->SetStaticMesh(Mesh);
			ISM->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			ISM->RegisterComponent();

			for (const FTransform& T : InstanceTransforms)
			{
				ISM->AddInstance(T, false);
			}

			return ISM;
		}

		bool RunExport()
		{
			if (!Exporter || !World || !OutputAsset) { return false; }
			return Exporter->ExportLevelData_Implementation(World, OutputAsset);
		}

		const FPCGTaggedData* FindPin(FName PinName) const
		{
			if (!OutputAsset) { return nullptr; }
			for (const FPCGTaggedData& TD : OutputAsset->Data.TaggedData)
			{
				if (TD.Pin == PinName) { return &TD; }
			}
			return nullptr;
		}

		const UPCGBasePointData* GetPinPointData(FName PinName) const
		{
			const FPCGTaggedData* TD = FindPin(PinName);
			if (!TD) { return nullptr; }
			return Cast<UPCGBasePointData>(TD->Data);
		}

		template <typename T>
		T ReadAttribute(const UPCGBasePointData* Data, FName AttrName, int32 Index, T Default = T{}) const
		{
			if (!Data || !Data->ConstMetadata()) { return Default; }
			const FPCGMetadataAttribute<T>* Attr = Data->ConstMetadata()->GetConstTypedAttribute<T>(AttrName);
			if (!Attr) { return Default; }
			if (Index < 0 || Index >= Data->GetNumPoints()) { return Default; }
			return Attr->GetValueFromItemKey(Data->GetMetadataEntry(Index));
		}

		void Cleanup()
		{
			for (AActor* Actor : SpawnedActors)
			{
				if (Actor) { Actor->Destroy(); }
			}
			SpawnedActors.Empty();
		}

		~FExporterTestScope()
		{
			Cleanup();
		}
	};
}
