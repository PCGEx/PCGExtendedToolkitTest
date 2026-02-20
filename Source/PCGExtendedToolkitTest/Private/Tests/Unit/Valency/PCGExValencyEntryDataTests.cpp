// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency EntryData Unit Tests
 *
 * Tests PCGExValency::EntryData namespace from PCGExValencyMap.h:
 * - Pack/Unpack roundtrip
 * - INVALID_ENTRY sentinel handling
 * - Boundary values
 * - Flag manipulation (Set/Clear/Has)
 *
 * Test naming convention: PCGEx.Unit.Valency.EntryData.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExValencyMap.h"

using namespace PCGExValency::EntryData;

// =============================================================================
// Pack/Unpack Roundtrip
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEntryDataPackUnpackRoundtripTest,
	"PCGEx.Unit.Valency.EntryData.PackUnpackRoundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEntryDataPackUnpackRoundtripTest::RunTest(const FString& Parameters)
{
	const uint64 Hash = Pack(42, 7, Flags::Consumed);

	TestTrue(TEXT("Packed hash is valid"), IsValid(Hash));
	TestEqual(TEXT("BondingRulesMapId roundtrips"), GetBondingRulesMapId(Hash), 42u);
	TestEqual(TEXT("ModuleIndex roundtrips"), GetModuleIndex(Hash), 7);
	TestEqual(TEXT("PatternFlags roundtrips"), GetPatternFlags(Hash), Flags::Consumed);

	return true;
}

// =============================================================================
// INVALID_ENTRY Sentinel
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEntryDataInvalidEntryTest,
	"PCGEx.Unit.Valency.EntryData.InvalidEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEntryDataInvalidEntryTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("INVALID_ENTRY is not valid"), IsValid(INVALID_ENTRY));
	TestEqual(TEXT("GetModuleIndex on INVALID returns -1"), GetModuleIndex(INVALID_ENTRY), -1);
	TestEqual(TEXT("GetBondingRulesMapId on INVALID returns MAX_uint32"), GetBondingRulesMapId(INVALID_ENTRY), MAX_uint32);
	TestEqual(TEXT("GetPatternFlags on INVALID returns None"), GetPatternFlags(INVALID_ENTRY), Flags::None);

	return true;
}

// =============================================================================
// Boundary Values
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEntryDataBoundaryValuesTest,
	"PCGEx.Unit.Valency.EntryData.BoundaryValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEntryDataBoundaryValuesTest::RunTest(const FString& Parameters)
{
	// Max uint16 module index
	{
		const uint16 MaxModule = MAX_uint16;
		const uint64 Hash = Pack(1, MaxModule, 0);
		TestTrue(TEXT("Max module hash is valid"), IsValid(Hash));
		TestEqual(TEXT("Max uint16 module index roundtrips"), GetModuleIndex(Hash), static_cast<int32>(MaxModule));
	}

	// Large rules ID
	{
		const uint32 LargeId = 100000;
		const uint64 Hash = Pack(LargeId, 0, 0);
		TestTrue(TEXT("Large rules ID hash is valid"), IsValid(Hash));
		TestEqual(TEXT("Large rules ID roundtrips"), GetBondingRulesMapId(Hash), LargeId);
	}

	// All flags set
	{
		const uint16 AllFlags = Flags::Consumed | Flags::Swapped | Flags::Collapsed | Flags::Annotated;
		const uint64 Hash = Pack(1, 1, AllFlags);
		TestEqual(TEXT("All flags roundtrip"), GetPatternFlags(Hash), AllFlags);
	}

	return true;
}

// =============================================================================
// SetFlag
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEntryDataSetFlagTest,
	"PCGEx.Unit.Valency.EntryData.SetFlag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEntryDataSetFlagTest::RunTest(const FString& Parameters)
{
	const uint64 Hash = Pack(10, 5, Flags::None);

	const uint64 WithSwapped = SetFlag(Hash, Flags::Swapped);
	TestTrue(TEXT("SetFlag produces valid hash"), IsValid(WithSwapped));
	TestTrue(TEXT("Swapped flag is set"), HasFlag(WithSwapped, Flags::Swapped));
	TestEqual(TEXT("BondingRulesMapId preserved"), GetBondingRulesMapId(WithSwapped), 10u);
	TestEqual(TEXT("ModuleIndex preserved"), GetModuleIndex(WithSwapped), 5);

	return true;
}

// =============================================================================
// ClearFlag
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEntryDataClearFlagTest,
	"PCGEx.Unit.Valency.EntryData.ClearFlag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEntryDataClearFlagTest::RunTest(const FString& Parameters)
{
	const uint64 Hash = Pack(10, 5, Flags::Consumed | Flags::Swapped);

	const uint64 Cleared = ClearFlag(Hash, Flags::Consumed);
	TestFalse(TEXT("Consumed flag is cleared"), HasFlag(Cleared, Flags::Consumed));
	TestTrue(TEXT("Swapped flag preserved"), HasFlag(Cleared, Flags::Swapped));
	TestEqual(TEXT("BondingRulesMapId preserved"), GetBondingRulesMapId(Cleared), 10u);
	TestEqual(TEXT("ModuleIndex preserved"), GetModuleIndex(Cleared), 5);

	return true;
}

// =============================================================================
// HasFlag
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEntryDataHasFlagTest,
	"PCGEx.Unit.Valency.EntryData.HasFlag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEntryDataHasFlagTest::RunTest(const FString& Parameters)
{
	const uint64 Hash = Pack(1, 1, Flags::Consumed);

	TestTrue(TEXT("HasFlag returns true for set flag"), HasFlag(Hash, Flags::Consumed));
	TestFalse(TEXT("HasFlag returns false for unset flag"), HasFlag(Hash, Flags::Swapped));
	TestFalse(TEXT("HasFlag returns false for unset Collapsed"), HasFlag(Hash, Flags::Collapsed));

	return true;
}

// =============================================================================
// SetPatternFlags — replaces all flags
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEntryDataSetPatternFlagsTest,
	"PCGEx.Unit.Valency.EntryData.SetPatternFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEntryDataSetPatternFlagsTest::RunTest(const FString& Parameters)
{
	const uint64 Hash = Pack(10, 5, Flags::Consumed | Flags::Swapped);

	const uint16 NewFlags = Flags::Collapsed | Flags::Annotated;
	const uint64 Updated = SetPatternFlags(Hash, NewFlags);

	TestEqual(TEXT("Flags fully replaced"), GetPatternFlags(Updated), NewFlags);
	TestFalse(TEXT("Old Consumed flag gone"), HasFlag(Updated, Flags::Consumed));
	TestFalse(TEXT("Old Swapped flag gone"), HasFlag(Updated, Flags::Swapped));
	TestTrue(TEXT("New Collapsed flag set"), HasFlag(Updated, Flags::Collapsed));
	TestTrue(TEXT("New Annotated flag set"), HasFlag(Updated, Flags::Annotated));
	TestEqual(TEXT("BondingRulesMapId preserved"), GetBondingRulesMapId(Updated), 10u);
	TestEqual(TEXT("ModuleIndex preserved"), GetModuleIndex(Updated), 5);

	return true;
}

// =============================================================================
// Flag operations on INVALID_ENTRY
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEntryDataFlagOnInvalidTest,
	"PCGEx.Unit.Valency.EntryData.FlagOnInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEntryDataFlagOnInvalidTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("SetFlag on INVALID returns INVALID"), SetFlag(INVALID_ENTRY, Flags::Consumed), INVALID_ENTRY);
	TestEqual(TEXT("ClearFlag on INVALID returns INVALID"), ClearFlag(INVALID_ENTRY, Flags::Consumed), INVALID_ENTRY);
	TestFalse(TEXT("HasFlag on INVALID returns false"), HasFlag(INVALID_ENTRY, Flags::Consumed));
	TestEqual(TEXT("SetPatternFlags on INVALID returns INVALID"), SetPatternFlags(INVALID_ENTRY, Flags::Consumed), INVALID_ENTRY);

	return true;
}

// =============================================================================
// Multiple flags set independently
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEntryDataMultipleFlagsTest,
	"PCGEx.Unit.Valency.EntryData.MultipleFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEntryDataMultipleFlagsTest::RunTest(const FString& Parameters)
{
	const uint64 Hash = Pack(1, 1, Flags::None);

	const uint64 WithConsumed = SetFlag(Hash, Flags::Consumed);
	const uint64 WithBoth = SetFlag(WithConsumed, Flags::Swapped);

	TestTrue(TEXT("Consumed still set after adding Swapped"), HasFlag(WithBoth, Flags::Consumed));
	TestTrue(TEXT("Swapped is set"), HasFlag(WithBoth, Flags::Swapped));

	const uint16 ExpectedFlags = Flags::Consumed | Flags::Swapped;
	TestEqual(TEXT("Both flags present in pattern flags"), GetPatternFlags(WithBoth), ExpectedFlags);

	return true;
}
