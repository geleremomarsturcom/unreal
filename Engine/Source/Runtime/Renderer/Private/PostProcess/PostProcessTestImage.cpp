// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PostProcess/PostProcessTestImage.h"
#include "PostProcess/PostProcessCombineLUTs.h"
#include "CanvasTypes.h"
#include "RenderTargetTemp.h"

class FTestImagePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTestImagePS);
	SHADER_USE_PARAMETER_STRUCT(FTestImagePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_STRUCT_INCLUDE(FColorRemapParameters, ColorRemap)
		SHADER_PARAMETER(uint32, FrameNumber)
		SHADER_PARAMETER(float, FrameTime)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTestImagePS, "/Engine/Private/PostProcessTestImage.usf", "MainPS", SF_Pixel);

void AddTestImagePass(
	FRDGBuilder& GraphBuilder,
	const FScreenPassViewInfo& ScreenPassView,
	FRDGTextureRef OutputTexture,
	FIntRect OutputViewRect)
{
	check(OutputTexture);
	check(!OutputViewRect.IsEmpty());

	const FScreenPassTextureViewport OutputViewport(OutputViewRect, OutputTexture);

	const FViewInfo& View = ScreenPassView.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FTestImagePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTestImagePS::FParameters>();
	PassParameters->Output = GetScreenPassTextureViewportParameters(OutputViewport);
	PassParameters->ColorRemap = GetColorRemapParameters();
	PassParameters->FrameNumber = ViewFamily.FrameNumber;
	PassParameters->FrameTime = ViewFamily.CurrentRealTime;
	PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ELoad);

	TShaderMapRef<FTestImagePS> PixelShader(View.ShaderMap);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("TestImage %dx%d (PS)", OutputViewport.Rect.Width(), OutputViewport.Rect.Height()),
		PassParameters,
		ERDGPassFlags::Raster,
		[ScreenPassView, OutputTexture, OutputViewport, PixelShader, PassParameters](FRHICommandListImmediate& RHICmdList)
	{
		DrawScreenPass(RHICmdList, ScreenPassView, OutputViewport, OutputViewport, *PixelShader, *PassParameters);

		// Draw debug text
		{
			const FViewInfo& LocalView = ScreenPassView.View;
			const FSceneViewFamily& LocalViewFamily = *LocalView.Family;
			FRenderTargetTemp TempRenderTarget(static_cast<FRHITexture2D*>(OutputTexture->GetRHI()), OutputTexture->Desc.Extent);
			FCanvas Canvas(&TempRenderTarget, nullptr, LocalViewFamily.CurrentRealTime, LocalViewFamily.CurrentWorldTime, LocalViewFamily.DeltaWorldTime, LocalView.GetFeatureLevel());

			float X = 30;
			float Y = 8;
			const float YStep = 14;
			const float ColumnWidth = 250;

			FString Line;

			Line = FString::Printf(TEXT("Top bars:"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("   Moving bars using FrameTime"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("   Black and white raster, Pixel sized, Watch for Moire pattern"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("   Black and white raster, 2x2 block sized"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("Bottom bars:"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("   8 bars near white, 4 right bars should appear as one (HDTV)"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("   8 bars near black, 4 left bars should appear as one (HDTV)"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("   Linear Greyscale in sRGB from 0 to 255"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("Color bars:"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("   Red, Green, Blue"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("Outside:"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("   Moving bars using FrameNumber, Tearing without VSync"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("Circles:"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("   Should be round and centered"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("Border:"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("   4 white pixel sized lines (only visible without overscan)"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));

			const bool bForce = false;
			const bool bInsideRenderPass = true;
			Canvas.Flush_RenderThread(RHICmdList, bForce, bInsideRenderPass);
		}
	});
}

FRenderingCompositeOutputRef AddTestImagePass(FRenderingCompositionGraph& Graph, FRenderingCompositeOutputRef Input)
{
	FRenderingCompositePass* Pass = Graph.RegisterPass(new(FMemStack::Get()) TRCPassForRDG<1, 1>(
		[](FRenderingCompositePass* InPass, FRenderingCompositePassContext& InContext)
	{
		FRDGBuilder GraphBuilder(InContext.RHICmdList);

		const FRDGTextureDesc OutputTextureDesc = FRDGTextureDesc::Create2DDesc(
			InContext.ReferenceBufferSize, PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false);

		FRDGTextureRef SceneTextureOutput = InPass->FindOrCreateRDGTextureForOutput(GraphBuilder, ePId_Output0, OutputTextureDesc, TEXT("TestImage"));

		FScreenPassViewInfo ScreenPassView(InContext.View);

		AddTestImagePass(GraphBuilder, ScreenPassView, SceneTextureOutput, InContext.SceneColorViewRect);

		InPass->ExtractRDGTextureForOutput(GraphBuilder, ePId_Output0, SceneTextureOutput);

		GraphBuilder.Execute();
	}));
	Pass->SetInput(ePId_Input0, Input);
	return FRenderingCompositeOutputRef(Pass);
}
