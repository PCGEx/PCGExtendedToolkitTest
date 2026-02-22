// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Pattern Struct Unit Tests
 *
 * Tests pure structs from PCGExValencyPattern.h and PCGExValencyCommon.h:
 * - FPCGExValencyPatternEntryCompiled (MatchesModule, IsWildcard)
 * - FPCGExValencyPatternSetCompiled (HasPatterns, ExclusivePatternIndices, AdditivePatternIndices)
 * - FPCGExValencyPatternCompiled (IsValid)
 * - FPCGExValencyPatternMatch (IsValid, GetRootNodeIndex, ComputeCentroid, etc.)
 * - FValencyState (IsResolved, IsBoundary, IsUnsolvable)
 * - FPCGExBoundsModifier (Apply, IsDefault)
 * - FPCGExValencyModuleSettings (HasBehavior, SetBehavior, ClearBehavior)
 *
 * Test naming convention: PCGEx.Unit.Valency.PatternStruct.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExValencyPattern.h"
#include "Core/PCGExValencyCommon.h"

// =============================================================================
// Entry -- MatchesModule with wildcard (empty ModuleIndices)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternEntryMatchesModuleWildcardTest,
	"PCGEx.Unit.Valency.PatternStruct.EntryMatchesModuleWildcard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternEntryMatchesModuleWildcardTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternEntryCompiled Entry;
	// Empty ModuleIndices = wildcard

	TestTrue(TEXT("Wildcard matches module 0"), Entry.MatchesModule(0));
	TestTrue(TEXT("Wildcard matches module 42"), Entry.MatchesModule(42));
	TestTrue(TEXT("Wildcard matches module 999"), Entry.MatchesModule(999));

	return true;
}

// =============================================================================
// Entry -- MatchesModule with specific indices
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternEntryMatchesModuleSpecificTest,
	"PCGEx.Unit.Valency.PatternStruct.EntryMatchesModuleSpecific",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternEntryMatchesModuleSpecificTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternEntryCompiled Entry;
	Entry.ModuleIndices = {0, 3};

	TestTrue(TEXT("Matches module 0"), Entry.MatchesModule(0));
	TestTrue(TEXT("Matches module 3"), Entry.MatchesModule(3));
	TestFalse(TEXT("Does not match module 1"), Entry.MatchesModule(1));
	TestFalse(TEXT("Does not match module 2"), Entry.MatchesModule(2));

	return true;
}

// =============================================================================
// Entry -- IsWildcard
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternEntryIsWildcardTest,
	"PCGEx.Unit.Valency.PatternStruct.EntryIsWildcard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternEntryIsWildcardTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternEntryCompiled WildcardEntry;
	TestTrue(TEXT("Empty ModuleIndices = wildcard"), WildcardEntry.IsWildcard());

	FPCGExValencyPatternEntryCompiled SpecificEntry;
	SpecificEntry.ModuleIndices = {0};
	TestFalse(TEXT("Non-empty ModuleIndices = not wildcard"), SpecificEntry.IsWildcard());

	return true;
}

// =============================================================================
// PatternSet -- HasPatterns
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternSetHasPatternsTest,
	"PCGEx.Unit.Valency.PatternStruct.PatternSetHasPatterns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternSetHasPatternsTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternSetCompiled EmptySet;
	TestFalse(TEXT("Empty set has no patterns"), EmptySet.HasPatterns());

	FPCGExValencyPatternSetCompiled NonEmptySet;
	NonEmptySet.Patterns.AddDefaulted();
	TestTrue(TEXT("Non-empty set has patterns"), NonEmptySet.HasPatterns());

	return true;
}

// =============================================================================
// PatternSet -- Exclusive/Additive indices
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternSetExclusiveAdditiveTest,
	"PCGEx.Unit.Valency.PatternStruct.PatternSetExclusiveAdditive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternSetExclusiveAdditiveTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternSetCompiled Set;
	Set.Patterns.AddDefaulted(3);
	Set.ExclusivePatternIndices = {0, 2};
	Set.AdditivePatternIndices = {1};

	TestEqual(TEXT("2 exclusive patterns"), Set.ExclusivePatternIndices.Num(), 2);
	TestEqual(TEXT("1 additive pattern"), Set.AdditivePatternIndices.Num(), 1);
	TestEqual(TEXT("Total pattern count"), Set.GetPatternCount(), 3);

	return true;
}

// =============================================================================
// Pattern -- IsValid
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternIsValidTest,
	"PCGEx.Unit.Valency.PatternStruct.PatternIsValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternIsValidTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternCompiled EmptyPattern;
	TestFalse(TEXT("Empty entries = not valid"), EmptyPattern.IsValid());

	FPCGExValencyPatternCompiled ValidPattern;
	ValidPattern.Entries.AddDefaulted();
	TestTrue(TEXT("Non-empty entries = valid"), ValidPattern.IsValid());

	return true;
}

// =============================================================================
// Match -- IsValid
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternMatchIsValidTest,
	"PCGEx.Unit.Valency.PatternStruct.MatchIsValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternMatchIsValidTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternMatch InvalidMatch;
	TestFalse(TEXT("Default match is not valid"), InvalidMatch.IsValid());

	FPCGExValencyPatternMatch ValidMatch;
	ValidMatch.PatternIndex = 0;
	ValidMatch.EntryToNode = {5, 10, 15};
	TestTrue(TEXT("Match with pattern and entries is valid"), ValidMatch.IsValid());

	FPCGExValencyPatternMatch NoEntriesMatch;
	NoEntriesMatch.PatternIndex = 0;
	TestFalse(TEXT("Match with no entries is not valid"), NoEntriesMatch.IsValid());

	return true;
}

// =============================================================================
// Match -- GetRootNodeIndex
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternMatchGetRootNodeIndexTest,
	"PCGEx.Unit.Valency.PatternStruct.MatchGetRootNodeIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternMatchGetRootNodeIndexTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternMatch Match;
	Match.PatternIndex = 0;
	Match.EntryToNode = {42, 10, 15};
	TestEqual(TEXT("Root is EntryToNode[0]"), Match.GetRootNodeIndex(), 42);

	FPCGExValencyPatternMatch EmptyMatch;
	TestEqual(TEXT("Empty match returns INDEX_NONE"), EmptyMatch.GetRootNodeIndex(), INDEX_NONE);

	return true;
}

// =============================================================================
// Match -- ComputeCentroid
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternMatchComputeCentroidTest,
	"PCGEx.Unit.Valency.PatternStruct.MatchComputeCentroid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternMatchComputeCentroidTest::RunTest(const FString& Parameters)
{
	// Node indices: 0, 1, 2 map to points 0, 1, 2
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(0, 100, 0)
	};
	TArray<int32> NodeToPointIndex = {0, 1, 2};

	FPCGExValencyPatternMatch Match;
	Match.PatternIndex = 0;
	Match.EntryToNode = {0, 1, 2};

	const FVector Centroid = Match.ComputeCentroid(Positions, NodeToPointIndex);
	const FVector Expected(100.0 / 3.0, 100.0 / 3.0, 0);

	TestTrue(TEXT("Centroid X"),
		FMath::IsNearlyEqual(Centroid.X, Expected.X, 0.01));
	TestTrue(TEXT("Centroid Y"),
		FMath::IsNearlyEqual(Centroid.Y, Expected.Y, 0.01));
	TestTrue(TEXT("Centroid Z"),
		FMath::IsNearlyEqual(Centroid.Z, Expected.Z, 0.01));

	return true;
}

// =============================================================================
// Match -- ComputePatternRotationDelta
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternMatchRotationDeltaTest,
	"PCGEx.Unit.Valency.PatternStruct.MatchComputePatternRotationDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternMatchRotationDeltaTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternMatch Match;
	Match.PatternIndex = 0;
	Match.EntryToNode = {0};

	// Pattern at identity, matched rotated 90 degrees around Z
	const FTransform PatternRoot = FTransform::Identity;
	const FTransform MatchedRoot(FQuat(FVector::UpVector, FMath::DegreesToRadians(90.0)));

	const FQuat Delta = Match.ComputePatternRotationDelta(PatternRoot, MatchedRoot);
	const FQuat Expected = MatchedRoot.GetRotation(); // Since pattern is identity

	TestTrue(TEXT("Rotation delta matches expected"),
		Delta.Equals(Expected, 0.001));

	return true;
}

// =============================================================================
// Match -- TransformPatternToMatched
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternMatchTransformToMatchedTest,
	"PCGEx.Unit.Valency.PatternStruct.MatchTransformPatternToMatched",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternMatchTransformToMatchedTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternMatch Match;
	Match.PatternIndex = 0;
	Match.EntryToNode = {0};

	// Pattern root at origin, matched root at (500, 0, 0)
	const FTransform PatternRoot = FTransform::Identity;
	const FTransform MatchedRoot(FQuat::Identity, FVector(500, 0, 0));

	// Point at (100, 0, 0) in pattern space should be at (600, 0, 0) in matched space
	const FVector Result = Match.TransformPatternToMatched(FVector(100, 0, 0), PatternRoot, MatchedRoot);

	TestTrue(TEXT("Translated X correctly"),
		FMath::IsNearlyEqual(Result.X, 600.0, 0.01));
	TestTrue(TEXT("Y unchanged"),
		FMath::IsNearlyEqual(Result.Y, 0.0, 0.01));

	return true;
}

// =============================================================================
// FValencyState -- Defaults
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStateDefaultsTest,
	"PCGEx.Unit.Valency.PatternStruct.ValencyStateDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStateDefaultsTest::RunTest(const FString& Parameters)
{
	PCGExValency::FValencyState State;

	TestEqual(TEXT("Default NodeIndex is -1"), State.NodeIndex, -1);
	TestEqual(TEXT("Default ResolvedModule is UNSET"), State.ResolvedModule, PCGExValency::SlotState::UNSET);

	return true;
}

// =============================================================================
// FValencyState -- IsResolved
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStateIsResolvedTest,
	"PCGEx.Unit.Valency.PatternStruct.ValencyStateIsResolved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStateIsResolvedTest::RunTest(const FString& Parameters)
{
	PCGExValency::FValencyState State;

	State.ResolvedModule = 5;
	TestTrue(TEXT("Module>=0 is resolved"), State.IsResolved());

	State.ResolvedModule = PCGExValency::SlotState::NULL_SLOT;
	TestTrue(TEXT("NULL_SLOT is resolved"), State.IsResolved());

	State.ResolvedModule = PCGExValency::SlotState::UNSOLVABLE;
	TestTrue(TEXT("UNSOLVABLE is resolved"), State.IsResolved());

	State.ResolvedModule = PCGExValency::SlotState::UNSET;
	TestFalse(TEXT("UNSET is not resolved"), State.IsResolved());

	State.ResolvedModule = PCGExValency::SlotState::PLACEHOLDER;
	TestFalse(TEXT("PLACEHOLDER is not resolved"), State.IsResolved());

	return true;
}

// =============================================================================
// FValencyState -- IsBoundary
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStateIsBoundaryTest,
	"PCGEx.Unit.Valency.PatternStruct.ValencyStateIsBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStateIsBoundaryTest::RunTest(const FString& Parameters)
{
	PCGExValency::FValencyState State;

	State.ResolvedModule = PCGExValency::SlotState::NULL_SLOT;
	TestTrue(TEXT("NULL_SLOT is boundary"), State.IsBoundary());

	State.ResolvedModule = 0;
	TestFalse(TEXT("Module 0 is not boundary"), State.IsBoundary());

	State.ResolvedModule = PCGExValency::SlotState::UNSOLVABLE;
	TestFalse(TEXT("UNSOLVABLE is not boundary"), State.IsBoundary());

	return true;
}

// =============================================================================
// FValencyState -- IsUnsolvable
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStateIsUnsolvableTest,
	"PCGEx.Unit.Valency.PatternStruct.ValencyStateIsUnsolvable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStateIsUnsolvableTest::RunTest(const FString& Parameters)
{
	PCGExValency::FValencyState State;

	State.ResolvedModule = PCGExValency::SlotState::UNSOLVABLE;
	TestTrue(TEXT("UNSOLVABLE returns true"), State.IsUnsolvable());

	State.ResolvedModule = PCGExValency::SlotState::NULL_SLOT;
	TestFalse(TEXT("NULL_SLOT is not unsolvable"), State.IsUnsolvable());

	State.ResolvedModule = 0;
	TestFalse(TEXT("Module 0 is not unsolvable"), State.IsUnsolvable());

	return true;
}

// =============================================================================
// FPCGExBoundsModifier -- Apply
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBoundsModifierApplyTest,
	"PCGEx.Unit.Valency.PatternStruct.BoundsModifierApply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBoundsModifierApplyTest::RunTest(const FString& Parameters)
{
	FPCGExBoundsModifier Modifier;
	Modifier.Scale = FVector(2.0, 1.0, 0.5);
	Modifier.Offset = FVector(10, 0, 0);

	const FBox InputBounds(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FBox Result = Modifier.Apply(InputBounds);

	// Center should be (0,0,0) + (10,0,0) = (10,0,0)
	// Extent should be (50,50,50) * (2,1,0.5) = (100,50,25)
	const FVector ExpectedMin(10 - 100, 0 - 50, 0 - 25);
	const FVector ExpectedMax(10 + 100, 0 + 50, 0 + 25);

	TestTrue(TEXT("Min X"), FMath::IsNearlyEqual(Result.Min.X, ExpectedMin.X, 0.01));
	TestTrue(TEXT("Min Y"), FMath::IsNearlyEqual(Result.Min.Y, ExpectedMin.Y, 0.01));
	TestTrue(TEXT("Min Z"), FMath::IsNearlyEqual(Result.Min.Z, ExpectedMin.Z, 0.01));
	TestTrue(TEXT("Max X"), FMath::IsNearlyEqual(Result.Max.X, ExpectedMax.X, 0.01));
	TestTrue(TEXT("Max Y"), FMath::IsNearlyEqual(Result.Max.Y, ExpectedMax.Y, 0.01));
	TestTrue(TEXT("Max Z"), FMath::IsNearlyEqual(Result.Max.Z, ExpectedMax.Z, 0.01));

	return true;
}

// =============================================================================
// FPCGExBoundsModifier -- IsDefault
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBoundsModifierIsDefaultTest,
	"PCGEx.Unit.Valency.PatternStruct.BoundsModifierIsDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBoundsModifierIsDefaultTest::RunTest(const FString& Parameters)
{
	FPCGExBoundsModifier DefaultModifier;
	TestTrue(TEXT("Default modifier is default"), DefaultModifier.IsDefault());

	FPCGExBoundsModifier ScaledModifier;
	ScaledModifier.Scale = FVector(2.0, 1.0, 1.0);
	TestFalse(TEXT("Scaled modifier is not default"), ScaledModifier.IsDefault());

	FPCGExBoundsModifier OffsetModifier;
	OffsetModifier.Offset = FVector(0, 0, 10);
	TestFalse(TEXT("Offset modifier is not default"), OffsetModifier.IsDefault());

	return true;
}

// =============================================================================
// FPCGExValencyModuleSettings -- Behavior Flags
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyModuleSettingsHasBehaviorTest,
	"PCGEx.Unit.Valency.PatternStruct.ModuleSettingsHasBehavior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyModuleSettingsHasBehaviorTest::RunTest(const FString& Parameters)
{
	FPCGExValencyModuleSettings Settings;

	// Initially no behaviors
	TestFalse(TEXT("No PreferredStart by default"), Settings.HasBehavior(EPCGExModuleBehavior::PreferredStart));
	TestFalse(TEXT("No PreferredEnd by default"), Settings.HasBehavior(EPCGExModuleBehavior::PreferredEnd));
	TestFalse(TEXT("No Greedy by default"), Settings.HasBehavior(EPCGExModuleBehavior::Greedy));

	// Set behaviors
	Settings.SetBehavior(EPCGExModuleBehavior::PreferredStart);
	TestTrue(TEXT("PreferredStart set"), Settings.HasBehavior(EPCGExModuleBehavior::PreferredStart));
	TestFalse(TEXT("PreferredEnd not set"), Settings.HasBehavior(EPCGExModuleBehavior::PreferredEnd));

	Settings.SetBehavior(EPCGExModuleBehavior::Greedy);
	TestTrue(TEXT("Greedy set"), Settings.HasBehavior(EPCGExModuleBehavior::Greedy));
	TestTrue(TEXT("PreferredStart still set"), Settings.HasBehavior(EPCGExModuleBehavior::PreferredStart));

	// Clear behavior
	Settings.ClearBehavior(EPCGExModuleBehavior::PreferredStart);
	TestFalse(TEXT("PreferredStart cleared"), Settings.HasBehavior(EPCGExModuleBehavior::PreferredStart));
	TestTrue(TEXT("Greedy unaffected"), Settings.HasBehavior(EPCGExModuleBehavior::Greedy));

	return true;
}
