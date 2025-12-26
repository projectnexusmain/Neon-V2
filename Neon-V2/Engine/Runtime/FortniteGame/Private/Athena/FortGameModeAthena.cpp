#include "pch.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/FortGameModeAthena.h"

#include "Creative/FortAthenaCreativePortal.cpp"
#include "Engine/Plugins/Kismet/Public/DataTableFunctionLibrary.h"
#include "Engine/Plugins/Kismet/Public/FortKismetLibrary.h"
#include "Engine/Plugins/Kismet/Public/KismetMathLibrary.h"
#include "Engine/Plugins/Kismet/Public/KismetSystemLibrary.h"
#include "Engine/Plugins/Neon/Public/Neon.h"
#include "Engine/Runtime/Engine/Classes/GameplayStatics.h"
#include "Engine/Runtime/Engine/Classes/World.h"
#include "Engine/Runtime/Engine/Classes/Components/SceneComponent.h"
#include "Engine/Runtime/Engine/CoreUObject/Public/UObject/Misc.h"
#include "Engine/Runtime/Engine/CoreUObject/Public/UObject/UObjectGlobals.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/FortPlaylistAthena.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/AI/FortServerBotManagerAthena.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/Player/FortPlayerControllerAthena.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/Vehicles/FortAthenaVehicle.h"
#include "Engine/Runtime/FortniteGame/Public/Building/BuildingFoundation.h"
#include "Engine/Runtime/FortniteGame/Public/Building/BuildingSMActor.h"
#include "Engine/Runtime/FortniteGame/Public/Components/FortLevelSaveComponent.h"
#include "Engine/Runtime/FortniteGame/Public/Components/PlaysetLevelStreamComponent.h"
#include "Engine/Runtime/FortniteGame/Public/Items/FortQuickBars.h"
#include "Engine/Runtime/FortniteGame/Public/Player/FortPlayerController.h"
#include "Engine/Runtime/FortniteGame/Public/Player/FortPlayerState.h"
#include "Engine/Runtime/FortniteGame/Public/STW/FortGameModeOutpost.h"
#include "Engine/Runtime/FortniteGame/Public/STW/FortGameStateOutpost.h"
#include "Engine/Runtime/FortniteGame/Public/STW/Player/FortPlayerControllerOutpost.h"
#include "Engine/Runtime/FortniteGame/Public/STW/Player/FortPlayerStateOutpost.h"
#include "Engine/Runtime/FortniteGame/Public/ItemDefinitions/FortPlaysetItemDefinition.h"
#include "Engine/Runtime/FortniteGame/Public/Creative/FortAthenaCreativePortal.h"

struct FObjectKey
{
public:
    UObject* ResolveObjectPtr() const
    {
        FWeakObjectPtr WeakPtr;
        WeakPtr.ObjectIndex = ObjectIndex;
        WeakPtr.ObjectSerialNumber = ObjectSerialNumber;

        return WeakPtr.Get();
    }

    int32 ObjectIndex;
    int32 ObjectSerialNumber;
};

enum class EClassRepNodeMapping : uint8
{
    NotRouted                                = 0,
    RelevantAllConnections                   = 1,
    Spatialize_Static                        = 2,
    Spatialize_Dynamic                       = 3,
    Spatialize_Dormancy                      = 4,
    EClassRepNodeMapping_MAX                 = 5,
};

TMap<FObjectKey, EClassRepNodeMapping>& GetClassReplicationNodePolicies(UReplicationDriver* Driver)
{
    auto Res = Memcury::Scanner::FindStringRef(L"Fort Replication Routing Policies").ScanFor({0x48, 0x81, 0xC3}).Get();
    
    return *reinterpret_cast<TMap<FObjectKey, EClassRepNodeMapping>*>(__int64(Driver) + *reinterpret_cast<uint32_t*>(Res + 3));
}

bool AFortGameModeAthena::ReadyToStartMatch(FFrame& Stack, bool* Result)
{
    Stack.IncrementCode();
    AFortGameStateAthena* GameState = GetGameState();
    
    if ( !GameState ) return *Result = false;
    if ( !GameState->GetMapInfo() ) return *Result = false;

    SetbWorldIsReady(true);

    if ( Fortnite_Version >= 6.10 ? GetCurrentPlaylistId() == -1 : !GameState->GetCurrentPlaylistData() )
    {
        UFortPlaylistAthena* Playlist = GNeon->bCreative ? (UFortPlaylistAthena*)GUObjectArray.FindObject( "Playlist_PlaygroundV2" ) : (UFortPlaylistAthena*)GUObjectArray.FindObject( "Playlist_DefaultSolo" );
        if (GNeon->bProd)
        {
            auto it = std::find_if(args.begin(), args.end(), [](const std::wstring& s) { 
                return s.rfind(L"-playlist=", 0) == 0; 
            });

            if (it != args.end())
            {
                std::wstring PlaylistID = it->substr(10);
                std::transform(PlaylistID.begin(), PlaylistID.end(), PlaylistID.begin(), ::towlower);

                for (auto& It : TGetObjectsOfClass<UFortPlaylistAthena>())
                {
                    std::wstring playlistName = It->GetPlaylistName().ToString().ToWideString();
                    std::transform(playlistName.begin(), playlistName.end(), playlistName.begin(), ::towlower);
    
                    if (wcsstr(playlistName.c_str(), PlaylistID.c_str()) != NULL)
                    {
                        Playlist = It;
                        break;
                    }
                }
            }
        }
        
        GameState->SetPlaylist( Playlist );

        static bool bBotManagerInitialized = false;
        if (!bBotManagerInitialized)
        {
            UFortServerBotManagerAthena::Init();
            bBotManagerInitialized = true;
        }
        return *Result = false;
    }

    SetbEnableReplicationGraph(true);

    if (!GetWorld()->GetNetDriver())
    {
        FURL URL{};
        URL.Port = GNeon->bProd ? UKismetMathLibrary::RandomIntegerInRange(7777, 8888) : 7777;
     //   URL.Port = 7777;
        GetWorld()->Listen(URL);
    }

    GetGameSession()->SetMaxPlayers(100);

    bool Res = GetWorld()->GetNetDriver()->GetClientConnections().Num() > 0;

    static bool bSetGameMode = false;
    
    if (Cast<AFortGameModeAthena>(GetWorld()->GetAuthorityGameMode()) && !bSetGameMode && Res)
    {
        bSetGameMode = true;
        static bool bFirstJoin = true;
        if (bFirstJoin)
        {
            bFirstJoin = false;
            UFortKismetLibrary::SpawnFloorLootForContainer(StaticLoadObject<UBlueprintGeneratedClass>("/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C"));
            UFortKismetLibrary::SpawnFloorLootForContainer(StaticLoadObject<UBlueprintGeneratedClass>("/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C"));

            UFortKismetLibrary::SpawnBGAConsumables(); // hop rocks

            if (Fortnite_Version.Season() >= 6)
            {
                TArray<AActor*> VehicleSpawners = UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFortAthenaVehicleSpawner::StaticClass());
                for (auto& VehicleSpawner : VehicleSpawners)
                {
                    auto Spawner = (AFortAthenaVehicleSpawner*)VehicleSpawner;
                    FTransform Transform = Spawner->GetTransform();
                    GetWorld()->SpawnActor<AFortAthenaVehicle>(Spawner->CallFunc<UClass*>("FortAthenaVehicleSpawner", "GetVehicleClass"), Transform.Translation, Spawner->K2_GetActorRotation());
                }
            } 
        }
    }
    else if (Cast<AFortGameModeOutpost>(GetWorld()->GetAuthorityGameMode()) && !bSetGameMode)
    {
        bSetGameMode = true;
        auto GameModeOutpost = Cast<AFortGameModeOutpost>(GetWorld()->GetAuthorityGameMode());
        auto GSOutpost = Cast<AFortGameStateOutpost>(GameModeOutpost->GetGameState());

        if (GSOutpost)
        {
            GSOutpost->SetZoneTheme(StaticLoadObject<UFortZoneTheme>("/Game/World/ZoneThemes/Outposts/BP_ZT_TheOutpost_PvE_01.BP_ZT_TheOutpost_PvE_01_C"));
        }
    }

    static bool bSetPolicies = false;
    
    if (GetWorld()->GetNetDriver() && !bSetPolicies && GNeon->bOutpost)
    {
        bSetPolicies = true;
        auto Map = GetClassReplicationNodePolicies(GetWorld()->GetNetDriver()->GetReplicationDriver());
        for (auto&& It : Map)
        {
            auto Obj = It.Key.ResolveObjectPtr();
            
            if (Obj == AFortInventory::StaticClass() || Obj == AFortQuickBars::StaticClass())
            {
                It.Value = EClassRepNodeMapping::RelevantAllConnections;
            }
        }
    }

    return *Result = Res;
}

void AFortGameModeAthena::HandleStartingNewPlayer(APlayerController* NewPlayer)
{
    if (!NewPlayer) return HandleStartingNewPlayerOG(this, NewPlayer);
    AFortPlayerStateAthena* PlayerState = Cast<AFortPlayerStateAthena>(NewPlayer->GetPlayerState());
    AFortGameStateAthena* GameState = GetGameState();

    if (Fortnite_Version >= 5.40)// guess
    {
        static UClass* GameMemberInfo = StaticClassImpl("GameMemberInfo");

        if (GameMemberInfo)
        {
            static int32 GameMemberInfoSize = GameMemberInfo->GetSize();
            
            static FGameMemberInfo* Member = nullptr;
            if (!Member)
                Member = (FGameMemberInfo*)VirtualAlloc(0, GameMemberInfoSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

        //    PlayerState->SetSquadId(PlayerState->GetTeamIndex() - 3);
      //      PlayerState->OnRep_SquadId();
            
            Member->MostRecentArrayReplicationKey = -1;
            Member->ReplicationID = -1;
            Member->ReplicationKey = -1;
            Member->TeamIndex = PlayerState->GetTeamIndex(); 
            Member->SquadId = PlayerState->GetSquadId();
            Member->MemberUniqueId = PlayerState->GetUniqueId();

            struct FGameMemberInfoArray final : public FFastArraySerializer
            {
            public:
                TArray<struct FGameMemberInfo>                Members;                                           // 0x0108(0x0010)(ZeroConstructor, NativeAccessSpecifierPrivate)
                class AFortGameStateAthena*                   OwningGameState;                                   // 0x0118(0x0008)(ZeroConstructor, Transient, IsPlainOldData, RepSkip, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPrivate)
            };
            
            static int GameMemberInfoArrayOffset = UKismetMemLibrary::GetOffset(GameState, "GameMemberInfoArray");
            FGameMemberInfoArray& GameMemberInfoArrayPtr = *(FGameMemberInfoArray*)(__int64(GameState) + GameMemberInfoArrayOffset);
            auto& Members = *(TArray<FGameMemberInfo>*)(__int64(&GameMemberInfoArrayPtr) + 0x0108);
            Members.Add(*Member, GameMemberInfoSize);
            GameMemberInfoArrayPtr.MarkItemDirty(Member);
            GameMemberInfoArrayPtr.MarkArrayDirty();
        }

        if (GNeon->bCreative)
        {
            AFortPlayerControllerAthena* PlayerAthena = Cast<AFortPlayerControllerAthena>(NewPlayer);
            AFortAthenaCreativePortal* Portal = GetGameState()->GetCreativePortalManager()->ClaimPortal();
            
            if (!Portal) return HandleStartingNewPlayerOG(this, NewPlayer);

            Portal->GetLinkedVolume()->SetbShowPublishWatermark(false);
            Portal->GetLinkedVolume()->SetbNeverAllowSaving(false);
            Portal->GetLinkedVolume()->SetVolumeState(EVolumeState::Ready);
            Portal->GetLinkedVolume()->OnRep_VolumeState();
            Portal->SetOwningPlayer(PlayerState->GetUniqueId());
            Portal->OnRep_OwningPlayer();

            Portal->GetIslandInfo().SetCreatorName(PlayerState->GetPlayerName());
            Portal->GetIslandInfo().SetSupportCode(L"Test");
            Portal->GetIslandInfo().SetVersion(1.0f);
            Portal->OnRep_IslandInfo();

            Portal->SetbPortalOpen(true);
            Portal->OnRep_PortalOpen();

            Portal->GetPlayersReady().Add(PlayerState->GetUniqueId());
            Portal->OnRep_PlayersReady();

            Portal->SetbUserInitiatedLoad(true);
            Portal->SetbInErrorState(false);

            PlayerAthena->SetOwnedPortal(Portal);
            auto Comp = (UPlaysetLevelStreamComponent*)Portal->GetLinkedVolume()->GetComponentByClass(UPlaysetLevelStreamComponent::StaticClass());
            
            auto Comp2 = (UFortLevelSaveComponent*)Portal->GetLinkedVolume()->GetComponentByClass(UFortLevelSaveComponent::StaticClass());
            Comp2->SetAccountIdOfOwner(PlayerState->GetUniqueId());

            PlayerAthena->SetCreativePlotLinkedVolume(Portal->GetLinkedVolume());
            PlayerAthena->OnRep_CreativePlotLinkedVolume();
            static auto MinigameSettingsMachine = StaticLoadObject<UClass>("/Game/Athena/Items/Gameplay/MinigameSettingsControl/MinigameSettingsMachine.MinigameSettingsMachine_C");
            GetWorld()->SpawnActor(MinigameSettingsMachine, Portal->GetLinkedVolume()->K2_GetActorLocation(), {}, Portal->GetLinkedVolume());

            Comp2->SetbIsLoaded(true);

            static auto Playset = StaticLoadObject<UFortPlaysetItemDefinition>("/Game/Playsets/PID_Playset_60x60_Composed.PID_Playset_60x60_Composed");
            Comp->SetPlayset(Playset);
            static auto LoadPlayset = UKismetMemLibrary::Get<void (*)(UPlaysetLevelStreamComponent*)>(L"LoadPlayset");
            LoadPlayset(Comp);
        }
    }
    
    AFortPlayerControllerAthena* Controller = Cast<AFortPlayerControllerAthena>(NewPlayer);
    if (Controller)
    {
        if (!Controller->GetMatchReport())
            Controller->SetMatchReport((UAthenaPlayerMatchReport*)UGameplayStatics::SpawnObject(UAthenaPlayerMatchReport::StaticClass(), NewPlayer));

        auto& StartingItemsArray = GetStartingItems();
        int32 FItemAndCountSize = StaticClassImpl("ItemAndCount")->GetSize();
        for (int i = 0; i < StartingItemsArray.Num(); i++)
        {
            auto Item = (FItemAndCount*) ((uint8*) StartingItemsArray.GetData() + (i * FItemAndCountSize));
            if ( !Item || !Item->Item ) return HandleStartingNewPlayerOG(this, NewPlayer);
            Cast<AFortPlayerControllerAthena>(NewPlayer)->GetWorldInventory()->GiveItem(Item->Item, Item->Count, 1, 1);
        }
    }
}

APawn* AFortGameModeAthena::SpawnDefaultPawnFor(APlayerController* NewPlayer, AActor* StartSpot)
{
    FTransform Transform{};

    // believe me i have no idea also.
    if (Fortnite_Version.Season() == 4)
    {
        Transform.Rotation = FQuat();
        Transform.Scale3D = FVector(1, 1, 1);
        Transform.Translation = StartSpot->K2_GetActorLocation();
    }
    else Transform = StartSpot->GetTransform();
    
    bool bAthena = NewPlayer->IsA(AFortPlayerControllerAthena::StaticClass());
    if (!bAthena) Transform.Translation.Z += 866.f;
    
    APawn* Pawn = CallFunc<APawn*>("GameModeBase", "SpawnDefaultPawnAtTransform", NewPlayer, Transform);

    auto PlayerState = Cast<AFortPlayerState>(NewPlayer->GetPlayerState());
    
    if (!bAthena)
    {
        AFortQuickBars* QuickBars = GetWorld()->SpawnActor<AFortQuickBars>(AFortQuickBars::StaticClass(), FVector{ 0,0, 3029 }, FRotator(), NewPlayer);

        auto FortPlayerController = Cast<AFortPlayerController>(NewPlayer);

        FortPlayerController->SetQuickBars(QuickBars);
        FortPlayerController->OnRep_QuickBar();
        FortPlayerController->GetQuickBars()->OnRep_PrimaryQuickBar();
        FortPlayerController->GetQuickBars()->OnRep_SecondaryQuickBar();
        FortPlayerController->SetAcknowledgedPawn(Pawn);
        FortPlayerController->OnRep_Pawn();

        static auto EditTool = StaticFindObject<UFortItemDefinition>("/Game/Items/Weapons/BuildingTools/EditTool.EditTool");
        static auto Wall = StaticFindObject<UFortBuildingItemDefinition>("/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall");
        static auto Floor = StaticFindObject<UFortBuildingItemDefinition>("/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor");
        static auto Stair = StaticFindObject<UFortBuildingItemDefinition>("/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W");
        static auto Roof = StaticFindObject<UFortBuildingItemDefinition>("/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS");

        FortPlayerController->GetWorldInventory()->GiveItem(EditTool, 1, 1, 1, 1, false);
        FortPlayerController->GetWorldInventory()->GiveItem(Wall, 1, 1, 1, 0, false);
        FortPlayerController->GetWorldInventory()->GiveItem(Floor, 1, 1, 1, 1, false);
        FortPlayerController->GetWorldInventory()->GiveItem(Stair, 1, 1, 1, 2, false);
        FortPlayerController->GetWorldInventory()->GiveItem(Roof, 1, 1, 1, 3, false);

        static auto Pickaxe = StaticFindObject<UFortWeaponMeleeItemDefinition>("/Game/Items/Weapons/Melee/Harvest/WID_Harvest_Pickaxe_VR_T05.WID_Harvest_Pickaxe_VR_T05");

        FortPlayerController->GetWorldInventory()->GiveItem(Pickaxe, 1, 1, 1, 0, false);

        if (auto PlayerStateOutpost = Cast<AFortPlayerStateOutpost>(PlayerState))
        {
            auto PlayerControllerOutpost = Cast<AFortPlayerControllerOutpost>(FortPlayerController);
            
            //PlayerControllerOutpost->SetbIsOutpostOwnerInPIE(true);
            PlayerStateOutpost->SetbIsWorldDataOwner(true);

            auto GameModeOutpost = Cast<AFortGameModeOutpost>(this);
            auto GameStateOutpost = Cast<AFortGameStateOutpost>(GameModeOutpost->GetGameState());

            GameModeOutpost->GetCurrentCoreInfo().GetAccountsWithEditPermission().Add(PlayerStateOutpost->GetPlayerName());
            GameStateOutpost->SetMissionManager(GetWorld()->SpawnActor<AFortMissionManager>(AFortMissionManager::StaticClass(), {}));
            GameStateOutpost->OnRep_MissionManager();
            
            /*auto Actors = UGameplayStatics::GetAllActorsOfClass(GetWorld(), StaticFindObject<UBlueprintGeneratedClass>("/Game/Missions/Primary/Outpost/Props/BP_StormShield_Core_Egg.BP_StormShield_Core_Egg_C"));
            auto ControlPanelActors = UGameplayStatics::GetAllActorsOfClass(GetWorld(), StaticFindObject<UBlueprintGeneratedClass>("/Game/Missions/Primary/Outpost/Props/BP_Outpost_ControlPanel_New.BP_Outpost_ControlPanel_New_C"));

            UE_LOG(LogTemp, Log, "Actors: %d", Actors.Num());
            
            auto Loc = FVector{-2102.51, -2167.98, 387.23};*/

            auto Info = StaticLoadObject<UFortMissionInfo>("/Game/Missions/Primary/Outpost/MissionInfo_Outpost_PvE_01.MissionInfo_Outpost_PvE_01");
            Info->SetbStartPlayingOnLoad(true);
            
            UFortMissionLibrary::LoadMission(GetWorld(), Info);
            UFortMissionLibrary::SetWorldSavingEnabled(GetWorld(), true);
        }
    }
    else
    {
       
    }
    return Pawn;
}

void* AFortWorldManager::GetCurrentTheaterMapData()
{
    auto GameState = GetWorld()->GetGameState();
    auto WorldManager = GameState->GetWorldManager();
    auto WorldRecord = WorldManager->GetCurrentWorldRecord();

    WorldRecord->GetZoneInstanceInfo().ZoneThemeClass = StaticClassImpl("BP_ZT_TheOutpost_PvE_01_C");
    WorldRecord->GetZoneInstanceInfo().WorldId = L"Test";
    WorldRecord->GetZoneInstanceInfo().TheaterId = L"";
    
    return GetCurrentTheaterMapDataOG(this);
}

UClass** AFortGameModeAthena::GetGameSessionClass(AFortGameModeAthena*, UClass** Result)
{
    UE_LOG(LogTemp, Log, "GetGameSessionClass!");
    
    *Result = AFortGameSessionDedicated::StaticClass();
    return Result;
}
static inline DWORD WINAPI StartLGThread(LPVOID)
{
    auto GameState = GetWorld()->GetGameState();
    auto GameMode = GetWorld()->GetAuthorityGameMode();
    auto LocalAircraft = GameState->GetAircrafts()[0];

    while (GameState->GetGamePhase() == EAthenaGamePhase::Warmup)
    {
        Sleep(1000 / 30);
    }

    while (GameState->GetGamePhase() != EAthenaGamePhase::Aircraft)
    {
        Sleep(1000 / 30);
    }

    float time = UGameplayStatics::GetTimeSeconds(GetWorld());

    auto start = std::chrono::high_resolution_clock::now();

    while (GameState->GetGamePhase() == EAthenaGamePhase::Aircraft && time < LocalAircraft->GetDropEndTime() + 1) {
        Sleep(1000 / 30);

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = now - start;
        time += elapsed.count();
        start = now;
    }


    GameState->SetGamePhase(EAthenaGamePhase::SafeZones);
    GameState->OnRep_GamePhase(GameState->GetGamePhase());
    GameState->SetGamePhaseStep(EAthenaGamePhaseStep::StormForming);
    GameState->OnRep_GamePhase(GameState->GetGamePhase());
    return 0;
}

static float precision(float f, float places)
{
    float n = powf(10.0f, places);
    return round(f * n) / n;
}

inline FVector_NetQuantize100 Quantize100(FVector p)
{
    FVector_NetQuantize100 ret;
    ret.X = precision(p.X, 2);
    ret.Y = precision(p.Y, 2);
    ret.Z = precision(p.Z, 2);
    return ret;
}


void AFortGameModeAthena::StartAircraftPhase(char a2)
{
    

    if (GNeon->bProd) {
        static bool bFirst = false;
        if (!bFirst) {
            bFirst = true;

            auto itSession = std::find_if(args.begin(), args.end(), [](const std::wstring& s) {
                return s.rfind(L"-session=", 0) == 0;
                });

            if (itSession != args.end())
            {
                std::wstring sessionValue = itSession->substr(9);
                std::wstring path = L"/fortnite/api/game/v2/matchmakingservice/close/" + sessionValue;
                PostRequest(L"matchmaking-service.fluxfn.org", path);
            }
        }
    }
    
    GNeon->bLategame = GetAlivePlayers().Num() < 30 ? true : false;
    static auto PlaylistName = GetGameState()->GetCurrentPlaylistInfo().GetBasePlaylist()->GetPlaylistName().ToString().ToString();
    std::transform(PlaylistName.begin(), PlaylistName.end(), PlaylistName.begin(), ::tolower);
    if (!PlaylistName.contains("showdownalt"))
        GNeon->bLategame = false;

    if (GNeon->bLategame)
    {
        auto GameState = GetWorld()->GetGameState();
        GameState->SetCachedSafeZoneStartUp(ESafeZoneStartUp::StartsWithWarmUp);
        GameState->SetAirCraftBehavior(EAirCraftBehavior::FlyTowardFirstCircleCenter);
        StartAircraftPhaseOG(this, 0);
    }
    else
    {
        StartAircraftPhaseOG(this, a2);
    }
    
    

    if (GNeon->bCreative) return;
    
    if (GNeon->bLategame)
    {
        SetbSafeZoneActive(true);
        ((void* (*)(AFortGameModeAthena * GameMode, bool a))(__int64(GetModuleHandleW(0)) + 0xFB5580))(this, false);
        this->StartNewSafeZonePhase(-1);
        this->StartNewSafeZonePhase(-1);
        //this->StartNewSafeZonePhase(-1);
        //this->StartNewSafeZonePhase(-1);
        auto GameState = GetGameState();
        if (!GameState || GameState->GetAircrafts().Num() == 0)
            return;
        GameState->SetDefaultParachuteDeployTraceForGroundDistance(3500.0f);
        auto LocalAircraft = GameState->GetAircrafts()[0];

        const FVector ZoneCenterLocation = GetSafeZoneLocations()[3];//4);
        FVector LocationToStartAircraft1 = ZoneCenterLocation;
        LocationToStartAircraft1.Z += 25000;//35000;//17500;
        
        float FlightSpeed = 2500.0f;

        FVector CurrentLocation = LocationToStartAircraft1;
        FRotator CurrentRotation = UKismetMathLibrary::FindLookAtRotation(ZoneCenterLocation, GetSafeZoneLocations()[4]);//5));

        FVector ForwardVector = LocalAircraft->GetActorForwardVector();

        LocalAircraft->SetCameraInitialRotation(CurrentRotation);

        float Distance = 30000.0f;

        FVector LocationToStartAircraft = CurrentLocation - (ForwardVector * Distance);

        LocalAircraft->K2_TeleportTo(LocationToStartAircraft, CurrentRotation);

        FRotator CurrentRotationAfter = UKismetMathLibrary::FindLookAtRotation(LocalAircraft->K2_GetActorLocation(), ZoneCenterLocation);
        CurrentRotationAfter.Pitch = 0;

        LocalAircraft->K2_SetActorRotation(CurrentRotationAfter, true);
        FAircraftFlightInfo& FlightInfo = LocalAircraft->GetFlightInfo();
        FlightInfo.FlightSpeed = FlightSpeed;
        FlightInfo.FlightStartLocation = Quantize100(LocationToStartAircraft);
        FlightInfo.FlightStartRotation = CurrentRotationAfter;
        LocalAircraft->SetDropEndTime(LocalAircraft->GetDropEndTime() - (8 * 2 + 5));
        FlightInfo.TimeTillDropStart -= 3;
        LocalAircraft->SetDropStartTime(LocalAircraft->GetDropStartTime() - 3);
        FlightInfo.TimeTillDropEnd -= (8 * 2 + 5);
    }

    CreateThread(0, 0, StartLGThread, 0, 0, 0);
}

void AFortGameModeAthena::StartNewSafeZonePhase(int NewSafeZonePhase)
{
    AFortGameStateAthena* GameState = Cast<AFortGameStateAthena>(GetGameState());
    if ( !GameState ) return StartNewSafeZonePhaseOG(this, NewSafeZonePhase);
    
    static bool bZoneReversing = false;
    static bool bEnableReverseZone = false;
    static int32 NewLateGameSafeZonePhase = 1;
    static const int32 EndReverseZonePhase = 5;
    static const int32 StartReverseZonePhase = 7;


    
    
    static int Season = floor(stod(Fortnite_Version.ToString()));
    //if ( Season < 13 ) return StartNewSafeZonePhaseOG(this, NewSafeZonePhase);

    static auto ZoneDurationsOffset =
    Fortnite_Version >= 15.20 && Season < 18 ? 0x258 : Season >= 18 ? 0x248 : 0x1F8; // Season 13 - Season 14

    AFortSafeZoneIndicator* SafeZoneIndicator = GetSafeZoneIndicator();
    
    if ( !SafeZoneIndicator ) return StartNewSafeZonePhaseOG(this, NewSafeZonePhase);

    TArray<float> Durations = *(TArray<float>*)(__int64(&GameState->GetMapInfo()->GetSafeZoneDefinition()) + ZoneDurationsOffset);
    TArray<float> HoldDurations = *(TArray<float>*)(__int64(&GameState->GetMapInfo()->GetSafeZoneDefinition()) + ZoneDurationsOffset - 0x10);
    
    auto Sum = 0.f;
    for (auto& _v : Durations) Sum += _v;
    if (Sum == 0.f)
    {
        UCurveTable* GameData = nullptr;
        
        if (Fortnite_Version >= 6.10)
        {
            static int CurrentPlaylistInfoOffset = UKismetMemLibrary::GetOffset(GameState, "CurrentPlaylistInfo");
            FPlaylistPropertyArray& CurrentPlaylistInfoPtr = *reinterpret_cast<FPlaylistPropertyArray*>(__int64(GameState) + CurrentPlaylistInfoOffset);

            GameData = CurrentPlaylistInfoPtr.GetBasePlaylist()->GetGameData().Get();
        }
        else
        {
            GameData = GameState->GetCurrentPlaylistData()->GetGameData().Get();
        }

        if (!GameData)
            GameData = StaticFindObject<UCurveTable>("/Game/Balance/AthenaGameData.AthenaGameData");

        auto ShrinkTime = UKismetStringLibrary::Conv_StringToName(L"Default.SafeZone.ShrinkTime");
        auto HoldTime = UKismetStringLibrary::Conv_StringToName(L"Default.SafeZone.WaitTime");
        
        for (int i = 0; i < Durations.Num(); i++)
        {
            Durations[i] = UDataTableFunctionLibrary::EvaluateCurveTableRow(GameData, ShrinkTime, (float)i, FString(), nullptr);
        }
        for (int i = 0; i < HoldDurations.Num(); i++)
        {
            HoldDurations[i] = UDataTableFunctionLibrary::EvaluateCurveTableRow(GameData, HoldTime, (float)i, FString(), nullptr);
        }
    }
    if (GNeon->bLategame)
    {
        //bSetupLG = true;
        SetSafeZonePhase(NewLateGameSafeZonePhase);
        GameState->SetSafeZonePhase(NewLateGameSafeZonePhase);
        
        StartNewSafeZonePhaseOG(this, NewSafeZonePhase);
        if (NewLateGameSafeZonePhase == EndReverseZonePhase) bZoneReversing = false;
        if (NewLateGameSafeZonePhase >= StartReverseZonePhase) bZoneReversing = false;
        //GameState->SetSafeZonesStartTime(0.0001f);
        NewLateGameSafeZonePhase = (bZoneReversing && bEnableReverseZone) ? NewLateGameSafeZonePhase - 1 : NewLateGameSafeZonePhase + 1;
    }
    else {
        StartNewSafeZonePhaseOG(this, NewSafeZonePhase);
    }

    float ZoneHoldDuration = (GetSafeZonePhase() >= 0 && GetSafeZonePhase() < HoldDurations.Num())
        ? HoldDurations[(GetSafeZonePhase())] : 0.0f;

    SafeZoneIndicator->SetSafeZoneStartShrinkTime(UGameplayStatics::GetTimeSeconds(GetWorld()) + ZoneHoldDuration);

    float ZoneDuration = (GetSafeZonePhase() >= 0 && GetSafeZonePhase() < Durations.Num())
        ? Durations[GetSafeZonePhase()] : 0.0f;

    SafeZoneIndicator->SetSafeZoneFinishShrinkTime(SafeZoneIndicator->GetSafeZoneStartShrinkTime() + ZoneDuration);

    if (GNeon->bLategame)
    {
        if (GetSafeZonePhase() == 3)
        {
            const float FixedInitialZoneSize = 5000.0f;
            SafeZoneIndicator->SetRadius(FixedInitialZoneSize);
        }

        if (GetSafeZonePhase() == 2 || GetSafeZonePhase() == 3)
        {
            if (SafeZoneIndicator)
            {
                SafeZoneIndicator->SetSafeZoneStartShrinkTime(UGameplayStatics::GetTimeSeconds(GetWorld()));
                SafeZoneIndicator->SetSafeZoneFinishShrinkTime(UGameplayStatics::GetTimeSeconds(GetWorld()) + 0.2);
            }

            ExecuteConsoleCommand(GetWorld(), L"skipsafezone", nullptr);
        }
    }

    
    
}

EFortTeam AFortGameModeAthena::PickTeam(uint8_t PreferredTeam, AFortPlayerControllerAthena* Controller)
{
    static uint8_t CurrentTeam = 3;
    std::string PlayerName = Controller->GetPlayerState()->GetPlayerName().ToString();
  
    static std::string TeamsJson = "";
    static std::chrono::steady_clock::time_point LastFetchTime = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    static auto itSession = std::find_if(args.begin(), args.end(), [](const std::wstring& s) {
        return s.rfind(L"-session=", 0) == 0;
    })->substr(9);
  
    auto Now = std::chrono::steady_clock::now();
    if (TeamsJson.empty() || std::chrono::duration_cast<std::chrono::seconds>(Now - LastFetchTime).count() >= 5) {
        std::wstring path = L"/fortnite/api/game/v2/matchmakingservice/teams/" + itSession;
        TeamsJson = GetRequest(L"matchmaking-service.fluxfn.org", path);
        LastFetchTime = Now;
    }
  
    if (!TeamsJson.empty()) {
        try {
            auto teamsArray = json::parse(TeamsJson);
            static std::map<size_t, uint8_t> teamIndexToGameTeam;
            static std::unordered_map<std::string, uint8_t> playerToTeam;
  
            if (playerToTeam.find(PlayerName) != playerToTeam.end())
                return EFortTeam(playerToTeam[PlayerName]);
  
            for (size_t teamIndex = 0; teamIndex < teamsArray.size(); ++teamIndex) {
                const auto& team = teamsArray[teamIndex];
                for (const auto& member : team) {
                    std::string memberName = member.get<std::string>();
                    if (memberName == PlayerName) {
                        uint8_t assignedTeam;
                        if (teamIndexToGameTeam.find(teamIndex) != teamIndexToGameTeam.end()) {
                            assignedTeam = teamIndexToGameTeam[teamIndex];
                        } else {
                            assignedTeam = CurrentTeam;
                            teamIndexToGameTeam[teamIndex] = assignedTeam;
                            CurrentTeam++;
                        }
                        playerToTeam[PlayerName] = assignedTeam;
                        return EFortTeam(assignedTeam);
                    }
                }
            }
        } catch (const std::exception& e) {
            UE_LOG(LogTemp, Error, "FOUND INVALID PLAYER! KICKING!!");
            Controller->CallFunc<void>("PlayerController", "ClientReturnToMainMenu");
        }
    }

    UE_LOG(LogTemp, Error, "FOUND INVALID PLAYER! KICKING!!");
    Controller->CallFunc<void>("PlayerController", "ClientReturnToMainMenu");

    if (!GetWorld()) return EFortTeam(0);
    
    static uint8_t Next = 3;
    uint8_t Teams = Next;

    static uint8_t Players = 0;

    auto GameState = Cast<AFortGameStateAthena>(GetWorld()->GetGameState());
    if (!GameState || !GameState->GetCurrentPlaylistInfo().GetBasePlaylist()) return EFortTeam(0);
    
    Players++;

    if (Players >= GameState->GetCurrentPlaylistInfo().GetBasePlaylist()->GetMaxSquadSize())
    {
        Next++;
        Players = 0;
    }

    return EFortTeam(Teams);
}