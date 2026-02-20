// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * Valency Test Helpers
 *
 * Factory functions for creating lightweight test UDataAssets in the transient package.
 * All inline/header-only — no .cpp needed.
 */

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValencyOrbitalSet.h"
#include "Core/PCGExBondingRules.h"
#include "Core/PCGExCagePatternAsset.h"
#include "Data/Bitmasks/PCGExBitmaskCollection.h"

namespace PCGExTest::ValencyHelpers
{
	/**
	 * Create a UPCGExBitmaskCollection populated with entries matching the given names.
	 * Each entry gets a unique bitmask (1 << i) and a cycling cardinal direction.
	 */
	inline UPCGExBitmaskCollection* CreateBitmaskCollection(const TArray<FName>& Names)
	{
		static const FVector CardinalDirections[] = {
			FVector::ForwardVector,
			FVector::BackwardVector,
			FVector::RightVector,
			-FVector::RightVector,
			FVector::UpVector,
			-FVector::UpVector
		};
		static constexpr int32 NumCardinals = UE_ARRAY_COUNT(CardinalDirections);

		UPCGExBitmaskCollection* Collection = NewObject<UPCGExBitmaskCollection>(GetTransientPackage());
		Collection->Entries.Reserve(Names.Num());

		for (int32 i = 0; i < Names.Num(); ++i)
		{
			FPCGExBitmaskCollectionEntry& Entry = Collection->Entries.AddDefaulted_GetRef();
			Entry.Identifier = Names[i];
			Entry.Bitmask.Bitmask = 1LL << i;
			Entry.Direction = CardinalDirections[i % NumCardinals];
		}

		return Collection;
	}

	/**
	 * Create minimal UPCGExValencyOrbitalSet with named orbitals.
	 * Creates a backing UPCGExBitmaskCollection so that BitmaskRef.Source is valid
	 * and Validate()/Compile() succeed.
	 */
	inline UPCGExValencyOrbitalSet* CreateOrbitalSet(const TArray<FName>& OrbitalNames)
	{
		UPCGExBitmaskCollection* BitmaskCollection = CreateBitmaskCollection(OrbitalNames);

		UPCGExValencyOrbitalSet* OrbitalSet = NewObject<UPCGExValencyOrbitalSet>(GetTransientPackage());
		OrbitalSet->Orbitals.Reserve(OrbitalNames.Num());
		for (const FName& Name : OrbitalNames)
		{
			FPCGExValencyOrbitalEntry Entry;
			Entry.BitmaskRef.Source = BitmaskCollection;
			Entry.BitmaskRef.Identifier = Name;
			OrbitalSet->Orbitals.Add(Entry);
		}
		return OrbitalSet;
	}

	/**
	 * Create UPCGExValencyBondingRules with pre-set OrbitalSet, empty Modules.
	 */
	inline UPCGExValencyBondingRules* CreateBondingRules(UPCGExValencyOrbitalSet* InOrbitalSet)
	{
		UPCGExValencyBondingRules* Rules = NewObject<UPCGExValencyBondingRules>(GetTransientPackage());
		Rules->OrbitalSet = InOrbitalSet;
		return Rules;
	}

	/**
	 * Create UPCGExCagePatternAsset with pre-set OrbitalSet.
	 */
	inline UPCGExCagePatternAsset* CreateCagePatternAsset(UPCGExValencyOrbitalSet* InOrbitalSet)
	{
		UPCGExCagePatternAsset* Asset = NewObject<UPCGExCagePatternAsset>(GetTransientPackage());
		Asset->OrbitalSet = InOrbitalSet;
		return Asset;
	}

	/**
	 * Build FOrbitalDirectionResolver directly (bypasses BitmaskRef).
	 * Populates Directions[] and Bitmasks[] arrays, sets DotThreshold.
	 */
	inline PCGExValency::FOrbitalDirectionResolver BuildResolver(
		const TArray<FVector>& InDirections, double AngleThresholdDeg = 22.5)
	{
		PCGExValency::FOrbitalDirectionResolver Resolver;
		Resolver.DotThreshold = FMath::Cos(FMath::DegreesToRadians(AngleThresholdDeg));
		Resolver.bTransformOrbital = false;

		Resolver.Directions.Reserve(InDirections.Num());
		Resolver.Bitmasks.Reserve(InDirections.Num());

		for (int32 i = 0; i < InDirections.Num(); ++i)
		{
			Resolver.Directions.Add(InDirections[i].GetSafeNormal());
			Resolver.Bitmasks.Add(1LL << i);
		}

		return Resolver;
	}
}
