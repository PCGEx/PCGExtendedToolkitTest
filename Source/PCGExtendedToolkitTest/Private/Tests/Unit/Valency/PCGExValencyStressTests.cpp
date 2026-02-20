// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Stress Tests
 *
 * Heavy-data tests that push the Valency system with large/complex inputs:
 * - Max orbital counts (64)
 * - Many modules with full topology
 * - Large pattern sets with many entries
 * - Assembler dedup under load
 * - BondingRules compilation at scale
 * - EntryData packing at volume
 * - CandidateLookup with combinatorial masks
 *
 * Test naming convention: PCGEx.Stress.Valency.<Category>.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExBondingRulesAssembler.h"
#include "Core/PCGExBondingRules.h"
#include "Core/PCGExCagePatternAsset.h"
#include "Core/PCGExValencyCommon.h"
#include "Core/PCGExValencyMap.h"
#include "Core/PCGExValencyPattern.h"
#include "Helpers/PCGExValencyTestHelpers.h"

// =============================================================================
// Helper utilities
// =============================================================================

namespace
{
	/** Build orbital names: O0, O1, O2, ... ON */
	TArray<FName> MakeOrbitalNames(int32 Count)
	{
		TArray<FName> Names;
		Names.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			Names.Add(FName(*FString::Printf(TEXT("O%d"), i)));
		}
		return Names;
	}

	/** Build a unique fake asset path for module index i */
	TSoftObjectPtr<UObject> MakeFakeAsset(int32 i)
	{
		return TSoftObjectPtr<UObject>(FSoftObjectPath(
			FString::Printf(TEXT("/Game/Stress/Module%d.Module%d"), i, i)));
	}

	/** Populate assembler with N modules, each occupying a unique orbital bit */
	void PopulateModules(FPCGExBondingRulesAssembler& Assembler, int32 Count)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			FPCGExAssemblerModuleDesc Desc;
			Desc.Asset = MakeFakeAsset(i);
			Desc.OrbitalMask = 1LL << (i % 64);
			Desc.ModuleName = FName(*FString::Printf(TEXT("Mod%d"), i));
			Desc.Settings.Weight = static_cast<float>(i + 1);
			Assembler.AddModule(Desc);
		}
	}

	/** Build rules from assembler + orbital names, return compiled rules */
	UPCGExValencyBondingRules* BuildRules(
		FPCGExBondingRulesAssembler& Assembler,
		const TArray<FName>& OrbitalNames)
	{
		UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(OrbitalNames);
		UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);
		Assembler.Apply(Rules);
		return Rules;
	}

	/** Build a compiled pattern with N active entries */
	FPCGExValencyPatternCompiled MakeCompiledPattern(FName Name, int32 EntryCount)
	{
		FPCGExValencyPatternCompiled Pattern;
		Pattern.Settings.PatternName = Name;
		Pattern.ActiveEntryCount = EntryCount;
		for (int32 i = 0; i < EntryCount; ++i)
		{
			FPCGExValencyPatternEntryCompiled Entry;
			Entry.bIsActive = true;
			Pattern.Entries.Add(Entry);
		}
		return Pattern;
	}
}

// =============================================================================
// 64 Orbitals — Maximum orbital set with full compilation
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStress64OrbitalsCompileTest,
	"PCGEx.Stress.Valency.Assembler.MaxOrbitals64",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStress64OrbitalsCompileTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumOrbitals = 64;
	const TArray<FName> Names = MakeOrbitalNames(NumOrbitals);

	FPCGExBondingRulesAssembler Assembler;

	// 1 module per orbital, each on a unique bit
	for (int32 i = 0; i < NumOrbitals; ++i)
	{
		FPCGExAssemblerModuleDesc Desc;
		Desc.Asset = MakeFakeAsset(i);
		Desc.OrbitalMask = 1LL << i;
		Desc.ModuleName = FName(*FString::Printf(TEXT("Mod%d"), i));
		Assembler.AddModule(Desc);
	}

	UPCGExValencyBondingRules* Rules = BuildRules(Assembler, Names);

	TestTrue(TEXT("64-orbital compile succeeds"), Rules->IsCompiled());
	TestEqual(TEXT("64 modules compiled"), Rules->CompiledData.ModuleCount, NumOrbitals);
	TestEqual(TEXT("All mask arrays sized"), Rules->CompiledData.ModuleOrbitalMasks.Num(), NumOrbitals);
	TestEqual(TEXT("All weight arrays sized"), Rules->CompiledData.ModuleWeights.Num(), NumOrbitals);
	TestEqual(TEXT("All name arrays sized"), Rules->CompiledData.ModuleNames.Num(), NumOrbitals);

	// Verify each module got its correct single-bit mask
	for (int32 i = 0; i < NumOrbitals; ++i)
	{
		TestEqual(*FString::Printf(TEXT("Module %d mask = 1<<%d"), i, i),
			Rules->CompiledData.ModuleOrbitalMasks[i], 1LL << i);
	}

	return true;
}

// =============================================================================
// 64 Modules with fully-connected neighbor topology
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressFullyConnectedNeighborsTest,
	"PCGEx.Stress.Valency.Assembler.FullyConnectedNeighbors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressFullyConnectedNeighborsTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumModules = 32;
	constexpr int32 NumOrbitals = 4;
	const TArray<FName> Names = MakeOrbitalNames(NumOrbitals);

	FPCGExBondingRulesAssembler Assembler;
	PopulateModules(Assembler, NumModules);

	// Every module is a valid neighbor for every other module on every orbital
	TArray<int32> AllIndices;
	AllIndices.Reserve(NumModules);
	for (int32 i = 0; i < NumModules; ++i) { AllIndices.Add(i); }

	for (int32 m = 0; m < NumModules; ++m)
	{
		for (int32 o = 0; o < NumOrbitals; ++o)
		{
			Assembler.AddNeighbors(m, Names[o], AllIndices);
		}
	}

	UPCGExValencyBondingRules* Rules = BuildRules(Assembler, Names);

	TestTrue(TEXT("Fully-connected compile succeeds"), Rules->IsCompiled());
	TestEqual(TEXT("All modules compiled"), Rules->CompiledData.ModuleCount, NumModules);

	return true;
}

// =============================================================================
// Many modules with transforms — 100 modules × 10 transforms each
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressManyTransformsTest,
	"PCGEx.Stress.Valency.Assembler.ManyTransforms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressManyTransformsTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumModules = 100;
	constexpr int32 TransformsPerModule = 10;
	const TArray<FName> Names = MakeOrbitalNames(4);

	FPCGExBondingRulesAssembler Assembler;
	PopulateModules(Assembler, NumModules);

	// Add multiple transforms per module
	for (int32 m = 0; m < NumModules; ++m)
	{
		for (int32 t = 0; t < TransformsPerModule; ++t)
		{
			FTransform Transform;
			Transform.SetLocation(FVector(m * 100.0, t * 50.0, 0.0));
			Transform.SetRotation(FQuat(FRotator(0, t * 36.0f, 0)));
			Transform.SetScale3D(FVector(1.0 + t * 0.1));
			Assembler.AddLocalTransform(m, Transform);
		}
	}

	UPCGExValencyBondingRules* Rules = BuildRules(Assembler, Names);

	TestTrue(TEXT("100-module compile succeeds"), Rules->IsCompiled());
	TestEqual(TEXT("AllLocalTransforms size"),
		Rules->CompiledData.AllLocalTransforms.Num(), NumModules * TransformsPerModule);

	// Verify headers point to correct ranges
	for (int32 m = 0; m < NumModules; ++m)
	{
		const FIntPoint& Header = Rules->CompiledData.ModuleLocalTransformHeaders[m];
		TestEqual(*FString::Printf(TEXT("Module %d transform count"), m),
			Header.Y, TransformsPerModule);
		TestEqual(*FString::Printf(TEXT("Module %d transform offset"), m),
			Header.X, m * TransformsPerModule);
	}

	return true;
}

// =============================================================================
// Assembler dedup under load — 200 additions, 50 unique modules
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressAssemblerDedupTest,
	"PCGEx.Stress.Valency.Assembler.DedupUnderLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressAssemblerDedupTest::RunTest(const FString& Parameters)
{
	constexpr int32 UniqueModules = 50;
	constexpr int32 DuplicatesPerModule = 4;

	FPCGExBondingRulesAssembler Assembler;

	TArray<int32> FirstIndices;
	FirstIndices.Reserve(UniqueModules);

	// Add each unique module once
	for (int32 i = 0; i < UniqueModules; ++i)
	{
		FPCGExAssemblerModuleDesc Desc;
		Desc.Asset = MakeFakeAsset(i);
		Desc.OrbitalMask = 1LL << (i % 64);
		FirstIndices.Add(Assembler.AddModule(Desc));
	}

	// Add duplicates — should return same index
	int32 DedupCount = 0;
	for (int32 i = 0; i < UniqueModules; ++i)
	{
		for (int32 d = 0; d < DuplicatesPerModule; ++d)
		{
			FPCGExAssemblerModuleDesc Desc;
			Desc.Asset = MakeFakeAsset(i);
			Desc.OrbitalMask = 1LL << (i % 64);
			const int32 Idx = Assembler.AddModule(Desc);
			if (Idx == FirstIndices[i]) { ++DedupCount; }
		}
	}

	TestEqual(TEXT("All duplicates dedup'd"),
		DedupCount, UniqueModules * DuplicatesPerModule);
	TestEqual(TEXT("Module count is unique count"),
		Assembler.GetModuleCount(), UniqueModules);

	// Compile the dedup'd set
	const TArray<FName> Names = MakeOrbitalNames(4);
	UPCGExValencyBondingRules* Rules = BuildRules(Assembler, Names);

	TestTrue(TEXT("Dedup'd assembler compiles"), Rules->IsCompiled());
	TestEqual(TEXT("Compiled module count matches unique"),
		Rules->CompiledData.ModuleCount, UniqueModules);

	return true;
}

// =============================================================================
// Mixed placement policies — verify candidate lookup correctness at scale
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressCandidateLookupTest,
	"PCGEx.Stress.Valency.Compile.CandidateLookupScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressCandidateLookupTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumModules = 60;
	const TArray<FName> Names = MakeOrbitalNames(6);

	FPCGExBondingRulesAssembler Assembler;

	int32 ExcludedCount = 0;
	int32 FillerCount = 0;
	int32 NormalCount = 0;

	for (int32 i = 0; i < NumModules; ++i)
	{
		FPCGExAssemblerModuleDesc Desc;
		Desc.Asset = MakeFakeAsset(i);
		Desc.OrbitalMask = 1LL << (i % 6); // Distribute across 6 orbitals

		// Distribute policies: 20 Normal, 20 Filler, 20 Excluded
		if (i % 3 == 0) { Desc.PlacementPolicy = EPCGExModulePlacementPolicy::Normal; ++NormalCount; }
		else if (i % 3 == 1) { Desc.PlacementPolicy = EPCGExModulePlacementPolicy::Filler; ++FillerCount; }
		else { Desc.PlacementPolicy = EPCGExModulePlacementPolicy::Excluded; ++ExcludedCount; }

		Assembler.AddModule(Desc);
	}

	UPCGExValencyBondingRules* Rules = BuildRules(Assembler, Names);

	TestTrue(TEXT("60-module compile succeeds"), Rules->IsCompiled());

	Rules->CompiledData.BuildCandidateLookup();
	TestTrue(TEXT("Candidate lookup populated"),
		Rules->CompiledData.MaskToCandidates.Num() > 0);

	// Verify no excluded module appears in any candidate list
	for (const auto& Pair : Rules->CompiledData.MaskToCandidates)
	{
		for (int32 CandIdx : Pair.Value)
		{
			TestTrue(*FString::Printf(TEXT("Candidate %d is not excluded"), CandIdx),
				!Rules->CompiledData.IsModuleExcluded(CandIdx));
		}
	}

	return true;
}

// =============================================================================
// Boundary + wildcard masks on every orbital
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressBoundaryWildcardTest,
	"PCGEx.Stress.Valency.Assembler.BoundaryWildcardPerOrbital",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressBoundaryWildcardTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumOrbitals = 16;
	const TArray<FName> Names = MakeOrbitalNames(NumOrbitals);

	FPCGExBondingRulesAssembler Assembler;

	// Module A: boundary on even orbitals
	FPCGExAssemblerModuleDesc DescA;
	DescA.Asset = MakeFakeAsset(0);
	DescA.OrbitalMask = -1LL; // All bits set (all 16 orbitals)
	const int32 IdxA = Assembler.AddModule(DescA);
	for (int32 o = 0; o < NumOrbitals; o += 2) { Assembler.SetBoundaryOrbital(IdxA, o); }

	// Module B: wildcard on odd orbitals
	FPCGExAssemblerModuleDesc DescB;
	DescB.Asset = MakeFakeAsset(1);
	DescB.OrbitalMask = -1LL;
	const int32 IdxB = Assembler.AddModule(DescB);
	for (int32 o = 1; o < NumOrbitals; o += 2) { Assembler.SetWildcardOrbital(IdxB, o); }

	UPCGExValencyBondingRules* Rules = BuildRules(Assembler, Names);

	TestTrue(TEXT("Compiles with complex boundary/wildcard"), Rules->IsCompiled());

	// Verify boundary mask for module A: even bits set
	int64 ExpectedBoundaryA = 0;
	for (int32 o = 0; o < NumOrbitals; o += 2) { ExpectedBoundaryA |= (1LL << o); }
	TestEqual(TEXT("Module A boundary mask"), Rules->CompiledData.ModuleBoundaryMasks[0], ExpectedBoundaryA);

	// Verify wildcard mask for module B: odd bits set
	int64 ExpectedWildcardB = 0;
	for (int32 o = 1; o < NumOrbitals; o += 2) { ExpectedWildcardB |= (1LL << o); }
	TestEqual(TEXT("Module B wildcard mask"), Rules->CompiledData.ModuleWildcardMasks[1], ExpectedWildcardB);

	return true;
}

// =============================================================================
// Tags at scale — 100 modules with many tags each
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressManyTagsTest,
	"PCGEx.Stress.Valency.Assembler.ManyTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressManyTagsTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumModules = 100;
	constexpr int32 TagsPerModule = 20;
	const TArray<FName> Names = MakeOrbitalNames(2);

	FPCGExBondingRulesAssembler Assembler;
	PopulateModules(Assembler, NumModules);

	for (int32 m = 0; m < NumModules; ++m)
	{
		for (int32 t = 0; t < TagsPerModule; ++t)
		{
			Assembler.AddTag(m, FName(*FString::Printf(TEXT("Tag_%d_%d"), m, t)));
		}
	}

	UPCGExValencyBondingRules* Rules = BuildRules(Assembler, Names);

	TestTrue(TEXT("100-module with tags compiles"), Rules->IsCompiled());
	TestEqual(TEXT("ModuleTags array sized"), Rules->CompiledData.ModuleTags.Num(), NumModules);

	return true;
}

// =============================================================================
// EntryData — Pack/unpack correctness at volume
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressEntryDataVolumeTest,
	"PCGEx.Stress.Valency.EntryData.PackUnpackVolume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressEntryDataVolumeTest::RunTest(const FString& Parameters)
{
	using namespace PCGExValency::EntryData;

	constexpr int32 Iterations = 10000;
	int32 Failures = 0;

	for (int32 i = 0; i < Iterations; ++i)
	{
		const uint32 RulesId = static_cast<uint32>(i * 7 + 3);
		const uint16 ModuleIdx = static_cast<uint16>(i % 65535);
		const uint8 Flags = static_cast<uint8>(i % 256);

		const uint64 Packed = Pack(RulesId, ModuleIdx, Flags);

		if (GetBondingRulesMapId(Packed) != RulesId) { ++Failures; }
		if (GetModuleIndex(Packed) != static_cast<int32>(ModuleIdx)) { ++Failures; }
		if (GetPatternFlags(Packed) != Flags) { ++Failures; }
	}

	TestEqual(TEXT("All 10K pack/unpack roundtrips succeeded"), Failures, 0);

	return true;
}

// =============================================================================
// EntryData — Flag operations at volume
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressEntryDataFlagsVolumeTest,
	"PCGEx.Stress.Valency.EntryData.FlagOpsVolume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressEntryDataFlagsVolumeTest::RunTest(const FString& Parameters)
{
	using namespace PCGExValency::EntryData;

	constexpr int32 Iterations = 5000;
	int32 Failures = 0;

	for (int32 i = 0; i < Iterations; ++i)
	{
		const uint32 RulesId = static_cast<uint32>(i + 1);
		const uint16 ModuleIdx = static_cast<uint16>(i % 1000);

		uint64 Hash = Pack(RulesId, ModuleIdx, 0);

		// Set all 8 flag bits one at a time
		for (uint8 Bit = 0; Bit < 8; ++Bit)
		{
			Hash = SetFlag(Hash, 1 << Bit);
		}

		// All bits should be set
		if (GetPatternFlags(Hash) != 0xFF) { ++Failures; }

		// RulesId and ModuleIdx should be preserved
		if (GetBondingRulesMapId(Hash) != RulesId) { ++Failures; }
		if (GetModuleIndex(Hash) != static_cast<int32>(ModuleIdx)) { ++Failures; }

		// Clear all bits one at a time
		for (uint8 Bit = 0; Bit < 8; ++Bit)
		{
			Hash = ClearFlag(Hash, 1 << Bit);
		}

		if (GetPatternFlags(Hash) != 0) { ++Failures; }
	}

	TestEqual(TEXT("All 5K flag operation cycles succeeded"), Failures, 0);

	return true;
}

// =============================================================================
// EdgeOrbital — Pack/unpack correctness at volume
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressEdgePackVolumeTest,
	"PCGEx.Stress.Valency.EdgePack.Volume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressEdgePackVolumeTest::RunTest(const FString& Parameters)
{
	int32 Failures = 0;

	// All valid orbital index combinations (0-254, 255 = sentinel)
	for (uint8 Start = 0; Start < 255; ++Start)
	{
		for (uint8 End = 0; End < 255; End += 17) // Step by 17 for reasonable coverage
		{
			const uint16 Packed = PCGExValency::EdgeOrbital::Pack(Start, End);
			if (PCGExValency::EdgeOrbital::GetStartOrbital(Packed) != Start) { ++Failures; }
			if (PCGExValency::EdgeOrbital::GetEndOrbital(Packed) != End) { ++Failures; }
		}
	}

	TestEqual(TEXT("All orbital pack/unpack roundtrips succeeded"), Failures, 0);

	// Connector pack — int32 range, packed into int64 via H64
	int32 ConnectorFailures = 0;
	for (int32 Src = 0; Src < 1000; Src += 7)
	{
		for (int32 Tgt = 0; Tgt < 1000; Tgt += 11)
		{
			const int64 Packed = PCGExValency::EdgeConnector::Pack(Src, Tgt);
			if (PCGExValency::EdgeConnector::GetSourceIndex(Packed) != Src) { ++ConnectorFailures; }
			if (PCGExValency::EdgeConnector::GetTargetIndex(Packed) != Tgt) { ++ConnectorFailures; }
		}
	}

	TestEqual(TEXT("All connector pack/unpack roundtrips succeeded"), ConnectorFailures, 0);

	return true;
}

// =============================================================================
// Pattern Assets — Large two-store merge
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressPatternMergeTest,
	"PCGEx.Stress.Valency.PatternAsset.LargeTwoStoreMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressPatternMergeTest::RunTest(const FString& Parameters)
{
	constexpr int32 AuthoredCount = 50;
	constexpr int32 BuilderCount = 100;

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		MakeOrbitalNames(4));
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	// Add authored patterns and compile
	for (int32 i = 0; i < AuthoredCount; ++i)
	{
		FPCGExPatternAuthored Pattern;
		Pattern.PatternName = FName(*FString::Printf(TEXT("Auth%d"), i));
		Pattern.bExclusive = (i % 2 == 0);

		FPCGExPatternEntryAuthored Entry;
		Entry.bIsActive = true;
		Pattern.Entries.Add(Entry);

		Asset->Patterns.Add(Pattern);
	}
	Asset->Compile();

	// Add builder patterns
	for (int32 i = 0; i < BuilderCount; ++i)
	{
		const bool bExclusive = (i % 3 == 0);
		Asset->AppendCompiledPattern(
			MakeCompiledPattern(FName(*FString::Printf(TEXT("Build%d"), i)), 3),
			bExclusive);
	}

	// Merge
	FPCGExValencyPatternSetCompiled Merged;
	const bool bResult = Asset->GetAsOrbitalPatterns(Merged);

	TestTrue(TEXT("Large merge succeeds"), bResult);
	TestEqual(TEXT("Merged count = authored + builder"),
		Merged.GetPatternCount(), AuthoredCount + BuilderCount);

	// Verify exclusive/additive split in builder store
	const auto& BuilderPatterns = Asset->GetBuilderCagePatterns();
	TestEqual(TEXT("Builder pattern count"), BuilderPatterns.GetPatternCount(), BuilderCount);

	int32 ExpectedExclusive = 0;
	int32 ExpectedAdditive = 0;
	for (int32 i = 0; i < BuilderCount; ++i)
	{
		if (i % 3 == 0) { ++ExpectedExclusive; }
		else { ++ExpectedAdditive; }
	}
	TestEqual(TEXT("Builder exclusive count"), BuilderPatterns.ExclusivePatternIndices.Num(), ExpectedExclusive);
	TestEqual(TEXT("Builder additive count"), BuilderPatterns.AdditivePatternIndices.Num(), ExpectedAdditive);

	return true;
}

// =============================================================================
// Pattern Assets — Repeated clear + append cycles
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressPatternClearAppendCyclesTest,
	"PCGEx.Stress.Valency.PatternAsset.ClearAppendCycles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressPatternClearAppendCyclesTest::RunTest(const FString& Parameters)
{
	constexpr int32 Cycles = 50;
	constexpr int32 PatternsPerCycle = 20;

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		MakeOrbitalNames(4));
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	for (int32 c = 0; c < Cycles; ++c)
	{
		Asset->ClearCompiledPatterns();

		for (int32 p = 0; p < PatternsPerCycle; ++p)
		{
			Asset->AppendCompiledPattern(
				MakeCompiledPattern(FName(*FString::Printf(TEXT("C%d_P%d"), c, p)), 2),
				p % 2 == 0);
		}

		const auto& Builder = Asset->GetBuilderCagePatterns();
		TestEqual(*FString::Printf(TEXT("Cycle %d: correct count"), c),
			Builder.GetPatternCount(), PatternsPerCycle);
	}

	// After all cycles, final state should have exactly PatternsPerCycle
	TestEqual(TEXT("Final builder count"),
		Asset->GetBuilderCagePatterns().GetPatternCount(), PatternsPerCycle);

	return true;
}

// =============================================================================
// Assembler — Interleaved unique + duplicate additions
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressAssemblerInterleavedDedupTest,
	"PCGEx.Stress.Valency.Assembler.InterleavedDedup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressAssemblerInterleavedDedupTest::RunTest(const FString& Parameters)
{
	constexpr int32 UniqueCount = 40;

	FPCGExBondingRulesAssembler Assembler;

	// Interleave: add unique 0, dup 0, unique 1, dup 0, dup 1, unique 2, ...
	TArray<int32> ExpectedIndices;
	ExpectedIndices.Reserve(UniqueCount);

	for (int32 i = 0; i < UniqueCount; ++i)
	{
		FPCGExAssemblerModuleDesc Desc;
		Desc.Asset = MakeFakeAsset(i);
		Desc.OrbitalMask = 1LL << (i % 64);
		const int32 Idx = Assembler.AddModule(Desc);
		ExpectedIndices.Add(Idx);

		// Re-add all previous modules as duplicates
		for (int32 j = 0; j <= i; ++j)
		{
			FPCGExAssemblerModuleDesc DupDesc;
			DupDesc.Asset = MakeFakeAsset(j);
			DupDesc.OrbitalMask = 1LL << (j % 64);
			const int32 DupIdx = Assembler.AddModule(DupDesc);
			TestEqual(*FString::Printf(TEXT("Dup of module %d returns original index"), j),
				DupIdx, ExpectedIndices[j]);
		}
	}

	TestEqual(TEXT("Final module count = unique count"),
		Assembler.GetModuleCount(), UniqueCount);

	return true;
}

// =============================================================================
// BondingRules compile — all array sizes consistent for many modules
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressArrayConsistencyTest,
	"PCGEx.Stress.Valency.Compile.ArraySizeConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressArrayConsistencyTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumModules = 64;
	const TArray<FName> Names = MakeOrbitalNames(8);

	FPCGExBondingRulesAssembler Assembler;
	PopulateModules(Assembler, NumModules);

	// Add varying transforms and tags
	for (int32 m = 0; m < NumModules; ++m)
	{
		const int32 TransformCount = (m % 5) + 1;
		for (int32 t = 0; t < TransformCount; ++t)
		{
			Assembler.AddLocalTransform(m, FTransform(FVector(m, t, 0)));
		}
		Assembler.AddTag(m, FName(*FString::Printf(TEXT("Group%d"), m % 10)));
	}

	UPCGExValencyBondingRules* Rules = BuildRules(Assembler, Names);
	const auto& CD = Rules->CompiledData;

	// All per-module arrays must be exactly ModuleCount in size
	TestEqual(TEXT("ModuleWeights"), CD.ModuleWeights.Num(), NumModules);
	TestEqual(TEXT("ModuleOrbitalMasks"), CD.ModuleOrbitalMasks.Num(), NumModules);
	TestEqual(TEXT("ModuleBoundaryMasks"), CD.ModuleBoundaryMasks.Num(), NumModules);
	TestEqual(TEXT("ModuleWildcardMasks"), CD.ModuleWildcardMasks.Num(), NumModules);
	TestEqual(TEXT("ModuleNames"), CD.ModuleNames.Num(), NumModules);
	TestEqual(TEXT("ModulePlacementPolicies"), CD.ModulePlacementPolicies.Num(), NumModules);
	TestEqual(TEXT("ModuleLocalTransformHeaders"), CD.ModuleLocalTransformHeaders.Num(), NumModules);
	TestEqual(TEXT("ModuleTags"), CD.ModuleTags.Num(), NumModules);
	TestEqual(TEXT("ModuleBehaviorFlags"), CD.ModuleBehaviorFlags.Num(), NumModules);
	TestEqual(TEXT("ModuleBoundsModifiers"), CD.ModuleBoundsModifiers.Num(), NumModules);

	// Flattened transform count should match sum of per-module transforms
	int32 ExpectedTotalTransforms = 0;
	for (int32 m = 0; m < NumModules; ++m)
	{
		ExpectedTotalTransforms += (m % 5) + 1;
	}
	TestEqual(TEXT("Total flattened transforms"),
		CD.AllLocalTransforms.Num(), ExpectedTotalTransforms);

	return true;
}

// =============================================================================
// MakeModuleKey — Determinism under volume (10K calls)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressModuleKeyDeterminismTest,
	"PCGEx.Stress.Valency.ModuleKey.DeterminismVolume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressModuleKeyDeterminismTest::RunTest(const FString& Parameters)
{
	constexpr int32 Iterations = 10000;
	int32 Mismatches = 0;

	for (int32 i = 0; i < Iterations; ++i)
	{
		const FSoftObjectPath Path(FString::Printf(TEXT("/Game/Stress/Asset%d.Asset%d"), i, i));
		const int64 Mask = static_cast<int64>(i) * 17 + 3;

		const FString Key1 = PCGExValency::MakeModuleKey(Path, Mask);
		const FString Key2 = PCGExValency::MakeModuleKey(Path, Mask);

		if (Key1 != Key2) { ++Mismatches; }
	}

	TestEqual(TEXT("10K MakeModuleKey calls all deterministic"), Mismatches, 0);

	return true;
}

// =============================================================================
// MakeModuleKey — Uniqueness under volume (1K distinct inputs)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressModuleKeyUniquenessTest,
	"PCGEx.Stress.Valency.ModuleKey.UniquenessVolume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressModuleKeyUniquenessTest::RunTest(const FString& Parameters)
{
	constexpr int32 Count = 1000;
	TSet<FString> Keys;
	Keys.Reserve(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		const FSoftObjectPath Path(FString::Printf(TEXT("/Game/Stress/U%d.U%d"), i, i));
		Keys.Add(PCGExValency::MakeModuleKey(Path, i));
	}

	TestEqual(TEXT("1K distinct inputs → 1K unique keys"), Keys.Num(), Count);

	return true;
}

// =============================================================================
// Compile + BuildCandidateLookup — overlapping multi-bit masks
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressOverlappingMasksTest,
	"PCGEx.Stress.Valency.Compile.OverlappingMasks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressOverlappingMasksTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumOrbitals = 8;
	const TArray<FName> Names = MakeOrbitalNames(NumOrbitals);

	FPCGExBondingRulesAssembler Assembler;

	// Create modules with overlapping multi-bit masks
	// Module 0: bits 0-3, Module 1: bits 2-5, Module 2: bits 4-7, etc.
	constexpr int32 NumModules = 20;
	for (int32 i = 0; i < NumModules; ++i)
	{
		FPCGExAssemblerModuleDesc Desc;
		Desc.Asset = MakeFakeAsset(i);
		// Sliding window of 4 bits
		Desc.OrbitalMask = 0xFLL << (i % 5);
		Assembler.AddModule(Desc);
	}

	UPCGExValencyBondingRules* Rules = BuildRules(Assembler, Names);

	TestTrue(TEXT("Overlapping masks compile"), Rules->IsCompiled());

	Rules->CompiledData.BuildCandidateLookup();

	// Each mask key in the lookup should have at least 1 candidate
	for (const auto& Pair : Rules->CompiledData.MaskToCandidates)
	{
		TestTrue(*FString::Printf(TEXT("Mask 0x%llX has candidates"), Pair.Key),
			Pair.Value.Num() > 0);
	}

	return true;
}

// =============================================================================
// Full pipeline — Assembler → Compile → BuildLookup → Pattern merge
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyStressFullPipelineTest,
	"PCGEx.Stress.Valency.Pipeline.AssemblerToPatternMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyStressFullPipelineTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumOrbitals = 16;
	constexpr int32 NumModules = 48;
	constexpr int32 NumBuilderPatterns = 30;
	constexpr int32 NumAuthoredPatterns = 15;
	const TArray<FName> Names = MakeOrbitalNames(NumOrbitals);

	// --- Stage 1: Assembler ---

	FPCGExBondingRulesAssembler Assembler;
	for (int32 i = 0; i < NumModules; ++i)
	{
		FPCGExAssemblerModuleDesc Desc;
		Desc.Asset = MakeFakeAsset(i);
		Desc.OrbitalMask = (1LL << (i % NumOrbitals)) | (1LL << ((i + 1) % NumOrbitals));
		Desc.ModuleName = FName(*FString::Printf(TEXT("Mod%d"), i));
		Desc.Settings.Weight = FMath::RandRange(0.5f, 5.0f);

		if (i % 5 == 0) { Desc.PlacementPolicy = EPCGExModulePlacementPolicy::Filler; }
		else if (i % 7 == 0) { Desc.PlacementPolicy = EPCGExModulePlacementPolicy::Excluded; }

		Assembler.AddModule(Desc);
	}

	// Add neighbor topology: ring connectivity
	for (int32 m = 0; m < NumModules; ++m)
	{
		const int32 Next = (m + 1) % NumModules;
		const int32 Prev = (m + NumModules - 1) % NumModules;
		Assembler.AddNeighbors(m, Names[m % NumOrbitals], {Prev, Next});
	}

	// Add transforms
	for (int32 m = 0; m < NumModules; ++m)
	{
		for (int32 t = 0; t < 3; ++t)
		{
			Assembler.AddLocalTransform(m, FTransform(FVector(m * 100, t * 50, 0)));
		}
	}

	// --- Stage 2: Compile ---

	UPCGExValencyBondingRules* Rules = BuildRules(Assembler, Names);

	TestTrue(TEXT("Pipeline: compile succeeded"), Rules->IsCompiled());
	TestEqual(TEXT("Pipeline: module count"), Rules->CompiledData.ModuleCount, NumModules);

	// --- Stage 3: Candidate lookup ---

	Rules->CompiledData.BuildCandidateLookup();
	TestTrue(TEXT("Pipeline: candidate lookup populated"),
		Rules->CompiledData.MaskToCandidates.Num() > 0);

	// --- Stage 4: Pattern asset with two stores ---

	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(Names);
	UPCGExCagePatternAsset* PatternAsset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	// Authored patterns
	for (int32 i = 0; i < NumAuthoredPatterns; ++i)
	{
		FPCGExPatternAuthored Pattern;
		Pattern.PatternName = FName(*FString::Printf(TEXT("Authored%d"), i));
		Pattern.bExclusive = (i % 2 == 0);

		FPCGExPatternEntryAuthored Entry;
		Entry.bIsActive = true;
		Pattern.Entries.Add(Entry);
		PatternAsset->Patterns.Add(Pattern);
	}
	PatternAsset->Compile();

	// Builder patterns
	for (int32 i = 0; i < NumBuilderPatterns; ++i)
	{
		PatternAsset->AppendCompiledPattern(
			MakeCompiledPattern(FName(*FString::Printf(TEXT("Builder%d"), i)), 4),
			i % 3 == 0);
	}

	// --- Stage 5: Merge ---

	FPCGExValencyPatternSetCompiled Merged;
	const bool bMerge = PatternAsset->GetAsOrbitalPatterns(Merged);

	TestTrue(TEXT("Pipeline: pattern merge succeeded"), bMerge);
	TestEqual(TEXT("Pipeline: merged pattern count"),
		Merged.GetPatternCount(), NumAuthoredPatterns + NumBuilderPatterns);

	TestTrue(TEXT("Pipeline: has patterns"), Merged.HasPatterns());

	return true;
}
