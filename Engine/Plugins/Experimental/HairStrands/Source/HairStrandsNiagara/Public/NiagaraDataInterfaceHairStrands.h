// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceRW.h"
#include "NiagaraCommon.h"
#include "VectorVM.h"
#include "GroomAsset.h"
#include "GroomActor.h"
#include "NiagaraDataInterfaceHairStrands.generated.h"

/** Size of each strands*/
UENUM(BlueprintType)
enum class EHairStrandsSize : uint8
{
	None = 0 UMETA(Hidden),
	Size2 = 0x02 UMETA(DisplatName = "2"),
	Size4 = 0x04 UMETA(DisplatName = "4"),
	Size8 = 0x08 UMETA(DisplatName = "8"),
	Size16 = 0x10 UMETA(DisplatName = "16"),
	Size32 = 0x20 UMETA(DisplatName = "32")
};

/** Structure storing the grid size*/
struct FGridSize
{
	FGridSize(const uint32 InGridSizeX = 0, const uint32 InGridSizeY = 0, 
		      const uint32 InGridSizeZ = 0, const uint32 InGridSizeW = 0) : GridSizeX(InGridSizeX), 
		GridSizeY(InGridSizeY), GridSizeZ(InGridSizeZ), GridSizeW(InGridSizeW) {}

	uint32 GridSizeX;
	uint32 GridSizeY;
	uint32 GridSizeZ;
	uint32 GridSizeW;
};


/** Render buffers that will be used in hlsl functions */
struct FNDICollisionGridBuffer : public FRenderResource
{
	/** Set the grid size */
	void SetGridSize(const FGridSize GridSize);

	/** Init the buffer */
	virtual void InitRHI() override;

	/** Release the buffer */
	virtual void ReleaseRHI() override;

	/** Clear all UAV*/
	void ClearBuffers(FRHICommandList& RHICmdList);

	/** Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FNDICollisionGridBuffer"); }

	/** Grid data texture */
	FTextureRWBuffer3D GridDataBuffer;

	/** Grid size that will be used for the collision*/
	FGridSize GridSize;
};

/** Render buffers that will be used in hlsl functions */
struct FNDIHairStrandsBuffer : public FRenderResource
{
	/** Set the asset that will be used to affect the buffer */
	void SetHairAsset(const FHairStrandsDatas*  HairStrandsDatas, const FHairStrandsResource*  HairStrandsResource );

	/** Init the buffer */
	virtual void InitRHI() override;

	/** Release the buffer */
	virtual void ReleaseRHI() override;

	/** Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FNDIHairStrandsBuffer"); }

	/** Strand curves point offset buffer */
	FRWBuffer CurvesOffsetsBuffer;

	/** The strand asset datas from which to sample */
	const FHairStrandsDatas* SourceDatas;

	/** The strand asset resource from which to sample */
	const FHairStrandsResource* SourceResources;
};

/** Data stored per strand base instance*/
struct FNDIHairStrandsData
{
	/** Swap the current and the destination data */
	void SwapBuffers();

	/** Init the current and the destination data */
	void InitBuffers();

	/** Cached World transform. */
	FMatrix WorldTransform;

	/** Number of strands*/
	int32 NumStrands;

	/** Strand size */
	int32 StrandSize;

	/** Strand Density */
	float StrandDensity;

	/** Root Thickness */
	float RootThickness;

	/** Tip Thickness */
	float TipThickness;

	/** Grid Origin */
	FVector4 GridOrigin;

	/** Grid Size */
	FGridSize GridSize;

	/** Strands Gpu buffer */
	FNDIHairStrandsBuffer* HairStrandsBuffer;

	/** Collision Grid Buffer A */
	FNDICollisionGridBuffer* CollisionGridBufferA;

	/** Collision Grid Buffer B */
	FNDICollisionGridBuffer* CollisionGridBufferB;

	/** Pointer to the current buffer */
	FNDICollisionGridBuffer* CurrentGridBuffer;

	/** Pointer to the destination buffer */
	FNDICollisionGridBuffer* DestinationGridBuffer;

	/** Sorted index particle buffers*/
	//FParticleSortBuffers SortedParticleBuffers;
};

/** Data Interface for the strand base */
UCLASS(EditInlineNew, Category = "Strands", meta = (DisplayName = "Hair Strands"))
class HAIRSTRANDSNIAGARA_API UNiagaraDataInterfaceHairStrands : public UNiagaraDataInterfaceRWBase
{
	GENERATED_UCLASS_BODY()

public:

	/** Size of each strand. */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	EHairStrandsSize StrandSize;

	/** Density of each strand. */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float StrandDensity;

	/** Strand Root thickness. */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float RootThickness;

	/** Strand Tip thickness. */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float TipThickness;

	/** Hair Strands Asset used to sample from when not overridden by a source actor from the scene. Also useful for previewing in the editor. */
	UPROPERTY(EditAnywhere, Category = "Source")
	UGroomAsset* DefaultSource;

	/** The source actor from which to sample */
	UPROPERTY(EditAnywhere, Category = "Source")
	AActor* SourceActor;

	/** Source transform to be applied to the asset. */
	UPROPERTY(EditAnywhere, Category = "Source")
	FMatrix SourceTransform;

	/** Grid size along the X axis. */
	UPROPERTY(EditAnywhere, Category = "Grid")
	uint32 GridSizeX;

	/** Grid size along the Y axis. */
	UPROPERTY(EditAnywhere, Category = "Grid")
	uint32 GridSizeY;

	/** Grid size along the Z axis. */
	UPROPERTY(EditAnywhere, Category = "Grid")
	uint32 GridSizeZ;

	/** The source component from which to sample */
	TWeakObjectPtr<class UGroomComponent> SourceComponent;

	/** The source asset from which to sample */
	TWeakObjectPtr<class UGroomAsset> GroomAsset;

	/** UObject Interface */
	virtual void PostInitProperties() override;

	/** UNiagaraDataInterface Interface */
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return Target == ENiagaraSimTarget::GPUComputeSim; }
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)override;
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	virtual int32 PerInstanceDataSize()const override { return sizeof(FNDIHairStrandsData); }
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	/** GPU simulation  functionality */
	virtual bool GetFunctionHLSL(const FName&  DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual FNiagaraDataInterfaceParametersCS* ConstructComputeParameters() const override;
	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance) override;
	virtual void GetCommonHLSL(FString& OutHLSL) override;

	/** Check if the component is Valid */
	bool IsComponentValid() const;

	/** Get the number of strands */
	void GetNumStrands(FVectorVMContext& Context);

	/** Get the strand size  */
	void GetStrandSize(FVectorVMContext& Context);

	/** Get the strand density */
	void GetStrandDensity(FVectorVMContext& Context);

	/** Get the strand thickness at the root */
	void GetRootThickness(FVectorVMContext& Context);

	/** Get the strand thickness at the tip */
	void GetTipThickness(FVectorVMContext& Context);

	/** Get the world transform */
	void GetWorldTransform(FVectorVMContext& Context);

	/** Get the world inverse */
	void GetWorldInverse(FVectorVMContext& Context);

	/** Get the strand vertex position in world space*/
	void GetPointPosition(FVectorVMContext& Context);

	/** Get the strand node position in world space*/
	void ComputeNodePosition(FVectorVMContext& Context);

	/** Get the strand node orientation in world space*/
	void ComputeNodeOrientation(FVectorVMContext& Context);

	/** Get the strand node mass */
	void ComputeNodeMass(FVectorVMContext& Context);

	/** Get the strand node inertia */
	void ComputeNodeInertia(FVectorVMContext& Context);

	/** Compute the edge length (diff between 2 nodes positions)*/
	void ComputeEdgeLength(FVectorVMContext& Context);

	/** Compute the edge orientation (diff between 2 nodes orientations) */
	void ComputeEdgeRotation(FVectorVMContext& Context);

	/** Compute the rest local position */
	void ComputeRestPosition(FVectorVMContext& Context);

	/** Compute the rest local orientation */
	void ComputeRestOrientation(FVectorVMContext& Context);

	/** Update the root node orientation based on the current transform */
	void AttachNodePosition(FVectorVMContext& Context);

	/** Update the root node position based on the current transform */
	void AttachNodeOrientation(FVectorVMContext& Context);

	/** Report the node displacement onto the points position*/
	void UpdatePointPosition(FVectorVMContext& Context);

	/** Reset the point position to be the rest one */
	void ResetPointPosition(FVectorVMContext& Context);

	/** Add external force to the linear velocity and advect node position */
	void AdvectNodePosition(FVectorVMContext& Context);

	/** Add external torque to the angular velocity and advect node orientation*/
	void AdvectNodeOrientation(FVectorVMContext& Context);

	/** Update the node linear velocity based on the node position difference */
	void UpdateLinearVelocity(FVectorVMContext& Context);

	/** Update the node angular velocity based on the node orientation difference */
	void UpdateAngularVelocity(FVectorVMContext& Context);

	/** Build the collision grid */
	void BuildCollisionGrid(FVectorVMContext& Context);

	/** Query the collision grid */
	void QueryCollisionGrid(FVectorVMContext& Context);

	/** Project the collision grid */
	void ProjectCollisionGrid(FVectorVMContext& Context);

	/** Name of the world transform */
	static const FString WorldTransformName;

	/** Name of the world transform */
	static const FString WorldInverseName;

	/** Name of the world rotation */
	static const FString WorldRotationName;

	/** Name of the number of strands */
	static const FString NumStrandsName;

	/** Name of the strand size */
	static const FString StrandSizeName;

	/** Name of the strand density */
	static const FString StrandDensityName;

	/** Name of the root thickness */
	static const FString RootThicknessName;

	/** Name of the tip thickness */
	static const FString TipThicknessName;

	/** Name of the points positions buffer */
	static const FString PointsPositionsBufferName;

	/** Name of the curves offsets buffer */
	static const FString CurvesOffsetsBufferName;

	/** Name of the nodes positions buffer */
	static const FString RestPositionsBufferName;

	/** Name of the grid current buffer */
	static const FString GridCurrentBufferName;

	/** Name of the grid X velocity buffer */
	static const FString GridDestinationBufferName;

	/** Name of the grid origin  */
	static const FString GridOriginName;

	/** Name of the grid size */
	static const FString GridSizeName;

protected:
	/** Copy one niagara DI to this */
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
};

/** Proxy to send data to gpu */
struct FNDIHairStrandsProxy : public FNiagaraDataInterfaceProxy
{
	/** Destroy internal data */
	virtual void DeferredDestroy() override;

	/** Get the size of the data that will be passed to render*/
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override { return sizeof(FNDIHairStrandsData); }

	/** Get the data that will be passed to render*/
	virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance) override;

	/** Initialize the Proxy data strands buffer */
	void InitializePerInstanceData(const FNiagaraSystemInstanceID& SystemInstance, FNDIHairStrandsBuffer* StrandsBuffer, FNDICollisionGridBuffer* CollisionGridBufferA, FNDICollisionGridBuffer* CollisionGridBufferB, const uint32 NumStrands, const uint8 StrandSize,
		const float StrandDensity, const float RootThickness, const float TipThickness, const FVector4& GridOrigin, const FGridSize& GridSize);

	/** Destroy the proxy data if necessary */
	void DestroyPerInstanceData(NiagaraEmitterInstanceBatcher* Batcher, const FNiagaraSystemInstanceID& SystemInstance);

	/** Launch all pre stage functions */
	virtual void PreStage(FRHICommandList& RHICmdList, const FNiagaraDataInterfaceSetArgs& Context) override;

	/** Launch all post stage functions */
	virtual void PostStage(FRHICommandList& RHICmdList, const FNiagaraDataInterfaceSetArgs& Context) override;

	/** Reset the buffers  */
	virtual void ResetData(FRHICommandList& RHICmdList, const FNiagaraDataInterfaceSetArgs& Context) override;

	/** List of proxy data for each system instances*/
	TMap<FNiagaraSystemInstanceID, FNDIHairStrandsData> SystemInstancesToProxyData;

	/** List of proxy data to destroy later */
	TSet<FNiagaraSystemInstanceID> DeferredDestroyList;
};

