#include "pch.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/FortGameStateAthena.h"

#include "Engine/Plugins/Kismet/Public/KismetHookingLibrary.h"
#include "Engine/Plugins/Kismet/Public/KismetMemLibrary.h"
#include "Engine/Plugins/Neon/Public/Neon.h"
#include "Engine/Runtime/Engine/Classes/LevelStreamingDynamic.h"
#include "Engine/Runtime/Engine/Classes/World.h"
#include "Engine/Runtime/FortniteGame/Public/Athena/FortPlaylistAthena.h"
#include <Engine/Runtime/FortniteGame/Public/Building/BuildingFoundation.h>

void AFortGameStateAthena::OnRep_CurrentPlaylistId()
{
    static UFunction* Func = SDK::PropLibrary->GetFunctionByName("FortGameStateAthena", "OnRep_CurrentPlaylistId").Func;
    if ( Func ) this->ProcessEvent(Func, nullptr);
}

void AFortGameStateAthena::OnRep_CurrentPlaylistInfo()
{
    this->CallFunc<void>("FortGameStateAthena", "OnRep_CurrentPlaylistInfo");
}

void AFortGameStateAthena::OnFinishedStreamingAdditionalPlaylistLevel()
{
    static UFunction* Func = SDK::PropLibrary->GetFunctionByName("FortGameStateAthena", "OnFinishedStreamingAdditionalPlaylistLevel").Func;
    if ( Func ) this->ProcessEvent(Func, nullptr);
}
void AFortGameStateAthena::OnRep_CurrentPlaylistData()
{
    static UFunction* Func = SDK::PropLibrary->GetFunctionByName("FortGameStateAthena", "OnRep_CurrentPlaylistData").Func;
    if ( Func ) this->ProcessEvent(Func, nullptr);
}

void AFortGameStateAthena::OnRep_AdditionalPlaylistLevelsStreamed()
{
    static UFunction* Func = SDK::PropLibrary->GetFunctionByName("FortGameStateAthena", "OnRep_AdditionalPlaylistLevelsStreamed").Func;
    if ( Func ) this->ProcessEvent(Func, nullptr);
}

void AFortGameStateAthena::SetPlaylist(UFortPlaylistAthena* Playlist) 
{
    Playlist->SetGarbageCollectionFrequency(9999999.f);

    if (Fortnite_Version <= 6.10)
    {
        SetCurrentPlaylistData(Playlist);
        OnRep_CurrentPlaylistData();
        
        SetCurrentPlaylistId(Playlist->GetPlaylistId());
        GetWorld()->GetAuthorityGameMode()->SetCurrentPlaylistName(Playlist->GetPlaylistName());
        GetWorld()->GetAuthorityGameMode()->SetCurrentPlaylistId(Playlist->GetPlaylistId());
        OnRep_CurrentPlaylistId();

        return;
    }
    
 //   static int CurrentPlaylistInfoOffset = UKismetMemLibrary::GetOffsetStruct("FortGameStateAthena", "CurrentPlaylistInfo");
   // FPlaylistPropertyArray& CurrentPlaylistInfoPtr = *reinterpret_cast<FPlaylistPropertyArray*>(__int64(this) + CurrentPlaylistInfoOffset);
    FPlaylistPropertyArray& CurrentPlaylistInfoPtr = GetCurrentPlaylistInfo();
    
    CurrentPlaylistInfoPtr.SetBasePlaylist( Playlist );
    CurrentPlaylistInfoPtr.SetOverridePlaylist( Playlist );
    CurrentPlaylistInfoPtr.SetPlaylistReplicationKey( CurrentPlaylistInfoPtr.GetPlaylistReplicationKey() + 1 );
    CurrentPlaylistInfoPtr.MarkArrayDirty();

    SetCurrentPlaylistId(Playlist->GetPlaylistId());
    GetWorld()->GetAuthorityGameMode()->SetCurrentPlaylistName(Playlist->GetPlaylistName());
    GetWorld()->GetAuthorityGameMode()->SetCurrentPlaylistId(Playlist->GetPlaylistId());
    
    OnRep_CurrentPlaylistId();
    OnRep_CurrentPlaylistInfo();

    Playlist->SetGarbageCollectionFrequency(9999999.f);
    GetWorld()->GetAuthorityGameMode()->SetbDisableGCOnServerDuringMatch(true);
    GetWorld()->GetAuthorityGameMode()->SetbPlaylistHotfixChangedGCDisabling(true);

    auto Load = [&](int Offset, bool bServerOnly) {
        if (Offset == -1) return;
        auto& Levels = *reinterpret_cast<TArray<TSoftObjectPtr<UWorld>>*>(__int64(Playlist) + Offset);
        
        for (size_t i = 0; i < Levels.Num(); i++) {
            auto LevelName = Levels[i].SoftObjectPtr.ObjectID.AssetPathName;
            bool Success;
            ULevelStreamingDynamic::LoadLevelInstance(GetWorld(), UKismetStringLibrary::Conv_NameToString(LevelName), {}, {}, &Success, {});
            
            if (Fortnite_Version >= 11.50) {
                GetAdditionalPlaylistLevelsStreamed().Add({LevelName, bServerOnly}, StaticClassImpl("AdditionalLevelStreamed")->GetSize());
                UE_LOG(LogGameState, Log, "%s Level: %s", bServerOnly ? "Additional Server" : "Additional", LevelName.ToString().ToString().c_str());
            } else if (!bServerOnly) {
                static int StreamedOffset = UKismetMemLibrary::GetOffset(this, "AdditionalPlaylistLevelsStreamed");
                auto* StreamedLevels = reinterpret_cast<TArray<FName>*>(__int64(this) + StreamedOffset);
                if (StreamedLevels) StreamedLevels->Add(LevelName);
                UE_LOG(LogGameState, Log, "Level: %s", LevelName.ToString().ToString().c_str());
            }
        }
    };

   if (auto* VolcanoFoundation = StaticLoadObject<ABuildingFoundation>("/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.LF_Athena_POI_50x53_Volcano"))
   {
       VolcanoFoundation->SetDynamicFoundationType(EDynamicFoundationType::Static);
       VolcanoFoundation->SetbServerStreamedInLevel(true);
       VolcanoFoundation->OnRep_ServerStreamedInLevel();
       VolcanoFoundation->OnRep_DynamicFoundationRepData();
       VolcanoFoundation->SetDynamicFoundationEnabled(true);
   }
   else
   {
       UE_LOG(LogGameState, Warning, "SetPlaylist: failed to load LF_Athena_POI_50x53_Volcano foundation asset");
   }

       
    Load(UKismetMemLibrary::GetOffset(Playlist, "AdditionalLevels"), false);
    Load(UKismetMemLibrary::GetOffset(Playlist, "AdditionalLevelsServerOnly"), true);

    OnRep_AdditionalPlaylistLevelsStreamed();
    OnFinishedStreamingAdditionalPlaylistLevel();
}
