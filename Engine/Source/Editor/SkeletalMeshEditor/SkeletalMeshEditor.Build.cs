// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SkeletalMeshEditor : ModuleRules
{
	public SkeletalMeshEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Persona",
            }
        );

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
                "UnrealEd",
                "SkeletonEditor",
                "Kismet",
                "KismetWidgets",
                "ActorPickerMode",
                "SceneDepthPickerMode",
                "MainFrame",
                "DesktopPlatform",
                "PropertyEditor",
                "RHI",
                "ClothingSystemRuntimeNv",
                "ClothingSystemEditorInterface",
				"ClothingSystemRuntimeInterface",
				"SkeletalMeshUtilitiesCommon",
            }
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "PropertyEditor",
            }
        );

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
            }
		);
	}
}
