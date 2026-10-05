#include "global.h"
#include "battle.h"
#include "battle_pyramid.h"
#include "battle_setup.h"
#include "battle_transition.h"
#include "data.h"
#include "event_data.h"
#include "evolution_scene.h"
#include "field_message_box.h"
#include "item.h"
#include "link.h"
#include "load_save.h"
#include "main.h"
#include "money.h"
#include "online_battle.h"
#include "online_link.h"
#include "online_session.h"
#include "overworld.h"
#include "palette.h"
#include "party_menu.h"
#include "recorded_battle.h"
#include "sound.h"
#include "string_util.h"
#include "task.h"
#include "trainer_hill.h"
#include "constants/hold_effects.h"
#include "constants/rgb.h"
#include "constants/trainers.h"

// Online co-op battles: both players team up against an NPC trainer, each
// bringing half a party. Started from the trainer's battle prompt
// (EventScript_DoOnlineDoubleBattle), once both players are waiting at the
// same trainer. The host's game runs the battle as a link battle.

bool8 gDisableMonSelectCancel;

// Where this player's chosen mons sit in gPlayerParty during an online battle,
// and which party slots they came from.
static EWRAM_DATA u8 sOnlinePartyOffset = 0;
static EWRAM_DATA u8 sOnlineSelectedOrder[MULTI_PARTY_SIZE] = {0};

// The partner's picks for the next online battle. They go into gPlayerParty's
// partner slots before the battle, for the team preview that runs before the
// battle start sequence swaps the parties.
static EWRAM_DATA struct Pokemon sOnlinePartnerParty[MULTI_PARTY_SIZE] = {0};

// After an online battle: party slots whose Pokémon leveled up, and the prize
static EWRAM_DATA u8 sOnlineLeveledUp = 0;
static EWRAM_DATA u32 sOnlinePrizeMoney = 0;

static const u8 sText_WaitingForOnlinePartner[] = _("Waiting for another player\nto join you…");

static void Task_WaitForOnlineDoubleBattleConnection(u8 taskId);
static void CB2_EndOnlineDoubleBattle(void);
static void CB2_FinishOnlineBattleWon(void);
static void RestorePartyAfterOnlineBattle(void);
static u32 GiveOnlinePrizeMoney(void);

// LINK | INGAME_PARTNER marks an online co-op battle
bool32 IsOnlineBattle(void)
{
    return (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_INGAME_PARTNER)) == (BATTLE_TYPE_LINK | BATTLE_TYPE_INGAME_PARTNER);
}

void OnlineBattle_Init(void)
{
    gDisableMonSelectCancel = FALSE;
}

// The party menu can't be backed out of while picking mons for a battle
// that's already been agreed to
void DisableMonSelectCancel(void)
{
    gDisableMonSelectCancel = TRUE;
}

void EnableMonSelectCancel(void)
{
    gDisableMonSelectCancel = FALSE;
}

// Run once the player has picked their half of the party (use waitstate).
// Waits for the partner at the same trainer, then starts the battle.
void OnlineBattle_Start(void)
{
    // IS_MASTER is decided during the battle start sequence
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER | BATTLE_TYPE_LINK;

    // One trainer sends out two Pokémon against both players (2 vs 1)
    gTrainerBattleOpponent_B = 0xFFFF;

    // Skips the in-game partner's front-pic intro and partner-only paths
    gPartnerTrainerId = TRAINER_CUSTOM_PARTNER;

    CreateTask(Task_WaitForOnlineDoubleBattleConnection, 0);
}

// The whiteout expects to start from a black screen, as it does after a battle
static void Task_BailOutOfOnlineBattle(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        gMain.state = 0;
        SetMainCallback2(CB2_WhiteOut);
        DestroyTask(taskId);
    }
}

// Pairing is a two-step handshake over the online link, so both players start
// the battle or neither does:
//   READY:     "I'm waiting at this trainer". Cancelling (B) is only allowed here.
//   COMMITTED: sent once the partner is ready at the same trainer. The battle
//              starts once the partner has committed too, which means they can
//              no longer cancel. If they cancelled first, go back to READY.
enum
{
    ONLINE_PAIR_READY,
    ONLINE_PAIR_COMMITTED,
};

#define tPairState  data[1]
#define tReadySent  data[2]

// Called from Online_UpdateLink with an ONLINE_MSG_PARTNER_PARTY at the front
// of the inbox
void OnlineBattle_ReceivePartnerParty(void)
{
    OnlineLink_Receive(sOnlinePartnerParty, sizeof(sOnlinePartnerParty));
}

static void Task_StartOnlineBattleAfterTransition(u8 taskId)
{
    if (IsBattleTransitionDone() == TRUE)
    {
        gMain.savedCallback = CB2_EndOnlineDoubleBattle;
        SetMainCallback2(CB2_InitBattle);
        DestroyTask(taskId);
    }
}

static void StartOnlineBattle(u8 taskId)
{
    OnlinePair_ClearPeer();
    Online_RestoreLinkPlayers();

    // For the team preview. The partner sent their picks before committing, and
    // messages arrive in order, so these are the ones for this battle.
    memcpy(&gPlayerParty[MULTI_PARTY_SIZE], sOnlinePartnerParty, sizeof(sOnlinePartnerParty));

    // The battle start sequence swaps the parties, putting the host's mons
    // first, then the joiner's
    sOnlinePartyOffset = GetMultiplayerId() == 0 ? 0 : MULTI_PARTY_SIZE;
    memcpy(sOnlineSelectedOrder, gSelectedOrderFromParty, sizeof(sOnlineSelectedOrder));

    HideFieldMessageBox();
    CreateTask(Task_StartOnlineBattleAfterTransition, 1);
    PlayMapChosenOrBattleBGM(0);
    BattleTransition_StartOnField(B_TRANSITION_BLACKHOLE_PULSATE);
    DestroyTask(taskId);
    EnableMonSelectCancel();
}

static void Task_WaitForOnlineDoubleBattleConnection(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    u16 trainerId = gTrainerBattleOpponent_A;
    bool32 connected = OnlineLink_IsConnected() && gReceivedRemoteLinkPlayers;

    if (IsFieldMessageBoxHidden())
        ShowFieldMessage(sText_WaitingForOnlinePartner);

    // Losing the connection undoes any commitment; start over once it's back
    if (!connected)
    {
        task->tPairState = ONLINE_PAIR_READY;
        task->tReadySent = FALSE;
    }
    else if (!task->tReadySent)
    {
        // Our picks go ahead of READY, so they reach the partner before we can commit
        OnlineLink_Send(ONLINE_MSG_PARTNER_PARTY, gPlayerParty, sizeof(struct Pokemon) * MULTI_PARTY_SIZE);
        OnlinePair_Send(ONLINE_MSG_PAIR_READY, trainerId);
        task->tReadySent = TRUE;
    }

    switch (task->tPairState)
    {
    case ONLINE_PAIR_READY:
        // Cancel: bail out to the last heal location, so the players can regroup
        // and approach the same trainer together instead of battling alone
        if (JOY_NEW(B_BUTTON))
        {
            if (connected)
                OnlinePair_Send(ONLINE_MSG_PAIR_CANCEL, trainerId);
            HideFieldMessageBox();
            EnableMonSelectCancel();
            LoadPlayerParty();
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
            task->func = Task_BailOutOfOnlineBattle;
            return;
        }

        if (connected && (OnlinePair_GetPeerReady() == trainerId || OnlinePair_GetPeerCommitted() == trainerId))
        {
            OnlinePair_Send(ONLINE_MSG_PAIR_COMMIT, trainerId);
            task->tPairState = ONLINE_PAIR_COMMITTED;
        }
        break;
    case ONLINE_PAIR_COMMITTED:
        if (OnlinePair_GetPeerCommitted() == trainerId)
        {
            StartOnlineBattle(taskId);
        }
        else if (OnlinePair_GetPeerReady() != trainerId)
        {
            // The partner cancelled before committing
            OnlinePair_Send(ONLINE_MSG_PAIR_READY, trainerId);
            task->tPairState = ONLINE_PAIR_READY;
        }
        break;
    }
}

#undef tPairState
#undef tReadySent

static void CB2_EndOnlineDoubleBattle(void)
{
    RecordedBattle_SaveBattleOutcome();
    EnableMonSelectCancel();

    // Tells the trainer script whether to show the prize money
    gSpecialVar_0x8006 = FALSE;
    gSpecialVar_Result = FALSE;

    if (Online_WasLinkLost())
    {
        // The battle ended early: undo it, back to the party from before
        // the picks. No result, so no flags, prize or whiteout.
        LoadPlayerParty();
        gTrainerBattleOpponent_B = 0;
        SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
        return;
    }

    RestorePartyAfterOnlineBattle();

    // 0xFFFF only marked the battle as 2 vs 1; SetBattledTrainersFlags
    // would otherwise set a flag for it.
    gTrainerBattleOpponent_B = 0;

    if (gTrainerBattleOpponent_A == TRAINER_SECRET_BASE)
    {
        SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
    }
    else if (IsPlayerDefeated(gBattleOutcome) == TRUE)
    {
        if (InBattlePyramid() || InTrainerHillChallenge())
            SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
        else
            SetMainCallback2(CB2_WhiteOut);
    }
    else
    {
        if (!InBattlePyramid() && !InTrainerHillChallenge())
        {
            RegisterTrainerInMatchCall();
            SetBattledTrainersFlags();
        }
        sOnlinePrizeMoney = GiveOnlinePrizeMoney();
        gSpecialVar_0x8006 = TRUE;
        SetMainCallback2(CB2_FinishOnlineBattleWon);
    }
}

// Each player gets the prize they'd get for beating this trainer alone. The
// Amulet Coin counts if one of this player's own battlers held it.
static u32 GiveOnlinePrizeMoney(void)
{
    const struct Trainer *trainer = &gTrainers[gTrainerBattleOpponent_A];
    u32 i, money, multiplier = 1;
    u32 lastMonLevel = trainer->party[trainer->partySize - 1].lvl;

    for (i = 0; gTrainerMoneyTable[i].classId != 0xFF; i++)
    {
        if (gTrainerMoneyTable[i].classId == trainer->trainerClass)
            break;
    }

    for (i = 0; i < MULTI_PARTY_SIZE; i++)
    {
        if (sOnlineSelectedOrder[i] != 0
         && ItemId_GetHoldEffect(GetMonData(&gPlayerParty[sOnlineSelectedOrder[i] - 1], MON_DATA_HELD_ITEM)) == HOLD_EFFECT_DOUBLE_PRIZE)
            multiplier = 2;
    }

    money = 4 * lastMonLevel * multiplier * gTrainerMoneyTable[i].value;
    if (trainer->doubleBattle)
        money *= 2;

    AddMoney(&gSaveBlock1Ptr->money, money);
    return money;
}

// Evolves this player's Pokémon that leveled up, one scene at a time, then
// returns to the field. The trainer script shows the prize money.
static void CB2_FinishOnlineBattleWon(void)
{
    u32 i;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (sOnlineLeveledUp & (1u << i))
        {
            u16 species;

            sOnlineLeveledUp &= ~(1u << i);
            species = GetEvolutionTargetSpecies(&gPlayerParty[i], EVO_MODE_NORMAL, ITEM_NONE, NULL);
            if (species != SPECIES_NONE)
            {
                gCB2_AfterEvolution = CB2_FinishOnlineBattleWon;
                BeginEvolutionScene(&gPlayerParty[i], species, TRUE, i);
                return;
            }
        }
    }

    // Buffered last, since the evolution scene uses the string vars
    ConvertIntToDecimalStringN(gStringVar1, sOnlinePrizeMoney, STR_CONV_MODE_LEFT_ALIGN, 7);
    SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
}

// Puts this player's mons, as they ended the battle, back into their full party
static void RestorePartyAfterOnlineBattle(void)
{
    struct Pokemon battleMons[MULTI_PARTY_SIZE];
    u32 i;

    for (i = 0; i < MULTI_PARTY_SIZE; i++)
        battleMons[i] = gPlayerParty[sOnlinePartyOffset + i];

    LoadPlayerParty();

    sOnlineLeveledUp = 0;
    for (i = 0; i < MULTI_PARTY_SIZE; i++)
    {
        u32 slot = sOnlineSelectedOrder[i] - 1;

        if (sOnlineSelectedOrder[i] == 0)
            continue;
        if (GetMonData(&battleMons[i], MON_DATA_LEVEL) > GetMonData(&gPlayerParty[slot], MON_DATA_LEVEL))
            sOnlineLeveledUp |= 1u << slot;
        gPlayerParty[slot] = battleMons[i];
    }
}
