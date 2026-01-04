#include "pch.h"
#include "Engine/Plugins/Neon/Public/Neon.h"

#include "Minhook.h"
#include "Engine/Plugins/Kismet/Public/KismetHookingLibrary.h"
#include "Engine/Plugins/Kismet/Public/KismetMemLibrary.h"
#include "Engine/Plugins/Neon/Public/Patches/MCP.h"
#include "Engine/Runtime/Engine/Classes/World.h"
#include "Engine/Runtime/Engine/Classes/AI/NavigationSystemConfig.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/FortGameModeAthena.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/AI/FortServerBotManagerAthena.h"
#include "Engine/Runtime/FortniteGame/Public/Building/BuildingFoundation.h"
#include "Engine/Runtime/GameplayAbilities/Public/AbilitySystemComponent.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/Player/FortPlayerControllerAthena.h"
#include "Engine/Runtime/FortniteGame/Public/Building/BuildingContainer.h"
#include "Engine/Runtime/FortniteGame/Public/Building/BuildingSMActor.h"
#include "Engine/Runtime/FortniteGame/Public/Components/FortControllerComponent_Aircraft.h"
#include "Engine/Runtime/FortniteGame/Public/Player/FortPlayerController.h"
#include "Engine/Runtime/FortniteGame/Public/Quests/FortQuestManager.h"
#include "Engine/Runtime/NavigationSystem/Public/NavigationSystem.h"
#include "Engine/Plugins/Kismet/Public/FortKismetLibrary.h"
#include "Engine/Runtime/Engine/Classes/GameplayStatics.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/Vehicles/FortAthenaVehicle.h"
#include "Engine/Runtime/FortniteGame/Public/STW/FortGameModeOutpost.h"
#include "Engine/Runtime/Engine/Classes/Components/SceneComponent.h"
#include "Engine/Runtime/FortniteGame/Public/FortEngine.h"
#include "Engine/Runtime/FortniteGame/Public/AI/PlayerBots.h"
#include "Engine/Runtime/FortniteGame/Public/Components/FortControllerComponent_Interaction.h"
#include "Engine/Runtime/FortniteGame/Public/Creative/B_Prj_Athena_PlaysetGrenade.h"
#include "Engine/Runtime/FortniteGame/Public/Creative/FortMinigame.h"
#include "Engine/Runtime/FortniteGame/Public/Weapons/FortDecoTool.h"

void InitializeMMRInfos();

inline __int64 (*RandomCrashOG)(__int64 a1);
__int64 RandomCrash(__int64 a1)
{
	auto Huh = *(__int64**)(a1 + 2176);
	if (!Huh)
		return 0;
	return RandomCrashOG(a1);
}

inline __int64 (*Test2OG)(__int64 a1, float a2);
__int64 Test2(__int64 a1, float a2)
{
	auto weirdPtr = *(__int64*)(a1 + 584) - 1;
	if (weirdPtr <= 0)
	{
		return 0;
	}

	auto weird = (__int64*)(a1 + 576);
	if (IsBadReadPtr(weird, sizeof(__int64)))
	{
		return 0;
	}
	
	return Test2OG(a1, a2);
}

static void* (*ProcessEventOG)(UObject*, UFunction*, void*);
static bool bLogProcessEvent = true;
static std::vector<std::string> LoggedFunctions;
void* ProcessEvent(UObject* Obj, UFunction* Function, void* Params)
{
	if (Function && bLogProcessEvent)
	{
		static bool firstCall = true;
		static std::ofstream logFile("ProcessEvent.log", firstCall ? std::ios::trunc : std::ios::app);
		if (firstCall) firstCall = false;

		std::string FunctionName = Function->GetFName().ToString().ToString();

		if (FunctionName.contains("OnLoaded_3645F4484F4ECED813C69D92F55C7A1F"))
		{
			struct OnLoaded_3645F4484F4ECED813C69D92F55C7A1F final { class UObject* Loaded;  };
			auto Param = *(OnLoaded_3645F4484F4ECED813C69D92F55C7A1F*)Params;
			((AB_Prj_Athena_PlaysetGrenade_C*)Obj)->OnLoaded_3645F4484F4ECED813C69D92F55C7A1F(Param.Loaded);
		}

		if (FunctionName.contains("OnAircraftExitedDropZone"))
		{		
			if (GNeon->bLategame)
			{
				auto GameMode = Cast<AFortGameModeAthena>(GetWorld()->GetAuthorityGameMode());
				if (!GameMode) return ProcessEventOG(Obj, Function, Params);
				for (auto& Player : GameMode->GetAlivePlayers())
				{
					if (!Player->IsValidLowLevel()) continue;
					if (Player->IsInAircraft())
						Player->ServerAttemptAircraftJump({});
				}
			}
		}

		static bool bJoining = false;
		if (FunctionName.contains("HandlePlaylistLoaded") && !bJoining)
		{
			bJoining = true;
			if (GNeon->bProd)
			{
				std::thread([]() {
					std::this_thread::sleep_for(std::chrono::seconds(10));
					auto itSession = std::find_if(args.begin(), args.end(), [](const std::wstring& s) {
						return s.rfind(L"-session=", 0) == 0;
						});

					if (itSession != args.end())
					{
						std::wstring sessionValue = itSession->substr(9);
						std::wstring path = L"/fortnite/api/game/v2/matchmakingservice/start/" + sessionValue + L"?port=" + std::to_wstring(GNeon->Port);
						PostRequest(L"matchmaking-service.fluxfn.org", path);
					}
				}).detach();
			}
		
		}
		
		if (std::find(LoggedFunctions.begin(), LoggedFunctions.end(), FunctionName) == LoggedFunctions.end()) {
			std::cout << "ProcessEvent: " << FunctionName << std::endl;
			LoggedFunctions.push_back(FunctionName);
		}

		return ProcessEventOG(Obj, Function, Params);
	}
}

void Return() { return; }

// ignore processevent ^

void UNeon::Initialize()
{
	if ( !this->bHasInitialized )
	{
		Sleep(8000);
		
		Init(); // SDK::Init

		UKismetMemLibrary::LoadCache(); // (should) work
		
		this->bHasInitialized = true;

		MH_Initialize();
		
		UHook* Hook = new UHook();

		if (Fortnite_Version.Season() >= 7)
		{
			uintptr_t Collision = Memcury::Scanner::FindPattern("40 53 48 83 EC ? 33 C0 48 89 7C 24 ? 48 89 44 24 ? 48 8D 54 24 ? 48 89 44 24 ? 48 8B D9").Get();
			Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("Listen"));
			Hook->Swap = Collision ? Collision : Memcury::Scanner::FindPattern("40 53 48 83 EC ? 48 8B 01 48 8D 54 24 ? 48 89 74 24 ? 48 8B D9").Get();
			CREATE_HOOK(Listen, Hook, (UWorld* a1, FURL URL) -> void, (URL));
			if (!Hook->Address || !Hook->Swap)
			{
				Hook->Path = "/Script/Engine.GameMode.ReadyToStartMatch";
				CREATE_HOOK(ReadyToStartMatch, Hook, (AFortGameModeAthena* a1, FFrame& Stack, bool* Res) -> bool, (Stack, Res))
				UKismetHookingLibrary::Hook(Hook, EHook::Exec);
			}
			UKismetHookingLibrary::Hook(Hook, EHook::Modify);
		} else
		{
			Hook->Path = "/Script/Engine.GameMode.ReadyToStartMatch";
			CREATE_HOOK(ReadyToStartMatch, Hook, (AFortGameModeAthena* a1, FFrame& Stack, bool* Res) -> bool, (Stack, Res))
			UKismetHookingLibrary::Hook(Hook, EHook::Exec);
		}
		
		Hook->Path = "Server_SetCanEditCreativeIsland";
		Hook->Class = AFortPlayerStateAthena::StaticClass();
		Hook->Original = reinterpret_cast<void**>(&AFortPlayerStateAthena::Server_SetCanEditCreativeIslandOG);
		CREATE_HOOK(Server_SetCanEditCreativeIsland, Hook, (AFortPlayerStateAthena * a1, bool bCanEdit, TArray<class FString> WhiteList) -> bool, (bCanEdit, WhiteList))
		UKismetHookingLibrary::Hook(Hook, EHook::StaticVFT);

		Hook->Path = "/Script/Engine.PlayerController.ServerAcknowledgePossession";
		CREATE_HOOK(ServerAcknowledgePossession, Hook, (AFortPlayerControllerAthena* a1, FFrame& Stack) -> void, (Stack))
		UKismetHookingLibrary::Hook(Hook, EHook::Exec);
		
		Hook->Path = "/Script/FortniteGame.BuildingFoundation.SetDynamicFoundationTransform";
		Hook->Original = reinterpret_cast<void**>(&ABuildingFoundation::SetDynamicFoundationTransformOG);
		CREATE_HOOK(SetDynamicFoundationTransform, Hook, (ABuildingFoundation* a1, FFrame& Stack) -> void, (Stack))
		if (Fortnite_Version >= 10.00) UKismetHookingLibrary::Hook(Hook, EHook::Exec);
		
		Hook->Path = "ServerPlaySquadQuickChatMessage";
		Hook->Class = AFortPlayerControllerAthena::StaticClass();
		CREATE_HOOK(ServerPlaySquadQuickChatMessage, Hook, (AFortPlayerControllerAthena * a1, __int64 ChatEntry, __int64 SenderID) -> void, (ChatEntry, SenderID))
	//	UKismetHookingLibrary::Hook(Hook, EHook::EveryStaticVFT);

		Hook->Path = "ServerRemoveInventoryItem";
		Hook->Class = AFortPlayerControllerAthena::StaticClass();
		CREATE_HOOK(ServerRemoveInventoryItem, Hook, (AFortPlayerControllerAthena* a1, FGuid ItemGuid, int Count, bool bForceRemoveFromQuickBars, bool bForceRemoval) -> bool, (ItemGuid, Count, bForceRemoveFromQuickBars, bForceRemoval))
		UKismetHookingLibrary::Hook(Hook, EHook::StaticVFT);

		Hook->Path = "/Script/FortniteGame.BuildingFoundation.SetDynamicFoundationEnabled";
		Hook->Original = reinterpret_cast<void**>(&ABuildingFoundation::SetDynamicFoundationEnabledOG);
		CREATE_HOOK(SetDynamicFoundationEnabled, Hook, (ABuildingFoundation* a1, FFrame& Stack) -> void, (Stack))
		if (Fortnite_Version >= 10.00) UKismetHookingLibrary::Hook(Hook, EHook::Exec);
		
		Hook->Path = "/Script/FortniteGame.FortAthenaVehicle.ServerUpdatePhysicsParams";
		Hook->Original = reinterpret_cast<void**>(&AFortAthenaVehicle::ServerUpdatePhysicsParamsOG);
		CREATE_HOOK(ServerUpdatePhysicsParams, Hook, (AFortAthenaVehicle * a1, FFrame & Stack) -> void, (Stack))
		UKismetHookingLibrary::Hook(Hook, EHook::Exec);

		Hook->Path = "/Script/FortniteGame.FortPhysicsPawn.ServerUpdatePhysicsParams";
		Hook->Original = reinterpret_cast<void**>(&AFortAthenaVehicle::ServerUpdatePhysicsParamsOG);
		CREATE_HOOK(ServerUpdatePhysicsParams, Hook, (AFortAthenaVehicle * a1, FFrame & Stack) -> void, (Stack))
		UKismetHookingLibrary::Hook(Hook, EHook::Exec);

		Hook->Path = "/Script/FortniteGame.FortPhysicsPawn.ServerMove";
		Hook->Original = reinterpret_cast<void**>(&AFortAthenaVehicle::ServerUpdatePhysicsParamsOG);
		CREATE_HOOK(ServerUpdatePhysicsParams, Hook, (AFortAthenaVehicle * a1, FFrame & Stack) -> void, (Stack))
		UKismetHookingLibrary::Hook(Hook, EHook::Exec);

		Hook->Path = "/Script/FortniteGame.FortPlayerController.ServerAttemptInteract";
		Hook->Original = reinterpret_cast<void**>(&AFortPlayerController::ServerAttemptInteractOG);
		CREATE_HOOK(ServerAttemptInteract, Hook, (AFortPlayerController* a1, FFrame& Stack) -> void, (Stack))
		UKismetHookingLibrary::Hook(Hook, EHook::Exec);

		Hook->Path = "/Script/FortniteGame.FortControllerComponent_Interaction.ServerAttemptInteract";
		Hook->Original = reinterpret_cast<void**>(&UFortControllerComponent_Interaction::ServerAttemptInteractOG);
		CREATE_HOOK(ServerAttemptInteract, Hook, (UFortControllerComponent_Interaction* a1, FFrame& Stack) -> void, (Stack))
		UKismetHookingLibrary::Hook(Hook, EHook::Exec);

		/*Hook->Path = "/Script/FortniteGame.FortKismetLibrary.PickLootDrops";
		CREATE_HOOK(PickLootDropsHook, Hook, (UFortKismetLibrary* a1, FFrame& Stack) -> bool, (Stack))
		UKismetHookingLibrary::Hook(Hook, EHook::Exec);
		*/

		//UE_LOG(LogTemp, Log, "AGameModeBase_VFT: %#lx", __int64(AGameModeBase::GetDefaultObj()->GetVTable()) - IMAGEBASE);

		if (bDedicatedServer)
		{
			Hook->Address = IMAGEBASE + 0x2167290; // TODO: Make universal
			Hook->Detour = AFortGameModeAthena::GetGameSessionClass;
			UKismetHookingLibrary::Hook(Hook, EHook::Address);

			Hook->Address = IMAGEBASE + 0xC50010; // TODO: Make universal
			Hook->Detour = AFortGameModeAthena::GetGameSessionClass;
			UKismetHookingLibrary::Hook(Hook, EHook::Address);
		
			Hook->Address = Memcury::Scanner::FindPattern("40 53 48 83 EC ? 48 8B D9 48 8B 89 ? ? ? ? 48 85 C9 74 ? 48 8B 01 FF 90 ? ? ? ? 48 8B 8B ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 83 C4 ? 5B 48 FF A0", false).Get();
			Hook->Original = reinterpret_cast<void**>(&UWorld::BeginPlayOG);
			CREATE_HOOK(BeginPlay, Hook, (UWorld* a1) -> void, ());
			UKismetHookingLibrary::Hook(Hook, EHook::Address);
		}

		if (!bDedicatedServer)
		{
			Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("KickPlayer"));
			UKismetHookingLibrary::Hook(Hook, EHook::RTrue);
			
			Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("ChangeGameSessionID"));
			UKismetHookingLibrary::Hook(Hook, EHook::Null);
		}
		
		if (Fortnite_Version.Season() <= 8 && Fortnite_Version.Season() >= 4)
		{
			Hook->Address = Memcury::Scanner::FindPattern("40 53 55 57 48 83 EC ? 48 8B F9 0F 29 74 24", false).Get();
			Hook->Original = reinterpret_cast<void**>(&Test2OG);
			Hook->Detour = Test2;
			UKismetHookingLibrary::Hook(Hook, EHook::Null);

	//		Hook->Address = Memcury::Scanner::FindPattern("48 8B C4 55 48 83 EC ? 48 89 58 ? 33 ED").Get();
	//		UKismetHookingLibrary::Hook(Hook, EHook::Null);
		}
		
		Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("AActor_GetNetMode"));
		UKismetHookingLibrary::Hook(Hook, EHook::RTrue);
		Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("GetNetMode"));
		UKismetHookingLibrary::Hook(Hook, EHook::RTrue);
		Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("CanActivateAbility"));
		UKismetHookingLibrary::Hook(Hook, EHook::RTrue);
		Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("TickFlush"));
		Hook->Original = reinterpret_cast<void**>(&UNetDriver::TickFlushOG);
		Hook->Detour = UNetDriver::TickFlush;
		UKismetHookingLibrary::Hook(Hook, EHook::Address);
		Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("SendRequestNow"));
		Hook->Original = reinterpret_cast<void**>(&MCP::SendRequestNowOG);
		Hook->Detour = MCP::SendRequestNow;
		UKismetHookingLibrary::Hook(Hook, EHook::Address);

		Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("SetState"));
		Hook->Original = reinterpret_cast<void**>(&AFortMinigame::ChangeMinigameStateOG);
		CREATE_HOOK(ChangeMinigameState, Hook, (AFortMinigame* a1, EFortMinigameState NewState) -> void, (NewState))
		UKismetHookingLibrary::Hook(Hook, EHook::Address);
		
		Hook->Path = "/Script/FortniteGame.FortKismetLibrary.GetAIDirector";
		Hook->Class = UFortKismetLibrary::StaticClass();
		CREATE_HOOK(GetAIDirector, Hook, (UFortKismetLibrary* a1, FFrame& Stack, AFortAIDirector** Ret) -> AFortAIDirector*, (Stack, Ret))
		UKismetHookingLibrary::Hook(Hook, EHook::Exec);

		Hook->Path = "/Script/FortniteGame.FortKismetLibrary.GetAIGoalManager";
		Hook->Class = UFortKismetLibrary::StaticClass();
		CREATE_HOOK(GetAIGoalManager, Hook, (UFortKismetLibrary* a1, FFrame& Stack, AFortAIGoalManager** Ret) -> AFortAIGoalManager*, (Stack, Ret))
		UKismetHookingLibrary::Hook(Hook, EHook::Exec);
		
		Hook->Address = Offsets::UObject__ProcessEvent;
		Hook->Original = reinterpret_cast<void**>(&ProcessEventOG);
		Hook->Detour = ProcessEvent;
		UKismetHookingLibrary::Hook(Hook, EHook::Address);
		
		GetWorld()->GetOwningGameInstance()->GetLocalPlayers().Remove(0);
		bool* GIsClient = UKismetMemLibrary::GetAddress<bool>(TEXT("GIsClient"));
		bool* GIsServer = UKismetMemLibrary::GetAddress<bool>(TEXT("GIsServer"));
		if (GIsClient && GIsServer)
		{
			*GIsClient = false;
			*GIsServer = true;
		}

		if (Fortnite_Version >= 17.00)
		{
			Hook->Address = Memcury::Scanner::FindPattern("48 89 5C 24 10 48 89 6C 24 20 56 57 41 54 41 56 41 57 48 81 EC ? ? ? ? 65 48 8B 04 25 ? ? ? ? 4C 8B F9").Get();
			UKismetHookingLibrary::Hook(Hook, EHook::Null);
			
			if (Fortnite_Version.Season() == 17)
			{
				Hook->Address = Memcury::Scanner::FindPattern("48 8B C4 48 89 70 08 48 89 78 10 55 41 54 41 55 41 56 41 57 48 8D 68 A1 48 81 EC ? ? ? ? 45 33 ED").Get();
				UKismetHookingLibrary::Hook(Hook, EHook::Null);

				Hook->Address = Memcury::Scanner::FindPattern("48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 4C 89 60 20 55 41 56 41 57 48 8B EC 48 83 EC 60 4D 8B F9 41 8A F0 4C 8B F2 48 8B F9 45 32 E4").Get();
				UKismetHookingLibrary::Hook(Hook, EHook::RTrue);
			}	
		}

		if (GNeon->bProd)
		{
			auto it = std::find_if(args.begin(), args.end(), [](const std::wstring& s) { 
				return s.rfind(L"-playlist=", 0) == 0; 
			});

			if (it != args.end())
			{
				std::wstring PlaylistID = it->substr(10);
				if (PlaylistID.contains(L"playground"))
					GNeon->bCreative = true;
			}
		}
		
		if (GNeon->bCreative)
		{
			auto Addr = Memcury::Scanner::FindPattern("48 89 5C 24 ? 48 89 74 24 ? 55 57 41 56 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 5D").Get(); // works on 12.61
			if (Addr)
			{
				Hook->Address = Addr;
				UKismetHookingLibrary::Hook(Hook, EHook::RFalse);
			} else
			{
				Addr = Memcury::Scanner::FindPattern("48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 81 EC ? ? ? ? 48 8B 9C 24 ? ? ? ? 33 FF").Get();
				if (Addr)
				{
					Hook->Address = Addr;
					UKismetHookingLibrary::Hook(Hook, EHook::RFalse);
				}
			}
		} else if (Fortnite_Version < 16.40)
		{
			Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("StartAircraftPhase"));
			Hook->Original = reinterpret_cast<void**>(&AFortGameModeAthena::StartAircraftPhaseOG);
			CREATE_HOOK(StartAircraftPhase, Hook, (AFortGameModeAthena* a1, char a2) -> void, (a2))
			UKismetHookingLibrary::Hook(Hook, EHook::Address);
			
			if (Fortnite_Version.Season() <= 6)
			{
				Hook->Address = Memcury::Scanner::FindPattern("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 8B F1 0F B6 DA").Get();
			} else
			{
				Hook->Address = Memcury::Scanner::FindStringRef(L"TryCollectGarbage: forcing GC after %d skipped attempts.")
.ScanFor({ 0x41, 0x8B })
.ScanFor({ 0xE8 })
.RelativeOffset(1).Get();
			}
			
			Hook->Detour = Return;
			UKismetHookingLibrary::Hook(Hook, EHook::Address);

			if (Fortnite_Version.Season() >= 8)
			{
				Hook->Address = Memcury::Scanner::FindStringRef(L"STAT_CollectGarbageInternal").ScanFor({ 0x48, 0x89, 0x5C }, false, 0, 1, 2000).Get();
				Hook->Detour = Return;
				UKismetHookingLibrary::Hook(Hook, EHook::RFalse);
			} 
		}
		
		if (bProd)
		{
			auto it = std::find_if(args.begin(), args.end(), [](const std::wstring& s) {
				return s.rfind(L"-map=", 0) == 0;
				});
			if (it != args.end())
			{
				std::wstring value = it->substr(5);

				std::wstring Command = L"open " + value;
				ExecuteConsoleCommand(GetWorld(), FString(Command.c_str()), nullptr);

				auto itSession = std::find_if(args.begin(), args.end(), [](const std::wstring& s) {
					return s.rfind(L"-session=", 0) == 0;
					});

				if (itSession != args.end())
				{
					std::wstring sessionValue = itSession->substr(9);
					std::wstring path = L"/fortnite/api/game/v2/matchmakingservice/assign/" + sessionValue;
					PostRequest(L"matchmaking-service.fluxfn.org", path);
				}
			}
		} else
		{
			FString WorldName;
			if (bOutpost)
				WorldName = L"open Zone_Outpost_Stonewood?game=outpost";
			else if (Fortnite_Version <= 10.40)
				WorldName = L"open Athena_Terrain";
			else if (Fortnite_Version <= 18.40) 
				WorldName = L"open Apollo_Terrain";
			else if (Fortnite_Version <= 22.40) 
				WorldName = L"open Artemis_Terrain";
			else 
				WorldName = (Fortnite_Version >= 23.00) ? L"open Asteria_Terrain" : WorldName;
		
                        ExecuteConsoleCommand(GetWorld(), WorldName, nullptr);

                        ExecuteConsoleCommand(GetWorld(), L"log LogFortPlacement VeryVerbose", nullptr);
                        ExecuteConsoleCommand(GetWorld(), L"log LogFortWorld VeryVerbose", nullptr);
                        ExecuteConsoleCommand(GetWorld(), L"log LogAthenaBots VeryVerbose", nullptr);

                        static bool bInitializedMMR = false;
                        if (!bInitializedMMR)
                        {
                                InitializeMMRInfos();
                                bInitializedMMR = true;
                        }
                }
        }
}
 
void InitializeMMRInfos()
{
        UAthenaAIServicePlayerBots* AIServicePlayerBots = nullptr;

        if (auto* AthenaAIBlueprintLibraryClass = StaticClassImpl("AthenaAIBlueprintLibrary"))
        {
                if (auto* AthenaAIBlueprintLibrary = AthenaAIBlueprintLibraryClass->GetClassDefaultObject())
                {
                        AIServicePlayerBots = AthenaAIBlueprintLibrary->CallFunc<UAthenaAIServicePlayerBots*>("AthenaAIBlueprintLibrary", "GetAIServicePlayerBots", GetWorld());
                }
        }

        if (!AIServicePlayerBots)
        {
                if (auto* AIServicePlayerBotsClass = StaticClassImpl("AthenaAIServicePlayerBots"))
                {
                        AIServicePlayerBots = static_cast<UAthenaAIServicePlayerBots*>(AIServicePlayerBotsClass->GetClassDefaultObject());
                }
        }

        if (!AIServicePlayerBots)
        {
                UE_LOG(LogNeon, Warning, TEXT("InitializeMMRInfos: unable to locate AI service; skipping bot MMR setup"));
                return;
        }

        AIServicePlayerBots->SetDefaultBotAISpawnerData(StaticLoadObject<UClass>("/Game/Athena/AI/Phoebe/BP_AISpawnerData_Phoebe.BP_AISpawnerData_Phoebe_C"));

        FMMRSpawningInfo NewSpawningInfo{};
        NewSpawningInfo.BotSpawningDataInfoTargetELO = 1400.f;
        NewSpawningInfo.BotSpawningDataInfoWeight = 100.f;
        NewSpawningInfo.NumBotsToSpawn = 60;
        NewSpawningInfo.AISpawnerData = AIServicePlayerBots->GetDefaultBotAISpawnerData();

        AIServicePlayerBots->SetDefaultAISpawnerDataComponentList(UFortAthenaAISpawnerData::CreateComponentListFromClass(AIServicePlayerBots->GetDefaultBotAISpawnerData(), GetWorld()));
        AIServicePlayerBots->GetCachedMMRSpawningInfo().SpawningInfos.Add(NewSpawningInfo);
        AIServicePlayerBots->SetGamePhaseToStartSpawning(EAthenaGamePhase::Warmup);
        AIServicePlayerBots->SetbWaitForNavmeshToBeLoaded(false);
        *reinterpret_cast<bool*>(__int64(AIServicePlayerBots) + 0x820) = true; //bCanActivateBrain
}

void UNeon::ChangeState(const wchar_t* State) 
{
	time_t t = time(nullptr);
	tm lt;
	(void)localtime_s(&lt, &t);

	std::wostringstream oss;
	oss << TEXT("Neon-V2 | ") << State
		<< TEXT(" | Compiled at: ")
		<< std::put_time(&lt, TEXT("%I:%M:%S %p"));

	SetConsoleTitleW(oss.str().c_str());

	if (State && wcsstr(State, L"Listening "))
		PostHook();
}

void UNeon::UpdatePort(int32 P)
{
	Port = P;
}

void UNeon::PostHook()
{
	UHook* Hook = new UHook();
	/*Hook->Path = "/Script/GameplayAbilities.AbilitySystemComponent.ServerTryActivateAbility";
	CREATE_HOOK(
		ServerTryActivateAbility, Hook,
		(UAbilitySystemComponent* a1, FFrame& Stack) -> void,
		(Stack)
	)
	UKismetHookingibrary::Hook(Hook, EHook::Exec);
		
	Hook->Path = "ServerTryActivateAbilityWithEventData";
	Hook->Class = StaticClassImpl("FortAbilitySystemComponent");
	CREATE_HOOK(
		ServerTryActivateAbilityWithEventData, Hook,
		(UAbilitySystemComponent* a1, FGameplayAbilitySpecHandle Handle, bool InputPressed, FPredictionKey PredictionKey, const FGameplayEventData& TriggerEventData) -> void,
		(Handle, InputPressed, PredictionKey, TriggerEventData)
	)
	UKismetHookingibrary::Hook(Hook, EHook::StaticVFT);
*/
	Hook->Path = "SpawnDefaultPawnFor";
	Hook->Class = AGameMode::StaticClass();
	CREATE_HOOK(SpawnDefaultPawnFor, Hook, (AFortGameModeAthena* a1, APlayerController* NewPlayer, AActor* StartSpot) -> APawn*, (NewPlayer, StartSpot))
	UKismetHookingLibrary::Hook(Hook, EHook::EveryStaticVFT);
		
	Hook->Path = "HandleStartingNewPlayer";
	Hook->Class = AGameMode::StaticClass();
	Hook->Original = reinterpret_cast<void**>(&AFortGameModeAthena::HandleStartingNewPlayerOG);
	CREATE_HOOK(HandleStartingNewPlayer, Hook, (AFortGameModeAthena* a1, APlayerController* NewPlayer) -> void, (NewPlayer))
	UKismetHookingLibrary::Hook(Hook, EHook::EveryStaticVFT);

	Hook->Path = "ServerExecuteInventoryItem";
	Hook->Class = AFortPlayerController::StaticClass();
	CREATE_HOOK(ServerExecuteInventoryItem, Hook, (AFortPlayerControllerAthena* a1, FGuid ItemGuid) -> void, (ItemGuid))
	UKismetHookingLibrary::Hook(Hook, EHook::EveryStaticVFT);
	
	int InternalServerTryActivateAbilityIndex = 0;

	if (Engine_Version > 4.20)
	{
		static auto OnRep_ReplicatedAnimMontageFn = StaticFindObject<UFunction>("/Script/GameplayAbilities.AbilitySystemComponent.OnRep_ReplicatedAnimMontage");
		if (OnRep_ReplicatedAnimMontageFn)
		{
			InternalServerTryActivateAbilityIndex = ( UKismetHookingLibrary::GetVTableIndex(OnRep_ReplicatedAnimMontageFn) - 8) / 8;
			UE_LOG(LogTemp, Log, "InternalServerTryActivateAbilityIndex: 0x%d", InternalServerTryActivateAbilityIndex);
		}
	} else
	{
		static auto ServerTryActivateAbilityWithEventDataFn = StaticFindObject<UFunction>("/Script/GameplayAbilities.AbilitySystemComponent.ServerTryActivateAbilityWithEventData");
		auto addr = (__int64)UAbilitySystemComponent::GetDefaultObj()->GetVTable()[UKismetHookingLibrary::GetVTableIndex(ServerTryActivateAbilityWithEventDataFn) / 8];

		for (int i = 0; i < 400; i++) {
			uint8_t b1 = *(uint8_t*)(addr + i), b2 = *(uint8_t*)(addr + i + 1);
			if ((b1 == 0xFF && (b2 == 0x90 || b2 == 0x93))) {
				auto h = [](__int64 callAddr) -> __int64 {
					std::string hex; bool found = false;
					for (__int64 z = callAddr + 5; z != callAddr + 1; z--) {
						int v = *(uint8_t*)z;
						std::string s = (v < 0x10 ? "0" : "") + std::format("{:x}", v);
						if (v == 0 ? found : true) { hex += s; found = true; }
					}
					std::transform(hex.begin(), hex.end(), hex.begin(), ::toupper);
					__int64 res = 0, base = 1;
					for (int i = (int)hex.size() - 1; i >= 0; i--, base *= 16)
						res += (hex[i] <= '9' ? hex[i] - '0' : hex[i] - 'A' + 10) * base;
					return res;
				};
				InternalServerTryActivateAbilityIndex = h(addr + i) / 8;
				UE_LOG(LogTemp, Log, "InternalServerTryActivateAbilityIndex: 0x%d", InternalServerTryActivateAbilityIndex);
				break;
			}
		}
	}

	if (InternalServerTryActivateAbilityIndex != 0)
	{
		Hook->Address = InternalServerTryActivateAbilityIndex;
		Hook->Class = UAbilitySystemComponent::StaticClass();
		CREATE_HOOK(
			InternalServerTryActivateAbility, Hook,
			(UAbilitySystemComponent* a1, FGameplayAbilitySpecHandle Handle,
			 bool InputPressed, FPredictionKey* PredictionKey,
			 FGameplayEventData* TriggerEventData) -> void,
			(Handle, InputPressed, PredictionKey, TriggerEventData)
		)
		UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);
	}

	if (Fortnite_Version.Season() >= 7)
	{
		uintptr_t Collision = Memcury::Scanner::FindPattern("40 53 48 83 EC ? 33 C0 48 89 7C 24 ? 48 89 44 24 ? 48 8D 54 24 ? 48 89 44 24 ? 48 8B D9").Get();
		Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("Listen"));
		Hook->Swap = Collision ? Collision : Memcury::Scanner::FindPattern("40 53 48 83 EC ? 48 8B 01 48 8D 54 24 ? 48 89 74 24 ? 48 8B D9").Get();
		if (!Hook->Address || !Hook->Swap)
		{
			Hook->Path = "/Script/Engine.GameMode.ReadyToStartMatch";
			CREATE_HOOK(ReadyToStartMatch, Hook, (AFortGameModeAthena* a1, FFrame& Stack, bool* Res) -> bool, (Stack, Res))
			UKismetHookingLibrary::Hook(Hook, EHook::Exec);
		}
	}
	
	Hook->Address = UKismetMemLibrary::Get<uintptr_t>(L"GetMaxTickRate");
	Hook->Class = UFortEngine::StaticClass();
	CREATE_HOOK(GetMaxTickRate, Hook, (UFortEngine* a1) -> float, ());
	UKismetHookingLibrary::Hook(Hook, EHook::VFT);

	Hook->Address = Memcury::Scanner::FindPattern("48 8B C4 57 48 81 EC ? ? ? ? 4C 8B 82 ? ? ? ? 48 8B F9 0F 29 70 E8 0F 29 78 D8").Get();
	UKismetHookingLibrary::Hook(Hook, EHook::Null);

	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("StartNewSafeZonePhase"));
	Hook->Original = reinterpret_cast<void**>(&AFortGameModeAthena::StartNewSafeZonePhaseOG);
	CREATE_HOOK(StartNewSafeZonePhase, Hook, (AFortGameModeAthena* a1, int NewSafeZonePhase) -> void, (NewSafeZonePhase))
	if (Fortnite_Version.Season() >= 7) UKismetHookingLibrary::Hook(Hook, EHook::Address);

	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("SpawnBot"));
	Hook->Original = reinterpret_cast<void**>(&UFortServerBotManagerAthena::SpawnBotOG);
	CREATE_HOOK(SpawnBot, Hook, (UFortServerBotManagerAthena* a1, FVector SpawnLoc, FRotator SpawnRot, UFortAthenaAIBotCustomizationData* BotData, FFortAthenaAIBotRunTimeCustomizationData& RuntimeBotData) -> APawn*, (SpawnLoc, SpawnRot, BotData, RuntimeBotData))
	if (Fortnite_Version >= 12.00 && Fortnite_Version < 14) UKismetHookingLibrary::Hook(Hook, EHook::Address);

	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("SendStatEventWithTags"));
	Hook->Original = reinterpret_cast<void**>(&UFortMinigameManager::SendStatEventWithTagsOG);
	CREATE_HOOK(SendStatEventWithTags, Hook,
		(UFortMinigameManager* a1, EFortQuestObjectiveStatEvent Type, UObject* TargetObject, FGameplayTagContainer& TargetTags, FGameplayTagContainer& SourceTags, FGameplayTagContainer& ContextTags, int Count) ->
		void, (Type, TargetObject, TargetTags, SourceTags, ContextTags, Count))
	if (Fortnite_Version.Season() >= 9) UKismetHookingLibrary::Hook(Hook, EHook::Address);

	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("SendComplexCustomStatEvent"));
	Hook->Original = reinterpret_cast<void**>(&UFortQuestManager::SendComplexCustomStatEventOG);
	CREATE_HOOK(SendComplexCustomStatEvent, Hook,
		(UFortQuestManager* a1, UObject* TargetObj, FGameplayTagContainer& SourceTags, FGameplayTagContainer& TargetTags, bool* QuestActive, bool* QuestCompleted, int32 Count) ->
		void, (TargetObj, SourceTags, TargetTags, QuestActive, QuestCompleted, Count))
	UKismetHookingLibrary::Hook(Hook, EHook::Address);

	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("UseSpareAmmo"));
	Hook->Original = reinterpret_cast<void**>(&AFortWeapon::UseSpareAmmoOG);
	CREATE_HOOK(UseSpareAmmo, Hook, (AFortWeapon* a1, int AmountToUse) -> void, (AmountToUse))
	UKismetHookingLibrary::Hook(Hook, EHook::Address);

	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("ApplyCost"));
	Hook->Original = reinterpret_cast<void**>(&UFortGameplayAbility::ApplyCostOG);
	CREATE_HOOK(ApplyCost, Hook, (UFortGameplayAbility* a1, void *a2, void *a3, void *a4) -> __int64, (a2, a3, a4))
	UKismetHookingLibrary::Hook(Hook, EHook::Address);

	Hook->Path = "/Script/FortniteGame.FortPlayerController.ServerEmote";
	CREATE_HOOK(ServerEmote, Hook, (AFortPlayerController* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortDecoTool.ServerCreateBuildingAndSpawnDeco";
	CREATE_HOOK(ServerCreateBuildingAndSpawnDeco, Hook, (AFortDecoTool* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);
	
	Hook->Path = "/Script/FortniteGame.FortKismetLibrary.K2_SpawnPickupInWorld";
	Hook->Original = reinterpret_cast<void**>(&UFortKismetLibrary::K2_SpawnPickupInWorldOG);
	CREATE_HOOK(K2_SpawnPickupInWorld, Hook, (UFortKismetLibrary* a1, FFrame& Stack, AFortPickup** Ret) -> AFortPickup*, (Stack, Ret))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortKismetLibrary.GiveItemToInventoryOwner";
	Hook->Original = reinterpret_cast<void**>(&UFortKismetLibrary::GiveItemToInventoryOwnerOG);
	CREATE_HOOK(GiveItemToInventoryOwner, Hook, (UFortKismetLibrary* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);
	
	Hook->Path = "/Script/FortniteGame.FortKismetLibrary.K2_GiveItemToPlayer";
	Hook->Original = reinterpret_cast<void**>(&UFortKismetLibrary::K2_GiveItemToPlayerOG);
	CREATE_HOOK(K2_GiveItemToPlayer, Hook, (UFortKismetLibrary* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortPlayerController.ServerLoadingScreenDropped";
	CREATE_HOOK(ServerLoadingScreenDropped, Hook, (AFortPlayerController* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortPlayerControllerAthena.ServerGiveCreativeItem";
	CREATE_HOOK(ServerGiveCreativeItem, Hook, (AFortPlayerControllerAthena* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortPlayerControllerAthena.ServerTeleportToPlaygroundLobbyIsland";
	CREATE_HOOK(ServerTeleportToPlaygroundLobbyIsland, Hook, (AFortPlayerControllerAthena* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortPlayerControllerAthena.ServerClientIsReadyToRespawn";
	CREATE_HOOK(ServerClientIsReadyToRespawn, Hook, (AFortPlayerControllerAthena* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);
	
	Hook->Path = "/Script/FortniteGame.FortAthenaCreativePortal.TeleportPlayerToLinkedVolume";
	CREATE_HOOK(TeleportPlayerToLinkedVolume, Hook, (AFortAthenaCreativePortal* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);
	
	Hook->Path = "/Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor";
	CREATE_HOOK(ServerCreateBuildingActor, Hook, (AFortPlayerController* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortPlayerController.ServerBeginEditingBuildingActor";
	CREATE_HOOK(ServerBeginEditingBuildingActor, Hook, (AFortPlayerController* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortPlayerController.ServerEditBuildingActor";
	CREATE_HOOK(ServerEditBuildingActor, Hook, (AFortPlayerController* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortPlayerController.ServerEndEditingBuildingActor";
	CREATE_HOOK(ServerEndEditingBuildingActor, Hook, (AFortPlayerController* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortPlayerController.ServerRepairBuildingActor";
	CREATE_HOOK(ServerRepairBuildingActor, Hook, (AFortPlayerController* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortPlayerController.ServerAttemptInventoryDrop";
	CREATE_HOOK(ServerAttemptInventoryDrop, Hook, (AFortPlayerController* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortPlayerController.ServerPlayEmoteItem";
	CREATE_HOOK(ServerPlayEmoteItem, Hook, (AFortPlayerController* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("ClientOnPawnDied"));
	Hook->Original = reinterpret_cast<void**>(&AFortPlayerControllerAthena::ClientOnPawnDiedOG);
	CREATE_HOOK(ClientOnPawnDied, Hook, (AFortPlayerControllerAthena* a1, FFortPlayerDeathReport Death) -> void, (Death))
	UKismetHookingLibrary::Hook(Hook, EHook::Address);

	auto Addr = PropLibrary->GetFunctionByName("FortPlayerPawn", "ServerSetAttachment").Func;
	if (Addr)
	{
		Hook->Address = (uintptr_t)Addr->GetNativeFunc();
		Hook->Detour = Return;
		UKismetHookingLibrary::Hook(Hook, EHook::Address);
	}
	
	if (bDedicatedServer)
	{
		Hook->Address = IMAGEBASE + 0xDBCDC0;
		Hook->Original = reinterpret_cast<void**>(&AFortMission::InitializeActorsForMissionOG);
		Hook->Detour = (void*)AFortMission::InitializeActorsForMission;
		UKismetHookingLibrary::Hook(Hook, EHook::Address);
	}
	
	Hook->Path = "/Script/FortniteGame.FortControllerComponent_Aircraft.ServerAttemptAircraftJump";
	CREATE_HOOK(
		ServerAttemptAircraftJump, Hook,
		(UFortControllerComponent_Aircraft * a1, FFrame & Stack) -> void,
		(Stack)
	)
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("OnDamageServer"));
	Hook->Original = reinterpret_cast<void**>(&ABuildingSMActor::OnDamageServerOG);
	CREATE_HOOK(OnDamageServer, Hook, (ABuildingSMActor* a1, float Damage, FGameplayTagContainer DamageTags, FVector Momentum, FHitResult HitInfo, AActor* InstigatedBy, AActor* DamageCauser, FGameplayEffectContextHandle Context) -> void, (Damage, DamageTags, Momentum, HitInfo, InstigatedBy, DamageCauser, Context))
	UKismetHookingLibrary::Hook(Hook, EHook::Address);

	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("GetPlayerViewPoint"));
	Hook->Original = reinterpret_cast<void**>(&AFortPlayerController::GetPlayerViewPointOG);
	CREATE_HOOK(GetPlayerViewPoint, Hook, (AFortPlayerController* a1, FVector& OutViewLocation, FRotator& OutViewRotation) -> void, (OutViewLocation, OutViewRotation))
	UKismetHookingLibrary::Hook(Hook, EHook::Address);
	
	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("CreateAndConfigureNavigationSystem"));
	Hook->Original = reinterpret_cast<void**>(&UAthenaNavSystemConfig::CreateAndConfigureNavigationSystemOG);
	CREATE_HOOK(CreateAndConfigureNavigationSystem, Hook, (UAthenaNavSystemConfig* a1, UWorld* World) -> void, (World))
	if (Fortnite_Version >= 10.00) UKismetHookingLibrary::Hook(Hook, EHook::Address);

	switch (Fortnite_Version <= 11.50 ? 1 : Fortnite_Version <= 13.00 ? 2 : Fortnite_Version >= 14.00 ? 3 : 4)
	{
		case 1: Hook->Address = 0x50; break;
		case 2: Hook->Address = 0x53; break;
		case 3: Hook->Address = 0x55; break;
		case 4: Hook->Address = 0x73; break;
	}
	UE_LOG(LogTemp, Log, "NavigationSystemV1::InitializeForWorld: %d", Hook->Address);
	Hook->Original = reinterpret_cast<void**>(&UNavigationSystemV1::InitializeForWorldOG);
	Hook->Class = UNavigationSystemV1::StaticClass();
	CREATE_HOOK(InitializeForWorld, Hook, (UNavigationSystemV1* a1, UWorld* World, uint8 Mode) -> void, (World, Mode))
	if (Fortnite_Version >= 10.00 && !Fortnite_Version.ToString().contains("11.31")) UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);

	if (Fortnite_Version.ToString().contains("11.31") && false)
	{
		Hook->Address = IMAGEBASE + 0x27E8B00; // server context (don't remove this is temp, ONLY REMOVE IF U FIND UNI SOLUTION)
		UKismetHookingLibrary::Hook(Hook, EHook::RTrue);

		Hook->Address = IMAGEBASE + 0x16414A0;
		Hook->Original = (void**)&AFortAthenaMutatorOnSafeZoneUpdatedOG;
		Hook->Detour = AFortAthenaMutatorOnSafeZoneUpdated;
		UKismetHookingLibrary::Hook(Hook, EHook::Address);

		Hook->Address = IMAGEBASE + 0x1654BB0;
		Hook->Original = (void**)&MutatorInitializeMMRInfosOG;
		Hook->Detour = MutatorInitializeMMRInfos;
		UKismetHookingLibrary::Hook(Hook, EHook::Address);
		
		Hook->Address = IMAGEBASE + 0x163DCA0;
		UKismetHookingLibrary::Hook(Hook, EHook::RTrue);

		Hook->Address = IMAGEBASE + 0x1648100;
		Hook->Detour = Test;
		UKismetHookingLibrary::Hook(Hook, EHook::Address);
	}

	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("PickTeam"));
	Hook->Original = reinterpret_cast<void**>(&AFortGameModeAthena::PickTeamOG);
	CREATE_HOOK(PickTeam, Hook, (AFortGameModeAthena* a1, uint8_t PreferredTeam, AFortPlayerControllerAthena* Controller) -> EFortTeam, (PreferredTeam, Controller))
	if (GNeon->bProd) UKismetHookingLibrary::Hook(Hook, EHook::Address);
	
	if (Fortnite_Version >= 11.50)
	{
		auto Context = Memcury::Scanner::FindPattern("E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? 80 3D ? ? ? ? ? 0F 82 ? ? ? ? 49 8B 46 ? 48 8D 54 24").Get();
		for (int i = 0; i < 2; i++) {
			Hook->Address = Context + 7 + i;
			Hook->Byte = (i == 0) ? 0x0F : 0x8D;
			UKismetHookingLibrary::Hook(Hook, EHook::Byte);
		}
		
		if (Fortnite_Version == 15.50)
		{
			Hook->Address = IMAGEBASE + 0x1F646A0;
			Hook->Original = reinterpret_cast<void**>(&RandomCrashOG);
			Hook->Detour = RandomCrash;
			UKismetHookingLibrary::Hook(Hook, EHook::Address);
		}
	}
	
	if (bOutpost)
	{
		Hook->Address = IMAGEBASE + 0xD2C0E0;
		Hook->Original = reinterpret_cast<void**>(&AFortWorldManager::GetCurrentTheaterMapDataOG);
		CREATE_HOOK(GetCurrentTheaterMapData, Hook, (AFortWorldManager* a1) -> void*, ());
		UKismetHookingLibrary::Hook(Hook, EHook::Address);
	}
	
	Hook->Path = "/Script/FortniteGame.FortPawn.MovingEmoteStopped";
	Hook->Original = reinterpret_cast<void**>(&AFortPawn::MovingEmoteStoppedOG);
	CREATE_HOOK(MovingEmoteStopped, Hook, (AFortPawn* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	if (Fortnite_Version <= 13.00)
	{
		Hook->Path = "/Script/FortniteGame.FortPlayerPawn.ServerHandlePickup";
		CREATE_HOOK(ServerHandlePickup, Hook, (AFortPlayerPawn* a1, FFrame& Stack) -> void, (Stack))
		UKismetHookingLibrary::Hook(Hook, EHook::Exec);
	} else
	{
		Hook->Path = "/Script/FortniteGame.FortPlayerPawn.ServerHandlePickupInfo";
		CREATE_HOOK(ServerHandlePickupInfo, Hook, (AFortPlayerPawn* a1, FFrame& Stack) -> void, (Stack))
		UKismetHookingLibrary::Hook(Hook, EHook::Exec);
	}

	Hook->Path = "/Script/FortniteGame.FortPlayerPawn.ServerSendZiplineState";
	CREATE_HOOK(ServerSendZiplineState, Hook, (AFortPlayerPawn* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "/Script/FortniteGame.FortPlayerPawn.OnCapsuleBeginOverlap";
	CREATE_HOOK(OnCapsuleBeginOverlap, Hook, (AFortPlayerPawn* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Address = Memcury::Scanner::FindPattern("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B 41 ? 48 8B F2 48 8D 54 24").Get();
	if (!Hook->Address)
		Hook->Address = Memcury::Scanner::FindPattern("48 89 5C 24 ? 57 48 83 EC ? 48 8B 41 ? 48 8B FA 48 8D 54 24").Get();
	Hook->Original = reinterpret_cast<void**>(&UFortQuestManager::SendCustomStatEventOG);
	CREATE_HOOK(SendCustomStatEvent, Hook, (UFortQuestManager* a1, struct FDataTableRowHandle& ObjectiveStat, int32 Count, bool bForceFlush) -> void, (ObjectiveStat, Count, bForceFlush))
	if (Fortnite_Version <= 9.41) UKismetHookingLibrary::Hook(Hook, EHook::Address);

	Hook->Path = "/Script/FortniteGame.FortPlayerPawnAthena.OnCapsuleBeginOverlap";
	CREATE_HOOK(OnCapsuleBeginOverlap, Hook, (AFortPlayerPawn* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Path = "ServerAttemptAircraftJump";
	Hook->Class = AFortPlayerControllerAthena::StaticClass();
	CREATE_HOOK(ServerAttemptAircraftJump, Hook, (AFortPlayerController* a1, FRotator Rotation) -> void, (Rotation))
	UKismetHookingLibrary::Hook(Hook, EHook::StaticVFT);

	Hook->Path = Fortnite_Version >= 6.10 ? "/Script/FortniteGame.FortPawn.NetMulticast_Athena_BatchedDamageCues" : "/Script/FortniteGame.FortPlayerPawnAthena.NetMulticast_Athena_BatchedDamageCues";
	Hook->Class = AFortPawn::StaticClass();
	Hook->Original = reinterpret_cast<void**>(&AFortPawn::NetMulticast_Athena_BatchedDamageCuesOG);
	CREATE_HOOK(NetMulticast_Athena_BatchedDamageCues, Hook, (AFortPawn* a1, FFrame& Stack) -> void, (Stack))
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);
	
	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("CompletePickupAnimation"));
	Hook->Original = reinterpret_cast<void**>(&AFortPlayerPawn::CompletePickupAnimationOG);
	Hook->Detour = AFortPlayerPawn::CompletePickupAnimation;
	UKismetHookingLibrary::Hook(Hook, EHook::Address);

	Hook->Address = (uintptr_t)UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("SpawnLoot"));
	CREATE_HOOK(SpawnLoot, Hook, (ABuildingContainer* a1) -> bool, ())
	UKismetHookingLibrary::Hook(Hook, EHook::Address);

	auto matchmaking = Memcury::Scanner::FindPattern("83 BD ? ? ? ? 01 7F 18 49 8D 4D D8 48 8B D6 E8 ? ? ? ? 48", false).Get(); 
	if (!matchmaking)
		matchmaking = Memcury::Scanner::FindPattern("83 7D 88 01 7F 0D 48 8B CE E8", false).Get();
	if (!matchmaking)
		matchmaking = Memcury::Scanner::FindPattern("83 BD ? ? ? ? ? 7F 18 49 8D 4D D8 48 8B D7 E8").Get(); 
	if (!matchmaking)
		matchmaking = Memcury::Scanner::FindPattern("83 7C 24 ?? 01 7F 0D 48 8B CF E8").Get();
	
	/* reboot thx */
	bool bMatchmakingSupported = matchmaking;
	int idx = 0;
	if (bMatchmakingSupported) {
		for (int i = 0; i < 9; i++) {
			auto byte = (uint8_t*)(matchmaking + i);
			if (IsBadReadPtr(byte, 1)) continue;
			if (*byte == 0x7F) {
				bMatchmakingSupported = true;
				idx = i;
				break;
			}
			bMatchmakingSupported = false;
		}
	}
	if (bMatchmakingSupported) {
		auto before = (uint8_t*)(matchmaking + idx);
		DWORD dwProtection;
		VirtualProtect((PVOID)before, 1, PAGE_EXECUTE_READWRITE, &dwProtection);
		*before = 0x74;
		DWORD dwTemp;
		VirtualProtect((PVOID)before, 1, dwProtection, &dwTemp);
	}
}