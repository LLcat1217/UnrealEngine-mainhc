// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealFrontendPrivatePCH.h"
#include "AutomationController.h"
#include "ISlateReflectorModule.h"
#include "Projects.h"
#include "SlateBasics.h"
#include "StandaloneRenderer.h"
#include "TaskGraphInterfaces.h"
#include "UserInterfaceCommand.h"


#define IDEAL_FRAMERATE 60;


namespace UserInterfaceCommand
{
	TSharedPtr<FTabManager::FLayout> ApplicationLayout;
	TSharedRef<FWorkspaceItem> DeveloperTools = FWorkspaceItem::NewGroup( NSLOCTEXT("UnrealFrontend", "DeveloperToolsMenu", "Developer Tools") );
}


/* FUserInterfaceCommand interface
 *****************************************************************************/

void FUserInterfaceCommand::Run(  )
{
	FString UnrealFrontendLayoutIni = FPaths::GetPath(GEngineIni) + "/Layout.ini";

	// load required modules
	FModuleManager::Get().LoadModuleChecked("EditorStyle");
	FModuleManager::Get().LoadModuleChecked("Messaging");

	IAutomationControllerModule& AutomationControllerModule = FModuleManager::LoadModuleChecked<IAutomationControllerModule>("AutomationController");
	AutomationControllerModule.Init();

	// load plug-ins
	// @todo: allow for better plug-in support in standalone Slate applications
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreDefault);

	// load optional modules
	FModuleManager::Get().LoadModule("DeviceManager");
	FModuleManager::Get().LoadModule("ProfilerClient");
	FModuleManager::Get().LoadModule("ProjectLauncher");
	FModuleManager::Get().LoadModule("SessionFrontend");
	FModuleManager::Get().LoadModule("SettingsEditor");

	InitializeSlateApplication(UnrealFrontendLayoutIni);

	// enter main loop
	double DeltaTime = 0.0;
	double LastTime = FPlatformTime::Seconds();
	const float IdealFrameTime = 1.0f / IDEAL_FRAMERATE;

	while (!GIsRequestingExit)
	{
		//Save the state of the tabs here rather than after close of application (the tabs are undesirably saved out with ClosedTab state on application close)
		//UserInterfaceCommand::UserConfiguredNewLayout = FGlobalTabmanager::Get()->PersistLayout();

		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();
		FTicker::GetCoreTicker().Tick(DeltaTime);
		AutomationControllerModule.Tick();

		// throttle frame rate
		FPlatformProcess::Sleep(FMath::Max<float>(0.0f, IdealFrameTime - (FPlatformTime::Seconds() - LastTime)));

		double CurrentTime = FPlatformTime::Seconds();
		DeltaTime =  CurrentTime - LastTime;
		LastTime = CurrentTime;

		FStats::AdvanceFrame( false );

		GLog->FlushThreadedLogs();
	}

	ShutdownSlateApplication(UnrealFrontendLayoutIni);
}


/* FUserInterfaceCommand implementation
 *****************************************************************************/

void FUserInterfaceCommand::InitializeSlateApplication( const FString& LayoutIni )
{
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());
	FGlobalTabmanager::Get()->SetApplicationTitle(NSLOCTEXT("UnrealFrontend", "AppTitle", "Unreal Frontend"));

	// load widget reflector
	const bool bAllowDebugTools = FParse::Param(FCommandLine::Get(), TEXT("DebugTools"));

	if (bAllowDebugTools)
	{
		ISlateReflectorModule* SlateReflectorModule = FModuleManager::GetModulePtr<ISlateReflectorModule>("SlateReflector");

		if (SlateReflectorModule != nullptr)
		{
			SlateReflectorModule->RegisterTabSpawner(UserInterfaceCommand::DeveloperTools);
		}
	}

	// restore application layout
	TSharedRef<FTabManager::FLayout> NewLayout = FTabManager::NewLayout("SessionFrontendLayout_v1.1")
		->AddArea
		(
			FTabManager::NewArea(1280.f, 720.0f)
				->Split
				(
					FTabManager::NewStack()
						->AddTab(FName("DeviceManager"), ETabState::OpenedTab)
						->AddTab(FName("MessagingDebugger"), ETabState::ClosedTab)
						->AddTab(FName("SessionFrontend"), ETabState::OpenedTab)
						->AddTab(FName("ProjectLauncher"), ETabState::OpenedTab)
				)
		)
		->AddArea
		(
			FTabManager::NewArea(600.0f, 600.0f)
				->SetWindow(FVector2D(10.0f, 10.0f), false)
				->Split
				(
					FTabManager::NewStack()->AddTab("WidgetReflector", bAllowDebugTools ? ETabState::OpenedTab : ETabState::ClosedTab)
				)
		);

	UserInterfaceCommand::ApplicationLayout = FLayoutSaveRestore::LoadFromConfig(LayoutIni, NewLayout);
	FGlobalTabmanager::Get()->RestoreFrom(UserInterfaceCommand::ApplicationLayout.ToSharedRef(), TSharedPtr<SWindow>());
}


void FUserInterfaceCommand::ShutdownSlateApplication( const FString& LayoutIni )
{
	check(UserInterfaceCommand::ApplicationLayout.IsValid());

	// save application layout
	FLayoutSaveRestore::SaveToConfig(LayoutIni, UserInterfaceCommand::ApplicationLayout.ToSharedRef());
	GConfig->Flush(false, LayoutIni);

	// shut down application
	FSlateApplication::Shutdown();
}
