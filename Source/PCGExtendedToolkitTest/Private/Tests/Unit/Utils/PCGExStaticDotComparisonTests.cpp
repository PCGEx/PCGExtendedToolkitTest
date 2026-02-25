// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Static Dot Comparison Unit Tests
 *
 * Tests FPCGExStaticDotComparisonDetails Init() and Test() methods directly.
 * These tests exercise the actual struct, not a simulation.
 *
 * Key regression targets:
 * - Init() must compute ComparisonThreshold from DotConstant/DegreesConstant
 *   (not from DotTolerance/DegreesTolerance, which was the original bug)
 * - Test() must compare against ComparisonThreshold (not ComparisonTolerance)
 * - Signed remapping: (1+x)*0.5
 * - Unsigned remapping: FMath::Abs(x)
 * - Degrees conversion: DegreesToDot(180 - degrees)
 *
 * Test naming convention: PCGEx.Unit.Utils.StaticDotComparison.<Category>.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Utils/PCGExCompare.h"
#include "Math/PCGExMath.h"
#include "Helpers/PCGExTestHelpers.h"

// Helper to build and init a FPCGExStaticDotComparisonDetails with given parameters
namespace StaticDotTestHelpers
{
	FPCGExStaticDotComparisonDetails MakeScalar(
		double InDotConstant,
		EPCGExComparison InComparison,
		bool bInUnsigned = false,
		double InDotTolerance = 0.1)
	{
		FPCGExStaticDotComparisonDetails Details;
		Details.Domain = EPCGExAngularDomain::Scalar;
		Details.DotConstant = InDotConstant;
		Details.DotTolerance = InDotTolerance;
		Details.Comparison = InComparison;
		Details.bUnsignedComparison = bInUnsigned;
		Details.Init();
		return Details;
	}

	FPCGExStaticDotComparisonDetails MakeDegrees(
		double InDegreesConstant,
		EPCGExComparison InComparison,
		bool bInUnsigned = false,
		double InDegreesTolerance = 5.0)
	{
		FPCGExStaticDotComparisonDetails Details;
		Details.Domain = EPCGExAngularDomain::Degrees;
		Details.DegreesConstant = InDegreesConstant;
		Details.DegreesTolerance = InDegreesTolerance;
		Details.Comparison = InComparison;
		Details.bUnsignedComparison = bInUnsigned;
		Details.Init();
		return Details;
	}
}

// =============================================================================
// Init() - ComparisonThreshold Computation (Core Regression)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotInitThresholdScalarSignedTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Init.ThresholdScalarSigned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotInitThresholdScalarSignedTest::RunTest(const FString& Parameters)
{
	// Signed scalar: ComparisonThreshold = (1 + DotConstant) * 0.5

	{
		auto D = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrGreater);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, (1.0 + 0.5) * 0.5, 0.001, "DotConstant=0.5 -> threshold=0.75");
	}

	{
		auto D = StaticDotTestHelpers::MakeScalar(0.0, EPCGExComparison::EqualOrGreater);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 0.5, 0.001, "DotConstant=0.0 -> threshold=0.5");
	}

	{
		auto D = StaticDotTestHelpers::MakeScalar(1.0, EPCGExComparison::EqualOrGreater);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 1.0, 0.001, "DotConstant=1.0 -> threshold=1.0");
	}

	{
		auto D = StaticDotTestHelpers::MakeScalar(-1.0, EPCGExComparison::EqualOrGreater);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 0.0, 0.001, "DotConstant=-1.0 -> threshold=0.0");
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotInitThresholdScalarUnsignedTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Init.ThresholdScalarUnsigned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotInitThresholdScalarUnsignedTest::RunTest(const FString& Parameters)
{
	// Unsigned scalar: ComparisonThreshold = FMath::Abs(DotConstant)

	{
		auto D = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrGreater, true);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 0.5, 0.001, "unsigned DotConstant=0.5 -> threshold=0.5");
	}

	{
		auto D = StaticDotTestHelpers::MakeScalar(-0.5, EPCGExComparison::EqualOrGreater, true);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 0.5, 0.001, "unsigned DotConstant=-0.5 -> threshold=0.5 (abs)");
	}

	{
		auto D = StaticDotTestHelpers::MakeScalar(0.0, EPCGExComparison::EqualOrGreater, true);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 0.0, 0.001, "unsigned DotConstant=0.0 -> threshold=0.0");
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotInitThresholdDegreesSignedTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Init.ThresholdDegreesSigned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotInitThresholdDegreesSignedTest::RunTest(const FString& Parameters)
{
	// Signed degrees: ComparisonThreshold = (1 + DegreesToDot(180 - DegreesConstant)) * 0.5

	{
		// 90 deg -> DegreesToDot(180-90) = DegreesToDot(90) = cos(90) = 0 -> (1+0)*0.5 = 0.5
		auto D = StaticDotTestHelpers::MakeDegrees(90.0, EPCGExComparison::EqualOrGreater);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 0.5, 0.001, "DegreesConstant=90 -> threshold=0.5");
	}

	{
		// 0 deg -> DegreesToDot(180) = cos(180) = -1 -> (1+-1)*0.5 = 0.0
		auto D = StaticDotTestHelpers::MakeDegrees(0.0, EPCGExComparison::EqualOrGreater);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 0.0, 0.001, "DegreesConstant=0 -> threshold=0.0");
	}

	{
		// 180 deg -> DegreesToDot(0) = cos(0) = 1 -> (1+1)*0.5 = 1.0
		auto D = StaticDotTestHelpers::MakeDegrees(180.0, EPCGExComparison::EqualOrGreater);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 1.0, 0.001, "DegreesConstant=180 -> threshold=1.0");
	}

	{
		// 60 deg -> DegreesToDot(120) = cos(120) = -0.5 -> (1+-0.5)*0.5 = 0.25
		auto D = StaticDotTestHelpers::MakeDegrees(60.0, EPCGExComparison::EqualOrGreater);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 0.25, 0.001, "DegreesConstant=60 -> threshold=0.25");
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotInitThresholdDegreesUnsignedTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Init.ThresholdDegreesUnsigned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotInitThresholdDegreesUnsignedTest::RunTest(const FString& Parameters)
{
	// Unsigned degrees: ComparisonThreshold = FMath::Abs(DegreesToDot(180 - DegreesConstant))

	{
		// 90 deg -> DegreesToDot(90) = 0 -> abs(0) = 0
		auto D = StaticDotTestHelpers::MakeDegrees(90.0, EPCGExComparison::EqualOrGreater, true);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 0.0, 0.001, "unsigned 90deg -> threshold=0.0");
	}

	{
		// 60 deg -> DegreesToDot(120) = -0.5 -> abs = 0.5
		auto D = StaticDotTestHelpers::MakeDegrees(60.0, EPCGExComparison::EqualOrGreater, true);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 0.5, 0.001, "unsigned 60deg -> threshold=0.5");
	}

	{
		// 180 deg -> DegreesToDot(0) = 1 -> abs = 1
		auto D = StaticDotTestHelpers::MakeDegrees(180.0, EPCGExComparison::EqualOrGreater, true);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonThreshold, 1.0, 0.001, "unsigned 180deg -> threshold=1.0");
	}

	return true;
}

// =============================================================================
// Init() - ComparisonTolerance Computation
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotInitToleranceTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Init.Tolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotInitToleranceTest::RunTest(const FString& Parameters)
{
	// Verify tolerance is computed from DotTolerance/DegreesTolerance, NOT from DotConstant

	{
		// Signed scalar: ComparisonTolerance = (1 + DotTolerance) * 0.5
		auto D = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::NearlyEqual, false, 0.1);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonTolerance, (1.0 + 0.1) * 0.5, 0.001, "Scalar tolerance from DotTolerance=0.1");
		// Verify threshold is separate from tolerance
		TestTrue(TEXT("Threshold != Tolerance"), !FMath::IsNearlyEqual(D.ComparisonThreshold, D.ComparisonTolerance, 0.001));
	}

	{
		// Unsigned scalar: ComparisonTolerance = FMath::Abs(DotTolerance)
		auto D = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::NearlyEqual, true, 0.1);
		PCGEX_TEST_NEARLY_EQUAL(D.ComparisonTolerance, 0.1, 0.001, "Unsigned scalar tolerance = abs(DotTolerance)");
	}

	return true;
}

// =============================================================================
// Init() - Threshold vs Tolerance Independence (Core Bug Regression)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotInitThresholdNotToleranceTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Init.ThresholdNotTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotInitThresholdNotToleranceTest::RunTest(const FString& Parameters)
{
	// THE BUG: Before the fix, ComparisonThreshold didn't exist and Test() compared
	// against ComparisonTolerance (derived from DotTolerance) instead of DotConstant.
	// With defaults DotConstant=0.5 and DotTolerance=0.1, the effective threshold
	// was 0.1 instead of 0.5, making almost everything pass.

	// Verify that changing DotConstant changes ComparisonThreshold, not ComparisonTolerance
	{
		auto D1 = StaticDotTestHelpers::MakeScalar(0.3, EPCGExComparison::EqualOrGreater, false, 0.1);
		auto D2 = StaticDotTestHelpers::MakeScalar(0.8, EPCGExComparison::EqualOrGreater, false, 0.1);

		TestTrue(TEXT("Different DotConstant -> different ComparisonThreshold"),
			!FMath::IsNearlyEqual(D1.ComparisonThreshold, D2.ComparisonThreshold, 0.001));
		TestTrue(TEXT("Same DotTolerance -> same ComparisonTolerance"),
			FMath::IsNearlyEqual(D1.ComparisonTolerance, D2.ComparisonTolerance, 0.001));
	}

	// Verify that changing DotTolerance changes ComparisonTolerance, not ComparisonThreshold
	{
		auto D1 = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrGreater, false, 0.05);
		auto D2 = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrGreater, false, 0.2);

		TestTrue(TEXT("Same DotConstant -> same ComparisonThreshold"),
			FMath::IsNearlyEqual(D1.ComparisonThreshold, D2.ComparisonThreshold, 0.001));
		TestTrue(TEXT("Different DotTolerance -> different ComparisonTolerance"),
			!FMath::IsNearlyEqual(D1.ComparisonTolerance, D2.ComparisonTolerance, 0.001));
	}

	return true;
}

// =============================================================================
// Test() - Signed Scalar (Default Mode)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestScalarSignedEqualOrGreaterTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.ScalarSigned.EqualOrGreater",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestScalarSignedEqualOrGreaterTest::RunTest(const FString& Parameters)
{
	// DotConstant=0.5 with EqualOrGreater
	// Threshold = (1+0.5)*0.5 = 0.75
	// Test remaps input: (1+A)*0.5 >= 0.75 => A >= 0.5
	auto D = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrGreater);

	TestTrue(TEXT("dot=1.0 (same dir) passes"), D.Test(1.0));     // (1+1)*0.5=1.0 >= 0.75
	TestTrue(TEXT("dot=0.8 passes"), D.Test(0.8));                  // (1+0.8)*0.5=0.9 >= 0.75
	TestTrue(TEXT("dot=0.5 (at threshold) passes"), D.Test(0.5));   // (1+0.5)*0.5=0.75 >= 0.75
	TestFalse(TEXT("dot=0.3 fails"), D.Test(0.3));                  // (1+0.3)*0.5=0.65 < 0.75
	TestFalse(TEXT("dot=0.0 (perpendicular) fails"), D.Test(0.0));  // (1+0)*0.5=0.5 < 0.75
	TestFalse(TEXT("dot=-0.5 fails"), D.Test(-0.5));                // (1-0.5)*0.5=0.25 < 0.75
	TestFalse(TEXT("dot=-1.0 (opposite) fails"), D.Test(-1.0));     // (1-1)*0.5=0.0 < 0.75

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestScalarSignedStrictlyGreaterTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.ScalarSigned.StrictlyGreater",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestScalarSignedStrictlyGreaterTest::RunTest(const FString& Parameters)
{
	auto D = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::StrictlyGreater);

	TestTrue(TEXT("dot=0.8 > threshold"), D.Test(0.8));
	TestFalse(TEXT("dot=0.5 at threshold fails strictly"), D.Test(0.5));
	TestFalse(TEXT("dot=0.3 below threshold"), D.Test(0.3));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestScalarSignedEqualOrSmallerTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.ScalarSigned.EqualOrSmaller",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestScalarSignedEqualOrSmallerTest::RunTest(const FString& Parameters)
{
	auto D = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrSmaller);

	TestFalse(TEXT("dot=0.8 above threshold fails"), D.Test(0.8));
	TestTrue(TEXT("dot=0.5 at threshold passes"), D.Test(0.5));
	TestTrue(TEXT("dot=0.0 below passes"), D.Test(0.0));
	TestTrue(TEXT("dot=-1.0 opposite passes"), D.Test(-1.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestScalarSignedStrictlyEqualTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.ScalarSigned.StrictlyEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestScalarSignedStrictlyEqualTest::RunTest(const FString& Parameters)
{
	auto D = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::StrictlyEqual);

	TestTrue(TEXT("dot=0.5 exactly at threshold passes"), D.Test(0.5));
	TestFalse(TEXT("dot=0.8 above threshold fails"), D.Test(0.8));
	TestFalse(TEXT("dot=0.0 below threshold fails"), D.Test(0.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestScalarSignedNearlyEqualTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.ScalarSigned.NearlyEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestScalarSignedNearlyEqualTest::RunTest(const FString& Parameters)
{
	// DotConstant=0.5, DotTolerance=0.1
	// ComparisonThreshold = 0.75, ComparisonTolerance = (1+0.1)*0.5 = 0.55
	auto D = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::NearlyEqual, false, 0.1);

	// Remapped input at dot=0.5 -> 0.75 (at threshold)
	TestTrue(TEXT("dot=0.5 at threshold passes ~="), D.Test(0.5));

	// Remapped input at dot=0.6 -> 0.8, diff from 0.75 = 0.05 < tolerance 0.55
	TestTrue(TEXT("dot=0.6 near threshold passes ~="), D.Test(0.6));

	// Far from threshold: dot=-0.5 -> 0.25, diff from 0.75 = 0.5 < tolerance 0.55
	TestTrue(TEXT("dot=-0.5 within wide tolerance passes ~="), D.Test(-0.5));

	return true;
}

// =============================================================================
// Test() - Unsigned Scalar
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestScalarUnsignedTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.ScalarUnsigned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestScalarUnsignedTest::RunTest(const FString& Parameters)
{
	// DotConstant=0.5, unsigned
	// ComparisonThreshold = abs(0.5) = 0.5
	// Test uses abs(A) >= 0.5
	auto D = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrGreater, true);

	TestTrue(TEXT("dot=1.0 passes (abs=1.0 >= 0.5)"), D.Test(1.0));
	TestTrue(TEXT("dot=0.8 passes (abs=0.8 >= 0.5)"), D.Test(0.8));
	TestTrue(TEXT("dot=0.5 passes (abs=0.5 >= 0.5)"), D.Test(0.5));
	TestFalse(TEXT("dot=0.3 fails (abs=0.3 < 0.5)"), D.Test(0.3));
	TestFalse(TEXT("dot=0.0 fails (abs=0.0 < 0.5)"), D.Test(0.0));

	// Unsigned: negative dot values are treated as positive
	TestTrue(TEXT("dot=-1.0 passes unsigned (abs=1.0 >= 0.5)"), D.Test(-1.0));
	TestTrue(TEXT("dot=-0.8 passes unsigned (abs=0.8 >= 0.5)"), D.Test(-0.8));
	TestTrue(TEXT("dot=-0.5 passes unsigned (abs=0.5 >= 0.5)"), D.Test(-0.5));
	TestFalse(TEXT("dot=-0.3 fails unsigned (abs=0.3 < 0.5)"), D.Test(-0.3));

	return true;
}

// =============================================================================
// Test() - Degrees Domain (Signed)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestDegreesSignedTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.DegreesSigned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestDegreesSignedTest::RunTest(const FString& Parameters)
{
	// DegreesConstant=90, signed, EqualOrGreater
	// Init: DegreesToDot(180-90) = cos(90) = 0.0 -> (1+0)*0.5 = 0.5
	// Test remaps: (1+A)*0.5 >= 0.5 => A >= 0
	// So passes when dot >= 0 (within 90 degrees)
	auto D = StaticDotTestHelpers::MakeDegrees(90.0, EPCGExComparison::EqualOrGreater);

	TestTrue(TEXT("dot=1.0 (0 deg) passes"), D.Test(1.0));            // Aligned
	TestTrue(TEXT("dot=0.707 (~45 deg) passes"), D.Test(0.707));       // 45 degrees
	TestTrue(TEXT("dot=0.0 (90 deg, boundary) passes"), D.Test(0.0));  // Exactly at threshold
	TestFalse(TEXT("dot=-0.707 (~135 deg) fails"), D.Test(-0.707));    // 135 degrees
	TestFalse(TEXT("dot=-1.0 (180 deg) fails"), D.Test(-1.0));         // Opposite

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestDegreesTightThresholdTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.DegreesTightThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestDegreesTightThresholdTest::RunTest(const FString& Parameters)
{
	// DegreesConstant=135 with EqualOrGreater
	// Init: DegreesToDot(180-135) = cos(45) = 0.707 -> (1+0.707)*0.5 = 0.854
	// Test: (1+A)*0.5 >= 0.854 => A >= 0.707
	// So only within ~45 degrees of alignment passes
	auto D = StaticDotTestHelpers::MakeDegrees(135.0, EPCGExComparison::EqualOrGreater);

	TestTrue(TEXT("dot=1.0 (0 deg) passes tight"), D.Test(1.0));
	TestTrue(TEXT("dot=0.8 (~37 deg) passes tight"), D.Test(0.8));
	TestFalse(TEXT("dot=0.5 (~60 deg) fails tight"), D.Test(0.5));
	TestFalse(TEXT("dot=0.0 (90 deg) fails tight"), D.Test(0.0));

	return true;
}

// =============================================================================
// Test() - Degrees Domain (Unsigned)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestDegreesUnsignedTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.DegreesUnsigned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestDegreesUnsignedTest::RunTest(const FString& Parameters)
{
	// DegreesConstant=90, unsigned, EqualOrGreater
	// Init: DegreesToDot(90) = cos(pi/2) ≈ 6e-17 -> abs ≈ epsilon
	// Test: abs(A) >= epsilon => passes for any non-zero abs
	// NOTE: cos(90°) is not exactly 0 in floating point, so the threshold is epsilon-above-zero.
	// dot=0.0 (exact perpendicular) fails because abs(0.0) < epsilon.
	auto D = StaticDotTestHelpers::MakeDegrees(90.0, EPCGExComparison::EqualOrGreater, true);

	TestTrue(TEXT("unsigned dot=1.0 passes"), D.Test(1.0));
	TestTrue(TEXT("unsigned dot=0.5 passes"), D.Test(0.5));
	TestFalse(TEXT("unsigned dot=0.0 fails (fp precision: abs(0) < cos(90°)≈epsilon)"), D.Test(0.0));
	TestTrue(TEXT("unsigned dot=-0.5 passes (abs=0.5)"), D.Test(-0.5));
	TestTrue(TEXT("unsigned dot=-1.0 passes (abs=1.0)"), D.Test(-1.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestDegreesUnsignedTightTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.DegreesUnsignedTight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestDegreesUnsignedTightTest::RunTest(const FString& Parameters)
{
	// DegreesConstant=135, unsigned, EqualOrGreater
	// Init: DegreesToDot(45) = 0.707 -> abs(0.707) = 0.707
	// Test: abs(A) >= 0.707
	auto D = StaticDotTestHelpers::MakeDegrees(135.0, EPCGExComparison::EqualOrGreater, true);

	TestTrue(TEXT("unsigned dot=1.0 passes"), D.Test(1.0));
	TestTrue(TEXT("unsigned dot=0.8 passes"), D.Test(0.8));
	TestFalse(TEXT("unsigned dot=0.5 fails (abs=0.5 < 0.707)"), D.Test(0.5));
	TestFalse(TEXT("unsigned dot=0.0 fails"), D.Test(0.0));

	// Unsigned treats negatives as positive
	TestTrue(TEXT("unsigned dot=-1.0 passes (abs=1.0)"), D.Test(-1.0));
	TestTrue(TEXT("unsigned dot=-0.8 passes (abs=0.8)"), D.Test(-0.8));
	TestFalse(TEXT("unsigned dot=-0.5 fails (abs=0.5 < 0.707)"), D.Test(-0.5));

	return true;
}

// =============================================================================
// All Comparison Operators
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestAllOpsTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.AllOperators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestAllOpsTest::RunTest(const FString& Parameters)
{
	// Test all comparison operators with DotConstant=0.5, dot input=0.8
	// Remapped: input = (1+0.8)*0.5 = 0.9, threshold = (1+0.5)*0.5 = 0.75

	TestTrue(TEXT("0.8 == 0.5 (StrictlyEqual) should fail"),
		!StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::StrictlyEqual).Test(0.8));

	TestTrue(TEXT("0.8 != 0.5 (StrictlyNotEqual) should pass"),
		StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::StrictlyNotEqual).Test(0.8));

	TestTrue(TEXT("0.8 >= 0.5 (EqualOrGreater) should pass"),
		StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrGreater).Test(0.8));

	TestTrue(TEXT("0.8 <= 0.5 (EqualOrSmaller) should fail"),
		!StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrSmaller).Test(0.8));

	TestTrue(TEXT("0.8 > 0.5 (StrictlyGreater) should pass"),
		StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::StrictlyGreater).Test(0.8));

	TestTrue(TEXT("0.8 < 0.5 (StrictlySmaller) should fail"),
		!StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::StrictlySmaller).Test(0.8));

	// Now test below threshold: dot=0.3
	// Remapped: (1+0.3)*0.5 = 0.65 vs threshold 0.75

	TestTrue(TEXT("0.3 >= 0.5 (EqualOrGreater) should fail"),
		!StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrGreater).Test(0.3));

	TestTrue(TEXT("0.3 <= 0.5 (EqualOrSmaller) should pass"),
		StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrSmaller).Test(0.3));

	TestTrue(TEXT("0.3 < 0.5 (StrictlySmaller) should pass"),
		StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::StrictlySmaller).Test(0.3));

	// At threshold: dot=0.5
	// Remapped: (1+0.5)*0.5 = 0.75 vs threshold 0.75

	TestTrue(TEXT("0.5 == 0.5 (StrictlyEqual) should pass"),
		StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::StrictlyEqual).Test(0.5));

	TestTrue(TEXT("0.5 >= 0.5 (EqualOrGreater) should pass"),
		StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrGreater).Test(0.5));

	TestTrue(TEXT("0.5 <= 0.5 (EqualOrSmaller) should pass"),
		StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrSmaller).Test(0.5));

	TestTrue(TEXT("0.5 > 0.5 (StrictlyGreater) should fail"),
		!StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::StrictlyGreater).Test(0.5));

	TestTrue(TEXT("0.5 < 0.5 (StrictlySmaller) should fail"),
		!StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::StrictlySmaller).Test(0.5));

	return true;
}

// =============================================================================
// Practical PathStitch Scenario (The Bug That Started This)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestPathStitchScenarioTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.PathStitchScenario",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestPathStitchScenarioTest::RunTest(const FString& Parameters)
{
	// PathStitch alignment check: two path endpoints face each other if
	// dot(dirA, -dirB) is close to 1.0.
	// Default settings: DotConstant=0.5, EqualOrGreater
	// With the fix, this correctly rejects perpendicular paths.

	auto D = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrGreater);

	// Two paths facing each other (aligned): dot = 1.0
	TestTrue(TEXT("Facing paths (dot=1.0) should stitch"), D.Test(1.0));

	// Two paths at slight angle: dot = 0.9
	TestTrue(TEXT("Nearly aligned (dot=0.9) should stitch"), D.Test(0.9));

	// Two paths forming an L-shape (perpendicular): dot = 0.0
	// THIS IS THE BUG REGRESSION: before the fix, this incorrectly passed
	TestFalse(TEXT("Perpendicular paths (dot=0.0) should NOT stitch"), D.Test(0.0));

	// Two paths forming a V: dot = -0.5
	TestFalse(TEXT("V-shaped paths (dot=-0.5) should NOT stitch"), D.Test(-0.5));

	// Two paths going same direction: dot = -1.0
	TestFalse(TEXT("Same direction paths (dot=-1.0) should NOT stitch"), D.Test(-1.0));

	return true;
}

// =============================================================================
// Edge Cases
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestEdgeCasesTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestEdgeCasesTest::RunTest(const FString& Parameters)
{
	// Threshold at boundary: DotConstant = 1.0 (only exact same direction passes)
	{
		auto D = StaticDotTestHelpers::MakeScalar(1.0, EPCGExComparison::EqualOrGreater);
		TestTrue(TEXT("dot=1.0 at max threshold passes"), D.Test(1.0));
		TestFalse(TEXT("dot=0.99 just below max threshold fails"), D.Test(0.99));
	}

	// Threshold at boundary: DotConstant = -1.0 (everything passes)
	{
		auto D = StaticDotTestHelpers::MakeScalar(-1.0, EPCGExComparison::EqualOrGreater);
		TestTrue(TEXT("dot=1.0 with min threshold passes"), D.Test(1.0));
		TestTrue(TEXT("dot=0.0 with min threshold passes"), D.Test(0.0));
		TestTrue(TEXT("dot=-1.0 at min threshold passes"), D.Test(-1.0));
	}

	// Threshold = 0 (perpendicular boundary)
	{
		auto D = StaticDotTestHelpers::MakeScalar(0.0, EPCGExComparison::EqualOrGreater);
		TestTrue(TEXT("dot=0.5 above zero threshold passes"), D.Test(0.5));
		TestTrue(TEXT("dot=0.0 at zero threshold passes"), D.Test(0.0));
		TestFalse(TEXT("dot=-0.5 below zero threshold fails"), D.Test(-0.5));
	}

	// Degrees boundary: 0 degrees (only perfect opposite passes)
	{
		auto D = StaticDotTestHelpers::MakeDegrees(0.0, EPCGExComparison::EqualOrGreater);
		// ComparisonThreshold = (1 + cos(180)) * 0.5 = (1 + (-1)) * 0.5 = 0.0
		// Everything with remapped >= 0 passes, which is all values since (1+A)*0.5 >= 0 for A >= -1
		TestTrue(TEXT("0 deg threshold: everything passes"), D.Test(0.0));
		TestTrue(TEXT("0 deg threshold: opposite passes"), D.Test(-1.0));
	}

	// Degrees boundary: 180 degrees (only perfect alignment passes)
	{
		auto D = StaticDotTestHelpers::MakeDegrees(180.0, EPCGExComparison::EqualOrGreater);
		// ComparisonThreshold = (1 + cos(0)) * 0.5 = (1 + 1) * 0.5 = 1.0
		TestTrue(TEXT("180 deg threshold: perfect alignment passes"), D.Test(1.0));
		TestFalse(TEXT("180 deg threshold: near-aligned fails"), D.Test(0.99));
	}

	return true;
}

// =============================================================================
// Consistency between Scalar and Degrees domains
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStaticDotTestDomainConsistencyTest,
	"PCGEx.Unit.Utils.StaticDotComparison.Test.DomainConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStaticDotTestDomainConsistencyTest::RunTest(const FString& Parameters)
{
	// Scalar DotConstant=0.5 should produce the same results as
	// Degrees DegreesConstant that maps to the same dot value.
	// cos(60) = 0.5, so DegreesConstant for the same threshold:
	// DegreesToDot(180 - DegreesConstant) = DotConstant
	// cos(180 - DegreesConstant) = 0.5
	// 180 - DegreesConstant = 60
	// DegreesConstant = 120

	auto DScalar = StaticDotTestHelpers::MakeScalar(0.5, EPCGExComparison::EqualOrGreater);
	auto DDegrees = StaticDotTestHelpers::MakeDegrees(120.0, EPCGExComparison::EqualOrGreater);

	// They should have the same ComparisonThreshold
	PCGEX_TEST_NEARLY_EQUAL(DScalar.ComparisonThreshold, DDegrees.ComparisonThreshold, 0.001,
		"Scalar(0.5) and Degrees(120) same threshold");

	// And produce the same Test() results
	const double TestDots[] = {1.0, 0.8, 0.5, 0.3, 0.0, -0.5, -1.0};
	for (double Dot : TestDots)
	{
		TestTrue(
			*FString::Printf(TEXT("Scalar and Degrees agree for dot=%f"), Dot),
			DScalar.Test(Dot) == DDegrees.Test(Dot));
	}

	return true;
}
