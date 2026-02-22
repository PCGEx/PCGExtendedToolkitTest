// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Pattern Compile Unit Tests
 *
 * Tests UPCGExCagePatternAsset::Compile() -- name→index resolution for orbital patterns.
 * Creates authored patterns with named orbitals, compiles, and verifies the compiled output.
 *
 * Test naming convention: PCGEx.Unit.Valency.PatternCompile.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExCagePatternAsset.h"
#include "Core/PCGExValencyPatternAsset.h"
#include "Helpers/PCGExValencyTestHelpers.h"

// =============================================================================
// Compile Empty -- no authored patterns
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternCompileEmptyTest,
	"PCGEx.Unit.Valency.PatternCompile.CompileEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternCompileEmptyTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	// No patterns added
	TArray<FText> Errors;
	const bool bResult = Asset->Compile(&Errors);

	// Empty patterns should succeed (nothing to compile)
	TestTrue(TEXT("Compile empty succeeds"), bResult);
	TestEqual(TEXT("No errors"), Errors.Num(), 0);

	return true;
}

// =============================================================================
// Compile requires OrbitalSet
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternCompileNoOrbitalSetTest,
	"PCGEx.Unit.Valency.PatternCompile.CompileNoOrbitalSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternCompileNoOrbitalSetTest::RunTest(const FString& Parameters)
{
	UPCGExCagePatternAsset* Asset = NewObject<UPCGExCagePatternAsset>(GetTransientPackage());
	// No OrbitalSet assigned

	// Add a pattern so compile actually tries
	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("TestPattern");
	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;
	Pattern.Entries.Add(Entry);
	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	const bool bResult = Asset->Compile(&Errors);

	TestFalse(TEXT("Compile without OrbitalSet fails"), bResult);
	TestTrue(TEXT("Has errors"), Errors.Num() > 0);

	return true;
}

// =============================================================================
// Compile Single Entry
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternCompileSingleEntryTest,
	"PCGEx.Unit.Valency.PatternCompile.CompileSingleEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternCompileSingleEntryTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S"), FName("E"), FName("W")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("SingleEntry");
	Pattern.Weight = 2.0f;
	Pattern.MinMatches = 1;
	Pattern.MaxMatches = 5;

	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;
	Pattern.Entries.Add(Entry);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	const bool bResult = Asset->Compile(&Errors);

	TestTrue(TEXT("Compile succeeds"), bResult);

	// Verify compiled output
	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);

	TestTrue(TEXT("Has compiled patterns"), OutPatterns.HasPatterns());
	TestEqual(TEXT("1 compiled pattern"), OutPatterns.GetPatternCount(), 1);

	const auto& Compiled = OutPatterns.Patterns[0];
	TestEqual(TEXT("Pattern name"), Compiled.Settings.PatternName, FName("SingleEntry"));
	TestEqual(TEXT("Pattern weight"), Compiled.Settings.Weight, 2.0f);
	TestEqual(TEXT("Pattern MinMatches"), Compiled.Settings.MinMatches, 1);
	TestEqual(TEXT("Pattern MaxMatches"), Compiled.Settings.MaxMatches, 5);
	TestEqual(TEXT("1 entry compiled"), Compiled.GetEntryCount(), 1);

	return true;
}

// =============================================================================
// Compile Boundary Mask
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternCompileBoundaryMaskTest,
	"PCGEx.Unit.Valency.PatternCompile.CompileBoundaryMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternCompileBoundaryMaskTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S"), FName("E"), FName("W")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("BoundaryTest");

	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;
	Entry.BoundaryNames.Add(FName("N"));  // N is orbital index 0
	Pattern.Entries.Add(Entry);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);

	if (OutPatterns.HasPatterns() && OutPatterns.Patterns[0].Entries.Num() > 0)
	{
		// N is index 0 → bit 0 should be set in BoundaryOrbitalMask
		TestTrue(TEXT("Boundary bit 0 set for orbital N"),
			(OutPatterns.Patterns[0].Entries[0].BoundaryOrbitalMask & (1ULL << 0)) != 0);
	}

	return true;
}

// =============================================================================
// Compile Wildcard Mask
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternCompileWildcardMaskTest,
	"PCGEx.Unit.Valency.PatternCompile.CompileWildcardMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternCompileWildcardMaskTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S"), FName("E"), FName("W")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("WildcardTest");

	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;
	Entry.WildcardNames.Add(FName("S"));  // S is orbital index 1
	Pattern.Entries.Add(Entry);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);

	if (OutPatterns.HasPatterns() && OutPatterns.Patterns[0].Entries.Num() > 0)
	{
		// S is index 1 → bit 1 should be set in WildcardOrbitalMask
		TestTrue(TEXT("Wildcard bit 1 set for orbital S"),
			(OutPatterns.Patterns[0].Entries[0].WildcardOrbitalMask & (1ULL << 1)) != 0);
	}

	return true;
}

// =============================================================================
// Compile Adjacency -- SourceName/TargetName → orbital indices
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternCompileAdjacencyTest,
	"PCGEx.Unit.Valency.PatternCompile.CompileAdjacency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternCompileAdjacencyTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S"), FName("E"), FName("W")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("AdjacencyTest");

	// Entry 0
	FPCGExPatternEntryAuthored Entry0;
	Entry0.bIsActive = true;
	{
		FPCGExPatternAdjacencyAuthored Adj;
		Adj.TargetEntryIndex = 1;
		FPCGExPatternIndexPairAuthored Pair;
		Pair.SourceName = FName("N");  // orbital 0
		Pair.TargetName = FName("S");  // orbital 1
		Adj.IndexPairs.Add(Pair);
		Entry0.Adjacencies.Add(Adj);
	}
	Pattern.Entries.Add(Entry0);

	// Entry 1
	FPCGExPatternEntryAuthored Entry1;
	Entry1.bIsActive = true;
	Pattern.Entries.Add(Entry1);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);

	if (OutPatterns.HasPatterns() && OutPatterns.Patterns[0].Entries.Num() >= 2)
	{
		const auto& Entry = OutPatterns.Patterns[0].Entries[0];
		TestTrue(TEXT("Entry 0 has adjacency"), Entry.Adjacency.Num() > 0);

		if (Entry.Adjacency.Num() > 0)
		{
			// FIntVector: X=TargetEntryIndex, Y=SourceOrbital, Z=TargetOrbital
			TestEqual(TEXT("Adjacency target entry"), Entry.Adjacency[0].X, 1);
			TestEqual(TEXT("Source orbital (N=0)"), Entry.Adjacency[0].Y, 0);
			TestEqual(TEXT("Target orbital (S=1)"), Entry.Adjacency[0].Z, 1);
		}
	}

	return true;
}

// =============================================================================
// Compile Exclusive Classification
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternCompileExclusiveTest,
	"PCGEx.Unit.Valency.PatternCompile.CompileExclusiveClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternCompileExclusiveTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("ExclusiveTest");
	Pattern.bExclusive = true;

	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;
	Pattern.Entries.Add(Entry);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);

	TestTrue(TEXT("Has exclusive patterns"), OutPatterns.ExclusivePatternIndices.Num() > 0);
	TestEqual(TEXT("No additive patterns"), OutPatterns.AdditivePatternIndices.Num(), 0);

	return true;
}

// =============================================================================
// Compile Additive Classification
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternCompileAdditiveTest,
	"PCGEx.Unit.Valency.PatternCompile.CompileAdditiveClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternCompileAdditiveTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("AdditiveTest");
	Pattern.bExclusive = false;

	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;
	Pattern.Entries.Add(Entry);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);

	TestEqual(TEXT("No exclusive patterns"), OutPatterns.ExclusivePatternIndices.Num(), 0);
	TestTrue(TEXT("Has additive patterns"), OutPatterns.AdditivePatternIndices.Num() > 0);

	return true;
}

// =============================================================================
// Compile Settings Transfer
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternCompileSettingsTest,
	"PCGEx.Unit.Valency.PatternCompile.CompileSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternCompileSettingsTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("SettingsTest");
	Pattern.Weight = 5.0f;
	Pattern.MinMatches = 3;
	Pattern.MaxMatches = 7;
	Pattern.bExclusive = true;
	Pattern.OutputStrategy = EPCGExPatternOutputStrategy::Collapse;
	Pattern.TransformMode = EPCGExPatternTransformMode::PatternRoot;

	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;
	Pattern.Entries.Add(Entry);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);

	if (OutPatterns.HasPatterns())
	{
		const auto& Settings = OutPatterns.Patterns[0].Settings;
		TestEqual(TEXT("PatternName"), Settings.PatternName, FName("SettingsTest"));
		TestEqual(TEXT("Weight"), Settings.Weight, 5.0f);
		TestEqual(TEXT("MinMatches"), Settings.MinMatches, 3);
		TestEqual(TEXT("MaxMatches"), Settings.MaxMatches, 7);
		TestEqual(TEXT("bExclusive"), Settings.bExclusive, true);
		TestEqual(TEXT("OutputStrategy"), Settings.OutputStrategy, EPCGExPatternOutputStrategy::Collapse);
		TestEqual(TEXT("TransformMode"), Settings.TransformMode, EPCGExPatternTransformMode::PatternRoot);
	}

	return true;
}

// =============================================================================
// Compile Active Entry Count
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternCompileActiveEntryCountTest,
	"PCGEx.Unit.Valency.PatternCompile.CompileActiveEntryCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternCompileActiveEntryCountTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("ActiveCountTest");

	FPCGExPatternEntryAuthored ActiveEntry;
	ActiveEntry.bIsActive = true;
	Pattern.Entries.Add(ActiveEntry);

	FPCGExPatternEntryAuthored InactiveEntry;
	InactiveEntry.bIsActive = false;
	Pattern.Entries.Add(InactiveEntry);

	FPCGExPatternEntryAuthored ActiveEntry2;
	ActiveEntry2.bIsActive = true;
	Pattern.Entries.Add(ActiveEntry2);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);

	if (OutPatterns.HasPatterns())
	{
		TestEqual(TEXT("ActiveEntryCount = 2 (2 active, 1 inactive)"),
			OutPatterns.Patterns[0].ActiveEntryCount, 2);
	}

	return true;
}

// =============================================================================
// Empty Entry Rejected
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternCompileEmptyEntryRejectedTest,
	"PCGEx.Unit.Valency.PatternCompile.EmptyEntryRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternCompileEmptyEntryRejectedTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("EmptyPattern");
	// No entries -- this pattern should be rejected

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);

	// Empty-entry pattern should be skipped
	TestFalse(TEXT("Empty-entry pattern not in compiled output"), OutPatterns.HasPatterns());

	return true;
}
