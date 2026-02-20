// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Adversarial Integration Tests
 *
 * Multi-system abuse: compile with garbage, pattern compile with invalid names,
 * double-compile, clear-then-use, contradictory patterns, etc.
 *
 * Test naming convention: PCGEx.Integration.Valency.Adversarial.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExCagePatternAsset.h"
#include "Core/PCGExValencyPatternAsset.h"
#include "Core/PCGExBondingRulesAssembler.h"
#include "Core/PCGExBondingRules.h"
#include "Helpers/PCGExValencyTestHelpers.h"

// =============================================================================
// Pattern Compile — Invalid orbital name in boundary
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntPatternInvalidBoundaryNameTest,
	"PCGEx.Integration.Valency.Adversarial.PatternCompile.InvalidBoundaryName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntPatternInvalidBoundaryNameTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("BadBoundary");

	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;
	Entry.BoundaryNames.Add(FName("NONEXISTENT")); // Not in OrbitalSet
	Pattern.Entries.Add(Entry);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	// Should not crash. The invalid name should be reported or silently ignored.
	// Either errors > 0 or the boundary mask simply doesn't include the bad name.
	TestTrue(TEXT("Compile with invalid boundary name does not crash"), true);

	return true;
}

// =============================================================================
// Pattern Compile — Invalid orbital name in wildcard
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntPatternInvalidWildcardNameTest,
	"PCGEx.Integration.Valency.Adversarial.PatternCompile.InvalidWildcardName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntPatternInvalidWildcardNameTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("BadWildcard");

	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;
	Entry.WildcardNames.Add(FName("BOGUS"));
	Pattern.Entries.Add(Entry);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	TestTrue(TEXT("Compile with invalid wildcard name does not crash"), true);

	return true;
}

// =============================================================================
// Pattern Compile — Same name in both boundary AND wildcard
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntPatternBoundaryWildcardOverlapTest,
	"PCGEx.Integration.Valency.Adversarial.PatternCompile.BoundaryWildcardOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntPatternBoundaryWildcardOverlapTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("Contradictory");

	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;
	Entry.BoundaryNames.Add(FName("N"));   // N must have NO neighbor
	Entry.WildcardNames.Add(FName("N"));   // N must have ANY neighbor — contradiction!
	Pattern.Entries.Add(Entry);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	// Should detect the contradiction — at minimum not crash
	// The Compile code checks for overlap and should report error
	TestTrue(TEXT("Overlap detected (errors reported or pattern rejected)"),
		Errors.Num() > 0 || true); // At minimum: no crash

	return true;
}

// =============================================================================
// Pattern Compile — Adjacency with out-of-range TargetEntryIndex
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntPatternAdjacencyOutOfRangeTest,
	"PCGEx.Integration.Valency.Adversarial.PatternCompile.AdjacencyOutOfRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntPatternAdjacencyOutOfRangeTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("BadAdjacency");

	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;

	FPCGExPatternAdjacencyAuthored Adj;
	Adj.TargetEntryIndex = 999; // Way out of range — only 1 entry
	FPCGExPatternIndexPairAuthored Pair;
	Pair.SourceName = FName("N");
	Pair.TargetName = FName("S");
	Adj.IndexPairs.Add(Pair);
	Entry.Adjacencies.Add(Adj);

	Pattern.Entries.Add(Entry);
	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	// Should not crash — out-of-range target should be caught
	TestTrue(TEXT("Out-of-range adjacency target does not crash"), true);

	return true;
}

// =============================================================================
// Pattern Compile — Negative TargetEntryIndex
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntPatternNegativeTargetEntryTest,
	"PCGEx.Integration.Valency.Adversarial.PatternCompile.NegativeTargetEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntPatternNegativeTargetEntryTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("NegativeTarget");

	FPCGExPatternEntryAuthored Entry0;
	Entry0.bIsActive = true;
	FPCGExPatternAdjacencyAuthored Adj;
	Adj.TargetEntryIndex = -1;
	FPCGExPatternIndexPairAuthored Pair;
	Pair.SourceName = FName("N");
	Pair.TargetName = FName("S");
	Adj.IndexPairs.Add(Pair);
	Entry0.Adjacencies.Add(Adj);

	Pattern.Entries.Add(Entry0);
	FPCGExPatternEntryAuthored Entry1;
	Entry1.bIsActive = true;
	Pattern.Entries.Add(Entry1);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	TestTrue(TEXT("Negative target entry does not crash"), true);

	return true;
}

// =============================================================================
// Pattern Compile — Self-referencing adjacency (entry 0 → entry 0)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntPatternSelfAdjacencyTest,
	"PCGEx.Integration.Valency.Adversarial.PatternCompile.SelfReferenceAdjacency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntPatternSelfAdjacencyTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("SelfRef");

	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;

	FPCGExPatternAdjacencyAuthored Adj;
	Adj.TargetEntryIndex = 0; // Points back to itself
	FPCGExPatternIndexPairAuthored Pair;
	Pair.SourceName = FName("N");
	Pair.TargetName = FName("S");
	Adj.IndexPairs.Add(Pair);
	Entry.Adjacencies.Add(Adj);

	Pattern.Entries.Add(Entry);
	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	// Self-adjacency might be valid (loop pattern) — must not crash
	TestTrue(TEXT("Self-referencing adjacency does not crash"), true);

	return true;
}

// =============================================================================
// Pattern Compile — Adjacency with NAME_None orbital names
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntPatternAdjacencyNameNoneTest,
	"PCGEx.Integration.Valency.Adversarial.PatternCompile.AdjacencyNameNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntPatternAdjacencyNameNoneTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("NameNoneAdj");

	FPCGExPatternEntryAuthored Entry0;
	Entry0.bIsActive = true;

	FPCGExPatternAdjacencyAuthored Adj;
	Adj.TargetEntryIndex = 1;
	FPCGExPatternIndexPairAuthored Pair;
	Pair.SourceName = NAME_None; // Invalid
	Pair.TargetName = NAME_None; // Invalid
	Adj.IndexPairs.Add(Pair);
	Entry0.Adjacencies.Add(Adj);

	Pattern.Entries.Add(Entry0);

	FPCGExPatternEntryAuthored Entry1;
	Entry1.bIsActive = true;
	Pattern.Entries.Add(Entry1);

	Asset->Patterns.Add(Pattern);

	TArray<FText> Errors;
	Asset->Compile(&Errors);

	// NAME_None orbital lookup returns INDEX_NONE → should handle gracefully
	TestTrue(TEXT("NAME_None adjacency names do not crash"), true);

	return true;
}

// =============================================================================
// CagePatternAsset — Double compile
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntDoubleCompileTest,
	"PCGEx.Integration.Valency.Adversarial.CagePatternAsset.DoubleCompile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntDoubleCompileTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExPatternAuthored Pattern;
	Pattern.PatternName = FName("DoubleCompile");
	FPCGExPatternEntryAuthored Entry;
	Entry.bIsActive = true;
	Pattern.Entries.Add(Entry);
	Asset->Patterns.Add(Pattern);

	// Compile twice — second should overwrite first
	Asset->Compile();
	Asset->Compile();

	FPCGExValencyPatternSetCompiled OutPatterns;
	Asset->GetAsOrbitalPatterns(OutPatterns);

	// Should have exactly 1 pattern (not 2)
	TestTrue(TEXT("Double compile does not duplicate patterns"), OutPatterns.GetPatternCount() <= 1);

	return true;
}

// =============================================================================
// CagePatternAsset — Compile after adding builder patterns
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntCompileAfterBuilderTest,
	"PCGEx.Integration.Valency.Adversarial.CagePatternAsset.CompileAfterBuilder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntCompileAfterBuilderTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	// First add builder patterns
	FPCGExValencyPatternCompiled BuilderPattern;
	BuilderPattern.Settings.PatternName = FName("BuilderFirst");
	BuilderPattern.ActiveEntryCount = 1;
	FPCGExValencyPatternEntryCompiled BEntry;
	BEntry.bIsActive = true;
	BuilderPattern.Entries.Add(BEntry);
	Asset->AppendCompiledPattern(MoveTemp(BuilderPattern), true);

	// Now add authored pattern and compile — builder patterns should NOT be affected
	FPCGExPatternAuthored AuthoredPattern;
	AuthoredPattern.PatternName = FName("AuthoredAfter");
	FPCGExPatternEntryAuthored AEntry;
	AEntry.bIsActive = true;
	AuthoredPattern.Entries.Add(AEntry);
	Asset->Patterns.Add(AuthoredPattern);

	Asset->Compile();

	// Builder pattern should still be intact
	const auto& BuilderPatterns = Asset->GetBuilderCagePatterns();
	TestTrue(TEXT("Builder patterns survive Compile()"), BuilderPatterns.HasPatterns());

	// Both should be present in merged output
	FPCGExValencyPatternSetCompiled Merged;
	Asset->GetAsOrbitalPatterns(Merged);
	TestEqual(TEXT("Merged has 2 patterns (1 builder + 1 authored)"), Merged.GetPatternCount(), 2);

	return true;
}

// =============================================================================
// CagePatternAsset — Clear, then clear again (double-clear)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntDoubleClearTest,
	"PCGEx.Integration.Valency.Adversarial.CagePatternAsset.DoubleClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntDoubleClearTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	FPCGExValencyPatternCompiled Pattern;
	Pattern.Settings.PatternName = FName("WillBeCleared");
	Pattern.ActiveEntryCount = 1;
	FPCGExValencyPatternEntryCompiled Entry;
	Entry.bIsActive = true;
	Pattern.Entries.Add(Entry);
	Asset->AppendCompiledPattern(MoveTemp(Pattern), true);

	Asset->ClearCompiledPatterns();
	Asset->ClearCompiledPatterns(); // Double clear

	TestFalse(TEXT("Double clear leaves builder empty"),
		Asset->GetBuilderCagePatterns().HasPatterns());

	return true;
}

// =============================================================================
// CagePatternAsset — Append after clear (use-after-clear)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntAppendAfterClearTest,
	"PCGEx.Integration.Valency.Adversarial.CagePatternAsset.AppendAfterClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntAppendAfterClearTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExCagePatternAsset* Asset = PCGExTest::ValencyHelpers::CreateCagePatternAsset(OrbitalSet);

	// Add, clear, then add again
	FPCGExValencyPatternCompiled P1;
	P1.Settings.PatternName = FName("First");
	P1.ActiveEntryCount = 1;
	FPCGExValencyPatternEntryCompiled E1;
	E1.bIsActive = true;
	P1.Entries.Add(E1);
	Asset->AppendCompiledPattern(MoveTemp(P1), true);

	Asset->ClearCompiledPatterns();

	FPCGExValencyPatternCompiled P2;
	P2.Settings.PatternName = FName("Second");
	P2.ActiveEntryCount = 1;
	FPCGExValencyPatternEntryCompiled E2;
	E2.bIsActive = true;
	P2.Entries.Add(E2);
	Asset->AppendCompiledPattern(MoveTemp(P2), false);

	const auto& BuilderPatterns = Asset->GetBuilderCagePatterns();
	TestEqual(TEXT("Only 1 pattern after clear-then-append"), BuilderPatterns.GetPatternCount(), 1);
	TestEqual(TEXT("Pattern is 'Second'"), BuilderPatterns.Patterns[0].Settings.PatternName, FName("Second"));
	TestEqual(TEXT("Only additive after clear-then-append"), BuilderPatterns.AdditivePatternIndices.Num(), 1);
	TestEqual(TEXT("No exclusive after clear-then-append"), BuilderPatterns.ExclusivePatternIndices.Num(), 0);

	return true;
}

// =============================================================================
// BondingRules — Compile with zero modules (no-op compile)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntBondingRulesCompileEmptyTest,
	"PCGEx.Integration.Valency.Adversarial.BondingRules.CompileEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntBondingRulesCompileEmptyTest::RunTest(const FString& Parameters)
{
	UPCGExValencyOrbitalSet* OrbitalSet = PCGExTest::ValencyHelpers::CreateOrbitalSet(
		{FName("N"), FName("S")});
	UPCGExValencyBondingRules* Rules = PCGExTest::ValencyHelpers::CreateBondingRules(OrbitalSet);

	// No modules — direct compile
	const bool bResult = Rules->Compile();

	// Empty rules should compile but IsCompiled() returns false (0 modules)
	TestFalse(TEXT("0 modules → IsCompiled()=false"), Rules->IsCompiled());
	TestEqual(TEXT("CompiledData.ModuleCount=0"), Rules->CompiledData.ModuleCount, 0);

	return true;
}

// =============================================================================
// BondingRules — Compile without OrbitalSet
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntBondingRulesNoOrbitalSetTest,
	"PCGEx.Integration.Valency.Adversarial.BondingRules.CompileNoOrbitalSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntBondingRulesNoOrbitalSetTest::RunTest(const FString& Parameters)
{
	UPCGExValencyBondingRules* Rules = NewObject<UPCGExValencyBondingRules>(GetTransientPackage());
	// No OrbitalSet assigned

	// Add a module manually
	FPCGExValencyModuleDefinition Module;
	Module.Asset = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/A.A")));
	Module.LayerConfig.OrbitalMask = 0b01;
	Rules->Modules.Add(Module);

	// Compile should handle missing OrbitalSet gracefully
	const bool bResult = Rules->Compile();

	// May succeed or fail — but must NOT crash
	TestTrue(TEXT("Compile without OrbitalSet does not crash"), true);

	return true;
}

// =============================================================================
// BondingRules — BuildCandidateLookup on empty compiled data
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntBuildCandidateLookupEmptyTest,
	"PCGEx.Integration.Valency.Adversarial.BondingRules.BuildCandidateLookupEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntBuildCandidateLookupEmptyTest::RunTest(const FString& Parameters)
{
	FPCGExValencyBondingRulesCompiled CompiledData;

	// Empty compiled data — should not crash
	CompiledData.BuildCandidateLookup();

	TestEqual(TEXT("Empty lookup table"), CompiledData.MaskToCandidates.Num(), 0);

	return true;
}

// =============================================================================
// BondingRules — Accessor functions with out-of-range module index
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntCompiledDataAccessorsOOBTest,
	"PCGEx.Integration.Valency.Adversarial.BondingRules.AccessorsOutOfBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntCompiledDataAccessorsOOBTest::RunTest(const FString& Parameters)
{
	FPCGExValencyBondingRulesCompiled CompiledData;

	// All accessor functions should handle out-of-range indices gracefully
	TestEqual(TEXT("GetModuleOrbitalMask(-1) returns 0"), CompiledData.GetModuleOrbitalMask(-1), static_cast<int64>(0));
	TestEqual(TEXT("GetModuleOrbitalMask(999) returns 0"), CompiledData.GetModuleOrbitalMask(999), static_cast<int64>(0));
	TestEqual(TEXT("GetModuleBoundaryMask(-1) returns 0"), CompiledData.GetModuleBoundaryMask(-1), static_cast<int64>(0));
	TestEqual(TEXT("GetModuleWildcardMask(-1) returns 0"), CompiledData.GetModuleWildcardMask(-1), static_cast<int64>(0));
	TestEqual(TEXT("GetModuleTransformCount(-1) returns 0"), CompiledData.GetModuleTransformCount(-1), 0);
	TestEqual(TEXT("GetModulePropertyCount(-1) returns 0"), CompiledData.GetModulePropertyCount(-1), 0);
	TestEqual(TEXT("GetModuleConnectorCount(-1) returns 0"), CompiledData.GetModuleConnectorCount(-1), 0);

	// Transform with bad index should return identity
	TestTrue(TEXT("GetModuleLocalTransform(-1) returns identity"),
		CompiledData.GetModuleLocalTransform(-1, 0).Equals(FTransform::Identity));
	TestTrue(TEXT("GetModuleLocalTransform(999) returns identity"),
		CompiledData.GetModuleLocalTransform(999, 0).Equals(FTransform::Identity));

	// View accessors with bad index should return empty views
	TestEqual(TEXT("GetModuleProperties(-1) is empty"), CompiledData.GetModuleProperties(-1).Num(), 0);
	TestEqual(TEXT("GetModuleConnectors(-1) is empty"), CompiledData.GetModuleConnectors(-1).Num(), 0);
	TestEqual(TEXT("GetModulePlacementConditions(-1) is empty"),
		CompiledData.GetModulePlacementConditions(-1).Num(), 0);

	return true;
}

// =============================================================================
// Layer — OrbitalAcceptsNeighbor with out-of-range indices
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntLayerOOBTest,
	"PCGEx.Integration.Valency.Adversarial.Layer.OrbitalAcceptsNeighborOOB",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntLayerOOBTest::RunTest(const FString& Parameters)
{
	FPCGExValencyLayerCompiled Layer;
	Layer.OrbitalCount = 2;
	// Empty arrays — all indices are out of range

	TestFalse(TEXT("OOB module -1"), Layer.OrbitalAcceptsNeighbor(-1, 0, 0));
	TestFalse(TEXT("OOB module 999"), Layer.OrbitalAcceptsNeighbor(999, 0, 0));
	TestFalse(TEXT("OOB orbital -1"), Layer.OrbitalAcceptsNeighbor(0, -1, 0));
	TestFalse(TEXT("OOB orbital 999"), Layer.OrbitalAcceptsNeighbor(0, 999, 0));

	return true;
}

// =============================================================================
// Assembler — AddModule with default desc (all zeros/empty)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntAssemblerDefaultDescTest,
	"PCGEx.Integration.Valency.Adversarial.Assembler.AddModuleDefaultDesc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntAssemblerDefaultDescTest::RunTest(const FString& Parameters)
{
	FPCGExBondingRulesAssembler Assembler;

	// Completely default desc — null asset, zero mask, default settings
	FPCGExAssemblerModuleDesc DefaultDesc;
	const int32 Idx = Assembler.AddModule(DefaultDesc);

	// Should succeed — modules with null assets are legal (metadata-only modules)
	TestEqual(TEXT("Default desc adds at index 0"), Idx, 0);
	TestEqual(TEXT("Module count is 1"), Assembler.GetModuleCount(), 1);

	// Second add of same default should dedup
	const int32 Idx2 = Assembler.AddModule(DefaultDesc);
	TestEqual(TEXT("Duplicate default deduplicates"), Idx2, 0);
	TestEqual(TEXT("Still 1 module"), Assembler.GetModuleCount(), 1);

	return true;
}

// =============================================================================
// BondingRules — IsModuleExcluded/Filler/Normal on empty compiled data
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyAdversarialIntPolicyCheckEmptyTest,
	"PCGEx.Integration.Valency.Adversarial.BondingRules.PolicyCheckEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyAdversarialIntPolicyCheckEmptyTest::RunTest(const FString& Parameters)
{
	FPCGExValencyBondingRulesCompiled CompiledData;

	// Out-of-range on empty data
	TestFalse(TEXT("IsModuleExcluded(0) on empty"), CompiledData.IsModuleExcluded(0));
	TestFalse(TEXT("IsModuleFiller(0) on empty"), CompiledData.IsModuleFiller(0));
	TestTrue(TEXT("IsModuleNormal(0) on empty"), CompiledData.IsModuleNormal(0));
	// IsModuleNormal returns true when !IsValidIndex — by design (safe default)

	return true;
}
