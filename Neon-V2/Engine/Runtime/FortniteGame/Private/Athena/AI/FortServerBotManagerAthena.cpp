#include "pch.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/AI/FortServerBotManagerAthena.h"

#include "Engine/Runtime/Engine/Classes/GameplayStatics.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/AI/FortAthenaAIBotController.h"

void UFortServerBotManagerAthena::SpawnAI()
{
    static bool bCalled = false;
    if (!bCalled) bCalled = true; else return;
    
    auto AISystem = GetWorld()->GetAISystem();

    AISystem->SpawnAI("Deadfire", "15");
    AISystem->SpawnAI("Guide", "15");
    AISystem->SpawnAI("Cosmos", "15");
    AISystem->SpawnAI("Gladiator", "15");
    AISystem->SpawnAI("Outlaw", "15");
    AISystem->SpawnAI("Nightmare", "15");
    AISystem->SpawnAI("Dummy", "15");
    AISystem->SpawnAI("Splode", "15");
    AISystem->SpawnAI("Remedy", "15");
    AISystem->SpawnAI("Doggo", "15");
    AISystem->SpawnAI("Fishstick", "15");
    AISystem->SpawnAI("Blaze", "15");
    AISystem->SpawnAI("Bullseye", "15");
    AISystem->SpawnAI("Burnout", "15");
    AISystem->SpawnAI("TomatoHead", "15");
    AISystem->SpawnAI("Ragnarok", "15");
    AISystem->SpawnAI("Outcast", "15");
    AISystem->SpawnAI("Bigfoot", "15");
    AISystem->SpawnAI("BeefBoss", "15");
    AISystem->SpawnAI("Kyle", "15");
    AISystem->SpawnAI("BigChuggus", "15");
    AISystem->SpawnAI("Bandolier", "15");
    AISystem->SpawnAI("RuckusH", "15");
    AISystem->SpawnAI("FutureSamurai", "15");
    AISystem->SpawnAI("Longshot", "15");
    AISystem->SpawnAI("Turk", "15");
    AISystem->SpawnAI("Cole", "15");
    AISystem->SpawnAI("TheReaper", "15");
    AISystem->SpawnAI("Triggerfish", "15");
    AISystem->SpawnAI("Grimbles", "15");
    AISystem->SpawnAI("Sleuth", "15");
    AISystem->SpawnAI("Bushranger", "15");
    AISystem->SpawnAI("BunkerJonesy", "15");
    AISystem->SpawnAI("Kit", "15");
    AISystem->SpawnAI("Shapeshifter", "15");
    AISystem->SpawnAI("Brutus", "15");
    AISystem->SpawnAI("WeaponsExpert", "15");
    AISystem->SpawnAI("FarmerSteel", "15");
    AISystem->SpawnAI("Sunflower", "15");

    auto PlayerStarts = UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFortPlayerStartWarmup::StaticClass());
    const int32 MaxBots = Fortnite_Version == 12.41 ? 100 : 50;
    const int32 SpawnCount = FMath::Min<int32>(MaxBots, PlayerStarts.Num());

    for (int32 i = 0; i < SpawnCount; i++)
    {
        auto Start = PlayerStarts[i];
        
        FTransform Transform{};
        Transform.Translation = Start->K2_GetActorLocation();
        Transform.Rotation = FQuat();
        Transform.Scale3D = FVector{ 1,1,1 };

        static auto PhoebeSpawnerData = StaticLoadObject<UClass>("/Game/Athena/AI/Phoebe/BP_AISpawnerData_Phoebe.BP_AISpawnerData_Phoebe_C");
        AISystem->GetAISpawner()->RequestSpawn(UFortAthenaAISpawnerData::CreateComponentListFromClass(PhoebeSpawnerData, GetWorld()), Transform);
    }
}

void UFortServerBotManagerAthena::Init()
{
    auto GameMode = GetWorld()->GetAuthorityGameMode();
    auto GameState = GetWorld()->GetGameState();

    auto BotManager = (UFortServerBotManagerAthena*)UGameplayStatics::SpawnObject(
            UFortServerBotManagerAthena::StaticClass(), GameMode);
    
    if (BotManager)
    {
        BotManager->SetCachedGameMode(GameMode);
        BotManager->SetCachedGameState(GameState);
        *(bool*)(__int64(BotManager) + 0x458) = true;
        GameMode->SetServerBotManager(BotManager);
    }

    GameMode->SetServerBotManagerClass(UFortServerBotManagerAthena::StaticClass());

    auto AIDirector = GetWorld()->SpawnActor<AFortAIDirector>(AFortAIDirector::StaticClass(), {});
    GameMode->SetAIDirector(AIDirector);
    if (GameMode->GetAIDirector()) GameMode->GetAIDirector()->CallFunc<void>("FortAIDirector", "Activate");

    auto AIGoalManager = GetWorld()->SpawnActor<AFortAIGoalManager>(AFortAIGoalManager::StaticClass(), {});
    GameMode->SetAIGoalManager(AIGoalManager);

    if (Fortnite_Version >= 14.60)
    {
        BotManager->SetCachedAIPopulationTracker(GetWorld()->GetAISystem()->Get<UAthenaAIPopulationTracker*>("AthenaAISystem", "AIPopulationTracker"));
        if (!GameMode->GetSpawningPolicyManager())
        {
            GameMode->SetSpawningPolicyManager((AFortAthenaSpawningPolicyManager*)GetWorld()->SpawnActor(AFortAthenaSpawningPolicyManager::StaticClass(), {}));
            GameMode->GetSpawningPolicyManager()->SetGameStateAthena(GameState);
            GameMode->GetSpawningPolicyManager()->SetGameModeAthena(GameMode);
        }
    }
}

AFortPlayerPawn* UFortServerBotManagerAthena::SpawnBot(FVector SpawnLoc, FRotator SpawnRot, UFortAthenaAIBotCustomizationData* BotData, FFortAthenaAIBotRunTimeCustomizationData& RuntimeBotData)
{
   /* if (__int64(_ReturnAddress()) == UKismetMemLibrary::Get<uintptr_t>(TEXT("SpawnBotRet"))) {
        return SpawnBotOG(this, SpawnLoc, SpawnRot, BotData, RuntimeBotData);
    }
    
   AActor* SpawnLocator = GetWorld()->SpawnActor<APawn>(APawn::StaticClass(), SpawnLoc, SpawnRot);
    AFortPlayerPawn* Ret = GetCachedBotMutator()->SpawnBot(BotData->GetPawnClass(), SpawnLocator, SpawnLoc, SpawnRot, true);
    SpawnLocator->K2_DestroyActor();
    */
    AFortPlayerPawn* Ret = (AFortPlayerPawn*)GetWorld()->SpawnActor(BotData->GetPawnClass(), SpawnLoc, SpawnRot);

    if (Ret)
    {
        AFortAthenaAIBotController* Controller = Cast<AFortAthenaAIBotController>(Ret->GetController());
        static int Season = floor(stod(Fortnite_Version.ToString()));

        if (BotData->GetCharacterCustomization())
        {
            if (&BotData->GetCharacterCustomization()->GetCustomizationLoadout())
            {
                if (Season == 12 && Fortnite_Version >= 12.41) {
                    if (BotData->GetFName().ToString().ToString().contains("MANG_POI_Yacht"))
                    {
                        BotData = StaticLoadObject<UFortAthenaAIBotCustomizationData>("/Game/Athena/AI/MANG/BotData/BotData_MANG_POI_HDP.BotData_MANG_POI_HDP");
                    }
                }

                if (BotData->GetCharacterCustomization()->GetCustomizationLoadout().GetCharacter()->GetFName().ToString().ToString() == "CID_556_Athena_Commando_F_RebirthDefaultA")
                {
                    std::string Tag = RuntimeBotData.PredefinedCosmeticSetTag.TagName.ToString().ToString();
                    if (Tag == "Athena.Faction.Alter") {
                        BotData->GetCharacterCustomization()->GetCustomizationLoadout().SetCharacter(StaticLoadObject<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_NPC_Athena_Commando_M_HenchmanBad.CID_NPC_Athena_Commando_M_HenchmanBad"));
                    }
                    else if (Tag == "Athena.Faction.Ego") {
                        BotData->GetCharacterCustomization()->GetCustomizationLoadout().SetCharacter(StaticLoadObject<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_NPC_Athena_Commando_M_HenchmanGood.CID_NPC_Athena_Commando_M_HenchmanGood"));
                    }
                    else if (Tag.contains("Box")) {
                        BotData->GetCharacterCustomization()->GetCustomizationLoadout().SetCharacter(StaticLoadObject<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_NPC_Athena_Commando_M_Scrapyard.CID_NPC_Athena_Commando_M_Scrapyard"));
                    }
                }
            }
        }

        Controller->Set("FortAthenaAIBotController", "CosmeticLoadoutBC", BotData->GetCharacterCustomization()->GetCustomizationLoadout());
        if (BotData->GetCharacterCustomization()->GetCustomizationLoadout().GetCharacter()->GetHeroDefinition())
        {
            for (int32 i = 0; i < BotData->GetCharacterCustomization()->GetCustomizationLoadout().GetCharacter()->GetHeroDefinition()->GetSpecializations().Num(); i++)
            {
                UFortHeroSpecialization* Spec = StaticLoadObject<UFortHeroSpecialization>(UKismetStringLibrary::Conv_NameToString(BotData->GetCharacterCustomization()->GetCustomizationLoadout().GetCharacter()->GetHeroDefinition()->GetSpecializations()[i].SoftObjectPtr.ObjectID.AssetPathName).ToString());

                if (Spec)
                {
                    for (int32 j = 0; j < Spec->GetCharacterParts().Num(); j++)
                    {
                        UCustomCharacterPart* Part = StaticLoadObject<UCustomCharacterPart>(UKismetStringLibrary::Conv_NameToString(Spec->GetCharacterParts()[j].SoftObjectPtr.ObjectID.AssetPathName).ToString());
                        auto PartDef = Part;
                        Ret->CallFunc<void>("FortPlayerPawn", "ServerChoosePart", PartDef->GetCharacterPartType() , PartDef);
                    }
                }
            }
        }
                
        Ret->SetCosmeticLoadout(&BotData->GetCharacterCustomization()->GetCustomizationLoadout());
        Ret->OnRep_CosmeticLoadout();
        
        UBlackboardComponent* Blackboard = Controller->GetBlackboard();
        Controller->UseBlackboard(Controller->GetBehaviorTree()->GetBlackboardAsset(), &Blackboard);
        Controller->OnUsingBlackBoard(Blackboard, Controller->GetBehaviorTree()->GetBlackboardAsset());

        if (BotData->GetBehaviorTree())
        {
            Controller->SetBehaviorTree(BotData->GetBehaviorTree());
            if (Controller->RunBehaviorTree(BotData->GetBehaviorTree())) {
                Controller->BlueprintOnBehaviorTreeStarted();
                Controller->GetBrainComponent()->RestartLogic();
            }
        } 
        
        AFortInventory* Inventory = Controller->GetInventory();
        if (!Inventory) {
            Inventory = GetWorld()->SpawnActor<AFortInventory>(AFortInventory::StaticClass(), {}, {}, Controller);
            Controller->SetInventory(Inventory);
        }
            
        if (BotData->GetStartupInventory() && Inventory)
        {
            for (int32 i = 0; i < BotData->GetStartupInventory()->GetItems().Num(); i++) {
                UFortItemDefinition* ItemDef = BotData->GetStartupInventory()->GetItems()[i];
                if (!ItemDef || !IsValidPointer(ItemDef)) continue;
                UFortWorldItem* Item = (UFortWorldItem*)Inventory->GiveItem(ItemDef, 1, 30, 0);
                    
                if (ItemDef->IsA(UFortWeaponItemDefinition::StaticClass()) && ((UFortWorldItemDefinition*)ItemDef)->GetbCanBeDropped()) {
                    Ret->EquipWeaponDefinition(Item->GetItemEntry().GetItemDefinition(), Item->GetItemEntry().GetItemGuid());
                    Controller->GetCacheAimingDigestedSkillSet()->SetCachedWeaponUsedToCalculateAccuracy(Ret->GetCurrentWeapon());
                }
            }
        }
        
        if (Fortnite_Version == 13.40)
        {
            using FT = void (*)(UFortServerBotManagerAthena *, AFortPlayerPawn *, UBehaviorTree *, void *, FConstructionBuildingInfo(*)[6], float *, void *, UFortAthenaAIBotInventoryItems *, UFortBotNameSettings *, FString *, BYTE *, uint8, __int64 a4, UObject *, FFortAthenaAIBotRunTimeCustomizationData, UFortAthenaAIBotCustomizationData *);
            FT BotManagerSetup = UKismetMemLibrary::Get<FT>(TEXT("BotManagerSetup"));
        
            BYTE FalseByte = 0;

            using GetOrCreateLODDataT = UObject* (*)(UFortAthenaAIBotCustomizationData*);
            auto GetOrCreateLODData = (GetOrCreateLODDataT)(IMAGEBASE + 0x1DBEC00);
            DWORD CustomSquadId = RuntimeBotData.CustomSquadId;
            BotManagerSetup(
                this,
                Ret,
                BotData->GetBehaviorTree(),
                nullptr,
                reinterpret_cast<FConstructionBuildingInfo (*)[6]>(&this->GetConstructionBuildingInfo()),
                &BotData->GetSkillLevel(),
                nullptr,
                    BotData->GetStartupInventory(),
                    BotData->GetBotNameSettings(),
                    nullptr,
                &FalseByte,
                1,
                CustomSquadId,
                GetOrCreateLODData(BotData),
                RuntimeBotData,
                BotData
            );
            
        } else
        {
            static void (*BotManagerSetup)(__int64 BotManaager, __int64 Pawn, __int64 BehaviorTree, __int64 a4, DWORD* SkillLevel, __int64 idk, __int64 StartupInventory, __int64 BotNameSettings, __int64 idk_1, BYTE* CanRespawnOnDeath, unsigned __int8 BitFieldDataThing, BYTE* CustomSquadId, FFortAthenaAIBotRunTimeCustomizationData InRuntimeBotData) = decltype(BotManagerSetup)(UKismetMemLibrary::GetAddress<uintptr_t>(TEXT("BotManagerSetup")));
            DWORD CustomSquadId = RuntimeBotData.CustomSquadId;
            BYTE TrueByte = 1;
            BYTE FalseByte = 0;
     //       BotManagerSetup(__int64(this), __int64(Ret), __int64(BotData->GetBehaviorTree()), 0, &CustomSquadId, 0, __int64(BotData->GetStartupInventory()), __int64(BotData->GetBotNameSettings()), 0, &FalseByte, 0, &TrueByte, RuntimeBotData);
        }
    }

    return Ret;
}