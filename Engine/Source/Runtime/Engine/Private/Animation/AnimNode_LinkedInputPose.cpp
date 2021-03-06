// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_LinkedInputPose.h"
#include "Animation/AnimInstanceProxy.h"

const FName FAnimNode_LinkedInputPose::DefaultInputPoseName("InPose");

void FAnimNode_LinkedInputPose::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	if(InputProxy)
	{
		FAnimationInitializeContext InputContext(InputProxy);
		InputPose.Initialize(InputContext);
	}
}

void FAnimNode_LinkedInputPose::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	if(InputProxy)
	{
		FAnimationCacheBonesContext InputContext(InputProxy);
		InputPose.CacheBones(InputContext);
	}
}

void FAnimNode_LinkedInputPose::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	if(InputProxy)
	{
		FAnimationUpdateContext InputContext = Context.WithOtherProxy(InputProxy);
		InputPose.Update(InputContext);
	}
}

void FAnimNode_LinkedInputPose::Evaluate_AnyThread(FPoseContext& Output)
{
	if(InputProxy)
	{
		Output.Pose.SetBoneContainer(&InputProxy->GetRequiredBones());

		FPoseContext InputContext(InputProxy, Output.ExpectsAdditivePose());
		InputPose.Evaluate(InputContext);

		Output.Pose.MoveBonesFrom(InputContext.Pose);
		Output.Curve.MoveFrom(InputContext.Curve);
	}
	else if(CachedInputPose.IsValid() && CachedInputCurve.IsValid())
	{
		Output.Pose.CopyBonesFrom(CachedInputPose);
		Output.Curve.CopyFrom(CachedInputCurve);
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_LinkedInputPose::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);

	if(InputProxy)
	{
		InputPose.GatherDebugData(DebugData);
	}
}

void FAnimNode_LinkedInputPose::DynamicLink(FAnimInstanceProxy* InInputProxy, FPoseLinkBase* InPoseLink)
{
	check(InputProxy == nullptr);	// Must be unlinked before re-linking

	InputProxy = InInputProxy;
	InputPose.SetDynamicLinkNode(InPoseLink);
}

void FAnimNode_LinkedInputPose::DynamicUnlink()
{
	check(InputProxy != nullptr);	// Must be linked before unlinking

	InputProxy = nullptr;
	InputPose.SetDynamicLinkNode(nullptr);
}