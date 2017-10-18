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
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "AssetRegistryModule.h"
#include "ARFilter.h"
#include "ModuleManager.h"
#include "ISettingsModule.h"
#include "PlatformFile.h"
#include "PlatformFilemanager.h"
#include "FileHelper.h"
#include "json.h"
#include "Misc/SecureHash.h"
#include "FileManager.h"
#include "PackageName.h"

struct FDependenciesInfo
{
	TArray<FString> DependenciesInGameContentDir;
	TArray<FString> OtherDependencies;
	FString AssetClassString;
};


#define LOCTEXT_NAMESPACE "ExportPak"

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
		.IsEnabled_Lambda([this]() -> bool { return ExportPakSettings != nullptr && ExportPakSettings->PackagesToExport.Num() != 0; })
		]
		]
		]
		];

	ExportPakSettings = UExportPakSettings::Get();
	SettingsView->SetObject(ExportPakSettings);
}

FString HashStringWithSHA1(const FString &InString)
{
	FSHAHash StringHash;
	FSHA1::HashBuffer(TCHAR_TO_ANSI(*InString), InString.Len(), StringHash.Hash);

	return StringHash.ToString();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHashStringWithSHA1Test, "ExportPak.HashStringWithSHA1", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHashStringWithSHA1Test::RunTest(const FString& Parameters)
{
	FString S = "/Game/MyProject/Maps/MyTestMap";
	FString HashString = HashStringWithSHA1(S);

	UE_LOG(LogExportPak, Log, TEXT("%s -> %s"), *S, *HashString);

	return HashString.ToLower() == "eb397865ca4d6f2d48fb44a45424cff0fe60541e";
}

FReply SExportPak::OnExportPakButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("TTTTT"));

	TMap<FString, FDependenciesInfo> DependenciesInfos;
	GetAssetDependecies(DependenciesInfos);

	// Write Results
	SaveDependenciesInfo(DependenciesInfos);

	//CookContent();

	GeneratePakFiles(DependenciesInfos);

	// Build the hash from the path name + the contents of the bulk data.

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

void SExportPak::GetAssetDependecies(TMap<FString, FDependenciesInfo>& DependenciesInfos)
{
	for (auto &PackageFilePath : ExportPakSettings->PackagesToExport)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		FStringAssetReference AssetRef = PackageFilePath.FilePath;
		FString TargetLongPackageName = AssetRef.GetLongPackageName();

		if (FPackageName::DoesPackageExist(TargetLongPackageName))
		{
			auto &DependenciesInfoEntry = DependenciesInfos.Add(TargetLongPackageName);

			// Try get asset type.
			{
				TArray<FAssetData> AssetDataList;
				bool  bResult = AssetRegistryModule.Get().GetAssetsByPackageName(FName(*TargetLongPackageName), AssetDataList);
				if (!bResult || AssetDataList.Num() == 0)
				{
					UE_LOG(LogExportPak, Error, TEXT("Failed to get AssetData of  %s, please check."), *TargetLongPackageName);
					return;
				}

				if (AssetDataList.Num() > 1)
				{
					UE_LOG(LogExportPak, Error, TEXT("Got multiple AssetData of  %s, please check."), *TargetLongPackageName);
				}

				DependenciesInfoEntry.AssetClassString = AssetDataList[0].AssetClass.ToString();
			}

			GatherDependenciesInfoRecursively(AssetRegistryModule, TargetLongPackageName, DependenciesInfoEntry.DependenciesInGameContentDir, DependenciesInfoEntry.OtherDependencies);
		}

		TArray<FName>  ACs;
		AssetRegistryModule.Get().GetAncestorClassNames(*TargetLongPackageName, ACs);
	}
}

void SExportPak::CookContent()
{
	//// @hack GDC: this should be moved somewhere else and be less hacky
	//ITargetPlatform* RunningTargetPlatform = GetTargetPlatformManager()->GetRunningTargetPlatform();

	//if (RunningTargetPlatform != nullptr)
	//{
	//	const FName CookedPlatformName = *(RunningTargetPlatform->PlatformName() + TEXT("NoEditor"));
	//	

	//	const PlatformInfo::FPlatformInfo* const PlatformInfo = PlatformInfo::FindPlatformInfo(CookedPlatformName);
	//	check(PlatformInfo);

	//	if (FInstalledPlatformInfo::Get().IsPlatformMissingRequiredFile(PlatformInfo->BinaryFolderName))
	//	{
	//		if (!FInstalledPlatformInfo::OpenInstallerOptions())
	//		{
	//			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("MissingPlatformFilesCook", "Missing required files to cook for this platform."));
	//		}
	//		return;
	//	}

	//	FString OptionalParams;

	//	if (!FModuleManager::LoadModuleChecked<IProjectTargetPlatformEditorModule>("ProjectTargetPlatformEditor").ShowUnsupportedTargetWarning(PlatformInfo->VanillaPlatformName))
	//	{
	//		return;
	//	}

	//	if (PlatformInfo->SDKStatus == PlatformInfo::EPlatformSDKStatus::NotInstalled)
	//	{
	//		IMainFrameModule& MainFrameModule = FModuleManager::GetModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	//		MainFrameModule.BroadcastMainFrameSDKNotInstalled(PlatformInfo->TargetPlatformName.ToString(), PlatformInfo->SDKTutorial);
	//		return;
	//	}

	//	// Append any extra UAT flags specified for this platform flavor
	//	if (!PlatformInfo->UATCommandLine.IsEmpty())
	//	{
	//		OptionalParams += TEXT(" ");
	//		OptionalParams += PlatformInfo->UATCommandLine;
	//	}
	//	else
	//	{
	//		OptionalParams += TEXT(" -targetplatform=");
	//		OptionalParams += *PlatformInfo->TargetPlatformName.ToString();
	//	}

	//	OptionalParams += GetCookingOptionalParams();

	//	if (FApp::IsRunningDebug())
	//	{
	//		OptionalParams += TEXT(" -UseDebugParamForEditorExe");
	//	}

	//	FString ProjectPath = FPaths::IsProjectFilePathSet() ? FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()) : FPaths::RootDir() / FApp::GetGameName() / FApp::GetGameName() + TEXT(".uproject");
	//	FString CommandLine = FString::Printf(TEXT("BuildCookRun %s%s -nop4 -project=\"%s\" -cook -skipstage -ue4exe=%s %s"),
	//		GetUATCompilationFlags(),
	//		FApp::IsEngineInstalled() ? TEXT(" -installed") : TEXT(""),
	//		*ProjectPath,
	//		*FUnrealEdMisc::Get().GetExecutableForCommandlets(),
	//		*OptionalParams
	//	);

	//	IUATHelperModule::Get().CreateUatTask(CommandLine, PlatformInfo->DisplayName, LOCTEXT("CookingContentTaskName", "Cooking content"), LOCTEXT("CookingTaskName", "Cooking"), FEditorStyle::GetBrush(TEXT("MainFrame.CookContent")));

	//}	
}

/**
* Gets compilation flags for UAT for this system.
*/
const TCHAR* GetUATCompilationFlags()
{
	// We never want to compile editor targets when invoking UAT in this context.
	// If we are installed or don't have a compiler, we must assume we have a precompiled UAT.
	return (FApp::GetEngineIsPromotedBuild() || FApp::IsEngineInstalled() || FPlatformMisc::IsDebuggerPresent())
		? TEXT("-nocompile -nocompileeditor")
		: TEXT("-nocompileeditor");
}

class FCookedAssetFileVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	FCookedAssetFileVisitor(const FString& InAssetFilename)
		:
		AssetFilename(InAssetFilename)
	{
	}

	virtual bool Visit(const TCHAR *FilenameOrDirectory, bool bIsDirectory) override
	{
		if (bIsDirectory)
		{
			return true;
		}

		FString StandardizedFilename(FilenameOrDirectory);
		FPaths::MakeStandardFilename(StandardizedFilename);
		if (FPaths::GetBaseFilename(StandardizedFilename) == AssetFilename)
		{
			Files.Add(StandardizedFilename);
		}

		return true;
	}

	TArray<FString> Files;

private:
	FString AssetFilename;
};

void GeneratePakFiles_Internal(const TArray<FString>& PackagesToHandle, const FString& MainPackage)
{
	FString HashedMainPackageName = HashStringWithSHA1(MainPackage);
	FString PakOutputDirectory = FPaths::Combine(FPaths::GameSavedDir(), TEXT("ExportPak/Paks"), HashedMainPackageName);
	for (auto& PackageNameInGameDir : PackagesToHandle)
	{

		UE_LOG(LogExportPak, Log, TEXT("    %s"), *PackageNameInGameDir);

		// Standardize package name. May this is not necessary.
		FString TargetLongPackageName;
		{
			FString FailedReason;
			bool bConvertionResult = FPackageName::TryConvertFilenameToLongPackageName(
				PackageNameInGameDir,
				TargetLongPackageName,
				&FailedReason
			);

			if (!bConvertionResult)
			{
				UE_LOG(LogExportPak, Log, TEXT("        TryConvertFilenameToLongPackageName Failed: %s!"), *FailedReason);
				return;
			}
			else
			{
				UE_LOG(LogExportPak, Log, TEXT("        %s"), *TargetLongPackageName);
			}
		}

		FString TargetAssetFilepath;
		{
			bool bConvertionResult = FPackageName::TryConvertLongPackageNameToFilename(
				TargetLongPackageName,
				TargetAssetFilepath,
				".uasset"
			);
			if (!bConvertionResult)
			{
				UE_LOG(LogExportPak, Log, TEXT("        TryConvertLongPackageNameToFilename Failed!"));
				return;
			}
			else
			{
				UE_LOG(LogExportPak, Log, TEXT("            %s"), *TargetAssetFilepath);
			}
		}

		FString Filename = FPaths::GetBaseFilename(TargetAssetFilepath);
		FString ProjectName = FPaths::GetBaseFilename(FPaths::GetProjectFilePath());
		FString ItermediateDirectory = FPaths::GetPath(TargetAssetFilepath).Replace(*FPaths::GameDir(), TEXT(""), ESearchCase::CaseSensitive);

		// TODO ArcEcho: Now it is hard-coded to WindowsNoEditor
		FString TargetCookedAssetDirectory = FPaths::Combine(FPaths::GameSavedDir(), TEXT("Cooked/WindowsNoEditor"), ProjectName, ItermediateDirectory);

		FCookedAssetFileVisitor CookedAssetFileVisitor(Filename);
		FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*TargetCookedAssetDirectory, CookedAssetFileVisitor);

		FString ResponseFileContent = "";
		for (auto &f : CookedAssetFileVisitor.Files)
		{
			FString Ext = FPaths::GetExtension(f);
			FString RelativePathForResponseFile = FPaths::Combine(TEXT("../../.."), ProjectName, ItermediateDirectory, Filename) + FString(".") + Ext;
		
			ResponseFileContent += FString::Printf(TEXT("\"%s\" \"%s\"\n"), *f, *RelativePathForResponseFile);
		}

		FString ResponseFilepath = FPaths::Combine(FPaths::GameSavedDir(), TEXT("ExportPak/Temp"), HashedMainPackageName + "_Paklist.txt");
		FFileHelper::SaveStringToFile(ResponseFileContent, *ResponseFilepath);

		FString LogFilepath = FPaths::Combine(FPaths::GameSavedDir(), TEXT("ExportPak/Temp"), HashedMainPackageName + ".log");

		FString HashedPackageName = HashStringWithSHA1(PackageNameInGameDir);
		FString OutputPakFilepath = FPaths::Combine(PakOutputDirectory, HashedPackageName + TEXT(".pak"));

		
		FString UnrealPakExeFilepath = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/Win64/UnrealPak.exe"));
		FPaths::MakeStandardFilename(UnrealPakExeFilepath);
		UnrealPakExeFilepath = FPaths::ConvertRelativePathToFull(UnrealPakExeFilepath);


		UE_LOG(LogExportPak, Log, TEXT("$$$$$ %s %s %s %s"), *PackageNameInGameDir, *OutputPakFilepath, *FPaths::EngineDir(), *UnrealPakExeFilepath);

		FString CommandLine = FString::Printf(
			TEXT("%s -create=%s -encryptionini -platform=Windows -installed -UTF8Output -multiprocess -patchpaddingalign=2048 -abslog=%s"),
			*OutputPakFilepath,
			*ResponseFilepath,
			*LogFilepath
		);

		FPlatformProcess::CreateProc(*UnrealPakExeFilepath, *CommandLine, true, true, true, nullptr, -1, nullptr, nullptr);
	}
}

void SExportPak::GeneratePakFiles(const TMap<FString, FDependenciesInfo> &DependenciesInfos)
{
	for (auto &DependencyInfo : DependenciesInfos)
	{
		UE_LOG(LogExportPak, Log, TEXT("%s"), *DependencyInfo.Key);
		FString HashedLongPackageName = HashStringWithSHA1(DependencyInfo.Key);

		TArray<FString> PackagesToHandle;
		PackagesToHandle.Add(DependencyInfo.Key);

		GeneratePakFiles_Internal(PackagesToHandle, DependencyInfo.Key);
	}
}

void SExportPak::GatherDependenciesInfoRecursively(FAssetRegistryModule &AssetRegistryModule, const FString &TargetLongPackageName, TArray<FString> &DependenciesInGameContentDir, TArray<FString> &OtherDependencies)
{
	TArray<FName> Dependencies;
	bool bGetDependenciesSuccess = AssetRegistryModule.Get().GetDependencies(FName(*TargetLongPackageName), Dependencies, EAssetRegistryDependencyType::Packages);
	if (bGetDependenciesSuccess)
	{
		for (auto &d : Dependencies)
		{
			// Pick out packages in game content dir.
			FString LongDependentPackageName = d.ToString();
			if (LongDependentPackageName.StartsWith("/Game"))
			{
				// Try find firstly to avoid duplicated entry.
				if (DependenciesInGameContentDir.Find(LongDependentPackageName) == INDEX_NONE)
				{
					DependenciesInGameContentDir.Add(LongDependentPackageName);
					GatherDependenciesInfoRecursively(AssetRegistryModule, LongDependentPackageName, DependenciesInGameContentDir, OtherDependencies);
				}
			}
			else
			{
				if (OtherDependencies.Find(LongDependentPackageName) == INDEX_NONE)
				{
					OtherDependencies.Add(LongDependentPackageName);
					GatherDependenciesInfoRecursively(AssetRegistryModule, LongDependentPackageName, DependenciesInGameContentDir, OtherDependencies);
				}
			}
		}
	}
}

void SExportPak::SaveDependenciesInfo(const TMap<FString, FDependenciesInfo> &DependenciesInfos)
{
	TSharedPtr<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject);
	for (auto &DependenciesInfoEntry : DependenciesInfos)
	{
		TSharedPtr<FJsonObject> EntryJsonObject = MakeShareable(new FJsonObject);

		// Write current AssetClass.
		EntryJsonObject->SetStringField("AssetClass", DependenciesInfoEntry.Value.AssetClassString);

		// Write dependencies in game content dir.
		{
			TArray< TSharedPtr<FJsonValue> > DependenciesEntry;
			for (auto &d : DependenciesInfoEntry.Value.DependenciesInGameContentDir)
			{
				DependenciesEntry.Add(MakeShareable(new FJsonValueString(d)));
			}
			EntryJsonObject->SetArrayField("DependenciesInGameContentDir", DependenciesEntry);
		}

		// Write dependencies not in game content dir.
		{
			TArray< TSharedPtr<FJsonValue> > DependenciesEntry;
			for (auto &d : DependenciesInfoEntry.Value.OtherDependencies)
			{
				DependenciesEntry.Add(MakeShareable(new FJsonValueString(d)));
			}
			EntryJsonObject->SetArrayField("OtherDependencies", DependenciesEntry);
		}

		RootJsonObject->SetObjectField(DependenciesInfoEntry.Key, EntryJsonObject);
	}

	FString OutputString;
	auto JsonWirter = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWirter);

	FString ResultFileFilename = FPaths::Combine(FPaths::GameSavedDir(), TEXT("ExportPak"), TEXT("/AssetDependencies.json"));
	ResultFileFilename = FPaths::ConvertRelativePathToFull(ResultFileFilename);

	// Attention to FFileHelper::EEncodingOptions::ForceUTF8 here. 
	// In some case, UE4 will save as UTF16 according to the content.
	bool bSaveSuccess = FFileHelper::SaveStringToFile(OutputString, *ResultFileFilename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (bSaveSuccess)
	{
		// UE4 API to show an editor notification.
		auto Message = LOCTEXT("ExportPakSuccessNotification", "Succeed to export asset dependecies.");
		FNotificationInfo Info(Message);
		Info.bFireAndForget = true;
		Info.ExpireDuration = 5.0f;
		Info.bUseSuccessFailIcons = false;
		Info.bUseLargeFont = false;

		const FString HyperLinkText = ResultFileFilename;
		Info.Hyperlink = FSimpleDelegate::CreateStatic([](FString SourceFilePath)
		{
			FPlatformProcess::ExploreFolder(*SourceFilePath);
		}, HyperLinkText);
		Info.HyperlinkText = FText::FromString(HyperLinkText);

		FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);

		UE_LOG(LogExportPak, Log, TEXT("%s. At %s"), *Message.ToString(), *ResultFileFilename);
	}
	else
	{
		UE_LOG(LogExportPak, Error, TEXT("Failed to export %s"), *ResultFileFilename);
	}
}

#undef LOCTEXT_NAMESPACE