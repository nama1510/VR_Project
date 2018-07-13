// Fill out your copyright notice in the Description page of Project Settings.

#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"

#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PostProcessComponent.h"

#include "Engine/World.h"
#include "Public/DrawDebugHelpers.h"
#include "Public/TimerManager.h"
#include "AI/Navigation/NavigationSystem.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Curves/CurveFloat.h"
// Sets default values
AVRCharacter::AVRCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	VRRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VRRoot"));
	VRRoot->SetupAttachment(GetRootComponent());

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VRRoot);


	DestinationMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DestinationMarker"));
	DestinationMarker->SetupAttachment(GetRootComponent());

	PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessComponent"));
	PostProcessComponent->SetupAttachment(GetRootComponent());


}

// Called when the game starts or when spawned
void AVRCharacter::BeginPlay()
{
	Super::BeginPlay();
	DestinationMarker->SetVisibility(false);

	if (BlinkerMaterialBase != nullptr) {
		BlinkerMaterialInstance = UMaterialInstanceDynamic::Create(BlinkerMaterialBase, this);
		PostProcessComponent->AddOrUpdateBlendable(BlinkerMaterialInstance);

	}

}

// Called every frame
void AVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector NewCameraOffset = Camera->GetComponentLocation() - GetActorLocation();
	//FVector::VectorPlaneProject(NewCameraOffset, FVector::UpVector);
	NewCameraOffset.Z = 0;
	AddActorWorldOffset(NewCameraOffset);
	VRRoot->AddWorldOffset(-NewCameraOffset);

	UpdateDestinationMarker();

	UpdateBlinkers();

}
void AVRCharacter::UpdateBlinkers()
{
	if (RadiusVsVelocity == nullptr) return;
	float Speed = GetVelocity().Size();
	float Radius = RadiusVsVelocity->GetFloatValue(Speed);

	BlinkerMaterialInstance->SetScalarParameterValue(TEXT("Radius"), Radius);

	FVector2D Centre = GetBlinkerCenter();
	BlinkerMaterialInstance->SetVectorParameterValue(TEXT("Centre"), FLinearColor(Centre.X, Centre.Y, 0));


}
FVector2D AVRCharacter::GetBlinkerCenter()
{
	FVector MovementDirection = GetVelocity().GetSafeNormal();
	if (MovementDirection.IsNearlyZero() )
	{
		return FVector2D(.5f, .5f);
	}

	FVector WorldStationaryLocation;
	if (FVector::DotProduct(Camera->GetForwardVector(), MovementDirection) > 0) {
		WorldStationaryLocation = Camera->GetComponentLocation() + MovementDirection * 1000;
	}
	else {
		WorldStationaryLocation = Camera->GetComponentLocation() - MovementDirection * 1000;
	}


	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC == nullptr)
	{
		return FVector2D(.5f, .5f);
	}
	FVector2D ScreenStationaryLocation;
	PC->ProjectWorldLocationToScreen(WorldStationaryLocation, ScreenStationaryLocation);

	int32 SizeX, SizeY;

	PC->GetViewportSize(SizeX, SizeY);

	ScreenStationaryLocation.X /= SizeX;
	ScreenStationaryLocation.Y /= SizeY;
	return ScreenStationaryLocation;
}
void AVRCharacter::UpdateDestinationMarker()
{

	FVector Location;

	bool bHasDestination = FindTeleportDestination(Location);

	if (bHasDestination) {
		DestinationMarker->SetWorldLocation(Location);
		DestinationMarker->SetVisibility(true);
	}
	else 
	{
		DestinationMarker->SetVisibility(false);
	}
}

bool AVRCharacter::FindTeleportDestination(FVector & OutLocation)
{
	FHitResult HitResult;
	FVector StartLocation = Camera->GetComponentLocation();
	FVector EndLocation = StartLocation + (Camera->GetForwardVector() * MaxTeleportDistance);
	bool bIsHitLocation = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		ECollisionChannel::ECC_Visibility
	);

	if (!bIsHitLocation) return false;

	FNavLocation NavLocation;
	bool bOnNavMesh = GetWorld()->GetNavigationSystem()->ProjectPointToNavigation(HitResult.Location, NavLocation, TeleportProjectionExtent);

	if (!bOnNavMesh) return false;

	OutLocation = NavLocation;

	return true;
}



// Called to bind functionality to input
void AVRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Forward"),this, &AVRCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("Right"), this, &AVRCharacter::MoveRight);
	PlayerInputComponent->BindAction(TEXT("Teleport"), IE_Released, this, &AVRCharacter::BeginTeleport);
	

}

void AVRCharacter::MoveForward(float throttle)
{
	AddMovementInput(throttle * Camera->GetForwardVector());
}

void AVRCharacter::MoveRight(float throttle)
{
	AddMovementInput(throttle * Camera->GetRightVector());

}

void AVRCharacter::BeginTeleport()
{
	FTimerHandle TimerHandle;
	//Fade out
	StartFade(0, 1);
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AVRCharacter::TeleportFinish, FadeDuration);
}

void AVRCharacter::TeleportFinish()
{
	SetActorLocation(DestinationMarker->GetComponentLocation() + GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	APlayerController* PC = Cast<APlayerController>(GetController());
	StartFade(1, 0);
}

void AVRCharacter::StartFade(float FromAlpha, float ToAlpha)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC != nullptr)
	{
		PC->PlayerCameraManager->StartCameraFade(FromAlpha, ToAlpha, FadeDuration, FLinearColor::Black);
	}
}


