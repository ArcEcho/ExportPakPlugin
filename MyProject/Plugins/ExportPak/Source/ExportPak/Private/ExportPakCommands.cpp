// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ExportPakCommands.h"

#define LOCTEXT_NAMESPACE "FExportPakModule"

void FExportPakCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "ExportPak", "Bring up ExportPak window", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
