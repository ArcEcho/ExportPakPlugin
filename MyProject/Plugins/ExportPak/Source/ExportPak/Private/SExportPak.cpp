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
						.IsEnabled(this, &SExportPak::CanExportPakExecuted)
					]
				]
			]
		];

	ExportPakSettings = UExportPakSettings::Get();
	SettingsView->SetObject(ExportPakSettings);
}

bool SExportPak::CanExportPakExecuted() const
{
	if (ExportPakSettings == nullptr)
	{
		return false;
	}
	
	if(	ExportPakSettings->PackagesToExport.Num() == 0)
	{
		return false;
	}

	bool bAllEmpty = true;
	for(auto PackageToExport : ExportPakSettings->PackagesToExport)
	{
		if(!PackageToExport.FilePath.IsEmpty())
		{
			bAllEmpty = false;
			break;
		}
	}

	if(bAllEmpty)
	{
		return false;
	}

	return true;
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
	TMap<FString, FDependenciesInfo> DependenciesInfos;
	GetAssetDependecies(DependenciesInfos);

	SaveDependenciesInfo(DependenciesInfos);

	GeneratePakFiles(DependenciesInfos);

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
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;

	SettingsView = EditModule.CreateDetailView(DetailsViewArgs);;
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

void GenerateIndividualPakFiles(const TArray<FString>& PackagesToHandle, const FString& MainPackage)
{
	FString HashedMainPackageName = HashStringWithSHA1(MainPackage);
	FString PakOutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExportPak/Paks"), HashedMainPackageName);

	float AmountOfWorkProgress = static_cast<float>(PackagesToHandle.Num());
	float CurrentProgress = 0.0f;
	FScopedSlowTask SlowTask(AmountOfWorkProgress);
	SlowTask.MakeDialog();
	for (auto& PackageNameInGameDir : PackagesToHandle)
	{
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
				UE_LOG(LogExportPak, Error, TEXT("        TryConvertFilenameToLongPackageName Failed: %s!"), *FailedReason);
				return;
			}
			else
			{
				UE_LOG(LogExportPak, Log, TEXT("        %s"), *TargetLongPackageName);
			}
		}

		SlowTask.EnterProgressFrame(CurrentProgress, FText::Format(NSLOCTEXT("ExportPak", "GenerateIndividualPakFiles", "Dependent asset {0}"), FText::FromString(TargetLongPackageName)));

		FString TargetAssetFilepath;
		{
			bool bConvertionResult = FPackageName::TryConvertLongPackageNameToFilename(
				TargetLongPackageName,
				TargetAssetFilepath,
				".uasset"
			);
			if (!bConvertionResult)
			{
				UE_LOG(LogExportPak, Error, TEXT("        TryConvertLongPackageNameToFilename Failed!"));
				return;
			}
			else
			{
				UE_LOG(LogExportPak, Log, TEXT("            %s"), *TargetAssetFilepath);
			}
		}

		FString Filename = FPaths::GetBaseFilename(TargetAssetFilepath);
		FString ProjectName = FPaths::GetBaseFilename(FPaths::GetProjectFilePath());
		FString IntermediateDirectory = FPaths::GetPath(TargetAssetFilepath).Replace(*FPaths::ProjectDir(), TEXT(""), ESearchCase::CaseSensitive);

		// TODO ArcEcho: Now it is hard-coded to WindowsNoEditor
		FString TargetCookedAssetDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Cooked/WindowsNoEditor"), ProjectName, IntermediateDirectory);

		FCookedAssetFileVisitor CookedAssetFileVisitor(Filename);
		FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*TargetCookedAssetDirectory, CookedAssetFileVisitor);

		FString ResponseFileContent = "";
		for (auto &f : CookedAssetFileVisitor.Files)
		{
			FString Ext = FPaths::GetExtension(f);
			FString RelativePathForResponseFile = FPaths::Combine(TEXT("../../.."), ProjectName, IntermediateDirectory, Filename) + FString(".") + Ext;

			ResponseFileContent += FString::Printf(TEXT("\"%s\" \"%s\"\n"), *f, *RelativePathForResponseFile);
		}

		FString HashedPackageName = HashStringWithSHA1(PackageNameInGameDir);
		FString LogFilepath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExportPak/Temp"), HashedPackageName + ".log");
		FString OutputPakFilepath = FPaths::Combine(PakOutputDirectory, HashedPackageName + TEXT(".pak"));

		FString ResponseFilepath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExportPak/Temp"), HashedPackageName + "_Paklist.txt");
		FFileHelper::SaveStringToFile(ResponseFileContent, *ResponseFilepath);

		FString UnrealPakExeFilepath = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/Win64/UnrealPak.exe"));
		FPaths::MakeStandardFilename(UnrealPakExeFilepath);
		UnrealPakExeFilepath = FPaths::ConvertRelativePathToFull(UnrealPakExeFilepath);

		FString CommandLine = FString::Printf(
			TEXT("%s -create=%s -encryptionini -platform=Windows -installed -UTF8Output -multiprocess -patchpaddingalign=2048 -abslog=%s"),
			*OutputPakFilepath,
			*ResponseFilepath,
			*LogFilepath
		);

		void* PipeRead = nullptr;
		void* PipeWrite = nullptr;
		int32 ReturnCode = -1;

		verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));
		bool bLaunchDetached = false;
		bool bLaunchHidden = true;
		bool bLaunchReallyHidden = true;
		uint32* OutProcessID = nullptr ;
		int32 PriorityModifier = -1;
		const TCHAR* OptionalWorkingDirectory = nullptr;
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
			*UnrealPakExeFilepath, *CommandLine,
			bLaunchDetached, bLaunchHidden, bLaunchReallyHidden,
			OutProcessID, PriorityModifier,
			OptionalWorkingDirectory,
			PipeWrite
		);


		if (ProcessHandle.IsValid())
		{
			FPlatformProcess::WaitForProc(ProcessHandle);
			FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);
			if (ReturnCode == 0)
			{
				TArray<FString> OutResults;
				const FString StdOut = FPlatformProcess::ReadPipe(PipeRead);
				StdOut.ParseIntoArray(OutResults, TEXT("\n"), true);
				UE_LOG(LogExportPak, Log, TEXT("ExportPak success:\n%s"), *StdOut);
			}
			else
			{
				TArray<FString> OutErrorMessages;
				const FString StdOut = FPlatformProcess::ReadPipe(PipeRead);
				StdOut.ParseIntoArray(OutErrorMessages, TEXT("\n"), true);
				UE_LOG(LogExportPak, Warning, TEXT("ExportPak Falied:\nReturnCode=%d\n%s"), ReturnCode, *StdOut);
			}

			FPlatformProcess::ClosePipe(PipeRead, PipeWrite);
			FPlatformProcess::CloseProc(ProcessHandle);
		}
		else
		{
			UE_LOG(LogExportPak, Error, TEXT(" Failed to launch unrealPak.exe: %s"), *UnrealPakExeFilepath);
		}

		CurrentProgress += 1.0f;
	}
}

void GenerateBatchPakFiles(const TArray<FString>& PackagesToHandle, const FString& MainPackage)
{
	FString HashedMainPackageName = HashStringWithSHA1(MainPackage);
	FString PakOutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExportPak/Paks"), HashedMainPackageName);

	FString ResponseFileContent = "";

	for (auto& PackageNameInGameDir : PackagesToHandle)
	{
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
				UE_LOG(LogExportPak, Error, TEXT("        TryConvertFilenameToLongPackageName Failed: %s!"), *FailedReason);
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
				UE_LOG(LogExportPak, Error, TEXT("        TryConvertLongPackageNameToFilename Failed!"));
				return;
			}
			else
			{
				UE_LOG(LogExportPak, Log, TEXT("            %s"), *TargetAssetFilepath);
			}
		}

		FString Filename = FPaths::GetBaseFilename(TargetAssetFilepath);
		FString ProjectName = FPaths::GetBaseFilename(FPaths::GetProjectFilePath());
		FString IntermediateDirectory = FPaths::GetPath(TargetAssetFilepath).Replace(*FPaths::ProjectDir(), TEXT(""), ESearchCase::CaseSensitive);

		// TODO ArcEcho: Now it is hard-coded to WindowsNoEditor
		FString TargetCookedAssetDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Cooked/WindowsNoEditor"), ProjectName, IntermediateDirectory);

		FCookedAssetFileVisitor CookedAssetFileVisitor(Filename);
		FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*TargetCookedAssetDirectory, CookedAssetFileVisitor);

		for (auto &f : CookedAssetFileVisitor.Files)
		{
			FString Ext = FPaths::GetExtension(f);
			FString RelativePathForResponseFile = FPaths::Combine(TEXT("../../.."), ProjectName, IntermediateDirectory, Filename) + FString(".") + Ext;

			ResponseFileContent += FString::Printf(TEXT("\"%s\" \"%s\"\n"), *f, *RelativePathForResponseFile);
		}
	}

	FString HashedPackageName = HashStringWithSHA1(MainPackage);
	FString LogFilepath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExportPak/Temp"), HashedPackageName + ".log");
	FString OutputPakFilepath = FPaths::Combine(PakOutputDirectory, HashedPackageName + TEXT(".pak"));

	FString ResponseFilepath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExportPak/Temp"), HashedPackageName + "_Paklist.txt");
	FFileHelper::SaveStringToFile(ResponseFileContent, *ResponseFilepath);

	FString UnrealPakExeFilepath = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/Win64/UnrealPak.exe"));
	FPaths::MakeStandardFilename(UnrealPakExeFilepath);
	UnrealPakExeFilepath = FPaths::ConvertRelativePathToFull(UnrealPakExeFilepath);

	FString CommandLine = FString::Printf(
		TEXT("%s -create=%s -encryptionini -platform=Windows -installed -UTF8Output -multiprocess -patchpaddingalign=2048 -abslog=%s"),
		*OutputPakFilepath,
		*ResponseFilepath,
		*LogFilepath
	);

	void* PipeRead = nullptr;
	void* PipeWrite = nullptr;
	int32 ReturnCode = -1;

	verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));
	bool bLaunchDetached = false;
	bool bLaunchHidden = true;
	bool bLaunchReallyHidden = true;
	uint32* OutProcessID = nullptr;
	int32 PriorityModifier = -1;
	const TCHAR* OptionalWorkingDirectory = nullptr;
	FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
		*UnrealPakExeFilepath, *CommandLine,
		bLaunchDetached, bLaunchHidden, bLaunchReallyHidden,
		OutProcessID, PriorityModifier,
		OptionalWorkingDirectory,
		PipeWrite
	);

	if (ProcessHandle.IsValid())
	{
		FPlatformProcess::WaitForProc(ProcessHandle);
		FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);
		if (ReturnCode == 0)
		{
			TArray<FString> OutResults;
			const FString StdOut = FPlatformProcess::ReadPipe(PipeRead);
			StdOut.ParseIntoArray(OutResults, TEXT("\n"), true);
			UE_LOG(LogExportPak, Log, TEXT("ExportPak success:\n%s"),  *StdOut);
		}
		else
		{
			TArray<FString> OutErrorMessages;
			const FString StdOut = FPlatformProcess::ReadPipe(PipeRead);
			StdOut.ParseIntoArray(OutErrorMessages, TEXT("\n"), true);
			UE_LOG(LogExportPak, Warning, TEXT("ExportPak Falied:\nReturnCode=%d\n%s"), ReturnCode, *StdOut);
		}

		FPlatformProcess::CloseProc(ProcessHandle);
		FPlatformProcess::ClosePipe(PipeRead, PipeWrite);
	}
	else
	{
		UE_LOG(LogExportPak, Error, TEXT(" Failed to launch unrealPak.exe: %s"), *UnrealPakExeFilepath);
	}

}

void SExportPak::GeneratePakFiles(const TMap<FString, FDependenciesInfo> &DependenciesInfos)
{
	float AmountOfWorkProgress = static_cast<float>(DependenciesInfos.Num());
	float CurrentProgress = 0.0f;
	FScopedSlowTask SlowTask(AmountOfWorkProgress);
	SlowTask.MakeDialog();
	for (auto &DependencyInfo : DependenciesInfos)
	{
		SlowTask.EnterProgressFrame(CurrentProgress, FText::Format(NSLOCTEXT("ExportPak", "GeneratePakFiles", "Exporting Paks of asset: {0}"), FText::FromString(DependencyInfo.Key)));

		FString HashedLongPackageName = HashStringWithSHA1(DependencyInfo.Key);
		TArray<FString> PackagesToHandle = DependencyInfo.Value.DependenciesInGameContentDir;
		PackagesToHandle.Add(DependencyInfo.Key);
		
		if(ExportPakSettings->bUseBatchMode)
		{
			GenerateBatchPakFiles(PackagesToHandle, DependencyInfo.Key);
		}
		else
		{
			GenerateIndividualPakFiles(PackagesToHandle, DependencyInfo.Key);
		}

		SavePakDescriptionFile(DependencyInfo.Key, DependencyInfo.Value);
		CurrentProgress += 1.0f;
	}
}

void SExportPak::SavePakDescriptionFile(const FString& TargetPackage, const FDependenciesInfo& DependecyInfo)
{
	FString HashedMainPackageName = HashStringWithSHA1(TargetPackage);
	FString PakOutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExportPak/Paks"), HashedMainPackageName);

	TSharedPtr<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject);
	{
		RootJsonObject->SetStringField("long_package_name", TargetPackage);

		FString PakFilepath = FPaths::Combine(PakOutputDirectory, HashedMainPackageName + TEXT(".pak"));

		RootJsonObject->SetStringField("pak_file", HashedMainPackageName + TEXT(".pak"));
		RootJsonObject->SetStringField("asset_class", DependecyInfo.AssetClassString);

		FString FileSizeInBytes = FString::Printf(TEXT("%lld"), FPlatformFileManager::Get().GetPlatformFile().FileSize(*PakFilepath));
		RootJsonObject->SetStringField("file_size_in_bytes", FileSizeInBytes);
	}

	TArray<TSharedPtr<FJsonValue>> DependencyEntries;
	for (const auto& DependencyInGameContentDir : DependecyInfo.DependenciesInGameContentDir)
	{
		TSharedPtr<FJsonObject> EntryJsonObject = MakeShareable(new FJsonObject);

		FString HashedPackageName = HashStringWithSHA1(DependencyInGameContentDir);
		FString PakFilepath = FPaths::Combine(PakOutputDirectory, HashedPackageName + TEXT(".pak"));
		FString FileSizeInBytes = FString::Printf(TEXT("%lld"), FPlatformFileManager::Get().GetPlatformFile().FileSize(*PakFilepath));

		EntryJsonObject->SetStringField("long_package_name", DependencyInGameContentDir);
		EntryJsonObject->SetStringField("pak_file", HashedPackageName + TEXT(".pak"));
		EntryJsonObject->SetStringField("file_size_in_bytes", FileSizeInBytes);

		TSharedRef< FJsonValueObject > JsonValue = MakeShareable(new FJsonValueObject(EntryJsonObject));
		DependencyEntries.Add(JsonValue);
	}
	RootJsonObject->SetArrayField("dependencies_in_game_content_dir", DependencyEntries);

	FString OutputString;
	auto JsonWirter = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWirter);

	FString PakDescriptionFilename = FPaths::Combine(PakOutputDirectory,  HashedMainPackageName + TEXT(".json"));
	PakDescriptionFilename = FPaths::ConvertRelativePathToFull(PakDescriptionFilename);

	// Attention to FFileHelper::EEncodingOptions::ForceUTF8 here. 
	// In some case, UE4 will save as UTF16 according to the content.
	bool bSaveSuccess = FFileHelper::SaveStringToFile(OutputString, *PakDescriptionFilename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSaveSuccess)
	{
		UE_LOG(LogExportPak, Error, TEXT("Failed to save pak description file: %s"), *PakDescriptionFilename);
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

	FString ResultFileFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExportPak"), TEXT("/AssetDependencies.json"));
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
	}
	else
	{
		UE_LOG(LogExportPak, Error, TEXT("Failed to export %s"), *ResultFileFilename);
	}
}

#undef LOCTEXT_NAMESPACE