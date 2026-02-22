// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Assembler Unit Tests
 *
 * Tests FPCGExBondingRulesAssembler from PCGExBondingRulesAssembler.h:
 * - AddModule (index allocation, deduplication)
 * - Neighbor relationships (AddNeighbors, SetBoundaryOrbital, SetWildcardOrbital)
 * - Additive data (AddLocalTransform, AddTag, AddConnector)
 * - Query (GetModuleCount, FindModule, GetKeyToIndexMap)
 * - Apply to BondingRules
 *
 * Test naming convention: PCGEx.Unit.Valency.Assembler.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExBondingRulesAssembler.h"
#include "Core/PCGExBondingRules.h"
#include "Helpers/PCGExValencyTestHelpers.h"

namespace
{
	FSoftObjectPath TestAssetA() { return FSoftObjectPath(TEXT("/Game/Test/AssetA.AssetA")); }
	FSoftObjectPath TestAssetB() { return FSoftObjectPath(TEXT("/Game/Test/AssetB.AssetB")); }
	FSoftObjectPath TestAssetC() { return FSoftObjectPath(TEXT("/Game/Test/AssetC.AssetC")); }

	TSoftObjectPtr<UObject> SoftA() { return TSoftObjectPtr<UObject>(TestAssetA()); }
	TSoftObjectPtr<UObject> SoftB() { return TSoftObjectPtr<UObject>(TestAssetB()); }
	TSoftObjectPtr<UObject> SoftC() { return TSoftObjectPtr<UObject>(TestAssetC()); }
}

// =============================================================================
// AddModule Returns Sequential Indices
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerAddModuleReturnsIndexTest,
	"PCGEx.Unit.Valency.Assembler.AddModuleReturnsIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerAddModuleReturnsIndexTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	const int32 IdxA = Assembler.AddModule(SoftA(), 0b01);
	const int32 IdxB = Assembler.AddModule(SoftB(), 0b10);

	TestEqual(TEXT("First module is index 0"), IdxA, 0);
	TestEqual(TEXT("Second module is index 1"), IdxB, 1);
	TestEqual(TEXT("Module count is 2"), Assembler.GetModuleCount(), 2);

	return true;
}

// =============================================================================
// AddModule Dedup -- Same asset+mask returns existing index
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerAddModuleDedupTest,
	"PCGEx.Unit.Valency.Assembler.AddModuleDedup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerAddModuleDedupTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	const int32 Idx1 = Assembler.AddModule(SoftA(), 0b1111);
	const int32 Idx2 = Assembler.AddModule(SoftA(), 0b1111);

	TestEqual(TEXT("Duplicate returns same index"), Idx1, Idx2);
	TestEqual(TEXT("Module count is still 1"), Assembler.GetModuleCount(), 1);

	return true;
}

// =============================================================================
// AddModule Material Variant Dedup -- Different variant = different module
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerMaterialVariantDedupTest,
	"PCGEx.Unit.Valency.Assembler.AddModuleMaterialVariantDedup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerMaterialVariantDedupTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	// First module -- no material variant
	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = SoftA();
	DescA.OrbitalMask = 0b1111;
	const int32 IdxA = Assembler.AddModule(DescA);

	// Second module -- same asset+mask but WITH material variant
	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = SoftA();
	DescB.OrbitalMask = 0b1111;
	DescB.bHasMaterialVariant = true;
	FPCGExValencyMaterialOverride Override;
	Override.SlotIndex = 0;
	Override.Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/Red.Red")));
	DescB.MaterialVariant.Overrides.Add(Override);
	const int32 IdxB = Assembler.AddModule(DescB);

	TestNotEqual(TEXT("Different material variant = different module"), IdxA, IdxB);
	TestEqual(TEXT("Module count is 2"), Assembler.GetModuleCount(), 2);

	return true;
}

// =============================================================================
// Convenience AddModule matches desc-based
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerConvenienceAddModuleTest,
	"PCGEx.Unit.Valency.Assembler.ConvenienceAddModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerConvenienceAddModuleTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	// Add via convenience
	const int32 IdxConv = Assembler.AddModule(SoftA(), 0b1010);

	// Try to add same via desc -- should dedup
	FPCGExAssemblerModuleDesc Desc;
	Desc.Asset = SoftA();
	Desc.OrbitalMask = 0b1010;
	const int32 IdxDesc = Assembler.AddModule(Desc);

	TestEqual(TEXT("Convenience and desc produce same index"), IdxConv, IdxDesc);
	TestEqual(TEXT("Module count is 1"), Assembler.GetModuleCount(), 1);

	return true;
}

// =============================================================================
// AddNeighbors
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerAddNeighborsTest,
	"PCGEx.Unit.Valency.Assembler.AddNeighbors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerAddNeighborsTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	const int32 A = Assembler.AddModule(SoftA(), 0b01);
	const int32 B = Assembler.AddModule(SoftB(), 0b10);

	Assembler.AddNeighbors(A, FName("North"), {B});
	Assembler.AddNeighbors(B, FName("South"), {A});

	// Apply to rules to verify the neighbor data transferred
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("North"), FName("South")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	const FPCGExAssemblerResult Result = Assembler.Apply(Rules);
	TestTrue(TEXT("Apply succeeds"), Result.bSuccess);
	TestEqual(TEXT("2 modules in result"), Result.ModuleCount, 2);

	return true;
}

// =============================================================================
// SetBoundaryOrbital
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerSetBoundaryOrbitalTest,
	"PCGEx.Unit.Valency.Assembler.SetBoundaryOrbital",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerSetBoundaryOrbitalTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	const int32 Idx = Assembler.AddModule(SoftA(), 0b11);

	Assembler.SetBoundaryOrbital(Idx, 0);  // Bit 0 is boundary

	// Apply and verify
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("North"), FName("South")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	Assembler.Apply(Rules);

	// After compile, boundary mask should have bit 0 set
	TestTrue(TEXT("Module has boundary bit 0"),
		(Rules->Modules[0].LayerConfig.BoundaryOrbitalMask & 1) != 0);

	return true;
}

// =============================================================================
// SetWildcardOrbital
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerSetWildcardOrbitalTest,
	"PCGEx.Unit.Valency.Assembler.SetWildcardOrbital",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerSetWildcardOrbitalTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	const int32 Idx = Assembler.AddModule(SoftA(), 0b11);

	Assembler.SetWildcardOrbital(Idx, 1);  // Bit 1 is wildcard

	// Apply and verify
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("North"), FName("South")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	Assembler.Apply(Rules);

	TestTrue(TEXT("Module has wildcard bit 1"),
		(Rules->Modules[0].LayerConfig.WildcardOrbitalMask & (1LL << 1)) != 0);

	return true;
}

// =============================================================================
// AddLocalTransform
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerAddLocalTransformTest,
	"PCGEx.Unit.Valency.Assembler.AddLocalTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerAddLocalTransformTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	const int32 Idx = Assembler.AddModule(SoftA(), 0b01);

	const FTransform T(FQuat::Identity, FVector(100, 0, 0));
	Assembler.AddLocalTransform(Idx, T);

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet({FName("North")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	Assembler.Apply(Rules);

	TestEqual(TEXT("Module has 1 local transform"), Rules->Modules[0].LocalTransforms.Num(), 1);
	TestTrue(TEXT("Transform matches"),
		Rules->Modules[0].LocalTransforms[0].GetLocation().Equals(FVector(100, 0, 0), 0.01));

	return true;
}

// =============================================================================
// AddTag
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerAddTagTest,
	"PCGEx.Unit.Valency.Assembler.AddTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerAddTagTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	const int32 Idx = Assembler.AddModule(SoftA(), 0b01);

	Assembler.AddTag(Idx, FName("TestTag"));
	Assembler.AddTag(Idx, FName("AnotherTag"));

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet({FName("North")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	Assembler.Apply(Rules);

	TestEqual(TEXT("Module has 2 tags"), Rules->Modules[0].Tags.Num(), 2);
	TestTrue(TEXT("Contains TestTag"), Rules->Modules[0].Tags.Contains(FName("TestTag")));
	TestTrue(TEXT("Contains AnotherTag"), Rules->Modules[0].Tags.Contains(FName("AnotherTag")));

	return true;
}

// =============================================================================
// GetModuleCount
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerGetModuleCountTest,
	"PCGEx.Unit.Valency.Assembler.GetModuleCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerGetModuleCountTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	TestEqual(TEXT("Empty assembler has 0 modules"), Assembler.GetModuleCount(), 0);

	Assembler.AddModule(SoftA(), 0b01);
	TestEqual(TEXT("1 module after add"), Assembler.GetModuleCount(), 1);

	Assembler.AddModule(SoftB(), 0b10);
	TestEqual(TEXT("2 modules after second add"), Assembler.GetModuleCount(), 2);

	// Dedup should not increase count
	Assembler.AddModule(SoftA(), 0b01);
	TestEqual(TEXT("Still 2 modules after dedup"), Assembler.GetModuleCount(), 2);

	return true;
}

// =============================================================================
// FindModule
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerFindModuleExistsTest,
	"PCGEx.Unit.Valency.Assembler.FindModuleExists",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerFindModuleExistsTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	Assembler.AddModule(SoftA(), 0b01);
	Assembler.AddModule(SoftB(), 0b10);

	TestEqual(TEXT("FindModule A returns 0"), Assembler.FindModule(SoftA(), 0b01), 0);
	TestEqual(TEXT("FindModule B returns 1"), Assembler.FindModule(SoftB(), 0b10), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerFindModuleNotFoundTest,
	"PCGEx.Unit.Valency.Assembler.FindModuleNotFound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerFindModuleNotFoundTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	Assembler.AddModule(SoftA(), 0b01);

	TestEqual(TEXT("Non-existent returns INDEX_NONE"),
		Assembler.FindModule(SoftC(), 0b01), INDEX_NONE);
	TestEqual(TEXT("Wrong mask returns INDEX_NONE"),
		Assembler.FindModule(SoftA(), 0b10), INDEX_NONE);

	return true;
}

// =============================================================================
// GetKeyToIndexMap
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerGetKeyToIndexMapTest,
	"PCGEx.Unit.Valency.Assembler.GetKeyToIndexMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerGetKeyToIndexMapTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	Assembler.AddModule(SoftA(), 0b01);
	Assembler.AddModule(SoftB(), 0b10);

	const TMap<FString, int32>& Map = Assembler.GetKeyToIndexMap();

	TestEqual(TEXT("Map has 2 entries"), Map.Num(), 2);

	// The keys should correspond to MakeModuleKey outputs
	const FString KeyA = PCGExValency::MakeModuleKey(TestAssetA(), 0b01);
	const FString KeyB = PCGExValency::MakeModuleKey(TestAssetB(), 0b10);

	const int32* IdxA = Map.Find(KeyA);
	const int32* IdxB = Map.Find(KeyB);

	TestNotNull(TEXT("Key A exists in map"), IdxA);
	TestNotNull(TEXT("Key B exists in map"), IdxB);

	if (IdxA) { TestEqual(TEXT("Key A maps to index 0"), *IdxA, 0); }
	if (IdxB) { TestEqual(TEXT("Key B maps to index 1"), *IdxB, 1); }

	return true;
}

// =============================================================================
// Validate -- Empty
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerValidateEmptyTest,
	"PCGEx.Unit.Valency.Assembler.ValidateEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerValidateEmptyTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	const FPCGExAssemblerResult Result = Assembler.Validate();

	TestTrue(TEXT("Empty assembler validates OK"), Result.bSuccess);
	TestEqual(TEXT("Module count is 0"), Result.ModuleCount, 0);

	return true;
}

// =============================================================================
// Apply to Rules
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerApplyToRulesTest,
	"PCGEx.Unit.Valency.Assembler.ApplyToRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerApplyToRulesTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = SoftA();
	DescA.OrbitalMask = 0b11;
	DescA.ModuleName = FName("ModuleA");
	DescA.Settings.Weight = 2.0f;
	Assembler.AddModule(DescA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = SoftB();
	DescB.OrbitalMask = 0b11;
	DescB.ModuleName = FName("ModuleB");
	DescB.PlacementPolicy = EPCGExModulePlacementPolicy::Filler;
	Assembler.AddModule(DescB);

	Assembler.AddNeighbors(0, FName("North"), {1});
	Assembler.AddNeighbors(1, FName("South"), {0});

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("North"), FName("South")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	const FPCGExAssemblerResult Result = Assembler.Apply(Rules);

	TestTrue(TEXT("Apply succeeds"), Result.bSuccess);
	TestEqual(TEXT("2 modules applied"), Result.ModuleCount, 2);
	TestEqual(TEXT("Rules has 2 modules"), Rules->GetModuleCount(), 2);
	TestTrue(TEXT("Rules is compiled"), Rules->IsCompiled());

	return true;
}

// =============================================================================
// Apply and Verify Module Data
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAssemblerApplyAndVerifyModuleDataTest,
	"PCGEx.Unit.Valency.Assembler.ApplyAndVerifyModuleData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAssemblerApplyAndVerifyModuleDataTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = SoftA();
	DescA.OrbitalMask = 0b01;
	DescA.ModuleName = FName("TestModuleA");
	DescA.Settings.Weight = 3.5f;
	DescA.Settings.MinSpawns = 2;
	DescA.Settings.MaxSpawns = 10;
	DescA.PlacementPolicy = EPCGExModulePlacementPolicy::Normal;
	Assembler.AddModule(DescA);

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet({FName("North")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	Assembler.Apply(Rules);

	// Verify the module definition matches assembler input
	const FPCGExValencyModuleDefinition& Module = Rules->Modules[0];
	TestEqual(TEXT("Module name matches"), Module.ModuleName, FName("TestModuleA"));
	TestEqual(TEXT("Weight matches"), Module.Settings.Weight, 3.5f);
	TestEqual(TEXT("MinSpawns matches"), Module.Settings.MinSpawns, 2);
	TestEqual(TEXT("MaxSpawns matches"), Module.Settings.MaxSpawns, 10);
	TestEqual(TEXT("PlacementPolicy matches"), Module.PlacementPolicy, EPCGExModulePlacementPolicy::Normal);

	return true;
}
