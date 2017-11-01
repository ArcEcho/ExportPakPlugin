// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ExportPak.h"
#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"


class IDetailsView;
class SBox;
class UExportPakSettings;
struct FDependenciesInfo; 
class FAssetRegistryModule;


//////////////////////////////////////////////////////////////////////////
// SExportPak

class SExportPak : public SCompoundWidget
{

public:
	SLATE_BEGIN_ARGS(SExportPak)
	{}
	SLATE_END_ARGS()

public:
	/**
	* Construct the widget
	*
	* @param	InArgs			A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs);

private:
	FReply OnExportPakButtonClicked();

	void CreateTargetAssetListView();

	void GetAssetDependecies(TMap<FString, FDependenciesInfo>& DependenciesInfos);

	void GeneratePakFiles(const TMap<FString, FDependenciesInfo> &DependenciesInfos);

	void SavePakDescriptionFile(const FString& TargetPackage, const FDependenciesInfo& DependecyInfo);

	void GatherDependenciesInfoRecursively(FAssetRegistryModule &AssetRegistryModule, const FString &TargetLongPackageName,
		TArray<FString> &DependenciesInGameContentDir, TArray<FString> &OtherDependencies);

	/** This will save the dependencies information to the OutputPath/AssetDependencies.json */
	void SaveDependenciesInfo(const TMap<FString, FDependenciesInfo> &DependenciesInfos);

	bool CanExportPakExecuted() const;

private:

	/** Inline content area for different tool modes */
	TSharedPtr<SBox> InlineContentHolder;

	/** Settings view ui element ptr */
	TSharedPtr<IDetailsView> SettingsView;

	UExportPakSettings* ExportPakSettings;

};