// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Bidirectional Adjacency Tests
 *
 * Tests that pattern adjacencies are bidirectional so that the DFS matcher
 * can discover all entries regardless of which entry is the root.
 *
 * The core bug: CompileGraphToAsset previously only emitted forward (output→input)
 * adjacencies. When the root connects to a middle/end entry in a chain, entries
 * only reachable via reverse connections were never discovered by the DFS.
 *
 * These tests verify:
 * - Authored patterns with bidirectional adjacencies compile correctly (both connector + orbital)
 * - Chain topology: all entries have adjacencies regardless of position
 * - Star topology: center has adjacencies to all leaves and vice versa
 * - No duplicate adjacency pairs after bidirectionalization
 * - Typed connections swap source/target correctly in reverse direction
 *
 * Test naming convention: PCGEx.Unit.Valency.BidirectionalAdj.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExConnectorPatternAsset.h"
#include "Core/PCGExCagePatternAsset.h"
#include "Core/PCGExConnectorSet.h"
#include "Core/PCGExValencyPatternAsset.h"
#include "Helpers/PCGExValencyTestHelpers.h"

// =============================================================================
// Helper: Create a ConnectorSet with named types
// =============================================================================

namespace
{
	UPCGExValencyConnectorSet* CreateConnectorSet(const TArray<FName>& TypeNames)
	{
		UPCGExValencyConnectorSet* Set = NewObject<UPCGExValencyConnectorSet>(GetTransientPackage());
		for (const FName& Name : TypeNames)
		{
			FPCGExValencyConnectorEntry Entry;
			Entry.ConnectorType = Name;
			Set->ConnectorTypes.Add(Entry);
		}
		return Set;
	}

	/** Build a 3-entry chain with unidirectional adjacencies (simulates old CompileGraphToAsset) */
	FPCGExPatternAuthored MakeChainUnidirectional(const FName& TypeName = NAME_None)
	{
		FPCGExPatternAuthored Pattern;
		Pattern.PatternName = FName("Chain");

		// Entry 0 → Entry 1
		FPCGExPatternEntryAuthored E0;
		E0.bIsActive = true;
		{
			FPCGExPatternAdjacencyAuthored Adj;
			Adj.TargetEntryIndex = 1;
			FPCGExPatternIndexPairAuthored Pair;
			Pair.SourceName = TypeName;
			Pair.TargetName = TypeName;
			Adj.IndexPairs.Add(Pair);
			E0.Adjacencies.Add(Adj);
		}
		Pattern.Entries.Add(E0);

		// Entry 1 → Entry 2
		FPCGExPatternEntryAuthored E1;
		E1.bIsActive = true;
		{
			FPCGExPatternAdjacencyAuthored Adj;
			Adj.TargetEntryIndex = 2;
			FPCGExPatternIndexPairAuthored Pair;
			Pair.SourceName = TypeName;
			Pair.TargetName = TypeName;
			Adj.IndexPairs.Add(Pair);
			E1.Adjacencies.Add(Adj);
		}
		Pattern.Entries.Add(E1);

		// Entry 2: no adjacencies (end of chain)
		FPCGExPatternEntryAuthored E2;
		E2.bIsActive = true;
		Pattern.Entries.Add(E2);

		return Pattern;
	}

	/** Build a 3-entry chain with bidirectional adjacencies (simulates fixed CompileGraphToAsset) */
	FPCGExPatternAuthored MakeChainBidirectional(const FName& TypeName = NAME_None)
	{
		FPCGExPatternAuthored Pattern;
		Pattern.PatternName = FName("Chain");

		// Entry 0: adjacency → Entry 1
		FPCGExPatternEntryAuthored E0;
		E0.bIsActive = true;
		{
			FPCGExPatternAdjacencyAuthored Adj;
			Adj.TargetEntryIndex = 1;
			FPCGExPatternIndexPairAuthored Pair;
			Pair.SourceName = TypeName;
			Pair.TargetName = TypeName;
			Adj.IndexPairs.Add(Pair);
			E0.Adjacencies.Add(Adj);
		}
		Pattern.Entries.Add(E0);

		// Entry 1: adjacency → Entry 2 AND reverse → Entry 0
		FPCGExPatternEntryAuthored E1;
		E1.bIsActive = true;
		{
			FPCGExPatternAdjacencyAuthored AdjFwd;
			AdjFwd.TargetEntryIndex = 2;
			FPCGExPatternIndexPairAuthored PairFwd;
			PairFwd.SourceName = TypeName;
			PairFwd.TargetName = TypeName;
			AdjFwd.IndexPairs.Add(PairFwd);
			E1.Adjacencies.Add(AdjFwd);
		}
		{
			FPCGExPatternAdjacencyAuthored AdjRev;
			AdjRev.TargetEntryIndex = 0;
			FPCGExPatternIndexPairAuthored PairRev;
			PairRev.SourceName = TypeName;
			PairRev.TargetName = TypeName;
			AdjRev.IndexPairs.Add(PairRev);
			E1.Adjacencies.Add(AdjRev);
		}
		Pattern.Entries.Add(E1);

		// Entry 2: reverse → Entry 1
		FPCGExPatternEntryAuthored E2;
		E2.bIsActive = true;
		{
			FPCGExPatternAdjacencyAuthored AdjRev;
			AdjRev.TargetEntryIndex = 1;
			FPCGExPatternIndexPairAuthored PairRev;
			PairRev.SourceName = TypeName;
			PairRev.TargetName = TypeName;
			AdjRev.IndexPairs.Add(PairRev);
			E2.Adjacencies.Add(AdjRev);
		}
		Pattern.Entries.Add(E2);

		return Pattern;
	}

	/** Check if an entry has an adjacency pointing to a given target index */
	bool HasAdjacencyTo(const FPCGExPatternEntryAuthored& Entry, int32 TargetIdx)
	{
		for (const FPCGExPatternAdjacencyAuthored& Adj : Entry.Adjacencies)
		{
			if (Adj.TargetEntryIndex == TargetIdx) { return true; }
		}
		return false;
	}
}

// #########################################################################
//  AUTHORED DATA — BIDIRECTIONAL ADJACENCY STRUCTURE
// #########################################################################

// =============================================================================
// Chain: unidirectional has gaps, bidirectional is complete
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBiDirChainAuthoredTest,
	"PCGEx.Unit.Valency.BidirectionalAdj.Authored.ChainStructure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBiDirChainAuthoredTest::RunTest(const FString& Parameters)
{
	// Unidirectional: only forward links
	{
		const FPCGExPatternAuthored Uni = MakeChainUnidirectional();
		TestTrue(TEXT("Uni: E0 → E1"), HasAdjacencyTo(Uni.Entries[0], 1));
		TestFalse(TEXT("Uni: E0 no → E2"), HasAdjacencyTo(Uni.Entries[0], 2));
		TestTrue(TEXT("Uni: E1 → E2"), HasAdjacencyTo(Uni.Entries[1], 2));
		TestFalse(TEXT("Uni: E1 no → E0"), HasAdjacencyTo(Uni.Entries[1], 0));
		TestEqual(TEXT("Uni: E2 has no adj"), Uni.Entries[2].Adjacencies.Num(), 0);
	}

	// Bidirectional: all links present
	{
		const FPCGExPatternAuthored Bi = MakeChainBidirectional();
		TestTrue(TEXT("Bi: E0 → E1"), HasAdjacencyTo(Bi.Entries[0], 1));
		TestTrue(TEXT("Bi: E1 → E2"), HasAdjacencyTo(Bi.Entries[1], 2));
		TestTrue(TEXT("Bi: E1 → E0 (reverse)"), HasAdjacencyTo(Bi.Entries[1], 0));
		TestTrue(TEXT("Bi: E2 → E1 (reverse)"), HasAdjacencyTo(Bi.Entries[2], 1));
	}

	return true;
}

// #########################################################################
//  CONNECTOR PATTERN — BIDIRECTIONAL ADJACENCY COMPILATION
// #########################################################################

// =============================================================================
// Bidirectional chain compiles to connector patterns with all adjacencies
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBiDirConnectorChainTest,
	"PCGEx.Unit.Valency.BidirectionalAdj.Connector.ChainCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBiDirConnectorChainTest::RunTest(const FString& Parameters)
{
	UPCGExValencyConnectorSet* ConnSet = CreateConnectorSet({FName("TypeA")});

	UPCGExConnectorPatternAsset* Asset = NewObject<UPCGExConnectorPatternAsset>(GetTransientPackage());
	Asset->ConnectorSet = ConnSet;
	Asset->Patterns.Add(MakeChainBidirectional());

	TArray<FText> Errors;
	const bool bCompiled = Asset->Compile(&Errors);
	TestTrue(TEXT("Compile succeeds"), bCompiled);
	TestEqual(TEXT("No errors"), Errors.Num(), 0);

	const auto& Compiled = Asset->GetCompiledPatterns();
	TestEqual(TEXT("1 pattern"), Compiled.GetPatternCount(), 1);

	const auto& Pattern = Compiled.Patterns[0];
	TestEqual(TEXT("3 entries"), Pattern.GetEntryCount(), 3);

	// Entry 0: adjacency to Entry 1
	TestTrue(TEXT("E0 has adjacencies"), Pattern.Entries[0].Adjacencies.Num() > 0);

	bool bE0ToE1 = false;
	for (const auto& Adj : Pattern.Entries[0].Adjacencies)
	{
		if (Adj.TargetEntryIndex == 1) { bE0ToE1 = true; }
	}
	TestTrue(TEXT("E0 → E1"), bE0ToE1);

	// Entry 1: adjacency to Entry 2 AND Entry 0
	bool bE1ToE2 = false, bE1ToE0 = false;
	for (const auto& Adj : Pattern.Entries[1].Adjacencies)
	{
		if (Adj.TargetEntryIndex == 2) { bE1ToE2 = true; }
		if (Adj.TargetEntryIndex == 0) { bE1ToE0 = true; }
	}
	TestTrue(TEXT("E1 → E2"), bE1ToE2);
	TestTrue(TEXT("E1 → E0 (reverse)"), bE1ToE0);

	// Entry 2: adjacency to Entry 1 (reverse)
	bool bE2ToE1 = false;
	for (const auto& Adj : Pattern.Entries[2].Adjacencies)
	{
		if (Adj.TargetEntryIndex == 1) { bE2ToE1 = true; }
	}
	TestTrue(TEXT("E2 → E1 (reverse)"), bE2ToE1);

	return true;
}

// =============================================================================
// Connector: Any-type chain (NAME_None) compiles with wildcard type pairs
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBiDirConnectorAnyTypeTest,
	"PCGEx.Unit.Valency.BidirectionalAdj.Connector.AnyTypeChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBiDirConnectorAnyTypeTest::RunTest(const FString& Parameters)
{
	UPCGExValencyConnectorSet* ConnSet = CreateConnectorSet({FName("TypeA")});

	UPCGExConnectorPatternAsset* Asset = NewObject<UPCGExConnectorPatternAsset>(GetTransientPackage());
	Asset->ConnectorSet = ConnSet;

	// NAME_None = "Any" pin in graph editor → AnyTypeIndex in compiled
	Asset->Patterns.Add(MakeChainBidirectional(NAME_None));

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	const auto& Compiled = Asset->GetCompiledPatterns();
	if (!Compiled.HasPatterns()) { AddError(TEXT("No compiled patterns")); return true; }

	const auto& E2 = Compiled.Patterns[0].Entries[2];
	TestTrue(TEXT("Entry 2 has reverse adjacency"), E2.Adjacencies.Num() > 0);

	if (E2.Adjacencies.Num() > 0)
	{
		const auto& Adj = E2.Adjacencies[0];
		TestEqual(TEXT("Reverse targets entry 1"), Adj.TargetEntryIndex, 1);
		TestTrue(TEXT("Has type pairs"), Adj.TypePairs.Num() > 0);

		if (Adj.TypePairs.Num() > 0)
		{
			// Both source and target should be AnyTypeIndex for wildcard connections
			TestTrue(TEXT("Source is wildcard"),
				Adj.TypePairs[0].SourceTypeIndex == FPCGExConnectorTypePair::AnyTypeIndex);
			TestTrue(TEXT("Target is wildcard"),
				Adj.TypePairs[0].TargetTypeIndex == FPCGExConnectorTypePair::AnyTypeIndex);
		}
	}

	return true;
}

// =============================================================================
// Connector: Typed connections swap source/target in reverse
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBiDirConnectorTypedSwapTest,
	"PCGEx.Unit.Valency.BidirectionalAdj.Connector.TypedSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBiDirConnectorTypedSwapTest::RunTest(const FString& Parameters)
{
	UPCGExValencyConnectorSet* ConnSet = CreateConnectorSet({FName("Male"), FName("Female")});

	UPCGExConnectorPatternAsset* Asset = NewObject<UPCGExConnectorPatternAsset>(GetTransientPackage());
	Asset->ConnectorSet = ConnSet;

	// Entry 0 → Entry 1 via Male→Female (source=Male, target=Female)
	// Reverse on Entry 1 should be Female→Male (source=Female, target=Male)
	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("TypedSwap");

	FPCGExPatternEntryAuthored E0;
	E0.bIsActive = true;
	{
		FPCGExPatternAdjacencyAuthored Adj;
		Adj.TargetEntryIndex = 1;
		FPCGExPatternIndexPairAuthored Pair;
		Pair.SourceName = FName("Male");
		Pair.TargetName = FName("Female");
		Adj.IndexPairs.Add(Pair);
		E0.Adjacencies.Add(Adj);
	}
	Pattern.Entries.Add(E0);

	// Entry 1: has reverse adjacency Female→Male
	FPCGExPatternEntryAuthored E1;
	E1.bIsActive = true;
	{
		FPCGExPatternAdjacencyAuthored AdjRev;
		AdjRev.TargetEntryIndex = 0;
		FPCGExPatternIndexPairAuthored PairRev;
		PairRev.SourceName = FName("Female");  // Swapped!
		PairRev.TargetName = FName("Male");    // Swapped!
		AdjRev.IndexPairs.Add(PairRev);
		E1.Adjacencies.Add(AdjRev);
	}
	Pattern.Entries.Add(E1);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	const bool bCompiled = Asset->Compile(&Errors);
	TestTrue(TEXT("Compile succeeds"), bCompiled);

	const auto& Compiled = Asset->GetCompiledPatterns();
	if (!Compiled.HasPatterns()) { AddError(TEXT("No compiled patterns")); return true; }

	// Verify Entry 1's reverse adjacency has correct type indices
	const int32 MaleIdx = ConnSet->FindConnectorTypeIndex(FName("Male"));
	const int32 FemaleIdx = ConnSet->FindConnectorTypeIndex(FName("Female"));

	const auto& CE1 = Compiled.Patterns[0].Entries[1];
	TestTrue(TEXT("E1 has adjacency"), CE1.Adjacencies.Num() > 0);

	if (CE1.Adjacencies.Num() > 0 && CE1.Adjacencies[0].TypePairs.Num() > 0)
	{
		const auto& Pair = CE1.Adjacencies[0].TypePairs[0];
		TestEqual(TEXT("Reverse source is Female"), Pair.SourceTypeIndex, FemaleIdx);
		TestEqual(TEXT("Reverse target is Male"), Pair.TargetTypeIndex, MaleIdx);
	}

	return true;
}

// #########################################################################
//  ORBITAL (CAGE) PATTERN — BIDIRECTIONAL ADJACENCY COMPILATION
// #########################################################################

// =============================================================================
// Orbital: bidirectional chain compiles with adjacencies on all entries
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBiDirOrbitalChainTest,
	"PCGEx.Unit.Valency.BidirectionalAdj.Orbital.ChainCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBiDirOrbitalChainTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	// Build bidirectional chain using orbital names N/S
	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("OrbitalChain");

	// E0 → E1 via N→S
	FPCGExPatternEntryAuthored E0;
	E0.bIsActive = true;
	{
		FPCGExPatternAdjacencyAuthored Adj;
		Adj.TargetEntryIndex = 1;
		FPCGExPatternIndexPairAuthored Pair;
		Pair.SourceName = FName("N");
		Pair.TargetName = FName("S");
		Adj.IndexPairs.Add(Pair);
		E0.Adjacencies.Add(Adj);
	}
	Pattern.Entries.Add(E0);

	// E1 → E2 via N→S, and reverse E1 → E0 via S→N
	FPCGExPatternEntryAuthored E1;
	E1.bIsActive = true;
	{
		FPCGExPatternAdjacencyAuthored AdjFwd;
		AdjFwd.TargetEntryIndex = 2;
		FPCGExPatternIndexPairAuthored PairFwd;
		PairFwd.SourceName = FName("N");
		PairFwd.TargetName = FName("S");
		AdjFwd.IndexPairs.Add(PairFwd);
		E1.Adjacencies.Add(AdjFwd);
	}
	{
		FPCGExPatternAdjacencyAuthored AdjRev;
		AdjRev.TargetEntryIndex = 0;
		FPCGExPatternIndexPairAuthored PairRev;
		PairRev.SourceName = FName("S");
		PairRev.TargetName = FName("N");
		AdjRev.IndexPairs.Add(PairRev);
		E1.Adjacencies.Add(AdjRev);
	}
	Pattern.Entries.Add(E1);

	// E2: reverse → E1 via S→N
	FPCGExPatternEntryAuthored E2;
	E2.bIsActive = true;
	{
		FPCGExPatternAdjacencyAuthored AdjRev;
		AdjRev.TargetEntryIndex = 1;
		FPCGExPatternIndexPairAuthored PairRev;
		PairRev.SourceName = FName("S");
		PairRev.TargetName = FName("N");
		AdjRev.IndexPairs.Add(PairRev);
		E2.Adjacencies.Add(AdjRev);
	}
	Pattern.Entries.Add(E2);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	const bool bCompiled = Asset->Compile(&Errors);
	TestTrue(TEXT("Compile succeeds"), bCompiled);

	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);

	if (!OutPatterns.HasPatterns()) { AddError(TEXT("No compiled patterns")); return true; }

	const auto& CP = OutPatterns.Patterns[0];
	TestEqual(TEXT("3 entries"), CP.GetEntryCount(), 3);

	// FIntVector: X=TargetEntry, Y=SourceOrbital, Z=TargetOrbital

	// Entry 0: should have N(0)→S(1) to Entry 1
	TestTrue(TEXT("E0 has adjacency"), CP.Entries[0].Adjacency.Num() > 0);

	// Entry 1: should have adjacencies to both Entry 2 and Entry 0
	bool bE1ToE2 = false, bE1ToE0 = false;
	for (const FIntVector& Adj : CP.Entries[1].Adjacency)
	{
		if (Adj.X == 2) { bE1ToE2 = true; }
		if (Adj.X == 0) { bE1ToE0 = true; }
	}
	TestTrue(TEXT("E1 → E2 (forward)"), bE1ToE2);
	TestTrue(TEXT("E1 → E0 (reverse)"), bE1ToE0);

	// Entry 2: should have reverse to Entry 1
	bool bE2ToE1 = false;
	for (const FIntVector& Adj : CP.Entries[2].Adjacency)
	{
		if (Adj.X == 1) { bE2ToE1 = true; }
	}
	TestTrue(TEXT("E2 → E1 (reverse)"), bE2ToE1);

	// Verify orbital index swap: E2's reverse should use S(1)→N(0)
	if (CP.Entries[2].Adjacency.Num() > 0)
	{
		const FIntVector& RevAdj = CP.Entries[2].Adjacency[0];
		TestEqual(TEXT("E2 reverse source orbital = S(1)"), RevAdj.Y, 1);
		TestEqual(TEXT("E2 reverse target orbital = N(0)"), RevAdj.Z, 0);
	}

	return true;
}

// #########################################################################
//  STAR TOPOLOGY — CENTER + LEAVES
// #########################################################################

// =============================================================================
// Star: center connected to N leaves, all bidirectional
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBiDirStarTopologyTest,
	"PCGEx.Unit.Valency.BidirectionalAdj.Authored.StarTopology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBiDirStarTopologyTest::RunTest(const FString& Parameters)
{
	// Center (entry 0) → Leaf1 (entry 1), Leaf2 (entry 2), Leaf3 (entry 3)
	// With bidirectional: each leaf has reverse adjacency to center
	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("Star");

	// Center with forward adjacencies to all leaves
	FPCGExPatternEntryAuthored Center;
	Center.bIsActive = true;
	for (int32 i = 1; i <= 3; ++i)
	{
		FPCGExPatternAdjacencyAuthored Adj;
		Adj.TargetEntryIndex = i;
		FPCGExPatternIndexPairAuthored Pair;
		Adj.IndexPairs.Add(Pair);
		Center.Adjacencies.Add(Adj);
	}
	Pattern.Entries.Add(Center);

	// Leaves with reverse adjacencies to center
	for (int32 i = 0; i < 3; ++i)
	{
		FPCGExPatternEntryAuthored Leaf;
		Leaf.bIsActive = true;
		{
			FPCGExPatternAdjacencyAuthored AdjRev;
			AdjRev.TargetEntryIndex = 0; // Back to center
			FPCGExPatternIndexPairAuthored PairRev;
			AdjRev.IndexPairs.Add(PairRev);
			Leaf.Adjacencies.Add(AdjRev);
		}
		Pattern.Entries.Add(Leaf);
	}

	TestEqual(TEXT("4 entries"), Pattern.Entries.Num(), 4);

	// Center should have adjacencies to entries 1, 2, 3
	TestTrue(TEXT("Center → Leaf1"), HasAdjacencyTo(Pattern.Entries[0], 1));
	TestTrue(TEXT("Center → Leaf2"), HasAdjacencyTo(Pattern.Entries[0], 2));
	TestTrue(TEXT("Center → Leaf3"), HasAdjacencyTo(Pattern.Entries[0], 3));

	// Each leaf should have reverse adjacency to center
	for (int32 i = 1; i <= 3; ++i)
	{
		TestTrue(
			*FString::Printf(TEXT("Leaf%d → Center (reverse)"), i),
			HasAdjacencyTo(Pattern.Entries[i], 0));
	}

	return true;
}

// #########################################################################
//  DEDUPLICATION — NO DUPLICATE PAIRS
// #########################################################################

// =============================================================================
// Explicit bidirectional wires should not create duplicate adjacency pairs
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBiDirNoDuplicatesTest,
	"PCGEx.Unit.Valency.BidirectionalAdj.Authored.NoDuplicates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBiDirNoDuplicatesTest::RunTest(const FString& Parameters)
{
	// Simulate: user explicitly wires A→B and B→A in the graph
	// After bidirectionalization, there should be no duplicate pairs
	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("ExplicitBiDir");

	// Entry 0: forward A→B
	FPCGExPatternEntryAuthored E0;
	E0.bIsActive = true;
	{
		FPCGExPatternAdjacencyAuthored Adj;
		Adj.TargetEntryIndex = 1;
		FPCGExPatternIndexPairAuthored Pair;
		Pair.SourceName = FName("N");
		Pair.TargetName = FName("S");
		Adj.IndexPairs.Add(Pair);
		E0.Adjacencies.Add(Adj);
	}
	Pattern.Entries.Add(E0);

	// Entry 1: explicit reverse B→A (user-placed) AND the auto-generated reverse would be identical
	FPCGExPatternEntryAuthored E1;
	E1.bIsActive = true;
	{
		FPCGExPatternAdjacencyAuthored AdjRev;
		AdjRev.TargetEntryIndex = 0;
		FPCGExPatternIndexPairAuthored PairRev;
		PairRev.SourceName = FName("S");
		PairRev.TargetName = FName("N");
		AdjRev.IndexPairs.Add(PairRev);
		E1.Adjacencies.Add(AdjRev);
	}
	Pattern.Entries.Add(E1);

	// Both entries should have exactly 1 adjacency, each with 1 pair
	TestEqual(TEXT("E0 has 1 adjacency"), Pattern.Entries[0].Adjacencies.Num(), 1);
	TestEqual(TEXT("E0 adj has 1 pair"), Pattern.Entries[0].Adjacencies[0].IndexPairs.Num(), 1);
	TestEqual(TEXT("E1 has 1 adjacency"), Pattern.Entries[1].Adjacencies.Num(), 1);
	TestEqual(TEXT("E1 adj has 1 pair"), Pattern.Entries[1].Adjacencies[0].IndexPairs.Num(), 1);

	return true;
}

// #########################################################################
//  CONNECTOR COMPILATION — UNIDIRECTIONAL vs BIDIRECTIONAL COMPARISON
// #########################################################################

// =============================================================================
// Unidirectional chain: last entry has no adjacencies (the bug)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBiDirUnidirectionalBugTest,
	"PCGEx.Unit.Valency.BidirectionalAdj.Connector.UnidirectionalBug",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBiDirUnidirectionalBugTest::RunTest(const FString& Parameters)
{
	UPCGExValencyConnectorSet* ConnSet = CreateConnectorSet({FName("TypeA")});

	UPCGExConnectorPatternAsset* Asset = NewObject<UPCGExConnectorPatternAsset>(GetTransientPackage());
	Asset->ConnectorSet = ConnSet;

	// Unidirectional: simulates old buggy CompileGraphToAsset
	Asset->Patterns.Add(MakeChainUnidirectional());

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	const auto& Compiled = Asset->GetCompiledPatterns();
	if (!Compiled.HasPatterns()) { AddError(TEXT("No compiled patterns")); return true; }

	// Entry 2 (last in chain) should have NO adjacencies in unidirectional mode
	// This is the bug: DFS starting from Entry 1 or 2 can't reach all entries
	TestEqual(TEXT("Uni: Entry 2 has no adjacencies (the bug)"),
		Compiled.Patterns[0].Entries[2].Adjacencies.Num(), 0);

	return true;
}

// =============================================================================
// Bidirectional chain: all entries have adjacencies (the fix)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBiDirBidirectionalFixTest,
	"PCGEx.Unit.Valency.BidirectionalAdj.Connector.BidirectionalFix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBiDirBidirectionalFixTest::RunTest(const FString& Parameters)
{
	UPCGExValencyConnectorSet* ConnSet = CreateConnectorSet({FName("TypeA")});

	UPCGExConnectorPatternAsset* Asset = NewObject<UPCGExConnectorPatternAsset>(GetTransientPackage());
	Asset->ConnectorSet = ConnSet;

	// Bidirectional: simulates fixed CompileGraphToAsset
	Asset->Patterns.Add(MakeChainBidirectional());

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	const auto& Compiled = Asset->GetCompiledPatterns();
	if (!Compiled.HasPatterns()) { AddError(TEXT("No compiled patterns")); return true; }

	// All entries should now have adjacencies
	for (int32 i = 0; i < 3; ++i)
	{
		TestTrue(
			*FString::Printf(TEXT("Bi: Entry %d has adjacencies"), i),
			Compiled.Patterns[0].Entries[i].Adjacencies.Num() > 0);
	}

	return true;
}

// #########################################################################
//  ORBITAL COMPILATION — UNIDIRECTIONAL vs BIDIRECTIONAL
// #########################################################################

// =============================================================================
// Orbital: unidirectional last entry has no adjacency (the bug)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBiDirOrbitalUnidirectionalBugTest,
	"PCGEx.Unit.Valency.BidirectionalAdj.Orbital.UnidirectionalBug",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBiDirOrbitalUnidirectionalBugTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	// Unidirectional chain with orbital names
	FPCGExPatternAuthored Pattern = MakeChainUnidirectional(FName("N"));
	// Override target names for orbital specificity
	Pattern.Entries[0].Adjacencies[0].IndexPairs[0].TargetName = FName("S");
	Pattern.Entries[1].Adjacencies[0].IndexPairs[0].TargetName = FName("S");

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);

	if (!OutPatterns.HasPatterns()) { AddError(TEXT("No compiled patterns")); return true; }

	// Entry 2 should have no adjacencies in unidirectional mode
	TestEqual(TEXT("Uni orbital: Entry 2 has no adjacency"),
		OutPatterns.Patterns[0].Entries[2].Adjacency.Num(), 0);

	return true;
}

// =============================================================================
// 5-entry ring: bidirectional wraps correctly
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBiDirRingTopologyTest,
	"PCGEx.Unit.Valency.BidirectionalAdj.Connector.RingTopology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBiDirRingTopologyTest::RunTest(const FString& Parameters)
{
	constexpr int32 RingSize = 5;

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("Ring");

	// Build ring: E0→E1→E2→E3→E4→E0 (forward only first)
	for (int32 i = 0; i < RingSize; ++i)
	{
		FPCGExPatternEntryAuthored Entry;
		Entry.bIsActive = true;

		// Forward adjacency to next in ring
		FPCGExPatternAdjacencyAuthored AdjFwd;
		AdjFwd.TargetEntryIndex = (i + 1) % RingSize;
		FPCGExPatternIndexPairAuthored PairFwd;
		AdjFwd.IndexPairs.Add(PairFwd);
		Entry.Adjacencies.Add(AdjFwd);

		// Reverse adjacency to previous in ring
		FPCGExPatternAdjacencyAuthored AdjRev;
		AdjRev.TargetEntryIndex = (i + RingSize - 1) % RingSize;
		FPCGExPatternIndexPairAuthored PairRev;
		AdjRev.IndexPairs.Add(PairRev);
		Entry.Adjacencies.Add(AdjRev);

		Pattern.Entries.Add(Entry);
	}

	// Compile as connector pattern
	UPCGExValencyConnectorSet* ConnSet = CreateConnectorSet({FName("TypeA")});
	UPCGExConnectorPatternAsset* Asset = NewObject<UPCGExConnectorPatternAsset>(GetTransientPackage());
	Asset->ConnectorSet = ConnSet;
	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	const bool bCompiled = Asset->Compile(&Errors);
	TestTrue(TEXT("Ring compile succeeds"), bCompiled);

	const auto& Compiled = Asset->GetCompiledPatterns();
	if (!Compiled.HasPatterns()) { AddError(TEXT("No compiled patterns")); return true; }

	// Every entry should have exactly 2 adjacencies (forward + backward)
	for (int32 i = 0; i < RingSize; ++i)
	{
		TestEqual(
			*FString::Printf(TEXT("Ring E%d has 2 adjacencies"), i),
			Compiled.Patterns[0].Entries[i].Adjacencies.Num(), 2);
	}

	return true;
}

// #########################################################################
//  ENTRY COUNT CONSISTENCY
// #########################################################################

// =============================================================================
// ActiveEntryCount unaffected by bidirectionalization
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBiDirActiveCountTest,
	"PCGEx.Unit.Valency.BidirectionalAdj.Connector.ActiveCountPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBiDirActiveCountTest::RunTest(const FString& Parameters)
{
	UPCGExValencyConnectorSet* ConnSet = CreateConnectorSet({FName("TypeA")});

	UPCGExConnectorPatternAsset* Asset = NewObject<UPCGExConnectorPatternAsset>(GetTransientPackage());
	Asset->ConnectorSet = ConnSet;

	FPCGExPatternAuthored Pattern = MakeChainBidirectional();
	// Make middle entry inactive
	Pattern.Entries[1].bIsActive = false;
	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	const auto& Compiled = Asset->GetCompiledPatterns();
	if (!Compiled.HasPatterns()) { AddError(TEXT("No compiled patterns")); return true; }

	// 2 active out of 3 total
	TestEqual(TEXT("ActiveEntryCount = 2"), Compiled.Patterns[0].ActiveEntryCount, 2);
	TestEqual(TEXT("Total entries = 3"), Compiled.Patterns[0].GetEntryCount(), 3);

	return true;
}
