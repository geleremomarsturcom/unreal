// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTonemap.h: Post processing tone mapping implementation, can add bloom.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RendererInterface.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "RHIStaticStates.h"
#include "GlobalShader.h"
#include "PostProcessParameters.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessEyeAdaptation.h"
#include "Math/Halton.h"


static void GrainRandomFromFrame(FVector* RESTRICT const Constant, uint32 FrameNumber)
{
	Constant->X = Halton(FrameNumber & 1023, 2);
	Constant->Y = Halton(FrameNumber & 1023, 3);
}


BEGIN_SHADER_PARAMETER_STRUCT(FMobileFilmTonemapParameters, )
	SHADER_PARAMETER(FVector4, ColorMatrixR_ColorCurveCd1)
	SHADER_PARAMETER(FVector4, ColorMatrixG_ColorCurveCd3Cm3)
	SHADER_PARAMETER(FVector4, ColorMatrixB_ColorCurveCm2)
	SHADER_PARAMETER(FVector4, ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3)
	SHADER_PARAMETER(FVector4, ColorCurve_Ch1_Ch2)
	SHADER_PARAMETER(FVector4, ColorShadow_Luma)
	SHADER_PARAMETER(FVector4, ColorShadow_Tint1)
	SHADER_PARAMETER(FVector4, ColorShadow_Tint2)
END_SHADER_PARAMETER_STRUCT()

FMobileFilmTonemapParameters GetMobileFilmTonemapParameters(const FPostProcessSettings& PostProcessSettings, bool bUseColorMatrix, bool bUseShadowTint, bool bUseContrast);

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor
// ePId_Input1: BloomCombined (not needed for bDoGammaOnly)
// ePId_Input2: EyeAdaptation (not needed for bDoGammaOnly)
// ePId_Input3: LUTsCombined (not needed for bDoGammaOnly)
class FRCPassPostProcessTonemap : public TRenderingCompositePassBase<4, 1>
{
public:
	// constructor
	FRCPassPostProcessTonemap(const FViewInfo& InView, bool bInDoGammaOnly, bool bDoEyeAdaptation, bool bHDROutput, bool InIsComputePass);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	virtual FRHIComputeFence* GetComputePassEndFence() const override { return AsyncEndFence; }

	bool bDoGammaOnly;
	bool bDoScreenPercentageInTonemapper;
private:
	bool bDoEyeAdaptation;
	bool bHDROutput;

	const FViewInfo& View;

	FComputeFenceRHIRef AsyncEndFence;
};



// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor
// ePId_Input1: BloomCombined (not needed for bDoGammaOnly)
// ePId_Input2: Dof (not needed for bDoGammaOnly)
class FRCPassPostProcessTonemapES2 : public TRenderingCompositePassBase<3, 1>
{
public:
	FRCPassPostProcessTonemapES2(const FViewInfo& View, bool bInUsedFramebufferFetch, bool bInSRGBAwareTarget);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	
	bool bDoScreenPercentageInTonemapper;

private:
	const FViewInfo& View;

	bool bUsedFramebufferFetch;
	bool bSRGBAwareTarget;
};


/** Encapsulates the post processing tone map vertex shader. */
class FPostProcessTonemapVS : public FGlobalShader
{
	// This class is in the header so that Temporal AA can share this vertex shader.
	DECLARE_GLOBAL_SHADER(FPostProcessTonemapVS);

	class FTonemapperVSSwitchAxis : SHADER_PERMUTATION_BOOL("NEEDTOSWITCHVERTICLEAXIS");
	class FTonemapperVSUseAutoExposure : SHADER_PERMUTATION_BOOL("EYEADAPTATION_EXPOSURE_FIX");
	using FPermutationDomain = TShaderPermutationDomain<
		FTonemapperVSSwitchAxis,
		FTonemapperVSUseAutoExposure
	>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		// Prevent switch axis permutation on platforms that dont require it.
		if (PermutationVector.Get<FTonemapperVSSwitchAxis>() && !RHINeedsToSwitchVerticalAxis(Parameters.Platform))
		{
			return false;
		}
		return true;
	}

	/** Default constructor. */
	FPostProcessTonemapVS(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter EyeAdaptation;
	FShaderParameter GrainRandomFull;
	FShaderParameter DefaultEyeExposure;
	FShaderParameter ScreenPosToScenePixel;

	/** Initialization constructor. */
	FPostProcessTonemapVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		EyeAdaptation.Bind(Initializer.ParameterMap, TEXT("EyeAdaptation"));
		GrainRandomFull.Bind(Initializer.ParameterMap, TEXT("GrainRandomFull"));
		DefaultEyeExposure.Bind(Initializer.ParameterMap, TEXT("DefaultEyeExposure"));
		ScreenPosToScenePixel.Bind(Initializer.ParameterMap, TEXT("ScreenPosToScenePixel"));
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static FPermutationDomain BuildPermutationVector(bool bDoEyeAdaptation, bool bNeedsToSwitchVerticalAxis)
	{
		FPermutationDomain PermutationVector;
		PermutationVector.Set<FTonemapperVSSwitchAxis>(bNeedsToSwitchVerticalAxis);
		PermutationVector.Set<FTonemapperVSUseAutoExposure>(bDoEyeAdaptation);

		return PermutationVector;
	}

	void TransitionResources(const FRenderingCompositePassContext& Context)
	{
		if (Context.View.HasValidEyeAdaptation())
		{
			IPooledRenderTarget* EyeAdaptationRT = Context.View.GetEyeAdaptation(Context.RHICmdList);
			FRHITexture* EyeAdaptationRTRef = EyeAdaptationRT->GetRenderTargetItem().TargetableTexture;
			if (EyeAdaptationRTRef)
			{
				Context.RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, &EyeAdaptationRTRef, 1);
			}
		}
	}

	void SetVS(const FRenderingCompositePassContext& Context, const FPermutationDomain& PermutationVector )
	{
		FRHIVertexShader* ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		FVector GrainRandomFullValue;
		{
			uint8 FrameIndexMod8 = 0;
			if (Context.View.State)
			{
				FrameIndexMod8 = Context.View.ViewState->GetFrameIndex(8);
			}
			GrainRandomFromFrame(&GrainRandomFullValue, FrameIndexMod8);
		}

		SetShaderValue(Context.RHICmdList, ShaderRHI, GrainRandomFull, GrainRandomFullValue);

		
		if (Context.View.HasValidEyeAdaptation())
		{
			IPooledRenderTarget* EyeAdaptationRT = Context.View.GetEyeAdaptation(Context.RHICmdList);
			FRHITexture* EyeAdaptationRTRef = EyeAdaptationRT->GetRenderTargetItem().TargetableTexture;
			if (EyeAdaptationRTRef)
			{
				//Context.RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, &EyeAdaptationRTRef, 1);
			}
			SetTextureParameter(Context.RHICmdList, ShaderRHI, EyeAdaptation, EyeAdaptationRT->GetRenderTargetItem().TargetableTexture);
		}
		else
		{
			// some views don't have a state, thumbnail rendering?
			SetTextureParameter(Context.RHICmdList, ShaderRHI, EyeAdaptation, GWhiteTexture->TextureRHI);
		}

		// Compile time template-based conditional
		if (!PermutationVector.Get<FTonemapperVSUseAutoExposure>())
		{
			// Compute a CPU-based default.  NB: reverts to "1" if SM5 feature level is not supported
			float FixedExposure = GetEyeAdaptationFixedExposure(Context.View);
			// Load a default value 
			SetShaderValue(Context.RHICmdList, ShaderRHI, DefaultEyeExposure, FixedExposure);
		}

		{
			FIntPoint ViewportOffset = Context.SceneColorViewRect.Min;
			FIntPoint ViewportExtent = Context.SceneColorViewRect.Size();
			FVector4 ScreenPosToScenePixelValue(
				ViewportExtent.X * 0.5f,
				-ViewportExtent.Y * 0.5f,
				ViewportExtent.X * 0.5f - 0.5f + ViewportOffset.X,
				ViewportExtent.Y * 0.5f - 0.5f + ViewportOffset.Y);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ScreenPosToScenePixel, ScreenPosToScenePixelValue);
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << GrainRandomFull << EyeAdaptation << DefaultEyeExposure << ScreenPosToScenePixel;

		return bShaderHasOutdatedParameters;
	}
};
