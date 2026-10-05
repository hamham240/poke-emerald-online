#include "global.h"
#include "battle.h"
#include "battle_setup.h"
#include "battle_transition.h"
#include "link.h"
#include "load_save.h"
#include "main.h"
#include "online_duel.h"
#include "online_session.h"
#include "overworld.h"
#include "task.h"
#include "constants/songs.h"
#include "constants/trainers.h"

// Duels
//
// A plain link single battle against the partner's whole party, as in the
// Colosseum. Link battles give no EXP or prize money; the party is restored
// afterwards, so the battle leaves no trace on either team.

static EWRAM_DATA bool8 sIsOnlineDuel = FALSE;

bool32 IsOnlineDuel(void)
{
    return sIsOnlineDuel;
}

static void CB2_EndOnlineDuel(void)
{
    LoadPlayerParty();
    sIsOnlineDuel = FALSE;
    SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
}

static void Task_StartOnlineDuel(u8 taskId)
{
    if (IsBattleTransitionDone() == TRUE)
    {
        gMain.savedCallback = CB2_EndOnlineDuel;
        SetMainCallback2(CB2_InitBattle);
        DestroyTask(taskId);
    }
}

// Run by both players once the handshake says go (use waitstate)
void OnlineDuel_Start(void)
{
    Online_RestoreLinkPlayers();
    SavePlayerParty();
    sIsOnlineDuel = TRUE;
    gLinkType = LINKTYPE_BATTLE;
    gBattleTypeFlags = BATTLE_TYPE_LINK | BATTLE_TYPE_TRAINER;
    gTrainerBattleOpponent_A = TRAINER_LINK_OPPONENT;

    CreateTask(Task_StartOnlineDuel, 1);
    PlayMapChosenOrBattleBGM(MUS_VS_TRAINER);
    BattleTransition_StartOnField(B_TRANSITION_BLACKHOLE_PULSATE);
}
