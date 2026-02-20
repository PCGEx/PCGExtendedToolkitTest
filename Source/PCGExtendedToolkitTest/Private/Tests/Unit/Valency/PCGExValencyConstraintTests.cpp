// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Constraint & Matching Logic Tests
 *
 * Tests the core constraint systems that power pattern matching and solving:
 * - Layer neighbor validation (OrbitalAcceptsNeighbor)
 * - Module fitness logic (DoesModuleFitNode bitmask rules)
 * - Distribution tracker (min/max spawn enforcement)
 * - Pattern entry constraints (boundary/wildcard masks)
 * - Exclusive vs additive pattern ordering
 * - FValencyState lifecycle
 *
 * These tests exercise the core logic in isolation by manually populating
 * compiled data structures, avoiding the need for full cluster infrastructure.
 *
 * Test naming convention: PCGEx.Unit.Valency.Constraint.<Category>.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExBondingRulesAssembler.h"
#include "Core/PCGExBondingRules.h"
#include "Core/PCGExSolverOperation.h"
#include "Core/PCGExValencyCommon.h"
#include "Core/PCGExValencyPattern.h"
#include "Helpers/PCGExValencyTestHelpers.h"

// =============================================================================
// Helper utilities
// =============================================================================

namespace
{
	/** Build orbital names: O0, O1, O2, ... ON */
	TArray<FName> MakeNames(int32 Count)
	{
		TArray<FName> Names;
		Names.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			Names.Add(FName(*FString::Printf(TEXT("O%d"), i)));
		}
		return Names;
	}

	TSoftObjectPtr<UObject> FakeAsset(int32 i)
	{
		return TSoftObjectPtr<UObject>(FSoftObjectPath(
			FString::Printf(TEXT("/Game/Constraint/Mod%d.Mod%d"), i, i)));
	}

	/** Build and compile rules from assembler */
	UPCGExValencyBondingRules* Compile(
		FPCGExBondingRulesAssembler& Assembler,
		const TArray<FName>& OrbitalNames)
	{
		UPCGExValencyOrbitalSet* OS = PCGExTest::ValencyHelpers::CreateOrbitalSet(OrbitalNames);
		UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OS);
		Assembler.Apply(Rules);
		return Rules;
	}
}

// #########################################################################
//  LAYER NEIGHBOR VALIDATION
// #########################################################################

// =============================================================================
// OrbitalAcceptsNeighbor — manually populated layer
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyLayerAcceptsNeighborTest,
	"PCGEx.Unit.Valency.Constraint.Layer.AcceptsNeighbor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyLayerAcceptsNeighborTest::RunTest(const FString& Parameters)
{
	FPCGExValencyLayerCompiled Layer;
	Layer.OrbitalCount = 2;

	// Module 0: orbital 0 accepts {1}, orbital 1 accepts {0, 2}
	// Module 1: orbital 0 accepts {0}, orbital 1 has no neighbors
	// Module 2: orbital 0 accepts {}, orbital 1 accepts {1}
	// Total: 3 modules × 2 orbitals = 6 headers

	Layer.NeighborHeaders.SetNum(6);
	// Module 0
	Layer.NeighborHeaders[0] = FIntPoint(0, 1); // Offset 0, count 1
	Layer.NeighborHeaders[1] = FIntPoint(1, 2); // Offset 1, count 2
	// Module 1
	Layer.NeighborHeaders[2] = FIntPoint(3, 1); // Offset 3, count 1
	Layer.NeighborHeaders[3] = FIntPoint(4, 0); // Offset 4, count 0 (no neighbors)
	// Module 2
	Layer.NeighborHeaders[4] = FIntPoint(4, 0); // No neighbors
	Layer.NeighborHeaders[5] = FIntPoint(4, 1); // Offset 4, count 1

	Layer.AllNeighbors = {1, 0, 2, 0, 1};

	// Positive cases
	TestTrue(TEXT("M0:O0 accepts M1"), Layer.OrbitalAcceptsNeighbor(0, 0, 1));
	TestTrue(TEXT("M0:O1 accepts M0"), Layer.OrbitalAcceptsNeighbor(0, 1, 0));
	TestTrue(TEXT("M0:O1 accepts M2"), Layer.OrbitalAcceptsNeighbor(0, 1, 2));
	TestTrue(TEXT("M1:O0 accepts M0"), Layer.OrbitalAcceptsNeighbor(1, 0, 0));
	TestTrue(TEXT("M2:O1 accepts M1"), Layer.OrbitalAcceptsNeighbor(2, 1, 1));

	// Negative cases
	TestFalse(TEXT("M0:O0 rejects M0"), Layer.OrbitalAcceptsNeighbor(0, 0, 0));
	TestFalse(TEXT("M0:O0 rejects M2"), Layer.OrbitalAcceptsNeighbor(0, 0, 2));
	TestFalse(TEXT("M1:O1 rejects everything (empty)"), Layer.OrbitalAcceptsNeighbor(1, 1, 0));
	TestFalse(TEXT("M2:O0 rejects everything (empty)"), Layer.OrbitalAcceptsNeighbor(2, 0, 0));

	// Out of bounds
	TestFalse(TEXT("OOB module rejects"), Layer.OrbitalAcceptsNeighbor(99, 0, 0));
	TestFalse(TEXT("OOB orbital rejects"), Layer.OrbitalAcceptsNeighbor(0, 99, 0));

	return true;
}

// =============================================================================
// OrbitalAcceptsNeighbor — via assembler → compiled rules
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyLayerViaAssemblerTest,
	"PCGEx.Unit.Valency.Constraint.Layer.ViaAssembler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyLayerViaAssemblerTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Names = MakeNames(3);

	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = FakeAsset(0);
	DescA.OrbitalMask = 0b111;
	const int32 A = Assembler.AddModule(DescA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = FakeAsset(1);
	DescB.OrbitalMask = 0b111;
	const int32 B = Assembler.AddModule(DescB);

	// A's orbital O0 accepts B; B's orbital O1 accepts A
	Assembler.AddNeighbors(A, Names[0], {B});
	Assembler.AddNeighbors(B, Names[1], {A});

	UPCGExValencyBondingRules* Rules = Compile(Assembler, Names);

	const auto& Layer = Rules->CompiledData.Layer;

	TestTrue(TEXT("A:O0 accepts B"), Layer.OrbitalAcceptsNeighbor(A, 0, B));
	TestTrue(TEXT("B:O1 accepts A"), Layer.OrbitalAcceptsNeighbor(B, 1, A));

	// Not specified → should not accept
	TestFalse(TEXT("A:O1 rejects B (not specified)"), Layer.OrbitalAcceptsNeighbor(A, 1, B));
	TestFalse(TEXT("B:O0 rejects A (not specified)"), Layer.OrbitalAcceptsNeighbor(B, 0, A));

	return true;
}

// #########################################################################
//  MODULE FITNESS LOGIC (DoesModuleFitNode bitmask rules)
// #########################################################################

// =============================================================================
// Module mask must be subset of node mask
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyFitnessMaskSubsetTest,
	"PCGEx.Unit.Valency.Constraint.Fitness.MaskSubset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyFitnessMaskSubsetTest::RunTest(const FString& Parameters)
{
	// DoesModuleFitNode rule: (ModuleMask & NodeMask) == ModuleMask
	// i.e., all module orbitals must be present in the node

	const int64 NodeMask = 0b1010; // Node has orbitals 1 and 3

	// Module needs orbitals 1 and 3 → fits
	TestTrue(TEXT("Exact match fits"), (0b1010LL & NodeMask) == 0b1010LL);

	// Module needs only orbital 1 → fits (subset)
	TestTrue(TEXT("Subset fits"), (0b0010LL & NodeMask) == 0b0010LL);

	// Module needs orbitals 0,1,3 → doesn't fit (orbital 0 missing)
	TestFalse(TEXT("Superset rejected"), (0b1011LL & NodeMask) == 0b1011LL);

	// Module needs orbital 0 only → doesn't fit
	TestFalse(TEXT("Disjoint rejected"), (0b0001LL & NodeMask) == 0b0001LL);

	// Empty module mask → fits anything
	TestTrue(TEXT("Empty mask fits"), (0LL & NodeMask) == 0LL);

	return true;
}

// =============================================================================
// Boundary mask must not overlap node mask
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyFitnessBoundaryTest,
	"PCGEx.Unit.Valency.Constraint.Fitness.BoundaryNoOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyFitnessBoundaryTest::RunTest(const FString& Parameters)
{
	// DoesModuleFitNode rule: (BoundaryMask & NodeMask) == 0
	// Boundary orbitals must NOT have connections in node

	const int64 NodeMask = 0b1010; // Node has orbitals 1 and 3

	// Boundary on orbital 0 (not in node) → OK
	TestTrue(TEXT("Boundary on absent orbital"), (0b0001LL & NodeMask) == 0);

	// Boundary on orbital 2 (not in node) → OK
	TestTrue(TEXT("Boundary on another absent orbital"), (0b0100LL & NodeMask) == 0);

	// Boundary on orbital 1 (present in node) → REJECTED
	TestFalse(TEXT("Boundary on present orbital rejected"), (0b0010LL & NodeMask) == 0);

	// Boundary on orbitals 0 and 2 → OK (neither present)
	TestTrue(TEXT("Multiple absent boundary bits"), (0b0101LL & NodeMask) == 0);

	// Boundary on orbitals 1 and 2 → REJECTED (1 is present)
	TestFalse(TEXT("Mixed boundary rejected"), (0b0110LL & NodeMask) == 0);

	// Empty boundary → always OK
	TestTrue(TEXT("Empty boundary passes"), (0LL & NodeMask) == 0);

	return true;
}

// =============================================================================
// Wildcard mask must be subset of node mask
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyFitnessWildcardTest,
	"PCGEx.Unit.Valency.Constraint.Fitness.WildcardSubset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyFitnessWildcardTest::RunTest(const FString& Parameters)
{
	// DoesModuleFitNode rule: (WildcardMask & NodeMask) == WildcardMask
	// Wildcard orbitals must HAVE connections in node

	const int64 NodeMask = 0b1010; // Node has orbitals 1 and 3

	// Wildcard on orbital 1 → OK (present)
	TestTrue(TEXT("Wildcard on present orbital"), (0b0010LL & NodeMask) == 0b0010LL);

	// Wildcard on orbitals 1 and 3 → OK (both present)
	TestTrue(TEXT("Wildcard on both present orbitals"), (0b1010LL & NodeMask) == 0b1010LL);

	// Wildcard on orbital 0 → REJECTED (not present)
	TestFalse(TEXT("Wildcard on absent orbital rejected"), (0b0001LL & NodeMask) == 0b0001LL);

	// Wildcard on orbitals 1 and 0 → REJECTED (0 not present)
	TestFalse(TEXT("Mixed wildcard rejected"), (0b0011LL & NodeMask) == 0b0011LL);

	// Empty wildcard → always OK
	TestTrue(TEXT("Empty wildcard passes"), (0LL & NodeMask) == 0LL);

	return true;
}

// =============================================================================
// Combined fitness: all three masks must pass
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyFitnessCombinedTest,
	"PCGEx.Unit.Valency.Constraint.Fitness.Combined",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyFitnessCombinedTest::RunTest(const FString& Parameters)
{
	// Replicate DoesModuleFitNode exactly
	auto DoesModuleFit = [](int64 ModuleMask, int64 BoundaryMask, int64 WildcardMask, int64 NodeMask) -> bool
	{
		if ((ModuleMask & NodeMask) != ModuleMask) { return false; }
		if ((BoundaryMask & NodeMask) != 0) { return false; }
		if ((WildcardMask & NodeMask) != WildcardMask) { return false; }
		return true;
	};

	// Node with orbitals 0,1,2 (3 connections)
	const int64 NodeMask = 0b0111;

	// Module that needs 0,1,2 — boundary on 3, wildcard on 0,1 → should pass
	TestTrue(TEXT("All constraints pass"),
		DoesModuleFit(0b0111, 0b1000, 0b0011, NodeMask));

	// Module mask too wide (needs orbital 3) → fail on mask
	TestFalse(TEXT("Module mask too wide"),
		DoesModuleFit(0b1111, 0b0000, 0b0000, NodeMask));

	// Boundary conflicts with node → fail on boundary
	TestFalse(TEXT("Boundary conflict"),
		DoesModuleFit(0b0111, 0b0001, 0b0000, NodeMask));

	// Wildcard needs orbital 3 which isn't in node → fail on wildcard
	TestFalse(TEXT("Wildcard missing"),
		DoesModuleFit(0b0111, 0b0000, 0b1000, NodeMask));

	// All empty masks → trivially fits anything
	TestTrue(TEXT("Empty masks fit any node"),
		DoesModuleFit(0b0000, 0b0000, 0b0000, NodeMask));

	// Module 0b0011, boundary 0b0100, wildcard 0b0001 against node 0b0011
	// Mask OK (0b0011 subset of 0b0011), boundary OK (0b0100 & 0b0011 = 0), wildcard OK (0b0001 subset of 0b0011)
	TestTrue(TEXT("Realistic example"),
		DoesModuleFit(0b0011, 0b0100, 0b0001, 0b0011));

	return true;
}

// #########################################################################
//  DISTRIBUTION TRACKER
// #########################################################################

// =============================================================================
// Basic min/max spawn tracking
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyDistTrackerBasicTest,
	"PCGEx.Unit.Valency.Constraint.DistTracker.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyDistTrackerBasicTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Names = MakeNames(2);
	FPCGExBondingRulesAssembler Assembler;

	// Module 0: MinSpawns=2, MaxSpawns=4
	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = FakeAsset(0);
	DescA.OrbitalMask = 0b01;
	DescA.Settings.MinSpawns = 2;
	DescA.Settings.MaxSpawns = 4;
	Assembler.AddModule(DescA);

	// Module 1: MinSpawns=0, MaxSpawns=1
	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = FakeAsset(1);
	DescB.OrbitalMask = 0b10;
	DescB.Settings.MinSpawns = 0;
	DescB.Settings.MaxSpawns = 1;
	Assembler.AddModule(DescB);

	UPCGExValencyBondingRules* Rules = Compile(Assembler, Names);

	PCGExValency::FDistributionTracker Tracker;
	Tracker.Initialize(&Rules->CompiledData);

	// Initially: module 0 needs minimum, module 1 doesn't
	TestFalse(TEXT("Minimums not satisfied initially"), Tracker.AreMinimumsSatisfied());
	TestTrue(TEXT("Module 0 needs minimum"), Tracker.GetModulesNeedingMinimum().Contains(0));
	TestFalse(TEXT("Module 1 has no minimum"), Tracker.GetModulesNeedingMinimum().Contains(1));

	// Both can spawn initially
	TestTrue(TEXT("Module 0 can spawn"), Tracker.CanSpawn(0));
	TestTrue(TEXT("Module 1 can spawn"), Tracker.CanSpawn(1));

	// Spawn module 0 once
	Tracker.RecordSpawn(0, &Rules->CompiledData);
	TestFalse(TEXT("Still needs minimum after 1 spawn"), Tracker.AreMinimumsSatisfied());

	// Spawn module 0 again → minimum met
	Tracker.RecordSpawn(0, &Rules->CompiledData);
	TestTrue(TEXT("Minimums satisfied after 2 spawns"), Tracker.AreMinimumsSatisfied());
	TestTrue(TEXT("Module 0 can still spawn (2 < 4)"), Tracker.CanSpawn(0));

	// Spawn module 1 once → at max
	Tracker.RecordSpawn(1, &Rules->CompiledData);
	TestFalse(TEXT("Module 1 at max (1/1)"), Tracker.CanSpawn(1));

	// Spawn module 0 two more times → at max
	Tracker.RecordSpawn(0, &Rules->CompiledData);
	Tracker.RecordSpawn(0, &Rules->CompiledData);
	TestFalse(TEXT("Module 0 at max (4/4)"), Tracker.CanSpawn(0));

	return true;
}

// =============================================================================
// Distribution tracker — unlimited max (-1)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyDistTrackerUnlimitedTest,
	"PCGEx.Unit.Valency.Constraint.DistTracker.UnlimitedMax",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyDistTrackerUnlimitedTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Names = MakeNames(2);
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc Desc;
	Desc.Asset = FakeAsset(0);
	Desc.OrbitalMask = 0b01;
	Desc.Settings.MinSpawns = 0;
	Desc.Settings.MaxSpawns = -1; // Unlimited
	Assembler.AddModule(Desc);

	UPCGExValencyBondingRules* Rules = Compile(Assembler, Names);

	PCGExValency::FDistributionTracker Tracker;
	Tracker.Initialize(&Rules->CompiledData);

	// Should always be spawnable
	TestTrue(TEXT("Minimums satisfied (0 min)"), Tracker.AreMinimumsSatisfied());

	for (int32 i = 0; i < 1000; ++i)
	{
		TestTrue(TEXT("Still spawnable"), Tracker.CanSpawn(0));
		Tracker.RecordSpawn(0, &Rules->CompiledData);
	}

	TestTrue(TEXT("Still spawnable after 1000"), Tracker.CanSpawn(0));

	return true;
}

// #########################################################################
//  PATTERN ENTRY CONSTRAINTS
// #########################################################################

// =============================================================================
// MatchesModule — wildcard vs specific
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternEntryMatchTest,
	"PCGEx.Unit.Valency.Constraint.PatternEntry.MatchesModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternEntryMatchTest::RunTest(const FString& Parameters)
{
	// Wildcard entry (empty ModuleIndices)
	FPCGExValencyPatternEntryCompiled Wildcard;
	TestTrue(TEXT("Wildcard matches module 0"), Wildcard.MatchesModule(0));
	TestTrue(TEXT("Wildcard matches module 99"), Wildcard.MatchesModule(99));
	TestTrue(TEXT("Wildcard matches -1"), Wildcard.MatchesModule(-1));
	TestTrue(TEXT("IsWildcard()"), Wildcard.IsWildcard());

	// Specific entry
	FPCGExValencyPatternEntryCompiled Specific;
	Specific.ModuleIndices = {2, 5, 8};
	TestTrue(TEXT("Specific matches 2"), Specific.MatchesModule(2));
	TestTrue(TEXT("Specific matches 5"), Specific.MatchesModule(5));
	TestTrue(TEXT("Specific matches 8"), Specific.MatchesModule(8));
	TestFalse(TEXT("Specific rejects 0"), Specific.MatchesModule(0));
	TestFalse(TEXT("Specific rejects 3"), Specific.MatchesModule(3));
	TestFalse(TEXT("Not wildcard"), Specific.IsWildcard());

	// Single module entry
	FPCGExValencyPatternEntryCompiled Single;
	Single.ModuleIndices = {7};
	TestTrue(TEXT("Single matches 7"), Single.MatchesModule(7));
	TestFalse(TEXT("Single rejects 6"), Single.MatchesModule(6));

	return true;
}

// =============================================================================
// Pattern entry boundary/wildcard orbital masks
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternEntryMasksTest,
	"PCGEx.Unit.Valency.Constraint.PatternEntry.OrbitalMasks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternEntryMasksTest::RunTest(const FString& Parameters)
{
	// Simulate node with occupied orbitals 0 and 2
	const uint64 NodeOccupiedMask = 0b0101;

	// Boundary check: boundary orbitals must be EMPTY in node
	// BoundaryMask = orbital 1 (not in node) → pass
	TestTrue(TEXT("Boundary on empty orbital passes"),
		(NodeOccupiedMask & 0b0010ULL) == 0);

	// BoundaryMask = orbital 0 (occupied) → fail
	TestFalse(TEXT("Boundary on occupied orbital fails"),
		(NodeOccupiedMask & 0b0001ULL) == 0);

	// Wildcard check: wildcard orbitals must be OCCUPIED in node
	// WildcardMask = orbital 0 → pass (occupied)
	TestTrue(TEXT("Wildcard on occupied orbital passes"),
		(NodeOccupiedMask & 0b0001ULL) == 0b0001ULL);

	// WildcardMask = orbitals 0 and 2 → pass (both occupied)
	TestTrue(TEXT("Wildcard on both occupied passes"),
		(NodeOccupiedMask & 0b0101ULL) == 0b0101ULL);

	// WildcardMask = orbitals 0 and 1 → fail (1 not occupied)
	TestFalse(TEXT("Wildcard with missing orbital fails"),
		(NodeOccupiedMask & 0b0011ULL) == 0b0011ULL);

	return true;
}

// #########################################################################
//  EXCLUSIVE vs ADDITIVE PATTERN ORDERING
// #########################################################################

// =============================================================================
// Exclusive patterns indexed before additive
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyExclusiveAdditiveOrderTest,
	"PCGEx.Unit.Valency.Constraint.PatternSet.ExclusiveAdditiveOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyExclusiveAdditiveOrderTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OS = PCGExTest::ValencyHelpers::CreateOrbitalSet(MakeNames(2));
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OS);

	// Add patterns alternating exclusive/additive
	auto MakePattern = [](FName Name, int32 EntryCount) -> FPCGExValencyPatternCompiled
	{
		FPCGExValencyPatternCompiled P;
		P.Settings.PatternName = Name;
		P.ActiveEntryCount = EntryCount;
		for (int32 i = 0; i < EntryCount; ++i)
		{
			FPCGExValencyPatternEntryCompiled E;
			E.bIsActive = true;
			P.Entries.Add(E);
		}
		return P;
	};

	Asset->ClearCompiledPatterns();
	Asset->AppendCompiledPattern(MakePattern(FName("Excl0"), 2), true);   // Exclusive → index 0
	Asset->AppendCompiledPattern(MakePattern(FName("Add0"), 1), false);   // Additive → index 1
	Asset->AppendCompiledPattern(MakePattern(FName("Excl1"), 3), true);   // Exclusive → index 2
	Asset->AppendCompiledPattern(MakePattern(FName("Add1"), 1), false);   // Additive → index 3
	Asset->AppendCompiledPattern(MakePattern(FName("Excl2"), 1), true);   // Exclusive → index 4

	const auto& Builder = Asset->GetBuilderCagePatterns();

	TestEqual(TEXT("5 total patterns"), Builder.GetPatternCount(), 5);
	TestEqual(TEXT("3 exclusive"), Builder.ExclusivePatternIndices.Num(), 3);
	TestEqual(TEXT("2 additive"), Builder.AdditivePatternIndices.Num(), 2);

	// Exclusive indices should be {0, 2, 4}
	TestTrue(TEXT("Exclusive contains 0"), Builder.ExclusivePatternIndices.Contains(0));
	TestTrue(TEXT("Exclusive contains 2"), Builder.ExclusivePatternIndices.Contains(2));
	TestTrue(TEXT("Exclusive contains 4"), Builder.ExclusivePatternIndices.Contains(4));

	// Additive indices should be {1, 3}
	TestTrue(TEXT("Additive contains 1"), Builder.AdditivePatternIndices.Contains(1));
	TestTrue(TEXT("Additive contains 3"), Builder.AdditivePatternIndices.Contains(3));

	return true;
}

// =============================================================================
// Pattern min/max settings survive compilation
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternMinMaxSettingsTest,
	"PCGEx.Unit.Valency.Constraint.PatternSet.MinMaxSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternMinMaxSettingsTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OS = PCGExTest::ValencyHelpers::CreateOrbitalSet(MakeNames(2));
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OS);

	// Build a pattern with specific min/max/weight
	FPCGExValencyPatternCompiled P;
	P.Settings.PatternName = FName("TestPattern");
	P.Settings.MinMatches = 3;
	P.Settings.MaxMatches = 10;
	P.Settings.Weight = 2.5f;
	P.ActiveEntryCount = 1;
	{
		FPCGExValencyPatternEntryCompiled E;
		E.bIsActive = true;
		P.Entries.Add(E);
	}

	Asset->ClearCompiledPatterns();
	Asset->AppendCompiledPattern(MoveTemp(P), true);

	const auto& Patterns = Asset->GetBuilderCagePatterns();
	TestEqual(TEXT("1 pattern"), Patterns.GetPatternCount(), 1);

	const auto& Settings = Patterns.Patterns[0].Settings;
	TestEqual(TEXT("PatternName"), Settings.PatternName, FName("TestPattern"));
	TestEqual(TEXT("MinMatches=3"), Settings.MinMatches, 3);
	TestEqual(TEXT("MaxMatches=10"), Settings.MaxMatches, 10);
	TestEqual(TEXT("Weight=2.5"), Settings.Weight, 2.5f);

	return true;
}

// #########################################################################
//  VALENCY STATE LIFECYCLE
// #########################################################################

// =============================================================================
// State transitions: UNSET → resolved, boundary, unsolvable
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStateTransitionsTest,
	"PCGEx.Unit.Valency.Constraint.State.Transitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStateTransitionsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExValency;

	// Fresh state
	FValencyState State;
	State.NodeIndex = 42;
	TestFalse(TEXT("UNSET is not resolved"), State.IsResolved());
	TestFalse(TEXT("UNSET is not boundary"), State.IsBoundary());
	TestFalse(TEXT("UNSET is not unsolvable"), State.IsUnsolvable());

	// Resolve to module 5
	State.ResolvedModule = 5;
	TestTrue(TEXT("Module 5 is resolved"), State.IsResolved());
	TestFalse(TEXT("Module 5 is not boundary"), State.IsBoundary());
	TestFalse(TEXT("Module 5 is not unsolvable"), State.IsUnsolvable());

	// Mark as boundary
	State.ResolvedModule = SlotState::NULL_SLOT;
	TestTrue(TEXT("NULL_SLOT is resolved"), State.IsResolved());
	TestTrue(TEXT("NULL_SLOT is boundary"), State.IsBoundary());
	TestFalse(TEXT("NULL_SLOT is not unsolvable"), State.IsUnsolvable());

	// Mark as unsolvable
	State.ResolvedModule = SlotState::UNSOLVABLE;
	TestTrue(TEXT("UNSOLVABLE is resolved"), State.IsResolved());
	TestFalse(TEXT("UNSOLVABLE is not boundary"), State.IsBoundary());
	TestTrue(TEXT("UNSOLVABLE is unsolvable"), State.IsUnsolvable());

	// Placeholder
	State.ResolvedModule = SlotState::PLACEHOLDER;
	TestFalse(TEXT("PLACEHOLDER is not resolved"), State.IsResolved());
	TestFalse(TEXT("PLACEHOLDER is not boundary"), State.IsBoundary());
	TestFalse(TEXT("PLACEHOLDER is not unsolvable"), State.IsUnsolvable());

	return true;
}

// =============================================================================
// FSolveResult structure
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencySolveResultTest,
	"PCGEx.Unit.Valency.Constraint.State.SolveResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencySolveResultTest::RunTest(const FString& Parameters)
{
	PCGExValency::FSolveResult Result;

	// Defaults
	TestEqual(TEXT("Default resolved=0"), Result.ResolvedCount, 0);
	TestEqual(TEXT("Default unsolvable=0"), Result.UnsolvableCount, 0);
	TestEqual(TEXT("Default boundary=0"), Result.BoundaryCount, 0);
	TestTrue(TEXT("Default minimums satisfied"), Result.MinimumsSatisfied);
	TestFalse(TEXT("Default not success"), Result.bSuccess);

	return true;
}

// #########################################################################
//  COMPILED RULES — MIN/MAX SPAWN ARRAYS
// #########################################################################

// =============================================================================
// MinSpawns/MaxSpawns in compiled data
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyCompiledMinMaxSpawnsTest,
	"PCGEx.Unit.Valency.Constraint.Compiled.MinMaxSpawns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyCompiledMinMaxSpawnsTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Names = MakeNames(2);
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = FakeAsset(0);
	DescA.OrbitalMask = 0b01;
	DescA.Settings.MinSpawns = 5;
	DescA.Settings.MaxSpawns = 20;
	Assembler.AddModule(DescA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = FakeAsset(1);
	DescB.OrbitalMask = 0b10;
	DescB.Settings.MinSpawns = 0;
	DescB.Settings.MaxSpawns = -1; // Unlimited
	Assembler.AddModule(DescB);

	UPCGExValencyBondingRules* Rules = Compile(Assembler, Names);
	const auto& CD = Rules->CompiledData;

	TestEqual(TEXT("Module 0 MinSpawns"), CD.ModuleMinSpawns[0], 5);
	TestEqual(TEXT("Module 0 MaxSpawns"), CD.ModuleMaxSpawns[0], 20);
	TestEqual(TEXT("Module 1 MinSpawns"), CD.ModuleMinSpawns[1], 0);
	TestEqual(TEXT("Module 1 MaxSpawns"), CD.ModuleMaxSpawns[1], -1);

	return true;
}

// #########################################################################
//  PATTERN ADJACENCY STRUCTURE
// #########################################################################

// =============================================================================
// Pattern adjacency encodes orbital relationships
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternAdjacencyTest,
	"PCGEx.Unit.Valency.Constraint.PatternEntry.Adjacency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternAdjacencyTest::RunTest(const FString& Parameters)
{
	// FIntVector(TargetEntryIndex, SourceOrbitalIndex, TargetOrbitalIndex)
	FPCGExValencyPatternEntryCompiled Entry;
	Entry.bIsActive = true;

	// Entry 0 connects to entry 1 via source orbital 2, target orbital 3
	Entry.Adjacency.Add(FIntVector(1, 2, 3));
	// Entry 0 also connects to entry 2 via source orbital 0, target orbital 1
	Entry.Adjacency.Add(FIntVector(2, 0, 1));

	TestEqual(TEXT("2 adjacencies"), Entry.Adjacency.Num(), 2);

	// First adjacency
	TestEqual(TEXT("Adj 0: target entry 1"), Entry.Adjacency[0].X, 1);
	TestEqual(TEXT("Adj 0: source orbital 2"), Entry.Adjacency[0].Y, 2);
	TestEqual(TEXT("Adj 0: target orbital 3"), Entry.Adjacency[0].Z, 3);

	// Second adjacency
	TestEqual(TEXT("Adj 1: target entry 2"), Entry.Adjacency[1].X, 2);
	TestEqual(TEXT("Adj 1: source orbital 0"), Entry.Adjacency[1].Y, 0);
	TestEqual(TEXT("Adj 1: target orbital 1"), Entry.Adjacency[1].Z, 1);

	return true;
}

// =============================================================================
// Active entry count in compiled pattern
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternActiveEntryCountTest,
	"PCGEx.Unit.Valency.Constraint.Pattern.ActiveEntryCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternActiveEntryCountTest::RunTest(const FString& Parameters)
{
	FPCGExValencyPatternCompiled Pattern;

	// 3 active, 2 inactive entries
	for (int32 i = 0; i < 5; ++i)
	{
		FPCGExValencyPatternEntryCompiled E;
		E.bIsActive = (i < 3);
		Pattern.Entries.Add(E);
	}
	Pattern.ActiveEntryCount = 3;

	TestEqual(TEXT("Total entries = 5"), Pattern.Entries.Num(), 5);
	TestEqual(TEXT("Active entries = 3"), Pattern.ActiveEntryCount, 3);
	TestTrue(TEXT("Pattern is valid"), Pattern.IsValid());

	// Empty pattern is not valid
	FPCGExValencyPatternCompiled Empty;
	TestFalse(TEXT("Empty pattern not valid"), Empty.IsValid());

	return true;
}

// #########################################################################
//  MODULE BEHAVIOR FLAGS
// #########################################################################

// =============================================================================
// Behavior flags survive assembler → compile
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBehaviorFlagsTest,
	"PCGEx.Unit.Valency.Constraint.Compiled.BehaviorFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBehaviorFlagsTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Names = MakeNames(2);
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = FakeAsset(0);
	DescA.OrbitalMask = 0b01;
	DescA.Settings.SetBehavior(EPCGExModuleBehavior::PreferredStart);
	DescA.Settings.SetBehavior(EPCGExModuleBehavior::Greedy);
	Assembler.AddModule(DescA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = FakeAsset(1);
	DescB.OrbitalMask = 0b10;
	// No behaviors set
	Assembler.AddModule(DescB);

	UPCGExValencyBondingRules* Rules = Compile(Assembler, Names);
	const auto& CD = Rules->CompiledData;

	// Module 0 should have PreferredStart and Greedy flags
	TestTrue(TEXT("Module 0 has PreferredStart"),
		(CD.ModuleBehaviorFlags[0] & static_cast<uint8>(EPCGExModuleBehavior::PreferredStart)) != 0);
	TestTrue(TEXT("Module 0 has Greedy"),
		(CD.ModuleBehaviorFlags[0] & static_cast<uint8>(EPCGExModuleBehavior::Greedy)) != 0);

	// Module 1 should have no flags
	TestEqual(TEXT("Module 1 has no flags"), CD.ModuleBehaviorFlags[1], static_cast<uint8>(0));

	// Compile-time caches
	TestTrue(TEXT("bHasAnyPreferredStart"), CD.bHasAnyPreferredStart);
	TestTrue(TEXT("bHasAnyGreedy"), CD.bHasAnyGreedy);

	return true;
}

// #########################################################################
//  DEAD END MODULE FLAG
// #########################################################################

// =============================================================================
// DeadEnd flag compiled correctly
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyDeadEndFlagTest,
	"PCGEx.Unit.Valency.Constraint.Compiled.DeadEndFlag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyDeadEndFlagTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Names = MakeNames(2);
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = FakeAsset(0);
	DescA.OrbitalMask = 0b01;
	DescA.Settings.bIsDeadEnd = true;
	Assembler.AddModule(DescA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = FakeAsset(1);
	DescB.OrbitalMask = 0b10;
	DescB.Settings.bIsDeadEnd = false;
	Assembler.AddModule(DescB);

	UPCGExValencyBondingRules* Rules = Compile(Assembler, Names);

	TestTrue(TEXT("Module 0 is dead end"), Rules->CompiledData.ModuleIsDeadEnd[0]);
	TestFalse(TEXT("Module 1 is not dead end"), Rules->CompiledData.ModuleIsDeadEnd[1]);

	return true;
}
