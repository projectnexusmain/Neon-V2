#include "pch.h"
#include "./Engine/Runtime/FortniteGame/Public/Creative/FortMinigame.h"
#include <Engine/Runtime/FortniteGame/Public/Player/FortPlayerState.h>
#include <Engine/Runtime/FortniteGame/Public/Athena/Player/FortPlayerControllerAthena.h>

struct FortPlayerControllerAthena_ClientStartRespawnPreparation final
{
public:
	struct FVector                                RespawnLoc;                                        // 0x0000(0x000C)(Parm, ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
	struct FRotator                               RespawnRot;                                        // 0x000C(0x000C)(Parm, ZeroConstructor, IsPlainOldData, NoDestructor, NativeAccessSpecifierPublic)
	float                                         RespawnCameraDist;                                 // 0x0018(0x0004)(Parm, ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
	class FName                                   InRespawnCameraBehavior;                           // 0x001C(0x0008)(Parm, ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
	uint8                                         Pad_2712[0x4];                                     // 0x0024(0x0004)(Fixing Size After Last Property [ Dumper-7 ])
	class FText                                   HUDReasonText;                                     // 0x0028(0x0018)(ConstParm, Parm, ReferenceParm, NativeAccessSpecifierPublic)
};


void AFortMinigame::ChangeMinigameState(EFortMinigameState State)
{
	UE_LOG(LogCreative, Log, TEXT("AFortMinigame::ChangeMinigameState %d"), State);

	static auto GetParticipatingPlayers = StaticFindObject<UFunction>("/Script/FortniteGame.FortMinigame.GetParticipatingPlayers");
	static auto OnPlayerPawnPossessedDuringTransition = StaticFindObject<UFunction>("/Script/FortniteGame.FortMinigame.OnPlayerPawnPossessedDuringTransition");
	static auto ClientStartRespawnPreparation = StaticFindObject<UFunction>("/Script/FortniteGame.FortPlayerControllerAthena.ClientStartRespawnPreparation");
	static auto OnClientFinishTeleportingForMinigame = StaticFindObject<UFunction>("/Script/FortniteGame.FortMinigame.OnClientFinishTeleportingForMinigame");
	static auto AdvanceState = StaticFindObject<UFunction>("/Script/FortniteGame.FortMinigame.AdvanceState");
	static auto HandleMinigameStarted = StaticFindObject<UFunction>("/Script/FortniteGame.FortMinigame.HandleMinigameStarted");
	static auto HandleVolumeEditModeChange = StaticFindObject<UFunction>("/Script/FortniteGame.FortMinigame.HandleVolumeEditModeChange");
	static auto FinishTeleport = StaticFindObject<UFunction>("/Script/FortniteGame.FortMinigame.OnClientFinishTeleportingForMinigame");

	struct FortMinigame_GetParticipatingPlayers final
	{
	public:
		TArray<class AFortPlayerState*> OutPlayers; // 0x0000(0x0010)(Parm, OutParm, ZeroConstructor, NativeAccessSpecifierPublic)
	};
	FortMinigame_GetParticipatingPlayers ret;
	ProcessEvent(GetParticipatingPlayers, &ret);
	TArray<class AFortPlayerState *> Players = ret.OutPlayers;

	    FFortMinigameStatCollection Stats{};
    std::unordered_map<int, int> TeamScores;

    for (int i = 0; i < Players.Num(); i++)
    {
        auto Player = Cast<AFortPlayerStateAthena>(Players[i]);
        if (!Player)
            continue;

        int TeamIndex = Player->GetTeamIndex();
        TeamScores[TeamIndex] += 1;

        FFortMinigameStat Stat{};
        Stat.Count = 1;

        FFortMinigamePlayerStats PlayerStats{};
        PlayerStats.Player = Player->GetUniqueId();
        PlayerStats.Stats.Add(Stat);

        FFortMinigamePlayerBucketStats BucketStats{};
        BucketStats.BucketIndex = TeamIndex;
        BucketStats.Stats.Add(Stat);

        Stats.PlayerStats.Add(PlayerStats);
        Stats.PlayerBucketStats.Add(BucketStats);
        Stats.GroupStats.Stats.Add(Stat);
    }

    // Pick winner
    int WinningTeam = 0;
    int BestScore = -1;
    for (auto& Pair : TeamScores)
    {
        if (Pair.second > BestScore)
        {
            BestScore = Pair.second;
            WinningTeam = Pair.first;
        }
    }
    this->SetWinningTeamIndex(WinningTeam);

    switch (State)
    {

    default:
        {
                ChangeMinigameStateOG(this, State);
                FFortMinigameStatCollection Stats{};
                for (int i = 0; i < Players.Num(); i++)
                {
                        auto Player = Cast<AFortPlayerStateAthena>(Players[i]);
                        this->SetWinningTeamIndex(0);

                        FFortMinigamePlayerBucketStats BucketStats{};
                        FFortMinigameStat Stat{};
                        FFortMinigamePlayerStats PlayerStats{};

                        Stat.Count = 1;

                        PlayerStats.Player = Player->GetUniqueId();
                        PlayerStats.Stats.Add(Stat);

                        BucketStats.BucketIndex = 0;
                        BucketStats.Stats.Add(Stat);

                        Stats.PlayerBucketStats.Add(BucketStats);
                        Stats.GroupStats.Stats.Add(Stat);
                        Stats.PlayerStats.Add(PlayerStats);
                }

                std::thread([this, State, Stats]()
                {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        this->SetGameStats(Stats);
                        ChangeMinigameStateOG(this, State);
                }).detach();

                break;
        }
    case EFortMinigameState::Transitioning:
        {
		static auto AdvanceState = StaticFindObject<UFunction>("/Script/FortniteGame.FortMinigame.AdvanceState");
		static auto HandleMinigameStarted = StaticFindObject<UFunction>("/Script/FortniteGame.FortMinigame.HandleMinigameStarted");
		static auto HandleVolumeEditModeChange = StaticFindObject<UFunction>("/Script/FortniteGame.FortMinigame.HandleVolumeEditModeChange");

		for (int i = 0; i < Players.Num(); i++)
		{
			auto Player = Cast<AFortPlayerStateAthena>(Players[i]);
			auto PC = Cast<AFortPlayerControllerAthena>(Player->GetOwner());
			if (!PC) 
				continue;
			auto Pawn = PC->GetMyFortPawn();
			if (!Pawn) 
			{
				continue;
			}
			
			FVector Loc{};
			FVector Loc2 = Pawn->K2_GetActorLocation();
			FRotator Rot = Pawn->K2_GetActorRotation();
			DetermineSpawnLocation(Player, &Loc, &Rot, nullptr);

				PC->GetWorldInventory()->ClearInventory();
				
				if (GetNumTeams() == 0)
					PC->ServerSetTeam(i + 3);
				else
					PC->ServerSetTeam((i % GetNumTeams()) + 3);
				
				FortPlayerControllerAthena_ClientStartRespawnPreparation f{};
				f.HUDReasonText = GetClientMinigameStartedText();
				f.InRespawnCameraBehavior = GetMinigameStartCameraBehavior();
				f.RespawnCameraDist = 0;
				f.RespawnLoc = Loc;
				f.RespawnRot = Rot;
				
				

				PC->ProcessEvent(ClientStartRespawnPreparation, &f);
				Player->GetRespawnData().RespawnLocation = Loc;
				Player->GetRespawnData().RespawnRotation = Rot;
				Pawn->K2_TeleportTo(Loc, Rot);

				
				PC->UnPossess();
				Pawn->K2_DestroyActor();
				this->ProcessEvent(OnPlayerPawnPossessedDuringTransition, &Pawn);
				

			}
	
			std::thread([this, Players]()
			{
			bool AllRespawned = false;

			while (!AllRespawned)
			{
				AllRespawned = true;

				for (int i = 0; i < Players.Num(); i++)
				{
					auto Player = Cast<AFortPlayerStateAthena>(Players[i]);
					auto PC = Cast<AFortPlayerControllerAthena>(Player->GetOwner());
					if (!PC) 
						continue;

					auto Pawn = PC->GetMyFortPawn();
					if (!Pawn)
					{
						AllRespawned = false;
						break;
					}

					if (Pawn->GetbIsRespawning())
					{
						AllRespawned = false;
						break;
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}

			this->ProcessEvent(AdvanceState, nullptr);
			//ProcessEvent(HandleMinigameStarted, nullptr);

			//struct { bool bInEditMode; } params{ true };
			//ProcessEvent(HandleVolumeEditModeChange, &params);
			}).detach();
		}
		break;
	case EFortMinigameState::WaitingForCameras:
	{
		for (int i = 0; i < Players.Num(); i++)
		{
			auto Player = Cast<AFortPlayerStateAthena>(Players[i]);


			auto PC = Cast<AFortPlayerControllerAthena>(Player->GetOwner());
			if (!PC)
			{
				continue;
			}
			auto Pawn = PC->GetMyFortPawn();
			if (!Pawn)
			{
				//LOG_INFO(LogDev, "Player has no pawn, skipping.");
				continue;
			}
			this->ProcessEvent(FinishTeleport, &Pawn);




		}
		std::thread([this, State]()
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				ChangeMinigameStateOG(this, State);
			}).detach();
	}
	break;
	case EFortMinigameState::PostGameReset:
	{
		ChangeMinigameStateOG(this, State);
		for (int i = 0; i < Players.Num(); i++)
		{
			auto Player = Cast<AFortPlayerStateAthena>(Players[i]);
			auto PC = Cast<AFortPlayerControllerAthena>(Player->GetOwner());
			auto Pawn = PC->GetMyFortPawn();
			if (!Pawn) continue;

			FVector Loc{};
			FVector Loc2 = Pawn->K2_GetActorLocation();
			FRotator Rot = Pawn->K2_GetActorRotation();
			DetermineSpawnLocation(Player, &Loc, &Rot, nullptr);
			
			PC->GetWorldInventory()->ClearInventory();

			FortPlayerControllerAthena_ClientStartRespawnPreparation f{};
			f.HUDReasonText = this->GetClientMinigameEndedText();
			f.InRespawnCameraBehavior = this->GetMinigameEndCameraBehavior();
			f.RespawnCameraDist = 0;
			f.RespawnLoc = Loc;
			f.RespawnRot = Rot;
			Player->GetRespawnData().bRespawnDataAvailable = true;
			Player->GetRespawnData().bServerIsReady = true;
			//Player->GetRespawnData().RespawnLocation = Loc;
			//Player->GetRespawnData().RespawnRotation = Rot;
			PC->ProcessEvent(ClientStartRespawnPreparation, &f);
			Pawn->K2_TeleportTo(Loc, Rot);
			//PC->UnPossess();
			Pawn->K2_DestroyActor();
			ProcessEvent(OnPlayerPawnPossessedDuringTransition, &Pawn);

		}
		this->SetCurrentState(EFortMinigameState::PreGame);
	}
	break;
	break;
	
	}

	if (State != EFortMinigameState::WaitingForCameras && State != EFortMinigameState::PostGameReset)
	{
		return ChangeMinigameStateOG(this, State);
	}
	
}

void AFortMinigame::GetParticipatingPlayers(TArray<class AFortPlayerState *> &OutPlayers)
{
	static SDK::UFunction *Func = nullptr;
	static SDK::FFunctionInfo Info = SDK::PropLibrary->GetFunctionByName("FortMinigame", "GetParticipatingPlayers");

	if (Func == nullptr)
		Func = Info.Func;
	if (!Func)
		return;

	this->ProcessEvent(Func, &OutPlayers);
}

bool AFortMinigame::DetermineSpawnLocation(AFortPlayerStateAthena *PlayerState, FVector *OutLocation, FRotator *OutRotation, bool *bOutRespawningOnGround)
{
	static SDK::UFunction *Func = nullptr;
	static SDK::FFunctionInfo Info = SDK::PropLibrary->GetFunctionByName("FortMinigame", "DetermineSpawnLocation");

	if (Func == nullptr)
		Func = Info.Func;
	if (!Func)
		return false;

	struct FortMinigame_DetermineSpawnLocation final
	{
	public:
		class AFortPlayerStateAthena *PlayerState; // 0x0000(0x0008)(Parm, ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
		struct FVector OutLocation;				   // 0x0008(0x000C)(Parm, OutParm, ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
		struct FRotator OutRotation;			   // 0x0014(0x000C)(Parm, OutParm, ZeroConstructor, IsPlainOldData, NoDestructor, NativeAccessSpecifierPublic)
		bool bOutRespawningOnGround;			   // 0x0020(0x0001)(Parm, OutParm, ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
		bool ReturnValue;						   // 0x0021(0x0001)(Parm, OutParm, ZeroConstructor, ReturnParm, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
		uint8 Pad_25FF[0x6];					   // 0x0022(0x0006)(Fixing Struct Size After Last Property [ Dumper-7 ])
	} Params{};
	Params.PlayerState = PlayerState;

	this->ProcessEvent(Func, &Params);
	if (OutLocation)
		*OutLocation = Params.OutLocation;
	if (OutRotation)
		*OutRotation = Params.OutRotation;
	if (bOutRespawningOnGround)
		*bOutRespawningOnGround = Params.bOutRespawningOnGround;
	return Params.ReturnValue;
}