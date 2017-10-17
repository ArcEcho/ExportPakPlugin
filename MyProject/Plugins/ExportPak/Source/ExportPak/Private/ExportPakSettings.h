// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ExportPak.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "ExportPakSettings.generated.h"

/** Singleton wrapper to allow for using the setting structure in SSettingsView */
UCLASS(config = Game)
class UExportPakSettings : public UObject
{
	GENERATED_BODY()
public:
	UExportPakSettings()
		:
		bUseBatchMode (false)
	{
	}

	static UExportPakSettings* Get()
	{
		static bool bInitialized = false;
		// This is a singleton, use default object
		UExportPakSettings* DefaultSettings = GetMutableDefault<UExportPakSettings>();

		if (!bInitialized)
		{
			bInitialized = true;
		}

		return DefaultSettings;
	}

public:
	/** If true, package all assets into a single pak file.*/
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = Default)
	bool  bUseBatchMode;

	/** You can use copied asset string reference here, e.g. World'/Game/NewMap.NewMap'*/
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Target Asset List")
	TArray<FFilePath> PackagesToExport;
};