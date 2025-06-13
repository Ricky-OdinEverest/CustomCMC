// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomCMC_AnimInstance.h"

#include "CustomCMCCharacter.h"
#include "CustomCharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UCustomCMC_AnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	ClimbingSystemCharacter = Cast<ACustomCMCCharacter>(TryGetPawnOwner());

	if(ClimbingSystemCharacter)
	{
		CustomMovementComponent = ClimbingSystemCharacter->GetCustomMovementComponent();
	}
}

void UCustomCMC_AnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if(!ClimbingSystemCharacter || !CustomMovementComponent) return;

	GetGroundSpeed();
	GetAirSpeed();
	GetShouldMove();
	GetIsFalling();
	GetIsClimbing();
	GetClimbVelocity();
}

void UCustomCMC_AnimInstance::GetGroundSpeed()
{	
	GroundSpeed = UKismetMathLibrary::VSizeXY(ClimbingSystemCharacter->GetVelocity());
}

void UCustomCMC_AnimInstance::GetAirSpeed()
{
	AirSpeed = ClimbingSystemCharacter->GetVelocity().Z;
}

void UCustomCMC_AnimInstance::GetShouldMove()
{	
	bShouldMove =
	CustomMovementComponent->GetCurrentAcceleration().Size()>0&&
	GroundSpeed>5.f &&
	!bIsFalling;
}

void UCustomCMC_AnimInstance::GetIsFalling()
{
	bIsFalling = CustomMovementComponent->IsFalling();
}

void UCustomCMC_AnimInstance::GetIsClimbing()
{
	bIsClimbing = CustomMovementComponent->IsHanging();
}

void UCustomCMC_AnimInstance::GetClimbVelocity()
{
	ClimbVelocity = CustomMovementComponent->GetUnrotatedClimbVelocity();
}
