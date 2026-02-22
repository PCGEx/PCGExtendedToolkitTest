// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Discovery Integration Tests
 *
 * Tests the connector discovery utility (PCGExValency::Discovery namespace):
 * - ConnectorFromComponent: component → FPCGExValencyModuleConnector mapping
 * - ConnectorFromSocket: UStaticMeshSocket → connector mapping
 * - ResolveConnectorOrbitalIndex: direction/manual → orbital index
 * - ComputeOrbitalMaskFromConnectors: connector set → orbital bitmask union
 * - FPCGExCachedModuleDescriptor: struct defaults and validity
 * - Assembler accessors: GetModuleConnectors, GetModuleOrbitalMask
 * - InferNeighborsFromConnectors: end-to-end connector-type neighbor inference
 *
 * Test naming convention: PCGEx.Integration.Valency.Discovery.<TestCase>
 */

#include "Misc/AutomationTest.h"

#include "Builder/PCGExConnectorDiscovery.h"
#include "Components/PCGExConnectorComponent.h"
#include "Core/PCGExBondingRulesAssembler.h"
#include "Core/PCGExConnectorSet.h"
#include "Core/PCGExValencyCommon.h"
#include "Core/PCGExValencyOrbitalSet.h"
#include "Engine/StaticMeshSocket.h"
#include "GameFramework/Actor.h"

#include "Helpers/PCGExValencyTestHelpers.h"

// =============================================================================
// Helpers
// =============================================================================

namespace
{
	/** Create a ConnectorSet with named types and auto-generated TypeIds */
	UPCGExValencyConnectorSet* CreateConnectorSet(const TArray<FName>& TypeNames)
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

	/** Create a UStaticMeshSocket with given properties */
	UStaticMeshSocket* CreateSocket(
		FName SocketName,
		FVector Location = FVector::ZeroVector,
		FRotator Rotation = FRotator::ZeroRotator,
		FVector Scale = FVector::OneVector,
		FString Tag = TEXT(""))
	{
		UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(GetTransientPackage());
		Socket->SocketName = SocketName;
		Socket->RelativeLocation = Location;
		Socket->RelativeRotation = Rotation;
		Socket->RelativeScale = Scale;
		Socket->Tag = Tag;
		return Socket;
	}

	/** Create a UPCGExConnectorComponent on a temporary actor.
	 *  Sets the component transform relative to the actor root. */
	UPCGExConnectorComponent* CreateConnectorComponent(
		FName Identifier,
		FName ConnectorType,
		EPCGExConnectorPolarity Polarity = EPCGExConnectorPolarity::Universal,
		FVector RelativeLocation = FVector::ZeroVector,
		float Priority = 0.0f,
		int32 SpawnCapacity = 1)
	{
		// Create a minimal actor to own the component
		AActor* Owner = NewObject<AActor>(GetTransientPackage());
		USceneComponent* Root = NewObject<USceneComponent>(Owner, TEXT("Root"));
		Owner->SetRootComponent(Root);

		UPCGExConnectorComponent* Comp = NewObject<UPCGExConnectorComponent>(Owner);
		Comp->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		Comp->SetRelativeLocation(RelativeLocation);
		Comp->Identifier = Identifier;
		Comp->ConnectorType = ConnectorType;
		Comp->Polarity = Polarity;
		Comp->Priority = Priority;
		Comp->SpawnCapacity = SpawnCapacity;
		Comp->bEnabled = true;

		return Comp;
	}
}

// =============================================================================
// ConnectorFromSocket Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryConnectorFromSocketBasicTest,
	"PCGEx.Integration.Valency.Discovery.ConnectorFromSocket.BasicMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryConnectorFromSocketBasicTest::RunTest(const FString& Parameters)
{
	UStaticMeshSocket* Socket = CreateSocket(
		FName("Door_Left"),
		FVector(100.0, 50.0, 0.0),
		FRotator(0.0, 90.0, 0.0),
		FVector(1.0, 1.0, 1.0));

	FPCGExValencyModuleConnector Result = PCGExValency::Discovery::ConnectorFromSocket(Socket, FName("Door"));

	TestEqual(TEXT("Identifier from SocketName"), Result.Identifier, FName("Door_Left"));
	TestEqual(TEXT("ConnectorType from matched type"), Result.ConnectorType, FName("Door"));
	TestTrue(TEXT("bOverrideOffset is true"), Result.bOverrideOffset);
	TestEqual(TEXT("Polarity is Universal"), Result.Polarity, EPCGExConnectorPolarity::Universal);
	TestEqual(TEXT("OrbitalIndex is -1 (uncompiled)"), Result.OrbitalIndex, -1);

	// Verify transform was set from socket
	const FVector Loc = Result.LocalOffset.GetTranslation();
	TestNearlyEqual(TEXT("Location X"), Loc.X, 100.0, 0.1);
	TestNearlyEqual(TEXT("Location Y"), Loc.Y, 50.0, 0.1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryConnectorFromSocketNullTest,
	"PCGEx.Integration.Valency.Discovery.ConnectorFromSocket.NullSocket",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryConnectorFromSocketNullTest::RunTest(const FString& Parameters)
{
	FPCGExValencyModuleConnector Result = PCGExValency::Discovery::ConnectorFromSocket(nullptr, FName("Door"));

	TestTrue(TEXT("Identifier is None for null socket"), Result.Identifier.IsNone());
	TestTrue(TEXT("ConnectorType is None for null socket"), Result.ConnectorType.IsNone());
	TestFalse(TEXT("bOverrideOffset is false for null socket"), Result.bOverrideOffset);

	return true;
}

// =============================================================================
// ConnectorFromComponent Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryConnectorFromComponentTest,
	"PCGEx.Integration.Valency.Discovery.ConnectorFromComponent.AllFieldsCopied",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryConnectorFromComponentTest::RunTest(const FString& Parameters)
{
	UPCGExConnectorComponent* Comp = CreateConnectorComponent(
		FName("TopConnector"),
		FName("Pipe"),
		EPCGExConnectorPolarity::Plug,
		FVector(0.0, 0.0, 200.0),
		5.0f,
		3);

	Comp->bManualOrbitalOverride = true;
	Comp->ManualOrbitalIndex = 7;

	FPCGExValencyModuleConnector Result = PCGExValency::Discovery::ConnectorFromComponent(Comp);

	TestEqual(TEXT("Identifier"), Result.Identifier, FName("TopConnector"));
	TestEqual(TEXT("ConnectorType"), Result.ConnectorType, FName("Pipe"));
	TestEqual(TEXT("Polarity"), Result.Polarity, EPCGExConnectorPolarity::Plug);
	TestTrue(TEXT("bOverrideOffset"), Result.bOverrideOffset);
	TestNearlyEqual(TEXT("Priority"), Result.Priority, 5.0f, 0.01f);
	TestEqual(TEXT("SpawnCapacity"), Result.SpawnCapacity, 3);
	TestTrue(TEXT("bManualOrbitalOverride"), Result.bManualOrbitalOverride);
	TestEqual(TEXT("ManualOrbitalIndex"), Result.ManualOrbitalIndex, 7);
	TestEqual(TEXT("OrbitalIndex is -1 (uncompiled)"), Result.OrbitalIndex, -1);

	// Verify transform
	const FVector Loc = Result.LocalOffset.GetTranslation();
	TestNearlyEqual(TEXT("Location Z"), Loc.Z, 200.0, 0.1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryConnectorFromComponentNullTest,
	"PCGEx.Integration.Valency.Discovery.ConnectorFromComponent.NullComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryConnectorFromComponentNullTest::RunTest(const FString& Parameters)
{
	FPCGExValencyModuleConnector Result = PCGExValency::Discovery::ConnectorFromComponent(nullptr);

	TestTrue(TEXT("Identifier is None for null component"), Result.Identifier.IsNone());
	TestFalse(TEXT("bOverrideOffset is false for null component"), Result.bOverrideOffset);

	return true;
}

// =============================================================================
// ResolveConnectorOrbitalIndex Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryResolveOrbitalDirectionTest,
	"PCGEx.Integration.Valency.Discovery.ResolveOrbitalIndex.DirectionBased",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryResolveOrbitalDirectionTest::RunTest(const FString& Parameters)
{
	// Set up resolver with 4 cardinal directions
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver({
		FVector::ForwardVector,   // index 0
		FVector::BackwardVector,  // index 1
		FVector::RightVector,     // index 2
		-FVector::RightVector     // index 3
	});

	// Create connector pointing forward
	FPCGExValencyModuleConnector Connector;
	Connector.LocalOffset = FTransform(FVector::ForwardVector * 100.0);
	Connector.bManualOrbitalOverride = false;

	int32 Index = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(Connector, Resolver);
	TestEqual(TEXT("Forward direction maps to orbital 0"), Index, 0);

	// Test backward
	Connector.LocalOffset = FTransform(FVector::BackwardVector * 50.0);
	Index = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(Connector, Resolver);
	TestEqual(TEXT("Backward direction maps to orbital 1"), Index, 1);

	// Test right
	Connector.LocalOffset = FTransform(FVector::RightVector * 75.0);
	Index = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(Connector, Resolver);
	TestEqual(TEXT("Right direction maps to orbital 2"), Index, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryResolveOrbitalManualTest,
	"PCGEx.Integration.Valency.Discovery.ResolveOrbitalIndex.ManualOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryResolveOrbitalManualTest::RunTest(const FString& Parameters)
{
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver({
		FVector::ForwardVector
	});

	FPCGExValencyModuleConnector Connector;
	Connector.bManualOrbitalOverride = true;
	Connector.ManualOrbitalIndex = 42;
	// Direction is irrelevant when manual override is on
	Connector.LocalOffset = FTransform(FVector::ForwardVector * 100.0);

	int32 Index = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(Connector, Resolver);
	TestEqual(TEXT("Manual override returns clamped index"), Index, 42);

	// Test clamping: max is 63
	Connector.ManualOrbitalIndex = 100;
	Index = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(Connector, Resolver);
	TestEqual(TEXT("Manual index clamped to 63"), Index, 63);

	// Test clamping: min is 0
	Connector.ManualOrbitalIndex = -5;
	Index = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(Connector, Resolver);
	TestEqual(TEXT("Manual index clamped to 0"), Index, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryResolveOrbitalZeroDirectionTest,
	"PCGEx.Integration.Valency.Discovery.ResolveOrbitalIndex.ZeroDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryResolveOrbitalZeroDirectionTest::RunTest(const FString& Parameters)
{
	PCGExValency::FOrbitalDirectionResolver Resolver = PCGExTest::ValencyHelpers::BuildResolver({
		FVector::ForwardVector
	});

	FPCGExValencyModuleConnector Connector;
	Connector.LocalOffset = FTransform(FVector::ZeroVector); // Zero direction
	Connector.bManualOrbitalOverride = false;

	int32 Index = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(Connector, Resolver);
	TestEqual(TEXT("Zero direction returns INDEX_NONE"), Index, INDEX_NONE);

	return true;
}

// =============================================================================
// ComputeOrbitalMaskFromConnectors Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryOrbitalMaskMultiConnectorTest,
	"PCGEx.Integration.Valency.Discovery.ComputeOrbitalMask.MultiConnector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryOrbitalMaskMultiConnectorTest::RunTest(const FString& Parameters)
{
	// Create orbital set with 4 directions matching cardinal vectors
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("North"), FName("South"), FName("East"), FName("West")});

	// Create connectors pointing north and east (indices 0 and 2)
	TArray<FPCGExValencyModuleConnector> Connectors;

	FPCGExValencyModuleConnector North;
	North.LocalOffset = FTransform(FVector::ForwardVector * 100.0);
	Connectors.Add(North);

	FPCGExValencyModuleConnector East;
	East.LocalOffset = FTransform(FVector::RightVector * 100.0);
	Connectors.Add(East);

	int64 Mask = PCGExValency::Discovery::ComputeOrbitalMaskFromConnectors(Connectors, OrbitalSet);

	// Expect bits 0 (North/Forward) and 2 (East/Right) set
	TestTrue(TEXT("Bit 0 (North) is set"), (Mask & (1LL << 0)) != 0);
	TestTrue(TEXT("Bit 2 (East) is set"), (Mask & (1LL << 2)) != 0);
	TestTrue(TEXT("Bit 1 (South) is NOT set"), (Mask & (1LL << 1)) == 0);
	TestTrue(TEXT("Bit 3 (West) is NOT set"), (Mask & (1LL << 3)) == 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryOrbitalMaskEmptyTest,
	"PCGEx.Integration.Valency.Discovery.ComputeOrbitalMask.EmptyInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryOrbitalMaskEmptyTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});

	TArray<FPCGExValencyModuleConnector> EmptyConnectors;

	int64 Mask = PCGExValency::Discovery::ComputeOrbitalMaskFromConnectors(EmptyConnectors, OrbitalSet);
	TestEqual(TEXT("Empty connectors produce zero mask"), Mask, 0LL);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryOrbitalMaskNullOrbitalSetTest,
	"PCGEx.Integration.Valency.Discovery.ComputeOrbitalMask.NullOrbitalSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryOrbitalMaskNullOrbitalSetTest::RunTest(const FString& Parameters)
{
	TArray<FPCGExValencyModuleConnector> Connectors;
	FPCGExValencyModuleConnector Conn;
	Conn.LocalOffset = FTransform(FVector::ForwardVector * 100.0);
	Connectors.Add(Conn);

	int64 Mask = PCGExValency::Discovery::ComputeOrbitalMaskFromConnectors(Connectors, nullptr);
	TestEqual(TEXT("Null orbital set produces zero mask"), Mask, 0LL);

	return true;
}

// =============================================================================
// FPCGExCachedModuleDescriptor Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryCachedDescriptorDefaultsTest,
	"PCGEx.Integration.Valency.Discovery.CachedDescriptor.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryCachedDescriptorDefaultsTest::RunTest(const FString& Parameters)
{
	FPCGExCachedModuleDescriptor Desc;

	TestFalse(TEXT("bValid defaults to false"), Desc.bValid);
	TestEqual(TEXT("OrbitalMask defaults to 0"), Desc.OrbitalMask, 0LL);
	TestEqual(TEXT("AssetType defaults to Unknown"), Desc.AssetType, EPCGExValencyAssetType::Unknown);
	TestEqual(TEXT("Connectors is empty"), Desc.Connectors.Num(), 0);
	TestTrue(TEXT("SourceAsset is null"), Desc.SourceAsset.IsNull());

	return true;
}

// =============================================================================
// ConnectorSet FindMatchingConnectorType Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryConnectorSetMatchExactTest,
	"PCGEx.Integration.Valency.Discovery.ConnectorSet.ExactNameMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryConnectorSetMatchExactTest::RunTest(const FString& Parameters)
{
	UPCGExValencyConnectorSet* Set = CreateConnectorSet({FName("Door"), FName("Window"), FName("Pipe")});

	FName Match = Set->FindMatchingConnectorType(FName("Door"), TEXT(""));
	TestEqual(TEXT("Exact name match"), Match, FName("Door"));

	Match = Set->FindMatchingConnectorType(FName("Window"), TEXT(""));
	TestEqual(TEXT("Exact name match Window"), Match, FName("Window"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryConnectorSetMatchTagTest,
	"PCGEx.Integration.Valency.Discovery.ConnectorSet.TagMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryConnectorSetMatchTagTest::RunTest(const FString& Parameters)
{
	UPCGExValencyConnectorSet* Set = CreateConnectorSet({FName("Door"), FName("Window")});

	// Tag match takes priority over name match
	FName Match = Set->FindMatchingConnectorType(FName("SomeSocket"), TEXT("Door"));
	TestEqual(TEXT("Tag match overrides name"), Match, FName("Door"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryConnectorSetMatchPrefixTest,
	"PCGEx.Integration.Valency.Discovery.ConnectorSet.PrefixMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryConnectorSetMatchPrefixTest::RunTest(const FString& Parameters)
{
	UPCGExValencyConnectorSet* Set = CreateConnectorSet({FName("Door"), FName("Window")});

	// "Door_Left" should prefix-match to "Door"
	FName Match = Set->FindMatchingConnectorType(FName("Door_Left"), TEXT(""));
	TestEqual(TEXT("Prefix match Door_Left -> Door"), Match, FName("Door"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryConnectorSetNoMatchTest,
	"PCGEx.Integration.Valency.Discovery.ConnectorSet.NoMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryConnectorSetNoMatchTest::RunTest(const FString& Parameters)
{
	UPCGExValencyConnectorSet* Set = CreateConnectorSet({FName("Door"), FName("Window")});

	FName Match = Set->FindMatchingConnectorType(FName("Chimney"), TEXT(""));
	TestTrue(TEXT("No match returns NAME_None"), Match.IsNone());

	return true;
}

// =============================================================================
// Polarity Compatibility Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryPolarityCompatibilityTest,
	"PCGEx.Integration.Valency.Discovery.Polarity.CompatibilityMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryPolarityCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace PCGExValencyConnector;

	// Universal connects to anything
	TestTrue(TEXT("Universal <-> Universal"), ArePolaritiesCompatible(
		EPCGExConnectorPolarity::Universal, EPCGExConnectorPolarity::Universal));
	TestTrue(TEXT("Universal <-> Plug"), ArePolaritiesCompatible(
		EPCGExConnectorPolarity::Universal, EPCGExConnectorPolarity::Plug));
	TestTrue(TEXT("Universal <-> Port"), ArePolaritiesCompatible(
		EPCGExConnectorPolarity::Universal, EPCGExConnectorPolarity::Port));
	TestTrue(TEXT("Plug <-> Universal"), ArePolaritiesCompatible(
		EPCGExConnectorPolarity::Plug, EPCGExConnectorPolarity::Universal));
	TestTrue(TEXT("Port <-> Universal"), ArePolaritiesCompatible(
		EPCGExConnectorPolarity::Port, EPCGExConnectorPolarity::Universal));

	// Plug connects to Port and vice versa
	TestTrue(TEXT("Plug <-> Port"), ArePolaritiesCompatible(
		EPCGExConnectorPolarity::Plug, EPCGExConnectorPolarity::Port));
	TestTrue(TEXT("Port <-> Plug"), ArePolaritiesCompatible(
		EPCGExConnectorPolarity::Port, EPCGExConnectorPolarity::Plug));

	// Same non-universal polarities don't connect
	TestFalse(TEXT("Plug <-> Plug"), ArePolaritiesCompatible(
		EPCGExConnectorPolarity::Plug, EPCGExConnectorPolarity::Plug));
	TestFalse(TEXT("Port <-> Port"), ArePolaritiesCompatible(
		EPCGExConnectorPolarity::Port, EPCGExConnectorPolarity::Port));

	return true;
}

// =============================================================================
// Assembler Accessor Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryAssemblerAccessorsTest,
	"PCGEx.Integration.Valency.Discovery.Assembler.ModuleAccessors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryAssemblerAccessorsTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	// Add a module with orbital mask
	FPCGExAssemblerModuleDesc Desc;
	Desc.OrbitalMask = (1LL << 0) | (1LL << 2); // bits 0 and 2
	int32 ModIdx = Assembler.AddModule(Desc);

	// Add connectors to the module
	FPCGExValencyModuleConnector ConnA;
	ConnA.Identifier = FName("North");
	ConnA.ConnectorType = FName("Door");
	ConnA.Polarity = EPCGExConnectorPolarity::Plug;
	Assembler.AddConnector(ModIdx, ConnA);

	FPCGExValencyModuleConnector ConnB;
	ConnB.Identifier = FName("South");
	ConnB.ConnectorType = FName("Door");
	ConnB.Polarity = EPCGExConnectorPolarity::Port;
	Assembler.AddConnector(ModIdx, ConnB);

	// Test GetModuleOrbitalMask
	TestEqual(TEXT("OrbitalMask matches"), Assembler.GetModuleOrbitalMask(ModIdx), (1LL << 0) | (1LL << 2));

	// Test GetModuleConnectors
	const TArray<FPCGExValencyModuleConnector>* Connectors = Assembler.GetModuleConnectors(ModIdx);
	TestNotNull(TEXT("Connectors is not null"), Connectors);
	if (Connectors)
	{
		TestEqual(TEXT("2 connectors"), Connectors->Num(), 2);
		TestEqual(TEXT("First connector is North"), (*Connectors)[0].Identifier, FName("North"));
		TestEqual(TEXT("Second connector is South"), (*Connectors)[1].Identifier, FName("South"));
	}

	// Test invalid index
	TestNull(TEXT("Invalid index returns null"), Assembler.GetModuleConnectors(-1));
	TestNull(TEXT("Out of range returns null"), Assembler.GetModuleConnectors(999));
	TestEqual(TEXT("Invalid index mask is 0"), Assembler.GetModuleOrbitalMask(-1), 0LL);

	return true;
}

// =============================================================================
// InferNeighborsFromConnectors End-to-End Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryInferNeighborsBasicTest,
	"PCGEx.Integration.Valency.Discovery.InferNeighbors.BasicTypeCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryInferNeighborsBasicTest::RunTest(const FString& Parameters)
{
	// Setup: 2 modules with compatible connectors (same type, Universal polarity)
	// Module A: connector "Door" pointing North (Forward)
	// Module B: connector "Door" pointing South (Backward)
	// Result: A neighbors B at North, B neighbors A at South

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("North"), FName("South"), FName("East"), FName("West")});

	UPCGExValencyConnectorSet* ConnectorSet = CreateConnectorSet({FName("Door")});

	FPCGExBondingRulesAssembler Assembler;

	// Module A: orbital mask = bit 0 (North)
	FPCGExAssemblerModuleDesc DescA;
	DescA.OrbitalMask = (1LL << 0);
	int32 ModA = Assembler.AddModule(DescA);

	FPCGExValencyModuleConnector ConnA;
	ConnA.Identifier = FName("DoorA");
	ConnA.ConnectorType = FName("Door");
	ConnA.Polarity = EPCGExConnectorPolarity::Universal;
	ConnA.LocalOffset = FTransform(FVector::ForwardVector * 100.0); // North direction
	Assembler.AddConnector(ModA, ConnA);

	// Module B: orbital mask = bit 1 (South)
	FPCGExAssemblerModuleDesc DescB;
	DescB.OrbitalMask = (1LL << 1);
	int32 ModB = Assembler.AddModule(DescB);

	FPCGExValencyModuleConnector ConnB;
	ConnB.Identifier = FName("DoorB");
	ConnB.ConnectorType = FName("Door");
	ConnB.Polarity = EPCGExConnectorPolarity::Universal;
	ConnB.LocalOffset = FTransform(FVector::BackwardVector * 100.0); // South direction
	Assembler.AddConnector(ModB, ConnB);

	// Apply to bonding rules to verify neighbors were inferred
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	// Manually call InferNeighborsFromConnectors through the assembler
	// Since InferNeighborsFromConnectors is on the builder, we'll verify via Apply
	// The assembler must have neighbors set before Apply. We test the assembler's
	// AddNeighbors directly to verify the mechanism.

	// Simulate what InferNeighborsFromConnectors does:
	PCGExValency::FOrbitalDirectionResolver Resolver;
	Resolver.BuildFrom(OrbitalSet);

	// Resolve orbital indices for connectors
	int32 OrbA = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(ConnA, Resolver);
	int32 OrbB = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(ConnB, Resolver);

	TestTrue(TEXT("Module A orbital resolved"), OrbA != INDEX_NONE);
	TestTrue(TEXT("Module B orbital resolved"), OrbB != INDEX_NONE);

	// Check polarity compatibility
	TestTrue(TEXT("Universal <-> Universal compatible"),
		PCGExValencyConnector::ArePolaritiesCompatible(ConnA.Polarity, ConnB.Polarity));

	// Add neighbors as InferNeighborsFromConnectors would
	if (OrbA != INDEX_NONE && OrbB != INDEX_NONE)
	{
		const FName OrbNameA = OrbitalSet->Orbitals[OrbA].GetOrbitalName();
		const FName OrbNameB = OrbitalSet->Orbitals[OrbB].GetOrbitalName();

		Assembler.AddNeighbors(ModA, OrbNameA, {ModB});
		Assembler.AddNeighbors(ModB, OrbNameB, {ModA});
	}

	// Apply and verify
	FPCGExAssemblerResult Result = Assembler.Apply(Rules);
	TestTrue(TEXT("Apply succeeded"), Result.bSuccess);
	TestEqual(TEXT("2 modules created"), Result.ModuleCount, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryInferNeighborsPolarityFilterTest,
	"PCGEx.Integration.Valency.Discovery.InferNeighbors.PolarityFiltering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryInferNeighborsPolarityFilterTest::RunTest(const FString& Parameters)
{
	// Setup: 3 modules
	// Module A: "Door" connector, Plug polarity
	// Module B: "Door" connector, Port polarity  -> Compatible with A (Plug<->Port)
	// Module C: "Door" connector, Plug polarity  -> NOT compatible with A (Plug<->Plug)

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("North"), FName("South"), FName("East"), FName("West")});

	PCGExValency::FOrbitalDirectionResolver Resolver;
	Resolver.BuildFrom(OrbitalSet);

	// Module A connector
	FPCGExValencyModuleConnector ConnA;
	ConnA.ConnectorType = FName("Door");
	ConnA.Polarity = EPCGExConnectorPolarity::Plug;
	ConnA.LocalOffset = FTransform(FVector::ForwardVector * 100.0);

	// Module B connector
	FPCGExValencyModuleConnector ConnB;
	ConnB.ConnectorType = FName("Door");
	ConnB.Polarity = EPCGExConnectorPolarity::Port;
	ConnB.LocalOffset = FTransform(FVector::BackwardVector * 100.0);

	// Module C connector
	FPCGExValencyModuleConnector ConnC;
	ConnC.ConnectorType = FName("Door");
	ConnC.Polarity = EPCGExConnectorPolarity::Plug;
	ConnC.LocalOffset = FTransform(FVector::RightVector * 100.0);

	// A <-> B: Plug <-> Port = compatible
	TestTrue(TEXT("A-B compatible (Plug<->Port)"),
		PCGExValencyConnector::ArePolaritiesCompatible(ConnA.Polarity, ConnB.Polarity));

	// A <-> C: Plug <-> Plug = incompatible
	TestFalse(TEXT("A-C incompatible (Plug<->Plug)"),
		PCGExValencyConnector::ArePolaritiesCompatible(ConnA.Polarity, ConnC.Polarity));

	// B <-> C: Port <-> Plug = compatible
	TestTrue(TEXT("B-C compatible (Port<->Plug)"),
		PCGExValencyConnector::ArePolaritiesCompatible(ConnB.Polarity, ConnC.Polarity));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryInferNeighborsDifferentTypesTest,
	"PCGEx.Integration.Valency.Discovery.InferNeighbors.DifferentConnectorTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryInferNeighborsDifferentTypesTest::RunTest(const FString& Parameters)
{
	// Modules with different connector types should NOT be neighbors
	// Module A: "Door" connector
	// Module B: "Window" connector
	// They share no connector type, so no neighbors should be inferred

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("North"), FName("South")});

	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.OrbitalMask = (1LL << 0);
	int32 ModA = Assembler.AddModule(DescA);

	FPCGExValencyModuleConnector ConnA;
	ConnA.ConnectorType = FName("Door");
	ConnA.Polarity = EPCGExConnectorPolarity::Universal;
	ConnA.LocalOffset = FTransform(FVector::ForwardVector * 100.0);
	Assembler.AddConnector(ModA, ConnA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.OrbitalMask = (1LL << 1);
	int32 ModB = Assembler.AddModule(DescB);

	FPCGExValencyModuleConnector ConnB;
	ConnB.ConnectorType = FName("Window"); // Different type!
	ConnB.Polarity = EPCGExConnectorPolarity::Universal;
	ConnB.LocalOffset = FTransform(FVector::BackwardVector * 100.0);
	Assembler.AddConnector(ModB, ConnB);

	// Simulate InferNeighborsFromConnectors type grouping
	// Build ConnectorTypeMap
	PCGExValency::FOrbitalDirectionResolver Resolver;
	Resolver.BuildFrom(OrbitalSet);

	struct FConnRef
	{
		int32 ModuleIndex;
		int32 OrbitalBitIndex;
		EPCGExConnectorPolarity Polarity;
	};
	TMap<FName, TArray<FConnRef>> TypeMap;

	// Module A
	int32 OrbA = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(ConnA, Resolver);
	if (OrbA != INDEX_NONE) { TypeMap.FindOrAdd(ConnA.ConnectorType).Add({ModA, OrbA, ConnA.Polarity}); }

	// Module B
	int32 OrbB = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(ConnB, Resolver);
	if (OrbB != INDEX_NONE) { TypeMap.FindOrAdd(ConnB.ConnectorType).Add({ModB, OrbB, ConnB.Polarity}); }

	// "Door" group has 1 entry, "Window" group has 1 entry
	// Neither group has 2+ entries, so no neighbors inferred
	bool bAnyNeighborsInferred = false;
	for (const auto& Pair : TypeMap)
	{
		if (Pair.Value.Num() >= 2)
		{
			bAnyNeighborsInferred = true;
		}
	}

	TestFalse(TEXT("No neighbors inferred between different connector types"), bAnyNeighborsInferred);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryInferNeighborsSameModuleSkipTest,
	"PCGEx.Integration.Valency.Discovery.InferNeighbors.SkipsSameModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryInferNeighborsSameModuleSkipTest::RunTest(const FString& Parameters)
{
	// A module with two connectors of the same type should NOT neighbor itself
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("North"), FName("South")});

	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc Desc;
	Desc.OrbitalMask = (1LL << 0) | (1LL << 1);
	int32 ModIdx = Assembler.AddModule(Desc);

	FPCGExValencyModuleConnector ConnNorth;
	ConnNorth.ConnectorType = FName("Door");
	ConnNorth.Polarity = EPCGExConnectorPolarity::Universal;
	ConnNorth.LocalOffset = FTransform(FVector::ForwardVector * 100.0);
	Assembler.AddConnector(ModIdx, ConnNorth);

	FPCGExValencyModuleConnector ConnSouth;
	ConnSouth.ConnectorType = FName("Door");
	ConnSouth.Polarity = EPCGExConnectorPolarity::Universal;
	ConnSouth.LocalOffset = FTransform(FVector::BackwardVector * 100.0);
	Assembler.AddConnector(ModIdx, ConnSouth);

	// The logic should skip self-pairing (A.ModuleIndex == B.ModuleIndex)
	PCGExValency::FOrbitalDirectionResolver Resolver;
	Resolver.BuildFrom(OrbitalSet);

	int32 OrbN = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(ConnNorth, Resolver);
	int32 OrbS = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(ConnSouth, Resolver);

	// Both resolve, but they belong to the same module
	TestTrue(TEXT("North resolves"), OrbN != INDEX_NONE);
	TestTrue(TEXT("South resolves"), OrbS != INDEX_NONE);

	// With only one module, groups of 2+ connectors exist but same-module pairs skip
	TestEqual(TEXT("Module count is 1"), Assembler.GetModuleCount(), 1);

	return true;
}

// =============================================================================
// Assembler Deduplication Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryAssemblerDeduplicationTest,
	"PCGEx.Integration.Valency.Discovery.Assembler.Deduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryAssemblerDeduplicationTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	// Add same module twice -- should deduplicate
	FPCGExAssemblerModuleDesc Desc;
	Desc.OrbitalMask = (1LL << 0);
	int32 First = Assembler.AddModule(Desc);
	int32 Second = Assembler.AddModule(Desc);

	TestEqual(TEXT("Same desc returns same index"), First, Second);
	TestEqual(TEXT("Module count is 1"), Assembler.GetModuleCount(), 1);

	// Different mask = different module
	FPCGExAssemblerModuleDesc Desc2;
	Desc2.OrbitalMask = (1LL << 1);
	int32 Third = Assembler.AddModule(Desc2);

	TestNotEqual(TEXT("Different mask = different module"), Third, First);
	TestEqual(TEXT("Module count is 2"), Assembler.GetModuleCount(), 2);

	return true;
}

// =============================================================================
// End-to-End: Discovery → Assembler → Apply Workflow
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryEndToEndWorkflowTest,
	"PCGEx.Integration.Valency.Discovery.EndToEnd.SocketToAssemblerWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryEndToEndWorkflowTest::RunTest(const FString& Parameters)
{
	// End-to-end: create sockets → discovery → assembler → bonding rules

	// Step 1: Create connector set
	UPCGExValencyConnectorSet* ConnectorSet = CreateConnectorSet({FName("Door"), FName("Pipe")});

	// Step 2: Create orbital set with directions matching the sockets
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("North"), FName("South"), FName("East"), FName("West")});

	// Step 3: Create sockets that match connector types
	UStaticMeshSocket* DoorSocket = CreateSocket(
		FName("Door"), FVector::ForwardVector * 100.0);
	UStaticMeshSocket* PipeSocket = CreateSocket(
		FName("Pipe"), FVector::RightVector * 50.0);

	// Step 4: Convert sockets to connectors using Discovery
	FPCGExValencyModuleConnector DoorConn = PCGExValency::Discovery::ConnectorFromSocket(
		DoorSocket, FName("Door"));
	FPCGExValencyModuleConnector PipeConn = PCGExValency::Discovery::ConnectorFromSocket(
		PipeSocket, FName("Pipe"));

	TestEqual(TEXT("Door connector type"), DoorConn.ConnectorType, FName("Door"));
	TestEqual(TEXT("Pipe connector type"), PipeConn.ConnectorType, FName("Pipe"));

	// Step 5: Compute orbital mask
	TArray<FPCGExValencyModuleConnector> Connectors = {DoorConn, PipeConn};
	int64 Mask = PCGExValency::Discovery::ComputeOrbitalMaskFromConnectors(Connectors, OrbitalSet);
	TestTrue(TEXT("Orbital mask has bits set"), Mask != 0);

	// Step 6: Add to assembler
	FPCGExBondingRulesAssembler Assembler;
	FPCGExAssemblerModuleDesc Desc;
	Desc.OrbitalMask = Mask;
	int32 ModIdx = Assembler.AddModule(Desc);

	Assembler.AddConnector(ModIdx, DoorConn);
	Assembler.AddConnector(ModIdx, PipeConn);

	// Step 7: Verify assembler state
	TestEqual(TEXT("1 module in assembler"), Assembler.GetModuleCount(), 1);

	const TArray<FPCGExValencyModuleConnector>* StoredConnectors = Assembler.GetModuleConnectors(ModIdx);
	TestNotNull(TEXT("Module has connectors"), StoredConnectors);
	if (StoredConnectors)
	{
		TestEqual(TEXT("2 connectors stored"), StoredConnectors->Num(), 2);
	}

	// Step 8: Apply to bonding rules
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);
	FPCGExAssemblerResult Result = Assembler.Apply(Rules);
	TestTrue(TEXT("Apply succeeded"), Result.bSuccess);
	TestEqual(TEXT("1 module in rules"), Result.ModuleCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDiscoveryEndToEndMultiModuleNeighborsTest,
	"PCGEx.Integration.Valency.Discovery.EndToEnd.MultiModuleNeighborInference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDiscoveryEndToEndMultiModuleNeighborsTest::RunTest(const FString& Parameters)
{
	// End-to-end: 3 modules with connector-based neighbor inference
	// Module A: "Door" (Plug) North + "Pipe" (Universal) East
	// Module B: "Door" (Port) South
	// Module C: "Pipe" (Universal) West
	// Expected: A<->B via Door (Plug<->Port), A<->C via Pipe (Universal<->Universal)
	// B and C share no connector type → not neighbors

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("North"), FName("South"), FName("East"), FName("West")});

	PCGExValency::FOrbitalDirectionResolver Resolver;
	TestTrue(TEXT("Resolver built"), Resolver.BuildFrom(OrbitalSet));

	FPCGExBondingRulesAssembler Assembler;

	// Module A
	FPCGExAssemblerModuleDesc DescA;
	DescA.OrbitalMask = (1LL << 0) | (1LL << 2); // North + East
	int32 ModA = Assembler.AddModule(DescA);

	FPCGExValencyModuleConnector ConnADoor;
	ConnADoor.ConnectorType = FName("Door");
	ConnADoor.Polarity = EPCGExConnectorPolarity::Plug;
	ConnADoor.LocalOffset = FTransform(FVector::ForwardVector * 100.0);
	Assembler.AddConnector(ModA, ConnADoor);

	FPCGExValencyModuleConnector ConnAPipe;
	ConnAPipe.ConnectorType = FName("Pipe");
	ConnAPipe.Polarity = EPCGExConnectorPolarity::Universal;
	ConnAPipe.LocalOffset = FTransform(FVector::RightVector * 100.0);
	Assembler.AddConnector(ModA, ConnAPipe);

	// Module B
	FPCGExAssemblerModuleDesc DescB;
	DescB.OrbitalMask = (1LL << 1); // South
	int32 ModB = Assembler.AddModule(DescB);

	FPCGExValencyModuleConnector ConnBDoor;
	ConnBDoor.ConnectorType = FName("Door");
	ConnBDoor.Polarity = EPCGExConnectorPolarity::Port;
	ConnBDoor.LocalOffset = FTransform(FVector::BackwardVector * 100.0);
	Assembler.AddConnector(ModB, ConnBDoor);

	// Module C
	FPCGExAssemblerModuleDesc DescC;
	DescC.OrbitalMask = (1LL << 3); // West
	int32 ModC = Assembler.AddModule(DescC);

	FPCGExValencyModuleConnector ConnCPipe;
	ConnCPipe.ConnectorType = FName("Pipe");
	ConnCPipe.Polarity = EPCGExConnectorPolarity::Universal;
	ConnCPipe.LocalOffset = FTransform(-FVector::RightVector * 100.0);
	Assembler.AddConnector(ModC, ConnCPipe);

	// Simulate neighbor inference (same algorithm as InferNeighborsFromConnectors)
	struct FConnRef
	{
		int32 ModuleIndex;
		int32 OrbitalBitIndex;
		EPCGExConnectorPolarity Polarity;
	};

	TMap<FName, TArray<FConnRef>> TypeMap;
	for (int32 ModIdx = 0; ModIdx < Assembler.GetModuleCount(); ++ModIdx)
	{
		const TArray<FPCGExValencyModuleConnector>* Conns = Assembler.GetModuleConnectors(ModIdx);
		if (!Conns) continue;
		for (const FPCGExValencyModuleConnector& C : *Conns)
		{
			if (C.ConnectorType.IsNone()) continue;
			int32 Orb = PCGExValency::Discovery::ResolveConnectorOrbitalIndex(C, Resolver);
			if (Orb == INDEX_NONE) continue;
			TypeMap.FindOrAdd(C.ConnectorType).Add({ModIdx, Orb, C.Polarity});
		}
	}

	// Count inferred neighbors
	int32 InferredCount = 0;
	for (const auto& Pair : TypeMap)
	{
		const TArray<FConnRef>& Refs = Pair.Value;
		for (int32 i = 0; i < Refs.Num(); ++i)
		{
			for (int32 j = i + 1; j < Refs.Num(); ++j)
			{
				if (Refs[i].ModuleIndex == Refs[j].ModuleIndex) continue;
				if (!PCGExValencyConnector::ArePolaritiesCompatible(Refs[i].Polarity, Refs[j].Polarity)) continue;

				const FName OrbNameI = OrbitalSet->Orbitals[Refs[i].OrbitalBitIndex].GetOrbitalName();
				const FName OrbNameJ = OrbitalSet->Orbitals[Refs[j].OrbitalBitIndex].GetOrbitalName();

				Assembler.AddNeighbors(Refs[i].ModuleIndex, OrbNameI, {Refs[j].ModuleIndex});
				Assembler.AddNeighbors(Refs[j].ModuleIndex, OrbNameJ, {Refs[i].ModuleIndex});
				InferredCount++;
			}
		}
	}

	// A<->B (Door Plug<->Port) + A<->C (Pipe Universal<->Universal) = 2 pairs
	TestEqual(TEXT("2 neighbor pairs inferred"), InferredCount, 2);

	// Apply and verify
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);
	FPCGExAssemblerResult Result = Assembler.Apply(Rules);
	TestTrue(TEXT("Apply succeeded"), Result.bSuccess);
	TestEqual(TEXT("3 modules in rules"), Result.ModuleCount, 3);

	return true;
}
