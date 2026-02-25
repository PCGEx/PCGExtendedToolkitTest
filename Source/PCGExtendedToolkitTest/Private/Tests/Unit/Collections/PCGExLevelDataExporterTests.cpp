// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"

// Shared test helpers
#include "Helpers/PCGExLevelDataExporterTestHelpers.h"

// PCG types
#include "PCGParamData.h"

// PCGEx collection types
#include "Collections/PCGExMeshCollection.h"
#include "Collections/PCGExActorCollection.h"
#include "PCGExCollectionsCommon.h"

/**
 * Tests for UPCGExDefaultLevelDataExporter
 * Covers: ClassifyActor, mesh unification (SM + ISM), tag intersection/delta,
 *         collection generation, material variant tracking
 */

//////////////////////////////////////////////////////////////////////////
// ClassifyActor Tests
//////////////////////////////////////////////////////////////////////////

#pragma region ClassifyActor

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDEClassifyActorTest,
	"PCGEx.Unit.Collections.LevelDataExporter.ClassifyActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDEClassifyActorTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize()) { return false; }

	UStaticMesh* Mesh = FExporterTestScope::LoadEngineMesh();
	if (!Mesh) { AddError(TEXT("Failed to load engine cube mesh")); return false; }

	// Actor with valid StaticMesh -> Mesh
	{
		AActor* Actor = Scope.SpawnTestActor();
		Scope.AddStaticMeshComponent(Actor, Mesh);

		UStaticMeshComponent* OutComp = nullptr;
		EPCGExActorExportType Type = Scope.Exporter->ClassifyActor(Actor, OutComp);
		TestEqual(TEXT("Actor with SM classified as Mesh"), Type, EPCGExActorExportType::Mesh);
		TestTrue(TEXT("OutComp is set"), OutComp != nullptr);
	}

	// Actor without any mesh component -> Actor
	{
		AActor* Actor = Scope.SpawnTestActor();
		UStaticMeshComponent* OutComp = nullptr;
		EPCGExActorExportType Type = Scope.Exporter->ClassifyActor(Actor, OutComp);
		TestEqual(TEXT("Actor without SM classified as Actor"), Type, EPCGExActorExportType::Actor);
	}

	// Actor with SM component but null mesh -> Actor
	{
		AActor* Actor = Scope.SpawnTestActor();
		UStaticMeshComponent* SMC = NewObject<UStaticMeshComponent>(Actor, NAME_None, RF_Transient);
		SMC->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		SMC->RegisterComponent();
		// Don't set a mesh — GetStaticMesh() returns nullptr

		UStaticMeshComponent* OutComp = nullptr;
		EPCGExActorExportType Type = Scope.Exporter->ClassifyActor(Actor, OutComp);
		TestEqual(TEXT("Actor with null mesh classified as Actor"), Type, EPCGExActorExportType::Actor);
	}

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Basic Export Tests (non-collection mode)
//////////////////////////////////////////////////////////////////////////

#pragma region BasicExport

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDEBasicExportTest,
	"PCGEx.Unit.Collections.LevelDataExporter.BasicExport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDEBasicExportTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(false)) { return false; }

	UStaticMesh* MeshA = FExporterTestScope::LoadEngineMesh();
	if (!MeshA) { AddError(TEXT("Failed to load engine mesh")); return false; }

	// 2 mesh actors
	AActor* MeshActor1 = Scope.SpawnTestActor(FTransform(FVector(100, 0, 0)));
	Scope.AddStaticMeshComponent(MeshActor1, MeshA);

	AActor* MeshActor2 = Scope.SpawnTestActor(FTransform(FVector(200, 0, 0)));
	Scope.AddStaticMeshComponent(MeshActor2, MeshA);

	// 1 pure actor (no mesh)
	AActor* PureActor = Scope.SpawnTestActor(FTransform(FVector(300, 0, 0)));

	// Run export
	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	// Verify "Meshes" pin
	const UPCGBasePointData* MeshData = Scope.GetPinPointData(FName(TEXT("Meshes")));
	TestTrue(TEXT("Meshes pin exists"), MeshData != nullptr);
	if (MeshData)
	{
		TestEqual(TEXT("Meshes has 2 points"), MeshData->GetNumPoints(), 2);

		// Verify Mesh attribute (non-collection mode writes FSoftObjectPath)
		const FPCGMetadataAttributeBase* MeshAttrBase = MeshData->ConstMetadata()->GetConstAttribute(FName(TEXT("Mesh")));
		TestTrue(TEXT("Mesh attribute exists"), MeshAttrBase != nullptr);

		// Verify ActorName attribute
		const FPCGMetadataAttributeBase* ActorNameAttrBase = MeshData->ConstMetadata()->GetConstAttribute(FName(TEXT("ActorName")));
		TestTrue(TEXT("ActorName attribute exists on mesh data"), ActorNameAttrBase != nullptr);
	}

	// Verify "Actors" pin
	const UPCGBasePointData* ActorData = Scope.GetPinPointData(FName(TEXT("Actors")));
	TestTrue(TEXT("Actors pin exists"), ActorData != nullptr);
	if (ActorData)
	{
		TestEqual(TEXT("Actors has 1 point"), ActorData->GetNumPoints(), 1);

		// Verify ActorClass attribute (non-collection mode)
		const FPCGMetadataAttributeBase* ClassAttr = ActorData->ConstMetadata()->GetConstAttribute(FName(TEXT("ActorClass")));
		TestTrue(TEXT("ActorClass attribute exists"), ClassAttr != nullptr);

		// Verify InstanceTags attribute exists
		const FPCGMetadataAttributeBase* TagsAttr = ActorData->ConstMetadata()->GetConstAttribute(FName(TEXT("InstanceTags")));
		TestTrue(TEXT("InstanceTags attribute exists on actor data"), TagsAttr != nullptr);
	}

	// Verify NO "Instances" pin exists (unified into Meshes)
	const FPCGTaggedData* InstancesPin = Scope.FindPin(FName(TEXT("Instances")));
	TestTrue(TEXT("No Instances pin"), InstancesPin == nullptr);

	// Verify no CollectionMap pin in non-collection mode
	const FPCGTaggedData* MapPin = Scope.FindPin(FName(TEXT("CollectionMap")));
	TestTrue(TEXT("No CollectionMap pin in non-collection mode"), MapPin == nullptr);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Tag Capture Tests
//////////////////////////////////////////////////////////////////////////

#pragma region TagCapture

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDETagIntersectionTest,
	"PCGEx.Unit.Collections.LevelDataExporter.TagCapture.Intersection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDETagIntersectionTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(false)) { return false; }

	// Actor 1: tags = {TestTag, Shared, UniqueA}
	AActor* Actor1 = Scope.SpawnTestActor(FTransform(FVector(100, 0, 0)));
	Actor1->Tags.Add(FName(TEXT("Shared")));
	Actor1->Tags.Add(FName(TEXT("UniqueA")));

	// Actor 2: tags = {TestTag, Shared, UniqueB}
	AActor* Actor2 = Scope.SpawnTestActor(FTransform(FVector(200, 0, 0)));
	Actor2->Tags.Add(FName(TEXT("Shared")));
	Actor2->Tags.Add(FName(TEXT("UniqueB")));

	// Both are same class (AActor), no mesh -> classified as Actor
	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	const UPCGBasePointData* ActorData = Scope.GetPinPointData(FName(TEXT("Actors")));
	TestTrue(TEXT("Actors pin exists"), ActorData != nullptr);
	if (!ActorData) { return false; }

	TestEqual(TEXT("2 actor points"), ActorData->GetNumPoints(), 2);

	// Read InstanceTags for both points
	// Tag intersection for AActor class: {TestTag, Shared} (both actors have both)
	// Actor1 delta: {UniqueA} (not in intersection)
	// Actor2 delta: {UniqueB} (not in intersection)
	const FPCGMetadataAttribute<FString>* TagsAttr =
		ActorData->ConstMetadata()->GetConstTypedAttribute<FString>(FName(TEXT("InstanceTags")));
	TestTrue(TEXT("InstanceTags attribute exists"), TagsAttr != nullptr);

	if (TagsAttr)
	{
		// Check that each point has its unique tag as delta
		bool bFoundUniqueA = false;
		bool bFoundUniqueB = false;

		for (int32 i = 0; i < ActorData->GetNumPoints(); i++)
		{
			const FString TagStr = TagsAttr->GetValueFromItemKey(ActorData->GetMetadataEntry(i));
			if (TagStr.Contains(TEXT("UniqueA"))) { bFoundUniqueA = true; }
			if (TagStr.Contains(TEXT("UniqueB"))) { bFoundUniqueB = true; }

			// Neither point should have "Shared" or the test tag in delta (they're in intersection)
			TestFalse(*FString::Printf(TEXT("Point %d delta should not contain 'Shared'"), i),
				TagStr.Contains(TEXT("Shared")));
		}

		TestTrue(TEXT("UniqueA found in some point's delta"), bFoundUniqueA);
		TestTrue(TEXT("UniqueB found in some point's delta"), bFoundUniqueB);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDETagIdenticalTest,
	"PCGEx.Unit.Collections.LevelDataExporter.TagCapture.IdenticalTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDETagIdenticalTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(false)) { return false; }

	// Two actors with identical tags
	AActor* Actor1 = Scope.SpawnTestActor(FTransform(FVector(100, 0, 0)));
	Actor1->Tags.Add(FName(TEXT("TagA")));
	Actor1->Tags.Add(FName(TEXT("TagB")));

	AActor* Actor2 = Scope.SpawnTestActor(FTransform(FVector(200, 0, 0)));
	Actor2->Tags.Add(FName(TEXT("TagA")));
	Actor2->Tags.Add(FName(TEXT("TagB")));

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	const UPCGBasePointData* ActorData = Scope.GetPinPointData(FName(TEXT("Actors")));
	if (!ActorData) { return false; }

	const FPCGMetadataAttribute<FString>* TagsAttr =
		ActorData->ConstMetadata()->GetConstTypedAttribute<FString>(FName(TEXT("InstanceTags")));
	TestTrue(TEXT("InstanceTags attribute exists"), TagsAttr != nullptr);

	if (TagsAttr)
	{
		// All tags are in the intersection -> delta should be empty for both points
		for (int32 i = 0; i < ActorData->GetNumPoints(); i++)
		{
			const FString TagStr = TagsAttr->GetValueFromItemKey(ActorData->GetMetadataEntry(i));
			TestTrue(*FString::Printf(TEXT("Point %d has empty delta"), i), TagStr.IsEmpty());
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDETagDisjointTest,
	"PCGEx.Unit.Collections.LevelDataExporter.TagCapture.DisjointTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDETagDisjointTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(false)) { return false; }

	// Two actors with completely different tags (except TestTag)
	AActor* Actor1 = Scope.SpawnTestActor(FTransform(FVector(100, 0, 0)));
	Actor1->Tags.Add(FName(TEXT("OnlyA1")));
	Actor1->Tags.Add(FName(TEXT("OnlyA2")));

	AActor* Actor2 = Scope.SpawnTestActor(FTransform(FVector(200, 0, 0)));
	Actor2->Tags.Add(FName(TEXT("OnlyB1")));

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	const UPCGBasePointData* ActorData = Scope.GetPinPointData(FName(TEXT("Actors")));
	if (!ActorData) { return false; }

	const FPCGMetadataAttribute<FString>* TagsAttr =
		ActorData->ConstMetadata()->GetConstTypedAttribute<FString>(FName(TEXT("InstanceTags")));
	if (!TagsAttr) { return false; }

	// Intersection is {TestTag} (the only common tag). Everything else is delta.
	bool bFoundA1Tags = false;
	bool bFoundB1Tags = false;

	for (int32 i = 0; i < ActorData->GetNumPoints(); i++)
	{
		const FString TagStr = TagsAttr->GetValueFromItemKey(ActorData->GetMetadataEntry(i));

		if (TagStr.Contains(TEXT("OnlyA1")) && TagStr.Contains(TEXT("OnlyA2")))
		{
			bFoundA1Tags = true;
		}
		if (TagStr.Contains(TEXT("OnlyB1")))
		{
			bFoundB1Tags = true;
		}
	}

	TestTrue(TEXT("Actor1's unique tags found in delta"), bFoundA1Tags);
	TestTrue(TEXT("Actor2's unique tags found in delta"), bFoundB1Tags);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// ISM Unification Tests
//////////////////////////////////////////////////////////////////////////

#pragma region ISMUnification

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDEISMUnificationTest,
	"PCGEx.Unit.Collections.LevelDataExporter.MeshUnification.ISMInstances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDEISMUnificationTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(false)) { return false; }

	UStaticMesh* MeshA = FExporterTestScope::LoadEngineMesh();
	if (!MeshA) { AddError(TEXT("Failed to load engine mesh")); return false; }

	// Actor with regular SM component
	AActor* SMActor = Scope.SpawnTestActor(FTransform(FVector(100, 0, 0)));
	Scope.AddStaticMeshComponent(SMActor, MeshA);

	// Actor without SM but with ISM (3 instances)
	// Note: An actor with ISM is classified as Mesh (ISM inherits SM),
	// so it contributes 1 SM point + 3 ISM points = 4 points from this actor
	AActor* ISMActor = Scope.SpawnTestActor(FTransform(FVector(500, 0, 0)));
	TArray<FTransform> Instances = {
		FTransform(FVector(200, 0, 0)),
		FTransform(FVector(300, 0, 0)),
		FTransform(FVector(400, 0, 0))
	};
	Scope.AddISMComponent(ISMActor, MeshA, Instances);

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	// Verify all mesh points on "Meshes" pin
	const UPCGBasePointData* MeshData = Scope.GetPinPointData(FName(TEXT("Meshes")));
	TestTrue(TEXT("Meshes pin exists"), MeshData != nullptr);
	if (MeshData)
	{
		// SMActor: 1 SM point
		// ISMActor: 1 SM point (ISM found by FindComponentByClass<USM>) + 3 ISM instances
		// Total: 5 points
		// Note: Also the SM actor is iterated by the ISM loop but has no ISM component -> 0 ISM points from it
		TestEqual(TEXT("Expected mesh point count (1 SM + 1 SM + 3 ISM)"), MeshData->GetNumPoints(), 5);
	}

	// Verify NO "Instances" pin (unified)
	TestTrue(TEXT("No Instances pin"), Scope.FindPin(FName(TEXT("Instances"))) == nullptr);

	// Verify NO "Actors" pin (both actors classified as Mesh)
	// ISMActor is classified as Mesh because ISM inherits SM
	TestTrue(TEXT("No Actors pin (all classified as Mesh)"), Scope.FindPin(FName(TEXT("Actors"))) == nullptr);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Collection Generation Tests
//////////////////////////////////////////////////////////////////////////

#pragma region CollectionGeneration

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDECollectionGenerationTest,
	"PCGEx.Unit.Collections.LevelDataExporter.CollectionGeneration.MeshAndActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDECollectionGenerationTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(true, false)) { return false; }

	UStaticMesh* MeshA = FExporterTestScope::LoadEngineMesh(TEXT("Cube"));
	UStaticMesh* MeshB = FExporterTestScope::LoadEngineMesh(TEXT("Sphere"));
	if (!MeshA || !MeshB) { AddError(TEXT("Failed to load engine meshes")); return false; }

	// 2 mesh actors with MeshA
	AActor* MA1 = Scope.SpawnTestActor(FTransform(FVector(100, 0, 0)));
	Scope.AddStaticMeshComponent(MA1, MeshA);

	AActor* MA2 = Scope.SpawnTestActor(FTransform(FVector(200, 0, 0)));
	Scope.AddStaticMeshComponent(MA2, MeshA);

	// 1 mesh actor with MeshB
	AActor* MB1 = Scope.SpawnTestActor(FTransform(FVector(300, 0, 0)));
	Scope.AddStaticMeshComponent(MB1, MeshB);

	// 1 pure actor with tags
	AActor* PureActor = Scope.SpawnTestActor(FTransform(FVector(400, 0, 0)));
	PureActor->Tags.Add(FName(TEXT("ActorTag")));

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	// Verify "Meshes" pin with entry hash attribute
	const UPCGBasePointData* MeshData = Scope.GetPinPointData(FName(TEXT("Meshes")));
	TestTrue(TEXT("Meshes pin exists"), MeshData != nullptr);
	if (MeshData)
	{
		TestEqual(TEXT("3 mesh points"), MeshData->GetNumPoints(), 3);

		// Entry hash attribute should exist in collection mode
		const FPCGMetadataAttributeBase* HashAttr = MeshData->ConstMetadata()->GetConstAttribute(
			PCGExCollections::Labels::Tag_EntryIdx);
		TestTrue(TEXT("Entry hash attribute on mesh data"), HashAttr != nullptr);

		// No raw "Mesh" attribute in collection mode
		const FPCGMetadataAttributeBase* MeshAttr = MeshData->ConstMetadata()->GetConstAttribute(FName(TEXT("Mesh")));
		TestTrue(TEXT("No raw Mesh attribute in collection mode"), MeshAttr == nullptr);

		// ActorName attribute should still exist
		const FPCGMetadataAttributeBase* NameAttr = MeshData->ConstMetadata()->GetConstAttribute(FName(TEXT("ActorName")));
		TestTrue(TEXT("ActorName attribute exists"), NameAttr != nullptr);
	}

	// Verify "Actors" pin with entry hash attribute
	const UPCGBasePointData* ActorData = Scope.GetPinPointData(FName(TEXT("Actors")));
	TestTrue(TEXT("Actors pin exists"), ActorData != nullptr);
	if (ActorData)
	{
		TestEqual(TEXT("1 actor point"), ActorData->GetNumPoints(), 1);

		const FPCGMetadataAttributeBase* HashAttr = ActorData->ConstMetadata()->GetConstAttribute(
			PCGExCollections::Labels::Tag_EntryIdx);
		TestTrue(TEXT("Entry hash attribute on actor data"), HashAttr != nullptr);

		// No raw "ActorClass" attribute in collection mode
		const FPCGMetadataAttributeBase* ClassAttr = ActorData->ConstMetadata()->GetConstAttribute(FName(TEXT("ActorClass")));
		TestTrue(TEXT("No raw ActorClass attribute in collection mode"), ClassAttr == nullptr);
	}

	// Verify CollectionMap pin
	const FPCGTaggedData* MapPin = Scope.FindPin(FName(TEXT("CollectionMap")));
	TestTrue(TEXT("CollectionMap pin exists"), MapPin != nullptr);
	if (MapPin)
	{
		TestTrue(TEXT("CollectionMap is UPCGParamData"), Cast<UPCGParamData>(MapPin->Data) != nullptr);
	}

	// Verify embedded mesh collection
	UPCGExMeshCollection* EmbeddedMeshColl = nullptr;
	ForEachObjectWithOuter(Scope.OutputAsset, [&](UObject* Obj)
	{
		if (auto* MC = Cast<UPCGExMeshCollection>(Obj)) { EmbeddedMeshColl = MC; }
	}, false);

	TestTrue(TEXT("Embedded mesh collection exists"), EmbeddedMeshColl != nullptr);
	if (EmbeddedMeshColl)
	{
		TestEqual(TEXT("Mesh collection has 2 entries (MeshA, MeshB)"), EmbeddedMeshColl->Entries.Num(), 2);

		// Verify weights match instance counts
		int32 TotalWeight = 0;
		for (const FPCGExMeshCollectionEntry& Entry : EmbeddedMeshColl->Entries)
		{
			TotalWeight += Entry.Weight;
		}
		TestEqual(TEXT("Total weight = 3 (2 MeshA + 1 MeshB)"), TotalWeight, 3);
	}

	// Verify embedded actor collection with tags
	UPCGExActorCollection* EmbeddedActorColl = nullptr;
	ForEachObjectWithOuter(Scope.OutputAsset, [&](UObject* Obj)
	{
		if (auto* AC = Cast<UPCGExActorCollection>(Obj)) { EmbeddedActorColl = AC; }
	}, false);

	TestTrue(TEXT("Embedded actor collection exists"), EmbeddedActorColl != nullptr);
	if (EmbeddedActorColl)
	{
		TestEqual(TEXT("Actor collection has 1 entry"), EmbeddedActorColl->Entries.Num(), 1);

		// The actor entry should have the intersected tags
		// Single actor -> intersection = its own tags = {TestTag, ActorTag}
		const FPCGExActorCollectionEntry& ActorEntry = EmbeddedActorColl->Entries[0];
		TestTrue(TEXT("Actor entry has TestTag"), ActorEntry.Tags.Contains(TestTag));
		TestTrue(TEXT("Actor entry has ActorTag"), ActorEntry.Tags.Contains(FName(TEXT("ActorTag"))));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDECollectionTagIntersectionTest,
	"PCGEx.Unit.Collections.LevelDataExporter.CollectionGeneration.ActorEntryTagIntersection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDECollectionTagIntersectionTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(true, false)) { return false; }

	// Two actors of same class with overlapping tags
	AActor* Actor1 = Scope.SpawnTestActor(FTransform(FVector(100, 0, 0)));
	Actor1->Tags.Add(FName(TEXT("Common")));
	Actor1->Tags.Add(FName(TEXT("OnlyFirst")));

	AActor* Actor2 = Scope.SpawnTestActor(FTransform(FVector(200, 0, 0)));
	Actor2->Tags.Add(FName(TEXT("Common")));
	Actor2->Tags.Add(FName(TEXT("OnlySecond")));

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	// Find embedded actor collection
	UPCGExActorCollection* EmbeddedActorColl = nullptr;
	ForEachObjectWithOuter(Scope.OutputAsset, [&](UObject* Obj)
	{
		if (auto* AC = Cast<UPCGExActorCollection>(Obj)) { EmbeddedActorColl = AC; }
	}, false);

	TestTrue(TEXT("Embedded actor collection exists"), EmbeddedActorColl != nullptr);
	if (EmbeddedActorColl)
	{
		TestEqual(TEXT("Actor collection has 1 entry (same class)"), EmbeddedActorColl->Entries.Num(), 1);

		const FPCGExActorCollectionEntry& Entry = EmbeddedActorColl->Entries[0];

		// Intersection: {TestTag, Common}
		TestTrue(TEXT("Entry has TestTag (in intersection)"), Entry.Tags.Contains(TestTag));
		TestTrue(TEXT("Entry has Common (in intersection)"), Entry.Tags.Contains(FName(TEXT("Common"))));
		TestFalse(TEXT("Entry does NOT have OnlyFirst"), Entry.Tags.Contains(FName(TEXT("OnlyFirst"))));
		TestFalse(TEXT("Entry does NOT have OnlySecond"), Entry.Tags.Contains(FName(TEXT("OnlySecond"))));
	}

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Material Variant Tests
//////////////////////////////////////////////////////////////////////////

#pragma region MaterialVariants

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDEMaterialVariantsTest,
	"PCGEx.Unit.Collections.LevelDataExporter.CollectionGeneration.MaterialVariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDEMaterialVariantsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(true, true)) { return false; }

	UStaticMesh* Mesh = FExporterTestScope::LoadEngineMesh();
	if (!Mesh) { AddError(TEXT("Failed to load engine mesh")); return false; }
	UMaterial* MatA = Scope.CreateTestMaterial(TEXT("MaterialA"));
	UMaterial* MatB = Scope.CreateTestMaterial(TEXT("MaterialB"));

	// Two actors with same mesh but different material overrides
	AActor* Actor1 = Scope.SpawnTestActor(FTransform(FVector(100, 0, 0)));
	UStaticMeshComponent* SMC1 = Scope.AddStaticMeshComponent(Actor1, Mesh);
	if (SMC1) { SMC1->SetMaterial(0, MatA); }

	AActor* Actor2 = Scope.SpawnTestActor(FTransform(FVector(200, 0, 0)));
	UStaticMeshComponent* SMC2 = Scope.AddStaticMeshComponent(Actor2, Mesh);
	if (SMC2) { SMC2->SetMaterial(0, MatB); }

	// Third actor with same material as first (should dedup)
	AActor* Actor3 = Scope.SpawnTestActor(FTransform(FVector(300, 0, 0)));
	UStaticMeshComponent* SMC3 = Scope.AddStaticMeshComponent(Actor3, Mesh);
	if (SMC3) { SMC3->SetMaterial(0, MatA); }

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	// Find embedded mesh collection
	UPCGExMeshCollection* EmbeddedMeshColl = nullptr;
	ForEachObjectWithOuter(Scope.OutputAsset, [&](UObject* Obj)
	{
		if (auto* MC = Cast<UPCGExMeshCollection>(Obj)) { EmbeddedMeshColl = MC; }
	}, false);

	TestTrue(TEXT("Embedded mesh collection exists"), EmbeddedMeshColl != nullptr);
	if (EmbeddedMeshColl)
	{
		TestEqual(TEXT("1 mesh entry (same mesh, variants)"), EmbeddedMeshColl->Entries.Num(), 1);

		const FPCGExMeshCollectionEntry& Entry = EmbeddedMeshColl->Entries[0];
		TestEqual(TEXT("Weight = 3 (all 3 actors)"), Entry.Weight, 3);

		// Should have Multi material variants (2 unique material combos: MatA and MatB)
		TestEqual(TEXT("Material variants mode is Multi"),
			Entry.MaterialVariants, EPCGExMaterialVariantsMode::Multi);
		TestEqual(TEXT("2 material variant entries"),
			Entry.MaterialOverrideVariantsList.Num(), 2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDENoMaterialVariantsTest,
	"PCGEx.Unit.Collections.LevelDataExporter.CollectionGeneration.NoMaterialVariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDENoMaterialVariantsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(true, true)) { return false; }

	UStaticMesh* Mesh = FExporterTestScope::LoadEngineMesh();
	if (!Mesh) { AddError(TEXT("Failed to load engine mesh")); return false; }
	UMaterial* Mat = Scope.CreateTestMaterial(TEXT("SharedMaterial"));

	// Two actors with same mesh AND same material override -> only 1 variant
	AActor* Actor1 = Scope.SpawnTestActor(FTransform(FVector(100, 0, 0)));
	UStaticMeshComponent* SMC1 = Scope.AddStaticMeshComponent(Actor1, Mesh);
	if (SMC1) { SMC1->SetMaterial(0, Mat); }

	AActor* Actor2 = Scope.SpawnTestActor(FTransform(FVector(200, 0, 0)));
	UStaticMeshComponent* SMC2 = Scope.AddStaticMeshComponent(Actor2, Mesh);
	if (SMC2) { SMC2->SetMaterial(0, Mat); }

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExMeshCollection* EmbeddedMeshColl = nullptr;
	ForEachObjectWithOuter(Scope.OutputAsset, [&](UObject* Obj)
	{
		if (auto* MC = Cast<UPCGExMeshCollection>(Obj)) { EmbeddedMeshColl = MC; }
	}, false);

	TestTrue(TEXT("Embedded mesh collection exists"), EmbeddedMeshColl != nullptr);
	if (EmbeddedMeshColl)
	{
		const FPCGExMeshCollectionEntry& Entry = EmbeddedMeshColl->Entries[0];

		// Only 1 unique material combo -> should NOT be Multi
		// (code only sets Multi when UniqueVariantMaterials.Num() > 1)
		TestEqual(TEXT("Material variants mode is None (single variant)"),
			Entry.MaterialVariants, EPCGExMaterialVariantsMode::None);
		TestEqual(TEXT("No variant list entries"),
			Entry.MaterialOverrideVariantsList.Num(), 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDEMaterialCaptureDisabledTest,
	"PCGEx.Unit.Collections.LevelDataExporter.CollectionGeneration.MaterialCaptureDisabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDEMaterialCaptureDisabledTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(true, false)) { return false; } // bCaptureMaterialOverrides = false

	UStaticMesh* Mesh = FExporterTestScope::LoadEngineMesh();
	if (!Mesh) { AddError(TEXT("Failed to load engine mesh")); return false; }
	UMaterial* MatA = Scope.CreateTestMaterial(TEXT("NoCaptMatA"));
	UMaterial* MatB = Scope.CreateTestMaterial(TEXT("NoCaptMatB"));

	// Different materials but capture disabled
	AActor* Actor1 = Scope.SpawnTestActor(FTransform(FVector(100, 0, 0)));
	UStaticMeshComponent* SMC1 = Scope.AddStaticMeshComponent(Actor1, Mesh);
	if (SMC1) { SMC1->SetMaterial(0, MatA); }

	AActor* Actor2 = Scope.SpawnTestActor(FTransform(FVector(200, 0, 0)));
	UStaticMeshComponent* SMC2 = Scope.AddStaticMeshComponent(Actor2, Mesh);
	if (SMC2) { SMC2->SetMaterial(0, MatB); }

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExMeshCollection* EmbeddedMeshColl = nullptr;
	ForEachObjectWithOuter(Scope.OutputAsset, [&](UObject* Obj)
	{
		if (auto* MC = Cast<UPCGExMeshCollection>(Obj)) { EmbeddedMeshColl = MC; }
	}, false);

	TestTrue(TEXT("Embedded mesh collection exists"), EmbeddedMeshColl != nullptr);
	if (EmbeddedMeshColl)
	{
		const FPCGExMeshCollectionEntry& Entry = EmbeddedMeshColl->Entries[0];

		// Material capture disabled -> no variants regardless of material differences
		TestEqual(TEXT("No material variants when capture disabled"),
			Entry.MaterialVariants, EPCGExMaterialVariantsMode::None);
		TestEqual(TEXT("No variant list when capture disabled"),
			Entry.MaterialOverrideVariantsList.Num(), 0);
	}

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Descriptor Population Tests
//////////////////////////////////////////////////////////////////////////

#pragma region DescriptorPopulation

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDEDescriptorPopulationTest,
	"PCGEx.Unit.Collections.LevelDataExporter.CollectionGeneration.DescriptorPopulation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDEDescriptorPopulationTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(true, false)) { return false; }

	UStaticMesh* Mesh = FExporterTestScope::LoadEngineMesh();
	if (!Mesh) { AddError(TEXT("Failed to load engine mesh")); return false; }

	// Create actor with mesh component that has specific settings
	AActor* Actor = Scope.SpawnTestActor(FTransform(FVector(100, 0, 0)));
	UStaticMeshComponent* SMC = Scope.AddStaticMeshComponent(Actor, Mesh);

	if (SMC)
	{
		// Set some component properties that descriptors should capture
		SMC->SetCastShadow(false);
	}

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExMeshCollection* EmbeddedMeshColl = nullptr;
	ForEachObjectWithOuter(Scope.OutputAsset, [&](UObject* Obj)
	{
		if (auto* MC = Cast<UPCGExMeshCollection>(Obj)) { EmbeddedMeshColl = MC; }
	}, false);

	TestTrue(TEXT("Embedded mesh collection exists"), EmbeddedMeshColl != nullptr);
	if (EmbeddedMeshColl && EmbeddedMeshColl->Entries.Num() > 0)
	{
		const FPCGExMeshCollectionEntry& Entry = EmbeddedMeshColl->Entries[0];

		// ISM descriptor should have been populated from the source component
		// The exact values depend on InitFrom behavior, but the mesh path should be set
		TestTrue(TEXT("ISM descriptor static mesh matches"),
			Entry.ISMDescriptor.StaticMesh.ToSoftObjectPath() == FSoftObjectPath(Mesh));
	}

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Empty / Edge Case Tests
//////////////////////////////////////////////////////////////////////////

#pragma region EdgeCases

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDEEmptyWorldTest,
	"PCGEx.Unit.Collections.LevelDataExporter.EdgeCases.NoMatchingActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDEEmptyWorldTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(false)) { return false; }

	// Don't spawn any test actors
	const bool bResult = Scope.RunExport();

	// Export should return false when no qualifying actors found
	TestFalse(TEXT("Export returns false with no matching actors"), bResult);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDEOnlyMeshActorsTest,
	"PCGEx.Unit.Collections.LevelDataExporter.EdgeCases.OnlyMeshActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDEOnlyMeshActorsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(false)) { return false; }

	UStaticMesh* Mesh = FExporterTestScope::LoadEngineMesh();
	if (!Mesh) { AddError(TEXT("Failed to load engine mesh")); return false; }
	AActor* Actor = Scope.SpawnTestActor();
	Scope.AddStaticMeshComponent(Actor, Mesh);

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	// Should have Meshes pin but no Actors pin
	TestTrue(TEXT("Meshes pin exists"), Scope.FindPin(FName(TEXT("Meshes"))) != nullptr);
	TestTrue(TEXT("No Actors pin"), Scope.FindPin(FName(TEXT("Actors"))) == nullptr);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDEOnlyPureActorsTest,
	"PCGEx.Unit.Collections.LevelDataExporter.EdgeCases.OnlyPureActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDEOnlyPureActorsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(false)) { return false; }

	// Spawn actor without mesh
	Scope.SpawnTestActor();

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	// Should have Actors pin but no Meshes pin
	TestTrue(TEXT("No Meshes pin"), Scope.FindPin(FName(TEXT("Meshes"))) == nullptr);
	TestTrue(TEXT("Actors pin exists"), Scope.FindPin(FName(TEXT("Actors"))) != nullptr);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Entry Hash Consistency Tests
//////////////////////////////////////////////////////////////////////////

#pragma region EntryHashConsistency

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLDEEntryHashConsistencyTest,
	"PCGEx.Unit.Collections.LevelDataExporter.CollectionGeneration.EntryHashConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLDEEntryHashConsistencyTest::RunTest(const FString& Parameters)
{
	using namespace PCGExLevelDataExporterTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(true, false)) { return false; }

	UStaticMesh* MeshA = FExporterTestScope::LoadEngineMesh(TEXT("Cube"));
	UStaticMesh* MeshB = FExporterTestScope::LoadEngineMesh(TEXT("Sphere"));
	if (!MeshA || !MeshB) { AddError(TEXT("Failed to load engine meshes")); return false; }

	// 2 actors with MeshA, 1 actor with MeshB
	AActor* A1 = Scope.SpawnTestActor(FTransform(FVector(100, 0, 0)));
	Scope.AddStaticMeshComponent(A1, MeshA);
	AActor* A2 = Scope.SpawnTestActor(FTransform(FVector(200, 0, 0)));
	Scope.AddStaticMeshComponent(A2, MeshA);
	AActor* B1 = Scope.SpawnTestActor(FTransform(FVector(300, 0, 0)));
	Scope.AddStaticMeshComponent(B1, MeshB);

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	const UPCGBasePointData* MeshData = Scope.GetPinPointData(FName(TEXT("Meshes")));
	TestTrue(TEXT("Meshes pin exists"), MeshData != nullptr);
	if (!MeshData) { return false; }

	TestEqual(TEXT("3 mesh points"), MeshData->GetNumPoints(), 3);

	const FPCGMetadataAttribute<int64>* HashAttr =
		MeshData->ConstMetadata()->GetConstTypedAttribute<int64>(PCGExCollections::Labels::Tag_EntryIdx);
	TestTrue(TEXT("Entry hash attribute exists"), HashAttr != nullptr);

	if (HashAttr)
	{
		// Read all hashes
		TArray<int64> Hashes;
		for (int32 i = 0; i < 3; i++)
		{
			Hashes.Add(HashAttr->GetValueFromItemKey(MeshData->GetMetadataEntry(i)));
		}

		// All hashes should be non-zero
		for (int32 i = 0; i < 3; i++)
		{
			TestTrue(*FString::Printf(TEXT("Hash %d is non-zero"), i), Hashes[i] != 0);
		}

		// Points with same mesh should have same hash
		// Points with different mesh should have different hash
		// Note: We don't know the order, but we know 2 have MeshA and 1 has MeshB
		// Find which are same and which are different
		if (Hashes[0] == Hashes[1])
		{
			// First two are same mesh
			TestTrue(TEXT("Different mesh -> different hash"), Hashes[0] != Hashes[2]);
		}
		else if (Hashes[0] == Hashes[2])
		{
			// First and third are same mesh
			TestTrue(TEXT("Different mesh -> different hash"), Hashes[0] != Hashes[1]);
		}
		else if (Hashes[1] == Hashes[2])
		{
			// Second and third are same mesh
			TestTrue(TEXT("Different mesh -> different hash"), Hashes[0] != Hashes[1]);
		}
		else
		{
			// All different - this shouldn't happen (two points should share a hash)
			TestTrue(TEXT("At least two points should share a hash"), false);
		}
	}

	return true;
}

#pragma endregion
