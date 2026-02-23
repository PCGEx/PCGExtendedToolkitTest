// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExChainTestHelpers.h"
#include "Algo/RemoveIf.h"
#include "PCGExH.h"

namespace PCGExTest
{
	using PCGExGraphs::FLink;

#pragma region FTestChain

	void FTestChain::FixUniqueHash()
	{
		UniqueHash = 0;

		if (Links.Num() <= 1)
		{
			SingleEdge = Seed.Edge;
			UniqueHash = SingleEdge;
			return;
		}

		const FLink LastLink = Links.Last();
		UniqueHash = PCGEx::H64U(HashCombineFast(Seed.Node, Seed.Edge), HashCombineFast(LastLink.Node, LastLink.Edge));
	}

	void FTestChain::BuildChain(const TSharedRef<FTestCluster>& Cluster, const TSharedPtr<TArray<int8>>& Breakpoints)
	{
		// Note: No TSet needed for cycle detection. Binary nodes have exactly 2 neighbors,
		// so walking through them can't revisit nodes - we came from one, go to the other.
		// Closed loops are detected by checking if we return to Seed.Node.

		FLink Last = Seed;
		PCGExClusters::FNode* FromNode = Cluster->GetEdgeOtherNode(Seed);
		Links.Add(FLink(FromNode->Index, Seed.Edge));

		while (FromNode)
		{
			if (FromNode->IsLeaf() || FromNode->IsComplex() || (Breakpoints && (*Breakpoints)[FromNode->PointIndex]))
			{
				bIsClosedLoop = false;
				break;
			}

			FLink NextLink = FromNode->Links[0];
			if (NextLink.Node == Last.Node) { NextLink = FromNode->Links[1]; }

			if (NextLink.Node == Seed.Node)
			{
				Seed.Edge = NextLink.Edge;
				bIsClosedLoop = true;
				break;
			}

			Last = Links.Last();
			Links.Add(NextLink);

			FromNode = Cluster->GetNode(NextLink.Node);
		}

		// Set leaf status
		bIsLeaf = Cluster->GetNode(Seed.Node)->IsLeaf() || Cluster->GetNode(Links.Last().Node)->IsLeaf();
		if (bIsClosedLoop) { bIsLeaf = false; }
		FixUniqueHash();
	}

	void FTestChain::GetNodeIndices(TArray<int32>& OutIndices, const bool bReverse) const
	{
		if (SingleEdge != -1)
		{
			OutIndices.Reset(2);
			if (bReverse)
			{
				OutIndices.Add(Links[0].Node);
				OutIndices.Add(Seed.Node);
			}
			else
			{
				OutIndices.Add(Seed.Node);
				OutIndices.Add(Links[0].Node);
			}
			return;
		}

		const int32 ChainSize = Links.Num();
		OutIndices.Reset(ChainSize + 1);

		if (bReverse)
		{
			for (int i = ChainSize - 1; i >= 0; i--) { OutIndices.Add(Links[i].Node); }
			OutIndices.Add(Seed.Node);
		}
		else
		{
			OutIndices.Add(Seed.Node);
			for (int i = 0; i < ChainSize; i++) { OutIndices.Add(Links[i].Node); }
		}
	}

#pragma endregion

#pragma region TestChainHelpers

	namespace TestChainHelpers
	{
		bool BuildChains(
			const TSharedRef<FTestCluster>& Cluster,
			TArray<TSharedPtr<FTestChain>>& OutChains,
			const TSharedPtr<TArray<int8>>& Breakpoints)
		{
			OutChains.Reset();
			OutChains.Reserve(Cluster->Edges->Num());

			int32 NumBinaries = 0;
			const int32 NumNodes = Cluster->Nodes->Num();

			for (int32 i = 0; i < NumNodes; i++)
			{
				PCGExClusters::FNode* Node = Cluster->GetNode(i);
				if (!Node || Node->IsEmpty()) { continue; }

				if (Node->IsLeaf())
				{
					OutChains.Add(MakeShared<FTestChain>(FLink(Node->Index, Node->Links[0].Edge)));
					continue;
				}

				if (Node->IsBinary())
				{
					NumBinaries++;
					continue;
				}

				for (const FLink& Lk : Node->Links)
				{
					// Skip immediately known leaves to avoid double-sampling
					if (Cluster->GetNode(Lk.Node)->IsLeaf()) { continue; }
					OutChains.Add(MakeShared<FTestChain>(FLink(Node->Index, Lk.Edge)));
				}
			}

			OutChains.Shrink();

			if (OutChains.IsEmpty())
			{
				if (NumBinaries > 0 && NumBinaries == NumNodes)
				{
					// Isolated closed loop - all nodes are binary
					OutChains.Add(MakeShared<FTestChain>(Cluster->GetNode(0)->Links[0]));
				}
				else
				{
					return false;
				}
			}

			// Build each chain
			for (const TSharedPtr<FTestChain>& Chain : OutChains)
			{
				Chain->BuildChain(Cluster, nullptr); // Build without breakpoints first
			}

			// Deduplicate based on unique hash
			TSet<uint64> UniqueHashSet;
			UniqueHashSet.Reserve(OutChains.Num());

			OutChains.SetNum(Algo::StableRemoveIf(
				OutChains,
				[&UniqueHashSet](const TSharedPtr<FTestChain>& Chain)
				{
					bool bAlreadySet = false;
					UniqueHashSet.Add(Chain->UniqueHash, &bAlreadySet);
					return bAlreadySet;
				}));

			// Apply breakpoints if provided
			if (Breakpoints && !Breakpoints->IsEmpty())
			{
				TArray<TSharedPtr<FTestChain>> SplitChains;
				ApplyBreakpoints(OutChains, Cluster, Breakpoints, SplitChains);
				OutChains = MoveTemp(SplitChains);
			}

			return !OutChains.IsEmpty();
		}

		void ApplyBreakpoints(
			const TArray<TSharedPtr<FTestChain>>& SourceChains,
			const TSharedRef<FTestCluster>& Cluster,
			const TSharedPtr<TArray<int8>>& Breakpoints,
			TArray<TSharedPtr<FTestChain>>& OutChains)
		{
			if (!Breakpoints || Breakpoints->IsEmpty())
			{
				OutChains = SourceChains;
				return;
			}

			OutChains.Reset();
			OutChains.Reserve(SourceChains.Num() * 2);

			const TArray<int8>& BreakpointsRef = *Breakpoints;

			for (const TSharedPtr<FTestChain>& SourceChain : SourceChains)
			{
				if (!SourceChain) { continue; }

				// Single edge chains can't be split - pass through as-is
				if (SourceChain->SingleEdge != -1)
				{
					OutChains.Add(SourceChain);
					continue;
				}

				// For closed loops, extend the walk to include the wrap-around back to the seed.
				// The Links array doesn't contain the seed node — the closure is implicit via bIsClosedLoop.
				TArray<FLink> ExtendedLinks;
				const TArray<FLink>& WalkLinks = [&]() -> const TArray<FLink>&
				{
					if (SourceChain->bIsClosedLoop)
					{
						ExtendedLinks = SourceChain->Links;
						ExtendedLinks.Add(FLink(SourceChain->Seed.Node, SourceChain->Seed.Edge));
						return ExtendedLinks;
					}
					return SourceChain->Links;
				}();

				// Walk through the chain and split at breakpoints
				TArray<FLink> CurrentSegmentLinks;
				CurrentSegmentLinks.Reserve(WalkLinks.Num());

				int32 SegmentSeedNode = SourceChain->Seed.Node;

				// For closed loops, Seed.Edge was overwritten with the closing edge during BuildChain.
				// The first segment needs the original edge from seed to first link (Links[0].Edge).
				int32 SegmentSeedEdge = (SourceChain->bIsClosedLoop && !SourceChain->Links.IsEmpty())
				                        ? SourceChain->Links[0].Edge
				                        : SourceChain->Seed.Edge;

				const int32 OriginalSeedPI = Cluster->GetNodePointIndex(SourceChain->Seed.Node);
				const bool bOriginalSeedIsBreakpoint = BreakpointsRef.IsValidIndex(OriginalSeedPI) && BreakpointsRef[OriginalSeedPI];
				bool bSegmentStartIsBreakpoint = bOriginalSeedIsBreakpoint;

				// For closed loops where the seed is NOT a breakpoint, merge the first and last
				// emitted segments to rejoin the chain across the arbitrary seed split point.
				const bool bNeedsMerge = SourceChain->bIsClosedLoop && !bOriginalSeedIsBreakpoint;
				int32 FirstEmittedIndex = -1;

				for (int32 i = 0; i < WalkLinks.Num(); i++)
				{
					const FLink& Link = WalkLinks[i];
					const int32 NodePointIndex = Cluster->GetNodePointIndex(Link.Node);
					const bool bIsBreakpoint = BreakpointsRef.IsValidIndex(NodePointIndex) && BreakpointsRef[NodePointIndex];

					if (bIsBreakpoint)
					{
						// Include this link in the current segment (chain goes TO the breakpoint)
						CurrentSegmentLinks.Add(Link);

						// Emit current segment
						TSharedPtr<FTestChain> NewChain = MakeShared<FTestChain>(FLink(SegmentSeedNode, SegmentSeedEdge));
						NewChain->Links = MoveTemp(CurrentSegmentLinks);
						NewChain->bIsClosedLoop = false;

						// Determine if this segment is a leaf chain (topology only)
						const PCGExClusters::FNode* StartNode = Cluster->GetNode(SegmentSeedNode);
						const PCGExClusters::FNode* EndNode = Cluster->GetNode(NewChain->Links.Last().Node);
						NewChain->bIsLeaf = (StartNode && StartNode->IsLeaf()) || (EndNode && EndNode->IsLeaf());

						NewChain->FixUniqueHash();

						if (FirstEmittedIndex == -1) { FirstEmittedIndex = OutChains.Num(); }
						OutChains.Add(NewChain);

						CurrentSegmentLinks.Reset();
						CurrentSegmentLinks.Reserve(WalkLinks.Num() - i);

						// Start new segment from breakpoint node
						SegmentSeedNode = Link.Node;
						SegmentSeedEdge = (i + 1 < WalkLinks.Num()) ? WalkLinks[i + 1].Edge : Link.Edge;
						bSegmentStartIsBreakpoint = true;
					}
					else
					{
						CurrentSegmentLinks.Add(Link);
					}
				}

				// Emit final segment
				if (!CurrentSegmentLinks.IsEmpty())
				{
					if (bNeedsMerge && FirstEmittedIndex >= 0)
					{
						// Merge last + first segments across the arbitrary seed node.
						TSharedPtr<FTestChain>& FirstSeg = OutChains[FirstEmittedIndex];
						CurrentSegmentLinks.Append(FirstSeg->Links);

						TSharedPtr<FTestChain> MergedChain = MakeShared<FTestChain>(FLink(SegmentSeedNode, SegmentSeedEdge));
						MergedChain->Links = MoveTemp(CurrentSegmentLinks);
						MergedChain->bIsClosedLoop = false;

						const PCGExClusters::FNode* StartNode = Cluster->GetNode(SegmentSeedNode);
						const PCGExClusters::FNode* EndNode = Cluster->GetNode(MergedChain->Links.Last().Node);
						MergedChain->bIsLeaf = (StartNode && StartNode->IsLeaf()) || (EndNode && EndNode->IsLeaf());

						MergedChain->FixUniqueHash();
						OutChains[FirstEmittedIndex] = MergedChain;
					}
					else if (bNeedsMerge)
					{
						// Closed loop with no breakpoints in any link — pass through unchanged
						OutChains.Add(SourceChain);
					}
					else
					{
						TSharedPtr<FTestChain> NewChain = MakeShared<FTestChain>(FLink(SegmentSeedNode, SegmentSeedEdge));
						NewChain->Links = MoveTemp(CurrentSegmentLinks);

						// Check for closed loop (only if source was a closed loop and no breakpoints hit)
						NewChain->bIsClosedLoop = SourceChain->bIsClosedLoop &&
						                          SegmentSeedNode == SourceChain->Seed.Node &&
						                          !bSegmentStartIsBreakpoint;

						// Determine if this is a leaf chain (topology only)
						const PCGExClusters::FNode* StartNode = Cluster->GetNode(SegmentSeedNode);
						const PCGExClusters::FNode* EndNode = Cluster->GetNode(NewChain->Links.Last().Node);
						NewChain->bIsLeaf = (StartNode && StartNode->IsLeaf()) || (EndNode && EndNode->IsLeaf());

						if (NewChain->bIsClosedLoop) { NewChain->bIsLeaf = false; }

						NewChain->FixUniqueHash();
						OutChains.Add(NewChain);
					}
				}
			}

			// Deduplicate results
			TSet<uint64> UniqueHashSet;
			UniqueHashSet.Reserve(OutChains.Num());

			OutChains.SetNum(Algo::StableRemoveIf(
				OutChains,
				[&UniqueHashSet](const TSharedPtr<FTestChain>& Chain)
				{
					if (!Chain || Chain->Links.IsEmpty()) { return true; }
					bool bAlreadySet = false;
					UniqueHashSet.Add(Chain->UniqueHash, &bAlreadySet);
					return bAlreadySet;
				}));
		}

		void FilterLeavesOnly(
			const TArray<TSharedPtr<FTestChain>>& SourceChains,
			TArray<TSharedPtr<FTestChain>>& OutChains)
		{
			if (&SourceChains == &OutChains)
			{
				// In-place filter
				OutChains.SetNum(Algo::StableRemoveIf(
					OutChains,
					[](const TSharedPtr<FTestChain>& Chain) { return !Chain || !Chain->bIsLeaf; }));
			}
			else
			{
				OutChains.Reset();
				OutChains.Reserve(SourceChains.Num());

				for (const TSharedPtr<FTestChain>& Chain : SourceChains)
				{
					if (Chain && Chain->bIsLeaf)
					{
						OutChains.Add(Chain);
					}
				}
			}
		}

		int32 CountLeafChains(const TArray<TSharedPtr<FTestChain>>& Chains)
		{
			int32 Count = 0;
			for (const TSharedPtr<FTestChain>& Chain : Chains)
			{
				if (Chain && Chain->bIsLeaf) { Count++; }
			}
			return Count;
		}

		int32 CountClosedLoops(const TArray<TSharedPtr<FTestChain>>& Chains)
		{
			int32 Count = 0;
			for (const TSharedPtr<FTestChain>& Chain : Chains)
			{
				if (Chain && Chain->bIsClosedLoop) { Count++; }
			}
			return Count;
		}

		int32 CountSingleEdgeChains(const TArray<TSharedPtr<FTestChain>>& Chains)
		{
			int32 Count = 0;
			for (const TSharedPtr<FTestChain>& Chain : Chains)
			{
				if (Chain && Chain->SingleEdge != -1) { Count++; }
			}
			return Count;
		}
	}

#pragma endregion
}
