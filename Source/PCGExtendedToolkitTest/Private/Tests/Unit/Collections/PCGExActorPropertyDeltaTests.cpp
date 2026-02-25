// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"

#include "Helpers/PCGExActorPropertyDelta.h"
#include "Helpers/PCGExPropertyDeltaTestActor.h"

#include "Engine/World.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

/**
 * Unit tests for PCGExActorDelta::SerializeActorDelta / ApplyPropertyDelta / HashDelta.
 * Tests the shared utility directly (no exporter, no collections).
 *
 * Covers: actor-level deltas, component deltas, round-trip, hash, edge cases.
 */

namespace PCGExActorDeltaTestHelpers
{
	/** Spawn a test actor into the editor world. Caller must Destroy() when done. */
	template <typename T>
	T* SpawnTestActor(UWorld* World)
	{
		if (!World) { return nullptr; }

		FActorSpawnParameters Params;
		Params.Name = MakeUniqueObjectName(World, T::StaticClass(), FName(TEXT("ADTest")));
		Params.ObjectFlags = RF_Transient;
		Params.bHideFromSceneOutliner = true;

		return World->SpawnActor<T>(T::StaticClass(), FTransform::Identity, Params);
	}

	/** Spawn deferred (for delta application testing). */
	template <typename T>
	T* SpawnTestActorDeferred(UWorld* World)
	{
		if (!World) { return nullptr; }

		FActorSpawnParameters Params;
		Params.Name = MakeUniqueObjectName(World, T::StaticClass(), FName(TEXT("ADTestDef")));
		Params.ObjectFlags = RF_Transient;
		Params.bHideFromSceneOutliner = true;
		Params.bDeferConstruction = true;

		return World->SpawnActor<T>(T::StaticClass(), FTransform::Identity, Params);
	}

	UWorld* GetTestWorld()
	{
#if WITH_EDITOR
		if (!GEditor) { return nullptr; }
		return GEditor->GetEditorWorldContext().World();
#else
		return nullptr;
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
// Actor-Level: CDO-Identical → Empty
//////////////////////////////////////////////////////////////////////////

#pragma region ActorCDOIdentical

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADActorCDOIdenticalTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Actor_CDOIdentical_EmptyDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADActorCDOIdenticalTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Actor = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Actor) { AddError(TEXT("Failed to spawn actor")); return false; }

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Actor);
	TestEqual(TEXT("CDO-identical actor produces empty delta"), Delta.Num(), 0);

	Actor->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Actor-Level: Modified → Non-Empty
//////////////////////////////////////////////////////////////////////////

#pragma region ActorModified

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADActorModifiedTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Actor_Modified_NonEmptyDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADActorModifiedTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Actor = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Actor) { AddError(TEXT("Failed to spawn actor")); return false; }

	Actor->TestHealth = 999;

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Actor);
	TestTrue(TEXT("Modified actor produces non-empty delta"), Delta.Num() > 0);

	Actor->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Actor-Level: Round-Trip
//////////////////////////////////////////////////////////////////////////

#pragma region ActorRoundTrip

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADActorRoundTripTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Actor_RoundTrip_PreservesValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADActorRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	// Serialize from modified actor
	APCGExPropertyDeltaTestActor* Source = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source actor")); return false; }

	Source->TestHealth = 999;
	Source->TestRadius = 123.456f;
	Source->TestDisplayName = TEXT("Modified");

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);
	TestTrue(TEXT("Delta is non-empty"), Delta.Num() > 0);

	// Apply to a fresh deferred actor
	APCGExPropertyDeltaTestActor* Target = SpawnTestActorDeferred<APCGExPropertyDeltaTestActor>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target actor")); Source->Destroy(); return false; }

	// Verify CDO defaults before apply
	TestEqual(TEXT("Target starts at CDO health"), Target->TestHealth, 100);
	TestEqual(TEXT("Target starts at CDO radius"), Target->TestRadius, 50.0f);

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("Health round-tripped"), Target->TestHealth, 999);
	TestTrue(TEXT("Radius round-tripped"), FMath::IsNearlyEqual(Target->TestRadius, 123.456f, 0.001f));
	TestEqual(TEXT("DisplayName round-tripped"), Target->TestDisplayName, FString(TEXT("Modified")));

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Component: Unmodified → No Component Bytes
//////////////////////////////////////////////////////////////////////////

#pragma region CompUnmodified

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADCompUnmodifiedTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Component_Unmodified_EmptyDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADCompUnmodifiedTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExDeltaTestActorWithComp* Actor = SpawnTestActor<APCGExDeltaTestActorWithComp>(World);
	if (!Actor) { AddError(TEXT("Failed to spawn actor")); return false; }

	// Actor and component both at CDO defaults
	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Actor);
	TestEqual(TEXT("Fully CDO-identical actor+comp produces empty delta"), Delta.Num(), 0);

	Actor->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Component: Modified → Non-Empty
//////////////////////////////////////////////////////////////////////////

#pragma region CompModified

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADCompModifiedTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Component_Modified_NonEmptyDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADCompModifiedTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExDeltaTestActorWithComp* Actor = SpawnTestActor<APCGExDeltaTestActorWithComp>(World);
	if (!Actor) { AddError(TEXT("Failed to spawn actor")); return false; }

	// Modify only the component, leave actor at defaults
	Actor->TestComp->CompValue = 999;

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Actor);
	TestTrue(TEXT("Modified component produces non-empty delta"), Delta.Num() > 0);

	Actor->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Component: Round-Trip
//////////////////////////////////////////////////////////////////////////

#pragma region CompRoundTrip

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADCompRoundTripTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Component_RoundTrip_PreservesValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADCompRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	// Serialize from actor with modified component
	APCGExDeltaTestActorWithComp* Source = SpawnTestActor<APCGExDeltaTestActorWithComp>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source actor")); return false; }

	Source->TestComp->CompValue = 777;
	Source->TestComp->CompName = TEXT("CustomComp");

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);
	TestTrue(TEXT("Delta is non-empty"), Delta.Num() > 0);

	// Apply to fresh deferred actor
	APCGExDeltaTestActorWithComp* Target = SpawnTestActorDeferred<APCGExDeltaTestActorWithComp>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target actor")); Source->Destroy(); return false; }

	// Verify CDO defaults
	TestEqual(TEXT("Target comp starts at CDO value"), Target->TestComp->CompValue, 42);

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("CompValue round-tripped"), Target->TestComp->CompValue, 777);
	TestEqual(TEXT("CompName round-tripped"), Target->TestComp->CompName, FString(TEXT("CustomComp")));

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Mixed: Actor + Component Deltas Round-Trip
//////////////////////////////////////////////////////////////////////////

#pragma region MixedRoundTrip

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADMixedRoundTripTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Mixed_ActorAndComponent_RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADMixedRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExDeltaTestActorWithComp* Source = SpawnTestActor<APCGExDeltaTestActorWithComp>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source actor")); return false; }

	// Modify both actor and component
	Source->ActorScore = 99;
	Source->TestComp->CompValue = 555;
	Source->TestComp->CompName = TEXT("Both");

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);
	TestTrue(TEXT("Delta is non-empty"), Delta.Num() > 0);

	APCGExDeltaTestActorWithComp* Target = SpawnTestActorDeferred<APCGExDeltaTestActorWithComp>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("ActorScore round-tripped"), Target->ActorScore, 99);
	TestEqual(TEXT("CompValue round-tripped"), Target->TestComp->CompValue, 555);
	TestEqual(TEXT("CompName round-tripped"), Target->TestComp->CompName, FString(TEXT("Both")));

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Hash: Determinism and Empty
//////////////////////////////////////////////////////////////////////////

#pragma region Hash

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADHashEmptyTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Hash_Empty_ReturnsZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADHashEmptyTest::RunTest(const FString& Parameters)
{
	const TArray<uint8> Empty;
	TestEqual(TEXT("Empty delta hashes to 0"), PCGExActorDelta::HashDelta(Empty), 0u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADHashDeterminismTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Hash_Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADHashDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Actor = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Actor) { AddError(TEXT("Failed to spawn actor")); return false; }

	Actor->TestHealth = 50;

	const TArray<uint8> Delta1 = PCGExActorDelta::SerializeActorDelta(Actor);
	const TArray<uint8> Delta2 = PCGExActorDelta::SerializeActorDelta(Actor);

	const uint32 Hash1 = PCGExActorDelta::HashDelta(Delta1);
	const uint32 Hash2 = PCGExActorDelta::HashDelta(Delta2);

	TestTrue(TEXT("Hash is non-zero for non-empty delta"), Hash1 != 0);
	TestEqual(TEXT("Same delta produces same hash"), Hash1, Hash2);

	Actor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADHashDifferentDeltasTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Hash_DifferentDeltas_DifferentHashes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADHashDifferentDeltasTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* A1 = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	APCGExPropertyDeltaTestActor* A2 = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!A1 || !A2) { AddError(TEXT("Failed to spawn actors")); return false; }

	A1->TestHealth = 50;
	A2->TestHealth = 200;

	const uint32 H1 = PCGExActorDelta::HashDelta(PCGExActorDelta::SerializeActorDelta(A1));
	const uint32 H2 = PCGExActorDelta::HashDelta(PCGExActorDelta::SerializeActorDelta(A2));

	TestTrue(TEXT("Different property values produce different hashes"), H1 != H2);

	A1->Destroy();
	A2->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Edge Case: Null Actor
//////////////////////////////////////////////////////////////////////////

#pragma region NullActor

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADNullActorTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.NullActor_SafeEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADNullActorTest::RunTest(const FString& Parameters)
{
	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(nullptr);
	TestEqual(TEXT("Null actor produces empty delta"), Delta.Num(), 0);

	// Apply to null should not crash
	PCGExActorDelta::ApplyPropertyDelta(nullptr, Delta);

	// Apply empty delta should not crash
	const TArray<uint8> NonEmpty = {1, 2, 3, 4};
	PCGExActorDelta::ApplyPropertyDelta(nullptr, NonEmpty);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Edge Case: Only Actor Modified, Component Untouched
//////////////////////////////////////////////////////////////////////////

#pragma region OnlyActorModified

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADOnlyActorModifiedTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.OnlyActorModified_ComponentPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADOnlyActorModifiedTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExDeltaTestActorWithComp* Source = SpawnTestActor<APCGExDeltaTestActorWithComp>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	// Only modify actor-level property
	Source->ActorScore = 77;
	// Component stays at CDO defaults (CompValue=42, CompName="CompDefault")

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);
	TestTrue(TEXT("Delta is non-empty"), Delta.Num() > 0);

	APCGExDeltaTestActorWithComp* Target = SpawnTestActorDeferred<APCGExDeltaTestActorWithComp>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("ActorScore applied"), Target->ActorScore, 77);
	TestEqual(TEXT("CompValue unchanged at CDO default"), Target->TestComp->CompValue, 42);
	TestEqual(TEXT("CompName unchanged at CDO default"), Target->TestComp->CompName, FString(TEXT("CompDefault")));

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Edge Case: Only Component Modified, Actor Untouched
//////////////////////////////////////////////////////////////////////////

#pragma region OnlyCompModified

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADOnlyCompModifiedTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.OnlyComponentModified_ActorPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADOnlyCompModifiedTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExDeltaTestActorWithComp* Source = SpawnTestActor<APCGExDeltaTestActorWithComp>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	// Only modify component
	Source->TestComp->CompValue = 123;
	// Actor stays at CDO defaults (ActorScore=10)

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);
	TestTrue(TEXT("Delta is non-empty"), Delta.Num() > 0);

	APCGExDeltaTestActorWithComp* Target = SpawnTestActorDeferred<APCGExDeltaTestActorWithComp>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("ActorScore unchanged at CDO default"), Target->ActorScore, 10);
	TestEqual(TEXT("CompValue applied"), Target->TestComp->CompValue, 123);

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// ADVERSARIAL TESTS
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Type Coverage: All property types round-trip correctly
//////////////////////////////////////////////////////////////////////////

#pragma region AllTypesRoundTrip

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADAllTypesRoundTripTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.AllTypes_RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADAllTypesRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Source = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	// Modify every property type
	Source->TestHealth = 999;
	Source->TestRadius = 3.14159f;
	Source->TestDisplayName = TEXT("AllTypes");
	Source->TestLocation = FVector(1.0, 2.0, 3.0);
	Source->TestRotation = FRotator(45.0, 90.0, 180.0);
	Source->TestColor = FLinearColor(0.1f, 0.2f, 0.3f, 0.4f);
	Source->bTestEnabled = true;
	Source->TestCategory = FName(TEXT("CategoryA"));
	Source->TestTags = { FName(TEXT("Tag1")), FName(TEXT("Tag2")), FName(TEXT("Tag3")) };

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);
	TestTrue(TEXT("Delta is non-empty"), Delta.Num() > 0);

	APCGExPropertyDeltaTestActor* Target = SpawnTestActorDeferred<APCGExPropertyDeltaTestActor>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("int32 round-tripped"), Target->TestHealth, 999);
	TestTrue(TEXT("float round-tripped"), FMath::IsNearlyEqual(Target->TestRadius, 3.14159f, 0.0001f));
	TestEqual(TEXT("FString round-tripped"), Target->TestDisplayName, FString(TEXT("AllTypes")));
	TestTrue(TEXT("FVector round-tripped"), Target->TestLocation.Equals(FVector(1.0, 2.0, 3.0), 0.001));
	TestTrue(TEXT("FRotator round-tripped"), Target->TestRotation.Equals(FRotator(45.0, 90.0, 180.0), 0.001f));
	TestTrue(TEXT("FLinearColor round-tripped"), Target->TestColor.Equals(FLinearColor(0.1f, 0.2f, 0.3f, 0.4f), 0.001f));
	TestEqual(TEXT("bool round-tripped"), Target->bTestEnabled, true);
	TestEqual(TEXT("FName round-tripped"), Target->TestCategory, FName(TEXT("CategoryA")));
	TestEqual(TEXT("TArray<FName> count"), Target->TestTags.Num(), 3);
	if (Target->TestTags.Num() == 3)
	{
		TestEqual(TEXT("TArray[0]"), Target->TestTags[0], FName(TEXT("Tag1")));
		TestEqual(TEXT("TArray[1]"), Target->TestTags[1], FName(TEXT("Tag2")));
		TestEqual(TEXT("TArray[2]"), Target->TestTags[2], FName(TEXT("Tag3")));
	}

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Extreme Values: Boundary conditions for numeric types
//////////////////////////////////////////////////////////////////////////

#pragma region ExtremeValues

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADExtremeValuesTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.ExtremeValues_RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADExtremeValuesTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Source = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	Source->TestHealth = MAX_int32;
	Source->TestRadius = -0.0f;
	Source->TestDisplayName = TEXT("");  // Empty string
	Source->TestLocation = FVector(MAX_dbl, -MAX_dbl, 0.0);

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);
	TestTrue(TEXT("Delta non-empty"), Delta.Num() > 0);

	APCGExPropertyDeltaTestActor* Target = SpawnTestActorDeferred<APCGExPropertyDeltaTestActor>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("MAX_int32 round-tripped"), Target->TestHealth, MAX_int32);
	TestEqual(TEXT("Empty string round-tripped"), Target->TestDisplayName, FString(TEXT("")));
	TestEqual(TEXT("FVector.X MAX_dbl"), Target->TestLocation.X, MAX_dbl);
	TestEqual(TEXT("FVector.Y -MAX_dbl"), Target->TestLocation.Y, -MAX_dbl);

	Source->Destroy();
	Target->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADNegativeAndZeroValuesTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.NegativeAndZero_RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADNegativeAndZeroValuesTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Source = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	Source->TestHealth = MIN_int32;
	Source->TestRadius = -999.999f;
	Source->TestLocation = FVector::ZeroVector;
	Source->TestRotation = FRotator(-180.0, -180.0, -180.0);
	Source->TestColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);

	APCGExPropertyDeltaTestActor* Target = SpawnTestActorDeferred<APCGExPropertyDeltaTestActor>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("MIN_int32"), Target->TestHealth, MIN_int32);
	TestTrue(TEXT("Negative float"), FMath::IsNearlyEqual(Target->TestRadius, -999.999f, 0.001f));
	TestTrue(TEXT("Zero color"), Target->TestColor.Equals(FLinearColor(0, 0, 0, 0), 0.001f));

	Source->Destroy();
	Target->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADLongStringTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.LongString_RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADLongStringTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Source = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	// 10,000 character string
	FString LongString;
	for (int32 i = 0; i < 10000; ++i) { LongString.AppendChar(TEXT('A') + (i % 26)); }
	Source->TestDisplayName = LongString;

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);

	APCGExPropertyDeltaTestActor* Target = SpawnTestActorDeferred<APCGExPropertyDeltaTestActor>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("Long string length preserved"), Target->TestDisplayName.Len(), 10000);
	TestEqual(TEXT("Long string content preserved"), Target->TestDisplayName, LongString);

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Idempotency: Applying delta twice doesn't corrupt
//////////////////////////////////////////////////////////////////////////

#pragma region Idempotency

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADIdempotentApplyTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.IdempotentApply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADIdempotentApplyTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Source = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	Source->TestHealth = 42;
	Source->TestDisplayName = TEXT("Idem");
	Source->TestColor = FLinearColor::Red;

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);

	APCGExPropertyDeltaTestActor* Target = SpawnTestActorDeferred<APCGExPropertyDeltaTestActor>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	// Apply twice
	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("Health stable after double apply"), Target->TestHealth, 42);
	TestEqual(TEXT("DisplayName stable"), Target->TestDisplayName, FString(TEXT("Idem")));
	TestTrue(TEXT("Color stable"), Target->TestColor.Equals(FLinearColor::Red, 0.001f));

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Re-serialization: Serialize → Apply → Re-serialize produces same hash
//////////////////////////////////////////////////////////////////////////

#pragma region Reserialization

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADReserializationConsistencyTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.Reserialization_SameHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADReserializationConsistencyTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Source = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	Source->TestHealth = 777;
	Source->TestRadius = 1.5f;
	Source->bTestEnabled = true;
	Source->TestTags = { FName(TEXT("X")), FName(TEXT("Y")) };

	const TArray<uint8> OriginalDelta = PCGExActorDelta::SerializeActorDelta(Source);
	const uint32 OriginalHash = PCGExActorDelta::HashDelta(OriginalDelta);

	// Apply to a new actor, then re-serialize from that actor
	APCGExPropertyDeltaTestActor* Middle = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Middle) { AddError(TEXT("Failed to spawn middle")); Source->Destroy(); return false; }

	PCGExActorDelta::ApplyPropertyDelta(Middle, OriginalDelta);

	const TArray<uint8> ReserializedDelta = PCGExActorDelta::SerializeActorDelta(Middle);
	const uint32 ReserializedHash = PCGExActorDelta::HashDelta(ReserializedDelta);

	TestEqual(TEXT("Re-serialized hash matches original"), ReserializedHash, OriginalHash);
	TestEqual(TEXT("Re-serialized byte count matches"), ReserializedDelta.Num(), OriginalDelta.Num());

	Source->Destroy();
	Middle->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Reset to CDO: Modify, serialize, reset, serialize → empty
//////////////////////////////////////////////////////////////////////////

#pragma region ResetToCDO

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADResetToCDOTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.ResetToCDO_EmptyDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADResetToCDOTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Actor = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Actor) { AddError(TEXT("Failed to spawn actor")); return false; }

	// Modify
	Actor->TestHealth = 999;
	Actor->TestDisplayName = TEXT("Changed");

	const TArray<uint8> ModifiedDelta = PCGExActorDelta::SerializeActorDelta(Actor);
	TestTrue(TEXT("Modified delta is non-empty"), ModifiedDelta.Num() > 0);

	// Reset to CDO defaults
	Actor->TestHealth = 100;
	Actor->TestDisplayName = TEXT("Default");

	const TArray<uint8> ResetDelta = PCGExActorDelta::SerializeActorDelta(Actor);
	TestEqual(TEXT("Reset-to-CDO delta is empty"), ResetDelta.Num(), 0);

	Actor->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Pre-Modified Target: Delta only overwrites delta'd properties
//////////////////////////////////////////////////////////////////////////

#pragma region PreModifiedTarget

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADPreModifiedTargetTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.PreModifiedTarget_SelectiveOverwrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADPreModifiedTargetTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	// Source modifies only TestHealth
	APCGExPropertyDeltaTestActor* Source = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }
	Source->TestHealth = 1;

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);

	// Target already has custom values for OTHER properties
	APCGExPropertyDeltaTestActor* Target = SpawnTestActorDeferred<APCGExPropertyDeltaTestActor>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	Target->TestRadius = 999.0f;
	Target->TestDisplayName = TEXT("PreExisting");
	Target->TestColor = FLinearColor::Green;

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	// Delta'd property overwritten
	TestEqual(TEXT("Health overwritten by delta"), Target->TestHealth, 1);

	// Non-delta'd properties preserved
	TestTrue(TEXT("Radius preserved"), FMath::IsNearlyEqual(Target->TestRadius, 999.0f));
	TestEqual(TEXT("DisplayName preserved"), Target->TestDisplayName, FString(TEXT("PreExisting")));
	TestTrue(TEXT("Color preserved"), Target->TestColor.Equals(FLinearColor::Green, 0.001f));

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Multi-Component: Selective modification per component
//////////////////////////////////////////////////////////////////////////

#pragma region MultiComponent

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADMultiCompSelectiveTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.MultiComponent_SelectiveModification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADMultiCompSelectiveTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExDeltaTestActorMultiComp* Source = SpawnTestActor<APCGExDeltaTestActorMultiComp>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	// Modify only CompA, leave CompB at CDO defaults
	Source->CompA->CompValue = 999;
	Source->CompA->CompName = TEXT("ModifiedA");
	// CompB stays at CompValue=42, CompName="CompDefault"

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);
	TestTrue(TEXT("Delta is non-empty"), Delta.Num() > 0);

	APCGExDeltaTestActorMultiComp* Target = SpawnTestActorDeferred<APCGExDeltaTestActorMultiComp>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	// CompA should have modified values
	TestEqual(TEXT("CompA.CompValue modified"), Target->CompA->CompValue, 999);
	TestEqual(TEXT("CompA.CompName modified"), Target->CompA->CompName, FString(TEXT("ModifiedA")));

	// CompB should be at CDO defaults
	TestEqual(TEXT("CompB.CompValue preserved"), Target->CompB->CompValue, 42);
	TestEqual(TEXT("CompB.CompName preserved"), Target->CompB->CompName, FString(TEXT("CompDefault")));

	Source->Destroy();
	Target->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADMultiCompBothModifiedTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.MultiComponent_BothModified_RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADMultiCompBothModifiedTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExDeltaTestActorMultiComp* Source = SpawnTestActor<APCGExDeltaTestActorMultiComp>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	Source->ActorValue = 77;
	Source->CompA->CompValue = 111;
	Source->CompA->CompName = TEXT("AlphaComp");
	Source->CompB->CompValue = 222;
	Source->CompB->CompName = TEXT("BetaComp");

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);

	APCGExDeltaTestActorMultiComp* Target = SpawnTestActorDeferred<APCGExDeltaTestActorMultiComp>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("ActorValue"), Target->ActorValue, 77);
	TestEqual(TEXT("CompA.CompValue"), Target->CompA->CompValue, 111);
	TestEqual(TEXT("CompA.CompName"), Target->CompA->CompName, FString(TEXT("AlphaComp")));
	TestEqual(TEXT("CompB.CompValue"), Target->CompB->CompValue, 222);
	TestEqual(TEXT("CompB.CompName"), Target->CompB->CompName, FString(TEXT("BetaComp")));

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Graceful Degradation: Corrupted/garbage bytes don't crash
//////////////////////////////////////////////////////////////////////////

#pragma region GracefulDegradation

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADGarbageBytesTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.GarbageBytes_NoCrash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADGarbageBytesTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Actor = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Actor) { AddError(TEXT("Failed to spawn actor")); return false; }

	const int32 OriginalHealth = Actor->TestHealth;

	// Garbage bytes — should not crash, values should remain unchanged
	TArray<uint8> Garbage = { 0xFF, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02 };
	PCGExActorDelta::ApplyPropertyDelta(Actor, Garbage);

	TestEqual(TEXT("Health unchanged after garbage"), Actor->TestHealth, OriginalHealth);

	// Truncated wire format — just a uint32 with no following data
	TArray<uint8> Truncated;
	FMemoryWriter Writer(Truncated);
	uint32 FakeSize = 1000;
	Writer << FakeSize;

	PCGExActorDelta::ApplyPropertyDelta(Actor, Truncated);
	TestEqual(TEXT("Health unchanged after truncated"), Actor->TestHealth, OriginalHealth);

	// Empty bytes
	TArray<uint8> Empty;
	PCGExActorDelta::ApplyPropertyDelta(Actor, Empty);
	TestEqual(TEXT("Health unchanged after empty"), Actor->TestHealth, OriginalHealth);

	Actor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADMissingComponentTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.MissingComponent_SafeSkip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADMissingComponentTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	// Serialize from actor with component
	APCGExDeltaTestActorWithComp* Source = SpawnTestActor<APCGExDeltaTestActorWithComp>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	Source->ActorScore = 55;
	Source->TestComp->CompValue = 888;

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);

	// Apply to an actor WITHOUT that component — should not crash
	APCGExPropertyDeltaTestActor* Target = SpawnTestActorDeferred<APCGExPropertyDeltaTestActor>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	// Should not crash — component delta for "TestComp" is safely skipped
	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	// Target's own properties should be unchanged (no matching property names)
	TestEqual(TEXT("TestHealth preserved"), Target->TestHealth, 100);

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Array Edge Cases: Empty array, populated→empty, empty→populated
//////////////////////////////////////////////////////////////////////////

#pragma region ArrayEdgeCases

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADArrayEmptyToPopulatedTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.Array_EmptyToPopulated_RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADArrayEmptyToPopulatedTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Source = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	// CDO has empty TestTags, source adds elements
	Source->TestTags = { FName(TEXT("A")), FName(TEXT("B")) };

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);
	TestTrue(TEXT("Delta non-empty for array change"), Delta.Num() > 0);

	APCGExPropertyDeltaTestActor* Target = SpawnTestActorDeferred<APCGExPropertyDeltaTestActor>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	TestEqual(TEXT("Array populated"), Target->TestTags.Num(), 2);
	if (Target->TestTags.Num() == 2)
	{
		TestEqual(TEXT("Array[0]"), Target->TestTags[0], FName(TEXT("A")));
		TestEqual(TEXT("Array[1]"), Target->TestTags[1], FName(TEXT("B")));
	}

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Partial Property: Only modified properties appear in delta
//////////////////////////////////////////////////////////////////////////

#pragma region PartialDelta

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExADPartialModificationTest,
	"PCGEx.Unit.Collections.ActorPropertyDelta.Adversarial.PartialModification_UnmodifiedPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExADPartialModificationTest::RunTest(const FString& Parameters)
{
	using namespace PCGExActorDeltaTestHelpers;
	UWorld* World = GetTestWorld();
	if (!World) { return false; }

	APCGExPropertyDeltaTestActor* Source = SpawnTestActor<APCGExPropertyDeltaTestActor>(World);
	if (!Source) { AddError(TEXT("Failed to spawn source")); return false; }

	// Modify ONLY TestHealth and TestColor
	Source->TestHealth = 1;
	Source->TestColor = FLinearColor::Blue;

	const TArray<uint8> Delta = PCGExActorDelta::SerializeActorDelta(Source);

	APCGExPropertyDeltaTestActor* Target = SpawnTestActorDeferred<APCGExPropertyDeltaTestActor>(World);
	if (!Target) { AddError(TEXT("Failed to spawn target")); Source->Destroy(); return false; }

	PCGExActorDelta::ApplyPropertyDelta(Target, Delta);
	Target->FinishSpawning(FTransform::Identity);

	// Modified properties applied
	TestEqual(TEXT("Health applied"), Target->TestHealth, 1);
	TestTrue(TEXT("Color applied"), Target->TestColor.Equals(FLinearColor::Blue, 0.001f));

	// All other properties remain at CDO defaults
	TestTrue(TEXT("Radius at CDO"), FMath::IsNearlyEqual(Target->TestRadius, 50.0f));
	TestEqual(TEXT("DisplayName at CDO"), Target->TestDisplayName, FString(TEXT("Default")));
	TestTrue(TEXT("Location at CDO"), Target->TestLocation.Equals(FVector::ZeroVector, 0.001));
	TestTrue(TEXT("Rotation at CDO"), Target->TestRotation.Equals(FRotator::ZeroRotator, 0.001f));
	TestEqual(TEXT("bTestEnabled at CDO"), Target->bTestEnabled, false);
	TestEqual(TEXT("TestCategory at CDO"), Target->TestCategory, NAME_None);
	TestEqual(TEXT("TestTags at CDO"), Target->TestTags.Num(), 0);

	Source->Destroy();
	Target->Destroy();
	return true;
}

#pragma endregion
