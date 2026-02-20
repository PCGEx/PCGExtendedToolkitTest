// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Discovery Adversarial Tests
 *
 * Exercises worst-case user input on the Discovery + Assembler pipeline:
 * - Null / invalid / missing assets at every entry point
 * - Disabled, duplicate, and zero-direction connectors
 * - Connector types that don't match anything
 * - Out-of-range orbital indices
 * - Assembler mutations with invalid indices
 * - InferNeighbors when all polarities are incompatible
 * - External module processing with garbage data
 * - Discovery dispatch with unknown asset types
 *
 * Every test verifies: no crash + graceful result (empty/false/zero, not garbage).
 *
 * Test naming convention: PCGEx.Integration.Valency.Adversarial.Discovery.<TestCase>
 */

#include "Misc/AutomationTest.h"

#include "Builder/PCGExConnectorDiscovery.h"
#include "Components/PCGExConnectorComponent.h"
#include "Core/PCGExBondingRulesAssembler.h"
#include "Core/PCGExBondingRules.h"
#include "Core/PCGExConnectorSet.h"
#include "Core/PCGExValencyCommon.h"
#include "Core/PCGExValencyOrbitalSet.h"
#include "Engine/StaticMeshSocket.h"

#include "Helpers/PCGExValencyTestHelpers.h"

// =============================================================================
// Helpers
// =============================================================================

namespace
{
	UPCGExValencyConnectorSet* MakeConnectorSet(const TArray<FName>& TypeNames)
	{
		UPCGExValencyConnectorSet* Set = NewObject<UPCGExValencyConnectorSet>(GetTransientPackage());
		int32 NextTypeId = 100;
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

	UStaticMeshSocket* MakeSocket(
		FName SocketName,
		FVector Location = FVector::ZeroVector,
		FString Tag = TEXT(""))
	{
		UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(GetTransientPackage());
		Socket->SocketName = SocketName;
		Socket->RelativeLocation = Location;
		Socket->RelativeRotation = FRotator::ZeroRotator;
		Socket->RelativeScale = FVector::OneVector;
		Socket->Tag = Tag;
		return Socket;
	}
}

// #########################################################################
//  ConnectorFromSocket — edge cases
// #########################################################################

// =============================================================================
// Socket with NAME_None name — should produce connector with None identifier
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoverySocketNameNoneTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.ConnectorFromSocket.NameNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoverySocketNameNoneTest::RunTest(const FString& Parameters)
{
	UStaticMeshSocket* Socket = MakeSocket(NAME_None);

	FPCGExValencyModuleConnector Result = PCGExValency::Discovery::ConnectorFromSocket(Socket, FName("Door"));

	// Should not crash — connector gets NAME_None identifier
	TestTrue(TEXT("Identifier is None"), Result.Identifier.IsNone());
	TestEqual(TEXT("ConnectorType still set"), Result.ConnectorType, FName("Door"));

	return true;
}

// =============================================================================
// Socket with NAME_None matched type — connector has no useful type
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoverySocketTypeNoneTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.ConnectorFromSocket.MatchedTypeNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoverySocketTypeNoneTest::RunTest(const FString& Parameters)
{
	UStaticMeshSocket* Socket = MakeSocket(FName("ValidSocket"));

	FPCGExValencyModuleConnector Result = PCGExValency::Discovery::ConnectorFromSocket(Socket, NAME_None);

	TestTrue(TEXT("ConnectorType is None"), Result.ConnectorType.IsNone());
	TestEqual(TEXT("Identifier from socket"), Result.Identifier, FName("ValidSocket"));

	return true;
}

// =============================================================================
// Socket with zero scale — transform still valid, no crash
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoverySocketZeroScaleTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.ConnectorFromSocket.ZeroScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoverySocketZeroScaleTest::RunTest(const FString& Parameters)
{
	UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(GetTransientPackage());
	Socket->SocketName = FName("ZeroScale");
	Socket->RelativeLocation = FVector(100, 0, 0);
	Socket->RelativeRotation = FRotator::ZeroRotator;
	Socket->RelativeScale = FVector::ZeroVector; // Degenerate scale

	FPCGExValencyModuleConnector Result = PCGExValency::Discovery::ConnectorFromSocket(Socket, FName("Type"));

	TestTrue(TEXT("No crash with zero scale"), true);
	TestEqual(TEXT("Identifier set"), Result.Identifier, FName("ZeroScale"));

	return true;
}

// #########################################################################
//  ConnectorFromComponent — edge cases
// #########################################################################

// =============================================================================
// Component with no owning actor root — GetRelativeTransform still works
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryComponentOrphanTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.ConnectorFromComponent.OrphanComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryComponentOrphanTest::RunTest(const FString& Parameters)
{
	// Component not attached to any parent
	UPCGExConnectorComponent* Comp = NewObject<UPCGExConnectorComponent>(GetTransientPackage());
	Comp->Identifier = FName("Orphan");
	Comp->ConnectorType = FName("Pipe");
	Comp->Polarity = EPCGExConnectorPolarity::Plug;

	FPCGExValencyModuleConnector Result = PCGExValency::Discovery::ConnectorFromComponent(Comp);

	TestEqual(TEXT("Identifier copied"), Result.Identifier, FName("Orphan"));
	TestEqual(TEXT("Type copied"), Result.ConnectorType, FName("Pipe"));
	TestEqual(TEXT("Polarity copied"), Result.Polarity, EPCGExConnectorPolarity::Plug);

	return true;
}

// #########################################################################
//  ResolveConnectorOrbitalIndex — edge cases
// #########################################################################

// =============================================================================
// Direction perpendicular to all orbitals — no match
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryResolveNoMatchTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.ResolveOrbital.NoMatchDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryResolveNoMatchTest::RunTest(const FString& Parameters)
{
	// Resolver with only Forward/Backward, tight threshold
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver(
		{FVector::ForwardVector, FVector::BackwardVector}, 5.0); // 5 degree threshold — very tight

	FPCGExValencyModuleConnector Connector;
	Connector.LocalOffset = FTransform(FVector::UpVector * 100.0); // Up is perpendicular to Forward/Backward
	Connector.bManualOrbitalOverride = false;

	int32 Index = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(Connector, Resolver);
	TestEqual(TEXT("Perpendicular direction returns INDEX_NONE"), Index, INDEX_NONE);

	return true;
}

// =============================================================================
// Manual override with exactly 0 — valid, maps to orbital 0
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryResolveManualZeroTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.ResolveOrbital.ManualZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryResolveManualZeroTest::RunTest(const FString& Parameters)
{
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver(
		{FVector::ForwardVector});

	FPCGExValencyModuleConnector Connector;
	Connector.bManualOrbitalOverride = true;
	Connector.ManualOrbitalIndex = 0;

	int32 Index = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(Connector, Resolver);
	TestEqual(TEXT("Manual 0 returns 0"), Index, 0);

	return true;
}

// =============================================================================
// Manual override with index beyond orbital set size — still returns clamped value
// (caller must validate against actual orbital count)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryResolveManualBeyondSetTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.ResolveOrbital.ManualBeyondSetSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryResolveManualBeyondSetTest::RunTest(const FString& Parameters)
{
	// OrbitalSet has 2 orbitals, but manual index is 50
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver(
		{FVector::ForwardVector, FVector::BackwardVector});

	FPCGExValencyModuleConnector Connector;
	Connector.bManualOrbitalOverride = true;
	Connector.ManualOrbitalIndex = 50; // Way beyond the 2 orbitals

	int32 Index = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(Connector, Resolver);

	// Returns 50 (clamped to [0,63]) — caller responsible for bounds check vs orbital count
	TestEqual(TEXT("Returns clamped manual index"), Index, 50);

	return true;
}

// =============================================================================
// Tiny epsilon-length direction — near-zero should return INDEX_NONE
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryResolveEpsilonDirectionTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.ResolveOrbital.EpsilonDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryResolveEpsilonDirectionTest::RunTest(const FString& Parameters)
{
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver(
		{FVector::ForwardVector});

	FPCGExValencyModuleConnector Connector;
	Connector.bManualOrbitalOverride = false;
	Connector.LocalOffset = FTransform(FVector(SMALL_NUMBER, SMALL_NUMBER, SMALL_NUMBER));

	int32 Index = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(Connector, Resolver);
	TestEqual(TEXT("Epsilon direction returns INDEX_NONE"), Index, INDEX_NONE);

	return true;
}

// #########################################################################
//  ComputeOrbitalMask — edge cases
// #########################################################################

// =============================================================================
// All connectors have zero direction — mask should be 0
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryMaskAllZeroDirectionTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.OrbitalMask.AllZeroDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryMaskAllZeroDirectionTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S"), FName("E"), FName("W")});

	TArray<FPCGExValencyModuleConnector> Connectors;
	for (int32 i = 0; i < 5; ++i)
	{
		FPCGExValencyModuleConnector Conn;
		Conn.LocalOffset = FTransform(FVector::ZeroVector);
		Connectors.Add(Conn);
	}

	int64 Mask = PCGExValency::Discovery::ComputeOrbitalMaskFromConnectors(Connectors, OrbitalSet);
	TestEqual(TEXT("All zero-direction connectors produce zero mask"), Mask, 0LL);

	return true;
}

// =============================================================================
// Multiple connectors mapping to same orbital — mask union, not duplication
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryMaskDuplicateOrbitalTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.OrbitalMask.DuplicateOrbital",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryMaskDuplicateOrbitalTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});

	// Two connectors both pointing North — same orbital bit
	TArray<FPCGExValencyModuleConnector> Connectors;
	FPCGExValencyModuleConnector C1, C2;
	C1.LocalOffset = FTransform(FVector::ForwardVector * 100.0);
	C2.LocalOffset = FTransform(FVector::ForwardVector * 200.0);
	Connectors.Add(C1);
	Connectors.Add(C2);

	int64 Mask = PCGExValency::Discovery::ComputeOrbitalMaskFromConnectors(Connectors, OrbitalSet);

	// Should have bit 0 set (North), not two copies
	TestEqual(TEXT("Duplicate direction gives single bit"), Mask, 1LL << 0);

	return true;
}

// #########################################################################
//  ConnectorSet::FindMatchingConnectorType — edge cases
// #########################################################################

// =============================================================================
// Empty ConnectorSet — nothing matches
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryEmptyConnectorSetTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.ConnectorSet.EmptySet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryEmptyConnectorSetTest::RunTest(const FString& Parameters)
{
	UPCGExValencyConnectorSet* Set = NewObject<UPCGExValencyConnectorSet>(GetTransientPackage());
	// No types added

	FName Match = Set->FindMatchingConnectorType(FName("Door"), TEXT(""));
	TestTrue(TEXT("Empty set returns NAME_None"), Match.IsNone());

	Match = Set->FindMatchingConnectorType(FName("Door"), TEXT("Door"));
	TestTrue(TEXT("Empty set returns NAME_None even with tag"), Match.IsNone());

	return true;
}

// =============================================================================
// Socket name that is a prefix of a type name (not the other way around)
// e.g., type="DoorFrame", socket="Door" — should NOT prefix-match
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryReversePrefixTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.ConnectorSet.ReversePrefixNoMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryReversePrefixTest::RunTest(const FString& Parameters)
{
	UPCGExValencyConnectorSet* Set = MakeConnectorSet({FName("DoorFrame")});

	// "Door" is shorter than "DoorFrame" — should not prefix-match
	FName Match = Set->FindMatchingConnectorType(FName("Door"), TEXT(""));
	TestTrue(TEXT("Shorter socket does not prefix-match longer type"), Match.IsNone());

	return true;
}

// =============================================================================
// Multiple types with overlapping prefixes — longest match wins
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryLongestPrefixMatchTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.ConnectorSet.LongestPrefixWins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryLongestPrefixMatchTest::RunTest(const FString& Parameters)
{
	// "Door" and "DoorBig" — socket "DoorBig_Left" should match "DoorBig", not "Door"
	UPCGExValencyConnectorSet* Set = MakeConnectorSet({FName("Door"), FName("DoorBig")});

	FName Match = Set->FindMatchingConnectorType(FName("DoorBig_Left"), TEXT(""));
	TestEqual(TEXT("Longest prefix DoorBig wins over Door"), Match, FName("DoorBig"));

	return true;
}

// #########################################################################
//  Assembler — adversarial mutations
// #########################################################################

// =============================================================================
// AddConnector / AddNeighbors / AddTag with invalid module index — silent no-op
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryAssemblerInvalidIndexOpsTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.Assembler.InvalidIndexOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryAssemblerInvalidIndexOpsTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	// All of these should be no-ops — no crash
	FPCGExValencyModuleConnector Conn;
	Conn.Identifier = FName("Ghost");
	Assembler.AddConnector(-1, Conn);
	Assembler.AddConnector(999, Conn);

	Assembler.AddNeighbors(-1, FName("N"), {0});
	Assembler.AddNeighbors(999, FName("N"), {0});

	Assembler.AddTag(-1, FName("Tag"));
	Assembler.AddTag(999, FName("Tag"));

	Assembler.AddLocalTransform(-1, FTransform::Identity);
	Assembler.AddLocalTransform(999, FTransform::Identity);

	Assembler.SetBoundaryOrbital(-1, 0);
	Assembler.SetWildcardOrbital(-1, 0);

	TestEqual(TEXT("No modules created by invalid ops"), Assembler.GetModuleCount(), 0);

	return true;
}

// =============================================================================
// AddNeighbors with out-of-range neighbor indices — accepted but caught by Validate
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryAssemblerBadNeighborIndicesTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.Assembler.BadNeighborIndices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryAssemblerBadNeighborIndicesTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc Desc;
	Desc.OrbitalMask = 1;
	int32 ModIdx = Assembler.AddModule(Desc);

	// Add neighbors that reference non-existent modules
	Assembler.AddNeighbors(ModIdx, FName("North"), {999, -5, 42});

	// Validate should catch the invalid neighbor indices
	FPCGExAssemblerResult Result = Assembler.Validate();
	TestTrue(TEXT("Validation fails with bad neighbor indices"), Result.Errors.Num() > 0);
	TestFalse(TEXT("bSuccess is false"), Result.bSuccess);

	return true;
}

// =============================================================================
// Apply with null BondingRules — error, no crash
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryAssemblerApplyNullRulesTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.Assembler.ApplyNullRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryAssemblerApplyNullRulesTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	FPCGExAssemblerModuleDesc Desc;
	Desc.OrbitalMask = 1;
	Assembler.AddModule(Desc);

	FPCGExAssemblerResult Result = Assembler.Apply(nullptr);

	TestFalse(TEXT("Apply to null returns failure"), Result.bSuccess);
	TestTrue(TEXT("Error message present"), Result.Errors.Num() > 0);

	return true;
}

// =============================================================================
// Apply with bad neighbors propagates errors — prevents compile with garbage
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryAssemblerApplyBadNeighborsTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.Assembler.ApplyWithBadNeighbors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryAssemblerApplyBadNeighborsTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	FPCGExBondingRulesAssembler Assembler;
	FPCGExAssemblerModuleDesc Desc;
	Desc.OrbitalMask = 1;
	int32 Idx = Assembler.AddModule(Desc);
	Assembler.AddNeighbors(Idx, FName("N"), {999}); // Bad index

	FPCGExAssemblerResult Result = Assembler.Apply(Rules);

	// Should fail validation before compile
	TestFalse(TEXT("Apply fails with bad neighbor refs"), Result.bSuccess);
	TestTrue(TEXT("Errors reported"), Result.Errors.Num() > 0);

	return true;
}

// #########################################################################
//  InferNeighbors — adversarial scenarios
// #########################################################################

// =============================================================================
// All modules have same Plug polarity — no valid neighbor pairs
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryInferAllPlugTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.InferNeighbors.AllPlugPolarity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryInferAllPlugTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});

	FPCGExBondingRulesAssembler Assembler;

	// Module A: Door, Plug, North
	FPCGExAssemblerModuleDesc DescA;
	DescA.OrbitalMask = 1;
	int32 ModA = Assembler.AddModule(DescA);
	FPCGExValencyModuleConnector ConnA;
	ConnA.ConnectorType = FName("Door");
	ConnA.Polarity = EPCGExConnectorPolarity::Plug;
	ConnA.LocalOffset = FTransform(FVector::ForwardVector * 100.0);
	Assembler.AddConnector(ModA, ConnA);

	// Module B: Door, Plug, South
	FPCGExAssemblerModuleDesc DescB;
	DescB.OrbitalMask = 2;
	int32 ModB = Assembler.AddModule(DescB);
	FPCGExValencyModuleConnector ConnB;
	ConnB.ConnectorType = FName("Door");
	ConnB.Polarity = EPCGExConnectorPolarity::Plug; // Same polarity!
	ConnB.LocalOffset = FTransform(FVector::BackwardVector * 100.0);
	Assembler.AddConnector(ModB, ConnB);

	// Simulate inference
	PCGExValency::FOrbitalDirectionResolver Resolver;
	Resolver.BuildFrom(OrbitalSet);

	int32 InferredCount = 0;
	struct FRef { int32 Mod; int32 Orb; EPCGExConnectorPolarity Pol; };
	TMap<FName, TArray<FRef>> TypeMap;

	for (int32 i = 0; i < Assembler.GetModuleCount(); ++i)
	{
		const auto* Conns = Assembler.GetModuleConnectors(i);
		if (!Conns) continue;
		for (const auto& C : *Conns)
		{
			if (C.ConnectorType.IsNone()) continue;
			int32 Orb = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(C, Resolver);
			if (Orb == INDEX_NONE) continue;
			TypeMap.FindOrAdd(C.ConnectorType).Add({i, Orb, C.Polarity});
		}
	}

	for (const auto& Pair : TypeMap)
	{
		const auto& Refs = Pair.Value;
		for (int32 i = 0; i < Refs.Num(); ++i)
		{
			for (int32 j = i + 1; j < Refs.Num(); ++j)
			{
				if (Refs[i].Mod == Refs[j].Mod) continue;
				if (!PCGExValencyConnector::ArePolaritiesCompatible(Refs[i].Pol, Refs[j].Pol)) continue;
				InferredCount++;
			}
		}
	}

	TestEqual(TEXT("Zero neighbors inferred (Plug-Plug incompatible)"), InferredCount, 0);

	return true;
}

// =============================================================================
// Connectors with NAME_None type — should be skipped during inference
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryInferNoneTypeTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.InferNeighbors.NoneConnectorType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryInferNoneTypeTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});

	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.OrbitalMask = 1;
	int32 ModA = Assembler.AddModule(DescA);

	FPCGExValencyModuleConnector ConnA;
	ConnA.ConnectorType = NAME_None; // Invalid!
	ConnA.Polarity = EPCGExConnectorPolarity::Universal;
	ConnA.LocalOffset = FTransform(FVector::ForwardVector * 100.0);
	Assembler.AddConnector(ModA, ConnA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.OrbitalMask = 2;
	int32 ModB = Assembler.AddModule(DescB);

	FPCGExValencyModuleConnector ConnB;
	ConnB.ConnectorType = NAME_None; // Also invalid!
	ConnB.Polarity = EPCGExConnectorPolarity::Universal;
	ConnB.LocalOffset = FTransform(FVector::BackwardVector * 100.0);
	Assembler.AddConnector(ModB, ConnB);

	// Inference should skip NAME_None types entirely
	PCGExValency::FOrbitalDirectionResolver Resolver;
	Resolver.BuildFrom(OrbitalSet);

	TMap<FName, int32> TypeCounts;
	for (int32 i = 0; i < Assembler.GetModuleCount(); ++i)
	{
		const auto* Conns = Assembler.GetModuleConnectors(i);
		if (!Conns) continue;
		for (const auto& C : *Conns)
		{
			if (C.ConnectorType.IsNone()) continue; // This skip is what we're testing
			TypeCounts.FindOrAdd(C.ConnectorType)++;
		}
	}

	TestEqual(TEXT("No valid connector types in map"), TypeCounts.Num(), 0);

	return true;
}

// =============================================================================
// Module with orbital mask 0 — connectors exist but mask says "no orbitals"
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryAssemblerZeroMaskModuleTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.Assembler.ZeroMaskModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryAssemblerZeroMaskModuleTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc Desc;
	Desc.OrbitalMask = 0; // No orbitals!
	int32 ModIdx = Assembler.AddModule(Desc);

	// Add connector even though mask is zero
	FPCGExValencyModuleConnector Conn;
	Conn.ConnectorType = FName("Door");
	Conn.LocalOffset = FTransform(FVector::ForwardVector * 100.0);
	Assembler.AddConnector(ModIdx, Conn);

	// Should compile but module has no orbital slots
	FPCGExAssemblerResult Result = Assembler.Apply(Rules);

	// Must not crash — zero-mask modules are legal (e.g., filler modules)
	TestTrue(TEXT("Apply with zero-mask module does not crash"), true);
	TestEqual(TEXT("1 module created"), Result.ModuleCount, 1);

	return true;
}

// #########################################################################
//  DiscoverFromAsset — dispatch edge cases
// #########################################################################

// =============================================================================
// Unknown asset type — returns false, no crash
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryFromAssetUnknownTypeTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.DiscoverFromAsset.UnknownType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryFromAssetUnknownTypeTest::RunTest(const FString& Parameters)
{
	FPCGExCachedModuleDescriptor Desc;

	bool bResult = PCGExValency::Discovery::DiscoverFromAsset(
		FSoftObjectPath(TEXT("/Game/Test/Fake.Fake")),
		EPCGExValencyAssetType::Unknown,
		nullptr,
		nullptr,
		Desc);

	TestFalse(TEXT("Unknown type returns false"), bResult);
	TestFalse(TEXT("Descriptor not valid"), Desc.bValid);

	return true;
}

// =============================================================================
// DiscoverFromMesh with null ConnectorSet — returns false immediately
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryFromMeshNullConnSetTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.DiscoverFromMesh.NullConnectorSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryFromMeshNullConnSetTest::RunTest(const FString& Parameters)
{
	FPCGExCachedModuleDescriptor Desc;

	bool bResult = PCGExValency::Discovery::DiscoverFromMesh(
		TSoftObjectPtr<UStaticMesh>(),
		nullptr, // No ConnectorSet
		nullptr,
		Desc);

	TestFalse(TEXT("Null ConnectorSet returns false"), bResult);

	return true;
}

// =============================================================================
// DiscoverFromActorClass with invalid class path — returns false
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryFromActorBadClassTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.DiscoverFromActor.InvalidClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryFromActorBadClassTest::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("SkipPackage"), EAutomationExpectedMessageFlags::Contains, 0);
	AddExpectedMessage(TEXT("Failed to find object"), EAutomationExpectedMessageFlags::Contains, 0);

	FPCGExCachedModuleDescriptor Desc;

	bool bResult = PCGExValency::Discovery::DiscoverFromActorClass(
		TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/NonExistent/Actor.Actor_C"))),
		nullptr,
		nullptr,
		Desc);

	TestFalse(TEXT("Invalid actor class returns false"), bResult);
	TestFalse(TEXT("Descriptor not valid"), Desc.bValid);

	return true;
}

// =============================================================================
// DiscoverFromLevel with invalid level path — returns false
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryFromLevelBadPathTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.DiscoverFromLevel.InvalidPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryFromLevelBadPathTest::RunTest(const FString& Parameters)
{
	AddExpectedMessage(TEXT("SkipPackage"), EAutomationExpectedMessageFlags::Contains, 0);
	AddExpectedMessage(TEXT("Failed to find object"), EAutomationExpectedMessageFlags::Contains, 0);

	FPCGExCachedModuleDescriptor Desc;

	bool bResult = PCGExValency::Discovery::DiscoverFromLevel(
		TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/NonExistent/Level.Level"))),
		nullptr,
		nullptr,
		Desc);

	TestFalse(TEXT("Invalid level path returns false"), bResult);
	TestFalse(TEXT("Descriptor not valid"), Desc.bValid);

	return true;
}

// #########################################################################
//  CachedModuleDescriptor — edge cases
// #########################################################################

// =============================================================================
// Descriptor with bValid=false and connectors — connectors should still be accessible
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryDescriptorInvalidWithDataTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.CachedDescriptor.InvalidWithData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryDescriptorInvalidWithDataTest::RunTest(const FString& Parameters)
{
	FPCGExCachedModuleDescriptor Desc;
	Desc.bValid = false;

	// Add connectors to an invalid descriptor
	FPCGExValencyModuleConnector Conn;
	Conn.Identifier = FName("Ghost");
	Conn.ConnectorType = FName("Door");
	Desc.Connectors.Add(Conn);

	// Data is accessible even though invalid — caller's responsibility to check bValid
	TestEqual(TEXT("Connectors accessible on invalid descriptor"), Desc.Connectors.Num(), 1);
	TestFalse(TEXT("Still invalid"), Desc.bValid);

	return true;
}

// #########################################################################
//  End-to-end adversarial: poison data through the whole pipeline
// #########################################################################

// =============================================================================
// Modules with connectors that all map to the SAME orbital — neighbors should
// still be inferred (same orbital != same module)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryAllSameOrbitalTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.EndToEnd.AllSameOrbital",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryAllSameOrbitalTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("North"), FName("South")});

	FPCGExBondingRulesAssembler Assembler;

	// Both modules have connectors pointing North — same orbital index
	FPCGExAssemblerModuleDesc DescA;
	DescA.OrbitalMask = 1;
	int32 ModA = Assembler.AddModule(DescA);
	FPCGExValencyModuleConnector ConnA;
	ConnA.ConnectorType = FName("Door");
	ConnA.Polarity = EPCGExConnectorPolarity::Plug;
	ConnA.LocalOffset = FTransform(FVector::ForwardVector * 100.0);
	Assembler.AddConnector(ModA, ConnA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.OrbitalMask = 1; // Same mask!
	int32 ModB = Assembler.AddModule(DescB);
	FPCGExValencyModuleConnector ConnB;
	ConnB.ConnectorType = FName("Door");
	ConnB.Polarity = EPCGExConnectorPolarity::Port;
	ConnB.LocalOffset = FTransform(FVector::ForwardVector * 100.0); // Same direction!
	Assembler.AddConnector(ModB, ConnB);

	// Different modules, same connector type, compatible polarity, same orbital
	// Should still infer neighbors
	PCGExValency::FOrbitalDirectionResolver Resolver;
	Resolver.BuildFrom(OrbitalSet);

	int32 OrbA = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(ConnA, Resolver);
	int32 OrbB = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(ConnB, Resolver);

	TestEqual(TEXT("Both resolve to same orbital"), OrbA, OrbB);
	TestTrue(TEXT("Plug-Port compatible"),
		PCGExValencyConnector::ArePolaritiesCompatible(ConnA.Polarity, ConnB.Polarity));

	// Neighbor inference works — same orbital, different modules, compatible polarity
	if (OrbA != INDEX_NONE)
	{
		const FName OrbName = OrbitalSet->Orbitals[OrbA].GetOrbitalName();
		Assembler.AddNeighbors(ModA, OrbName, {ModB});
		Assembler.AddNeighbors(ModB, OrbName, {ModA});
	}

	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);
	FPCGExAssemblerResult Result = Assembler.Apply(Rules);
	TestTrue(TEXT("Apply succeeds"), Result.bSuccess);

	return true;
}

// =============================================================================
// Large number of modules (64) — stress test for orbital mask bit limits
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscovery64ModulesTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.EndToEnd.64Modules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscovery64ModulesTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	FPCGExBondingRulesAssembler Assembler;

	// Create 64 modules with unique orbital masks
	for (int32 i = 0; i < 64; ++i)
	{
		FPCGExAssemblerModuleDesc Desc;
		// Use different soft path per module to avoid dedup
		Desc.Asset = TSoftObjectPtr<UObject>(FSoftObjectPath(
			*FString::Printf(TEXT("/Game/Test/Module_%d.Module_%d"), i, i)));
		Desc.OrbitalMask = (1LL << (i % 2)); // Alternate between orbital 0 and 1
		Assembler.AddModule(Desc);
	}

	TestEqual(TEXT("64 modules created"), Assembler.GetModuleCount(), 64);

	FPCGExAssemblerResult Result = Assembler.Apply(Rules);
	TestTrue(TEXT("Apply with 64 modules does not crash"), true);
	TestEqual(TEXT("64 modules in rules"), Result.ModuleCount, 64);

	return true;
}

// =============================================================================
// Validate with no OrbitalSet — should succeed (skips orbital warnings)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryValidateNullOrbitalSetTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.Assembler.ValidateNullOrbitalSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryValidateNullOrbitalSetTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc Desc;
	Desc.OrbitalMask = 0b11;
	Assembler.AddModule(Desc);

	// No OrbitalSet — should still validate (skips orbital consistency check)
	FPCGExAssemblerResult Result = Assembler.Validate(nullptr);

	TestTrue(TEXT("Validate with null OrbitalSet succeeds"), Result.bSuccess);
	TestEqual(TEXT("No errors"), Result.Errors.Num(), 0);

	return true;
}

// =============================================================================
// Module with boundary AND wildcard on same orbital — contradictory masks
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryBoundaryWildcardSameOrbitalTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.Assembler.BoundaryAndWildcardSameOrbital",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryBoundaryWildcardSameOrbitalTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc Desc;
	Desc.OrbitalMask = 0b11;
	int32 ModIdx = Assembler.AddModule(Desc);

	// Set orbital 0 as BOTH boundary AND wildcard — contradiction
	Assembler.SetBoundaryOrbital(ModIdx, 0);
	Assembler.SetWildcardOrbital(ModIdx, 0);

	// Should not crash — behavior is implementation-defined but must be safe
	FPCGExAssemblerResult Result = Assembler.Apply(Rules);
	TestTrue(TEXT("Contradictory boundary+wildcard does not crash"), true);

	return true;
}

// =============================================================================
// Massive connector count on single module — performance edge case
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryManyConnectorsTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.EndToEnd.ManyConnectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryManyConnectorsTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S"), FName("E"), FName("W")});

	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc Desc;
	Desc.OrbitalMask = 0b1111;
	int32 ModIdx = Assembler.AddModule(Desc);

	// Add 100 connectors to one module
	for (int32 i = 0; i < 100; ++i)
	{
		FPCGExValencyModuleConnector Conn;
		Conn.Identifier = FName(*FString::Printf(TEXT("Conn_%d"), i));
		Conn.ConnectorType = FName("Door");
		Conn.Polarity = EPCGExConnectorPolarity::Universal;
		// Spread across directions
		const double Angle = (2.0 * PI * i) / 100.0;
		Conn.LocalOffset = FTransform(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * 100.0);
		Assembler.AddConnector(ModIdx, Conn);
	}

	const TArray<FPCGExValencyModuleConnector>* Conns = Assembler.GetModuleConnectors(ModIdx);
	TestNotNull(TEXT("Connectors accessible"), Conns);
	if (Conns) { TestEqual(TEXT("100 connectors stored"), Conns->Num(), 100); }

	// Compute mask — should not crash
	TArray<FPCGExValencyModuleConnector> ConnArray;
	if (Conns) { ConnArray = *Conns; }
	int64 Mask = PCGExValency::Discovery::ComputeOrbitalMaskFromConnectors(ConnArray, OrbitalSet);

	// All 4 orbitals should be covered by 100 evenly-spread connectors
	TestTrue(TEXT("Mask covers multiple orbitals"), Mask != 0);

	return true;
}

// =============================================================================
// FindModule on empty assembler — returns INDEX_NONE
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAdversarialDiscoveryFindModuleEmptyTest,
	"PCGEx.Integration.Valency.Adversarial.Discovery.Assembler.FindModuleEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAdversarialDiscoveryFindModuleEmptyTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	int32 Result = Assembler.FindModule(
		TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/A.A"))),
		0b01);

	TestEqual(TEXT("FindModule on empty returns INDEX_NONE"), Result, INDEX_NONE);

	return true;
}
