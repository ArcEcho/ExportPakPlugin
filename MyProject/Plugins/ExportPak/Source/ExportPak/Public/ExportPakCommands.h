// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ExportPakStyle.h"

class FExportPakCommands : public TCommands<FExportPakCommands>
{
public:

	FExportPakCommands()
		: TCommands<FExportPakCommands>(TEXT("ExportPak"), NSLOCTEXT("Contexts", "ExportPak", "ExportPak Plugin"), NAME_None, FExportPakStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};