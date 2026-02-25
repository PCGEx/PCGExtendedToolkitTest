// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

#include "PCGExPropertyDeltaTestActor.generated.h"

/**
 * Test actor with a broad set of EditAnywhere property types for property delta testing.
 * Covers: int32, float, FString, FVector, FRotator, FLinearColor, bool, FName, TArray<FName>.
 */
UCLASS()
class APCGExPropertyDeltaTestActor : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Test")
	int32 TestHealth = 100;

	UPROPERTY(EditAnywhere, Category = "Test")
	float TestRadius = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Test")
	FString TestDisplayName = TEXT("Default");

	UPROPERTY(EditAnywhere, Category = "Test")
	FVector TestLocation = FVector(0, 0, 0);

	UPROPERTY(EditAnywhere, Category = "Test")
	FRotator TestRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Test")
	FLinearColor TestColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Test")
	bool bTestEnabled = false;

	UPROPERTY(EditAnywhere, Category = "Test")
	FName TestCategory = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Test")
	TArray<FName> TestTags;
};

/**
 * Minimal test component with simple UPROPERTYs for component delta testing.
 * CDO values: CompValue=42, CompName="CompDefault"
 */
UCLASS()
class UPCGExDeltaTestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Test")
	int32 CompValue = 42;

	UPROPERTY(EditAnywhere, Category = "Test")
	FString CompName = TEXT("CompDefault");
};

/**
 * Test actor with a default subobject component for component delta testing.
 * CDO values: ActorScore=10 + TestComp (CompValue=42, CompName="CompDefault")
 */
UCLASS()
class APCGExDeltaTestActorWithComp : public AActor
{
	GENERATED_BODY()

public:
	APCGExDeltaTestActorWithComp()
	{
		TestComp = CreateDefaultSubobject<UPCGExDeltaTestComponent>(TEXT("TestComp"));
	}

	UPROPERTY(EditAnywhere, Category = "Test")
	int32 ActorScore = 10;

	UPROPERTY(VisibleAnywhere, Category = "Test")
	TObjectPtr<UPCGExDeltaTestComponent> TestComp;
};

/**
 * Test actor with two default subobject components for multi-component delta testing.
 * CDO values: ActorValue=0 + CompA (CompValue=42, CompName="CompDefault") + CompB (same)
 */
UCLASS()
class APCGExDeltaTestActorMultiComp : public AActor
{
	GENERATED_BODY()

public:
	APCGExDeltaTestActorMultiComp()
	{
		CompA = CreateDefaultSubobject<UPCGExDeltaTestComponent>(TEXT("CompA"));
		CompB = CreateDefaultSubobject<UPCGExDeltaTestComponent>(TEXT("CompB"));
	}

	UPROPERTY(EditAnywhere, Category = "Test")
	int32 ActorValue = 0;

	UPROPERTY(VisibleAnywhere, Category = "Test")
	TObjectPtr<UPCGExDeltaTestComponent> CompA;

	UPROPERTY(VisibleAnywhere, Category = "Test")
	TObjectPtr<UPCGExDeltaTestComponent> CompB;
};
