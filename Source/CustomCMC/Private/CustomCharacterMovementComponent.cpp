// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomCharacterMovementComponent.h"

#include "CustomCMCCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

#if 1
float MacroDuration = 2.f;
#define SLOG(x) GEngine->AddOnScreenDebugMessage(-1, MacroDuration ? MacroDuration : -1.f, FColor::Yellow, x);
#define POINT(x, c) DrawDebugPoint(GetWorld(), x, 10, c, !MacroDuration, MacroDuration);
#define LINE(x1, x2, c) DrawDebugLine(GetWorld(), x1, x2, c, !MacroDuration, MacroDuration);
#define CAPSULE(x, c) DrawDebugCapsule(GetWorld(), x, CapHH(), CapR(), FQuat::Identity, c, !MacroDuration, MacroDuration);
#else
#define SLOG(x)
#define POINT(x, c)
#define LINE(x1, x2, c)
#define CAPSULE(x, c)
#endif


UCustomCharacterMovementComponent::UCustomCharacterMovementComponent(): Safe_bWantsToSprint(false),
                                                                        Safe_bHadAnimRootMotion(false),
                                                                        Safe_bTransitionFinished(false),
                                                                        TransitionQueuedMontage(nullptr),
                                                                        TransitionQueuedMontageSpeed(0),
                                                                        TransitionRMS_ID(0), Proxy_bLedgeGrabbed(false),
                                                                        TallLedgeGrabMontage(nullptr),
                                                                        TransitionTallLedgeGrabMontage(nullptr),
                                                                        ProxyShortLedgeGrabMontage(nullptr),
                                                                        ProxyTallLedgeGrabMontage(nullptr),
                                                                        CustomCharacterOwner(nullptr),
                                                                        CurrentClimbableSurfaceNormal(),
                                                                        CurrentClimbableSurfaceLocation()
{
	NavAgentProps.bCanCrouch = true;
}

bool UCustomCharacterMovementComponent::IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}

void UCustomCharacterMovementComponent::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	
	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
	//Walk after ledge Grab
	if (!HasAnimRootMotion() && Safe_bHadAnimRootMotion && IsMovementMode(MOVE_Flying))
	{
		UE_LOG(LogTemp, Warning, TEXT("Ending Anim Root Motion"))
		SetMovementMode(MOVE_Walking);
	}
	// Set transision finished to true and
	if (GetRootMotionSourceByID(TransitionRMS_ID) && GetRootMotionSourceByID(TransitionRMS_ID)->Status.HasFlag(ERootMotionSourceStatusFlags::Finished))
	{
		// Manual Removal is likely not needed
		RemoveRootMotionSourceByID(TransitionRMS_ID);
		Safe_bTransitionFinished = true;
	}
	
	Safe_bHadAnimRootMotion = HasAnimRootMotion();
}

bool UCustomCharacterMovementComponent::IsMovingOnGround() const
{
	return Super::IsMovingOnGround();
}

bool UCustomCharacterMovementComponent::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState();
}

float UCustomCharacterMovementComponent::GetMaxAcceleration() const
{
	return Super::GetMaxAcceleration();

	/*if (MovementMode != MOVE_Custom) return Super::GetMaxAcceleration();

	switch (CustomMovementMode)
	{
	case CMOVE_Slide:
		return 0.f;
	case CMOVE_Hang:
		return 200.f;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode"))
		return -1.f;
	}*/
}

float UCustomCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	if (MovementMode != MOVE_Custom) return Super::GetMaxBrakingDeceleration();

	switch (CustomMovementMode)
	{
	case CMOVE_Slide:
		return 0.f;
	case CMOVE_Hang:
		return LedgeBrakingDeceleration;
	case CMOVE_VertWallRun:
		return 0.f;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode"))
		return -1.f;
	}
}

float UCustomCharacterMovementComponent::GetGravityZ() const
{
	 if (MovementMode != MOVE_Custom) return Super::GetGravityZ();

	// seeing if 0 gravity works
	switch (CustomMovementMode)
	{
	case CMOVE_Hang:
		return 0.f;
	case CMOVE_VertWallRun:
		return 0.0;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode"))
		return -1.f;
	}
}

// every new mode needs this braking and maxspeed
void UCustomCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	 if (MovementMode != MOVE_Custom) return Super::PhysCustom(deltaTime, Iterations);

	switch (CustomMovementMode)
	{
	case CMOVE_Hang:
		PhysHang( deltaTime,  Iterations);
		break;
	case CMOVE_VertWallRun:
		PhysVertWallRun(deltaTime, Iterations);
		break;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode"))
	}
	
}

void UCustomCharacterMovementComponent::EnterVertWallRun(EMovementMode PrevMode,
	ECustomMovementMode PrevCustomMode)
{
	bOrientRotationToMovement = false;
//	float VertWallRunEnterImpulse = 100.f;
	//Velocity += Velocity.GetSafeNormal2D() * VertWallRunEnterImpulse;
}

void UCustomCharacterMovementComponent::ExitVertWallRun()
{
	bOrientRotationToMovement = true;
}

void UCustomCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode,
                                                              uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	
	if (IsFalling())
	{
		bOrientRotationToMovement = true;
	}
	else if (IsHanging())
	{
		bOrientRotationToMovement = false;
	}
	else
	{
		bOrientRotationToMovement = true;
	}

	if (IsCustomMovementMode(CMOVE_VertWallRun)) EnterVertWallRun(PreviousMovementMode, (ECustomMovementMode)PreviousCustomMode);


	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_VertWallRun) ExitVertWallRun();

	
	// Checks if wall is on left or right for the simulated proxy
	if (IsVertWallRunning() && GetOwnerRole() == ROLE_SimulatedProxy)
	{
		/*FVector Start = UpdatedComponent->GetComponentLocation();
		FVector End = Start + UpdatedComponent->GetRightVector() * CapR() * 2;
		auto Params = ZippyCharacterOwner->GetIgnoreCharacterParams();
		FHitResult WallHit;
		Safe_bWallRunIsRight = GetWorld()->LineTraceSingleByProfile(WallHit, Start, End, "BlockAll", Params);*/
	}
}

bool UCustomCharacterMovementComponent::CanAttemptJump() const
{
	return Super::CanAttemptJump() || IsHanging() || IsVertWallRunning();
}

bool UCustomCharacterMovementComponent::DoJump(bool bReplayingMoves)
{
	return Super::DoJump(bReplayingMoves);
}

FString UCustomCharacterMovementComponent::GetMovementModeAsString(EMovementMode MoveMode, uint8 CustomMode) const
{
	if (MoveMode != MOVE_Custom)
	{
		return UEnum::GetValueAsString(MoveMode);
	}

	return FString::Printf(TEXT("Custom:%s"), *UEnum::GetValueAsString((ECustomMovementMode)CustomMode));
}

bool UCustomCharacterMovementComponent::TryLedgeGrab()
{
	// could just set to climb here
	//probably need make rotation 54:33
		// Temp Enable Crouching

	// Only enable while falling
	if ( !IsMovementMode(MOVE_Falling)) return false;
	// original line is redundant at the moment
	if (!(IsMovementMode(MOVE_Walking) /*&& !IsCrouching()*/) && !IsMovementMode(MOVE_Falling)) return false;
	// Helper Variables
	FVector BaseLoc = UpdatedComponent->GetComponentLocation() + FVector::DownVector * CapHH();
	FVector Fwd = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	auto Params = CustomCharacterOwner->GetIgnoreCharacterParams();
	float MaxHeight = CapHH() * 2+ LedgeGrabReachHeight;
	float CosMMWSA = FMath::Cos(FMath::DegreesToRadians(LedgeGrabMinWallSteepnessAngle));
	float CosMMSA = FMath::Cos(FMath::DegreesToRadians(LedgeGrabMaxSurfaceAngle));
	float CosMMAA = FMath::Cos(FMath::DegreesToRadians(LedgeGrabMaxAlignmentAngle));

	//SLOG("Starting LedgeGrab Attempt")

	// Check Front Face
	FHitResult FrontHit;

	float CheckDistance = FMath::Clamp(Velocity | Fwd, CapR() + 30, MaxLedgeGrabDistance);
	FVector FrontStart = BaseLoc + FVector::UpVector * (MaxStepHeight - 1);
	for (int i = 0; i < 6; i++)
	{
		LINE(FrontStart, FrontStart + Fwd * CheckDistance, FColor::Red)
		if (GetWorld()->LineTraceSingleByProfile(FrontHit, FrontStart, FrontStart + Fwd * CheckDistance, "BlockAll", Params)) break;
		FrontStart += FVector::UpVector * (2.f * CapHH() - (MaxStepHeight - 1)) / 5;
	}
	if (!FrontHit.IsValidBlockingHit()) return false;
	float CosWallSteepnessAngle = FrontHit.Normal | FVector::UpVector;
	// Pipe Symbol is used for the dot product in this context
	if (FMath::Abs(CosWallSteepnessAngle) > CosMMWSA || (Fwd | -FrontHit.Normal) < CosMMAA) return false;

	//POINT(FrontHit.Location, FColor::Red);

	// Check Height
	TArray<FHitResult> HeightHits;
	FHitResult SurfaceHit;
	// Vector  traveling in the direction up the surface the wall to the edge 
	FVector WallUp = FVector::VectorPlaneProject(FVector::UpVector, FrontHit.Normal).GetSafeNormal();
	//Angle between world up and WallUP
	float WallCos = FVector::UpVector | FrontHit.Normal;

	//enbales us to make a downward cast down towards the wall
	float WallSin = FMath::Sqrt(1 - WallCos * WallCos);
	// 
	FVector TraceStart = FrontHit.Location + Fwd + WallUp * (MaxHeight - (MaxStepHeight - 1)) / WallSin;
	LINE(TraceStart, FrontHit.Location + Fwd, FColor::Orange)
		if (!GetWorld()->LineTraceMultiByProfile(HeightHits, TraceStart, FrontHit.Location + Fwd, "BlockAll", Params)) return false;
	for (const FHitResult& Hit : HeightHits)
	{
		if (Hit.IsValidBlockingHit())
		{
			SurfaceHit = Hit;
			break;
		}
	}
	// if no blocking hit or surface is too steep
	if (!SurfaceHit.IsValidBlockingHit() || (SurfaceHit.Normal | FVector::UpVector) < CosMMSA) return false;
	float Height = (SurfaceHit.Location - BaseLoc) | FVector::UpVector;

	SLOG(FString::Printf(TEXT("Height: %f"), Height))
	POINT(SurfaceHit.Location, FColor::Blue);

	if (Height > MaxHeight) return false;

	// Check Clearance
	float SurfaceCos = FVector::UpVector | SurfaceHit.Normal;
	float SurfaceSin = FMath::Sqrt(1 - SurfaceCos * SurfaceCos);
	FVector ClearCapLoc = SurfaceHit.Location + Fwd * CapR() + FVector::UpVector * (CapHH() + 1 + CapR() * 2 * SurfaceSin);
	FCollisionShape CapShape = FCollisionShape::MakeCapsule(CapR(), CapHH());
	if (GetWorld()->OverlapAnyTestByProfile(ClearCapLoc, FQuat::Identity, "BlockAll", CapShape, Params))
	{
		CAPSULE(ClearCapLoc, FColor::Red)
				return false;
	}
	else
	{
		CAPSULE(ClearCapLoc, FColor::Green)
	}
	//SLOG("Can LedgeGrab")
	
	// LedgeGrab Selection
	//FVector ShortLedgeGrabTarget = GetLedgeGrabStartLocation(FrontHit, SurfaceHit, false);
	FVector TallLedgeGrabTarget = GetLedgeGrabStartLocation(FrontHit, SurfaceHit);
	
	bool bTallLedgeGrab = false;
	if (IsMovementMode(MOVE_Falling) && Height > CapHH())
		bTallLedgeGrab = true;
	else if (IsMovementMode(MOVE_Falling) && (Velocity | FVector::UpVector) < 0)
	{
		if (!GetWorld()->OverlapAnyTestByProfile(TallLedgeGrabTarget, FQuat::Identity, "BlockAll", CapShape, Params))
			bTallLedgeGrab = true;
	}

	FVector ForwardOffset = UpdatedComponent->GetForwardVector() * LedgeGrabXOffset;
	FVector UpOffset = FVector::UpVector * LedgeGrabZOffset;
	FVector TransitionTarget = TallLedgeGrabTarget + ForwardOffset + UpOffset;
	CAPSULE(TransitionTarget, FColor::Yellow)

	//  Exit Early if Capsule Does not fit
	// Test if character can reach goal
	// Technically this moves the character in one frame and then decides if they 
	FTransform CurrentTransform = UpdatedComponent->GetComponentTransform();
	FHitResult Hit, ReturnHit;
	SafeMoveUpdatedComponent(TransitionTarget - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true, Hit);
	FVector ResultLocation = UpdatedComponent->GetComponentLocation();
	SafeMoveUpdatedComponent(CurrentTransform.GetLocation() - ResultLocation, UpdatedComponent->GetComponentQuat(), false, ReturnHit);
	
	if (!ResultLocation.Equals(TransitionTarget)) return false;
	

	// Perform Transition to LedgeGrab
	CAPSULE(UpdatedComponent->GetComponentLocation(), FColor::Red)


	float UpSpeed = Velocity | FVector::UpVector;
	float TransDistance = FVector::Dist(TransitionTarget, UpdatedComponent->GetComponentLocation());

	TransitionQueuedMontageSpeed = FMath::GetMappedRangeValueClamped(FVector2D(-500, 750), FVector2D(.9f, 1.2f), UpSpeed);
	TransitionRMS.Reset();
	// Make new RMS Struct
	TransitionRMS = MakeShared<FRootMotionSource_MoveToForce>();
	// Overide Rather than add, This is probably covered by reset
	TransitionRMS->AccumulateMode = ERootMotionAccumulateMode::Override;

	// Set duration based on distance to target. May need to be scaled up or down given current values between .1 and .25
	TransitionRMS->Duration = FMath::Clamp(TransDistance / 500.f, .1f, .5f);
	SLOG(FString::Printf(TEXT("Duration: %f"), TransitionRMS->Duration))
	SLOG(FString::Printf(TEXT("TransDistance: %f"), TransDistance))

	TransitionRMS->StartLocation = UpdatedComponent->GetComponentLocation();
	TransitionRMS->TargetLocation = TransitionTarget;

	// Apply Transition Root Motion Source
	Velocity = FVector::ZeroVector;
	// Flying helps with three dimensional root motion but I may be able to do it with a custom climbing mode
	SetMovementMode(MOVE_Flying);
	TransitionRMS_ID = ApplyRootMotionSource(TransitionRMS);

	TransitionName = "LedgeGrab";
	
	// cache the exact ledge direction for PhysHang:
	CurrentLedgeTangent = FVector::CrossProduct(SurfaceHit.Normal, FrontHit.Normal).GetSafeNormal();

	
	// Animations
	if (bTallLedgeGrab)
	{
		TransitionQueuedMontage = TallLedgeGrabMontage;
		// Transition is not a root motion montage but maybe I can see how this would work with motion warping
		CharacterOwner->PlayAnimMontage(TransitionTallLedgeGrabMontage, 1 / TransitionRMS->Duration);
		if (IsServer()) Proxy_bLedgeGrabbed = !Proxy_bLedgeGrabbed;

		
		if (GEngine)
		{
			
			GEngine->AddOnScreenDebugMessage(
				4,
				10.5f,
				FColor::Red,
				FString::Printf(TEXT("TallGrabAttmepted"))
			);
		}
	}
	else
	{
		if (GEngine)
		{
			
			GEngine->AddOnScreenDebugMessage(
				5,
				10.5f,
				FColor::Red,
				FString::Printf(TEXT("TallGrabFailed"))
			);
		}
	}
	// 5:41 6/30 comment
	/*FQuat NewRotation = FRotationMatrix::MakeFromXZ(-FrontHit.Normal, FVector::UpVector).ToQuat();
	SafeMoveUpdatedComponent(FVector::ZeroVector, NewRotation, false, FrontHit);*/


	

	// after calculate CurrentLedgeTangent…
	if (GEngine)
	{
		// -1 = auto key, 1.5f = stays on screen for 1.5 seconds
		GEngine->AddOnScreenDebugMessage(
			-11,
			10.5f,
			FColor::Red,
			FString::Printf(TEXT("Tangent = %s"), *CurrentLedgeTangent.ToString())
		);
	}

	// Cache front and surface hits
	CurrentLedgeWallNormal = FrontHit.Normal;
	CurrentLedgeTopNormal  = SurfaceHit.Normal;

	// MoveToUpdateBeforeMovement
	/*SetMovementMode(MOVE_Custom, CMOVE_Hang);
	bOrientRotationToMovement = false;*/

	return true;
}

bool UCustomCharacterMovementComponent::TryMantle()
{
	
	if (!IsCustomMovementMode(CMOVE_Hang)) return false;
	
	// Helper Variables
	// Baser Loc At Character Eye Height
	FVector BaseLoc = UpdatedComponent->GetComponentLocation() + UpdatedComponent->GetUpVector() * CharacterOwner->BaseEyeHeight;
	// Direction Vector
	FVector Fwd = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	// Ignore Character and Components
	auto Params = CustomCharacterOwner->GetIgnoreCharacterParams();
	//probably irrelevant for mantle
	float MaxHeight = CapHH() * 2+ LedgeGrabReachHeight;

	
	float CosMMWSA = FMath::Cos(FMath::DegreesToRadians(LedgeGrabMinWallSteepnessAngle));
	float CosMMSA = FMath::Cos(FMath::DegreesToRadians(LedgeGrabMaxSurfaceAngle));
	float CosMMAA = FMath::Cos(FMath::DegreesToRadians(LedgeGrabMaxAlignmentAngle));
	

	// distance for eye line trace
	const FVector WallEnd = BaseLoc + Fwd * MaxLedgeGrabDistance;
	
	// Check Front Face
	FHitResult FrontHit;

	//removed the velocity clamp
	//Simply Check forward 20 units
	float CheckDistance = 50.f;
	FVector FrontStart = BaseLoc;

	//Do five traces from the center of the body to hands
	for (int i = 0; i < 6; i++)
	{
		LINE(FrontStart, FrontStart + Fwd * CheckDistance, FColor::Red)
		if (GetWorld()->LineTraceSingleByProfile(FrontHit, FrontStart, FrontStart + Fwd * CheckDistance, "BlockAll", Params)) break;
		// Ideally I would be performing 5 traces from the center of the body to the hand roughly
		FrontStart += (FVector::UpVector * 2)  / 5;
	}
	// return if no hits. (Probably only possible if I am on a small railing)
	if (!FrontHit.IsValidBlockingHit()) return false;

//POINT(FrontHit.Location, FColor::Red);
	
	// Check Height
	TArray<FHitResult> HeightHits;
	FHitResult SurfaceHit;
// Project up vector onto the wall plane
	FVector WallUp = FVector::VectorPlaneProject(FVector::UpVector, FrontHit.Normal).GetSafeNormal();

	// angle between upvector and hit normal in radians
	float WallCos = FVector::UpVector | FrontHit.Normal;
	
	float WallSin = FMath::Sqrt(1 - WallCos * WallCos);
	
	FVector TraceStart = FrontHit.Location + Fwd + WallUp * (MaxHeight - (MaxStepHeight - 1)) / WallSin;
	LINE(TraceStart, FrontHit.Location + Fwd, FColor::Orange)
	if (!GetWorld()->LineTraceMultiByProfile(HeightHits, TraceStart, FrontHit.Location + Fwd, "BlockAll", Params)) return false;
	for (const FHitResult& Hit : HeightHits)
	{
		if (Hit.IsValidBlockingHit())
		{
			SurfaceHit = Hit;
			break;
		}
	}
	//return if the top of the surface has too great of a slope
	if (!SurfaceHit.IsValidBlockingHit() || (SurfaceHit.Normal | FVector::UpVector) < CosMMSA) return false;
	float Height = (SurfaceHit.Location - BaseLoc) | FVector::UpVector;

	POINT(SurfaceHit.Location, FColor::Blue);
	
	
	// Check Clearance
	float SurfaceCos = FVector::UpVector | SurfaceHit.Normal;
	float SurfaceSin = FMath::Sqrt(1 - SurfaceCos * SurfaceCos);
	FVector ClearCapLoc = SurfaceHit.Location + Fwd * CapR() + FVector::UpVector * (CapHH() + 1 + CapR() * 2 * SurfaceSin);
	FCollisionShape CapShape = FCollisionShape::MakeCapsule(CapR(), CapHH());
	if (GetWorld()->OverlapAnyTestByProfile(ClearCapLoc, FQuat::Identity, "BlockAll", CapShape, Params))
	{
	CAPSULE(ClearCapLoc, FColor::Red)
		return false;
	}
	else
	{
	CAPSULE(ClearCapLoc, FColor::Green)
	}
	SLOG("Can Mantle")
	

	return true;
}


void UCustomCharacterMovementComponent::PhysHang(float deltaTime, int32 Iterations)
{
	//SLOG("Entered Hang")
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}
	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	// --- 1) Trace the wall face (forward) ---
	FHitResult WallHit;
	{
		// 1. Grab your capsule’s world location
		const FVector ComponentLoc = UpdatedComponent->GetComponentLocation();

		// 2. Move up by BaseEyeHeight, then forward by 30cm
		const FVector WallStartEye = ComponentLoc
			+ UpdatedComponent->GetUpVector() * CharacterOwner->BaseEyeHeight
			+ UpdatedComponent->GetForwardVector() * 30.f;

		// 3. End point at max distance
		const FVector WallEnd = WallStartEye + UpdatedComponent->GetForwardVector() * MaxLedgeGrabDistance;


		GetWorld()->LineTraceSingleByProfile(
			WallHit, WallStartEye, WallEnd, TEXT("BlockAll"), CustomCharacterOwner->GetIgnoreCharacterParams()
		);

		// draw the wall trace in magenta
		/*DrawDebugLine(
			GetWorld(),
			WallStartEye,
			WallEnd,
			FColor::Magenta,   // color
			false,             // persistent lines?
			0.1f,              // life time
			0,                 // depth priority
			5.0f               // thickness
		);*/
	}

	// --- 2) Trace the top face (down from just above the wall hit) ---
	FHitResult TopHit;
	if (WallHit.IsValidBlockingHit())
	{
		FVector WallUp = FVector::VectorPlaneProject(FVector::UpVector, WallHit.Normal).GetSafeNormal();
		const float ProbeHeight = CapHH() * 2.f + 10.f;

		// how far forward off the wall you want to push your trace (in cm)
		const float ForwardOffset = 20.f;

		// move your start/end off the wall surface a bit
		FVector WallForward = -WallHit.Normal * ForwardOffset;

		// now build start/end
		const FVector TopStart = WallHit.Location + WallUp * ProbeHeight + WallForward;
		const FVector TopEnd   = WallHit.Location - WallUp * ProbeHeight + WallForward;

		GetWorld()->LineTraceSingleByProfile(
			TopHit, TopStart, TopEnd, TEXT("BlockAll"), CustomCharacterOwner->GetIgnoreCharacterParams()
		);

		/*// draw the top trace in cyan
		DrawDebugLine(
			GetWorld(),
			TopStart,
			TopEnd,
			FColor::Cyan,
			false,
			0.1f,
			0,
			5.0f
		);*/
	}

	// only proceed if we have both normals
	if (WallHit.IsValidBlockingHit() && TopHit.IsValidBlockingHit())
	{
		CurrentLedgeWallNormal = WallHit.Normal.GetSafeNormal();
		CurrentLedgeTopNormal  = TopHit.Normal.GetSafeNormal();

		// --- 3) Cross them to get the edge tangent ---
		FVector FreshTangent = FVector::CrossProduct(CurrentLedgeWallNormal, CurrentLedgeTopNormal).GetSafeNormal();
		CurrentLedgeTangent  = FMath::VInterpTo(CurrentLedgeTangent, FreshTangent, deltaTime, CornerInterpSpeed);


	}
	else
	{
		
		//CustomCharacterOwner->LaunchCharacter(FVector(0.f,0.f,700.f),false, false);
		
	}
	
	/*Process all the climbable surfaces info*/
	TraceClimbableSurfaces();
	ProcessClimbableSurfaceInfo();
	
	
	// after you calculate CurrentLedgeTangent…
	if (GEngine)
	{
		// -1 = auto key, 1.5f = stays on screen for 1.5 seconds
		GEngine->AddOnScreenDebugMessage(
			2,
			.5f,
			FColor::Green,
			FString::Printf(TEXT("Tangent = %s"), *CurrentLedgeTangent.ToString())
		);
	}
	/*Check if we should stop climbing*/
	if(CheckShouldStopHanging() || CheckHasReachedFloor())
	{
		StopHanging();
	}

	RestorePreAdditiveRootMotionVelocity();

	if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{	//Define the max climb speed and acceleration
		CalcVelocity(deltaTime, 0.f, true, LedgeBrakingDeceleration);
	}

	ApplyRootMotionToVelocity(deltaTime);

	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);

	//Handle climb rotation
	SafeMoveUpdatedComponent(Adjusted, GetClimbRotation(deltaTime), true, Hit);

	if (Hit.Time < 1.f)
	{
		//adjust and try again
		HandleImpact(Hit, deltaTime, Adjusted);
		SlideAlongSurface(Adjusted, (1.f-Hit.Time), Hit.Normal, Hit, true);
	}

	if(!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}

	/*Snap movement to climbable surfaces*/
	SnapMovementToClimableSurfaces(deltaTime);
	
	if (GetCurrentAcceleration().Z >= 2000.f && Velocity.Z >= 10.f)
	{

		Safe_bWantsToMantle = true;

	}
}

FVector UCustomCharacterMovementComponent::GetLedgeGrabStartLocation(FHitResult FrontHit, FHitResult SurfaceHit) const
{
	float CosWallSteepnessAngle = FrontHit.Normal | FVector::UpVector;
	//Probably needs adjusting
	// where my character is adjusted in position downward
	float DownDistance =  CapHH() * 2.f;
	FVector EdgeTangent = FVector::CrossProduct(SurfaceHit.Normal, FrontHit.Normal).GetSafeNormal();

	// 
	if (GEngine)
	{
		// -1 = auto key, 1.5f = stays on screen for 1.5 seconds
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Magenta,
			FString::Printf(TEXT("Tangent = %s"), *EdgeTangent.ToString())
		);
	}
	

	FVector LedgeGrabStart = SurfaceHit.Location;
	LedgeGrabStart += FrontHit.Normal.GetSafeNormal2D() * (2.f + CapR());
	LedgeGrabStart += UpdatedComponent->GetForwardVector().GetSafeNormal2D().ProjectOnTo(EdgeTangent) * CapR() * .3f;
	LedgeGrabStart += FVector::UpVector * CapHH();
	LedgeGrabStart += FVector::DownVector * DownDistance;
	LedgeGrabStart += FrontHit.Normal.GetSafeNormal2D() * CosWallSteepnessAngle * DownDistance;

	return LedgeGrabStart;
}

FVector UCustomCharacterMovementComponent::GetLedgeGrabCurrentLocation(FHitResult FrontHit, FHitResult SurfaceHit) const
{
	float CosWallSteepnessAngle = FrontHit.Normal | FVector::UpVector;
	//Probably needs adjusting
	float DownDistance =  CapHH() * 2.f;
	FVector EdgeTangent = FVector::CrossProduct(SurfaceHit.Normal, FrontHit.Normal).GetSafeNormal();

	FVector LedgeGrabStart = SurfaceHit.Location;
	LedgeGrabStart += FrontHit.Normal.GetSafeNormal2D() * (2.f + CapR());
	LedgeGrabStart += UpdatedComponent->GetForwardVector().GetSafeNormal2D().ProjectOnTo(EdgeTangent) * CapR() * .3f;
	LedgeGrabStart += FVector::UpVector * CapHH();
	LedgeGrabStart += FVector::DownVector * DownDistance;
	LedgeGrabStart += FrontHit.Normal.GetSafeNormal2D() * CosWallSteepnessAngle * DownDistance;

	return LedgeGrabStart;
}

void UCustomCharacterMovementComponent::SnapMovementToClimableSurfaces(float DeltaTime)
{
	const FVector ComponentForward = UpdatedComponent->GetForwardVector();
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();

	const FVector ProjectedCharacterToSurface = 
	(CurrentClimbableSurfaceLocation - ComponentLocation).ProjectOnTo(ComponentForward);

	const FVector SnapVector = -CurrentClimbableSurfaceNormal * ProjectedCharacterToSurface.Length();

	UpdatedComponent->MoveComponent(
	SnapVector*DeltaTime*100.f,
	UpdatedComponent->GetComponentQuat(),
	true);
}

FQuat UCustomCharacterMovementComponent::GetClimbRotation(float DeltaTime)
{	
	const FQuat CurrentQuat = UpdatedComponent->GetComponentQuat();

	if(HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
	{
		return CurrentQuat;
	}

	const FQuat TargetQuat = FRotationMatrix::MakeFromX(-CurrentClimbableSurfaceNormal).ToQuat();

	return FMath::QInterpTo(CurrentQuat,TargetQuat,DeltaTime,5.f);
}

bool UCustomCharacterMovementComponent::CheckShouldStopHanging()
{
	if(ClimbableSurfacesTracedResults.IsEmpty()) return true;

	const float DotResult = FVector::DotProduct(CurrentClimbableSurfaceNormal,FVector::UpVector);
	const float DegreeDiff = FMath::RadiansToDegrees(FMath::Acos(DotResult));

	if(DegreeDiff<=60.f)
	{
		return true;
	}
	

	return false;
}

bool UCustomCharacterMovementComponent::CheckHasReachedFloor()
{
	const FVector DownVector = -UpdatedComponent->GetUpVector();
	const FVector StartOffset = DownVector * 50.f;

	const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
	const FVector End = Start + DownVector;

	TArray<FHitResult> PossibleFloorHits = DoCapsuleTraceMultiByObject(Start,End);

	if(PossibleFloorHits.IsEmpty()) return false;

	for(const FHitResult& PossibleFloorHit:PossibleFloorHits)
	{	
		const bool bFloorReached =
		FVector::Parallel(-PossibleFloorHit.ImpactNormal,FVector::UpVector) &&
		GetUnrotatedClimbVelocity().Z<-10.f;

		if(bFloorReached)
		{
			return true;
		}
	}

	return false;
}

void UCustomCharacterMovementComponent::StopHanging()
{
	SetMovementMode(MOVE_Falling);
}

void UCustomCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//boolean to call replicated animation from movement component
	DOREPLIFETIME_CONDITION(UCustomCharacterMovementComponent, Proxy_bLedgeGrabbed, COND_SkipOwner)
}

FVector UCustomCharacterMovementComponent::GetUnrotatedClimbVelocity() const
{
	return UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(),Velocity);
}


#pragma region SavedMove_Custom
UCustomCharacterMovementComponent::FSavedMove_Custom::FSavedMove_Custom()
{
	Saved_bWantsToSprint=0;
}

// can tell the server to play a move twice if it is similar enough
bool UCustomCharacterMovementComponent::FSavedMove_Custom::CanCombineWith(const FSavedMovePtr& NewMove,
                                                                        ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_Custom* NewCustomMove = static_cast<FSavedMove_Custom*>(NewMove.Get());

	if (Saved_bWantsToSprint != NewCustomMove->Saved_bWantsToSprint)
	{
		return false;
	}

	return FSavedMove_Character::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

//Reset Move To Be Empty
void UCustomCharacterMovementComponent::FSavedMove_Custom::Clear()
{
	FSavedMove_Character::Clear();

	Saved_bWantsToSprint = 0;
	Saved_bWantsToMantle = 0;
	Saved_bHadAnimRootMotion = 0;
	Saved_bTransitionFinished = 0;
	Saved_bVertWallRun = 0;
}

// Can potentially add more of these for modes that need to be continuously updated
uint8 UCustomCharacterMovementComponent::FSavedMove_Custom::GetCompressedFlags() const
{

	uint8 Result = FSavedMove_Character::GetCompressedFlags();

	if (Saved_bWantsToSprint) Result |= FLAG_Sprint;
	if (Saved_bPressedCustomJump) Result |= FLAG_JumpPressed;
	
	
	return Result;
}

// Set move for prediction to value we operate on in the custom CMC
void UCustomCharacterMovementComponent::FSavedMove_Custom::SetMoveFor(ACharacter* C, float InDeltaTime,
	FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	UCustomCharacterMovementComponent* CharacterMovement = Cast<UCustomCharacterMovementComponent>(C->GetCharacterMovement());

	Saved_bWantsToSprint = CharacterMovement->Safe_bWantsToSprint;
	Saved_bPressedCustomJump = CharacterMovement->CustomCharacterOwner->bPressedCustomJump;

	Saved_bHadAnimRootMotion = CharacterMovement->Safe_bHadAnimRootMotion;
	Saved_bTransitionFinished = CharacterMovement->Safe_bTransitionFinished;
	
	Saved_bWantsToMantle = CharacterMovement->Safe_bWantsToMantle;

	Saved_bVertWallRun = CharacterMovement->Safe_bVertWallRun;

}

void UCustomCharacterMovementComponent::FSavedMove_Custom::PrepMoveFor(ACharacter* C)
{
	FSavedMove_Character::PrepMoveFor(C);
	
	UCustomCharacterMovementComponent* CharacterMovement = Cast<UCustomCharacterMovementComponent>(C->GetCharacterMovement());

	CharacterMovement->Safe_bWantsToSprint = Saved_bWantsToSprint;
	CharacterMovement->CustomCharacterOwner->bPressedCustomJump = Saved_bPressedCustomJump;

	CharacterMovement->Safe_bHadAnimRootMotion = Saved_bHadAnimRootMotion;
	CharacterMovement->Safe_bTransitionFinished = Saved_bTransitionFinished;

	CharacterMovement->Safe_bWantsToMantle = Saved_bWantsToMantle;

	CharacterMovement->Safe_bVertWallRun  = Saved_bVertWallRun;

}

#pragma endregion SavedMove_Custom

#pragma region NetworkPredictionData
UCustomCharacterMovementComponent::FNetworkPredictionData_Client_Custom::FNetworkPredictionData_Client_Custom(const UCharacterMovementComponent& ClientMovement)
: Super(ClientMovement)
{
}

FSavedMovePtr UCustomCharacterMovementComponent::FNetworkPredictionData_Client_Custom::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Custom());
}

//Create Client Prediction Data that references our movement component
FNetworkPredictionData_Client* UCustomCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr)

	if (ClientPredictionData == nullptr)
	{
		UCustomCharacterMovementComponent* MutableThis = const_cast<UCustomCharacterMovementComponent*>(this);

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Custom(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f; 
	}
		return ClientPredictionData;
}

void UCustomCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UCustomCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	
	CustomCharacterOwner = Cast<ACustomCMCCharacter>(GetOwner());
}


#pragma endregion NetworkPredictionData
// FirstThingCalledInPerformMovement
void UCustomCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Try LedgeGrab commentMore actions
	if (CustomCharacterOwner->bPressedCustomJump)
	{
		if (TryLedgeGrab())
		{
			CustomCharacterOwner->StopJumping();		
		}
		else
		{
			//SLOG("Failed LedgeGrab, Reverting to jump")
			CustomCharacterOwner->bPressedCustomJump = false;
			CharacterOwner->bPressedJump = true;
			CharacterOwner->CheckJumpInput(DeltaSeconds);
		}
	}

	// Transition LedgeGrab
	if (Safe_bTransitionFinished)
	{
		if (TransitionName == "LedgeGrab")
		{
			SetMovementMode(MOVE_Custom, CMOVE_Hang);
			bOrientRotationToMovement = false;
			Velocity = FVector::ZeroVector;
			
		}
		else if (TransitionName == "Mantle")
		{
			//CharacterOwner->PlayAnimMontage(TallLedgeGrabMontage, 1 / TransitionRMS->Duration);


			// setting this further down
			
			/*SetMovementMode(MOVE_Flying);
			CharacterOwner->PlayAnimMontage(TallLedgeGrabMontage, 1 / 1.f);
			TransitionQueuedMontageSpeed = 0.f;
			TransitionQueuedMontage = nullptr;*/
	
		}
		else
		{
			SetMovementMode(MOVE_Walking);
		}
		Safe_bTransitionFinished = false;
		TransitionName = "";
	}
	
		if(IsCustomMovementMode(CMOVE_Hang) && !HasAnimRootMotion())
		{
			if (Safe_bWantsToMantle)
			{
				SetMovementMode(MOVE_Flying);
				CharacterOwner->PlayAnimMontage(TallLedgeGrabMontage, 1.f);
				TransitionQueuedMontageSpeed = 0.f;
				TransitionQueuedMontage = nullptr;
				Safe_bWantsToMantle = false;
			}
		}

	if(IsCustomMovementMode(CMOVE_Hang) && bWantsToCrouch)
	{
	
			SetMovementMode(MOVE_Falling);
			bWantsToCrouch = false;
	}

	// Wall RunAdd commentMore actions
	if (IsFalling())
	{
		TryVertWallRun();
		//SLOG("TriedWallRun");
	}
	
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UCustomCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation,
	const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

//The set of actions to take for every tick that the movement mode is updated
}

float UCustomCharacterMovementComponent::GetMaxSpeed() const
{
	//Sprinting Currently Entered Here 
	if (IsMovementMode(MOVE_Walking) && Safe_bWantsToSprint && !IsCrouching()) return MaxSprintSpeed;

	if (MovementMode != MOVE_Custom) return Super::GetMaxSpeed();

	switch (CustomMovementMode)
	{
	case CMOVE_Slide:
		return 330.f;
	case CMOVE_Hang:
		return 100.f;
	case CMOVE_VertWallRun:
		return MaxWallRunSpeed;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode"))
		return -1.f;
	}
}
bool UCustomCharacterMovementComponent::IsMovementMode(EMovementMode InMovementMode) const
{
	return InMovementMode == MovementMode;
}


// Network
void UCustomCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	Safe_bWantsToSprint = (Flags & FSavedMove_Custom::FLAG_Custom_0) != 0;
}


void UCustomCharacterMovementComponent::SprintPressed()
{
	Safe_bWantsToSprint = true;
}
void UCustomCharacterMovementComponent::SprintReleased()
{
	Safe_bWantsToSprint = false;
}


bool UCustomCharacterMovementComponent::IsServer() const
{
	return CharacterOwner->HasAuthority();
}

float UCustomCharacterMovementComponent::CapR() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float UCustomCharacterMovementComponent::CapHH() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

void UCustomCharacterMovementComponent::OnRep_LedgeGrab()
{
	//Probably need a different specifier here
	CharacterOwner->PlayAnimMontage(TallLedgeGrabMontage);
}


void UCustomCharacterMovementComponent::ProcessClimbableSurfaceInfo()
{
	CurrentClimbableSurfaceLocation = FVector::ZeroVector;
	CurrentClimbableSurfaceNormal = FVector::ZeroVector;

	if(ClimbableSurfacesTracedResults.IsEmpty()) return;

	for(const FHitResult& TracedHitResult:ClimbableSurfacesTracedResults)
	{
		CurrentClimbableSurfaceLocation += TracedHitResult.ImpactPoint;
		CurrentClimbableSurfaceNormal += TracedHitResult.ImpactNormal;
	}

	CurrentClimbableSurfaceLocation /= ClimbableSurfacesTracedResults.Num();
	CurrentClimbableSurfaceNormal = CurrentClimbableSurfaceNormal.GetSafeNormal();
}

//Trace for climbable surfaces, return true if there are indeed valid surfaces, false otherwise
bool UCustomCharacterMovementComponent::TraceClimbableSurfaces()
{
	// BASE CODE IS SHORT BLURB BELOW
	const FVector StartOffset = UpdatedComponent->GetForwardVector() * 30.f;
	const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
	const FVector End = Start + UpdatedComponent->GetForwardVector();
	
	ClimbableSurfacesTracedResults = DoCapsuleTraceMultiByObject(Start,End);
	
	
	return !ClimbableSurfacesTracedResults.IsEmpty();
}


TArray<FHitResult> UCustomCharacterMovementComponent::DoCapsuleTraceMultiByObject(const FVector & Start, const FVector & End, bool bShowDebugShape,bool bDrawPersistantShapes)
{	
	TArray<FHitResult> OutCapsuleTraceHitResults;

	EDrawDebugTrace::Type DebugTraceType = EDrawDebugTrace::None;

	if(bShowDebugShape)
	{
		DebugTraceType = EDrawDebugTrace::ForOneFrame;

		if(bDrawPersistantShapes)
		{
			DebugTraceType = EDrawDebugTrace::Persistent;
		}
	}

	UKismetSystemLibrary::CapsuleTraceMultiForObjects(
		this,
		Start,
		End,
		ClimbCapsuleTraceRadius,
		ClimbCapsuleTraceHalfHeight,
		ClimbableSurfaceTraceTypes,
		false,
		TArray<AActor*>(),
		DebugTraceType,
		OutCapsuleTraceHitResults,
		false
	);

	return OutCapsuleTraceHitResults;
}

bool UCustomCharacterMovementComponent::TryVertWallRun()
{
	//Not Falling? cant wall run
	if (!IsFalling()) return false;
	
	// If falling too fast you may not wall run // current is 800
	if (Velocity.Z < -MaxVerticalWallRunSpeed) return false;
	
	//Player Location
	FVector Start = UpdatedComponent->GetComponentLocation();
	
	// Currently casting 1.5 capsules in front of the character
	FVector FrontEnd = Start + UpdatedComponent->GetForwardVector() * CapR() * 3;
	// Get Default Ignore params that include the character
	auto Params = CustomCharacterOwner->GetIgnoreCharacterParams();

	// Init Containers for hit results if any
	FHitResult FloorHit, WallHit;

	
	// Check Player Height off the ground
	if (GetWorld()->LineTraceSingleByProfile(FloorHit, Start, Start + FVector::DownVector * (CapHH() + MinWallRunHeight), "BlockAll", Params))
	{


		return false;
		
	}

	// Front Ray Cast
	GetWorld()->LineTraceSingleByProfile(WallHit, Start, FrontEnd, "BlockAll", Params);
	LINE(Start, FrontEnd, FColor::Orange)
	if (WallHit.IsValidBlockingHit() ) // && (Velocity | WallHit.Normal) > 0
	{
		Safe_bVertWallRun = true;
	
	}
	else
	{
		//SLOG("Wall not Hit")
		return false;
		
	}
	FVector ProjectedVelocity = FVector::VectorPlaneProject(Velocity, WallHit.Normal);
	if (Velocity.SizeSquared() < pow(MinWallRunSpeed, 2)) return false;
	
	// Passed all conditions
	Velocity = ProjectedVelocity;
	// Prevent exagerated vertical and downward movemnt
	//May have to update or modify climb
	Velocity.Z = FMath::Clamp(Velocity.Z, 0.f, MaxVerticalWallRunSpeed);
	SetMovementMode(MOVE_Custom, CMOVE_VertWallRun);
	//SLOG("Starting WallRun")
	return true;
}

void UCustomCharacterMovementComponent::PhysVertWallRun(float deltaTime, int32 Iterations)
{
	
		// Substeping covered in prone
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}
	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	bJustTeleported = false;
	float remainingTime = deltaTime;
	// Perform the move
	// Iterate inside of frame for higher resolution
	while ( (remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)) )
	{
		Iterations++;
		bJustTeleported = false;
		//instead of delta time
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		
		FVector Start = UpdatedComponent->GetComponentLocation();
		FVector FrontEnd = Start + UpdatedComponent->GetForwardVector() * CapR() * 3;
		auto Params = CustomCharacterOwner->GetIgnoreCharacterParams();

		/*// should work if I pull back
		float SinPullAwayAngle = FMath::Sin(FMath::DegreesToRadians(WallRunPullAwayAngle));*/
		
		FHitResult WallHit;
		GetWorld()->LineTraceSingleByProfile(WallHit, Start, FrontEnd, "BlockAll", Params);



		if (!WallHit.IsValidBlockingHit() )
		{
			SetMovementMode(MOVE_Falling);
			// if you leave the wall start new physics in between the frames
			StartNewPhysics(remainingTime, Iterations);

			return;
		}

				
		FVector OriginalAcceleration = Acceleration;
		
		VertRunWallUpDirection = FVector::VectorPlaneProject(FVector::UpVector, WallHit.Normal).GetSafeNormal();
		FVector WallRightDirection = FVector::CrossProduct(VertRunWallUpDirection, WallHit.Normal).GetSafeNormal();

		/*Acceleration = (OriginalAcceleration | UpdatedComponent->GetForwardVector()) * WallUpDirection +
			   (OriginalAcceleration | UpdatedComponent->GetRightVector()) * WallRightDirection;*/
		
		// Debug message for FVector accelDirection
		GEngine->AddOnScreenDebugMessage(
			11, // Using a new key to avoid conflicts
			10.5f,
			FColor::Purple, // Using a different color for clarity
			FString::Printf(TEXT("AccelerationDirection After Wall Up: %s"), *Acceleration.ToString())
		);


		// Apply acceleration to velocity
		CalcVelocity(timeTick, 0.f, false, GetMaxBrakingDeceleration());
		
		////// Project / Clamp Velocity tp Wall normal
		Velocity = FVector::VectorPlaneProject(Velocity,  WallHit.Normal);

		if (Velocity.SizeSquared() < pow(MinWallRunSpeed, 2) || Velocity.Z < -MaxVerticalWallRunSpeed)
		{
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);
			SLOG("TooSlow")
			return;
		}
		
		// Compute move parameters
		const FVector Delta = timeTick * Velocity; // dx = v * dt
		const bool bZeroDelta = Delta.IsNearlyZero();
		if ( bZeroDelta )
		{
			remainingTime = 0.f;
		}
		else
		{
			FHitResult Hit;
			SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
			FVector WallAttractionDelta = -WallHit.Normal * WallAttractionForce * timeTick;
			SafeMoveUpdatedComponent(WallAttractionDelta, UpdatedComponent->GetComponentQuat(), true, Hit);
		}
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}
		//Make Velocity == to how much the Char Actually moved
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick; // v = dx / dt
	}

	FVector Start = UpdatedComponent->GetComponentLocation();
	FVector FrontEnd = Start + UpdatedComponent->GetForwardVector() * CapR() * 2;
	auto Params = CustomCharacterOwner->GetIgnoreCharacterParams();
	
	FHitResult FloorHit, WallHit;
	GetWorld()->LineTraceSingleByProfile(WallHit, Start, FrontEnd, "BlockAll", Params);
	GetWorld()->LineTraceSingleByProfile(FloorHit, Start, Start + FVector::DownVector * (CapHH() + MinWallRunHeight * .5f), "BlockAll", Params);
	if (FloorHit.GetActor())
	{
		AActor* FloorHitActor = FloorHit.GetActor();   
		FString FloorActor = FString::Printf(TEXT("FloorHitActor: %s"), *FloorHitActor->GetName());
		GEngine->AddOnScreenDebugMessage(
		10,
		10.5f,
		FColor::Red,
		FloorActor
	);
	}

	
	LINE(Start, Start + FVector::DownVector * (CapHH() + MinWallRunHeight * .5f), FColor::Purple)
	
	if (FloorHit.IsValidBlockingHit() || !WallHit.IsValidBlockingHit() || Velocity.SizeSquared() < pow(MinWallRunSpeed, 2))
	{
		//SLOG("HitFloor")
		SetMovementMode(MOVE_Falling);
	}
}
