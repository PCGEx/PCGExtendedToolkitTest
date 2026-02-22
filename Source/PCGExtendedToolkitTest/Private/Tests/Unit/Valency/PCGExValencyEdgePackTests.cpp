// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Valency Edge Pack Unit Tests
 *
 * Tests EdgeOrbital and EdgeConnector pack/unpack from PCGExValencyCommon.h:
 * - EdgeOrbital: Pack/GetStartOrbital/GetEndOrbital, NoMatchSentinel
 * - EdgeConnector: Pack/GetSourceIndex/GetTargetIndex
 *
 * Test naming convention: PCGEx.Unit.Valency.EdgePack.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExValencyCommon.h"

using namespace PCGExValency;

// =============================================================================
// EdgeOrbital Roundtrip
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEdgeOrbitalRoundtripTest,
	"PCGEx.Unit.Valency.EdgePack.OrbitalRoundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEdgeOrbitalRoundtripTest::RunTest(const FString& Parameters)
{
	const int64 Packed = EdgeOrbital::Pack(10, 20);

	TestEqual(TEXT("StartOrbital roundtrips"), EdgeOrbital::GetStartOrbital(Packed), static_cast<uint8>(10));
	TestEqual(TEXT("EndOrbital roundtrips"), EdgeOrbital::GetEndOrbital(Packed), static_cast<uint8>(20));

	return true;
}

// =============================================================================
// EdgeOrbital NoMatchSentinel
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEdgeOrbitalNoMatchSentinelTest,
	"PCGEx.Unit.Valency.EdgePack.OrbitalNoMatchSentinel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEdgeOrbitalNoMatchSentinelTest::RunTest(const FString& Parameters)
{
	const int64 Sentinel = EdgeOrbital::NoMatchSentinel();

	TestEqual(TEXT("Sentinel start is NO_MATCH"), EdgeOrbital::GetStartOrbital(Sentinel), EdgeOrbital::NO_MATCH);
	TestEqual(TEXT("Sentinel end is NO_MATCH"), EdgeOrbital::GetEndOrbital(Sentinel), EdgeOrbital::NO_MATCH);

	return true;
}

// =============================================================================
// EdgeOrbital Zero (distinct from sentinel)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEdgeOrbitalZeroTest,
	"PCGEx.Unit.Valency.EdgePack.OrbitalZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEdgeOrbitalZeroTest::RunTest(const FString& Parameters)
{
	const int64 Packed = EdgeOrbital::Pack(0, 0);

	TestEqual(TEXT("Zero start roundtrips"), EdgeOrbital::GetStartOrbital(Packed), static_cast<uint8>(0));
	TestEqual(TEXT("Zero end roundtrips"), EdgeOrbital::GetEndOrbital(Packed), static_cast<uint8>(0));

	// Must be distinct from sentinel
	TestNotEqual(TEXT("Zero pack differs from sentinel"), Packed, EdgeOrbital::NoMatchSentinel());

	return true;
}

// =============================================================================
// EdgeOrbital Max (254 -- 255 is sentinel)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEdgeOrbitalMaxTest,
	"PCGEx.Unit.Valency.EdgePack.OrbitalMax",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEdgeOrbitalMaxTest::RunTest(const FString& Parameters)
{
	const int64 Packed = EdgeOrbital::Pack(254, 254);

	TestEqual(TEXT("Max start roundtrips"), EdgeOrbital::GetStartOrbital(Packed), static_cast<uint8>(254));
	TestEqual(TEXT("Max end roundtrips"), EdgeOrbital::GetEndOrbital(Packed), static_cast<uint8>(254));

	// Must be distinct from sentinel
	TestNotEqual(TEXT("Max pack differs from sentinel"), Packed, EdgeOrbital::NoMatchSentinel());

	return true;
}

// =============================================================================
// EdgeConnector Roundtrip
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEdgeConnectorRoundtripTest,
	"PCGEx.Unit.Valency.EdgePack.ConnectorRoundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEdgeConnectorRoundtripTest::RunTest(const FString& Parameters)
{
	const int64 Packed = EdgeConnector::Pack(100, 200);

	TestEqual(TEXT("SourceIndex roundtrips"), EdgeConnector::GetSourceIndex(Packed), 100);
	TestEqual(TEXT("TargetIndex roundtrips"), EdgeConnector::GetTargetIndex(Packed), 200);

	return true;
}

// =============================================================================
// EdgeConnector Zero
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExValencyEdgeConnectorZeroTest,
	"PCGEx.Unit.Valency.EdgePack.ConnectorZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExValencyEdgeConnectorZeroTest::RunTest(const FString& Parameters)
{
	const int64 Packed = EdgeConnector::Pack(0, 0);

	TestEqual(TEXT("Zero source roundtrips"), EdgeConnector::GetSourceIndex(Packed), 0);
	TestEqual(TEXT("Zero target roundtrips"), EdgeConnector::GetTargetIndex(Packed), 0);

	return true;
}
