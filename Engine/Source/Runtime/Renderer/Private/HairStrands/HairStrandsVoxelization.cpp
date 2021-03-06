// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "HairStrandsVoxelization.h"
#include "HairStrandsRasterCommon.h"
#include "HairStrandsCluster.h"
#include "HairStrandsUtils.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterStruct.h"
#include "SceneTextureParameters.h"
#include "RenderGraphUtils.h"
#include "PostProcessing.h"

static float GStrandHairVoxelizationRasterizationScale = 1.0f;
static FAutoConsoleVariableRef CVarStrandHairVoxelizationRasterizationScale(TEXT("r.HairStrands.VoxelizationRasterizationScale"), GStrandHairVoxelizationRasterizationScale, TEXT("Rasterization scale to snap strand to pixel for voxelization"));

static int32 GHairVoxelizationResolution = 256;
static FAutoConsoleVariableRef CVarGHairVoxelizationResolution(TEXT("r.HairStrands.Voxelization.Resolution"), GHairVoxelizationResolution, TEXT("Change the resolution of the voxelization volume for hair strands"));

static int32 GHairVoxelizationEnable = 1;
static FAutoConsoleVariableRef CVarGHairVoxelizationEnable(TEXT("r.HairStrands.Voxelization"), GHairVoxelizationEnable, TEXT("Enable hair voxelization for transmittance evaluation"));

static int32 GHairVoxelizationMaterialEnable = 0;
static FAutoConsoleVariableRef CVarHairVoxelizationMaterialEnable(TEXT("r.HairStrands.VoxelizationMaterial"), GHairVoxelizationMaterialEnable, TEXT("Enable hair material voxelization for LOD evaluation"));

static float GHairVoxelizationAABBScale = 1.0f;
static FAutoConsoleVariableRef CVarHairVoxelizationAABBScale(TEXT("r.HairStrands.Voxelization.AABBScale"), GHairVoxelizationAABBScale, TEXT("Scale the hair cluster bounding box"));

static float GHairVoxelizationDensityScale = 1.0f;
static float GHairVoxelizationDepthBiasScale = 3.0f;
static FAutoConsoleVariableRef CVarHairVoxelizationDensityScale(TEXT("r.HairStrands.Voxelization.DensityScale"), GHairVoxelizationDensityScale, TEXT("Scale the hair density when computing voxel transmittance. Default value is 2 (arbitraty)"));
static FAutoConsoleVariableRef CVarHairVoxelizationDepthBiasScale(TEXT("r.HairStrands.Voxelization.DepthBiasScale"), GHairVoxelizationDepthBiasScale, TEXT("Set depth bias for voxel ray marching. Offset the origin position towards the light"));

static int32 GHairVoxelInjectOpaqueDepthEnable = 1;
static FAutoConsoleVariableRef CVarHairVoxelInjectOpaqueDepthEnable(TEXT("r.HairStrands.Voxelization.InjectOpaqueDepth"), GHairVoxelInjectOpaqueDepthEnable, TEXT("Inject opaque geometry depth into the voxel volume for acting as occluder."));

float GetHairStrandsVoxelizationDensityScale() { return FMath::Max(0.0f, GHairVoxelizationDensityScale); }
float GetHairStrandsVoxelizationDepthBiasScale() { return FMath::Max(0.0f, GHairVoxelizationDepthBiasScale); }

static int32 GHairForVoxelTransmittanceAndShadow = 0;
static FAutoConsoleVariableRef CVarHairForVoxelTransmittanceAndShadow(TEXT("r.HairStrands.Voxelization.ForceTransmittanceAndShadow"), GHairForVoxelTransmittanceAndShadow, TEXT("For transmittance and shadow to be computed with density volume. This requires voxelization is enabled."));

bool IsHairStrandsVoxelizationEnable()
{
	return GHairVoxelizationEnable > 0;
}

bool IsHairStrandsForVoxelTransmittanceAndShadowEnable()
{
	return IsHairStrandsVoxelizationEnable() && GHairForVoxelTransmittanceAndShadow > 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
class FVoxelInjectOpaquePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVoxelInjectOpaquePS);
	SHADER_USE_PARAMETER_STRUCT(FVoxelInjectOpaquePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)

		SHADER_PARAMETER(FVector, VoxelMinAABB)
		SHADER_PARAMETER(uint32, VoxelResolution)
		SHADER_PARAMETER(FVector, VoxelMaxAABB)
		SHADER_PARAMETER(FVector2D, OutputResolution)
		SHADER_PARAMETER(FVector2D, SceneDepthResolution)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, DensityTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(Parameters.Platform); }
};

IMPLEMENT_GLOBAL_SHADER(FVoxelInjectOpaquePS, "/Engine/Private/HairStrands/HairStrandsVoxelOpaque.usf", "MainPS", SF_Pixel);

static void AddVoxelInjectOpaquePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FHairStrandsClusterData& Cluster)
{
	FSceneTextureParameters SceneTextures;
	SetupSceneTextureParameters(GraphBuilder, &SceneTextures);

	// #hair_todo: change this to a CS. PS was for easing debugging
	const uint32 LinearizeRes = FMath::Sqrt(Cluster.VoxelResources.DensityTexture->GetDesc().Depth);

	FIntPoint Resolution;
	Resolution.X = Cluster.VoxelResources.DensityTexture->GetDesc().Extent.X * LinearizeRes;
	Resolution.Y = Cluster.VoxelResources.DensityTexture->GetDesc().Extent.Y * LinearizeRes;

	if (!Cluster.VoxelResources.DensityTexture)
		return;

	const FRDGTextureRef VoxelDensityTexture = GraphBuilder.RegisterExternalTexture(Cluster.VoxelResources.DensityTexture, TEXT("HairVoxelDensityTexture"));

	FRDGTextureDesc OutputDesc;
	OutputDesc.Extent.X = Resolution.X;
	OutputDesc.Extent.Y = Resolution.Y;
	OutputDesc.Depth = 0;
	OutputDesc.Format = PF_FloatRGBA;
	OutputDesc.NumMips = 1;
	OutputDesc.Flags = 0;
	OutputDesc.TargetableFlags = TexCreate_RenderTargetable;
	FRDGTextureRef DummyTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("HairVoxelInjectDepth")); // Dummy texture for debugging. Convert this pass into a compute shader.

	FVoxelInjectOpaquePS::FParameters* Parameters = GraphBuilder.AllocParameters<FVoxelInjectOpaquePS::FParameters>();
	Parameters->ViewUniformBuffer = View.ViewUniformBuffer;
	Parameters->OutputResolution = Resolution;
	Parameters->SceneDepthResolution = SceneTextures.SceneDepthBuffer->Desc.Extent;
	Parameters->SceneDepthTexture = SceneTextures.SceneDepthBuffer;
	Parameters->SceneTextures = SceneTextures;
	Parameters->DensityTexture = GraphBuilder.CreateUAV(VoxelDensityTexture);
	Parameters->VoxelMinAABB = Cluster.GetMinBound();
	Parameters->VoxelMaxAABB = Cluster.GetMaxBound();
	Parameters->VoxelResolution = Cluster.GetResolution();
	Parameters->LinearSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters->RenderTargets[0] = FRenderTargetBinding(DummyTexture, ERenderTargetLoadAction::EClear);

	const FIntPoint OutputResolution = SceneTextures.SceneDepthBuffer->Desc.Extent;
	TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FVoxelInjectOpaquePS> PixelShader(View.ShaderMap);
	const TShaderMap<FGlobalShaderType>* GlobalShaderMap = View.ShaderMap;
	const FIntRect Viewport(0, 0, Resolution.X, Resolution.Y);
	const FViewInfo* CapturedView = &View;
	
	ClearUnusedGraphResources(*PixelShader, Parameters);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("HairStrandsVoxelInjectOpaque"),
		Parameters,
		ERDGPassFlags::Raster,
		[Parameters, VertexShader, PixelShader, Viewport, Resolution, CapturedView](FRHICommandList& RHICmdList)
	{
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		VertexShader->SetParameters(RHICmdList, CapturedView->ViewUniformBuffer);
		RHICmdList.SetViewport(Viewport.Min.X, Viewport.Min.Y, 0.0f, Viewport.Max.X, Viewport.Max.Y, 1.0f);
		SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), *Parameters);

		DrawRectangle(
			RHICmdList,
			0, 0,
			Viewport.Width(), Viewport.Height(),
			Viewport.Min.X, Viewport.Min.Y,
			Viewport.Width(), Viewport.Height(),
			Viewport.Size(),
			Resolution,
			*VertexShader,
			EDRF_UseTriangleOptimization);
	});
}

///////////////////////////////////////////////////////////////////////////////////////////////////
class FVoxelGenerateMipCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVoxelGenerateMipCS);
	SHADER_USE_PARAMETER_STRUCT(FVoxelGenerateMipCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)

		SHADER_PARAMETER(uint32, VoxelResolution)
		SHADER_PARAMETER(uint32, SourceMip)
		SHADER_PARAMETER(uint32, TargetMip)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, InDensityTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, OutDensityTexture)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(Parameters.Platform); }
};

IMPLEMENT_GLOBAL_SHADER(FVoxelGenerateMipCS, "/Engine/Private/HairStrands/HairStrandsVoxelMip.usf", "MainCS", SF_Compute);

static void AddVoxelGenerateMipPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FHairStrandsClusterData& Cluster)
{
	if (!Cluster.VoxelResources.DensityTexture)
		return;

	TRefCountPtr<IPooledRenderTarget> DensityTexture = Cluster.VoxelResources.DensityTexture;
	check(DensityTexture->GetDesc().Extent.X == DensityTexture->GetDesc().Extent.Y);

	const uint32 VoxelResolution = DensityTexture->GetDesc().Extent.X;
	const uint32 MipCount = DensityTexture->GetDesc().NumMips;
	FRDGTextureRef VoxelDensityTexture = GraphBuilder.RegisterExternalTexture(DensityTexture, TEXT("HairVoxelDensityTexture"));
	for (uint32 MipIt = 0; MipIt < MipCount-1; ++MipIt)
	{
		FVoxelGenerateMipCS::FParameters* Parameters = GraphBuilder.AllocParameters<FVoxelGenerateMipCS::FParameters>();
		Parameters->InDensityTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(VoxelDensityTexture, MipIt));
		Parameters->OutDensityTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(VoxelDensityTexture, MipIt+1));
		Parameters->VoxelResolution = VoxelResolution;
		Parameters->SourceMip = MipIt;
		Parameters->TargetMip = MipIt+1;

		uint32 SourceResolution = VoxelResolution >> (MipIt);
		uint32 TargetResolution = VoxelResolution >> (MipIt + 1);

		TShaderMapRef<FVoxelGenerateMipCS> ComputeShader(View.ShaderMap);
		const TShaderMap<FGlobalShaderType>* GlobalShaderMap = View.ShaderMap;
		const FIntVector DispatchCount = FComputeShaderUtils::GetGroupCount(FIntVector(TargetResolution, TargetResolution, TargetResolution), FIntVector(4, 4, 4));
		
		ClearUnusedGraphResources(*ComputeShader, Parameters);
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("HairStrandsVoxelMip"),
			Parameters,
			ERDGPassFlags::Compute | ERDGPassFlags::GenerateMips,
			[Parameters, ComputeShader, DispatchCount](FRHICommandList& RHICmdList)
		{
			FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, *Parameters, DispatchCount);
		});
	}
}

static void RenderVoxelization(
	FRHICommandListImmediate& RHICmdList,
	const FScene* Scene,
	const FViewInfo* ViewInfo,
	FHairStrandsClusterData& Cluster)
{
	const FHairStrandsClusterData::TPrimitiveInfos& PrimitiveSceneInfo = Cluster.PrimitivesInfos;
	const FBoxSphereBounds& Bounds = Cluster.Bounds;
	FHairStrandsVoxelResources& VoxelResources = Cluster.VoxelResources;

	DECLARE_GPU_STAT(HairStrandsVoxelization);
	SCOPED_DRAW_EVENT(RHICmdList, HairStrandsVoxelization);
	SCOPED_GPU_STAT(RHICmdList, HairStrandsVoxelization);

	const uint32 ResolutionDim = FMath::RoundUpToPowerOfTwo(GHairVoxelizationResolution);
	const EPixelFormat Format = PF_R32_UINT;
	const FIntPoint Resolution(ResolutionDim, ResolutionDim);
	uint32 MipCount = 1;
	{
		uint32 CurrentResolution = ResolutionDim;
		while (CurrentResolution > 4)
		{
			++MipCount;
			CurrentResolution = CurrentResolution >> 1;
		}
	}

	// todo remove this dummy texture
	TRefCountPtr<IPooledRenderTarget> DummyTexture;
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Resolution, PF_R32_UINT, FClearValueBinding::Black, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DummyTexture, TEXT("DummyTexture"));
	}

	const bool bVoxelizeMaterial = GHairVoxelizationMaterialEnable > 0;
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::CreateVolumeDesc(ResolutionDim, ResolutionDim, ResolutionDim, PF_R32_UINT, FClearValueBinding::Black, TexCreate_None, TexCreate_UAV | TexCreate_ShaderResource, false, MipCount));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, VoxelResources.DensityTexture, TEXT("DensityTexture"));
		if (bVoxelizeMaterial)
		{
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, VoxelResources.TangentXTexture, TEXT("TangentXTexture"));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, VoxelResources.TangentYTexture, TEXT("TangentYTexture"));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, VoxelResources.TangentZTexture, TEXT("TangentZTexture"));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, VoxelResources.MaterialTexture, TEXT("MaterialTexture"));
		}

		uint32 ClearValues[4] = { 0, 0, 0, 0 };
		RHICmdList.ClearTinyUAV(VoxelResources.DensityTexture->GetRenderTargetItem().UAV, ClearValues);
		if (bVoxelizeMaterial)
		{
			RHICmdList.ClearTinyUAV(VoxelResources.TangentXTexture->GetRenderTargetItem().UAV, ClearValues);
			RHICmdList.ClearTinyUAV(VoxelResources.TangentYTexture->GetRenderTargetItem().UAV, ClearValues);
			RHICmdList.ClearTinyUAV(VoxelResources.TangentZTexture->GetRenderTargetItem().UAV, ClearValues);
			RHICmdList.ClearTinyUAV(VoxelResources.MaterialTexture->GetRenderTargetItem().UAV, ClearValues);
		}
	}

	FRHIRenderPassInfo RPInfo(DummyTexture->GetRenderTargetItem().TargetableTexture, ERenderTargetActions::Load_Store);
	RPInfo.UAVIndex = 0;
	RPInfo.NumUAVs = bVoxelizeMaterial ? 5 : 1;
	RPInfo.UAVs[0] = VoxelResources.DensityTexture->GetRenderTargetItem().UAV;
	if (bVoxelizeMaterial)
	{
		RPInfo.UAVs[1] = VoxelResources.TangentXTexture->GetRenderTargetItem().UAV;
		RPInfo.UAVs[2] = VoxelResources.TangentYTexture->GetRenderTargetItem().UAV;
		RPInfo.UAVs[3] = VoxelResources.TangentZTexture->GetRenderTargetItem().UAV;
		RPInfo.UAVs[4] = VoxelResources.MaterialTexture->GetRenderTargetItem().UAV;
	}

	const FIntRect ViewportRect(0, 0, ResolutionDim, ResolutionDim);

	FHairStrandsLightDesc LightDesc;
	LightDesc.bIsOrtho = true;
	LightDesc.LightDirection = FVector(1, 0, 0);
	LightDesc.LightPosition = FVector(0, 0, 0);
	{
		const FSphere SphereBound = Bounds.GetSphere();
		const float SphereRadius = SphereBound.W * GHairVoxelizationAABBScale;

		FReversedZOrthoMatrix OrthoMatrix(SphereRadius, SphereRadius, 1.f / (2 * SphereRadius), 0);
		FLookAtMatrix LookAt(SphereBound.Center - LightDesc.LightDirection * SphereRadius, SphereBound.Center, FVector(0, 0, 1));
		LightDesc.WorldToLightClipTransform = LookAt * OrthoMatrix;
		LightDesc.MinStrandRadiusAtDepth1.Primary = GStrandHairVoxelizationRasterizationScale * SphereRadius / FMath::Min(Resolution.X, Resolution.Y);
		LightDesc.MinStrandRadiusAtDepth1.Velocity = LightDesc.MinStrandRadiusAtDepth1.Primary;

		VoxelResources.WorldToClip = LightDesc.WorldToLightClipTransform;
		VoxelResources.MinAABB = Bounds.GetSphere().Center - SphereRadius;
		VoxelResources.MaxAABB = Bounds.GetSphere().Center + SphereRadius;
	}

	RHICmdList.BeginRenderPass(RPInfo, TEXT("HairVoxelization"));
	RasterHairStrands(
		RHICmdList,
		Scene,
		ViewInfo,
		PrimitiveSceneInfo,
		LightDesc,
		bVoxelizeMaterial ? EHairStrandsRasterPassType::VoxelizationMaterial : EHairStrandsRasterPassType::Voxelization,
		nullptr,
		ViewportRect,
		VoxelResources.MinAABB,
		VoxelResources.MaxAABB);
	RHICmdList.EndRenderPass();

	if (GHairVoxelInjectOpaqueDepthEnable)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		AddVoxelInjectOpaquePass(GraphBuilder, *ViewInfo, Cluster);
		GraphBuilder.Execute();
	}

	{
		FRDGBuilder GraphBuilder(RHICmdList);
		AddVoxelGenerateMipPass(GraphBuilder, *ViewInfo, Cluster);
		GraphBuilder.Execute();
	}
}

void VoxelizeHairStrands(
	FRHICommandListImmediate& RHICmdList, 
	const FScene* Scene, 
	const TArray<FViewInfo>& Views,
	FHairStrandsClusterViews& DeepShadowClusterViews)
{
	if (!IsHairStrandsVoxelizationEnable())
		return;

	FHairStrandsClusterViews PrimitivesClusterViews;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{			
		if (ViewIndex >= DeepShadowClusterViews.Views.Num())
			continue;

		const FViewInfo& View = Views[ViewIndex];
		FHairStrandsClusterDatas& ClusterDatas = DeepShadowClusterViews.Views[ViewIndex];
		for (FHairStrandsClusterData& Cluster : ClusterDatas.Datas)
		{
			RenderVoxelization(
				RHICmdList,
				Scene,
				&View,
				Cluster);
		}
	}
}
