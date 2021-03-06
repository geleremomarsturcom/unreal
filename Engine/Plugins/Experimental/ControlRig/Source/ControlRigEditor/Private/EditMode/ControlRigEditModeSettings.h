// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/LazyObjectPtr.h"
#include "ControlRigEditModeSettings.generated.h"

class AActor;
class UControlRig;
class UControlRigSequence;

/** Settings object used to show useful information in the details panel */
UCLASS()
class UControlRigEditModeSettings : public UObject
{
	GENERATED_BODY()

	UControlRigEditModeSettings()
		: bDisplayHierarchy(false)
		, bShowManipulatorsDuringPlayback(true)
		, bHideManipulators(false)
		, bDisplayAxesOnSelection(true)
		, AxisScale(10.f)
	{}

	// UObject interface
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

public:
	/** Sequence to animate */
	UPROPERTY(EditAnywhere, Category = "Animation", NoClear)
	UControlRigSequence* Sequence;

	/** The actor we are currently animating */
	UPROPERTY(EditAnywhere, Category = "Animation")
	TLazyObjectPtr<AActor> Actor;

	/** Whether to show all nodes in the hierarchy being animated */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Animation")
	bool bDisplayHierarchy;

	/** Whether to show manipulators when animations are being played back */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Animation")
	bool bShowManipulatorsDuringPlayback;

	/** Should we always hide manipulators in viewport */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Animation")
	bool bHideManipulators;

	/** Should we show axes for the selected elements */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Animation")
	bool bDisplayAxesOnSelection;

	/** The scale for axes to draw on the selection */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Animation")
	float AxisScale;

private:
	/** Cache the previous actor for pre/post edit change handling */
	AActor* PrevActor;
};