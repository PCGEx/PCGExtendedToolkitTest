// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Module Key Unit Tests
 *
 * Tests PCGExValency::MakeModuleKey() from PCGExValencyCommon.h:
 * - Determinism (same inputs → same key)
 * - Differentiation by asset path, orbital mask, material variant
 *
 * Test naming convention: PCGEx.Unit.Valency.ModuleKey.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExValencyCommon.h"

// =============================================================================
// Determinism -- Same inputs produce same key
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyModuleKeyDeterminismTest,
	"PCGEx.Unit.Valency.ModuleKey.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyModuleKeyDeterminismTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath Path(TEXT("/Game/Meshes/Cube.Cube"));
	const int64 Mask = 0b1010;

	const FString Key1 = PCGExValency::MakeModuleKey(Path, Mask);
	const FString Key2 = PCGExValency::MakeModuleKey(Path, Mask);

	TestEqual(TEXT("Same inputs produce same key"), Key1, Key2);
	TestTrue(TEXT("Key is non-empty"), Key1.Len() > 0);

	return true;
}

// =============================================================================
// Different asset paths → different keys
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyModuleKeyAssetPathDiffTest,
	"PCGEx.Unit.Valency.ModuleKey.AssetPathDiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyModuleKeyAssetPathDiffTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath PathA(TEXT("/Game/Meshes/Cube.Cube"));
	const FSoftObjectPath PathB(TEXT("/Game/Meshes/Sphere.Sphere"));
	const int64 Mask = 0b1111;

	const FString KeyA = PCGExValency::MakeModuleKey(PathA, Mask);
	const FString KeyB = PCGExValency::MakeModuleKey(PathB, Mask);

	TestNotEqual(TEXT("Different paths produce different keys"), KeyA, KeyB);

	return true;
}

// =============================================================================
// Same asset, different orbital masks → different keys
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyModuleKeyMaskDiffTest,
	"PCGEx.Unit.Valency.ModuleKey.MaskDiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyModuleKeyMaskDiffTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath Path(TEXT("/Game/Meshes/Cube.Cube"));

	const FString KeyA = PCGExValency::MakeModuleKey(Path, 0b0011);
	const FString KeyB = PCGExValency::MakeModuleKey(Path, 0b1100);

	TestNotEqual(TEXT("Different masks produce different keys"), KeyA, KeyB);

	return true;
}

// =============================================================================
// Same asset+mask, different material variants → different keys
// MakeModuleKey uses SlotIndex + material path hash.
// TSoftObjectPtr::IsValid() requires the asset to be loaded, so unloaded
// material paths hash to 0. We differentiate by SlotIndex instead.
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyModuleKeyMaterialVariantDiffTest,
	"PCGEx.Unit.Valency.ModuleKey.MaterialVariantDiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyModuleKeyMaterialVariantDiffTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath Path(TEXT("/Game/Meshes/Cube.Cube"));
	const int64 Mask = 0b1111;

	// Variant A: override on slot 0
	FPCGExValencyMaterialVariant VariantA;
	{
		FPCGExValencyMaterialOverride Override;
		Override.SlotIndex = 0;
		VariantA.Overrides.Add(Override);
	}

	// Variant B: override on slot 1 (different slot → different key)
	FPCGExValencyMaterialVariant VariantB;
	{
		FPCGExValencyMaterialOverride Override;
		Override.SlotIndex = 1;
		VariantB.Overrides.Add(Override);
	}

	const FString KeyA = PCGExValency::MakeModuleKey(Path, Mask, &VariantA);
	const FString KeyB = PCGExValency::MakeModuleKey(Path, Mask, &VariantB);

	TestNotEqual(TEXT("Different slot indices produce different keys"), KeyA, KeyB);

	// Also verify: variant with overrides differs from no-variant
	const FString KeyNone = PCGExValency::MakeModuleKey(Path, Mask, nullptr);
	TestNotEqual(TEXT("Variant with overrides differs from nullptr"), KeyA, KeyNone);

	return true;
}

// =============================================================================
// nullptr vs empty material variant → same key
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyModuleKeyNullVariantVsEmptyTest,
	"PCGEx.Unit.Valency.ModuleKey.NullVariantVsEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyModuleKeyNullVariantVsEmptyTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath Path(TEXT("/Game/Meshes/Cube.Cube"));
	const int64 Mask = 0b1111;

	FPCGExValencyMaterialVariant EmptyVariant;
	// No overrides -- should behave same as nullptr

	const FString KeyNull = PCGExValency::MakeModuleKey(Path, Mask, nullptr);
	const FString KeyEmpty = PCGExValency::MakeModuleKey(Path, Mask, &EmptyVariant);

	TestEqual(TEXT("nullptr and empty variant produce same key"), KeyNull, KeyEmpty);

	return true;
}

// =============================================================================
// Zero mask produces valid key
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyModuleKeyZeroMaskTest,
	"PCGEx.Unit.Valency.ModuleKey.ZeroMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyModuleKeyZeroMaskTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath Path(TEXT("/Game/Meshes/Cube.Cube"));

	const FString Key = PCGExValency::MakeModuleKey(Path, 0);

	TestTrue(TEXT("Zero mask produces non-empty key"), Key.Len() > 0);

	return true;
}

// =============================================================================
// Max mask produces valid key
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyModuleKeyMaxMaskTest,
	"PCGEx.Unit.Valency.ModuleKey.MaxMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyModuleKeyMaxMaskTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath Path(TEXT("/Game/Meshes/Cube.Cube"));

	const FString Key = PCGExValency::MakeModuleKey(Path, INT64_MAX);

	TestTrue(TEXT("INT64_MAX mask produces non-empty key"), Key.Len() > 0);

	// Should differ from zero mask
	const FString KeyZero = PCGExValency::MakeModuleKey(Path, 0);
	TestNotEqual(TEXT("Max mask differs from zero mask"), Key, KeyZero);

	return true;
}
