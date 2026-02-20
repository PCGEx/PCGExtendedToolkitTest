// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency BondingRules Compile Unit Tests
 *
 * Tests UPCGExValencyBondingRules::Compile() + FPCGExValencyBondingRulesCompiled.
 * Uses assembler to build rules (avoids manual module population), then verifies
 * compiled flattened arrays.
 *
 * Test naming convention: PCGEx.Unit.Valency.BondingRulesCompile.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExBondingRulesAssembler.h"
#include "Core/PCGExBondingRules.h"
#include "Helpers/PCGExValencyTestHelpers.h"

namespace
{
	FSoftObjectPath TestAssetA() { return FSoftObjectPath(TEXT("/Game/Test/CompileA.CompileA")); }
	FSoftObjectPath TestAssetB() { return FSoftObjectPath(TEXT("/Game/Test/CompileB.CompileB")); }
	FSoftObjectPath TestAssetC() { return FSoftObjectPath(TEXT("/Game/Test/CompileC.CompileC")); }

	TSoftObjectPtr<UObject> SoftA() { return TSoftObjectPtr<UObject>(TestAssetA()); }
	TSoftObjectPtr<UObject> SoftB() { return TSoftObjectPtr<UObject>(TestAssetB()); }
	TSoftObjectPtr<UObject> SoftC() { return TSoftObjectPtr<UObject>(TestAssetC()); }

	/** Helper: build rules with given modules via assembler */
	UPCGExValencyBondingRules* BuildAndCompile(
		FPCGExBondingRulesAssembler& Assembler,
		const TArray<FName>& OrbitalNames = {FName("North"), FName("South")})
	{
		UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(OrbitalNames);
		UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);
		Assembler.Apply(Rules);
		return Rules;
	}
}

// =============================================================================
// Compile Empty — 0 modules
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBondingRulesCompileEmptyTest,
	"PCGEx.Unit.Valency.BondingRulesCompile.CompileEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBondingRulesCompileEmptyTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	UPCGExValencyBondingRules* Rules = BuildAndCompile(Assembler);

	// Empty rules still compile successfully (0 modules is valid)
	TestEqual(TEXT("CompiledData.ModuleCount is 0"), Rules->CompiledData.ModuleCount, 0);

	return true;
}

// =============================================================================
// Compile Basic Module — 1 module with flattened arrays
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBondingRulesCompileBasicModuleTest,
	"PCGEx.Unit.Valency.BondingRulesCompile.CompileBasicModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBondingRulesCompileBasicModuleTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc Desc;
	Desc.Asset = SoftA();
	Desc.OrbitalMask = 0b11;
	Desc.ModuleName = FName("SingleModule");
	Desc.Settings.Weight = 2.0f;
	Assembler.AddModule(Desc);

	UPCGExValencyBondingRules* Rules = BuildAndCompile(Assembler);

	TestEqual(TEXT("ModuleCount is 1"), Rules->CompiledData.ModuleCount, 1);
	TestEqual(TEXT("ModuleWeights size is 1"), Rules->CompiledData.ModuleWeights.Num(), 1);
	TestEqual(TEXT("ModuleOrbitalMasks size is 1"), Rules->CompiledData.ModuleOrbitalMasks.Num(), 1);
	TestEqual(TEXT("ModuleNames size is 1"), Rules->CompiledData.ModuleNames.Num(), 1);

	return true;
}

// =============================================================================
// Module Weights
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBondingRulesModuleWeightsTest,
	"PCGEx.Unit.Valency.BondingRulesCompile.ModuleWeights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBondingRulesModuleWeightsTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = SoftA();
	DescA.OrbitalMask = 0b01;
	DescA.Settings.Weight = 1.5f;
	Assembler.AddModule(DescA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = SoftB();
	DescB.OrbitalMask = 0b10;
	DescB.Settings.Weight = 3.0f;
	Assembler.AddModule(DescB);

	UPCGExValencyBondingRules* Rules = BuildAndCompile(Assembler);

	TestEqual(TEXT("Module 0 weight"), Rules->CompiledData.ModuleWeights[0], 1.5f);
	TestEqual(TEXT("Module 1 weight"), Rules->CompiledData.ModuleWeights[1], 3.0f);

	return true;
}

// =============================================================================
// Module Orbital Masks
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBondingRulesModuleOrbitalMasksTest,
	"PCGEx.Unit.Valency.BondingRulesCompile.ModuleOrbitalMasks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBondingRulesModuleOrbitalMasksTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = SoftA();
	DescA.OrbitalMask = 0b0101;
	Assembler.AddModule(DescA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = SoftB();
	DescB.OrbitalMask = 0b1010;
	Assembler.AddModule(DescB);

	UPCGExValencyBondingRules* Rules = BuildAndCompile(Assembler,
		{FName("N"), FName("S"), FName("E"), FName("W")});

	TestEqual(TEXT("Module 0 orbital mask"), Rules->CompiledData.ModuleOrbitalMasks[0], static_cast<int64>(0b0101));
	TestEqual(TEXT("Module 1 orbital mask"), Rules->CompiledData.ModuleOrbitalMasks[1], static_cast<int64>(0b1010));

	return true;
}

// =============================================================================
// Module Boundary Masks
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBondingRulesModuleBoundaryMasksTest,
	"PCGEx.Unit.Valency.BondingRulesCompile.ModuleBoundaryMasks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBondingRulesModuleBoundaryMasksTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;
	const int32 Idx = Assembler.AddModule(SoftA(), 0b11);
	Assembler.SetBoundaryOrbital(Idx, 0);

	UPCGExValencyBondingRules* Rules = BuildAndCompile(Assembler);

	TestTrue(TEXT("Boundary mask has bit 0"),
		(Rules->CompiledData.ModuleBoundaryMasks[0] & 1) != 0);
	TestTrue(TEXT("Boundary mask lacks bit 1"),
		(Rules->CompiledData.ModuleBoundaryMasks[0] & 2) == 0);

	return true;
}

// =============================================================================
// Module Names
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBondingRulesModuleNamesTest,
	"PCGEx.Unit.Valency.BondingRulesCompile.ModuleNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBondingRulesModuleNamesTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = SoftA();
	DescA.OrbitalMask = 0b01;
	DescA.ModuleName = FName("Alpha");
	Assembler.AddModule(DescA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = SoftB();
	DescB.OrbitalMask = 0b10;
	DescB.ModuleName = FName("Beta");
	Assembler.AddModule(DescB);

	UPCGExValencyBondingRules* Rules = BuildAndCompile(Assembler);

	TestEqual(TEXT("Module 0 name"), Rules->CompiledData.ModuleNames[0], FName("Alpha"));
	TestEqual(TEXT("Module 1 name"), Rules->CompiledData.ModuleNames[1], FName("Beta"));

	return true;
}

// =============================================================================
// Module Placement Policies
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBondingRulesModulePlacementPoliciesTest,
	"PCGEx.Unit.Valency.BondingRulesCompile.ModulePlacementPolicies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBondingRulesModulePlacementPoliciesTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = SoftA();
	DescA.OrbitalMask = 0b01;
	DescA.PlacementPolicy = EPCGExModulePlacementPolicy::Normal;
	Assembler.AddModule(DescA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = SoftB();
	DescB.OrbitalMask = 0b10;
	DescB.PlacementPolicy = EPCGExModulePlacementPolicy::Filler;
	Assembler.AddModule(DescB);

	FPCGExAssemblerModuleDesc DescC;
	DescC.Asset = SoftC();
	DescC.OrbitalMask = 0b11;
	DescC.PlacementPolicy = EPCGExModulePlacementPolicy::Excluded;
	Assembler.AddModule(DescC);

	UPCGExValencyBondingRules* Rules = BuildAndCompile(Assembler);

	TestEqual(TEXT("Module 0 is Normal"),
		Rules->CompiledData.ModulePlacementPolicies[0], EPCGExModulePlacementPolicy::Normal);
	TestEqual(TEXT("Module 1 is Filler"),
		Rules->CompiledData.ModulePlacementPolicies[1], EPCGExModulePlacementPolicy::Filler);
	TestEqual(TEXT("Module 2 is Excluded"),
		Rules->CompiledData.ModulePlacementPolicies[2], EPCGExModulePlacementPolicy::Excluded);

	// Verify helper functions
	TestTrue(TEXT("Module 0 IsModuleNormal"), Rules->CompiledData.IsModuleNormal(0));
	TestTrue(TEXT("Module 1 IsModuleFiller"), Rules->CompiledData.IsModuleFiller(1));
	TestTrue(TEXT("Module 2 IsModuleExcluded"), Rules->CompiledData.IsModuleExcluded(2));

	return true;
}

// =============================================================================
// BuildCandidateLookup — MaskToCandidates
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBondingRulesBuildCandidateLookupTest,
	"PCGEx.Unit.Valency.BondingRulesCompile.BuildCandidateLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBondingRulesBuildCandidateLookupTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = SoftA();
	DescA.OrbitalMask = 0b01;
	Assembler.AddModule(DescA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = SoftB();
	DescB.OrbitalMask = 0b10;
	Assembler.AddModule(DescB);

	UPCGExValencyBondingRules* Rules = BuildAndCompile(Assembler);

	Rules->CompiledData.BuildCandidateLookup();

	TestTrue(TEXT("MaskToCandidates has entries"), Rules->CompiledData.MaskToCandidates.Num() > 0);

	// Each mask should map to module indices with matching masks
	const TArray<int32>* CandA = Rules->CompiledData.MaskToCandidates.Find(0b01);
	if (CandA)
	{
		TestTrue(TEXT("Mask 0b01 candidates contain module 0"), CandA->Contains(0));
	}

	return true;
}

// =============================================================================
// Excluded modules not in candidate lookup
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBondingRulesExcludedNotInCandidatesTest,
	"PCGEx.Unit.Valency.BondingRulesCompile.ExcludedModulesNotInCandidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBondingRulesExcludedNotInCandidatesTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = SoftA();
	DescA.OrbitalMask = 0b01;
	DescA.PlacementPolicy = EPCGExModulePlacementPolicy::Excluded;
	Assembler.AddModule(DescA);

	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = SoftB();
	DescB.OrbitalMask = 0b01;
	DescB.PlacementPolicy = EPCGExModulePlacementPolicy::Normal;
	Assembler.AddModule(DescB);

	UPCGExValencyBondingRules* Rules = BuildAndCompile(Assembler);
	Rules->CompiledData.BuildCandidateLookup();

	// Check that excluded module 0 is not in any candidate list
	bool bFoundExcluded = false;
	for (const auto& Pair : Rules->CompiledData.MaskToCandidates)
	{
		if (Pair.Value.Contains(0))
		{
			bFoundExcluded = true;
			break;
		}
	}

	TestFalse(TEXT("Excluded module 0 not in any candidate list"), bFoundExcluded);

	return true;
}

// =============================================================================
// Multi-module compile — all arrays correctly sized
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyBondingRulesMultiModuleCompileTest,
	"PCGEx.Unit.Valency.BondingRulesCompile.MultiModuleCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyBondingRulesMultiModuleCompileTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	for (int32 i = 0; i < 3; ++i)
	{
		FPCGExAssemblerModuleDesc Desc;
		Desc.Asset = TSoftObjectPtr<UObject>(FSoftObjectPath(
			FString::Printf(TEXT("/Game/Test/Module%d.Module%d"), i, i)));
		Desc.OrbitalMask = 1LL << i;
		Desc.ModuleName = FName(*FString::Printf(TEXT("Module%d"), i));
		Desc.Settings.Weight = static_cast<float>(i + 1);
		Assembler.AddModule(Desc);
	}

	UPCGExValencyBondingRules* Rules = BuildAndCompile(Assembler,
		{FName("A"), FName("B"), FName("C")});

	const auto& CD = Rules->CompiledData;

	TestEqual(TEXT("ModuleCount is 3"), CD.ModuleCount, 3);
	TestEqual(TEXT("ModuleWeights size 3"), CD.ModuleWeights.Num(), 3);
	TestEqual(TEXT("ModuleOrbitalMasks size 3"), CD.ModuleOrbitalMasks.Num(), 3);
	TestEqual(TEXT("ModuleBoundaryMasks size 3"), CD.ModuleBoundaryMasks.Num(), 3);
	TestEqual(TEXT("ModuleNames size 3"), CD.ModuleNames.Num(), 3);
	TestEqual(TEXT("ModulePlacementPolicies size 3"), CD.ModulePlacementPolicies.Num(), 3);

	// Verify per-slot data
	for (int32 i = 0; i < 3; ++i)
	{
		TestEqual(*FString::Printf(TEXT("Module %d weight"), i),
			CD.ModuleWeights[i], static_cast<float>(i + 1));
		TestEqual(*FString::Printf(TEXT("Module %d mask"), i),
			CD.ModuleOrbitalMasks[i], 1LL << i);
		TestEqual(*FString::Printf(TEXT("Module %d name"), i),
			CD.ModuleNames[i], FName(*FString::Printf(TEXT("Module%d"), i)));
	}

	return true;
}
