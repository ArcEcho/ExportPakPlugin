// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class SBox;
class UExportPakSettings;

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

private:

	/** Inline content area for different tool modes */
	TSharedPtr<SBox> InlineContentHolder;

	/** Settings view ui element ptr */
	TSharedPtr<IDetailsView> SettingsView;

	UExportPakSettings* ExportPakSettings;

};