#include "pch.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/Player/FortPlayerControllerAthena.h"

#include "Engine/Plugins/Kismet/Public/FortKismetLibrary.h"
#include "Engine/Plugins/Neon/Public/Neon.h"
#include "Engine/Runtime/Engine/Classes/World.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/AI/FortAthenaAIBotController.h"
#include "Engine/Runtime/FortniteGame/Public/ItemDefinitions/FortConsumableItemDefinition.h"
#include "Engine/Runtime/FortniteGame/Public/ItemDefinitions/WeaponRangedItemDefinition.h"
#include "Engine/Runtime/FortniteGame/Public/Player/FortPlayerState.h"
#include "Engine/Runtime/FortniteGame/Public/Projectiles/FortProjectileBase.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/Player/FortPlayerControllerAthenaXPComponent.h"
#include "Engine/Runtime/FortniteGame/Public/Quests/FortQuestManager.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/AI/FortServerBotManagerAthena.h"
#include "Engine/Runtime/FortniteGame/Public/Creative/FortMinigame.h"
#include "Engine/Runtime/FortniteGame/Public/ItemDefinitions/AthenaEmojiItemDefinition.h"
#include <Engine/Runtime/FortniteGame/Public/Components/FortMutatorListComponent.h>
#include <Engine/Runtime/FortniteGame/Public/Mutators/FortAthenaMutator_HealthAndShield.h>
#include <Engine/Runtime/FortniteGame/Public/Mutators/FortAthenaMutator_SiphonValues.h>

#include "Engine/Runtime/FortniteGame/Public/STW/FortGameModeOutpost.h"
static std::unordered_map<AFortPlayerControllerAthena*, std::chrono::steady_clock::time_point> TimeMap;

void AFortPlayerControllerAthena::ServerAcknowledgePossession(FFrame& Stack)
{
    APawn* PawnToAcknowledge;
    Stack.StepCompiledIn(&PawnToAcknowledge);
    Stack.IncrementCode();
    
    SetAcknowledgedPawn(PawnToAcknowledge);
    AFortPlayerStateAthena* PlayerStateAthena = Cast<AFortPlayerStateAthena>(GetPlayerState());
	
	if (PlayerStateAthena)
	{
		PlayerStateAthena->SetHeroType(Fortnite_Version >= 8.50 ? GetCosmeticLoadoutPC().GetCharacter()->GetHeroDefinition() : GetCustomizationLoadout().GetCharacter()->GetHeroDefinition());

		auto Pickaxe = (Fortnite_Version >= 8.50 ? GetCosmeticLoadoutPC() : GetCustomizationLoadout()).GetPickaxe();
		auto WeaponDef = Pickaxe->GetWeaponDefinition();
		if ( GetWorldInventory() )
			GetWorldInventory()->GiveItem(WeaponDef, 1, 0, 1);

		TimeMap[this] = std::chrono::steady_clock::now(); // for time alive

                UFortServerBotManagerAthena::SpawnAI();

                if (Fortnite_Version >= 11.00)
                {
                        GetXPComponent()->Set(
                                "FortPlayerControllerAthenaXPComponent", "bRegisteredWithQuestManager", true
                        );
                        GetXPComponent()->OnRep_bRegisteredWithQuestManager();
                        GetQuestManager(ESubGame::Athena)->InitializeQuestAbilities(GetMyFortPawn());
                }
	}

	if (Fortnite_Version >= 11.00)
		UFortKismetLibrary::UpdatePlayerCustomCharacterPartsVisualization(PlayerStateAthena);
	else
	{
		UKismetMemLibrary::Get<void(*)(APlayerState*, APawn*)>(L"ApplyCharacterCustomization")(GetPlayerState(), PawnToAcknowledge);
	}

	/*if (GNeon->bProd)
	{
		std::thread([=]()
		{
			auto name = PlayerStateAthena->GetPlayerName().ToWideString();
			std::wstring Path = L"/beryllium/api/v1/status/" + name;
			
			auto Res = GetRequest(L"158.220.100.45:27192", Path);

			if (Res != "Valid")
			{
				CallFunc<void>("PlayerController", "ClientReturnToMainMenu", FString());
			}
		}).detach();
	}*/
	
	if (GetWorld()->GetGameState()->GetGamePhase() != EAthenaGamePhase::Warmup && GNeon->bLategame)
	{
		GetMyFortPawn()->SetShield(100);
	} else if (GetWorld()->GetGameState()->GetGamePhase() == EAthenaGamePhase::Warmup)
	{
		auto GameMode = GetWorld()->GetAuthorityGameMode();
		auto Player = GameMode->GetAlivePlayers().Search([&](AFortPlayerControllerAthena* Player) { return Player == this; });
		if (!Player)
		{
			GameMode->GetAlivePlayers().Add(this);
		}
	}
}

void AFortPlayerControllerAthena::ServerTeleportToPlaygroundLobbyIsland(FFrame& Stack)
{
	Stack.IncrementCode();

	static UFortMissionLibrary* MissionLib = (UFortMissionLibrary*)UFortMissionLibrary::GetDefaultObj();
	
	if (GetWarmupPlayerStart())
	{
		auto Location = GetWarmupPlayerStart()->K2_GetActorLocation();
		auto Rotation = GetWarmupPlayerStart()->K2_GetActorRotation();
		MissionLib->CallFunc<void>("FortMissionLibrary", "TeleportPlayerPawn", GetMyFortPawn(), Location, Rotation, true, true);
	}
	else {
		AActor* Actor = GetWorld()->GetAuthorityGameMode()->CallFunc<AActor*>("GameModeBase", "ChoosePlayerStart", this);
		auto Location = Actor->K2_GetActorLocation();
		auto Rotation = Actor->K2_GetActorRotation();
		MissionLib->CallFunc<void>("FortMissionLibrary", "TeleportPlayerPawn", GetMyFortPawn(), Location, Rotation, true, true);
	}
}

void AFortPlayerControllerAthena::ServerClientIsReadyToRespawn(FFrame& Stack)
{
	Stack.IncrementCode();
	
	auto GameMode = GetWorld()->GetAuthorityGameMode();
	auto GameState = GameMode->GetGameState();
	auto PlayerState = Cast<AFortPlayerStateAthena>(GetPlayerState());
	auto PC = Cast<AFortPlayerControllerAthena>(PlayerState->GetOwner());
	AFortPawn* OldPawn = nullptr;
	if (PC) OldPawn = PC->GetMyFortPawn();
	PlayerState->GetRespawnData().bRespawnDataAvailable = true;
	PlayerState->GetRespawnData().bServerIsReady = true;
	if (GameState->IsRespawningAllowed(PlayerState) || GNeon->bCreative)
	{
		if (PlayerState->GetRespawnData().bServerIsReady && PlayerState->GetRespawnData().bRespawnDataAvailable)
		{
			PlayerState->GetRespawnData().bClientIsReady = true;
				
			FTransform SpawnTransform{};
			SpawnTransform.Translation = PlayerState->GetRespawnData().RespawnLocation;
			SpawnTransform.Rotation = FRotator(
				PlayerState->GetRespawnData().RespawnRotation.Pitch, 
				PlayerState->GetRespawnData().RespawnRotation.Yaw, 
				PlayerState->GetRespawnData().RespawnRotation.Roll
			).Quaternion();
			
			SpawnTransform.Scale3D = FVector(1, 1, 1);

			/*if (GNeon->bCreative)
			{
				AFortMinigame* MiniGame = CallFunc<AFortMinigame*>("FortPlayerControllerAthena", "GetMinigame");
				if (MiniGame)
				{
					AActor* PlayerStart = MiniGame->CallFunc<AActor*>("FortMinigame", "ChoosePlayerStart", this);
					if (PlayerStart)
						SpawnTransform = PlayerStart->GetTransform();
				}
			}*/

			AFortPlayerPawn* Pawn = (AFortPlayerPawn*)GameMode->CallFunc<APawn*>("GameModeBase", "SpawnDefaultPawnAtTransform", this, SpawnTransform); 
			if (!Pawn) return;

			CallFunc<void>("Controller", "Possess", Pawn);
			auto vol = PC->GetCurrentVolume();
			if (!vol) return;
			auto cmpnt = (UFortMutatorListComponent*)vol->GetComponentByClass(UFortMutatorListComponent::StaticClass());
			auto muts = cmpnt->GetMutators();
			static auto HealthAndShieldMut = AFortAthenaMutator_HealthAndShield::StaticClass();
			for (int32 i = 0; i < muts.Num(); i++)
			{
				auto mut = muts[i];
				if (mut->IsA(HealthAndShieldMut))
				{
					auto castmut = Cast<AFortAthenaMutator_HealthAndShield>(mut);

					float MaxHealth = castmut->GetMaxHealth();
					float MaxShield = castmut->GetMaxShield();
					float StartingHealth = MaxHealth * (castmut->GetStartingHealth() / 100);
					float StartingShield = MaxShield * (castmut->GetStartingShield() / 100);

					Pawn->SetMaxHealth(MaxHealth);
					Pawn->SetHealth(StartingHealth);
					Pawn->SetMaxShield(MaxShield);
					Pawn->SetShield(StartingShield);
					break;
				}

			}
			
			CallFunc<void>("FortPlayerControllerAthena", "RespawnPlayerAfterDeath", true);
		}
	}
	if (OldPawn)
	{
		OldPawn->K2_DestroyActor();
	}
}

void AFortPlayerControllerAthena::ServerGiveCreativeItem(FFrame& Stack)
{
	static int Size = StaticClassImpl("FortItemEntry")->GetSize();
    void* Mem = malloc(Size);
    if (!Mem || !IsValidLowLevel()) return;
	
    memset(Mem, 0, Size);
    Stack.StepCompiledIn(Mem);

    auto CreativeItemPtr = reinterpret_cast<FFortItemEntry*>(Mem);
    auto ItemDef = CreativeItemPtr->GetItemDefinition();
    auto Count = CreativeItemPtr->GetCount() == 0 ? 1 : CreativeItemPtr->GetCount();
    
    if (!ItemDef)
    {
       free(Mem);
       return;
    }

    AFortInventory* WorldInventory = GetWorldInventory();
    if (!WorldInventory)
    {
        free(Mem);
        return;
    }

    TArray<UFortWorldItem*>& ItemInstances = WorldInventory->GetInventory().GetItemInstances();
            
    FFortItemEntry* ItemEntry = nullptr;
    for (UFortWorldItem* Item : ItemInstances) 
    {
        if (!Item)
            continue;
            
        auto ItemEntryDef = Item->GetItemEntry().GetItemDefinition();
        if (!ItemEntryDef)
            continue;
            
        if (ItemEntryDef->IsA(UFortResourceItemDefinition::StaticClass()) || 
            ItemEntryDef->IsA(UFortAmmoItemDefinition::StaticClass())) 
        {
        	if (ItemEntryDef == ItemDef)
        	{
        		ItemEntry = &Item->GetItemEntry();
        		break;
        	}
        }
    }

    if (ItemEntry)
    {
        ItemEntry->SetCount(ItemEntry->GetCount() + Count);
        WorldInventory->ReplaceEntry(*ItemEntry);
        free(Mem);
        return;
    }

    int32 LoadedAmmo = 0;
    if (ItemDef->IsA<UFortWeaponItemDefinition>())
    {
        auto WeaponStats = WorldInventory->GetStats((UFortWeaponItemDefinition*)ItemDef);
        if (WeaponStats)
        {
            LoadedAmmo = WeaponStats->GetClipSize();
        }
        else
        {
            LoadedAmmo = 1;
        }
    }
    else
    {
        LoadedAmmo = CreativeItemPtr->GetLoadedAmmo() == 0 ? 1 : CreativeItemPtr->GetLoadedAmmo();
    }

    WorldInventory->GiveItem(ItemDef, Count, LoadedAmmo, 1);
    free(Mem);
}

void AFortPlayerControllerAthena::ServerPlaySquadQuickChatMessage(__int64 ChatEntry, __int64 SenderID)
{
	AFortPlayerStateAthena* PlayerStateAthena = Cast<AFortPlayerStateAthena>(GetPlayerState());
	if (!PlayerStateAthena) return;
	
	ETeamMemberState TeamMemberState = ETeamMemberState::None;

	switch (ChatEntry)
	{
	case 0:
		TeamMemberState = ETeamMemberState::EnemySpotted;
		break;
	case 1:
		TeamMemberState = ETeamMemberState::NeedBandages;
		break;
	case 2:
		TeamMemberState = ETeamMemberState::NeedShields;
		break;
	case 3:
		TeamMemberState = ETeamMemberState::NeedMaterials;
		break;
	case 4:
		TeamMemberState = ETeamMemberState::NeedWeapon;
		break;
	case 5:
		TeamMemberState = ETeamMemberState::NeedAmmoLight;
		break;
	case 6:
		TeamMemberState = ETeamMemberState::NeedAmmoMedium;
		break;
	case 7:
		TeamMemberState = ETeamMemberState::NeedAmmoHeavy;
		break;
	case 8:
		TeamMemberState = ETeamMemberState::NeedAmmoShells;
		break;
	case 9:
		TeamMemberState = ETeamMemberState::NeedAmmoRocket;
		break;
	default:
		break;
	}

	PlayerStateAthena->SetReplicatedTeamMemberState(TeamMemberState);
	PlayerStateAthena->SetTeamMemberState(TeamMemberState);

	static auto EmojiComm = StaticFindObject<UAthenaEmojiItemDefinition>("/Game/Athena/Items/Cosmetics/Dances/Emoji/Emoji_Comm.Emoji_Comm");
	CallFunc<void>("FortPlayerController", "ServerPlayEmoteItem", EmojiComm);
	PlayerStateAthena->OnRep_ReplicatedTeamMemberState();
}

bool AFortPlayerControllerAthena::ServerRemoveInventoryItem(FGuid ItemGuid, int Count, bool bForceRemoveFromQuickBars, bool bForceRemoval)
{
	auto WorldInventory = GetWorldInventory();
	FFortItemList& Inventory = WorldInventory->GetInventory();
	TArray<FFortItemEntry>& ReplicatedEntries = Inventory.GetReplicatedEntries();

	static int StructSize = StaticClassImpl("FortItemEntry")->GetSize();
	auto ItemEntry = ReplicatedEntries.Search([&](FFortItemEntry& entry) { return entry.GetItemGuid() == ItemGuid; }, StructSize);
	if (!ItemEntry) return false;

	ItemEntry->SetCount(ItemEntry->GetCount() - Count);

	if (ItemEntry->GetCount() <= 0) {
		WorldInventory->Remove(ItemEntry->GetItemGuid());
	}
	else {
		WorldInventory->ReplaceEntry(*ItemEntry);
	}

	return true;
}

class UAbilitySystemBlueprintLibrary
{
public:
	DECLARE_DEFAULT_OBJECT(UAbilitySystemBlueprintLibrary);
	DECLARE_STATIC_CLASS(UAbilitySystemBlueprintLibrary);
};

void AFortPlayerControllerAthena::ClientOnPawnDied(FFortPlayerDeathReport& DeathReport)
{
	auto GameMode = GetWorld()->GetAuthorityGameMode();
	auto GameState = GetWorld()->GetGameState();
	auto PlayerState = (AFortPlayerStateAthena*)GetPlayerState();
	auto KillerPlayerState = (AFortPlayerStateAthena*)DeathReport.KillerPlayerState;
	auto KillerPawn = DeathReport.KillerPawn;
	auto VictimPawn = GetMyFortPawn();
	bool bRespawningAllowed = GameState && PlayerState ? GameState->IsRespawningAllowed(PlayerState) : false;
	if (bRespawningAllowed) return ClientOnPawnDiedOG(this, DeathReport);
	
	FVector DeathLocation = VictimPawn ? VictimPawn->K2_GetActorLocation() : FVector(0,0,0);

	if (!KillerPlayerState && VictimPawn)
		KillerPlayerState = (AFortPlayerStateAthena*)((AFortPlayerControllerAthena*)VictimPawn->GetController())->GetPlayerState();

	FDeathInfo* DeathInfo = &PlayerState->GetDeathInfo(); 
	auto MatchReport = GetMatchReport();
	FAthenaRewardResult* RewardResult = MatchReport ? &MatchReport->GetEndOfMatchResults() : nullptr;
	if (!RewardResult) RewardResult = new FAthenaRewardResult();
	FAthenaMatchStats* MatchStats = MatchReport ? &MatchReport->GetMatchStats() : nullptr;
	FAthenaMatchTeamStats* TeamStats = MatchReport ? &MatchReport->GetTeamStats() : nullptr;
	int32 KillScore = PlayerState->GetKillScore();

	auto DeathTags = DeathReport.Tags;
	uint8 DeathCause = PlayerState->ToDeathCause(DeathTags, false);

	PlayerState->Set("FortPlayerState", "PawnDeathLocation", DeathLocation);
	
	if (DeathInfo) {
		DeathInfo->SetbDBNO(false);
		DeathInfo->SetDeathLocation(DeathLocation);
		if (Fortnite_Version >= 6.00) DeathInfo->SetDeathTags(DeathTags);
		DeathInfo->SetDeathCause(DeathCause);
		DeathInfo->SetFinisherOrDowner((AActor*)KillerPlayerState);

		if (VictimPawn) {
			DeathInfo->GetDistance() = (DeathCause != 1) 
			? KillerPawn ? KillerPawn->GetDistanceTo(VictimPawn) : 0.0f
			: VictimPawn->Get<float>("FortPlayerPawnAthena", "LastFallDistance");
		}

		DeathInfo->SetbInitialized(true);
		PlayerState->SetDeathInfo(*DeathInfo);
	}

	static auto Wood = StaticFindObject<UFortWorldItemDefinition>("/Game/Items/ResourcePickups/WoodItemData.WoodItemData");
	static auto Stone = StaticFindObject<UFortWorldItemDefinition>("/Game/Items/ResourcePickups/StoneItemData.StoneItemData");
	static auto Metal = StaticFindObject<UFortWorldItemDefinition>("/Game/Items/ResourcePickups/MetalItemData.MetalItemData");
	if (GNeon->bCreative && KillerPawn)
	{
		auto KillerPC = Cast<AFortPlayerControllerAthena>(KillerPawn->GetController());
		if (KillerPC && (KillerPawn && KillerPawn != VictimPawn))
		{
			auto vol = KillerPC->GetCurrentVolume();
			auto cmpnt = (UFortMutatorListComponent*)vol->GetComponentByClass(UFortMutatorListComponent::StaticClass());
			auto muts = cmpnt->GetMutators();
			static auto SiphonValuesMut = StaticFindObject<UBlueprintGeneratedClass>("/Game/Athena/Playlists/Creative/Mutators/CreativeMutator_SiphonValues.CreativeMutator_SiphonValues_C");
			for (int32 i = 0; i < muts.Num(); i++)
			{
				auto mut = muts[i];
				if (mut->IsA(SiphonValuesMut))
				{
					auto castmut = Cast<AFortAthenaMutator_SiphonValues>(mut);
					auto WorldInventory = KillerPC->GetWorldInventory();

					if (!WorldInventory)
						return;
					
					int maxHealth = KillerPawn->GetMaxHealth();
					int maxShield = KillerPawn->GetMaxShield();
					int SiphonAmount = castmut->GetHealthSiphonValue();
					
					KillerPawn->ApplySiphon(SiphonAmount, maxHealth, maxShield);
					break;
				}

			}
		}
	} else if (GNeon->bLategame && KillerPawn)
	{
		auto KillerPC = Cast<AFortPlayerControllerAthena>(KillerPawn->GetController());
		if (KillerPC && (KillerPawn && KillerPawn != VictimPawn))
		{
			int MaxHealth = KillerPawn->GetMaxHealth();
			int MaxShield = KillerPawn->GetMaxShield();
		
			KillerPawn->ApplySiphon(75, MaxHealth, MaxShield);

			static FGameplayTag EarnedElim = { UKismetStringLibrary::Conv_StringToName(L"Event.EarnedElimination") };
			FGameplayEventData Data{};
			Data.EventTag = EarnedElim;
			Data.ContextHandle = KillerPlayerState->GetAbilitySystemComponent()->MakeEffectContext();
			Data.Instigator = KillerPlayerState->GetOwner();
			Data.Target = PlayerState;
			Data.TargetData = UAbilitySystemBlueprintLibrary::GetDefaultObj()->CallFunc<FGameplayAbilityTargetDataHandle>("AbilitySystemBlueprintLibrary", "AbilityTargetDataFromActor", PlayerState);

			UAbilitySystemBlueprintLibrary::GetDefaultObj()->CallFunc<void>("AbilitySystemBlueprintLibrary", "SendGameplayEventToActor", KillerPC->GetMyFortPawn(), EarnedElim, Data);
		}
	}
	
	auto WorldInventory = GetWorldInventory();
	if (!bRespawningAllowed && WorldInventory && VictimPawn)
	{
		static const UClass* WeaponClass = UFortWeaponRangedItemDefinition::StaticClass();
		static const UClass* ConsumableClass = UFortConsumableItemDefinition::StaticClass();
		static const UClass* AmmoClass = UFortAmmoItemDefinition::StaticClass();
		static const UClass* MeleeClass = UFortWeaponMeleeItemDefinition::StaticClass();
   
		bool bFoundMats = false;
   
		for (const auto& entry : WorldInventory->GetInventory().GetItemInstances()) {
			auto ItemDef = entry->GetItemEntry().GetItemDefinition();
        
			if (ItemDef->IsA(MeleeClass)) continue;
        
			int Count = entry->GetItemEntry().GetCount();
        
			if (ItemDef == Wood || ItemDef == Stone || ItemDef == Metal) {
				bFoundMats = true;
				WorldInventory->SpawnPickupStatic(
	DeathLocation, 
	&entry->GetItemEntry(), 
	EFortPickupSourceTypeFlag::Container, 
	EFortPickupSpawnSource::Unset, 
	nullptr, 
Count, false, true, true);
				
			}
			
			if (ItemDef->IsA(WeaponClass) || ItemDef->IsA(ConsumableClass) || ItemDef->IsA(AmmoClass)) {
				WorldInventory->SpawnPickupStatic(
DeathLocation, 
&entry->GetItemEntry(), 
EFortPickupSourceTypeFlag::Container, 
EFortPickupSpawnSource::Unset, 
nullptr, 
Count, false, true, true);
			}
		}
   
		if (!bFoundMats) {
			WorldInventory->SpawnPickupStatic(
DeathLocation, 
WorldInventory->MakeItemEntry(Metal, 50, 1),
EFortPickupSourceTypeFlag::Container, 
EFortPickupSpawnSource::Unset, 
nullptr, 
50, false, true, true);
			WorldInventory->SpawnPickupStatic(
DeathLocation, 
WorldInventory->MakeItemEntry(Stone, 50, 1),
EFortPickupSourceTypeFlag::Container, 
EFortPickupSpawnSource::Unset, 
nullptr, 
50, false, true, true);
			WorldInventory->SpawnPickupStatic(
DeathLocation, 
WorldInventory->MakeItemEntry(Wood, 50, 1),
EFortPickupSourceTypeFlag::Container, 
EFortPickupSpawnSource::Unset, 
nullptr, 
50, false, true, true);
		}
	}
	
	if (!KillerPlayerState) KillerPlayerState = PlayerState;
	if (!KillerPawn) KillerPawn = VictimPawn;

	static const UClass* AI = Fortnite_Version >= 9.00 ? AFortAthenaAIBotController::StaticClass() : nullptr;
	
	if (KillerPlayerState && KillerPawn && KillerPawn->GetController() && KillerPawn->GetController() != this && !(Fortnite_Version >= 9.00 ? KillerPawn->GetController()->IsA(AI) : false))
	{
		int32 KillerScore = KillerPlayerState->GetKillScore() + 1;
		int32 TeamScore = KillerPlayerState->GetTeamKillScore() + 1;
		
		KillerPlayerState->SetKillScore(KillerScore);
		KillerPlayerState->SetTeamKillScore(TeamScore);
		KillerPlayerState->ClientReportTeamKill(TeamScore);
		KillerPlayerState->ClientReportKill(PlayerState);
   	
		if (auto CPlayerController = (AFortPlayerControllerAthena*)KillerPawn->GetController()) {
			if (CPlayerController->GetMyFortPawn() && MatchStats) {
				ClientSendTeamStatsForPlayer(*TeamStats);
			}
		}
	}
	
	static int DamageCauserOffset = UKismetMemLibrary::GetOffsetStruct("FortPlayerDeathReport", "DamageCauser");
	AActor* DamageCauser = *(AActor**)((char*)&DeathReport + DamageCauserOffset);
	UFortWeaponItemDefinition* ItemDef = nullptr;

	static const UClass* FortProjectileBase = AFortProjectileBase::StaticClass();
	if (IsValidPointer(DamageCauser))
	{
		if (DamageCauser->IsValidLowLevel())
		{
			if (DamageCauser->IsA(FortProjectileBase))
			{
				auto Owner = (AFortWeapon*)(DamageCauser->GetOwner());
				ItemDef = Owner->IsValidLowLevel() ? Owner->GetWeaponData() : nullptr; 
			}

			if (auto WeaponDef = Cast<AFortWeapon>(DamageCauser))
			{
				ItemDef = WeaponDef->IsValidLowLevel() ? WeaponDef->GetWeaponData() : nullptr;
			}
		}
	} 
		
	int32 AliveCount = GameMode->GetAlivePlayers().Num() + (Fortnite_Version >= 9.00 ? GameMode->GetAliveBots().Num() : 0);
	
	if (!KillerPawn->GetController()->IsA(AI))
	{
		if (GameState->GetPlayersLeft() <= 25)
		{
			int Score = (GameState->GetPlayersLeft() == 1) ? 30 : 
						(GameState->GetPlayersLeft() == 2) ? 25 :
						(GameState->GetPlayersLeft() <= 5) ? 15 :
						(GameState->GetPlayersLeft() <= 25) ? 10 : 5;
			
			for (int i = 0; i < GameMode->GetAlivePlayers().Num(); i++)
			{
				GameMode->GetAlivePlayers()[i]->ClientReportTournamentPlacementPointsScored(GameState->GetPlayersLeft(), Score);
			}
		}
	}
	
	if (!bRespawningAllowed)
	{
		static void (*RemoveFromAlivePlayers)(AFortGameModeAthena* GameMode, AFortPlayerControllerAthena* PlayerController,
			APlayerState* KillerPlayerState, APawn* KillerPawn, UFortWeaponItemDefinition* KillerWeapon, uint8 DeathCause, bool bDBNO) = nullptr;

		if (!RemoveFromAlivePlayers) {
			RemoveFromAlivePlayers = decltype(RemoveFromAlivePlayers)(UKismetMemLibrary::GetAddress<uintptr_t>(L"RemoveFromAlivePlayers"));
		}

		if (RemoveFromAlivePlayers) {
			RemoveFromAlivePlayers(GameMode, this, KillerPlayerState, KillerPawn, ItemDef, DeathCause, false);
		} 

		int32 TotalAlive = GameMode->GetAlivePlayers().Num() + (Fortnite_Version >= 9.00 ? GameMode->GetAliveBots().Num() : 0);
		
		if (TotalAlive == 1) {
			AFortPlayerControllerAthena* LastAliveController = nullptr;
				
			for (int32 i = 0; i < GameMode->GetAlivePlayers().Num(); ++i) {
				if (GameMode->GetAlivePlayers()[i] && GameMode->GetAlivePlayers()[i]->GetMyFortPawn()) {
					LastAliveController = GameMode->GetAlivePlayers()[i];
					break;
				}
			}

			if (LastAliveController) {
				AFortPlayerStateAthena* WinnerPlayerState = (AFortPlayerStateAthena*)LastAliveController->GetPlayerState();
				AFortPlayerPawn* WinnerPawn = LastAliveController->GetMyFortPawn();
			   
				if (WinnerPlayerState && WinnerPawn) {
					WinnerPlayerState->SetPlace(1);
						
					auto WinnerMatchReport = LastAliveController->GetMatchReport();
					if (WinnerMatchReport && RewardResult) {
						auto WinnerRewardResult = &WinnerMatchReport->GetEndOfMatchResults();
						if (Fortnite_Version >= 11.00)
						{
							int32 WinnerXP = LastAliveController->GetXPComponent()->GetTotalXpEarned();
							WinnerRewardResult->SetTotalBookXpGained(WinnerXP);
							WinnerRewardResult->SetTotalSeasonXpGained(WinnerXP);
						}
						WinnerMatchReport->SetEndOfMatchResults(*WinnerRewardResult);
						LastAliveController->ClientSendEndBattleRoyaleMatchForPlayer(true, *WinnerRewardResult);

						auto WinnerMatchStats = &WinnerMatchReport->GetMatchStats();
						auto WinnerTeamStats = &WinnerMatchReport->GetTeamStats();

						WinnerMatchStats->Stats[3] = WinnerPlayerState->GetKillScore();
						WinnerMatchStats->Stats[8] = WinnerPlayerState->GetSquadId();
						WinnerMatchReport->SetMatchStats(*WinnerMatchStats);
						LastAliveController->ClientSendMatchStatsForPlayer(*WinnerMatchStats);

						int32 TotalPlayers = TotalAlive + 1;
						WinnerTeamStats->SetPlace(1);
						WinnerTeamStats->SetTotalPlayers(TotalPlayers);
						WinnerMatchReport->SetTeamStats(*WinnerTeamStats);
						LastAliveController->ClientSendTeamStatsForPlayer(*WinnerTeamStats);

						FGameplayTagContainer SourceTags;
						FGameplayTagContainer TargetTags;
						FGameplayTagContainer ContextTags;
						UFortQuestManager* QuestManager = GetQuestManager(ESubGame::Athena);
						QuestManager->GetSourceAndContextTags(&SourceTags, &ContextTags);
						QuestManager->SendStatEvent(SourceTags, TargetTags, nullptr, nullptr, 1, EFortQuestObjectiveStatEvent::Win, ContextTags);
					}

					uint8 WinningTeamIndex = WinnerPlayerState->GetTeamIndex();
					GameState->SetWinningTeam(WinningTeamIndex);
					GameState->SetWinningPlayerState(WinnerPlayerState);
					LastAliveController->CallFunc<void>("FortPlayerControllerAthena", "ClientNotifyWon", KillerPawn, ItemDef, DeathCause);
					LastAliveController->CallFunc<void>("FortPlayerController", "ClientFinishedInteractionInZone");
				}
			}
		}
		
		if (MatchReport && RewardResult) {
			MatchReport->SetEndOfMatchResults(*RewardResult);

			int32 PlayerPlace = AliveCount;
			PlayerState->SetPlace(PlayerPlace);

			int32 TotalXpEarned = 0;
			int32 TotalScoreEarned = 0;
			RewardResult->SetTotalSeasonXpGained(100);

			if (KillScore > 0)
			{
				TotalXpEarned += 100;
				TotalScoreEarned += 50;

				FAthenaAwardGroup FirstKill;
				FirstKill.RewardSource = ERewardSource::FirstKill;
				FirstKill.BookXp = 100;
				FirstKill.Score = 50;
			}

			if (KillScore >= 2)
			{
				KillScore--;

				TotalXpEarned += KillScore * 50;
				TotalScoreEarned += KillScore * 15;

				FAthenaAwardGroup FirstKill;
				FirstKill.RewardSource = ERewardSource::TeamKills;
				FirstKill.BookXp = KillScore * 50;
				FirstKill.Score = KillScore * 15;
			}

			auto It = TimeMap.find(this);
			int32 MinAlive = 0;

			if (It != TimeMap.end())
			{
				auto StartTime = It->second;
				auto EndTime   = std::chrono::steady_clock::now();
				auto AliveSecs = std::chrono::duration_cast<std::chrono::seconds>(EndTime - StartTime).count();

				MinAlive = static_cast<int32>(AliveSecs / 60); 
				if (MinAlive < 1) MinAlive = 1; 
			}
	
			{
				TotalXpEarned += MinAlive * 3;
				TotalScoreEarned += MinAlive;
				FAthenaAwardGroup MinutesPlay;
				MinutesPlay.RewardSource = ERewardSource::MinutesPlayed;
				MinutesPlay.BookXp = MinAlive * 3;
				MinutesPlay.Score = MinAlive;
			}

			int32 TotalXP = Fortnite_Version >= 11.10 ? GetXPComponent()->GetTotalXpEarned() : RewardResult->GetTotalSeasonXpGained() + TotalXpEarned;
			RewardResult->SetTotalBookXpGained(TotalXP);
			RewardResult->SetTotalSeasonXpGained(TotalXP);
			
			if (KillScore && PlayerState->GetSquadId() && MatchStats) {
				MatchStats->Stats[3] = KillScore;
				if (Fortnite_Version >= 7.00) MatchStats->Stats[8] = PlayerState->GetSquadId();
				MatchReport->SetMatchStats(*MatchStats);
				ClientSendMatchStatsForPlayer(*MatchStats);
			}

			if (TeamStats) {
				TeamStats->SetPlace(PlayerPlace);
				TeamStats->SetTotalPlayers(PlayerPlace);
				MatchReport->SetTeamStats(*TeamStats);
				ClientSendTeamStatsForPlayer(*TeamStats);
			}
			
			for (auto& Controller : GameMode->GetAlivePlayers())
			{
				FGameplayTagContainer SourceTags;
				FGameplayTagContainer TargetTags;
				FGameplayTagContainer ContextTags;
				UFortQuestManager* QuestManager = Controller->GetQuestManager(ESubGame::Athena);
				QuestManager->GetSourceAndContextTags(&SourceTags, &ContextTags);
				QuestManager->SendStatEvent(SourceTags, TargetTags, nullptr, nullptr, 1, EFortQuestObjectiveStatEvent::AthenaOutlive, ContextTags);
			}
		}
	}

	PlayerState->OnRep_DeathInfo();
	KillerPlayerState->OnRep_KillScore();
	KillerPlayerState->OnRep_TeamKillScore();
	PlayerState->OnRep_Place();
	GameState->OnRep_WinningTeam();
	if (Fortnite_Version >= 9.00) GameState->OnRep_WinningPlayerState();

	FGameplayTagContainer SourceTags;
	FGameplayTagContainer TargetTags;
	FGameplayTagContainer ContextTags;
	UFortQuestManager* QuestManager = GetQuestManager(ESubGame::Athena);
	if (!QuestManager)
	{
		if (!bRespawningAllowed && GNeon->bProd) FLUXBroadcastMatchResults(*RewardResult);
		return;
	}
	
	QuestManager->GetSourceAndContextTags(&SourceTags, &ContextTags);
	QuestManager->SendStatEvent(SourceTags, TargetTags, nullptr, nullptr, 1, EFortQuestObjectiveStatEvent::AthenaRank, ContextTags);
	
	std::thread([bRespawningAllowed, this, RewardResult]() {
		std::this_thread::sleep_for(std::chrono::seconds(5));
		if (!bRespawningAllowed && GNeon->bProd) {
			FLUXBroadcastMatchResults(*RewardResult);
		}
	}).detach();
	
	return ClientOnPawnDiedOG(this, DeathReport);
}

void AFortPlayerControllerAthena::FLUXBroadcastMatchResults(FAthenaRewardResult& Result)
{
	AFortPlayerStateAthena* PlayerState = (AFortPlayerStateAthena*)GetPlayerState();
	if (!PlayerState) return;
	std::string AccountId = PlayerState->GetAccountID();

	static int TotalPlayers = GetWorld()->GetGameState()->GetPlayersLeft() + 1;  // you want all players from when the game started
	static std::string Playlist = Fortnite_Version >= 6.10 ? GetWorld()->GetGameState()->GetCurrentPlaylistInfo().GetBasePlaylist()->GetPlaylistName().ToString().ToString() : GetWorld()->GetGameState()->GetCurrentPlaylistData()->GetPlaylistName().ToString().ToString();
	auto itSession = std::find_if(args.begin(), args.end(), [](const std::wstring& s) {
return s.rfind(L"-session=", 0) == 0;
});
	json j = {
		{"Controller", {
						{"AccountID", AccountId.c_str()},
						{"KillScore", PlayerState->GetKillScore()},
						{"Team", json::array({AccountId.c_str()})},
						{"Position", PlayerState->GetPlace()},
						{"TeamKillScore", PlayerState->GetTeamKillScore()},
						{"TotalXPEarned", Result.GetTotalSeasonXpGained()}
		}},
		{"Session", {
						{"ID", std::wstring(itSession != args.end() ? itSession->substr(9) : L"").c_str()},
						{"StartingPlayers", TotalPlayers},
						{"Playlist", Playlist}
		}}
	};

	std::vector<struct FFluxBroadcastQuestProgress> QuestProgress = GetQuestManager(ESubGame::Athena)->GetFluxBroadcastQuestProgress(this);
	j["QuestProgress"] = json::array();
	if (!QuestProgress.empty()) {
		for (const auto& Progress : QuestProgress) {
			if (!Progress.BackendName.empty()) {
				json ProgressJson;
				ProgressJson["BackendName"] = Progress.BackendName;
				ProgressJson["Count"] = Progress.Count;
				try {
					j["QuestProgress"].push_back(ProgressJson);
				} catch (const json::exception& e) {
					UE_LOG(LogTemp, Warning, "Failed to push quest progress: %s", e.what());
				}
			}
		}
	}
		
	PostRequest(L"flux-fn-server-data.fluxfn.org", L"/flux/api/v1/server/session/BroadcastMatchResults", j.dump());
}
