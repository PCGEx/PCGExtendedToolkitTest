// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency OrbitalSet Unit Tests
 *
 * Tests UPCGExValencyOrbitalSet + FOrbitalDirectionResolver:
 * - FindOrbitalIndexByName
 * - Num() / IsValidIndex()
 * - FOrbitalDirectionResolver FindMatchingOrbital
 *
 * Test naming convention: PCGEx.Unit.Valency.OrbitalSet.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Helpers/PCGExValencyTestHelpers.h"

// =============================================================================
// FindByName -- Found
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyOrbitalSetFindByNameFoundTest,
	"PCGEx.Unit.Valency.OrbitalSet.FindByNameFound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyOrbitalSetFindByNameFoundTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S"), FName("E"), FName("W")});

	TestEqual(TEXT("N is at index 0"), OrbitalSet->FindOrbitalIndexByName(FName("N")), 0);
	TestEqual(TEXT("S is at index 1"), OrbitalSet->FindOrbitalIndexByName(FName("S")), 1);
	TestEqual(TEXT("E is at index 2"), OrbitalSet->FindOrbitalIndexByName(FName("E")), 2);
	TestEqual(TEXT("W is at index 3"), OrbitalSet->FindOrbitalIndexByName(FName("W")), 3);

	return true;
}

// =============================================================================
// FindByName -- Not Found
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyOrbitalSetFindByNameNotFoundTest,
	"PCGEx.Unit.Valency.OrbitalSet.FindByNameNotFound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyOrbitalSetFindByNameNotFoundTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S"), FName("E"), FName("W")});

	TestEqual(TEXT("X returns INDEX_NONE"), OrbitalSet->FindOrbitalIndexByName(FName("X")), INDEX_NONE);
	TestEqual(TEXT("Unknown returns INDEX_NONE"), OrbitalSet->FindOrbitalIndexByName(FName("Unknown")), INDEX_NONE);

	return true;
}

// =============================================================================
// FindByName -- Empty Set
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyOrbitalSetFindByNameEmptyTest,
	"PCGEx.Unit.Valency.OrbitalSet.FindByNameEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyOrbitalSetFindByNameEmptyTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet({});

	TestEqual(TEXT("Any name returns INDEX_NONE on empty set"),
		OrbitalSet->FindOrbitalIndexByName(FName("N")), INDEX_NONE);

	return true;
}

// =============================================================================
// Num() matches entry count
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyOrbitalSetNumOrbitalsTest,
	"PCGEx.Unit.Valency.OrbitalSet.NumOrbitals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyOrbitalSetNumOrbitalsTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* EmptySet = PCGExTest::ValencyHelpers::CreateOrbitalSet({});
	TestEqual(TEXT("Empty set Num()=0"), EmptySet->Num(), 0);

	UPCGExValencyOrbitalSet* FourSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S"), FName("E"), FName("W")});
	TestEqual(TEXT("4-entry set Num()=4"), FourSet->Num(), 4);

	return true;
}

// =============================================================================
// Resolver -- FindExact
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyOrbitalSetResolverFindExactTest,
	"PCGEx.Unit.Valency.OrbitalSet.ResolverFindExact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyOrbitalSetResolverFindExactTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> Dirs = {FVector::ForwardVector, FVector::RightVector, FVector::UpVector};
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver(Dirs);

	TestTrue(TEXT("Resolver is valid"), Resolver.IsValid());
	TestEqual(TEXT("Resolver has 3 entries"), Resolver.Num(), 3);

	const uint8 ForwardIdx = Resolver.FindMatchingOrbital(FVector::ForwardVector, false, FTransform::Identity);
	TestEqual(TEXT("Forward matches index 0"), ForwardIdx, static_cast<uint8>(0));

	const uint8 RightIdx = Resolver.FindMatchingOrbital(FVector::RightVector, false, FTransform::Identity);
	TestEqual(TEXT("Right matches index 1"), RightIdx, static_cast<uint8>(1));

	const uint8 UpIdx = Resolver.FindMatchingOrbital(FVector::UpVector, false, FTransform::Identity);
	TestEqual(TEXT("Up matches index 2"), UpIdx, static_cast<uint8>(2));

	return true;
}

// =============================================================================
// Resolver -- Within Threshold
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyOrbitalSetResolverWithinThresholdTest,
	"PCGEx.Unit.Valency.OrbitalSet.ResolverWithinThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyOrbitalSetResolverWithinThresholdTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> Dirs = {FVector::ForwardVector, FVector::RightVector, FVector::UpVector};
	// 22.5 degrees threshold -- wide enough for slight deviation
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver(Dirs, 22.5);

	// Slightly off-axis forward (5 degrees off)
	const FVector SlightlyOff = FVector::ForwardVector.RotateAngleAxis(5.0, FVector::UpVector);
	const uint8 Idx = Resolver.FindMatchingOrbital(SlightlyOff.GetSafeNormal(), false, FTransform::Identity);

	TestEqual(TEXT("Slightly off-axis still matches Forward"), Idx, static_cast<uint8>(0));

	return true;
}

// =============================================================================
// Resolver -- Outside Threshold
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyOrbitalSetResolverOutsideThresholdTest,
	"PCGEx.Unit.Valency.OrbitalSet.ResolverOutsideThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyOrbitalSetResolverOutsideThresholdTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> Dirs = {FVector::ForwardVector, FVector::RightVector};
	// Very tight threshold (5 degrees)
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver(Dirs, 5.0);

	// 45 degrees between Forward and Right -- should NOT match either
	const FVector Between = (FVector::ForwardVector + FVector::RightVector).GetSafeNormal();
	const uint8 Idx = Resolver.FindMatchingOrbital(Between, false, FTransform::Identity);

	TestEqual(TEXT("Between directions returns NO_ORBITAL_MATCH"),
		Idx, PCGExValency::NO_ORBITAL_MATCH);

	return true;
}

// =============================================================================
// Resolver -- Empty
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyOrbitalSetResolverEmptyTest,
	"PCGEx.Unit.Valency.OrbitalSet.ResolverEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyOrbitalSetResolverEmptyTest::RunTest(const FString& Parameters)
{
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver({});

	TestFalse(TEXT("Empty resolver is not valid"), Resolver.IsValid());
	TestEqual(TEXT("Empty resolver has 0 entries"), Resolver.Num(), 0);

	const uint8 Idx = Resolver.FindMatchingOrbital(FVector::ForwardVector, false, FTransform::Identity);
	TestEqual(TEXT("Any direction returns NO_ORBITAL_MATCH"),
		Idx, PCGExValency::NO_ORBITAL_MATCH);

	return true;
}
