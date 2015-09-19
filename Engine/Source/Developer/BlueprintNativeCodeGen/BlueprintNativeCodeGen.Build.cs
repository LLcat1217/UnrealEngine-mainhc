// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BlueprintNativeCodeGen : ModuleRules
{
    public BlueprintNativeCodeGen(TargetInfo Target)
	{
        PrivateIncludePaths.Add("BlueprintNativeCodeGen/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "DesktopPlatform",
                "UnrealEd",
                "InputCore",
                "SlateCore",
                "Slate",
                "EditorStyle",
                "KismetCompiler",
                "Json",
                "JsonUtilities",
                "BlueprintCompilerCppBackend",
                "GameProjectGeneration"
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
			}
		);
	}
}
