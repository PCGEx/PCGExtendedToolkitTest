// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Adversarial Unit Tests
 *
 * Tests what happens when users provide garbage, out-of-range, null, contradictory,
 * or otherwise unexpected inputs. These should NOT crash — they should fail gracefully
 * with correct sentinel values, silently ignore, or produce deterministic results.
 *
 * Categories:
 * - Garbage indices (negative, INT_MAX, out-of-range)
 * - Overflow inputs (exceed storage width)
 * - Null/empty inputs (empty paths, NAME_None, zero vectors)
 * - Contradictory state (boundary AND wildcard on same orbital)
 * - Degenerate geometry (zero-scale transforms, zero-length directions, NaN positions)
 * - Assembler abuse (operations on non-existent modules)
 *
 * Test naming convention: PCGEx.Unit.Valency.Adversarial.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExValencyCommon.h"
#include "Core/PCGExValencyMap.h"
#include "Core/PCGExValencyPattern.h"
#include "Core/PCGExBondingRulesAssembler.h"
#include "Core/PCGExBondingRules.h"
#include "Helpers/PCGExValencyTestHelpers.h"

using namespace PCGExValency;

// =============================================================================
// EntryData — Pack with zero BondingRulesMapId (collides with INVALID_ENTRY)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialEntryPackZeroIdTest,
	"PCGEx.Unit.Valency.Adversarial.EntryData.PackZeroRulesId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialEntryPackZeroIdTest::RunTest(const FString& Parameters)
{
	// Pack(0, 0, 0) produces the same value as INVALID_ENTRY (0).
	// This is a known design decision — callers must avoid BondingRulesMapId=0 + ModuleIndex=0 + Flags=0.
	// Verify the behavior is at least deterministic.
	const uint64 Hash = EntryData::Pack(0, 0, 0);

	TestEqual(TEXT("Pack(0,0,0) equals INVALID_ENTRY"), Hash, EntryData::INVALID_ENTRY);
	TestFalse(TEXT("Pack(0,0,0) is not valid"), EntryData::IsValid(Hash));

	// Pack(0, 1, 0) should still be valid — only the exact zero sentinel is invalid
	const uint64 HashNonZero = EntryData::Pack(0, 1, 0);
	TestTrue(TEXT("Pack(0,1,0) is valid"), EntryData::IsValid(HashNonZero));

	return true;
}

// =============================================================================
// EntryData — Pack with max uint16 module index
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialEntryPackMaxModuleTest,
	"PCGEx.Unit.Valency.Adversarial.EntryData.PackMaxModuleIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialEntryPackMaxModuleTest::RunTest(const FString& Parameters)
{
	const uint16 MaxModule = MAX_uint16;
	const uint64 Hash = EntryData::Pack(1, MaxModule, EntryData::Flags::Consumed);

	TestTrue(TEXT("Max module hash is valid"), EntryData::IsValid(Hash));
	TestEqual(TEXT("Max module roundtrips"), EntryData::GetModuleIndex(Hash), static_cast<int32>(MaxModule));
	TestEqual(TEXT("RulesId preserved"), EntryData::GetBondingRulesMapId(Hash), 1u);
	TestEqual(TEXT("Flags preserved"), EntryData::GetPatternFlags(Hash), EntryData::Flags::Consumed);

	return true;
}

// =============================================================================
// EntryData — All flags set simultaneously
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialEntryAllFlagsTest,
	"PCGEx.Unit.Valency.Adversarial.EntryData.AllFlagsSimultaneous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialEntryAllFlagsTest::RunTest(const FString& Parameters)
{
	const uint16 AllFlags = EntryData::Flags::Consumed | EntryData::Flags::Swapped |
		EntryData::Flags::Collapsed | EntryData::Flags::Annotated;

	const uint64 Hash = EntryData::Pack(42, 7, AllFlags);

	TestTrue(TEXT("All flags hash is valid"), EntryData::IsValid(Hash));
	TestTrue(TEXT("Has Consumed"), EntryData::HasFlag(Hash, EntryData::Flags::Consumed));
	TestTrue(TEXT("Has Swapped"), EntryData::HasFlag(Hash, EntryData::Flags::Swapped));
	TestTrue(TEXT("Has Collapsed"), EntryData::HasFlag(Hash, EntryData::Flags::Collapsed));
	TestTrue(TEXT("Has Annotated"), EntryData::HasFlag(Hash, EntryData::Flags::Annotated));
	TestEqual(TEXT("All flags preserved"), EntryData::GetPatternFlags(Hash), AllFlags);

	return true;
}

// =============================================================================
// EntryData — SetFlag then ClearFlag same flag = round trip
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialEntrySetClearSameFlagTest,
	"PCGEx.Unit.Valency.Adversarial.EntryData.SetThenClearSameFlag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialEntrySetClearSameFlagTest::RunTest(const FString& Parameters)
{
	const uint64 Original = EntryData::Pack(10, 5, EntryData::Flags::None);
	const uint64 WithFlag = EntryData::SetFlag(Original, EntryData::Flags::Consumed);
	const uint64 Cleared = EntryData::ClearFlag(WithFlag, EntryData::Flags::Consumed);

	// Should return to original state
	TestEqual(TEXT("Set then Clear returns to original flags"),
		EntryData::GetPatternFlags(Cleared), EntryData::Flags::None);
	TestEqual(TEXT("BondingRulesMapId preserved through set/clear"),
		EntryData::GetBondingRulesMapId(Cleared), 10u);
	TestEqual(TEXT("ModuleIndex preserved through set/clear"),
		EntryData::GetModuleIndex(Cleared), 5);

	return true;
}

// =============================================================================
// EntryData — ClearFlag on flag that isn't set (no-op)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialEntryClearUnsetFlagTest,
	"PCGEx.Unit.Valency.Adversarial.EntryData.ClearFlagNotSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialEntryClearUnsetFlagTest::RunTest(const FString& Parameters)
{
	const uint64 Hash = EntryData::Pack(10, 5, EntryData::Flags::Consumed);
	const uint64 Cleared = EntryData::ClearFlag(Hash, EntryData::Flags::Swapped);

	// Swapped was never set — clearing it should be a no-op
	TestEqual(TEXT("Consumed flag still present"),
		EntryData::GetPatternFlags(Cleared), EntryData::Flags::Consumed);
	TestTrue(TEXT("Has Consumed"), EntryData::HasFlag(Cleared, EntryData::Flags::Consumed));
	TestFalse(TEXT("Still no Swapped"), EntryData::HasFlag(Cleared, EntryData::Flags::Swapped));

	return true;
}

// =============================================================================
// EntryData — SetFlag twice (idempotent)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialEntrySetFlagTwiceTest,
	"PCGEx.Unit.Valency.Adversarial.EntryData.SetFlagTwice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialEntrySetFlagTwiceTest::RunTest(const FString& Parameters)
{
	const uint64 Hash = EntryData::Pack(10, 5, EntryData::Flags::None);
	const uint64 Once = EntryData::SetFlag(Hash, EntryData::Flags::Consumed);
	const uint64 Twice = EntryData::SetFlag(Once, EntryData::Flags::Consumed);

	TestEqual(TEXT("Double SetFlag is idempotent"),
		EntryData::GetPatternFlags(Twice), EntryData::Flags::Consumed);

	return true;
}

// =============================================================================
// EdgeOrbital — Asymmetric pack (start != end)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialEdgeOrbitalAsymmetricTest,
	"PCGEx.Unit.Valency.Adversarial.EdgePack.OrbitalAsymmetric",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialEdgeOrbitalAsymmetricTest::RunTest(const FString& Parameters)
{
	// Start and end should never get swapped
	const int64 PackedAB = EdgeOrbital::Pack(100, 200);
	const int64 PackedBA = EdgeOrbital::Pack(200, 100);

	TestNotEqual(TEXT("(100,200) != (200,100)"), PackedAB, PackedBA);
	TestEqual(TEXT("Start of AB is 100"), EdgeOrbital::GetStartOrbital(PackedAB), static_cast<uint8>(100));
	TestEqual(TEXT("End of AB is 200"), EdgeOrbital::GetEndOrbital(PackedAB), static_cast<uint8>(200));
	TestEqual(TEXT("Start of BA is 200"), EdgeOrbital::GetStartOrbital(PackedBA), static_cast<uint8>(200));
	TestEqual(TEXT("End of BA is 100"), EdgeOrbital::GetEndOrbital(PackedBA), static_cast<uint8>(100));

	return true;
}

// =============================================================================
// EdgeOrbital — Pack with sentinel value (0xFF) as data
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialEdgeOrbitalSentinelAsDataTest,
	"PCGEx.Unit.Valency.Adversarial.EdgePack.OrbitalSentinelAsData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialEdgeOrbitalSentinelAsDataTest::RunTest(const FString& Parameters)
{
	// What if user has exactly 255 orbitals and tries to use index 255?
	// 255 = 0xFF = NO_MATCH sentinel — these are indistinguishable
	const int64 Packed = EdgeOrbital::Pack(255, 255);
	const int64 Sentinel = EdgeOrbital::NoMatchSentinel();

	TestEqual(TEXT("Pack(255,255) IS the sentinel — by design"),
		Packed, Sentinel);
	TestEqual(TEXT("Both extract as NO_MATCH start"),
		EdgeOrbital::GetStartOrbital(Packed), EdgeOrbital::NO_MATCH);
	TestEqual(TEXT("Both extract as NO_MATCH end"),
		EdgeOrbital::GetEndOrbital(Packed), EdgeOrbital::NO_MATCH);

	return true;
}

// =============================================================================
// EdgeConnector — Negative indices
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialEdgeConnectorNegativeTest,
	"PCGEx.Unit.Valency.Adversarial.EdgePack.ConnectorNegativeIndices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialEdgeConnectorNegativeTest::RunTest(const FString& Parameters)
{
	// Negative indices should roundtrip — H64/H64A/H64B handle int32→uint32 reinterpretation
	const int64 Packed = EdgeConnector::Pack(-1, -1);

	TestEqual(TEXT("Negative source roundtrips"), EdgeConnector::GetSourceIndex(Packed), -1);
	TestEqual(TEXT("Negative target roundtrips"), EdgeConnector::GetTargetIndex(Packed), -1);

	return true;
}

// =============================================================================
// EdgeConnector — Large indices
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialEdgeConnectorLargeTest,
	"PCGEx.Unit.Valency.Adversarial.EdgePack.ConnectorLargeIndices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialEdgeConnectorLargeTest::RunTest(const FString& Parameters)
{
	const int64 Packed = EdgeConnector::Pack(INT32_MAX, INT32_MAX);

	TestEqual(TEXT("INT32_MAX source roundtrips"), EdgeConnector::GetSourceIndex(Packed), INT32_MAX);
	TestEqual(TEXT("INT32_MAX target roundtrips"), EdgeConnector::GetTargetIndex(Packed), INT32_MAX);

	return true;
}

// =============================================================================
// MakeModuleKey — Empty asset path
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialModuleKeyEmptyPathTest,
	"PCGEx.Unit.Valency.Adversarial.ModuleKey.EmptyPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialModuleKeyEmptyPathTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath EmptyPath;
	const FString Key = PCGExValency::MakeModuleKey(EmptyPath, 0b1111);

	TestTrue(TEXT("Empty path produces non-empty key"), Key.Len() > 0);

	// Two empty paths with same mask should produce same key (deterministic)
	const FSoftObjectPath EmptyPath2;
	const FString Key2 = PCGExValency::MakeModuleKey(EmptyPath2, 0b1111);
	TestEqual(TEXT("Two empty paths produce same key"), Key, Key2);

	// Empty path with different mask should differ
	const FString KeyDiffMask = PCGExValency::MakeModuleKey(EmptyPath, 0b0001);
	TestNotEqual(TEXT("Empty path with different mask differs"), Key, KeyDiffMask);

	return true;
}

// =============================================================================
// MakeModuleKey — Negative orbital mask
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialModuleKeyNegativeMaskTest,
	"PCGEx.Unit.Valency.Adversarial.ModuleKey.NegativeMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialModuleKeyNegativeMaskTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath Path(TEXT("/Game/Meshes/Cube.Cube"));

	// -1 as int64 = all bits set (0xFFFFFFFFFFFFFFFF)
	const FString KeyNeg1 = PCGExValency::MakeModuleKey(Path, -1);
	const FString KeyAllBits = PCGExValency::MakeModuleKey(Path, static_cast<int64>(0xFFFFFFFFFFFFFFFF));

	TestEqual(TEXT("-1 and all-bits-set produce same key"), KeyNeg1, KeyAllBits);
	TestTrue(TEXT("Negative mask key is non-empty"), KeyNeg1.Len() > 0);

	return true;
}

// =============================================================================
// MakeModuleKey — Material variant with empty overrides array vs nullptr
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialModuleKeyEmptyVariantTest,
	"PCGEx.Unit.Valency.Adversarial.ModuleKey.EmptyVariantFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialModuleKeyEmptyVariantTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath Path(TEXT("/Game/Meshes/Cube.Cube"));
	const int64 Mask = 0b1111;

	// Variant with null material references
	FPCGExValencyMaterialVariant NullMatVariant;
	FPCGExValencyMaterialOverride Override;
	Override.SlotIndex = 0;
	Override.Material = TSoftObjectPtr<UMaterialInterface>(); // null
	NullMatVariant.Overrides.Add(Override);

	const FString Key = PCGExValency::MakeModuleKey(Path, Mask, &NullMatVariant);
	TestTrue(TEXT("Null-material variant produces valid key"), Key.Len() > 0);

	// Should differ from no-variant key since Overrides.Num() > 0
	const FString NoVariantKey = PCGExValency::MakeModuleKey(Path, Mask, nullptr);
	TestNotEqual(TEXT("Null-material variant differs from no variant"), Key, NoVariantKey);

	return true;
}

// =============================================================================
// FValencyState — All slot state constants are unique
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialSlotStateUniquenessTest,
	"PCGEx.Unit.Valency.Adversarial.SlotState.AllUnique",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialSlotStateUniquenessTest::RunTest(const FString& Parameters)
{
	TSet<int32> States;
	States.Add(SlotState::UNSET);
	States.Add(SlotState::NULL_SLOT);
	States.Add(SlotState::UNSOLVABLE);
	States.Add(SlotState::PLACEHOLDER);

	TestEqual(TEXT("All 4 slot states are unique values"), States.Num(), 4);

	// None of them should be >= 0 (module indices are non-negative)
	TestTrue(TEXT("UNSET < 0"), SlotState::UNSET < 0);
	TestTrue(TEXT("NULL_SLOT < 0"), SlotState::NULL_SLOT < 0);
	TestTrue(TEXT("UNSOLVABLE < 0"), SlotState::UNSOLVABLE < 0);
	TestTrue(TEXT("PLACEHOLDER < 0"), SlotState::PLACEHOLDER < 0);

	return true;
}

// =============================================================================
// FValencyState — PLACEHOLDER is not resolved
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialStatePlaceholderTest,
	"PCGEx.Unit.Valency.Adversarial.SlotState.PlaceholderNotResolved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialStatePlaceholderTest::RunTest(const FString& Parameters)
{
	FValencyState State;
	State.ResolvedModule = SlotState::PLACEHOLDER;

	TestFalse(TEXT("PLACEHOLDER is NOT resolved"), State.IsResolved());
	TestFalse(TEXT("PLACEHOLDER is NOT boundary"), State.IsBoundary());
	TestFalse(TEXT("PLACEHOLDER is NOT unsolvable"), State.IsUnsolvable());

	return true;
}

// =============================================================================
// FPCGExBoundsModifier — Zero scale collapses bounds
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialBoundsModifierZeroScaleTest,
	"PCGEx.Unit.Valency.Adversarial.BoundsModifier.ZeroScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialBoundsModifierZeroScaleTest::RunTest(const FString& Parameters)
{
	FPCGExBoundsModifier Modifier;
	Modifier.Scale = FVector::ZeroVector;
	Modifier.Offset = FVector::ZeroVector;

	const FBox Input(FVector(-100, -100, -100), FVector(100, 100, 100));
	const FBox Result = Modifier.Apply(Input);

	// Zero scale → extent becomes zero → degenerate box
	TestTrue(TEXT("Zero-scale produces degenerate bounds (volume=0)"),
		FMath::IsNearlyEqual(Result.GetVolume(), 0.0, 0.01));

	return true;
}

// =============================================================================
// FPCGExBoundsModifier — Negative scale flips bounds
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialBoundsModifierNegativeScaleTest,
	"PCGEx.Unit.Valency.Adversarial.BoundsModifier.NegativeScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialBoundsModifierNegativeScaleTest::RunTest(const FString& Parameters)
{
	FPCGExBoundsModifier Modifier;
	Modifier.Scale = FVector(-1.0, -1.0, -1.0);
	Modifier.Offset = FVector::ZeroVector;

	const FBox Input(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FBox Result = Modifier.Apply(Input);

	// Negative scale: Extent * (-1) = negative extent
	// Box constructor doesn't auto-fix min/max ordering
	// Center=0, Extent=(50*-1)=(-50) → Min = 0-(-50)=50, Max = 0+(-50)=-50
	// This produces an inverted box — the box IS valid but inverted
	TestTrue(TEXT("Negative scale produces inverted box (Min > Max)"),
		Result.Min.X > Result.Max.X || FMath::IsNearlyEqual(Result.GetVolume(), 0.0, 0.01));

	return true;
}

// =============================================================================
// PatternMatch — ComputeCentroid with empty positions array
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialCentroidEmptyPositionsTest,
	"PCGEx.Unit.Valency.Adversarial.PatternMatch.CentroidEmptyPositions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialCentroidEmptyPositionsTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternMatch Match;
	Match.PatternIndex = 0;
	Match.EntryToNode = {0, 1, 2};

	TArray<FVector> EmptyPositions;
	TArray<int32> NodeToPointIndex = {0, 1, 2};

	// Positions array empty — all indices out of range
	const FVector Centroid = Match.ComputeCentroid(EmptyPositions, NodeToPointIndex);

	// Should return ZeroVector since no valid points found
	TestTrue(TEXT("Centroid of empty positions is zero"),
		Centroid.Equals(FVector::ZeroVector, 0.01));

	return true;
}

// =============================================================================
// PatternMatch — ComputeCentroid with all INDEX_NONE mappings
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialCentroidAllInvalidMappingsTest,
	"PCGEx.Unit.Valency.Adversarial.PatternMatch.CentroidAllInvalidMappings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialCentroidAllInvalidMappingsTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternMatch Match;
	Match.PatternIndex = 0;
	Match.EntryToNode = {0, 1, 2};

	TArray<FVector> Positions = {FVector(100, 0, 0), FVector(0, 100, 0), FVector(0, 0, 100)};
	TArray<int32> NodeToPointIndex = {INDEX_NONE, INDEX_NONE, INDEX_NONE};

	const FVector Centroid = Match.ComputeCentroid(Positions, NodeToPointIndex);
	TestTrue(TEXT("Centroid with all INDEX_NONE is zero"),
		Centroid.Equals(FVector::ZeroVector, 0.01));

	return true;
}

// =============================================================================
// PatternMatch — ComputeCentroid with empty EntryToNode
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialCentroidEmptyMatchTest,
	"PCGEx.Unit.Valency.Adversarial.PatternMatch.CentroidEmptyMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialCentroidEmptyMatchTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternMatch Match;
	Match.PatternIndex = 0;
	// EntryToNode is empty

	TArray<FVector> Positions = {FVector(100, 0, 0)};
	TArray<int32> NodeToPointIndex = {0};

	const FVector Centroid = Match.ComputeCentroid(Positions, NodeToPointIndex);
	TestTrue(TEXT("Centroid with empty match is zero"),
		Centroid.Equals(FVector::ZeroVector, 0.01));

	return true;
}

// =============================================================================
// PatternMatch — TransformPatternToMatched with identity transforms
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialTransformIdentityTest,
	"PCGEx.Unit.Valency.Adversarial.PatternMatch.TransformIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialTransformIdentityTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternMatch Match;
	Match.PatternIndex = 0;
	Match.EntryToNode = {0};

	// Both transforms at identity → output should equal input
	const FVector Input(42, 99, -7);
	const FVector Result = Match.TransformPatternToMatched(Input, FTransform::Identity, FTransform::Identity);

	TestTrue(TEXT("Identity transforms pass through position"),
		Result.Equals(Input, 0.01));

	return true;
}

// =============================================================================
// PatternMatch — ComputePatternRotationDelta with same transform
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialRotationDeltaSameTransformTest,
	"PCGEx.Unit.Valency.Adversarial.PatternMatch.RotationDeltaSameTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialRotationDeltaSameTransformTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternMatch Match;
	Match.PatternIndex = 0;
	Match.EntryToNode = {0};

	const FTransform SomeTransform(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(45.0)),
		FVector(100, 200, 300));

	const FQuat Delta = Match.ComputePatternRotationDelta(SomeTransform, SomeTransform);

	// Same transforms → delta should be identity
	TestTrue(TEXT("Same transform gives identity delta"),
		Delta.Equals(FQuat::Identity, 0.001));

	return true;
}

// =============================================================================
// PatternEntry — MatchesModule with negative index
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialEntryMatchesNegativeModuleTest,
	"PCGEx.Unit.Valency.Adversarial.PatternEntry.MatchesNegativeModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialEntryMatchesNegativeModuleTest::RunTest(const FString& Parameters)
{
	// Wildcard should match anything including negative
	FPCGExValencyPatternEntryCompiled WildcardEntry;
	TestTrue(TEXT("Wildcard matches -1"), WildcardEntry.MatchesModule(-1));

	// Specific entry should not match -1 unless explicitly listed
	FPCGExValencyPatternEntryCompiled SpecificEntry;
	SpecificEntry.ModuleIndices = {0, 1, 2};
	TestFalse(TEXT("Specific entry does not match -1"), SpecificEntry.MatchesModule(-1));

	return true;
}

// =============================================================================
// LayerConfig — Contradictory: Boundary AND Wildcard on same orbital
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialLayerConfigContradictoryTest,
	"PCGEx.Unit.Valency.Adversarial.LayerConfig.BoundaryAndWildcardSameOrbital",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialLayerConfigContradictoryTest::RunTest(const FString& Parameters)
{
	FPCGExValencyModuleLayerConfig Config;

	// Set boundary on bit 0
	Config.SetBoundaryOrbital(0);
	// Now also set wildcard on bit 0 — this violates the invariant
	Config.SetWildcardOrbital(0);

	// Both flags should now be set (no automatic exclusion)
	TestTrue(TEXT("Both boundary and wildcard are set on bit 0 — CONTRADICTORY"),
		Config.IsBoundaryOrbital(0) && Config.IsWildcardOrbital(0));

	// SetWildcardOrbital also sets OrbitalMask
	TestTrue(TEXT("OrbitalMask has bit 0 from wildcard"), Config.HasOrbital(0));

	return true;
}

// =============================================================================
// LayerConfig — Orbital bit index at boundary of int64 (bit 63)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialLayerConfigBit63Test,
	"PCGEx.Unit.Valency.Adversarial.LayerConfig.Bit63",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialLayerConfigBit63Test::RunTest(const FString& Parameters)
{
	FPCGExValencyModuleLayerConfig Config;

	Config.SetOrbital(63);
	TestTrue(TEXT("Bit 63 set correctly"), Config.HasOrbital(63));

	Config.SetBoundaryOrbital(63);
	TestTrue(TEXT("Bit 63 boundary set"), Config.IsBoundaryOrbital(63));

	// Verify bit 0 is NOT affected
	TestFalse(TEXT("Bit 0 not set"), Config.HasOrbital(0));

	return true;
}

// =============================================================================
// OrbitalSet — FindOrbitalIndexByName with NAME_None
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialOrbitalSetNameNoneTest,
	"PCGEx.Unit.Valency.Adversarial.OrbitalSet.FindNameNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialOrbitalSetNameNoneTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});

	TestEqual(TEXT("NAME_None returns INDEX_NONE"),
		OrbitalSet->FindOrbitalIndexByName(NAME_None), INDEX_NONE);

	return true;
}

// =============================================================================
// OrbitalSet — Duplicate orbital names
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialOrbitalSetDuplicateNamesTest,
	"PCGEx.Unit.Valency.Adversarial.OrbitalSet.DuplicateNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialOrbitalSetDuplicateNamesTest::RunTest(const FString& Parameters)
{
	// Two orbitals with same name — FindOrbitalIndexByName returns first match
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("N"), FName("S")});

	TestEqual(TEXT("Duplicate name returns first index"), OrbitalSet->FindOrbitalIndexByName(FName("N")), 0);
	TestEqual(TEXT("S returns index 2"), OrbitalSet->FindOrbitalIndexByName(FName("S")), 2);
	TestEqual(TEXT("Num includes duplicates"), OrbitalSet->Num(), 3);

	return true;
}

// =============================================================================
// Resolver — Zero-length direction input
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialResolverZeroDirectionTest,
	"PCGEx.Unit.Valency.Adversarial.Resolver.ZeroLengthDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialResolverZeroDirectionTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> Dirs = {FVector::ForwardVector, FVector::RightVector};
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver(Dirs, 22.5);

	// Query with zero vector — GetSafeNormal returns zero
	const uint8 Idx = Resolver.FindMatchingOrbital(FVector::ZeroVector, false, FTransform::Identity);

	// Should either return NO_ORBITAL_MATCH or some index — must not crash
	// Dot product of (0,0,0) with anything = 0, which is below typical threshold
	TestTrue(TEXT("Zero direction does not crash (returns valid or NO_MATCH)"),
		Idx == NO_ORBITAL_MATCH || Idx < Resolver.Num());

	return true;
}

// =============================================================================
// Resolver — Opposite direction of all orbitals
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialResolverOppositeDirectionTest,
	"PCGEx.Unit.Valency.Adversarial.Resolver.OppositeDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialResolverOppositeDirectionTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> Dirs = {FVector::ForwardVector};
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver(Dirs, 22.5);

	// Query with exact opposite direction (dot = -1)
	const uint8 Idx = Resolver.FindMatchingOrbital(FVector::BackwardVector, false, FTransform::Identity);

	TestEqual(TEXT("Opposite direction returns NO_ORBITAL_MATCH"),
		Idx, NO_ORBITAL_MATCH);

	return true;
}

// =============================================================================
// Assembler — Operations on non-existent module indices
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialAssemblerBadIndexTest,
	"PCGEx.Unit.Valency.Adversarial.Assembler.OperationsOnBadIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialAssemblerBadIndexTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	// All these should silently no-op, not crash
	Assembler.AddLocalTransform(-1, FTransform::Identity);
	Assembler.AddLocalTransform(999, FTransform::Identity);
	Assembler.AddTag(-1, FName("BadTag"));
	Assembler.AddTag(999, FName("BadTag"));
	Assembler.AddNeighbors(-1, FName("N"), {0});
	Assembler.AddNeighbors(999, FName("N"), {0});
	Assembler.SetBoundaryOrbital(-1, 0);
	Assembler.SetBoundaryOrbital(999, 0);
	Assembler.SetWildcardOrbital(-1, 0);
	Assembler.SetWildcardOrbital(999, 0);

	// Should still have 0 modules and not crash
	TestEqual(TEXT("Still 0 modules after bad operations"), Assembler.GetModuleCount(), 0);

	return true;
}

// =============================================================================
// Assembler — Self-referencing neighbor
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialAssemblerSelfNeighborTest,
	"PCGEx.Unit.Valency.Adversarial.Assembler.SelfReferencingNeighbor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialAssemblerSelfNeighborTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	const int32 Idx = Assembler.AddModule(
		TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/Self.Self"))), 0b01);

	// Module 0 is its own neighbor
	Assembler.AddNeighbors(Idx, FName("North"), {Idx});

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet({FName("North")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	const FPCGExAssemblerResult Result = Assembler.Apply(Rules);

	// Self-neighbor should be legal (e.g., repeated tile pattern)
	TestTrue(TEXT("Self-neighbor apply succeeds"), Result.bSuccess);
	TestEqual(TEXT("1 module"), Result.ModuleCount, 1);

	return true;
}

// =============================================================================
// Assembler — Duplicate neighbors in same call
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialAssemblerDuplicateNeighborsTest,
	"PCGEx.Unit.Valency.Adversarial.Assembler.DuplicateNeighbors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialAssemblerDuplicateNeighborsTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	const int32 A = Assembler.AddModule(
		TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/A.A"))), 0b01);
	const int32 B = Assembler.AddModule(
		TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/B.B"))), 0b10);

	// Add B as neighbor of A twice
	Assembler.AddNeighbors(A, FName("North"), {B, B, B});

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet({FName("North")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	const FPCGExAssemblerResult Result = Assembler.Apply(Rules);
	TestTrue(TEXT("Duplicate neighbors apply succeeds"), Result.bSuccess);

	return true;
}

// =============================================================================
// Assembler — Apply to same rules twice
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialAssemblerApplyTwiceTest,
	"PCGEx.Unit.Valency.Adversarial.Assembler.ApplyTwice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialAssemblerApplyTwiceTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	Assembler.AddModule(
		TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/A.A"))), 0b01);

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet({FName("North")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	// Apply twice — second should replace first (bClearExisting=true default)
	Assembler.Apply(Rules);
	const FPCGExAssemblerResult Result2 = Assembler.Apply(Rules);

	TestTrue(TEXT("Second apply succeeds"), Result2.bSuccess);
	TestEqual(TEXT("Still 1 module after double apply"), Result2.ModuleCount, 1);
	TestEqual(TEXT("Rules has 1 module"), Rules->GetModuleCount(), 1);

	return true;
}

// =============================================================================
// Assembler — Apply with bClearExisting=false (append mode)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialAssemblerAppendApplyTest,
	"PCGEx.Unit.Valency.Adversarial.Assembler.ApplyAppendMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialAssemblerAppendApplyTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet({FName("North")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	// First apply
	FPCGExBondingRulesAssembler Assembler1;
	Assembler1.AddModule(
		TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/A.A"))), 0b01);
	Assembler1.Apply(Rules, true);
	TestEqual(TEXT("1 module after first apply"), Rules->GetModuleCount(), 1);

	// Second apply with bClearExisting=false — should append
	FPCGExBondingRulesAssembler Assembler2;
	Assembler2.AddModule(
		TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/B.B"))), 0b01);
	const FPCGExAssemblerResult Result = Assembler2.Apply(Rules, false);

	TestTrue(TEXT("Append apply succeeds"), Result.bSuccess);
	TestEqual(TEXT("2 modules after append"), Rules->GetModuleCount(), 2);

	return true;
}

// =============================================================================
// Assembler — Empty neighbor list
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialAssemblerEmptyNeighborListTest,
	"PCGEx.Unit.Valency.Adversarial.Assembler.EmptyNeighborList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialAssemblerEmptyNeighborListTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	const int32 Idx = Assembler.AddModule(
		TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/A.A"))), 0b01);

	// Empty neighbor list — should be legal
	Assembler.AddNeighbors(Idx, FName("North"), {});

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet({FName("North")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	const FPCGExAssemblerResult Result = Assembler.Apply(Rules);
	TestTrue(TEXT("Empty neighbor list apply succeeds"), Result.bSuccess);

	return true;
}

// =============================================================================
// ModuleSettings — SetBehavior with None flag (no-op)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialModuleSettingsSetNoneTest,
	"PCGEx.Unit.Valency.Adversarial.ModuleSettings.SetBehaviorNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialModuleSettingsSetNoneTest::RunTest(const FString& Parameters)
{
	FPCGExValencyModuleSettings Settings;
	Settings.SetBehavior(EPCGExModuleBehavior::PreferredStart);

	// Setting "None" (0) should not clear existing flags (OR with 0 = no-op)
	Settings.SetBehavior(EPCGExModuleBehavior::None);
	TestTrue(TEXT("Setting None does not clear PreferredStart"),
		Settings.HasBehavior(EPCGExModuleBehavior::PreferredStart));

	return true;
}

// =============================================================================
// ModuleSettings — ClearBehavior with None flag (no-op)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialModuleSettingsClearNoneTest,
	"PCGEx.Unit.Valency.Adversarial.ModuleSettings.ClearBehaviorNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialModuleSettingsClearNoneTest::RunTest(const FString& Parameters)
{
	FPCGExValencyModuleSettings Settings;
	Settings.SetBehavior(EPCGExModuleBehavior::PreferredStart);
	Settings.SetBehavior(EPCGExModuleBehavior::Greedy);

	// Clearing "None" (0) should not affect any flags (~0 = all 1s, AND with all 1s = no-op)
	Settings.ClearBehavior(EPCGExModuleBehavior::None);
	TestTrue(TEXT("ClearBehavior(None) preserves PreferredStart"),
		Settings.HasBehavior(EPCGExModuleBehavior::PreferredStart));
	TestTrue(TEXT("ClearBehavior(None) preserves Greedy"),
		Settings.HasBehavior(EPCGExModuleBehavior::Greedy));

	return true;
}

// =============================================================================
// PatternMatch — GetMatchedNodeCount with large arrays
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialMatchLargeEntryToNodeTest,
	"PCGEx.Unit.Valency.Adversarial.PatternMatch.LargeEntryToNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialMatchLargeEntryToNodeTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternMatch Match;
	Match.PatternIndex = 0;

	// 1000 entries — simulates a huge pattern
	Match.EntryToNode.SetNum(1000);
	for (int32 i = 0; i < 1000; ++i)
	{
		Match.EntryToNode[i] = i;
	}

	TestEqual(TEXT("Matched node count = 1000"), Match.GetMatchedNodeCount(), 1000);
	TestEqual(TEXT("Root is node 0"), Match.GetRootNodeIndex(), 0);
	TestTrue(TEXT("Match is valid"), Match.IsValid());

	return true;
}

// =============================================================================
// PatternMatch — Negative PatternIndex
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialMatchNegativePatternIndexTest,
	"PCGEx.Unit.Valency.Adversarial.PatternMatch.NegativePatternIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialMatchNegativePatternIndexTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternMatch Match;
	Match.PatternIndex = -5;
	Match.EntryToNode = {0, 1, 2};

	TestFalse(TEXT("Negative pattern index = not valid"), Match.IsValid());

	return true;
}

// =============================================================================
// OrbitalSet — IsValidIndex boundaries
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialOrbitalSetIsValidIndexTest,
	"PCGEx.Unit.Valency.Adversarial.OrbitalSet.IsValidIndexBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialOrbitalSetIsValidIndexTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});

	TestTrue(TEXT("Index 0 valid"), OrbitalSet->IsValidIndex(0));
	TestTrue(TEXT("Index 1 valid"), OrbitalSet->IsValidIndex(1));
	TestFalse(TEXT("Index 2 invalid"), OrbitalSet->IsValidIndex(2));
	TestFalse(TEXT("Index -1 invalid"), OrbitalSet->IsValidIndex(-1));
	TestFalse(TEXT("Index INT_MAX invalid"), OrbitalSet->IsValidIndex(INT32_MAX));

	return true;
}
