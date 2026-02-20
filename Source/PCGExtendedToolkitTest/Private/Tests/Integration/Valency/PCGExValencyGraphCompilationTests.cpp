// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Graph Compilation Integration Tests
 *
 * Full-flow tests that exercise the actual CompileGraphToAsset() pipeline:
 *   Graph nodes + wires → CompileGraphToAsset() → authored patterns → Compile() → runtime patterns
 *
 * This is the pipeline where the bidirectional adjacency fix lives. These tests
 * programmatically build UEdGraph objects with header/entry nodes and wires,
 * then verify the compiled output has correct bidirectional adjacencies.
 *
 * Test naming convention: PCGEx.Integration.Valency.GraphCompile.<TestCase>
 */

#include "Misc/AutomationTest.h"

#include "ConnectorPatternGraph/PCGExConnectorPatternGraph.h"
#include "ConnectorPatternGraph/PCGExConnectorPatternGraphNode.h"
#include "ConnectorPatternGraph/PCGExConnectorPatternGraphSchema.h"
#include "CagePatternGraph/PCGExCagePatternGraph.h"
#include "CagePatternGraph/PCGExCagePatternGraphNode.h"
#include "CagePatternGraph/PCGExCagePatternGraphSchema.h"
#include "PatternGraph/PCGExPatternHeaderNode.h"
#include "PatternGraph/PCGExPatternGraphNode.h"

#include "Core/PCGExConnectorPatternAsset.h"
#include "Core/PCGExCagePatternAsset.h"
#include "Core/PCGExConnectorSet.h"
#include "Core/PCGExValencyPatternAsset.h"

#include "Helpers/PCGExValencyTestHelpers.h"

// =============================================================================
// Helpers: programmatically build UEdGraph objects for testing
// =============================================================================

namespace PCGExTest::GraphCompileHelpers
{
	/** Create a ConnectorSet with named types and auto-generated TypeIds */
	UPCGExValencyConnectorSet* CreateConnectorSet(const TArray<FName>& TypeNames)
	{
		UPCGExValencyConnectorSet* Set = NewObject<UPCGExValencyConnectorSet>(GetTransientPackage());
		int32 NextTypeId = 100; // Start from 100 to avoid 0 (invalid)
		for (const FName& Name : TypeNames)
		{
			FPCGExValencyConnectorEntry Entry;
			Entry.ConnectorType = Name;
#if WITH_EDITORONLY_DATA
			Entry.TypeId = NextTypeId++;
#endif
			Set->ConnectorTypes.Add(Entry);
		}
		return Set;
	}

	/**
	 * Create a connector pattern graph with its asset, schema, and connector set.
	 * Returns the graph; the asset is accessible via Graph->OwningAsset.
	 */
	UPCGExConnectorPatternGraph* CreateConnectorGraph(UPCGExValencyConnectorSet* ConnSet)
	{
		UPCGExConnectorPatternAsset* Asset = NewObject<UPCGExConnectorPatternAsset>(GetTransientPackage());
		Asset->ConnectorSet = ConnSet;

		UPCGExConnectorPatternGraph* Graph = NewObject<UPCGExConnectorPatternGraph>(Asset, NAME_None, RF_Transactional);
		Graph->Schema = UPCGExConnectorPatternGraphSchema::StaticClass();
		Graph->OwningAsset = Asset;

		return Graph;
	}

	/**
	 * Create a cage pattern graph with its asset, schema, and orbital set.
	 * Returns the graph; the asset is accessible via Graph->OwningAsset.
	 */
	UPCGExCagePatternGraph* CreateCageGraph(UPCGExValencyOrbitalSet* OrbitalSet)
	{
		UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

		UPCGExCagePatternGraph* Graph = NewObject<UPCGExCagePatternGraph>(Asset, NAME_None, RF_Transactional);
		Graph->Schema = UPCGExCagePatternGraphSchema::StaticClass();
		Graph->OwningAsset = Asset;

		return Graph;
	}

	/** Create and finalize a header node in a graph */
	UPCGExPatternHeaderNode* AddHeaderNode(UPCGExPatternGraph* Graph, FName PatternName = FName("TestPattern"))
	{
		FGraphNodeCreator<UPCGExPatternHeaderNode> Creator(*Graph);
		UPCGExPatternHeaderNode* Header = Creator.CreateNode(false);
		Header->PatternName = PatternName;
		Creator.Finalize();
		return Header;
	}

	/** Create and finalize an entry node in a graph (uses the graph's entry node class) */
	UPCGExPatternGraphNode* AddEntryNode(UPCGExPatternGraph* Graph)
	{
		UPCGExPatternGraphNode* Entry = Graph->CreateEntryNode(false);
		Graph->FinalizeNode(Entry, false);
		return Entry;
	}

	/** Wire header's RootOut to entry's RootIn */
	bool WireHeaderToEntry(UPCGExPatternHeaderNode* Header, UPCGExPatternGraphNode* Entry)
	{
		UEdGraphPin* RootOut = Header->FindPin(TEXT("RootOut"), EGPD_Output);
		UEdGraphPin* RootIn = Entry->FindPin(TEXT("RootIn"), EGPD_Input);
		if (!RootOut || !RootIn) { return false; }
		RootOut->MakeLinkTo(RootIn);
		return true;
	}

	/** Wire source's AnyOut to target's AnyIn (wildcard "Any" connection) */
	bool WireAny(UPCGExPatternGraphNode* Source, UPCGExPatternGraphNode* Target)
	{
		UEdGraphPin* AnyOut = Source->FindPin(TEXT("AnyOut"), EGPD_Output);
		UEdGraphPin* AnyIn = Target->FindPin(TEXT("AnyIn"), EGPD_Input);
		if (!AnyOut || !AnyIn) { return false; }
		AnyOut->MakeLinkTo(AnyIn);
		return true;
	}

	/**
	 * Wire source's typed output to target's typed input.
	 * Adds typed pins to both nodes if they don't already exist.
	 */
	bool WireTyped(UPCGExPatternGraphNode* Source, UPCGExPatternGraphNode* Target,
		int32 SourceTypeId, FName SourceTypeName,
		int32 TargetTypeId, FName TargetTypeName)
	{
		// Ensure both nodes have the typed pins
		if (!Source->HasTypedPin(SourceTypeId, EGPD_Output))
		{
			Source->AddTypedPin(SourceTypeId, SourceTypeName, EGPD_Output);
		}
		if (!Target->HasTypedPin(TargetTypeId, EGPD_Input))
		{
			Target->AddTypedPin(TargetTypeId, TargetTypeName, EGPD_Input);
		}

		// Find the output pin on source
		UEdGraphPin* OutPin = nullptr;
		for (UEdGraphPin* Pin : Source->Pins)
		{
			if (Pin->Direction == EGPD_Output &&
				Pin->PinType.PinCategory == UPCGExPatternGraphNode::TypedPinCategory &&
				Pin->PinType.PinSubCategory == SourceTypeName)
			{
				OutPin = Pin;
				break;
			}
		}

		// Find the input pin on target
		UEdGraphPin* InPin = nullptr;
		for (UEdGraphPin* Pin : Target->Pins)
		{
			if (Pin->Direction == EGPD_Input &&
				Pin->PinType.PinCategory == UPCGExPatternGraphNode::TypedPinCategory &&
				Pin->PinType.PinSubCategory == TargetTypeName)
			{
				InPin = Pin;
				break;
			}
		}

		if (!OutPin || !InPin) { return false; }
		OutPin->MakeLinkTo(InPin);
		return true;
	}

	/** Check if an authored entry has adjacency to a given target index */
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
//  CONNECTOR GRAPH — CHAIN TOPOLOGY
// #########################################################################

// =============================================================================
// 3-entry chain via "Any" wires → CompileGraphToAsset produces bidirectional adjacencies
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyGraphConnectorChainAnyTest,
	"PCGEx.Integration.Valency.GraphCompile.Connector.ChainAny",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyGraphConnectorChainAnyTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest::GraphCompileHelpers;

	auto* ConnSet = CreateConnectorSet({FName("TypeA")});
	auto* Graph = CreateConnectorGraph(ConnSet);
	auto* Asset = Cast<UPCGExConnectorPatternAsset>(Graph->OwningAsset.Get());

	// Build: Header → E0 → E1 → E2 (all via "Any" wires)
	auto* Header = AddHeaderNode(Graph, FName("Chain3"));
	auto* E0 = AddEntryNode(Graph);
	auto* E1 = AddEntryNode(Graph);
	auto* E2 = AddEntryNode(Graph);

	TestTrue(TEXT("Wire Header→E0"), WireHeaderToEntry(Header, E0));
	TestTrue(TEXT("Wire E0→E1"), WireAny(E0, E1));
	TestTrue(TEXT("Wire E1→E2"), WireAny(E1, E2));

	// Compile
	Graph->CompileGraphToAsset();

	// Verify authored patterns
	TestEqual(TEXT("1 pattern authored"), Asset->Patterns.Num(), 1);
	if (Asset->Patterns.Num() == 0) { return true; }

	const auto& Pattern = Asset->Patterns[0];
	TestEqual(TEXT("3 entries"), Pattern.Entries.Num(), 3);
	if (Pattern.Entries.Num() < 3) { return true; }

	// Entry 0 (root): forward to E1
	TestTrue(TEXT("E0 → E1"), HasAdjacencyTo(Pattern.Entries[0], 1));
	// Entry 1 (middle): forward to E2 AND reverse to E0
	TestTrue(TEXT("E1 → E2"), HasAdjacencyTo(Pattern.Entries[1], 2));
	TestTrue(TEXT("E1 → E0 (reverse)"), HasAdjacencyTo(Pattern.Entries[1], 0));
	// Entry 2 (tail): reverse to E1
	TestTrue(TEXT("E2 → E1 (reverse)"), HasAdjacencyTo(Pattern.Entries[2], 1));

	// Compile to runtime format (ConnectorPatternGraph auto-compiles)
	const auto& Compiled = Asset->GetCompiledPatterns();
	TestTrue(TEXT("Has compiled patterns"), Compiled.HasPatterns());

	if (Compiled.HasPatterns())
	{
		// All 3 compiled entries should have adjacencies
		for (int32 i = 0; i < 3; ++i)
		{
			TestTrue(
				*FString::Printf(TEXT("Compiled E%d has adjacencies"), i),
				Compiled.Patterns[0].Entries[i].Adjacencies.Num() > 0);
		}
	}

	return true;
}

// =============================================================================
// Root connected to MIDDLE entry — all entries still reachable via bidirectional adj
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyGraphConnectorRootMiddleTest,
	"PCGEx.Integration.Valency.GraphCompile.Connector.RootMiddle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyGraphConnectorRootMiddleTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest::GraphCompileHelpers;

	auto* ConnSet = CreateConnectorSet({FName("TypeA")});
	auto* Graph = CreateConnectorGraph(ConnSet);
	auto* Asset = Cast<UPCGExConnectorPatternAsset>(Graph->OwningAsset.Get());

	// Build: E0 ← (Any) → E1 ← Header, E1 ← (Any) → E2
	// Root is connected to E1 (the MIDDLE entry)
	auto* Header = AddHeaderNode(Graph, FName("RootMiddle"));
	auto* E0 = AddEntryNode(Graph);
	auto* E1 = AddEntryNode(Graph);
	auto* E2 = AddEntryNode(Graph);

	TestTrue(TEXT("Wire Header→E1 (middle)"), WireHeaderToEntry(Header, E1));
	TestTrue(TEXT("Wire E0→E1"), WireAny(E0, E1));
	TestTrue(TEXT("Wire E1→E2"), WireAny(E1, E2));

	Graph->CompileGraphToAsset();

	TestEqual(TEXT("1 pattern"), Asset->Patterns.Num(), 1);
	if (Asset->Patterns.Num() == 0) { return true; }

	const auto& Pattern = Asset->Patterns[0];

	// Entry 0 = E1 (root, connected to header)
	// Entry ordering depends on flood-fill from root (E1)
	// E1 is root → entry 0, then E0 and E2 are discovered via flood-fill
	TestEqual(TEXT("3 entries"), Pattern.Entries.Num(), 3);
	if (Pattern.Entries.Num() < 3) { return true; }

	// All entries must have at least one adjacency (bidirectional guarantee)
	for (int32 i = 0; i < 3; ++i)
	{
		TestTrue(
			*FString::Printf(TEXT("Entry %d has adjacencies"), i),
			Pattern.Entries[i].Adjacencies.Num() > 0);
	}

	// Compile to runtime and verify
	const auto& Compiled = Asset->GetCompiledPatterns();
	TestTrue(TEXT("Has compiled patterns"), Compiled.HasPatterns());

	if (Compiled.HasPatterns())
	{
		for (int32 i = 0; i < Compiled.Patterns[0].GetEntryCount(); ++i)
		{
			TestTrue(
				*FString::Printf(TEXT("Compiled E%d has adjacencies"), i),
				Compiled.Patterns[0].Entries[i].Adjacencies.Num() > 0);
		}
	}

	return true;
}

// =============================================================================
// Root connected to LAST entry — entries reachable only via reverse adjacencies
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyGraphConnectorRootLastTest,
	"PCGEx.Integration.Valency.GraphCompile.Connector.RootLast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyGraphConnectorRootLastTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest::GraphCompileHelpers;

	auto* ConnSet = CreateConnectorSet({FName("TypeA")});
	auto* Graph = CreateConnectorGraph(ConnSet);
	auto* Asset = Cast<UPCGExConnectorPatternAsset>(Graph->OwningAsset.Get());

	auto* Header = AddHeaderNode(Graph, FName("RootLast"));
	auto* E0 = AddEntryNode(Graph);
	auto* E1 = AddEntryNode(Graph);
	auto* E2 = AddEntryNode(Graph);

	// Root connects to E2 (LAST entry)
	TestTrue(TEXT("Wire Header→E2 (last)"), WireHeaderToEntry(Header, E2));
	TestTrue(TEXT("Wire E0→E1"), WireAny(E0, E1));
	TestTrue(TEXT("Wire E1→E2"), WireAny(E1, E2));

	Graph->CompileGraphToAsset();

	TestEqual(TEXT("1 pattern"), Asset->Patterns.Num(), 1);
	if (Asset->Patterns.Num() == 0) { return true; }

	const auto& Pattern = Asset->Patterns[0];
	TestEqual(TEXT("3 entries"), Pattern.Entries.Num(), 3);
	if (Pattern.Entries.Num() < 3) { return true; }

	// All entries must have adjacencies — this is the core regression test
	for (int32 i = 0; i < 3; ++i)
	{
		TestTrue(
			*FString::Printf(TEXT("Entry %d has adjacencies"), i),
			Pattern.Entries[i].Adjacencies.Num() > 0);
	}

	return true;
}

// #########################################################################
//  CONNECTOR GRAPH — TYPED CONNECTIONS
// #########################################################################

// =============================================================================
// Typed Male→Female chain: reverse adjacencies have swapped type names
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyGraphConnectorTypedSwapTest,
	"PCGEx.Integration.Valency.GraphCompile.Connector.TypedSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyGraphConnectorTypedSwapTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest::GraphCompileHelpers;

	auto* ConnSet = CreateConnectorSet({FName("Male"), FName("Female")});
	auto* Graph = CreateConnectorGraph(ConnSet);
	auto* Asset = Cast<UPCGExConnectorPatternAsset>(Graph->OwningAsset.Get());

	const int32 MaleTypeId = ConnSet->ConnectorTypes[0].TypeId;
	const int32 FemaleTypeId = ConnSet->ConnectorTypes[1].TypeId;

	auto* Header = AddHeaderNode(Graph, FName("TypedSwap"));
	auto* E0 = AddEntryNode(Graph);
	auto* E1 = AddEntryNode(Graph);

	TestTrue(TEXT("Wire Header→E0"), WireHeaderToEntry(Header, E0));
	// Wire E0.Male(out) → E1.Female(in)
	TestTrue(TEXT("Wire Male→Female"),
		WireTyped(E0, E1, MaleTypeId, FName("Male"), FemaleTypeId, FName("Female")));

	Graph->CompileGraphToAsset();

	TestEqual(TEXT("1 pattern"), Asset->Patterns.Num(), 1);
	if (Asset->Patterns.Num() == 0) { return true; }

	const auto& Pattern = Asset->Patterns[0];
	TestEqual(TEXT("2 entries"), Pattern.Entries.Num(), 2);
	if (Pattern.Entries.Num() < 2) { return true; }

	// Entry 0: forward adj to E1 with Male→Female
	TestTrue(TEXT("E0 → E1"), HasAdjacencyTo(Pattern.Entries[0], 1));

	if (Pattern.Entries[0].Adjacencies.Num() > 0)
	{
		const auto& FwdPair = Pattern.Entries[0].Adjacencies[0].IndexPairs[0];
		TestEqual(TEXT("Forward source = Male"), FwdPair.SourceName, FName("Male"));
		TestEqual(TEXT("Forward target = Female"), FwdPair.TargetName, FName("Female"));
	}

	// Entry 1: reverse adj to E0 with Female→Male (swapped!)
	TestTrue(TEXT("E1 → E0 (reverse)"), HasAdjacencyTo(Pattern.Entries[1], 0));

	if (Pattern.Entries[1].Adjacencies.Num() > 0)
	{
		const auto& RevPair = Pattern.Entries[1].Adjacencies[0].IndexPairs[0];
		TestEqual(TEXT("Reverse source = Female (swapped)"), RevPair.SourceName, FName("Female"));
		TestEqual(TEXT("Reverse target = Male (swapped)"), RevPair.TargetName, FName("Male"));
	}

	return true;
}

// #########################################################################
//  CONNECTOR GRAPH — STAR TOPOLOGY
// #########################################################################

// =============================================================================
// Star: center (root) + 3 leaves, all via "Any" — all bidirectional
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyGraphConnectorStarTest,
	"PCGEx.Integration.Valency.GraphCompile.Connector.Star",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyGraphConnectorStarTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest::GraphCompileHelpers;

	auto* ConnSet = CreateConnectorSet({FName("TypeA")});
	auto* Graph = CreateConnectorGraph(ConnSet);
	auto* Asset = Cast<UPCGExConnectorPatternAsset>(Graph->OwningAsset.Get());

	auto* Header = AddHeaderNode(Graph, FName("Star"));
	auto* Center = AddEntryNode(Graph);
	auto* Leaf0 = AddEntryNode(Graph);
	auto* Leaf1 = AddEntryNode(Graph);
	auto* Leaf2 = AddEntryNode(Graph);

	TestTrue(TEXT("Wire Header→Center"), WireHeaderToEntry(Header, Center));
	TestTrue(TEXT("Wire Center→Leaf0"), WireAny(Center, Leaf0));
	TestTrue(TEXT("Wire Center→Leaf1"), WireAny(Center, Leaf1));
	TestTrue(TEXT("Wire Center→Leaf2"), WireAny(Center, Leaf2));

	Graph->CompileGraphToAsset();

	TestEqual(TEXT("1 pattern"), Asset->Patterns.Num(), 1);
	if (Asset->Patterns.Num() == 0) { return true; }

	const auto& Pattern = Asset->Patterns[0];
	TestEqual(TEXT("4 entries"), Pattern.Entries.Num(), 4);
	if (Pattern.Entries.Num() < 4) { return true; }

	// Center (entry 0): adjacencies to entries 1, 2, 3
	TestTrue(TEXT("Center → Leaf0"), HasAdjacencyTo(Pattern.Entries[0], 1));
	TestTrue(TEXT("Center → Leaf1"), HasAdjacencyTo(Pattern.Entries[0], 2));
	TestTrue(TEXT("Center → Leaf2"), HasAdjacencyTo(Pattern.Entries[0], 3));

	// Each leaf: reverse adjacency back to center
	for (int32 i = 1; i <= 3; ++i)
	{
		TestTrue(
			*FString::Printf(TEXT("Leaf%d → Center (reverse)"), i - 1),
			HasAdjacencyTo(Pattern.Entries[i], 0));
	}

	return true;
}

// #########################################################################
//  CONNECTOR GRAPH — RING TOPOLOGY
// #########################################################################

// =============================================================================
// 4-entry ring: E0→E1→E2→E3→E0, all via "Any" — all bidirectional
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyGraphConnectorRingTest,
	"PCGEx.Integration.Valency.GraphCompile.Connector.Ring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyGraphConnectorRingTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest::GraphCompileHelpers;

	constexpr int32 RingSize = 4;

	auto* ConnSet = CreateConnectorSet({FName("TypeA")});
	auto* Graph = CreateConnectorGraph(ConnSet);
	auto* Asset = Cast<UPCGExConnectorPatternAsset>(Graph->OwningAsset.Get());

	auto* Header = AddHeaderNode(Graph, FName("Ring"));

	TArray<UPCGExPatternGraphNode*> Entries;
	for (int32 i = 0; i < RingSize; ++i)
	{
		Entries.Add(AddEntryNode(Graph));
	}

	TestTrue(TEXT("Wire Header→E0"), WireHeaderToEntry(Header, Entries[0]));

	// Wire ring: E0→E1, E1→E2, E2→E3, E3→E0
	for (int32 i = 0; i < RingSize; ++i)
	{
		TestTrue(
			*FString::Printf(TEXT("Wire E%d→E%d"), i, (i + 1) % RingSize),
			WireAny(Entries[i], Entries[(i + 1) % RingSize]));
	}

	Graph->CompileGraphToAsset();

	TestEqual(TEXT("1 pattern"), Asset->Patterns.Num(), 1);
	if (Asset->Patterns.Num() == 0) { return true; }

	const auto& Pattern = Asset->Patterns[0];
	TestEqual(TEXT("4 entries"), Pattern.Entries.Num(), RingSize);
	if (Pattern.Entries.Num() < RingSize) { return true; }

	// Every entry should have exactly 2 adjacencies (forward + reverse)
	for (int32 i = 0; i < RingSize; ++i)
	{
		TestEqual(
			*FString::Printf(TEXT("E%d has 2 adjacencies"), i),
			Pattern.Entries[i].Adjacencies.Num(), 2);
	}

	return true;
}

// #########################################################################
//  CONNECTOR GRAPH — DEDUPLICATION
// #########################################################################

// =============================================================================
// Explicit bidirectional wires (A→B AND B→A) should not create duplicates
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyGraphConnectorNoDuplicatesTest,
	"PCGEx.Integration.Valency.GraphCompile.Connector.NoDuplicates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyGraphConnectorNoDuplicatesTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest::GraphCompileHelpers;

	auto* ConnSet = CreateConnectorSet({FName("TypeA")});
	auto* Graph = CreateConnectorGraph(ConnSet);
	auto* Asset = Cast<UPCGExConnectorPatternAsset>(Graph->OwningAsset.Get());

	auto* Header = AddHeaderNode(Graph, FName("BiDirExplicit"));
	auto* E0 = AddEntryNode(Graph);
	auto* E1 = AddEntryNode(Graph);

	TestTrue(TEXT("Wire Header→E0"), WireHeaderToEntry(Header, E0));
	// Wire BOTH directions explicitly
	TestTrue(TEXT("Wire E0→E1"), WireAny(E0, E1));
	TestTrue(TEXT("Wire E1→E0"), WireAny(E1, E0));

	Graph->CompileGraphToAsset();

	TestEqual(TEXT("1 pattern"), Asset->Patterns.Num(), 1);
	if (Asset->Patterns.Num() == 0) { return true; }

	const auto& Pattern = Asset->Patterns[0];
	TestEqual(TEXT("2 entries"), Pattern.Entries.Num(), 2);
	if (Pattern.Entries.Num() < 2) { return true; }

	// Each entry should have exactly 1 adjacency (to the other) — no duplicates
	TestEqual(TEXT("E0 has 1 adjacency"), Pattern.Entries[0].Adjacencies.Num(), 1);
	TestEqual(TEXT("E1 has 1 adjacency"), Pattern.Entries[1].Adjacencies.Num(), 1);

	// Each adjacency should have exactly 1 type pair (not duplicated)
	if (Pattern.Entries[0].Adjacencies.Num() > 0)
	{
		TestEqual(TEXT("E0 adj has 1 pair"), Pattern.Entries[0].Adjacencies[0].IndexPairs.Num(), 1);
	}
	if (Pattern.Entries[1].Adjacencies.Num() > 0)
	{
		TestEqual(TEXT("E1 adj has 1 pair"), Pattern.Entries[1].Adjacencies[0].IndexPairs.Num(), 1);
	}

	return true;
}

// #########################################################################
//  CAGE (ORBITAL) GRAPH — CHAIN TOPOLOGY
// #########################################################################

// =============================================================================
// Cage graph: 3-entry "Any" chain → bidirectional adjacencies in orbital format
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyGraphCageChainAnyTest,
	"PCGEx.Integration.Valency.GraphCompile.Cage.ChainAny",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyGraphCageChainAnyTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest::GraphCompileHelpers;

	auto* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet({FName("N"), FName("S")});
	auto* Graph = CreateCageGraph(OrbitalSet);
	auto* Asset = Cast<UPCGExCagePatternAsset>(Graph->OwningAsset.Get());

	auto* Header = AddHeaderNode(Graph, FName("CageChain"));
	auto* E0 = AddEntryNode(Graph);
	auto* E1 = AddEntryNode(Graph);
	auto* E2 = AddEntryNode(Graph);

	TestTrue(TEXT("Wire Header→E0"), WireHeaderToEntry(Header, E0));
	TestTrue(TEXT("Wire E0→E1"), WireAny(E0, E1));
	TestTrue(TEXT("Wire E1→E2"), WireAny(E1, E2));

	// CagePatternGraph::ShouldAutoCompile() returns false,
	// so CompileGraphToAsset only writes Patterns[] — doesn't call Asset->Compile()
	Graph->CompileGraphToAsset();

	TestEqual(TEXT("1 pattern authored"), Asset->Patterns.Num(), 1);
	if (Asset->Patterns.Num() == 0) { return true; }

	const auto& Pattern = Asset->Patterns[0];
	TestEqual(TEXT("3 entries"), Pattern.Entries.Num(), 3);
	if (Pattern.Entries.Num() < 3) { return true; }

	// Bidirectional check on authored data
	TestTrue(TEXT("E0 → E1"), HasAdjacencyTo(Pattern.Entries[0], 1));
	TestTrue(TEXT("E1 → E2"), HasAdjacencyTo(Pattern.Entries[1], 2));
	TestTrue(TEXT("E1 → E0 (reverse)"), HasAdjacencyTo(Pattern.Entries[1], 0));
	TestTrue(TEXT("E2 → E1 (reverse)"), HasAdjacencyTo(Pattern.Entries[2], 1));

	// Manually compile and check orbital format
	TArray<FText> Errors;
	const bool bCompiled = Asset->Compile(&Errors);
	TestTrue(TEXT("Compile succeeds"), bCompiled);

	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);
	TestTrue(TEXT("Has orbital patterns"), OutPatterns.HasPatterns());

	if (OutPatterns.HasPatterns())
	{
		const auto& CP = OutPatterns.Patterns[0];
		// All entries should have adjacencies in orbital format too
		for (int32 i = 0; i < CP.GetEntryCount(); ++i)
		{
			TestTrue(
				*FString::Printf(TEXT("Orbital E%d has adjacency"), i),
				CP.Entries[i].Adjacency.Num() > 0);
		}
	}

	return true;
}

// #########################################################################
//  DISABLED PATTERN
// #########################################################################

// =============================================================================
// Disabled header → pattern excluded from compilation, no crash
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyGraphDisabledPatternTest,
	"PCGEx.Integration.Valency.GraphCompile.Connector.DisabledPattern",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyGraphDisabledPatternTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest::GraphCompileHelpers;

	auto* ConnSet = CreateConnectorSet({FName("TypeA")});
	auto* Graph = CreateConnectorGraph(ConnSet);
	auto* Asset = Cast<UPCGExConnectorPatternAsset>(Graph->OwningAsset.Get());

	auto* Header = AddHeaderNode(Graph, FName("Disabled"));
	Header->bEnabled = false;

	auto* E0 = AddEntryNode(Graph);
	auto* E1 = AddEntryNode(Graph);

	TestTrue(TEXT("Wire Header→E0"), WireHeaderToEntry(Header, E0));
	TestTrue(TEXT("Wire E0→E1"), WireAny(E0, E1));

	// Add a second ENABLED pattern so we have headers (otherwise CompileGraphToAsset early-outs)
	auto* Header2 = AddHeaderNode(Graph, FName("Enabled"));
	auto* E2 = AddEntryNode(Graph);
	TestTrue(TEXT("Wire Header2→E2"), WireHeaderToEntry(Header2, E2));

	Graph->CompileGraphToAsset();

	// Only the enabled pattern should be compiled
	TestEqual(TEXT("1 pattern (disabled excluded)"), Asset->Patterns.Num(), 1);
	if (Asset->Patterns.Num() > 0)
	{
		TestEqual(TEXT("Pattern name = Enabled"), Asset->Patterns[0].PatternName, FName("Enabled"));
	}

	return true;
}

// #########################################################################
//  MULTIPLE PATTERNS IN ONE GRAPH
// #########################################################################

// =============================================================================
// Two separate patterns in same graph — each gets correct bidirectional adjacencies
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyGraphMultiplePatternsTest,
	"PCGEx.Integration.Valency.GraphCompile.Connector.MultiplePatterns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyGraphMultiplePatternsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest::GraphCompileHelpers;

	auto* ConnSet = CreateConnectorSet({FName("TypeA")});
	auto* Graph = CreateConnectorGraph(ConnSet);
	auto* Asset = Cast<UPCGExConnectorPatternAsset>(Graph->OwningAsset.Get());

	// Pattern 1: 2-entry chain
	auto* Header1 = AddHeaderNode(Graph, FName("Pat1"));
	auto* P1E0 = AddEntryNode(Graph);
	auto* P1E1 = AddEntryNode(Graph);
	TestTrue(TEXT("P1: Wire Header→E0"), WireHeaderToEntry(Header1, P1E0));
	TestTrue(TEXT("P1: Wire E0→E1"), WireAny(P1E0, P1E1));

	// Pattern 2: 3-entry chain
	auto* Header2 = AddHeaderNode(Graph, FName("Pat2"));
	auto* P2E0 = AddEntryNode(Graph);
	auto* P2E1 = AddEntryNode(Graph);
	auto* P2E2 = AddEntryNode(Graph);
	TestTrue(TEXT("P2: Wire Header→E0"), WireHeaderToEntry(Header2, P2E0));
	TestTrue(TEXT("P2: Wire E0→E1"), WireAny(P2E0, P2E1));
	TestTrue(TEXT("P2: Wire E1→E2"), WireAny(P2E1, P2E2));

	Graph->CompileGraphToAsset();

	TestEqual(TEXT("2 patterns"), Asset->Patterns.Num(), 2);
	if (Asset->Patterns.Num() < 2) { return true; }

	// Pattern 1: 2 entries, both with adjacencies
	{
		const auto& P1 = Asset->Patterns[0];
		TestEqual(TEXT("Pat1: 2 entries"), P1.Entries.Num(), 2);
		if (P1.Entries.Num() >= 2)
		{
			TestTrue(TEXT("Pat1 E0 has adj"), P1.Entries[0].Adjacencies.Num() > 0);
			TestTrue(TEXT("Pat1 E1 has adj"), P1.Entries[1].Adjacencies.Num() > 0);
		}
	}

	// Pattern 2: 3 entries, all with adjacencies
	{
		const auto& P2 = Asset->Patterns[1];
		TestEqual(TEXT("Pat2: 3 entries"), P2.Entries.Num(), 3);
		if (P2.Entries.Num() >= 3)
		{
			for (int32 i = 0; i < 3; ++i)
			{
				TestTrue(
					*FString::Printf(TEXT("Pat2 E%d has adj"), i),
					P2.Entries[i].Adjacencies.Num() > 0);
			}
		}
	}

	return true;
}
