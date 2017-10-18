// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ExportPak.h"
#include "ExportPakStyle.h"
#include "ExportPakCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SExportPak.h"

DEFINE_LOG_CATEGORY(LogExportPak);

static const FName ExportPakTabName("ExportPak");

#define LOCTEXT_NAMESPACE "FExportPakModule"

void FExportPakModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FExportPakStyle::Initialize();
	FExportPakStyle::ReloadTextures();

	FExportPakCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FExportPakCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FExportPakModule::PluginButtonClicked),
		FCanExecuteAction());
		
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	
	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FExportPakModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
	
	//{
	//	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	//	ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FExportPakModule::AddToolbarExtension));
	//	
	//	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	//}
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ExportPakTabName, FOnSpawnTab::CreateRaw(this, &FExportPakModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FExportPakTabTitle", "ExportPak"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FExportPakModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FExportPakStyle::Shutdown();

	FExportPakCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ExportPakTabName);
}

TSharedRef<SDockTab> FExportPakModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// Put your tab content here!
			SNew(SExportPak)
		];
}

void FExportPakModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->InvokeTab(ExportPakTabName);
}

void FExportPakModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FExportPakCommands::Get().OpenPluginWindow);
}

void FExportPakModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FExportPakCommands::Get().OpenPluginWindow);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FExportPakModule, ExportPak)