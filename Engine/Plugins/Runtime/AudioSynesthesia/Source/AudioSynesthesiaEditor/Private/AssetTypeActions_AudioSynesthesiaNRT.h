// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class UAudioSynesthesiaNRT;

/** FAssetTypeActions_AudioSynesthesiaNRT */
class FAssetTypeActions_AudioSynesthesiaNRT : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_AudioSynesthesiaNRT(UAudioSynesthesiaNRT* InSynesthesia);

	//~ Begin FAssetTypeActions_Base
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	//~ End FAssetTypeActions_Base

private:
	UAudioSynesthesiaNRT* Synesthesia;
};
