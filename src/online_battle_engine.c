#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_controllers.h"
#include "battle_interface.h"
#include "battle_message.h"
#include "battle_script_commands.h"
#include "battle_setup.h"
#include "bg.h"
#include "link.h"
#include "m4a.h"
#include "main.h"
#include "online_battle.h"
#include "online_duel.h"
#include "online_link.h"
#include "online_session.h"
#include "overworld.h"
#include "palette.h"
#include "party_menu.h"
#include "pokemon.h"
#include "pokemon_summary_screen.h"
#include "reshow_battle_screen.h"
#include "scanline_effect.h"
#include "sound.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "util.h"
#include "window.h"
#include "constants/battle_string_ids.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// Online battles inside the battle engine, called from hooks in the vanilla
// battle code. The host's game runs co-op battles (see online_battle.c), with
// the partner's Pokémon in party slots 3-5 and the partner's game playing the
// right-hand player battler.

// Losing the link
//
// The online partner's game is gone, so nothing more will arrive from it and
// the battle can't go on. Ends the battle without a result; the end callbacks
// (CB2_EndOnlineDoubleBattle, CB2_EndOnlineDuel) undo it.
bool32 OnlineBattle_TryAbortDisconnected(void)
{
    if (!(IsOnlineBattle() || IsOnlineDuel()) || OnlineLink_IsConnected())
        return FALSE;

    DebugPrintf("Online battle: link lost, ending the battle");
    m4aSongNumStop(SE_LOW_HEALTH);
    SetHBlankCallback(NULL);
    SetVBlankCallback(NULL);
    ScanlineEffect_Stop();
    ResetTasks();
    FreeAllWindowBuffers();
    FreeBattleResources();
    FreeBattleSpritesData();
    FreeMonSpritesGfx();

    // Online battles are only started from the overworld
    gMain.inBattle = FALSE;
    SetMainCallback1(CB1_Overworld);
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK_IN_BATTLE;
    Online_SetLinkLost();
    SetMainCallback2(gMain.savedCallback);
    return TRUE;
}

// Battle start
//
// Link players 2 and 3 are the opponents. Both are the one NPC trainer.
void OnlineBattle_SetOpponentLinkPlayers(void)
{
    StringCopyN(gLinkPlayers[2].name, GetTrainerNameFromId(gTrainerBattleOpponent_A), PLAYER_NAME_LENGTH);
    gLinkPlayers[2].name[PLAYER_NAME_LENGTH] = EOS;
    StringCopy(gLinkPlayers[3].name, gLinkPlayers[2].name);
    gLinkPlayers[2].language = gLinkPlayers[3].language = GAME_LANGUAGE;
}

// Battle end
//
// Battle resources can't be freed from inside a controller callback, since the
// remaining battlers' controllers still run this frame.
static void CB2_FreeOnlineBattle(void)
{
    FreeAllWindowBuffers();
    FreeBattleResources();
    FreeBattleSpritesData();
    FreeMonSpritesGfx();
    SetMainCallback2(gMain.savedCallback);
}

// Online co-op battles are against an NPC trainer, so they end like a local
// trainer battle rather than showing the link battle results. Returns FALSE
// for other battles.
bool32 OnlineBattle_TrySetEndCallbacks(void)
{
    if (!IsOnlineBattle())
        return FALSE;

    if (gReceivedRemoteLinkPlayers == 0)
    {
        m4aSongNumStop(SE_LOW_HEALTH);
        gMain.inBattle = FALSE;
        gMain.callback1 = gPreBattleCallback1;
        SetMainCallback2(CB2_FreeOnlineBattle);
    }
    return TRUE;
}

// EXP
//
// Both players' Pokémon get EXP. The host's game hands it out; each player's
// own battler animates and updates their mon.

bool32 OnlineBattle_IsPartnerMon(u32 monId)
{
    return IsOnlineBattle() && monId >= MULTI_PARTY_SIZE;
}

u32 OnlineBattle_GetExpGetterBattler(u32 monId)
{
    return OnlineBattle_IsPartnerMon(monId) ? 2 : 0;
}

// Traded relative to the Pokémon's owner, not whoever is running the battle
bool32 OnlineBattle_IsTradedMon(u32 monId)
{
    if (OnlineBattle_IsPartnerMon(monId))
        return GetMonData(&gPlayerParty[monId], MON_DATA_OT_ID) != gLinkPlayers[GetMultiplayerId() ^ 1].trainerId;
    return IsTradedMon(&gPlayerParty[monId]);
}

// The partner's game animates the EXP bar and reports level-ups to the host.
// This keeps our copy of their Pokémon in step by applying the same single
// step their EXP task does, without replying. Handles CONTROLLER_EXPUPDATE for
// the link partner.
void OnlineBattle_LinkPartnerHandleExpUpdate(u32 battler)
{
    u8 monId = gBattleResources->bufferA[battler][1];
    u32 gainedExp = T1_READ_32(&gBattleResources->bufferA[battler][2]);
    struct Pokemon *mon = &gPlayerParty[monId];
    u8 level = GetMonData(mon, MON_DATA_LEVEL);

    if (IsOnlineBattle() && level < MAX_LEVEL)
    {
        u16 species = GetMonData(mon, MON_DATA_SPECIES);
        u32 currExp = GetMonData(mon, MON_DATA_EXP);
        u32 nextLvlExp = gExperienceTables[gSpeciesInfo[species].growthRate][level + 1];

        if (currExp + gainedExp >= nextLvlExp)
        {
            SetMonData(mon, MON_DATA_EXP, &nextLvlExp);
            CalculateMonStats(mon);
        }
        else
        {
            currExp += gainedExp;
            SetMonData(mon, MON_DATA_EXP, &currExp);
        }

        if (gBattlerPartyIndexes[battler] == monId)
            UpdateHealthboxAttribute(gHealthboxSpriteIds[battler], mon, HEALTHBOX_ALL);
    }

    BattleControllerComplete(battler);
}

// Learning moves: the host's side
//
// The partner's Pokémon are asked about on the partner's screen, with
// CONTROLLER_ONLINELEARNMOVE.

// Online, both games hold a copy of every Pokémon in the battle. Push a
// Pokémon's moves to both so the copies match when parties are restored.
void OnlineBattle_SyncMoves(u32 monId)
{
    struct MovePpInfo moveData;
    u32 i, battler;

    if (!IsOnlineBattle())
        return;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        moveData.moves[i] = GetMonData(&gPlayerParty[monId], MON_DATA_MOVE1 + i);
        moveData.pp[i] = GetMonData(&gPlayerParty[monId], MON_DATA_PP1 + i);
    }
    moveData.ppBonuses = GetMonData(&gPlayerParty[monId], MON_DATA_PP_BONUSES);

    battler = OnlineBattle_IsPartnerMon(monId) ? GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT) : GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
    BtlController_EmitSetMonData(battler, BUFFER_A, REQUEST_MOVES_PP_BATTLE, gBitTable[monId], sizeof(moveData), &moveData);
    MarkBattlerForControllerExec(battler);
}

static void SendLearnMovePrompt(u8 mode)
{
    u32 battler = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);

    gBattleResources->bufferB[battler][0] = 0;
    BtlController_EmitOnlineLearnMove(battler, BUFFER_A, mode, gBattleStruct->expGetterMonId, gMoveToLearn);
    MarkBattlerForControllerExec(battler);
}

static u16 GetLearnMoveReply(void)
{
    u32 battler = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);

    return gBattleResources->bufferB[battler][1] | (gBattleResources->bufferB[battler][2] << 8);
}

// Cmd_yesnoboxlearnmove for the partner's Pokémon. Returns FALSE for anyone else's.
bool32 OnlineBattle_YesNoBoxLearnMove(const u8 *forgotMovePtr, const u8 *nextInstr)
{
    if (!OnlineBattle_IsPartnerMon(gBattleStruct->expGetterMonId))
        return FALSE;

    switch (gBattleScripting.learnMoveState)
    {
    case 0:
        SendLearnMovePrompt(ONLINE_LEARN_MOVE_ASK);
        gBattleScripting.learnMoveState++;
        break;
    case 1:
        if (gBattleControllerExecFlags == 0)
        {
            u16 movePosition = GetLearnMoveReply();

            if (movePosition >= MAX_MON_MOVES)
            {
                gBattlescriptCurrInstr = nextInstr;
            }
            else if (IsMoveHM(GetMonData(&gPlayerParty[gBattleStruct->expGetterMonId], MON_DATA_MOVE1 + movePosition)))
            {
                PrepareStringBattle(STRINGID_HMMOVESCANTBEFORGOTTEN, B_POSITION_PLAYER_LEFT);
                gBattleScripting.learnMoveState = 2;
            }
            else
            {
                gBattlescriptCurrInstr = forgotMovePtr;
                ReplaceMoveWithMoveToLearn(movePosition);
            }
        }
        break;
    case 2:
        // After the HM message, go back to picking a move like the vanilla flow
        if (gBattleControllerExecFlags == 0)
        {
            SendLearnMovePrompt(ONLINE_LEARN_MOVE_PICK);
            gBattleScripting.learnMoveState = 1;
        }
        break;
    }
    return TRUE;
}

// Cmd_yesnoboxstoplearningmove for the partner's Pokémon. Returns FALSE for anyone else's.
bool32 OnlineBattle_YesNoBoxStopLearningMove(const u8 *nextInstr, const u8 *noInstr)
{
    if (!OnlineBattle_IsPartnerMon(gBattleStruct->expGetterMonId))
        return FALSE;

    if (gBattleScripting.learnMoveState == 0)
    {
        SendLearnMovePrompt(ONLINE_LEARN_MOVE_ASK_STOP);
        gBattleScripting.learnMoveState++;
    }
    else if (gBattleControllerExecFlags == 0)
    {
        // The reply is TRUE if they chose to stop learning
        if (GetLearnMoveReply())
            gBattlescriptCurrInstr = nextInstr;
        else
            gBattlescriptCurrInstr = noInstr;
    }
    return TRUE;
}

// Learning moves: the Pokémon owner's side
//
// The host asks this game's player about moves their own Pokémon want to
// learn. The reply is the move slot to forget, MAX_MON_MOVES for "don't learn",
// or TRUE/FALSE for "stop learning?".

static EWRAM_DATA u8 sLearnMode = 0;
static EWRAM_DATA u8 sLearnMonId = 0;
static EWRAM_DATA u16 sLearnMove = MOVE_NONE;
static EWRAM_DATA u8 sLearnCursor = 0;

static void LearnMove_Reply(u32 battler, u16 value)
{
    BtlController_EmitOneReturnValue(battler, BUFFER_B, value);
    BattleControllerComplete(battler);
}

static void LearnMove_WaitForBattleScreen(u32 battler)
{
    if (!gPaletteFade.active && gMain.callback2 == BattleMainCB2)
        LearnMove_Reply(battler, GetMoveSlotToReplace());
}

// Like Cmd_yesnoboxlearnmove, wait for the battle screen twice after the
// summary screen closes before reading the chosen slot
static void LearnMove_WaitForSummaryScreen(u32 battler)
{
    if (!gPaletteFade.active && gMain.callback2 == BattleMainCB2)
        gBattlerControllerFuncs[battler] = LearnMove_WaitForBattleScreen;
}

static void LearnMove_OpenSummaryScreen(u32 battler)
{
    if (!gPaletteFade.active)
    {
        FreeAllWindowBuffers();
        // The Pokémon may sit in slots 3-5 of the battle party, past gPlayerPartyCount
        ShowSelectMovePokemonSummaryScreen(gPlayerParty, sLearnMonId, PARTY_SIZE - 1, ReshowBattleScreenAfterMenu, sLearnMove);
        gBattlerControllerFuncs[battler] = LearnMove_WaitForSummaryScreen;
    }
}

static void LearnMove_HandleYesNo(u32 battler)
{
    bool32 chose = FALSE, yes = FALSE;

    if (JOY_NEW(DPAD_UP) && sLearnCursor != 0)
    {
        PlaySE(SE_SELECT);
        BattleDestroyYesNoCursorAt(sLearnCursor);
        sLearnCursor = 0;
        BattleCreateYesNoCursorAt(0);
    }
    if (JOY_NEW(DPAD_DOWN) && sLearnCursor == 0)
    {
        PlaySE(SE_SELECT);
        BattleDestroyYesNoCursorAt(sLearnCursor);
        sLearnCursor = 1;
        BattleCreateYesNoCursorAt(1);
    }
    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        chose = TRUE;
        yes = (sLearnCursor == 0);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        chose = TRUE;
    }

    if (!chose)
        return;

    HandleBattleWindow(YESNOBOX_X_Y, WINDOW_CLEAR);
    if (sLearnMode == ONLINE_LEARN_MOVE_ASK_STOP)
    {
        LearnMove_Reply(battler, yes);
    }
    else if (yes)
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gBattlerControllerFuncs[battler] = LearnMove_OpenSummaryScreen;
    }
    else
    {
        LearnMove_Reply(battler, MAX_MON_MOVES);
    }
}

// Handles CONTROLLER_ONLINELEARNMOVE for the player
void OnlineBattle_PlayerHandleLearnMove(u32 battler)
{
    sLearnMode = gBattleResources->bufferA[battler][1];
    sLearnMonId = gBattleResources->bufferA[battler][2];
    sLearnMove = gBattleResources->bufferA[battler][3] | (gBattleResources->bufferA[battler][4] << 8);

    if (sLearnMode == ONLINE_LEARN_MOVE_PICK)
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gBattlerControllerFuncs[battler] = LearnMove_OpenSummaryScreen;
        return;
    }

    HandleBattleWindow(YESNOBOX_X_Y, 0);
    BattlePutTextOnWindow(gText_BattleYesNoChoice, B_WIN_YESNO);
    sLearnCursor = 0;
    BattleCreateYesNoCursorAt(0);
    gBattlerControllerFuncs[battler] = LearnMove_HandleYesNo;
}
