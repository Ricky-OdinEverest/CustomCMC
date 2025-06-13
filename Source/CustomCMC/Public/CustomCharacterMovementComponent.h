// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CustomCharacterMovementComponent.generated.h"

/*On tick you will call perform move which executes the movement logic
 *
 * Then it will set the saved move to the safe move and check if it can be combined with other moves
 * Then it will GetCompressed Flags to reduce move into a networkable packet that it can send to the server
 * the server will then call update from compressed flags and update our movement variables in addition to performing the move to see if ther
 * are any needed corrections
 */

// Custom movement modes are needed when we declare new physics.
// Since a slide requies physics calculation it needs more parameters
UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_None			UMETA(Hidden),
	CMOVE_Slide			UMETA(DisplayName = "Slide"),
	CMOVE_Hang			UMETA(DisplayName = "Hang"),
	CMOVE_MAX			UMETA(Hidden),
};


/**
 * 
 */
UCLASS()
class CUSTOMCMC_API UCustomCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UCustomCharacterMovementComponent();
	UFUNCTION(BlueprintPure)
	bool IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const;

private:
	
	// Allows Character To use private variables
	friend class ACustomCMCCharacter;
	
	// This class sends a lightweight version of our movement to the server
	class FSavedMove_Custom : public FSavedMove_Character
	{

		// For Movement States That need to be updated regularly
		// State Replicated from server to client
		enum CompressedFlags
		{
			FLAG_Sprint			= 0x10,
			FLAG_Dash			= 0x20,
			FLAG_LedgeGrab		= 0x40,
			FLAG_Custom_3		= 0x80,
		};

		//Flags
		uint8 Saved_bWantsToSprint:1;
		uint8 Saved_bPressedCustomJump:1;
		

		uint8 Saved_bHadAnimRootMotion:1;
		uint8 Saved_bTransitionFinished:1;

	public:
		FSavedMove_Custom();

		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
		virtual void Clear() override;
		virtual uint8 GetCompressedFlags() const override;
		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
		virtual void PrepMoveFor(ACharacter* C) override;
	};

	// Needed to switch to our custom movement
	class FNetworkPredictionData_Client_Custom : public FNetworkPredictionData_Client_Character
	{
	public:
		FNetworkPredictionData_Client_Custom(const UCharacterMovementComponent& ClientMovement);

		typedef FNetworkPredictionData_Client_Character Super;

		virtual FSavedMovePtr AllocateNewMove() override;
	};
	
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
#pragma region Overrides
	
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Actor Component

	virtual void InitializeComponent() override;

protected:


	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;
	
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

	virtual bool IsMovingOnGround() const override;
	
	virtual bool CanCrouchInCurrentState() const override;
	
	virtual float GetMaxSpeed() const override;

	virtual float GetMaxBrakingDeceleration() const override;
	// attempting to use in climb to stop downward velocity during hang state
	virtual float GetGravityZ() const override;
	
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	// jump overrides used in wall run
	virtual bool CanAttemptJump() const override;
	virtual bool DoJump(bool bReplayingMoves) override;

# pragma endregion Overrides

	
	FString GetMovementModeAsString(EMovementMode MoveMode, uint8 CustomMode) const;
	bool IsMovementMode(EMovementMode InMovementMode) const;

	// Flags Set On Clients to signal intention to move
	// if a player presses shift they probably want to sprint
	// These are the flags we manipulate through actions though they may be modified by the server if a correction is required

	//Flags
	bool Safe_bWantsToSprint;
	bool Safe_bHadAnimRootMotion;
	
	bool Safe_bTransitionFinished;
	TSharedPtr<FRootMotionSource_MoveToForce> TransitionRMS;
	UPROPERTY(Transient) UAnimMontage* TransitionQueuedMontage;
	float TransitionQueuedMontageSpeed;
	int TransitionRMS_ID;
	
	UPROPERTY(EditDefaultsOnly)
	float MaxSprintSpeed=750.f;

	//Replication

	UPROPERTY(ReplicatedUsing=OnRep_LedgeGrab)
	bool Proxy_bLedgeGrabbed;



	
#pragma region LedgeGrabVariables
	// LedgeGrab
	//How far away can you LedgeGrab from
	UPROPERTY(EditDefaultsOnly)
	float MaxLedgeGrabDistance = 200;
	
	// How high can you reach to LedgeGrab
	UPROPERTY(EditDefaultsOnly)
	float LedgeGrabReachHeight = 50;
	
	UPROPERTY(EditDefaultsOnly)
	float MinLedgeGrabDepth = 30;
	
	UPROPERTY(EditDefaultsOnly)
	float LedgeGrabMinWallSteepnessAngle = 75;
	
	UPROPERTY(EditDefaultsOnly)
	float LedgeGrabMaxSurfaceAngle = 40;
	
	UPROPERTY(EditDefaultsOnly)
	float LedgeGrabMaxAlignmentAngle = 45;

	//Transition montages enter the main movement
	UPROPERTY(EditDefaultsOnly)
	UAnimMontage* TallLedgeGrabMontage;
	UPROPERTY(EditDefaultsOnly)
	UAnimMontage* TransitionTallLedgeGrabMontage;
	
	UPROPERTY(EditDefaultsOnly)
	UAnimMontage* ProxyShortLedgeGrabMontage;
	
	UPROPERTY(EditDefaultsOnly)
	UAnimMontage* ProxyTallLedgeGrabMontage;

	UPROPERTY(EditDefaultsOnly)
	float LedgeGrabZOffset = 40.f;

	UPROPERTY(EditDefaultsOnly)
	float LedgeGrabXOffset = -10.f;

	float LedgeGrabSpeed = 100.f;
	float LedgeBrakingDeceleration = 10000.f;
# pragma endregion LedgeGrabVariables

	UPROPERTY(Transient)
	class ACustomCMCCharacter* CustomCharacterOwner;
	// LedgeGrab 
	bool TryLedgeGrab();
	void PhysHang(float deltaTime, int32 Iterations);

	FVector GetLedgeGrabStartLocation(FHitResult FrontHit, FHitResult SurfaceHit) const;
	//Custom To Be Replaced
	FVector GetLedgeGrabCurrentLocation(FHitResult FrontHit, FHitResult SurfaceHit) const;
	void SnapMovementToClimableSurfaces(float DeltaTime);
	FQuat GetClimbRotation(float DeltaTime);
	bool CheckShouldStopHanging();
	bool CheckHasReachedFloor();
	void StopHanging();

	FHitResult CurrentFrontHit;
	FHitResult CurrentTopSurfaceHit;

	// movement component header:
	FVector CurrentLedgeWallNormal;    // the wall you grabbed
	FVector CurrentLedgeTopNormal;     // the tabletop you grabbed
	
	FVector testTangent;  

	// Corner / angle movement smoothing factor... Higher == Faster
	UPROPERTY(EditAnywhere, Category="Climb|Hang")
	float CornerInterpSpeed = 10.f;

public:
#pragma region InputEvents
	UFUNCTION(BlueprintCallable)
	void SprintPressed();
	
	UFUNCTION(BlueprintCallable)
	void SprintReleased();
#pragma endregion InputEvents

	UFUNCTION(BlueprintPure) bool IsHanging() const { return IsCustomMovementMode(CMOVE_Hang); }
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FVector GetUnrotatedClimbVelocity() const;

	FORCEINLINE FVector GetClimbableSurfaceNormal() const {return CurrentClimbableSurfaceNormal;}

	

	FORCEINLINE FVector GetCurrentLedgeTangent() const { return CurrentLedgeTangent; }


private:
	//Helpers
	bool IsServer() const;
	float CapR() const;
	float CapHH() const;

	UFUNCTION()
	void OnRep_LedgeGrab();

	// Ledge Detection Experiment
	FVector CurrentLedgeTangent;

	


	// Climb Project Functions and variables
	void ProcessClimbableSurfaceInfo();
	bool TraceClimbableSurfaces();
	TArray<FHitResult> DoCapsuleTraceMultiByObject(const FVector& Start,const FVector& End,bool bShowDebugShape = false,bool bDrawPersistantShapes = false);

	FVector CurrentClimbableSurfaceNormal;

	TArray<FHitResult> ClimbableSurfacesTracedResults;

	FVector CurrentClimbableSurfaceLocation;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "Character Movement: Climbing",meta = (AllowPrivateAccess = "true"))
	TArray<TEnumAsByte<EObjectTypeQuery> > ClimbableSurfaceTraceTypes;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "Character Movement: Climbing",meta = (AllowPrivateAccess = "true"))
	float ClimbCapsuleTraceRadius = 50.f;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "Character Movement: Climbing",meta = (AllowPrivateAccess = "true"))
	float ClimbCapsuleTraceHalfHeight = 72.f;
	
};
