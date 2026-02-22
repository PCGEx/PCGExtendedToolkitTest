// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * LocalTangent Projection Unit Tests
 *
 * Tests for the LocalTangent projection method added to the DCEL face enumerator:
 * - EPCGExProjectionMethod::LocalTangent enum value
 * - FPCGExGeo2DProjectionDetails adaptive X hint (degeneracy avoidance)
 * - Per-node tangent frame computation (cross products, BFS consistency)
 * - Per-face BestFitPlane projection for polygon construction
 * - FFaceEnumeratorCacheFactory::ComputeProjectionHash for LocalTangent
 * - FCachedTangentFrames structure
 * - FPlanarFaceEnumerator LocalTangent accessors
 *
 * Test naming convention: PCGEx.Unit.Clusters.LocalTangent.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Math/PCGExProjectionDetails.h"
#include "Math/PCGExBestFitPlane.h"
#include "Clusters/Artifacts/PCGExCachedFaceEnumerator.h"
#include "Clusters/Artifacts/PCGExPlanarFaceEnumerator.h"
#include "Clusters/Artifacts/PCGExCell.h"
#include "Helpers/PCGExTestHelpers.h"
#include "Math/Geo/PCGExGeo.h"

// =============================================================================
// Enum Tests
// =============================================================================

/**
 * Test EPCGExProjectionMethod::LocalTangent enum value exists
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentEnumValueTest,
	"PCGEx.Unit.Clusters.LocalTangent.Enum.Value",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentEnumValueTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Normal is 0"), static_cast<uint8>(EPCGExProjectionMethod::Normal), 0);
	TestEqual(TEXT("BestFit is 1"), static_cast<uint8>(EPCGExProjectionMethod::BestFit), 1);
	TestEqual(TEXT("LocalTangent is 2"), static_cast<uint8>(EPCGExProjectionMethod::LocalTangent), 2);

	// Verify distinctness
	TestTrue(TEXT("LocalTangent != Normal"),
	         EPCGExProjectionMethod::LocalTangent != EPCGExProjectionMethod::Normal);
	TestTrue(TEXT("LocalTangent != BestFit"),
	         EPCGExProjectionMethod::LocalTangent != EPCGExProjectionMethod::BestFit);

	return true;
}

/**
 * Test FPCGExGeo2DProjectionDetails default method and LocalTangent assignment
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentProjectionMethodTest,
	"PCGEx.Unit.Clusters.LocalTangent.Enum.ProjectionMethod",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentProjectionMethodTest::RunTest(const FString& Parameters)
{
	// Default method is Normal
	FPCGExGeo2DProjectionDetails Projection;
	TestEqual(TEXT("Default method is Normal"),
	          Projection.Method, EPCGExProjectionMethod::Normal);

	// Can assign LocalTangent
	Projection.Method = EPCGExProjectionMethod::LocalTangent;
	TestEqual(TEXT("Method assigned to LocalTangent"),
	          Projection.Method, EPCGExProjectionMethod::LocalTangent);

	return true;
}

// =============================================================================
// Projection Adaptive X Hint Tests
// =============================================================================

/**
 * Test that projection with UpVector normal produces correct XY plane projection
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentProjectionUpNormalTest,
	"PCGEx.Unit.Clusters.LocalTangent.Projection.UpNormal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentProjectionUpNormalTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Create a BestFitPlane with UpVector normal (horizontal plane)
	TArray<FVector> PlanePoints = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};
	const TArrayView<const FVector> PlaneView(PlanePoints);
	PCGExMath::FBestFitPlane Plane(PlaneView);

	FPCGExGeo2DProjectionDetails Projection;
	Projection.Init(Plane);

	// Project a point - should preserve X, Y and zero out Z
	const FVector TestPoint(50, 75, 200);
	const FVector Projected = Projection.Project(TestPoint);

	// The Z component in projected space corresponds to depth (along normal)
	// X and Y should be close to original (since we're projecting along UpVector)
	TestTrue(TEXT("UpVector projection preserves XY relationship"),
	         FMath::IsFinite(Projected.X) && FMath::IsFinite(Projected.Y));

	// Round-trip: project then unproject should give back original
	const FVector Unprojected = Projection.Unproject(Projected);
	TestTrue(TEXT("Round-trip UpVector"),
	         PCGExTest::NearlyEqual(Unprojected, TestPoint, Tolerance));

	return true;
}

/**
 * Test projection with ForwardVector normal (should use adaptive X hint)
 * This is the key degeneracy case: when normal ≈ WorldFwd, MakeFromZX degenerates.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentProjectionForwardNormalTest,
	"PCGEx.Unit.Clusters.LocalTangent.Projection.ForwardNormal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentProjectionForwardNormalTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Create a BestFitPlane with ForwardVector normal (YZ plane, looking forward)
	// This triggers the adaptive X hint: dot(Normal, WorldFwd) ≈ 1.0 > 0.95
	TArray<FVector> PlanePoints = {
		FVector(0, 0, 0),
		FVector(0, 100, 0),
		FVector(0, 100, 100),
		FVector(0, 0, 100)
	};
	const TArrayView<const FVector> PlaneView(PlanePoints);
	PCGExMath::FBestFitPlane Plane(PlaneView);

	FPCGExGeo2DProjectionDetails Projection;
	Projection.Init(Plane);

	// Verify projection quaternion is valid (not degenerate)
	TestTrue(TEXT("ProjectionQuat is normalized"),
	         Projection.ProjectionQuat.IsNormalized());
	TestTrue(TEXT("ProjectionQuatInv is normalized"),
	         Projection.ProjectionQuatInv.IsNormalized());

	// Project a point and verify round-trip works (non-degenerate)
	const FVector TestPoint(0, 50, 75);
	const FVector Projected = Projection.Project(TestPoint);
	const FVector Unprojected = Projection.Unproject(Projected);
	PCGEX_TEST_VECTOR_NEARLY_EQUAL(Unprojected, TestPoint, Tolerance, "Round-trip ForwardNormal");

	// Verify finite results
	TestTrue(TEXT("Projected X is finite"), FMath::IsFinite(Projected.X));
	TestTrue(TEXT("Projected Y is finite"), FMath::IsFinite(Projected.Y));
	TestTrue(TEXT("Projected Z is finite"), FMath::IsFinite(Projected.Z));

	return true;
}

/**
 * Test projection with tilted normal near ForwardVector (almost-degenerate case)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentProjectionNearForwardNormalTest,
	"PCGEx.Unit.Clusters.LocalTangent.Projection.NearForwardNormal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentProjectionNearForwardNormalTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Normal nearly aligned with ForwardVector but slightly tilted
	// This should still trigger the adaptive hint (dot > 0.95)
	FVector TiltedForward = FVector::ForwardVector + FVector(0, 0.1, 0);
	TiltedForward.Normalize();

	// Build plane manually from a tilted YZ plane
	TArray<FVector> PlanePoints;
	PlanePoints.Add(FVector::ZeroVector);
	PlanePoints.Add(TiltedForward.Cross(FVector::UpVector).GetSafeNormal() * 100);
	PlanePoints.Add(TiltedForward.Cross(FVector::UpVector).GetSafeNormal() * 100 + FVector::UpVector * 100);
	PlanePoints.Add(FVector::UpVector * 100);

	const TArrayView<const FVector> PlaneView(PlanePoints);
	PCGExMath::FBestFitPlane Plane(PlaneView);

	FPCGExGeo2DProjectionDetails Projection;
	Projection.Init(Plane);

	// Verify non-degenerate
	TestTrue(TEXT("Near-forward ProjectionQuat is normalized"),
	         Projection.ProjectionQuat.IsNormalized());

	// Round-trip test
	const FVector TestPoint(5, 50, 75);
	const FVector Projected = Projection.Project(TestPoint);
	const FVector Unprojected = Projection.Unproject(Projected);
	PCGEX_TEST_VECTOR_NEARLY_EQUAL(Unprojected, TestPoint, Tolerance, "Round-trip NearForward");

	return true;
}

/**
 * Test projection with various cardinal normals to ensure none are degenerate
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentProjectionCardinalNormalsTest,
	"PCGEx.Unit.Clusters.LocalTangent.Projection.CardinalNormals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentProjectionCardinalNormalsTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Test all 6 cardinal directions
	const TArray<FVector> Normals = {
		FVector::UpVector,
		-FVector::UpVector,
		FVector::ForwardVector,
		-FVector::ForwardVector,
		FVector::RightVector,
		-FVector::RightVector
	};

	const FVector TestPoint(42, 73, 101);

	for (int32 i = 0; i < Normals.Num(); ++i)
	{
		// Build a plane with this normal
		const FVector& N = Normals[i];
		FVector U = FMath::Abs(FVector::DotProduct(N, FVector::UpVector)) < 0.9
			            ? FVector::UpVector
			            : FVector::RightVector;
		FVector Tangent1 = FVector::CrossProduct(N, U).GetSafeNormal();
		FVector Tangent2 = FVector::CrossProduct(N, Tangent1).GetSafeNormal();

		TArray<FVector> CardinalPoints = {
			FVector::ZeroVector,
			Tangent1 * 100,
			Tangent1 * 100 + Tangent2 * 100,
			Tangent2 * 100
		};

		const TArrayView<const FVector> CardinalView(CardinalPoints);
		PCGExMath::FBestFitPlane Plane(CardinalView);
		FPCGExGeo2DProjectionDetails Projection;
		Projection.Init(Plane);

		// Verify quaternion validity
		TestTrue(FString::Printf(TEXT("Cardinal %d: quat normalized"), i),
		         Projection.ProjectionQuat.IsNormalized());

		// Round-trip
		const FVector Projected = Projection.Project(TestPoint);
		const FVector Unprojected = Projection.Unproject(Projected);
		TestTrue(FString::Printf(TEXT("Cardinal %d: round-trip"), i),
		         PCGExTest::NearlyEqual(Unprojected, TestPoint, Tolerance));
	}

	return true;
}

// =============================================================================
// Tangent Frame Construction Tests
// =============================================================================

/**
 * Test tangent frame construction from a known normal using the adaptive X hint.
 * This tests the same algorithm used in IProcessor::Process() step 3.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentFrameFromNormalTest,
	"PCGEx.Unit.Clusters.LocalTangent.TangentFrame.FromNormal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentFrameFromNormalTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Test the tangent frame construction algorithm (same as in IProcessor::Process step 3)
	auto BuildTangentFrame = [](const FVector& N) -> FQuat
	{
		const FVector XHint = FMath::Abs(FVector::DotProduct(N, FVector::ForwardVector)) < 0.95
			                      ? FVector::ForwardVector
			                      : FVector::RightVector;
		return FRotationMatrix::MakeFromZX(N, XHint).ToQuat();
	};

	// Test 1: UpVector normal — straightforward case
	{
		const FQuat Frame = BuildTangentFrame(FVector::UpVector);
		TestTrue(TEXT("UpVector frame is normalized"), Frame.IsNormalized());

		// Z axis of frame should align with UpVector
		const FVector FrameZ = Frame.GetAxisZ();
		TestTrue(TEXT("UpVector frame Z ≈ UpVector"),
		         PCGExTest::NearlyEqual(FrameZ, FVector::UpVector, Tolerance));
	}

	// Test 2: ForwardVector normal — triggers adaptive hint (should use RightVector)
	{
		const FQuat Frame = BuildTangentFrame(FVector::ForwardVector);
		TestTrue(TEXT("ForwardVector frame is normalized"), Frame.IsNormalized());

		const FVector FrameZ = Frame.GetAxisZ();
		TestTrue(TEXT("ForwardVector frame Z ≈ ForwardVector"),
		         PCGExTest::NearlyEqual(FrameZ, FVector::ForwardVector, Tolerance));
	}

	// Test 3: -ForwardVector normal — also triggers adaptive hint
	{
		const FQuat Frame = BuildTangentFrame(-FVector::ForwardVector);
		TestTrue(TEXT("-ForwardVector frame is normalized"), Frame.IsNormalized());

		const FVector FrameZ = Frame.GetAxisZ();
		TestTrue(TEXT("-ForwardVector frame Z ≈ -ForwardVector"),
		         PCGExTest::NearlyEqual(FrameZ, -FVector::ForwardVector, Tolerance));
	}

	// Test 4: RightVector normal — doesn't trigger adaptive hint
	{
		const FQuat Frame = BuildTangentFrame(FVector::RightVector);
		TestTrue(TEXT("RightVector frame is normalized"), Frame.IsNormalized());

		const FVector FrameZ = Frame.GetAxisZ();
		TestTrue(TEXT("RightVector frame Z ≈ RightVector"),
		         PCGExTest::NearlyEqual(FrameZ, FVector::RightVector, Tolerance));
	}

	// Test 5: Diagonal normal — general case
	{
		const FVector DiagNormal = FVector(1, 1, 1).GetSafeNormal();
		const FQuat Frame = BuildTangentFrame(DiagNormal);
		TestTrue(TEXT("Diagonal frame is normalized"), Frame.IsNormalized());

		const FVector FrameZ = Frame.GetAxisZ();
		TestTrue(TEXT("Diagonal frame Z ≈ diagonal normal"),
		         PCGExTest::NearlyEqual(FrameZ, DiagNormal, Tolerance));
	}

	return true;
}

/**
 * Test tangent frame orthogonality (X, Y, Z axes should be orthogonal)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentFrameOrthogonalityTest,
	"PCGEx.Unit.Clusters.LocalTangent.TangentFrame.Orthogonality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentFrameOrthogonalityTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.001;

	auto BuildTangentFrame = [](const FVector& N) -> FQuat
	{
		const FVector XHint = FMath::Abs(FVector::DotProduct(N, FVector::ForwardVector)) < 0.95
			                      ? FVector::ForwardVector
			                      : FVector::RightVector;
		return FRotationMatrix::MakeFromZX(N, XHint).ToQuat();
	};

	// Test a variety of normals
	TArray<FVector> TestNormals = {
		FVector::UpVector,
		FVector::ForwardVector,
		FVector::RightVector,
		-FVector::UpVector,
		FVector(1, 1, 0).GetSafeNormal(),
		FVector(1, 1, 1).GetSafeNormal(),
		FVector(0.01, 0.01, 1.0).GetSafeNormal(),     // Nearly up
		FVector(1.0, 0.01, 0.01).GetSafeNormal(),      // Nearly forward
		FVector(0.3, -0.7, 0.65).GetSafeNormal(),      // Arbitrary
	};

	for (int32 i = 0; i < TestNormals.Num(); ++i)
	{
		const FQuat Frame = BuildTangentFrame(TestNormals[i]);
		const FVector X = Frame.GetAxisX();
		const FVector Y = Frame.GetAxisY();
		const FVector Z = Frame.GetAxisZ();

		// Axes should be orthogonal (dot products ≈ 0)
		TestTrue(FString::Printf(TEXT("Normal %d: X·Y ≈ 0"), i),
		         FMath::IsNearlyZero(FVector::DotProduct(X, Y), Tolerance));
		TestTrue(FString::Printf(TEXT("Normal %d: X·Z ≈ 0"), i),
		         FMath::IsNearlyZero(FVector::DotProduct(X, Z), Tolerance));
		TestTrue(FString::Printf(TEXT("Normal %d: Y·Z ≈ 0"), i),
		         FMath::IsNearlyZero(FVector::DotProduct(Y, Z), Tolerance));

		// Axes should be unit length
		TestTrue(FString::Printf(TEXT("Normal %d: |X| ≈ 1"), i),
		         FMath::IsNearlyEqual(X.Size(), 1.0, Tolerance));
		TestTrue(FString::Printf(TEXT("Normal %d: |Y| ≈ 1"), i),
		         FMath::IsNearlyEqual(Y.Size(), 1.0, Tolerance));
		TestTrue(FString::Printf(TEXT("Normal %d: |Z| ≈ 1"), i),
		         FMath::IsNearlyEqual(Z.Size(), 1.0, Tolerance));
	}

	return true;
}

/**
 * Test cross-product normal computation from edges
 * (same algorithm as IProcessor::Process step 1)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentCrossProductNormalTest,
	"PCGEx.Unit.Clusters.LocalTangent.TangentFrame.CrossProductNormal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentCrossProductNormalTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Simulate the cross-product normal computation for a node with neighbors
	// Algorithm: find two most linearly independent edges, take cross product
	auto ComputeNormalFromEdges = [](const FVector& NodePos, const TArray<FVector>& NeighborPositions) -> FVector
	{
		if (NeighborPositions.Num() < 2) { return FVector::UpVector; }

		double BestCross = -1.0;
		FVector BestNormal = FVector::UpVector;

		for (int32 i = 0; i < NeighborPositions.Num(); ++i)
		{
			const FVector DirA = (NeighborPositions[i] - NodePos).GetSafeNormal();
			for (int32 j = i + 1; j < NeighborPositions.Num(); ++j)
			{
				const FVector DirB = (NeighborPositions[j] - NodePos).GetSafeNormal();
				const FVector Cross = FVector::CrossProduct(DirA, DirB);
				const double CrossLen = Cross.Size();
				if (CrossLen > BestCross)
				{
					BestCross = CrossLen;
					BestNormal = Cross / CrossLen;
				}
			}
		}
		return BestNormal;
	};

	// Test 1: XY plane node (neighbors in XY, normal should be along Z)
	{
		FVector NodePos(0, 0, 0);
		TArray<FVector> Neighbors = {
			FVector(100, 0, 0),
			FVector(0, 100, 0),
			FVector(-100, 0, 0)
		};

		FVector Normal = ComputeNormalFromEdges(NodePos, Neighbors);
		TestTrue(TEXT("XY plane normal is along Z"),
		         FMath::IsNearlyEqual(FMath::Abs(Normal.Z), 1.0, Tolerance));
	}

	// Test 2: XZ plane node (normal should be along Y)
	{
		FVector NodePos(0, 0, 0);
		TArray<FVector> Neighbors = {
			FVector(100, 0, 0),
			FVector(0, 0, 100)
		};

		FVector Normal = ComputeNormalFromEdges(NodePos, Neighbors);
		TestTrue(TEXT("XZ plane normal is along Y"),
		         FMath::IsNearlyEqual(FMath::Abs(Normal.Y), 1.0, Tolerance));
	}

	// Test 3: Single neighbor (fallback to UpVector)
	{
		FVector NodePos(0, 0, 0);
		TArray<FVector> Neighbors = {
			FVector(100, 0, 0)
		};

		FVector Normal = ComputeNormalFromEdges(NodePos, Neighbors);
		PCGEX_TEST_VECTOR_NEARLY_EQUAL(Normal, FVector::UpVector, Tolerance, "Single neighbor fallback");
	}

	// Test 4: Nearly collinear edges (should still produce valid normal)
	{
		FVector NodePos(0, 0, 0);
		TArray<FVector> Neighbors = {
			FVector(100, 0, 0),
			FVector(100, 1, 0),      // Nearly collinear with first
			FVector(0, 100, 0)       // Perpendicular — best pair
		};

		FVector Normal = ComputeNormalFromEdges(NodePos, Neighbors);
		// Should pick the most independent pair (first,third or second,third)
		TestTrue(TEXT("Nearly collinear: normal Z is dominant"),
		         FMath::Abs(Normal.Z) > 0.5);
	}

	return true;
}

// =============================================================================
// BFS Normal Consistency Tests
// =============================================================================

/**
 * Test BFS normal consistency propagation algorithm
 * (same algorithm as IProcessor::Process step 2)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentBFSConsistencyTest,
	"PCGEx.Unit.Clusters.LocalTangent.TangentFrame.BFSConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentBFSConsistencyTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Simulate the BFS normal consistency propagation
	// Given: Array of normals and adjacency list
	// Algorithm: BFS from node 0, flip neighbor normals if they disagree with parent

	// Create a simple chain: 0 - 1 - 2 - 3
	// Node 0 and 2 point up, nodes 1 and 3 point down (inconsistent)
	TArray<FVector> NodeNormals = {
		FVector(0, 0, 1),   // Node 0: up
		FVector(0, 0, -1),  // Node 1: down (needs flip)
		FVector(0, 0, 1),   // Node 2: up
		FVector(0, 0, -1),  // Node 3: down (needs flip)
	};

	// Adjacency: chain topology
	TArray<TArray<int32>> Adjacency = {
		{1},       // Node 0 connects to 1
		{0, 2},    // Node 1 connects to 0, 2
		{1, 3},    // Node 2 connects to 1, 3
		{2}        // Node 3 connects to 2
	};

	// BFS propagation (same algorithm as IProcessor::Process)
	const int32 NumNodes = NodeNormals.Num();
	TArray<bool> Visited;
	Visited.SetNumZeroed(NumNodes);
	TArray<int32> BFSQueue;
	BFSQueue.Reserve(NumNodes);
	BFSQueue.Add(0);
	Visited[0] = true;

	for (int32 QueueIdx = 0; QueueIdx < BFSQueue.Num(); ++QueueIdx)
	{
		const int32 CurrentNode = BFSQueue[QueueIdx];

		for (const int32 NeighborNode : Adjacency[CurrentNode])
		{
			if (Visited[NeighborNode]) { continue; }

			if (FVector::DotProduct(NodeNormals[CurrentNode], NodeNormals[NeighborNode]) < 0)
			{
				NodeNormals[NeighborNode] = -NodeNormals[NeighborNode];
			}

			Visited[NeighborNode] = true;
			BFSQueue.Add(NeighborNode);
		}
	}

	// All normals should now point in the same direction (up)
	for (int32 i = 0; i < NumNodes; ++i)
	{
		TestTrue(FString::Printf(TEXT("Node %d: normal Z > 0 after BFS"), i),
		         NodeNormals[i].Z > 0);
		TestTrue(FString::Printf(TEXT("Node %d: normal consistent with (0,0,1)"), i),
		         PCGExTest::NearlyEqual(NodeNormals[i], FVector(0, 0, 1), Tolerance));
	}

	// Verify all nodes visited
	for (int32 i = 0; i < NumNodes; ++i)
	{
		TestTrue(FString::Printf(TEXT("Node %d visited"), i), Visited[i]);
	}

	return true;
}

/**
 * Test BFS consistency with a branching graph
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentBFSBranchingTest,
	"PCGEx.Unit.Clusters.LocalTangent.TangentFrame.BFSBranching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentBFSBranchingTest::RunTest(const FString& Parameters)
{
	// Star topology: center node 0 connected to 1, 2, 3
	// Center points up, all leaves point down
	TArray<FVector> NodeNormals = {
		FVector(0, 0, 1),   // Center: up
		FVector(0, 0, -1),  // Leaf 1: down
		FVector(0, 0, -1),  // Leaf 2: down
		FVector(0, 0, -1),  // Leaf 3: down
	};

	TArray<TArray<int32>> Adjacency = {
		{1, 2, 3},  // Center connects to all leaves
		{0},         // Leaf 1
		{0},         // Leaf 2
		{0}          // Leaf 3
	};

	// BFS
	const int32 NumNodes = NodeNormals.Num();
	TArray<bool> Visited;
	Visited.SetNumZeroed(NumNodes);
	TArray<int32> BFSQueue;
	BFSQueue.Reserve(NumNodes);
	BFSQueue.Add(0);
	Visited[0] = true;

	for (int32 QueueIdx = 0; QueueIdx < BFSQueue.Num(); ++QueueIdx)
	{
		const int32 CurrentNode = BFSQueue[QueueIdx];
		for (const int32 NeighborNode : Adjacency[CurrentNode])
		{
			if (Visited[NeighborNode]) { continue; }
			if (FVector::DotProduct(NodeNormals[CurrentNode], NodeNormals[NeighborNode]) < 0)
			{
				NodeNormals[NeighborNode] = -NodeNormals[NeighborNode];
			}
			Visited[NeighborNode] = true;
			BFSQueue.Add(NeighborNode);
		}
	}

	// All normals should agree with root (up)
	for (int32 i = 0; i < NumNodes; ++i)
	{
		TestTrue(FString::Printf(TEXT("Star node %d: normal Z > 0"), i),
		         NodeNormals[i].Z > 0);
	}

	return true;
}

/**
 * Test BFS consistency with tilted normals (non-axis-aligned)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentBFSTiltedNormalsTest,
	"PCGEx.Unit.Clusters.LocalTangent.TangentFrame.BFSTilted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentBFSTiltedNormalsTest::RunTest(const FString& Parameters)
{
	// Chain: 0 - 1 - 2
	// All normals tilted but some flipped
	const FVector RefNormal = FVector(0.3, 0.5, 0.8).GetSafeNormal();

	TArray<FVector> NodeNormals = {
		RefNormal,     // Node 0: reference direction
		-RefNormal,    // Node 1: opposite (should be flipped)
		RefNormal,     // Node 2: same as reference
	};

	TArray<TArray<int32>> Adjacency = {
		{1},
		{0, 2},
		{1}
	};

	// BFS
	const int32 NumNodes = NodeNormals.Num();
	TArray<bool> Visited;
	Visited.SetNumZeroed(NumNodes);
	TArray<int32> BFSQueue;
	BFSQueue.Add(0);
	Visited[0] = true;

	for (int32 QueueIdx = 0; QueueIdx < BFSQueue.Num(); ++QueueIdx)
	{
		const int32 CurrentNode = BFSQueue[QueueIdx];
		for (const int32 NeighborNode : Adjacency[CurrentNode])
		{
			if (Visited[NeighborNode]) { continue; }
			if (FVector::DotProduct(NodeNormals[CurrentNode], NodeNormals[NeighborNode]) < 0)
			{
				NodeNormals[NeighborNode] = -NodeNormals[NeighborNode];
			}
			Visited[NeighborNode] = true;
			BFSQueue.Add(NeighborNode);
		}
	}

	// All normals should now agree (positive dot with reference)
	for (int32 i = 0; i < NumNodes; ++i)
	{
		TestTrue(FString::Printf(TEXT("Tilted node %d: agrees with reference"), i),
		         FVector::DotProduct(NodeNormals[i], RefNormal) > 0.99);
	}

	return true;
}

// =============================================================================
// Cache Hash Tests
// =============================================================================

/**
 * Test ComputeProjectionHash for LocalTangent produces a stable hash
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentCacheHashTest,
	"PCGEx.Unit.Clusters.LocalTangent.CacheHash.Stable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentCacheHashTest::RunTest(const FString& Parameters)
{
	// LocalTangent hash should be method-only (no normal to hash)
	FPCGExGeo2DProjectionDetails Projection;
	Projection.Method = EPCGExProjectionMethod::LocalTangent;

	const uint32 Hash1 = PCGExClusters::FFaceEnumeratorCacheFactory::ComputeProjectionHash(Projection);
	const uint32 Hash2 = PCGExClusters::FFaceEnumeratorCacheFactory::ComputeProjectionHash(Projection);

	// Same input should produce same hash
	TestEqual(TEXT("LocalTangent hash is deterministic"), Hash1, Hash2);

	// Should be non-zero
	TestNotEqual(TEXT("LocalTangent hash is non-zero"), Hash1, 0u);

	return true;
}

/**
 * Test that LocalTangent hash differs from Normal hash
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentCacheHashDifferentTest,
	"PCGEx.Unit.Clusters.LocalTangent.CacheHash.DifferentFromNormal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentCacheHashDifferentTest::RunTest(const FString& Parameters)
{
	FPCGExGeo2DProjectionDetails ProjLocalTangent;
	ProjLocalTangent.Method = EPCGExProjectionMethod::LocalTangent;

	FPCGExGeo2DProjectionDetails ProjNormal;
	ProjNormal.Method = EPCGExProjectionMethod::Normal;

	FPCGExGeo2DProjectionDetails ProjBestFit;
	ProjBestFit.Method = EPCGExProjectionMethod::BestFit;

	const uint32 HashLT = PCGExClusters::FFaceEnumeratorCacheFactory::ComputeProjectionHash(ProjLocalTangent);
	const uint32 HashN = PCGExClusters::FFaceEnumeratorCacheFactory::ComputeProjectionHash(ProjNormal);
	const uint32 HashBF = PCGExClusters::FFaceEnumeratorCacheFactory::ComputeProjectionHash(ProjBestFit);

	// All three methods should produce different hashes
	TestNotEqual(TEXT("LocalTangent != Normal hash"), HashLT, HashN);
	TestNotEqual(TEXT("LocalTangent != BestFit hash"), HashLT, HashBF);

	return true;
}

/**
 * Test that LocalTangent hash is independent of the normal vector
 * (since tangent frames are per-node, the global projection normal is irrelevant)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentCacheHashNormalIndependentTest,
	"PCGEx.Unit.Clusters.LocalTangent.CacheHash.NormalIndependent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentCacheHashNormalIndependentTest::RunTest(const FString& Parameters)
{
	FPCGExGeo2DProjectionDetails Proj1;
	Proj1.Method = EPCGExProjectionMethod::LocalTangent;
	Proj1.Normal = FVector::UpVector;

	FPCGExGeo2DProjectionDetails Proj2;
	Proj2.Method = EPCGExProjectionMethod::LocalTangent;
	Proj2.Normal = FVector::ForwardVector;

	const uint32 Hash1 = PCGExClusters::FFaceEnumeratorCacheFactory::ComputeProjectionHash(Proj1);
	const uint32 Hash2 = PCGExClusters::FFaceEnumeratorCacheFactory::ComputeProjectionHash(Proj2);

	// LocalTangent hash should not depend on normal
	TestEqual(TEXT("LocalTangent hash same regardless of normal"), Hash1, Hash2);

	return true;
}

// =============================================================================
// FCachedTangentFrames Tests
// =============================================================================

/**
 * Test FCachedTangentFrames creation and access
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentCachedFramesTest,
	"PCGEx.Unit.Clusters.LocalTangent.CachedFrames.CreateAndAccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentCachedFramesTest::RunTest(const FString& Parameters)
{
	// Create tangent frames
	TSharedPtr<TArray<FQuat>> Frames = MakeShared<TArray<FQuat>>();
	Frames->Add(FQuat::Identity);
	Frames->Add(FQuat(FVector::UpVector, UE_HALF_PI));
	Frames->Add(FQuat(FVector::RightVector, UE_PI));

	// Create cached data
	TSharedPtr<PCGExClusters::FCachedTangentFrames> CachedFrames = MakeShared<PCGExClusters::FCachedTangentFrames>();
	CachedFrames->NodeTangentFrames = Frames;

	// Verify access
	TestNotNull(TEXT("NodeTangentFrames is set"), CachedFrames->NodeTangentFrames.Get());
	TestEqual(TEXT("Has 3 frames"), CachedFrames->NodeTangentFrames->Num(), 3);

	// Verify it inherits from ICachedClusterData
	TSharedPtr<PCGExClusters::ICachedClusterData> Base = CachedFrames;
	TestNotNull(TEXT("Can be cast to ICachedClusterData base"), Base.Get());

	return true;
}

// =============================================================================
// FPlanarFaceEnumerator Accessor Tests
// =============================================================================

/**
 * Test FPlanarFaceEnumerator default state
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentEnumeratorDefaultStateTest,
	"PCGEx.Unit.Clusters.LocalTangent.Enumerator.DefaultState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentEnumeratorDefaultStateTest::RunTest(const FString& Parameters)
{
	PCGExClusters::FPlanarFaceEnumerator Enumerator;

	TestFalse(TEXT("Default: not built"), Enumerator.IsBuilt());
	TestFalse(TEXT("Default: not local tangent"), Enumerator.IsLocalTangent());
	TestEqual(TEXT("Default: 0 half-edges"), Enumerator.GetNumHalfEdges(), 0);
	TestEqual(TEXT("Default: 0 faces"), Enumerator.GetNumFaces(), 0);
	TestNull(TEXT("Default: no cluster"), Enumerator.GetCluster());

	return true;
}

/**
 * Test that GetWrapperFaceIndex returns -1 when not built
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentEnumeratorWrapperNotBuiltTest,
	"PCGEx.Unit.Clusters.LocalTangent.Enumerator.WrapperNotBuilt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentEnumeratorWrapperNotBuiltTest::RunTest(const FString& Parameters)
{
	PCGExClusters::FPlanarFaceEnumerator Enumerator;
	TestEqual(TEXT("Wrapper face is -1 when not built"), Enumerator.GetWrapperFaceIndex(), -1);

	return true;
}

/**
 * Test that FindFaceContaining returns -1 when not built
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentEnumeratorFindFaceNotBuiltTest,
	"PCGEx.Unit.Clusters.LocalTangent.Enumerator.FindFaceNotBuilt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentEnumeratorFindFaceNotBuiltTest::RunTest(const FString& Parameters)
{
	PCGExClusters::FPlanarFaceEnumerator Enumerator;
	TestEqual(TEXT("FindFaceContaining returns -1 when not built"),
	          Enumerator.FindFaceContaining(FVector2D(50, 50)), -1);

	return true;
}

// =============================================================================
// Per-Face Projection Tests
// =============================================================================

/**
 * Test that per-face BestFitPlane + Init produces correct polygon projection
 * (This verifies the algorithm used in BuildCellFromFace for LocalTangent)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentPerFaceProjectionTest,
	"PCGEx.Unit.Clusters.LocalTangent.PerFace.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentPerFaceProjectionTest::RunTest(const FString& Parameters)
{
	// Simulate a face on a tilted plane (same as BuildCellFromFace LocalTangent path)
	// Face nodes form a square on a 45-degree tilted plane
	TArray<FVector> FaceNodes = {
		FVector(0, 0, 0),
		FVector(100, 0, 100),    // Tilted X
		FVector(100, 100, 100),  // Tilted X+Y
		FVector(0, 100, 0)       // Y only
	};

	// Compute BestFitPlane from face nodes (same as in BuildCellFromFace)
	PCGExMath::FBestFitPlane FacePlane(FaceNodes.Num(),
		[&](int32 i) { return FaceNodes[i]; });

	// Init projection from plane
	FPCGExGeo2DProjectionDetails FaceProjection;
	FaceProjection.Init(FacePlane);

	// Project each face node
	TArray<FVector2D> Polygon;
	Polygon.SetNumUninitialized(FaceNodes.Num());

	for (int32 i = 0; i < FaceNodes.Num(); ++i)
	{
		const FVector Projected = FaceProjection.Project(FaceNodes[i]);
		Polygon[i] = FVector2D(Projected.X, Projected.Y);
	}

	// All projected Z values should be near zero (points are on the plane)
	for (int32 i = 0; i < FaceNodes.Num(); ++i)
	{
		const FVector Projected = FaceProjection.Project(FaceNodes[i]);
		TestTrue(FString::Printf(TEXT("Node %d: projected Z ≈ 0"), i),
		         FMath::IsNearlyZero(Projected.Z, 1.0));
	}

	// Projected polygon should have meaningful area (not collapsed)
	double SignedArea = 0;
	for (int32 i = 0; i < Polygon.Num(); ++i)
	{
		const int32 j = (i + 1) % Polygon.Num();
		SignedArea += Polygon[i].X * Polygon[j].Y - Polygon[j].X * Polygon[i].Y;
	}
	const double Area = FMath::Abs(SignedArea) * 0.5;

	TestTrue(TEXT("Per-face polygon has non-zero area"), Area > 1.0);

	return true;
}

/**
 * Test per-face projection preserves area for a horizontal square face
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentPerFaceAreaPreservationTest,
	"PCGEx.Unit.Clusters.LocalTangent.PerFace.AreaPreservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentPerFaceAreaPreservationTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 1.0; // Allow some tolerance for projection rounding

	// Horizontal 100x100 square
	TArray<FVector> FaceNodes = {
		FVector(0, 0, 50),
		FVector(100, 0, 50),
		FVector(100, 100, 50),
		FVector(0, 100, 50)
	};

	PCGExMath::FBestFitPlane FacePlane(FaceNodes.Num(),
		[&](int32 i) { return FaceNodes[i]; });

	FPCGExGeo2DProjectionDetails FaceProjection;
	FaceProjection.Init(FacePlane);

	// Project nodes
	TArray<FVector2D> Polygon;
	Polygon.SetNumUninitialized(FaceNodes.Num());
	for (int32 i = 0; i < FaceNodes.Num(); ++i)
	{
		const FVector Projected = FaceProjection.Project(FaceNodes[i]);
		Polygon[i] = FVector2D(Projected.X, Projected.Y);
	}

	// Compute projected area
	double SignedArea = 0;
	for (int32 i = 0; i < Polygon.Num(); ++i)
	{
		const int32 j = (i + 1) % Polygon.Num();
		SignedArea += Polygon[i].X * Polygon[j].Y - Polygon[j].X * Polygon[i].Y;
	}
	const double ProjectedArea = FMath::Abs(SignedArea) * 0.5;

	// Expected area = 100 * 100 = 10000
	TestTrue(TEXT("Horizontal face: projected area ≈ 10000"),
	         FMath::IsNearlyEqual(ProjectedArea, 10000.0, Tolerance));

	return true;
}

/**
 * Test per-face projection for a vertical wall face
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentPerFaceVerticalWallTest,
	"PCGEx.Unit.Clusters.LocalTangent.PerFace.VerticalWall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentPerFaceVerticalWallTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 1.0;

	// Vertical wall in XZ plane (50x50 square)
	TArray<FVector> FaceNodes = {
		FVector(0, 0, 0),
		FVector(50, 0, 0),
		FVector(50, 0, 50),
		FVector(0, 0, 50)
	};

	PCGExMath::FBestFitPlane FacePlane(FaceNodes.Num(),
		[&](int32 i) { return FaceNodes[i]; });

	FPCGExGeo2DProjectionDetails FaceProjection;
	FaceProjection.Init(FacePlane);

	TArray<FVector2D> Polygon;
	Polygon.SetNumUninitialized(FaceNodes.Num());
	for (int32 i = 0; i < FaceNodes.Num(); ++i)
	{
		const FVector Projected = FaceProjection.Project(FaceNodes[i]);
		Polygon[i] = FVector2D(Projected.X, Projected.Y);
	}

	// Compute projected area
	double SignedArea = 0;
	for (int32 i = 0; i < Polygon.Num(); ++i)
	{
		const int32 j = (i + 1) % Polygon.Num();
		SignedArea += Polygon[i].X * Polygon[j].Y - Polygon[j].X * Polygon[i].Y;
	}
	const double ProjectedArea = FMath::Abs(SignedArea) * 0.5;

	// Expected area = 50 * 50 = 2500
	TestTrue(TEXT("Vertical wall: projected area ≈ 2500"),
	         FMath::IsNearlyEqual(ProjectedArea, 2500.0, Tolerance));

	return true;
}

// =============================================================================
// Half-Edge Angle Computation Tests (Local Tangent)
// =============================================================================

/**
 * Test that half-edge angles computed in a local tangent frame produce correct
 * angular ordering for DCEL construction.
 * Simulates the core of FPlanarFaceEnumerator::Build(TSharedPtr<TArray<FQuat>>)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentHalfEdgeAnglesTest,
	"PCGEx.Unit.Clusters.LocalTangent.HalfEdge.AnglesInLocalFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentHalfEdgeAnglesTest::RunTest(const FString& Parameters)
{
	// Simulate a node at origin on a horizontal surface (normal = Up)
	// With 4 neighbors at cardinal directions
	const FQuat Frame = FRotationMatrix::MakeFromZX(FVector::UpVector, FVector::ForwardVector).ToQuat();

	struct FTestEdge
	{
		FVector Direction;
		double ExpectedAngle; // Atan2(Y, X) of the direction in local frame
	};

	// In the local frame (Z=Up, X=Forward):
	// Forward → local X, so Atan2(0, 1) = 0
	// Right → local Y, so Atan2(1, 0) = PI/2
	// Back → local -X, so Atan2(0, -1) = PI
	// Left → local -Y, so Atan2(-1, 0) = -PI/2
	TArray<FTestEdge> TestEdges = {
		{FVector::ForwardVector, 0.0},
		{FVector::RightVector, UE_HALF_PI},
		{-FVector::ForwardVector, UE_PI},
		{-FVector::RightVector, -UE_HALF_PI},
	};

	const double Tolerance = 0.01;

	for (int32 i = 0; i < TestEdges.Num(); ++i)
	{
		const FVector LocalDir = Frame.UnrotateVector(TestEdges[i].Direction);
		const double Angle = FMath::Atan2(LocalDir.Y, LocalDir.X);

		TestTrue(FString::Printf(TEXT("Edge %d: angle ≈ expected"), i),
		         FMath::IsNearlyEqual(Angle, TestEdges[i].ExpectedAngle, Tolerance));
	}

	// Verify that sorting by angle gives CCW ordering: Forward, Right, Back, Left
	TArray<double> Angles;
	for (const FTestEdge& E : TestEdges)
	{
		const FVector LocalDir = Frame.UnrotateVector(E.Direction);
		Angles.Add(FMath::Atan2(LocalDir.Y, LocalDir.X));
	}

	Angles.Sort();

	// After sorting: -PI/2, 0, PI/2, PI  (Left, Forward, Right, Back)
	TestTrue(TEXT("Sorted: first is -PI/2 (Left)"),
	         FMath::IsNearlyEqual(Angles[0], -UE_HALF_PI, Tolerance));
	TestTrue(TEXT("Sorted: second is 0 (Forward)"),
	         FMath::IsNearlyEqual(Angles[1], 0.0, Tolerance));
	TestTrue(TEXT("Sorted: third is PI/2 (Right)"),
	         FMath::IsNearlyEqual(Angles[2], UE_HALF_PI, Tolerance));
	TestTrue(TEXT("Sorted: fourth is PI (Back)"),
	         FMath::IsNearlyEqual(Angles[3], UE_PI, Tolerance));

	return true;
}

/**
 * Test half-edge angles for a tilted tangent frame (sphere equator case)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentHalfEdgeTiltedFrameTest,
	"PCGEx.Unit.Clusters.LocalTangent.HalfEdge.TiltedFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentHalfEdgeTiltedFrameTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Simulate a node on a sphere equator (normal = ForwardVector)
	// This is the degeneracy case that triggered the adaptive X hint fix
	const FVector Normal = FVector::ForwardVector;
	const FVector XHint = FMath::Abs(FVector::DotProduct(Normal, FVector::ForwardVector)) < 0.95
		                      ? FVector::ForwardVector
		                      : FVector::RightVector;
	const FQuat Frame = FRotationMatrix::MakeFromZX(Normal, XHint).ToQuat();

	// Frame should be valid (non-degenerate)
	TestTrue(TEXT("Equator frame is normalized"), Frame.IsNormalized());

	// Test edges in the tangent plane of this node
	// The tangent plane is perpendicular to ForwardVector, i.e., the YZ plane
	const FVector EdgeUp = FVector::UpVector;       // In tangent plane
	const FVector EdgeRight = FVector::RightVector;  // In tangent plane

	const FVector LocalUp = Frame.UnrotateVector(EdgeUp);
	const FVector LocalRight = Frame.UnrotateVector(EdgeRight);

	// Both should have small Z component (tangent to surface = in XY of local frame)
	TestTrue(TEXT("EdgeUp local Z ≈ 0"),
	         FMath::IsNearlyZero(LocalUp.Z, Tolerance));
	TestTrue(TEXT("EdgeRight local Z ≈ 0"),
	         FMath::IsNearlyZero(LocalRight.Z, Tolerance));

	// Angles should be valid (finite, in [-PI, PI] range)
	const double AngleUp = FMath::Atan2(LocalUp.Y, LocalUp.X);
	const double AngleRight = FMath::Atan2(LocalRight.Y, LocalRight.X);

	TestTrue(TEXT("AngleUp is finite"), FMath::IsFinite(AngleUp));
	TestTrue(TEXT("AngleRight is finite"), FMath::IsFinite(AngleRight));

	// Angles should be different (Up and Right are perpendicular)
	TestTrue(TEXT("Up and Right have different angles"),
	         !FMath::IsNearlyEqual(AngleUp, AngleRight, 0.1));

	return true;
}

// =============================================================================
// Sphere Equator Regression Tests
// =============================================================================

/**
 * Test tangent frame construction for sphere nodes at various latitudes.
 * Validates that the adaptive X hint prevents degeneracy at the equator.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentSphereLatitudesTest,
	"PCGEx.Unit.Clusters.LocalTangent.Sphere.Latitudes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentSphereLatitudesTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	auto BuildTangentFrame = [](const FVector& N) -> FQuat
	{
		const FVector XHint = FMath::Abs(FVector::DotProduct(N, FVector::ForwardVector)) < 0.95
			                      ? FVector::ForwardVector
			                      : FVector::RightVector;
		return FRotationMatrix::MakeFromZX(N, XHint).ToQuat();
	};

	// Test normals at various latitudes on a sphere
	// Latitudes from pole (90) to equator (0) and past to other pole (-90)
	TArray<double> Latitudes = {90, 75, 60, 45, 30, 15, 5, 0, -5, -15, -30, -45, -60, -75, -90};

	for (double Latitude : Latitudes)
	{
		const double LatRad = FMath::DegreesToRadians(Latitude);
		const double LonRad = FMath::DegreesToRadians(45.0); // Arbitrary longitude

		// Sphere normal = position direction (for unit sphere)
		FVector Normal;
		Normal.X = FMath::Cos(LatRad) * FMath::Cos(LonRad);
		Normal.Y = FMath::Cos(LatRad) * FMath::Sin(LonRad);
		Normal.Z = FMath::Sin(LatRad);
		Normal.Normalize();

		const FQuat Frame = BuildTangentFrame(Normal);

		// Frame must be valid
		TestTrue(FString::Printf(TEXT("Lat %.0f: frame normalized"), Latitude),
		         Frame.IsNormalized());

		// Z axis must align with normal
		const FVector FrameZ = Frame.GetAxisZ();
		TestTrue(FString::Printf(TEXT("Lat %.0f: Z ≈ normal"), Latitude),
		         PCGExTest::NearlyEqual(FrameZ, Normal, Tolerance));

		// Axes must be orthogonal
		const FVector FrameX = Frame.GetAxisX();
		const FVector FrameY = Frame.GetAxisY();
		TestTrue(FString::Printf(TEXT("Lat %.0f: X·Z ≈ 0"), Latitude),
		         FMath::IsNearlyZero(FVector::DotProduct(FrameX, FrameZ), Tolerance));
		TestTrue(FString::Printf(TEXT("Lat %.0f: Y·Z ≈ 0"), Latitude),
		         FMath::IsNearlyZero(FVector::DotProduct(FrameY, FrameZ), Tolerance));
	}

	return true;
}

/**
 * Test tangent frame edge angle computation at sphere equator nodes.
 * Ensures DCEL angular ordering is correct even at the critical equator.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentSphereEquatorAnglesTest,
	"PCGEx.Unit.Clusters.LocalTangent.Sphere.EquatorAngles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentSphereEquatorAnglesTest::RunTest(const FString& Parameters)
{
	const double Radius = 100.0;

	auto BuildTangentFrame = [](const FVector& N) -> FQuat
	{
		const FVector XHint = FMath::Abs(FVector::DotProduct(N, FVector::ForwardVector)) < 0.95
			                      ? FVector::ForwardVector
			                      : FVector::RightVector;
		return FRotationMatrix::MakeFromZX(N, XHint).ToQuat();
	};

	// Test several equator nodes at different longitudes
	TArray<double> Longitudes = {0, 45, 90, 135, 180, 225, 270, 315};

	for (double Lon : Longitudes)
	{
		const double LonRad = FMath::DegreesToRadians(Lon);

		// Equator node position and normal
		const FVector Pos = FVector(FMath::Cos(LonRad), FMath::Sin(LonRad), 0) * Radius;
		const FVector Normal = Pos.GetSafeNormal();

		const FQuat Frame = BuildTangentFrame(Normal);

		// Create 4 neighbor directions (two along equator, two toward poles)
		const FVector ToNorthPole = (FVector(0, 0, Radius) - Pos).GetSafeNormal();
		const FVector ToSouthPole = (FVector(0, 0, -Radius) - Pos).GetSafeNormal();

		const double NextLonRad = FMath::DegreesToRadians(Lon + 30.0);
		const double PrevLonRad = FMath::DegreesToRadians(Lon - 30.0);
		const FVector ToNext = (FVector(FMath::Cos(NextLonRad), FMath::Sin(NextLonRad), 0) * Radius - Pos).GetSafeNormal();
		const FVector ToPrev = (FVector(FMath::Cos(PrevLonRad), FMath::Sin(PrevLonRad), 0) * Radius - Pos).GetSafeNormal();

		// Compute angles in local frame
		TArray<double> Angles;
		for (const FVector& Dir : {ToNorthPole, ToNext, ToSouthPole, ToPrev})
		{
			const FVector Local = Frame.UnrotateVector(Dir);
			const double A = FMath::Atan2(Local.Y, Local.X);
			Angles.Add(A);

			// Angle must be finite
			TestTrue(FString::Printf(TEXT("Lon %.0f: angle finite"), Lon), FMath::IsFinite(A));
		}

		// All 4 angles should be distinct
		bool bAllDistinct = true;
		for (int32 i = 0; i < Angles.Num() && bAllDistinct; ++i)
		{
			for (int32 j = i + 1; j < Angles.Num() && bAllDistinct; ++j)
			{
				if (FMath::IsNearlyEqual(Angles[i], Angles[j], 0.05))
				{
					bAllDistinct = false;
				}
			}
		}
		TestTrue(FString::Printf(TEXT("Lon %.0f: 4 distinct angles"), Lon), bAllDistinct);
	}

	return true;
}

// =============================================================================
// FCellConstraints Integration Tests
// =============================================================================

/**
 * Test FCellConstraints default state (relevant to LocalTangent wrapper handling)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentCellConstraintsDefaultTest,
	"PCGEx.Unit.Clusters.LocalTangent.CellConstraints.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentCellConstraintsDefaultTest::RunTest(const FString& Parameters)
{
	PCGExClusters::FCellConstraints Constraints;

	// Verify default state
	TestTrue(TEXT("Default bBuildWrapper is true"), Constraints.bBuildWrapper);
	TestTrue(TEXT("Default bKeepCellsWithLeaves is true"), Constraints.bKeepCellsWithLeaves);
	TestFalse(TEXT("Default bDuplicateLeafPoints is false"), Constraints.bDuplicateLeafPoints);
	TestFalse(TEXT("Default bConcaveOnly is false"), Constraints.bConcaveOnly);
	TestFalse(TEXT("Default bConvexOnly is false"), Constraints.bConvexOnly);
	TestEqual(TEXT("Default MaxPointCount is MAX_int32"), Constraints.MaxPointCount, MAX_int32);
	TestTrue(TEXT("Default WrapperCell is null"), !Constraints.WrapperCell.IsValid());
	TestTrue(TEXT("Default Holes is null"), !Constraints.Holes.IsValid());
	TestTrue(TEXT("Default Enumerator is null"), !Constraints.Enumerator.IsValid());

	return true;
}

// =============================================================================
// Cell Containment Tests — OverlapsPolygonLocal Algorithm
// =============================================================================

/**
 * Helper: Build a face projection and polygon from 3D face nodes.
 * Mirrors the BuildCellFromFace LocalTangent path.
 */
namespace LocalTangentTestHelpers
{
	struct FFaceData
	{
		FPCGExGeo2DProjectionDetails Projection;
		TArray<FVector2D> Polygon;
		FBox2D Bounds2D;
		FBox Bounds3D;

		void BuildFromNodes(const TArray<FVector>& FaceNodes)
		{
			PCGExMath::FBestFitPlane FacePlane(FaceNodes.Num(),
				[&](int32 i) { return FaceNodes[i]; });
			Projection.Init(FacePlane);

			const int32 Num = FaceNodes.Num();
			Polygon.SetNumUninitialized(Num);
			Bounds2D = FBox2D(ForceInit);
			Bounds3D = FBox(ForceInit);

			for (int32 i = 0; i < Num; ++i)
			{
				const FVector Proj = Projection.Project(FaceNodes[i]);
				Polygon[i] = FVector2D(Proj.X, Proj.Y);
				Bounds2D += Polygon[i];
				Bounds3D += FaceNodes[i];
			}
		}
	};

	/**
	 * Simulates the OverlapsPolygonLocal algorithm without requiring FFacade.
	 * Follows the exact same logic as FProjectedPointSet::OverlapsPolygonLocal.
	 */
	bool SimulateOverlapsPolygonLocal(
		const TArray<FVector>& HolePositions,
		const FBox& HoleBounds3D,
		const FFaceData& Face)
	{
		// Step 1: Coarse 3D AABB culling
		if (!HoleBounds3D.Intersect(Face.Bounds3D))
		{
			return false;
		}

		// Step 2: Per-point face-local projection + containment
		for (const FVector& HolePos : HolePositions)
		{
			const FVector Projected = Face.Projection.Project(HolePos);
			const FVector2D Point2D(Projected.X, Projected.Y);

			if (!Face.Bounds2D.IsInside(Point2D)) { continue; }
			if (PCGExMath::Geo::IsPointInPolygon(Point2D, Face.Polygon)) { return true; }
		}

		return false;
	}

	FBox ComputeBounds3D(const TArray<FVector>& Points)
	{
		FBox Bounds(ForceInit);
		for (const FVector& P : Points) { Bounds += P; }
		return Bounds;
	}
}

/**
 * Test: Hole point directly on a face is detected as overlapping
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainmentHoleOnFaceTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.HoleOnFace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainmentHoleOnFaceTest::RunTest(const FString& Parameters)
{
	using namespace LocalTangentTestHelpers;

	// Horizontal square face at Z=0
	TArray<FVector> FaceNodes = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};

	FFaceData Face;
	Face.BuildFromNodes(FaceNodes);

	// Hole point at the center of the face
	TArray<FVector> HolePoints = { FVector(50, 50, 0) };
	FBox HoleBounds = ComputeBounds3D(HolePoints);

	TestTrue(TEXT("Hole at face center is detected"),
	         SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, Face));

	return true;
}

/**
 * Test: Hole point far away from face is culled by 3D AABB
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainmentHoleFarAwayTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.HoleFarAway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainmentHoleFarAwayTest::RunTest(const FString& Parameters)
{
	using namespace LocalTangentTestHelpers;

	// Face at origin
	TArray<FVector> FaceNodes = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};

	FFaceData Face;
	Face.BuildFromNodes(FaceNodes);

	// Hole point very far away (3D AABB won't intersect)
	TArray<FVector> HolePoints = { FVector(10000, 10000, 10000) };
	FBox HoleBounds = ComputeBounds3D(HolePoints);

	TestFalse(TEXT("Hole far away is culled by 3D AABB"),
	          SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, Face));

	return true;
}

/**
 * Test: Hole point near face in 3D but outside polygon in 2D
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainmentHoleNearButOutsideTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.HoleNearButOutside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainmentHoleNearButOutsideTest::RunTest(const FString& Parameters)
{
	using namespace LocalTangentTestHelpers;

	// Face: small triangle
	TArray<FVector> FaceNodes = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(50, 100, 0)
	};

	FFaceData Face;
	Face.BuildFromNodes(FaceNodes);

	// Hole point: passes 3D AABB check but is outside the triangle polygon
	// Place it at a corner area that's within the face's bounding box but outside the triangle
	TArray<FVector> HolePoints = { FVector(90, 90, 0) };
	FBox HoleBounds = ComputeBounds3D(HolePoints);

	TestFalse(TEXT("Hole near face but outside polygon is not detected"),
	          SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, Face));

	return true;
}

/**
 * Test: Hole point above face plane but projecting into polygon
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainmentHoleAboveFaceTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.HoleAboveFace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainmentHoleAboveFaceTest::RunTest(const FString& Parameters)
{
	using namespace LocalTangentTestHelpers;

	// Horizontal face
	TArray<FVector> FaceNodes = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};

	FFaceData Face;
	Face.BuildFromNodes(FaceNodes);

	// Hole point directly above face center — 3D bounds overlap (face Z=0, hole Z=5 is within expanded bounds)
	// However, the face's 3D bounds only span Z=0 to Z=0, so a point at Z=5 won't intersect the face bounds
	// The 3D AABB check is checking if the two bounding boxes intersect
	{
		TArray<FVector> HolePoints = { FVector(50, 50, 5) };
		FBox HoleBounds = ComputeBounds3D(HolePoints);
		// Face bounds: Z in [0, 0]. Hole bounds: Z in [5, 5]. No intersection.
		TestFalse(TEXT("Hole 5 units above flat face is culled (zero-thickness Z)"),
		          SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, Face));
	}

	// Now use a face with some Z-thickness
	TArray<FVector> ThickFaceNodes = {
		FVector(0, 0, 0),
		FVector(100, 0, 10),    // Slight tilt
		FVector(100, 100, 10),
		FVector(0, 100, 0)
	};

	FFaceData ThickFace;
	ThickFace.BuildFromNodes(ThickFaceNodes);

	// Hole point at Z=5 — within the 3D bounds [0, 10]
	{
		TArray<FVector> HolePoints = { FVector(50, 50, 5) };
		FBox HoleBounds = ComputeBounds3D(HolePoints);

		// The point is within 3D bounds, and when projected to face-local 2D
		// it will land near the center of the tilted quad
		TestTrue(TEXT("Hole within thick face 3D bounds and inside polygon"),
		         SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, ThickFace));
	}

	return true;
}

/**
 * Test: Multiple hole points, only one is inside the face polygon
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainmentMultipleHolesTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.MultipleHoles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainmentMultipleHolesTest::RunTest(const FString& Parameters)
{
	using namespace LocalTangentTestHelpers;

	// Face: tilted square
	TArray<FVector> FaceNodes = {
		FVector(0, 0, 0),
		FVector(100, 0, 50),
		FVector(100, 100, 50),
		FVector(0, 100, 0)
	};

	FFaceData Face;
	Face.BuildFromNodes(FaceNodes);

	// Mix of inside and outside hole points
	TArray<FVector> HolePoints = {
		FVector(-500, -500, -500),  // Far away
		FVector(200, 200, 200),     // Far away other side
		FVector(50, 50, 25),        // Near center of tilted face — should be inside
	};
	FBox HoleBounds = ComputeBounds3D(HolePoints);

	TestTrue(TEXT("At least one hole is inside the face"),
	         SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, Face));

	return true;
}

/**
 * Test: No hole points are inside the face
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainmentNoHolesInsideTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.NoHolesInside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainmentNoHolesInsideTest::RunTest(const FString& Parameters)
{
	using namespace LocalTangentTestHelpers;

	// Face
	TArray<FVector> FaceNodes = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};

	FFaceData Face;
	Face.BuildFromNodes(FaceNodes);

	// All hole points outside — but within 3D AABB (they're coplanar but outside polygon)
	TArray<FVector> HolePoints = {
		FVector(-10, -10, 0),
		FVector(110, 50, 0),
		FVector(50, 110, 0),
		FVector(-5, 50, 0),
	};
	FBox HoleBounds = ComputeBounds3D(HolePoints);

	TestFalse(TEXT("No holes inside face polygon"),
	          SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, Face));

	return true;
}

// =============================================================================
// Multi-Face Disambiguation Tests
// =============================================================================

/**
 * Test: A hole point in face A's space should NOT be detected in face B's space
 * when the faces are on different planes of a non-planar surface.
 * This validates the per-face local projection approach.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainmentMultiFaceDisambiguationTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.MultiFaceDisambiguation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainmentMultiFaceDisambiguationTest::RunTest(const FString& Parameters)
{
	using namespace LocalTangentTestHelpers;

	// Face A: horizontal at Z=0 (top face of a cube)
	TArray<FVector> FaceANodes = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};

	// Face B: vertical wall at Y=0 (front face of a cube)
	TArray<FVector> FaceBNodes = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 0, -100),
		FVector(0, 0, -100)
	};

	FFaceData FaceA, FaceB;
	FaceA.BuildFromNodes(FaceANodes);
	FaceB.BuildFromNodes(FaceBNodes);

	// Hole point ON face A (center of the horizontal face)
	TArray<FVector> HoleOnA = { FVector(50, 50, 0) };
	FBox HoleBoundsA = ComputeBounds3D(HoleOnA);

	// Should be inside face A
	TestTrue(TEXT("Hole on face A is detected in face A"),
	         SimulateOverlapsPolygonLocal(HoleOnA, HoleBoundsA, FaceA));

	// Should NOT be inside face B (different plane)
	// The point is at Y=50 while face B is at Y=0, so 3D AABB culling handles this
	TestFalse(TEXT("Hole on face A is NOT detected in face B"),
	          SimulateOverlapsPolygonLocal(HoleOnA, HoleBoundsA, FaceB));

	return true;
}

/**
 * Test: Two adjacent faces sharing an edge — a hole on one face doesn't leak into the other.
 * Uses faces that share a common edge but are at 90 degrees.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainmentAdjacentFacesTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.AdjacentFaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainmentAdjacentFacesTest::RunTest(const FString& Parameters)
{
	using namespace LocalTangentTestHelpers;

	// Two faces sharing the edge at X=50:
	// Face A: horizontal quad [0,50]x[0,100] at Z=0
	TArray<FVector> FaceANodes = {
		FVector(0, 0, 0),
		FVector(50, 0, 0),
		FVector(50, 100, 0),
		FVector(0, 100, 0)
	};

	// Face B: vertical quad at X=50, going down
	TArray<FVector> FaceBNodes = {
		FVector(50, 0, 0),
		FVector(100, 0, -50),
		FVector(100, 100, -50),
		FVector(50, 100, 0)
	};

	FFaceData FaceA, FaceB;
	FaceA.BuildFromNodes(FaceANodes);
	FaceB.BuildFromNodes(FaceBNodes);

	// Hole deep inside face A (far from shared edge)
	TArray<FVector> HoleInA = { FVector(10, 50, 0) };
	FBox HoleBoundsA = ComputeBounds3D(HoleInA);

	TestTrue(TEXT("Hole inside face A is detected"),
	         SimulateOverlapsPolygonLocal(HoleInA, HoleBoundsA, FaceA));

	// Same hole should NOT match face B
	TestFalse(TEXT("Hole inside face A does NOT match face B"),
	          SimulateOverlapsPolygonLocal(HoleInA, HoleBoundsA, FaceB));

	return true;
}

// =============================================================================
// Tilted Face Containment Tests (Non-Planar Surface)
// =============================================================================

/**
 * Test containment for a tilted face (simulates a face on a curved surface)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainmentTiltedFaceTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.TiltedFace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainmentTiltedFaceTest::RunTest(const FString& Parameters)
{
	using namespace LocalTangentTestHelpers;

	// 45-degree tilted face (like a face on a sphere)
	TArray<FVector> FaceNodes = {
		FVector(0, 0, 0),
		FVector(70.71, 0, 70.71),      // 100 units along 45-degree tilt in XZ
		FVector(70.71, 100, 70.71),
		FVector(0, 100, 0)
	};

	FFaceData Face;
	Face.BuildFromNodes(FaceNodes);

	// Hole point at the center of the tilted face
	FVector FaceCenter = (FaceNodes[0] + FaceNodes[1] + FaceNodes[2] + FaceNodes[3]) / 4.0;
	TArray<FVector> HolePoints = { FaceCenter };
	FBox HoleBounds = ComputeBounds3D(HolePoints);

	TestTrue(TEXT("Hole at tilted face center is detected"),
	         SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, Face));

	return true;
}

/**
 * Test containment on a vertical face (forward-facing wall)
 * This is the degeneracy case for projection.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainmentVerticalFaceTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.VerticalFace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainmentVerticalFaceTest::RunTest(const FString& Parameters)
{
	using namespace LocalTangentTestHelpers;

	// Vertical face in YZ plane (normal ≈ ForwardVector, triggers adaptive hint)
	TArray<FVector> FaceNodes = {
		FVector(0, 0, 0),
		FVector(0, 100, 0),
		FVector(0, 100, 100),
		FVector(0, 0, 100)
	};

	FFaceData Face;
	Face.BuildFromNodes(FaceNodes);

	// Hole at center of vertical face
	{
		TArray<FVector> HolePoints = { FVector(0, 50, 50) };
		FBox HoleBounds = ComputeBounds3D(HolePoints);

		TestTrue(TEXT("Hole at vertical face center is detected"),
		         SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, Face));
	}

	// Hole behind the vertical face (different X)
	{
		TArray<FVector> HolePoints = { FVector(50, 50, 50) };
		FBox HoleBounds = ComputeBounds3D(HolePoints);

		// 3D AABB: face X=[0,0], hole X=[50,50] — no intersection
		TestFalse(TEXT("Hole behind vertical face is culled"),
		          SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, Face));
	}

	return true;
}

/**
 * Test: Sphere-like configuration with 3 faces meeting at different angles.
 * Hole point on one face should only match that face.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainmentSphereFacesTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.SphereFaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainmentSphereFacesTest::RunTest(const FString& Parameters)
{
	using namespace LocalTangentTestHelpers;

	const double R = 100.0;

	// Three faces simulating an octant of a sphere:
	// Face 1: top-front (normal ≈ (1,0,1)/sqrt(2))
	TArray<FVector> Face1Nodes = {
		FVector(R, 0, 0),
		FVector(0, 0, R),
		FVector(0, R, 0)
	};

	// Face 2: top-right (normal ≈ (0,1,1)/sqrt(2))
	TArray<FVector> Face2Nodes = {
		FVector(0, R, 0),
		FVector(0, 0, R),
		FVector(-R, 0, 0)
	};

	// Face 3: right-front (normal ≈ (1,1,0)/sqrt(2))
	TArray<FVector> Face3Nodes = {
		FVector(R, 0, 0),
		FVector(0, R, 0),
		FVector(0, 0, -R)
	};

	FFaceData Face1, Face2, Face3;
	Face1.BuildFromNodes(Face1Nodes);
	Face2.BuildFromNodes(Face2Nodes);
	Face3.BuildFromNodes(Face3Nodes);

	// Hole at centroid of Face 1
	FVector HolePos = (Face1Nodes[0] + Face1Nodes[1] + Face1Nodes[2]) / 3.0;
	TArray<FVector> HolePoints = { HolePos };
	FBox HoleBounds = ComputeBounds3D(HolePoints);

	// Test against each face
	bool bInFace1 = SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, Face1);
	bool bInFace2 = SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, Face2);
	bool bInFace3 = SimulateOverlapsPolygonLocal(HolePoints, HoleBounds, Face3);

	TestTrue(TEXT("Hole at Face1 centroid is in Face1"), bInFace1);

	// It shouldn't be in both other faces (it might match one due to 3D AABB overlap
	// but the fine-grained polygon test should reject it in most cases)
	// At minimum, we test that it's not simultaneously in all three
	const int32 MatchCount = (bInFace1 ? 1 : 0) + (bInFace2 ? 1 : 0) + (bInFace3 ? 1 : 0);
	TestTrue(TEXT("Hole matches at most 2 faces (not all 3)"), MatchCount < 3);

	AddInfo(FString::Printf(TEXT("Face matches: Face1=%d Face2=%d Face3=%d"),
	                         bInFace1, bInFace2, bInFace3));

	return true;
}

// =============================================================================
// AABB Early-Out Tests
// =============================================================================

/**
 * Test 3D AABB intersection logic used in OverlapsPolygonLocal
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainmentAABBTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.AABBEarlyOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainmentAABBTest::RunTest(const FString& Parameters)
{
	// Overlapping boxes
	{
		FBox A(FVector(0, 0, 0), FVector(100, 100, 100));
		FBox B(FVector(50, 50, 50), FVector(150, 150, 150));
		TestTrue(TEXT("Overlapping 3D boxes intersect"), A.Intersect(B));
	}

	// Non-overlapping boxes
	{
		FBox A(FVector(0, 0, 0), FVector(100, 100, 100));
		FBox B(FVector(200, 200, 200), FVector(300, 300, 300));
		TestFalse(TEXT("Non-overlapping 3D boxes don't intersect"), A.Intersect(B));
	}

	// Touching at corner (degenerate)
	{
		FBox A(FVector(0, 0, 0), FVector(100, 100, 100));
		FBox B(FVector(100, 100, 100), FVector(200, 200, 200));
		// Touching at a single point — Intersect should return true (per UE convention)
		TestTrue(TEXT("Boxes touching at corner intersect"), A.Intersect(B));
	}

	// Zero-volume box (flat face at Z=0) vs volume box
	{
		FBox FlatBox(FVector(0, 0, 0), FVector(100, 100, 0));
		FBox VolumeBox(FVector(50, 50, -10), FVector(150, 150, 10));
		TestTrue(TEXT("Flat box intersects volume box that spans its Z"), FlatBox.Intersect(VolumeBox));
	}

	// Flat face vs point above (no intersection)
	{
		FBox FlatBox(FVector(0, 0, 0), FVector(100, 100, 0));
		FBox PointBox(FVector(50, 50, 5), FVector(50, 50, 5));
		TestFalse(TEXT("Flat box doesn't intersect point above it"), FlatBox.Intersect(PointBox));
	}

	return true;
}

/**
 * Test 2D bounds check used in OverlapsPolygonLocal (medium-grained culling)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLocalTangentContainment2DBoundsTest,
	"PCGEx.Unit.Clusters.LocalTangent.Containment.Bounds2DEarlyOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExLocalTangentContainment2DBoundsTest::RunTest(const FString& Parameters)
{
	FBox2D Bounds(FVector2D(0, 0), FVector2D(100, 100));

	TestTrue(TEXT("Center is inside 2D bounds"), Bounds.IsInside(FVector2D(50, 50)));
	TestFalse(TEXT("Min corner on boundary is not strictly inside"), Bounds.IsInside(FVector2D(0, 0)));
	TestTrue(TEXT("Point just inside min corner is inside"), Bounds.IsInside(FVector2D(0.01, 0.01)));
	TestFalse(TEXT("Point outside is not inside 2D bounds"), Bounds.IsInside(FVector2D(150, 50)));
	TestFalse(TEXT("Negative point is outside 2D bounds"), Bounds.IsInside(FVector2D(-10, 50)));

	return true;
}
