// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentSourceInterfaces.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"

class EDITORINTERACTIVETOOLSFRAMEWORK_API FStaticMeshComponentTargetFactory : public FComponentTargetFactory
{
public:
	bool CanBuild( UActorComponent* Candidate ) override;
	TUniquePtr<FPrimitiveComponentTarget> Build( UPrimitiveComponent* PrimitiveComponent ) override;
};

class EDITORINTERACTIVETOOLSFRAMEWORK_API FStaticMeshComponentTarget : public FPrimitiveComponentTarget
{
public:
	FStaticMeshComponentTarget( UPrimitiveComponent* Component )
		: FPrimitiveComponentTarget( Cast<UStaticMeshComponent>(Component) ){}
	FMeshDescription* GetMesh() override;
	void CommitMesh( const FCommitter& ) override;
	static const int LODIndex{0};
};
