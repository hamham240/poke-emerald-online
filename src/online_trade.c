#include "global.h"
#include "bg.h"
#include "field_screen_effect.h"
#include "field_weather.h"
#include "link.h"
#include "load_save.h"
#include "mail.h"
#include "main.h"
#include "malloc.h"
#include "online_link.h"
#include "online_session.h"
#include "online_trade.h"
#include "overworld.h"
#include "palette.h"
#include "task.h"
#include "trade.h"
#include "constants/trade.h"

// Trades
//
// The Trade Center's trade menu, run over the online link, with hooks in
// trade.c. It returns to the field once both players leave the menu; each
// trade in between saves both games, as in vanilla.

static EWRAM_DATA bool8 sIsOnlineTrade = FALSE;

bool32 IsOnlineTrade(void)
{
    return sIsOnlineTrade;
}

// Both players agreed to trade. The partner may be into the trade menu before
// this player's script gets going (it can still be printing), so its party
// must be held for the trade menu from now on rather than dropped.
void OnlineTrade_Prepare(void)
{
    sIsOnlineTrade = TRUE;
}

void OnlineTrade_End(void)
{
    sIsOnlineTrade = FALSE;
}

static void Task_StartOnlineTrade(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_StartCreateTradeMenu);
        DestroyTask(taskId);
    }
}

// Run by both players once the handshake says go (use waitstate)
void OnlineTrade_Start(void)
{
    Online_RestoreLinkPlayers();
    sIsOnlineTrade = TRUE;
    gLinkType = LINKTYPE_TRADE_SETUP;
    gSelectedTradeMonPositions[TRADE_PLAYER] = 0;
    gSelectedTradeMonPositions[TRADE_PARTNER] = 0;
    FadeScreen(FADE_TO_BLACK, 0);
    CreateTask(Task_StartOnlineTrade, 80);
}

// Back to where the player was standing, to finish the script that started
// the trade. The trade menu replaced the overworld's callback1, which handles
// the player's input.
void OnlineTrade_ReturnToField(void)
{
    SetMainCallback1(CB1_Overworld);
    OnlineTrade_End();
    SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
}

// The partner's game is gone mid-trade; trade.c has freed its resources. A
// trade whose save didn't finish stays in memory only, as when a cable is
// pulled; the save is only valid once both games have written it.
void OnlineTrade_Abort(void)
{
    DebugPrintf("Online trade: link lost, leaving the trade");
    gSoftResetDisabled = FALSE;
    Online_SetLinkLost();
    OnlineTrade_ReturnToField();
}

// Continuing the save puts the player back where they traded. The cable trade
// goes to the dynamic warp, set on entering the Cable Club.
void OnlineTrade_SetContinueGameWarp(void)
{
    SetContinueGameWarp(gSaveBlock1Ptr->location.mapGroup, gSaveBlock1Ptr->location.mapNum, WARP_ID_NONE, gSaveBlock1Ptr->pos.x, gSaveBlock1Ptr->pos.y);
    SetContinueGameWarpStatus();
}

// Everything BufferTradeParties exchanges, as one online message
struct OnlineTradeParty
{
    struct Pokemon party[PARTY_SIZE];
    struct Mail mail[PARTY_SIZE];
    u8 giftRibbons[GIFT_RIBBONS_COUNT];
};

// Online, the parties are swapped in one message each way. The block requests
// the cable trade uses rely on both players running in step: the partner's
// block can arrive before the menu is ready for it and be cleared. Returns
// TRUE once the partner's party is in gEnemyParty.
bool32 OnlineTrade_BufferParties(u8 *state, u8 *giftRibbons)
{
    struct OnlineTradeParty *data;

    switch (*state)
    {
    case 0:
        data = AllocZeroed(sizeof(*data));
        memcpy(data->party, gPlayerParty, sizeof(data->party));
        memcpy(data->mail, gSaveBlock1Ptr->mail, sizeof(data->mail));
        memcpy(data->giftRibbons, gSaveBlock1Ptr->giftRibbons, sizeof(data->giftRibbons));
        if (OnlineLink_Send(ONLINE_MSG_TRADE_PARTY, data, sizeof(*data)))
            (*state)++;
        Free(data);
        break;
    case 1:
        // Left at the front of the inbox by Online_UpdateLink
        if (OnlineLink_PeekType() == ONLINE_MSG_TRADE_PARTY)
        {
            data = AllocZeroed(sizeof(*data));
            OnlineLink_Receive(data, sizeof(*data));
            memcpy(gEnemyParty, data->party, sizeof(data->party));
            memcpy(gTradeMail, data->mail, sizeof(data->mail));
            memcpy(giftRibbons, data->giftRibbons, GIFT_RIBBONS_COUNT);
            Free(data);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
