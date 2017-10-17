#include "SExportPak.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "IDetailsView.h"
#include "ExportPakSettings.h"

#define LOCTEXT_NAMESPACE "SMergeActorsToolbar"
void SExportPak::Construct(const FArguments& InArgs)
{
	CreateTargetAssetListView();

	ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2, 0, 0, 0)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(0.f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(4, 4, 4, 4)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.Padding(0, 10, 0, 0)
							[
								SNew(SBorder)
								.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
								[
									SNew(SVerticalBox)
									// Static mesh component selection
									+ SVerticalBox::Slot()
									.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.VAlign(VAlign_Center)
										[
											SettingsView->AsShared()
										]
									]
								]
							]
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Right)
					.Padding(4, 4, 10, 4)
					[
						SNew(SButton)
						.Text(LOCTEXT("ExportPak", "Export Pak file(s)"))
						.OnClicked(this, &SExportPak::OnExportPakButtonClicked)
						//.IsEnabled_Lambda([this]() -> bool { return this->RegisteredTools[CurrentlySelectedTool]->CanMerge(); })
					]
				]
			]
		];

	ExportPakSettings = UExportPakSettings::Get();
	SettingsView->SetObject(ExportPakSettings);
}

FReply SExportPak::OnExportPakButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("TTTTT"));

	return FReply::Handled();
}

void SExportPak::CreateTargetAssetListView()
{
// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = true;
	DetailsViewArgs.bLockable = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ComponentsAndActorsUseNameArea;
	DetailsViewArgs.bCustomNameAreaLocation = false;
	DetailsViewArgs.bCustomFilterAreaLocation = true;
	DetailsViewArgs.DefaultsOnlyVisibility = FDetailsViewArgs::EEditDefaultsOnlyNodeVisibility::Hide;
	
		
	//// Tiny hack to hide this setting, since we have no way / value to go off to 
	//struct Local
	//{
	//	/** Delegate to show all properties */
	//	static bool IsPropertyVisible(const FPropertyAndParent& PropertyAndParent, bool bInShouldShowNonEditable)
	//	{
	//		return (PropertyAndParent.Property.GetFName() != GET_MEMBER_NAME_CHECKED(FExportPakProxySettings, GutterSpace));
	//	}
	//};

	SettingsView = EditModule.CreateDetailView(DetailsViewArgs);
	//SettingsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateStatic(&Local::IsPropertyVisible, true));
}

#undef LOCTEXT_NAMESPACE