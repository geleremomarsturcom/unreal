// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "FractureToolBitmap.h"

#include "FractureEditorStyle.h"

#define LOCTEXT_NAMESPACE "FractureBitmap"

UFractureToolBitmap::UFractureToolBitmap(const FObjectInitializer& ObjInit) 
	: Super(ObjInit) 
{
}

FText UFractureToolBitmap::GetDisplayText() const
{ 
	return FText(NSLOCTEXT("Fracture", "FractureToolBitmap", "Bitmap")); 
}

FText UFractureToolBitmap::GetTooltipText() const 
{ 
	return FText(NSLOCTEXT("Fracture", "FractureToolBitmapTooltip", "Bitmap Fracture")); 
}

FSlateIcon UFractureToolBitmap::GetToolIcon() const 
{
	return FSlateIcon("FractureEditorStyle", "FractureEditor.Texture");
}

void UFractureToolBitmap::RegisterUICommand( FFractureEditorCommands* BindingContext ) 
{
	/*
	UI_COMMAND_EXT( BindingContext, UICommandInfo, "Texture", "Texture", "Texture Voronoi Fracture", EUserInterfaceActionType::ToggleButton, FInputChord() );
	BindingContext->Texture = UICommandInfo;
	*/
}

TArray<UObject*> UFractureToolBitmap::GetSettingsObjects() const 
{ 
	TArray<UObject*> Settings; 
	Settings.Add(GetMutableDefault<UFractureBitmapSettings>());
	return Settings;
}

#undef LOCTEXT_NAMESPACE
