// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"

// Shared test helpers
#include "Helpers/PCGExLevelDataExporterTestHelpers.h"

// Test actor
#include "Helpers/PCGExPropertyDeltaTestActor.h"

// PCGEx collection types
#include "Collections/PCGExActorCollection.h"
#include "Collections/PCGExMeshCollection.h"

/**
 * Tests for per-instance actor property delta capture and dedup.
 * Validates: CDO diff serialization, dedup key with delta hash,
 *            entry separation by property config, feature toggle, mesh exclusion.
 *
 * Uses APCGExPropertyDeltaTestActor (TestHealth=100, TestRadius=50, TestDisplayName="Default").
 * Actors are spawned WITHOUT tags and filtered by IncludeClasses to avoid tag-related delta noise.
 */

namespace PCGExPropertyDeltaTestHelpers
{
	using namespace PCGExLevelDataExporterTestHelpers;

	/** Configure an exporter scope for property delta testing.
	 *  Uses class filtering (not tags) so the actor's Tags UPROPERTY stays CDO-identical. */
	bool SetupScope(FExporterTestScope& Scope, bool bEnableDeltas = true, bool bGenCollections = true)
	{
		if (!Scope.Initialize(bGenCollections, false)) { return false; }

		// Replace tag filtering with class filtering to keep test actors tag-free
		Scope.Exporter->IncludeTags.Empty();
		Scope.Exporter->IncludeClasses.Add(TSoftClassPtr<AActor>(APCGExPropertyDeltaTestActor::StaticClass()));
		Scope.Exporter->bCapturePropertyDeltas = bEnableDeltas;

		return true;
	}

	/** Spawn a test actor WITHOUT root component and WITHOUT tags.
	 *  This keeps the actor CDO-identical for all non-transient UPROPERTYs. */
	APCGExPropertyDeltaTestActor* SpawnActor(
		FExporterTestScope& Scope,
		const FTransform& Transform = FTransform::Identity)
	{
		if (!Scope.World) { return nullptr; }

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(
			Scope.World, APCGExPropertyDeltaTestActor::StaticClass(), FName(TEXT("PDTestActor")));
		SpawnParams.ObjectFlags = RF_Transient;
		SpawnParams.bHideFromSceneOutliner = true;

		APCGExPropertyDeltaTestActor* Actor = Scope.World->SpawnActor<APCGExPropertyDeltaTestActor>(
			APCGExPropertyDeltaTestActor::StaticClass(), Transform, SpawnParams);
		if (!Actor) { return nullptr; }

		Scope.SpawnedActors.Add(Actor);
		return Actor;
	}

	/** Spawn a test actor WITH root component (needed for mesh attachment). */
	APCGExPropertyDeltaTestActor* SpawnActorWithRoot(
		FExporterTestScope& Scope,
		const FTransform& Transform = FTransform::Identity)
	{
		APCGExPropertyDeltaTestActor* Actor = SpawnActor(Scope, Transform);
		if (!Actor) { return nullptr; }

		USceneComponent* Root = NewObject<USceneComponent>(Actor, NAME_None, RF_Transient);
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();

		return Actor;
	}

	/** Find embedded actor collection in the output asset. */
	UPCGExActorCollection* FindActorCollection(const FExporterTestScope& Scope)
	{
		UPCGExActorCollection* Collection = nullptr;
		ForEachObjectWithOuter(Scope.OutputAsset, [&](UObject* Obj)
		{
			if (auto* AC = Cast<UPCGExActorCollection>(Obj)) { Collection = AC; }
		}, false);
		return Collection;
	}

	/** Find embedded mesh collection in the output asset. */
	UPCGExMeshCollection* FindMeshCollection(const FExporterTestScope& Scope)
	{
		UPCGExMeshCollection* Collection = nullptr;
		ForEachObjectWithOuter(Scope.OutputAsset, [&](UObject* Obj)
		{
			if (auto* MC = Cast<UPCGExMeshCollection>(Obj)) { Collection = MC; }
		}, false);
		return Collection;
	}
}

//////////////////////////////////////////////////////////////////////////
// CDO-Identical: Empty Delta
//////////////////////////////////////////////////////////////////////////

#pragma region CDOIdentical

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPDCDOIdenticalEmptyDeltaTest,
	"PCGEx.Unit.Collections.LevelDataExporter.PropertyDelta.CDOIdentical_EmptyDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPDCDOIdenticalEmptyDeltaTest::RunTest(const FString& Parameters)
{
	using namespace PCGExPropertyDeltaTestHelpers;
	FExporterTestScope Scope;
	if (!SetupScope(Scope)) { return false; }

	// Spawn one unmodified actor (no root component, no tags → CDO-identical for all non-transient UPROPERTYs)
	APCGExPropertyDeltaTestActor* Actor = SpawnActor(Scope);
	if (!Actor) { AddError(TEXT("Failed to spawn test actor")); return false; }

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExActorCollection* Collection = FindActorCollection(Scope);
	TestTrue(TEXT("Actor collection exists"), Collection != nullptr);
	if (!Collection || Collection->Entries.Num() == 0) { return false; }

	TestEqual(TEXT("1 entry for single actor"), Collection->Entries.Num(), 1);

	// Unmodified actor should have empty property delta (no non-transient UPROPERTY differs from CDO)
	const FPCGExActorCollectionEntry& Entry = Collection->Entries[0];
	TestEqual(TEXT("CDO-identical actor has empty SerializedPropertyDelta"),
		Entry.SerializedPropertyDelta.Num(), 0);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Modified Actor: Non-Empty Delta
//////////////////////////////////////////////////////////////////////////

#pragma region ModifiedActor

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPDModifiedActorNonEmptyDeltaTest,
	"PCGEx.Unit.Collections.LevelDataExporter.PropertyDelta.ModifiedActor_NonEmptyDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPDModifiedActorNonEmptyDeltaTest::RunTest(const FString& Parameters)
{
	using namespace PCGExPropertyDeltaTestHelpers;
	FExporterTestScope Scope;
	if (!SetupScope(Scope)) { return false; }

	APCGExPropertyDeltaTestActor* Actor = SpawnActor(Scope);
	if (!Actor) { AddError(TEXT("Failed to spawn test actor")); return false; }

	// Modify a property from CDO default (100 → 50)
	Actor->TestHealth = 50;

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExActorCollection* Collection = FindActorCollection(Scope);
	TestTrue(TEXT("Actor collection exists"), Collection != nullptr);
	if (!Collection || Collection->Entries.Num() == 0) { return false; }

	const FPCGExActorCollectionEntry& Entry = Collection->Entries[0];
	TestTrue(TEXT("Modified actor has non-empty SerializedPropertyDelta"),
		Entry.SerializedPropertyDelta.Num() > 0);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Same Class, Different Property Values → Separate Entries
//////////////////////////////////////////////////////////////////////////

#pragma region DifferentProps

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPDDifferentPropsSeparateEntriesTest,
	"PCGEx.Unit.Collections.LevelDataExporter.PropertyDelta.SameClassDifferentProps_SeparateEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPDDifferentPropsSeparateEntriesTest::RunTest(const FString& Parameters)
{
	using namespace PCGExPropertyDeltaTestHelpers;
	FExporterTestScope Scope;
	if (!SetupScope(Scope)) { return false; }

	// Two actors of same class with different TestHealth values
	APCGExPropertyDeltaTestActor* Actor1 = SpawnActor(Scope, FTransform(FVector(100, 0, 0)));
	APCGExPropertyDeltaTestActor* Actor2 = SpawnActor(Scope, FTransform(FVector(200, 0, 0)));
	if (!Actor1 || !Actor2) { AddError(TEXT("Failed to spawn test actors")); return false; }

	Actor1->TestHealth = 50;
	Actor2->TestHealth = 200;

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExActorCollection* Collection = FindActorCollection(Scope);
	TestTrue(TEXT("Actor collection exists"), Collection != nullptr);
	if (!Collection) { return false; }

	// Different property values → different delta hash → separate entries
	TestEqual(TEXT("2 separate entries for different property configs"),
		Collection->Entries.Num(), 2);

	// Both entries should have non-empty deltas
	for (int32 i = 0; i < Collection->Entries.Num(); i++)
	{
		TestTrue(*FString::Printf(TEXT("Entry %d has non-empty delta"), i),
			Collection->Entries[i].SerializedPropertyDelta.Num() > 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPDDifferentPropertyTypesSeparateEntriesTest,
	"PCGEx.Unit.Collections.LevelDataExporter.PropertyDelta.DifferentPropertyTypes_SeparateEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPDDifferentPropertyTypesSeparateEntriesTest::RunTest(const FString& Parameters)
{
	using namespace PCGExPropertyDeltaTestHelpers;
	FExporterTestScope Scope;
	if (!SetupScope(Scope)) { return false; }

	// One modifies int property, the other modifies float property
	APCGExPropertyDeltaTestActor* Actor1 = SpawnActor(Scope, FTransform(FVector(100, 0, 0)));
	APCGExPropertyDeltaTestActor* Actor2 = SpawnActor(Scope, FTransform(FVector(200, 0, 0)));
	if (!Actor1 || !Actor2) { AddError(TEXT("Failed to spawn test actors")); return false; }

	Actor1->TestHealth = 50; // CDO is 100
	Actor2->TestRadius = 99.0f; // CDO is 50.0f

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExActorCollection* Collection = FindActorCollection(Scope);
	TestTrue(TEXT("Actor collection exists"), Collection != nullptr);
	if (!Collection) { return false; }

	// Different properties modified → different deltas → separate entries
	TestEqual(TEXT("2 separate entries for different property modifications"),
		Collection->Entries.Num(), 2);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Same Class, Same Property Values → Shared Entry
//////////////////////////////////////////////////////////////////////////

#pragma region SameProps

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPDSamePropsSharedEntryTest,
	"PCGEx.Unit.Collections.LevelDataExporter.PropertyDelta.SameClassSameProps_SharedEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPDSamePropsSharedEntryTest::RunTest(const FString& Parameters)
{
	using namespace PCGExPropertyDeltaTestHelpers;
	FExporterTestScope Scope;
	if (!SetupScope(Scope)) { return false; }

	// Two actors with identical modifications
	APCGExPropertyDeltaTestActor* Actor1 = SpawnActor(Scope, FTransform(FVector(100, 0, 0)));
	APCGExPropertyDeltaTestActor* Actor2 = SpawnActor(Scope, FTransform(FVector(200, 0, 0)));
	if (!Actor1 || !Actor2) { AddError(TEXT("Failed to spawn test actors")); return false; }

	Actor1->TestHealth = 50;
	Actor2->TestHealth = 50;

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExActorCollection* Collection = FindActorCollection(Scope);
	TestTrue(TEXT("Actor collection exists"), Collection != nullptr);
	if (!Collection) { return false; }

	// Same modification → same delta hash → shared entry
	TestEqual(TEXT("1 shared entry for identical property configs"),
		Collection->Entries.Num(), 1);

	// Weight should reflect both actors
	TestEqual(TEXT("Entry weight = 2"), Collection->Entries[0].Weight, 2);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Mixed Population: Correct Entry Count and Weights
//////////////////////////////////////////////////////////////////////////

#pragma region MixedPopulation

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPDMixedPopulationTest,
	"PCGEx.Unit.Collections.LevelDataExporter.PropertyDelta.MixedPopulation_CorrectEntryCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPDMixedPopulationTest::RunTest(const FString& Parameters)
{
	using namespace PCGExPropertyDeltaTestHelpers;
	FExporterTestScope Scope;
	if (!SetupScope(Scope)) { return false; }

	// 2 actors with Health=50, 1 with Health=200, 1 unmodified
	APCGExPropertyDeltaTestActor* A1 = SpawnActor(Scope, FTransform(FVector(100, 0, 0)));
	APCGExPropertyDeltaTestActor* A2 = SpawnActor(Scope, FTransform(FVector(200, 0, 0)));
	APCGExPropertyDeltaTestActor* A3 = SpawnActor(Scope, FTransform(FVector(300, 0, 0)));
	APCGExPropertyDeltaTestActor* A4 = SpawnActor(Scope, FTransform(FVector(400, 0, 0)));
	if (!A1 || !A2 || !A3 || !A4) { AddError(TEXT("Failed to spawn test actors")); return false; }

	A1->TestHealth = 50;
	A2->TestHealth = 50;
	A3->TestHealth = 200;
	// A4 is unmodified (CDO-identical)

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExActorCollection* Collection = FindActorCollection(Scope);
	TestTrue(TEXT("Actor collection exists"), Collection != nullptr);
	if (!Collection) { return false; }

	// 3 unique configs: Health=50, Health=200, unmodified → 3 entries
	TestEqual(TEXT("3 entries for 3 unique property configs"),
		Collection->Entries.Num(), 3);

	// Total weight should equal number of actors
	int32 TotalWeight = 0;
	for (const FPCGExActorCollectionEntry& Entry : Collection->Entries)
	{
		TotalWeight += Entry.Weight;
	}
	TestEqual(TEXT("Total weight = 4 (all actors accounted for)"), TotalWeight, 4);

	// One entry should have weight 2 (the two Health=50 actors)
	bool bFoundWeight2 = false;
	for (const FPCGExActorCollectionEntry& Entry : Collection->Entries)
	{
		if (Entry.Weight == 2) { bFoundWeight2 = true; break; }
	}
	TestTrue(TEXT("One entry has weight 2 (two actors with same config)"), bFoundWeight2);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Mesh Actors: No Property Delta
//////////////////////////////////////////////////////////////////////////

#pragma region MeshActors

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPDMeshActorsNoDeltaTest,
	"PCGEx.Unit.Collections.LevelDataExporter.PropertyDelta.MeshActors_NoDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPDMeshActorsNoDeltaTest::RunTest(const FString& Parameters)
{
	using namespace PCGExPropertyDeltaTestHelpers;
	FExporterTestScope Scope;
	if (!SetupScope(Scope)) { return false; }

	UStaticMesh* Mesh = FExporterTestScope::LoadEngineMesh();
	if (!Mesh) { AddError(TEXT("Failed to load engine mesh")); return false; }

	// Mesh-classified actor: has SM component → goes to "Meshes" pin
	APCGExPropertyDeltaTestActor* MeshActor = SpawnActorWithRoot(Scope, FTransform(FVector(100, 0, 0)));
	if (!MeshActor) { AddError(TEXT("Failed to spawn mesh actor")); return false; }
	Scope.AddStaticMeshComponent(MeshActor, Mesh);
	MeshActor->TestHealth = 50; // Modified, but irrelevant for mesh-classified actors

	// Actor-classified actor: no SM component → goes to "Actors" pin
	APCGExPropertyDeltaTestActor* PureActor = SpawnActor(Scope, FTransform(FVector(200, 0, 0)));
	if (!PureActor) { AddError(TEXT("Failed to spawn pure actor")); return false; }
	PureActor->TestHealth = 50;

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	// Mesh collection should exist with 1 entry - no SerializedPropertyDelta field
	const UPCGBasePointData* MeshData = Scope.GetPinPointData(FName(TEXT("Meshes")));
	TestTrue(TEXT("Meshes pin exists"), MeshData != nullptr);

	UPCGExMeshCollection* MeshCollection = FindMeshCollection(Scope);
	TestTrue(TEXT("Mesh collection exists"), MeshCollection != nullptr);
	if (MeshCollection)
	{
		TestEqual(TEXT("1 mesh entry"), MeshCollection->Entries.Num(), 1);
		// FPCGExMeshCollectionEntry does not have SerializedPropertyDelta — mesh actors
		// are deduped by mesh path only, property deltas are not captured.
	}

	// Actor collection should exist with 1 entry WITH property delta
	UPCGExActorCollection* ActorCollection = FindActorCollection(Scope);
	TestTrue(TEXT("Actor collection exists"), ActorCollection != nullptr);
	if (ActorCollection)
	{
		TestEqual(TEXT("1 actor entry"), ActorCollection->Entries.Num(), 1);
		TestTrue(TEXT("Actor entry has non-empty delta"),
			ActorCollection->Entries[0].SerializedPropertyDelta.Num() > 0);
	}

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Feature Disabled: No Separation
//////////////////////////////////////////////////////////////////////////

#pragma region Disabled

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPDDisabledNoSeparationTest,
	"PCGEx.Unit.Collections.LevelDataExporter.PropertyDelta.Disabled_NoSeparation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPDDisabledNoSeparationTest::RunTest(const FString& Parameters)
{
	using namespace PCGExPropertyDeltaTestHelpers;
	FExporterTestScope Scope;
	if (!SetupScope(Scope, false)) { return false; } // bEnableDeltas = false

	// Two actors with different property values
	APCGExPropertyDeltaTestActor* Actor1 = SpawnActor(Scope, FTransform(FVector(100, 0, 0)));
	APCGExPropertyDeltaTestActor* Actor2 = SpawnActor(Scope, FTransform(FVector(200, 0, 0)));
	if (!Actor1 || !Actor2) { AddError(TEXT("Failed to spawn test actors")); return false; }

	Actor1->TestHealth = 50;
	Actor2->TestHealth = 200;

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExActorCollection* Collection = FindActorCollection(Scope);
	TestTrue(TEXT("Actor collection exists"), Collection != nullptr);
	if (!Collection) { return false; }

	// With deltas disabled, same class → same entry regardless of property differences
	TestEqual(TEXT("1 entry when deltas disabled (same class, no property separation)"),
		Collection->Entries.Num(), 1);

	// Weight should reflect both actors
	TestEqual(TEXT("Entry weight = 2"), Collection->Entries[0].Weight, 2);

	// Entry should have empty delta (feature disabled)
	TestEqual(TEXT("No delta bytes when feature disabled"),
		Collection->Entries[0].SerializedPropertyDelta.Num(), 0);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Tag Intersection: Preserved With Property Deltas
//////////////////////////////////////////////////////////////////////////

#pragma region TagIntersection

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPDTagIntersectionPreservedTest,
	"PCGEx.Unit.Collections.LevelDataExporter.PropertyDelta.TagIntersection_PreservedWithDeltas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPDTagIntersectionPreservedTest::RunTest(const FString& Parameters)
{
	using namespace PCGExPropertyDeltaTestHelpers;
	FExporterTestScope Scope;
	if (!Scope.Initialize(true, false)) { return false; }

	// Use BOTH tag AND class filtering for this test
	// (we want to test that tags work correctly with property deltas)
	Scope.Exporter->IncludeTags.Empty();
	const FName FilterTag(TEXT("PDTagTest"));
	Scope.Exporter->IncludeTags.Add(FilterTag);
	Scope.Exporter->IncludeClasses.Add(TSoftClassPtr<AActor>(APCGExPropertyDeltaTestActor::StaticClass()));
	Scope.Exporter->bCapturePropertyDeltas = true;

	// Two actors with same property config but different tags
	APCGExPropertyDeltaTestActor* Actor1 = SpawnActor(Scope, FTransform(FVector(100, 0, 0)));
	APCGExPropertyDeltaTestActor* Actor2 = SpawnActor(Scope, FTransform(FVector(200, 0, 0)));
	if (!Actor1 || !Actor2) { AddError(TEXT("Failed to spawn test actors")); return false; }

	// Same property modification
	Actor1->TestHealth = 50;
	Actor2->TestHealth = 50;

	// Add filter tag + shared + unique tags
	Actor1->Tags.Add(FilterTag);
	Actor1->Tags.Add(FName(TEXT("Shared")));
	Actor1->Tags.Add(FName(TEXT("OnlyFirst")));

	Actor2->Tags.Add(FilterTag);
	Actor2->Tags.Add(FName(TEXT("Shared")));
	Actor2->Tags.Add(FName(TEXT("OnlySecond")));

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExActorCollection* Collection = FindActorCollection(Scope);
	TestTrue(TEXT("Actor collection exists"), Collection != nullptr);
	if (!Collection) { return false; }

	// Same property config + same class → 1 entry (tags affect the ENTRY's Tags, not dedup)
	// Note: The dedup key includes delta hash (which includes Tags since we modified them).
	// But both actors have the SAME tags added? No - they have different tags!
	// Tags are UPROPERTYs, so different tags → different delta → separate entries.
	//
	// Actually: Both actors have Tags = {FilterTag, Shared, OnlyFirst/OnlySecond}.
	// Since Tags differ, their deltas differ, so they get separate entries.
	TestEqual(TEXT("2 entries (different tags → different deltas)"),
		Collection->Entries.Num(), 2);

	// Verify that entry tags (intersection) are computed per-entry
	for (const FPCGExActorCollectionEntry& Entry : Collection->Entries)
	{
		// Single actor per entry → intersection = that actor's tags
		TestTrue(TEXT("Entry has FilterTag"), Entry.Tags.Contains(FilterTag));
		TestTrue(TEXT("Entry has Shared tag"), Entry.Tags.Contains(FName(TEXT("Shared"))));
	}

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// No Collections: Feature Inactive
//////////////////////////////////////////////////////////////////////////

#pragma region NoCollections

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPDNoCollectionsInactiveTest,
	"PCGEx.Unit.Collections.LevelDataExporter.PropertyDelta.NoCollections_Inactive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPDNoCollectionsInactiveTest::RunTest(const FString& Parameters)
{
	using namespace PCGExPropertyDeltaTestHelpers;
	FExporterTestScope Scope;
	if (!SetupScope(Scope, true, false)) { return false; } // bGenCollections = false

	APCGExPropertyDeltaTestActor* Actor = SpawnActor(Scope);
	if (!Actor) { AddError(TEXT("Failed to spawn test actor")); return false; }

	Actor->TestHealth = 50;

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	// No embedded collections when bGenerateCollections is false
	UPCGExActorCollection* Collection = FindActorCollection(Scope);
	TestTrue(TEXT("No actor collection when bGenerateCollections=false"), Collection == nullptr);

	// No CollectionMap pin
	const FPCGTaggedData* MapPin = Scope.FindPin(FName(TEXT("CollectionMap")));
	TestTrue(TEXT("No CollectionMap pin"), MapPin == nullptr);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Multiple Properties Modified
//////////////////////////////////////////////////////////////////////////

#pragma region MultipleProperties

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPDMultiplePropertiesModifiedTest,
	"PCGEx.Unit.Collections.LevelDataExporter.PropertyDelta.MultipleProperties_CapturedInDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPDMultiplePropertiesModifiedTest::RunTest(const FString& Parameters)
{
	using namespace PCGExPropertyDeltaTestHelpers;
	FExporterTestScope Scope;
	if (!SetupScope(Scope)) { return false; }

	// Actor with single property modified
	APCGExPropertyDeltaTestActor* SingleMod = SpawnActor(Scope, FTransform(FVector(100, 0, 0)));
	// Actor with multiple properties modified
	APCGExPropertyDeltaTestActor* MultiMod = SpawnActor(Scope, FTransform(FVector(200, 0, 0)));
	if (!SingleMod || !MultiMod) { AddError(TEXT("Failed to spawn test actors")); return false; }

	SingleMod->TestHealth = 50;

	MultiMod->TestHealth = 50;
	MultiMod->TestRadius = 99.0f;
	MultiMod->TestDisplayName = TEXT("Custom");

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExActorCollection* Collection = FindActorCollection(Scope);
	TestTrue(TEXT("Actor collection exists"), Collection != nullptr);
	if (!Collection) { return false; }

	// Different property configs → 2 separate entries
	TestEqual(TEXT("2 entries (single vs multi property modification)"),
		Collection->Entries.Num(), 2);

	// Both should have non-empty deltas, and the multi-property delta should be larger
	bool bFoundSmaller = false;
	bool bFoundLarger = false;
	int32 MinSize = MAX_int32;
	int32 MaxSize = 0;

	for (const FPCGExActorCollectionEntry& Entry : Collection->Entries)
	{
		const int32 DeltaSize = Entry.SerializedPropertyDelta.Num();
		TestTrue(TEXT("Entry has non-empty delta"), DeltaSize > 0);
		MinSize = FMath::Min(MinSize, DeltaSize);
		MaxSize = FMath::Max(MaxSize, DeltaSize);
	}

	// More properties modified → more delta bytes
	TestTrue(TEXT("Multi-property delta is larger than single-property delta"), MaxSize > MinSize);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// CDO-Identical Actors Share Entry
//////////////////////////////////////////////////////////////////////////

#pragma region CDOIdenticalShared

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPDCDOIdenticalSharedEntryTest,
	"PCGEx.Unit.Collections.LevelDataExporter.PropertyDelta.CDOIdentical_SharedEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPDCDOIdenticalSharedEntryTest::RunTest(const FString& Parameters)
{
	using namespace PCGExPropertyDeltaTestHelpers;
	FExporterTestScope Scope;
	if (!SetupScope(Scope)) { return false; }

	// Two unmodified actors → should share one entry
	APCGExPropertyDeltaTestActor* Actor1 = SpawnActor(Scope, FTransform(FVector(100, 0, 0)));
	APCGExPropertyDeltaTestActor* Actor2 = SpawnActor(Scope, FTransform(FVector(200, 0, 0)));
	if (!Actor1 || !Actor2) { AddError(TEXT("Failed to spawn test actors")); return false; }

	const bool bResult = Scope.RunExport();
	TestTrue(TEXT("Export succeeded"), bResult);

	UPCGExActorCollection* Collection = FindActorCollection(Scope);
	TestTrue(TEXT("Actor collection exists"), Collection != nullptr);
	if (!Collection) { return false; }

	// Both unmodified → same delta hash → 1 entry
	TestEqual(TEXT("1 shared entry for CDO-identical actors"), Collection->Entries.Num(), 1);
	TestEqual(TEXT("Entry weight = 2"), Collection->Entries[0].Weight, 2);

	return true;
}

#pragma endregion
