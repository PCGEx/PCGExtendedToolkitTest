// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Pattern Asset Integration Tests
 *
 * Tests the two-store architecture on UPCGExCagePatternAsset:
 * - Compile() writes AuthoredCompiledPatterns, builder writes BuilderCompiledPatterns
 * - GetAsOrbitalPatterns merges both stores
 * - ClearCompiledPatterns only affects builder store
 * - IsCompiled() checks either store
 *
 * Test naming convention: PCGEx.Integration.Valency.PatternAsset.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExCagePatternAsset.h"
#include "Core/PCGExValencyPatternAsset.h"
#include "Helpers/PCGExValencyTestHelpers.h"

namespace
{
	/** Helper: create a minimal authored pattern for compile tests */
	FPCGExPatternAuthored MakeSimpleAuthoredPattern(FName PatternName, bool bExclusive = true)
	{
		FPCGExPatternAuthored Pattern;
		Pattern.PatternName = PatternName;
		Pattern.bExclusive = bExclusive;

		FPCGExPatternEntryAuthored Entry;
		Entry.bIsActive = true;
		Pattern.Entries.Add(Entry);

		return Pattern;
	}

	/** Helper: create a minimal compiled pattern for builder tests */
	FPCGExValencyPatternCompiled MakeSimpleCompiledPattern(FName PatternName)
	{
		FPCGExValencyPatternCompiled Pattern;
		Pattern.Settings.PatternName = PatternName;
		Pattern.ActiveEntryCount = 1;

		FPCGExValencyPatternEntryCompiled Entry;
		Entry.bIsActive = true;
		Pattern.Entries.Add(Entry);

		return Pattern;
	}
}

// =============================================================================
// Two-Store Independence
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternAssetTwoStoreIndependenceTest,
	"PCGEx.Integration.Valency.PatternAsset.TwoStoreIndependence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternAssetTwoStoreIndependenceTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	// Step 1: Add authored pattern and compile
	Asset->Patterns.Add(MakeSimpleAuthoredPattern(FName("AuthoredA")));
	Asset->Compile();

	// Step 2: Add builder pattern
	Asset->AppendCompiledPattern(MakeSimpleCompiledPattern(FName("BuilderA")), true);

	// Verify independence: builder store should NOT contain authored data
	const auto& BuilderPatterns = Asset->GetBuilderCagePatterns();
	TestEqual(TEXT("Builder store has 1 pattern"), BuilderPatterns.GetPatternCount(), 1);
	TestEqual(TEXT("Builder pattern name is BuilderA"),
		BuilderPatterns.Patterns[0].Settings.PatternName, FName("BuilderA"));

	return true;
}

// =============================================================================
// GetAsOrbitalPatterns Merges Both Stores
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternAssetMergeTest,
	"PCGEx.Integration.Valency.PatternAsset.GetAsOrbitalPatternsMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternAssetMergeTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	// Authored: 1 pattern
	Asset->Patterns.Add(MakeSimpleAuthoredPattern(FName("AuthoredA")));
	Asset->Compile();

	// Builder: 1 pattern
	Asset->AppendCompiledPattern(MakeSimpleCompiledPattern(FName("BuilderA")), true);

	// Merge
	FPCGExValencyPatternSetCompiled Merged;
	const bool bResult = Asset->GetAsOrbitalPatterns(Merged);

	TestTrue(TEXT("GetAsOrbitalPatterns succeeds"), bResult);
	TestEqual(TEXT("Merged has 2 patterns"), Merged.GetPatternCount(), 2);

	return true;
}

// =============================================================================
// Builder Append — Exclusive vs Additive
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternAssetBuilderAppendTest,
	"PCGEx.Integration.Valency.PatternAsset.BuilderAppendExclusiveAdditive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternAssetBuilderAppendTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	Asset->ClearCompiledPatterns();
	Asset->AppendCompiledPattern(MakeSimpleCompiledPattern(FName("ExclusiveP")), true);
	Asset->AppendCompiledPattern(MakeSimpleCompiledPattern(FName("AdditiveP")), false);

	const auto& BuilderPatterns = Asset->GetBuilderCagePatterns();
	TestEqual(TEXT("Builder has 2 patterns"), BuilderPatterns.GetPatternCount(), 2);
	TestEqual(TEXT("1 exclusive index"), BuilderPatterns.ExclusivePatternIndices.Num(), 1);
	TestEqual(TEXT("1 additive index"), BuilderPatterns.AdditivePatternIndices.Num(), 1);

	return true;
}

// =============================================================================
// Clear Only Affects Builder Store
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternAssetClearOnlyBuilderTest,
	"PCGEx.Integration.Valency.PatternAsset.ClearOnlyAffectsBuilder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternAssetClearOnlyBuilderTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	// Authored: compile 1 pattern
	Asset->Patterns.Add(MakeSimpleAuthoredPattern(FName("AuthoredA")));
	Asset->Compile();

	// Builder: add and then clear
	Asset->AppendCompiledPattern(MakeSimpleCompiledPattern(FName("BuilderA")), true);
	Asset->ClearCompiledPatterns();

	// Builder store should be empty
	TestFalse(TEXT("Builder store empty after clear"),
		Asset->GetBuilderCagePatterns().HasPatterns());

	// But authored should survive — verify via GetAsOrbitalPatterns
	FPCGExValencyPatternSetCompiled Merged;
	Asset->GetAsOrbitalPatterns(Merged);
	TestTrue(TEXT("Authored patterns survive clear"), Merged.HasPatterns());

	return true;
}

// =============================================================================
// IsCompiled — Either Store
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternAssetIsCompiledEitherStoreTest,
	"PCGEx.Integration.Valency.PatternAsset.IsCompiledEitherStore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternAssetIsCompiledEitherStoreTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});

	// Test 1: Only builder patterns
	{
		UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);
		Asset->AppendCompiledPattern(MakeSimpleCompiledPattern(FName("BuilderOnly")), true);
		TestTrue(TEXT("IsCompiled with builder-only"), Asset->IsCompiled());
	}

	// Test 2: Only authored patterns
	{
		UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);
		Asset->Patterns.Add(MakeSimpleAuthoredPattern(FName("AuthoredOnly")));
		Asset->Compile();
		TestTrue(TEXT("IsCompiled with authored-only"), Asset->IsCompiled());
	}

	return true;
}

// =============================================================================
// Empty IsNotCompiled
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternAssetEmptyIsNotCompiledTest,
	"PCGEx.Integration.Valency.PatternAsset.EmptyIsNotCompiled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternAssetEmptyIsNotCompiledTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	TestFalse(TEXT("Fresh asset is not compiled"), Asset->IsCompiled());

	return true;
}

// =============================================================================
// Authored Survives Clear
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternAssetAuthoredSurvivesClearTest,
	"PCGEx.Integration.Valency.PatternAsset.AuthoredSurvivesClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternAssetAuthoredSurvivesClearTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	// Compile authored patterns
	Asset->Patterns.Add(MakeSimpleAuthoredPattern(FName("AuthA")));
	Asset->Patterns.Add(MakeSimpleAuthoredPattern(FName("AuthB")));
	Asset->Compile();

	// Clear builder store
	Asset->ClearCompiledPatterns();

	// IsCompiled should still be true (authored patterns exist)
	TestTrue(TEXT("IsCompiled after clearing builder"), Asset->IsCompiled());

	FPCGExValencyPatternSetCompiled Merged;
	Asset->GetAsOrbitalPatterns(Merged);
	TestEqual(TEXT("2 authored patterns still present"), Merged.GetPatternCount(), 2);

	return true;
}

// =============================================================================
// Merged Count Correct
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyPatternAssetMergedCountCorrectTest,
	"PCGEx.Integration.Valency.PatternAsset.MergedCountCorrect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyPatternAssetMergedCountCorrectTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	// 2 authored patterns
	Asset->Patterns.Add(MakeSimpleAuthoredPattern(FName("AuthA")));
	Asset->Patterns.Add(MakeSimpleAuthoredPattern(FName("AuthB")));
	Asset->Compile();

	// 3 builder patterns
	Asset->AppendCompiledPattern(MakeSimpleCompiledPattern(FName("BuildA")), true);
	Asset->AppendCompiledPattern(MakeSimpleCompiledPattern(FName("BuildB")), false);
	Asset->AppendCompiledPattern(MakeSimpleCompiledPattern(FName("BuildC")), true);

	FPCGExValencyPatternSetCompiled Merged;
	Asset->GetAsOrbitalPatterns(Merged);

	TestEqual(TEXT("2 authored + 3 builder = 5 total"), Merged.GetPatternCount(), 5);

	return true;
}
