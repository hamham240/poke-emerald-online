#include "global.h"
#include "characters.h"
#include "event_data.h"
#include "link.h"
#include "main.h"
#include "online_interact.h"
#include "online_link.h"
#include "online_session.h"
#include "online_trade.h"
#include "overworld.h"
#include "script.h"
#include "string_util.h"
#include "task.h"

// Lets one online player ask the other to battle or trade, by pressing A on
// their avatar. The handshake over ONLINE_MSG_INTERACT:
//
//   Requester               Partner
//   REQUEST  ------------>  (prompt shown once they're free)
//            <------------  ACCEPT or DECLINE
//   START    ------------>
//            <------------  ACK
//   both go                 both go
//
// The requester can back out (CANCEL) until it sends START; the partner can't
// once it has accepted. START is only sent to a partner that accepted, and the
// requester only goes once it's been acknowledged, so neither player can end up
// starting alone. If both players ask each other for the same thing at once,
// the host treats the joiner's request as an acceptance.

enum
{
    INTERACT_OP_REQUEST,
    INTERACT_OP_ACCEPT,
    INTERACT_OP_DECLINE,
    INTERACT_OP_START,
    INTERACT_OP_ACK,
    INTERACT_OP_CANCEL,
};

struct OnlineInteractMsg
{
    u8 op;
    u8 kind;
};

enum
{
    INTERACT_STATE_IDLE,
    INTERACT_STATE_REQUESTING, // Asked the partner, waiting for an answer
    INTERACT_STATE_ACCEPTED,   // Accepted the partner's request, waiting for START
    INTERACT_STATE_STARTING,   // Sent START, waiting for ACK
    INTERACT_STATE_GO,
    INTERACT_STATE_DECLINED,
    INTERACT_STATE_CANCELLED,  // The partner backed out
};

static EWRAM_DATA u8 sState = INTERACT_STATE_IDLE;
static EWRAM_DATA u8 sKind = ONLINE_INTERACT_NONE;
static EWRAM_DATA u8 sIncomingKind = ONLINE_INTERACT_NONE; // A request the player hasn't seen yet
static EWRAM_DATA bool8 sIncomingCancelled = FALSE;
static EWRAM_DATA bool8 sIncomingShown = FALSE;

static void Send(u8 op, u8 kind)
{
    struct OnlineInteractMsg msg = {.op = op, .kind = kind};

    OnlineLink_Send(ONLINE_MSG_INTERACT, &msg, sizeof(msg));
}

// Both players are in. The partner may be into the trade menu before this
// player's script gets going (it can still be printing), so its party must be
// held for the trade menu from now on rather than dropped.
static void Go(void)
{
    sState = INTERACT_STATE_GO;
    if (sKind == ONLINE_INTERACT_TRADE)
        OnlineTrade_Prepare();
}

static void ClearIncomingRequest(void)
{
    sIncomingKind = ONLINE_INTERACT_NONE;
    sIncomingCancelled = FALSE;
    sIncomingShown = FALSE;
}

// Called from Online_UpdateLink with an ONLINE_MSG_INTERACT at the front of the
// inbox.
void OnlineInteract_Receive(void)
{
    struct OnlineInteractMsg msg = {0};

    OnlineLink_Receive(&msg, sizeof(msg));

    switch (msg.op)
    {
    case INTERACT_OP_REQUEST:
        if (sState == INTERACT_STATE_REQUESTING && msg.kind == sKind)
        {
            // Both asked at once. The host goes ahead; the joiner waits for its START.
            if (GetMultiplayerId() == 0)
            {
                Send(INTERACT_OP_START, sKind);
                sState = INTERACT_STATE_STARTING;
            }
        }
        else if (sState == INTERACT_STATE_IDLE && sIncomingKind == ONLINE_INTERACT_NONE)
        {
            sIncomingKind = msg.kind;
            sIncomingCancelled = FALSE;
            sIncomingShown = FALSE;
        }
        else
        {
            Send(INTERACT_OP_DECLINE, msg.kind);
        }
        break;
    case INTERACT_OP_ACCEPT:
        if (sState == INTERACT_STATE_REQUESTING && msg.kind == sKind)
        {
            Send(INTERACT_OP_START, sKind);
            sState = INTERACT_STATE_STARTING;
        }
        else
        {
            // No longer asking; the partner is waiting on us
            Send(INTERACT_OP_CANCEL, msg.kind);
        }
        break;
    case INTERACT_OP_DECLINE:
        if (sState == INTERACT_STATE_REQUESTING && msg.kind == sKind)
            sState = INTERACT_STATE_DECLINED;
        break;
    case INTERACT_OP_START:
        if ((sState == INTERACT_STATE_ACCEPTED || sState == INTERACT_STATE_REQUESTING) && msg.kind == sKind)
        {
            Send(INTERACT_OP_ACK, sKind);
            Go();
        }
        else
        {
            Send(INTERACT_OP_CANCEL, msg.kind);
        }
        break;
    case INTERACT_OP_ACK:
        if (sState == INTERACT_STATE_STARTING && msg.kind == sKind)
            Go();
        break;
    case INTERACT_OP_CANCEL:
        if (sIncomingKind == msg.kind)
            sIncomingCancelled = TRUE;
        if ((sState == INTERACT_STATE_ACCEPTED || sState == INTERACT_STATE_STARTING) && msg.kind == sKind)
            sState = INTERACT_STATE_CANCELLED;
        break;
    }
}

// Called when the partner's link player info arrives: when they (re)connect or
// restart, and after every link close (battles, trades). Only cancels a
// handshake in progress; a trade carries on through its link closes.
void OnlineInteract_OnLinkReset(void)
{
    if (sState != INTERACT_STATE_IDLE)
        sState = INTERACT_STATE_CANCELLED;
    ClearIncomingRequest();
}

// Called when the link connects or drops
void OnlineInteract_OnDisconnect(void)
{
    OnlineInteract_OnLinkReset();
    OnlineTrade_End();
}

// Called from ProcessPlayerFieldInput, so the prompt only interrupts a player
// who is free to act
bool32 OnlineInteract_TryStartIncomingRequestScript(void)
{
    if (sIncomingKind == ONLINE_INTERACT_NONE || sIncomingShown || sState != INTERACT_STATE_IDLE)
        return FALSE;

    if (sIncomingCancelled || !OnlineLink_IsConnected())
    {
        ClearIncomingRequest();
        return FALSE;
    }

    sIncomingShown = TRUE;
    ScriptContext_SetupScript(EventScript_OnlineInteractRequested);
    return TRUE;
}

void OnlineInteract_BufferPartnerName(void)
{
    u8 peerId = GetMultiplayerId() ^ 1;

    Online_RestoreLinkPlayers();

    StringCopyN(gStringVar1, gLinkPlayers[peerId].name, PLAYER_NAME_LENGTH);
    gStringVar1[PLAYER_NAME_LENGTH] = EOS;
}

static void FinishWaiting(u8 taskId, u16 result)
{
    if (result != ONLINE_INTERACT_RESULT_START)
        sState = INTERACT_STATE_IDLE;
    gSpecialVar_Result = result;
    ScriptContext_Enable();
    DestroyTask(taskId);
}

static void Task_WaitForPartner(u8 taskId)
{
    if (!OnlineLink_IsConnected())
    {
        FinishWaiting(taskId, ONLINE_INTERACT_RESULT_DISCONNECTED);
        return;
    }

    switch (sState)
    {
    case INTERACT_STATE_GO:
        sState = INTERACT_STATE_IDLE;
        FinishWaiting(taskId, ONLINE_INTERACT_RESULT_START);
        break;
    case INTERACT_STATE_DECLINED:
        FinishWaiting(taskId, ONLINE_INTERACT_RESULT_DECLINED);
        break;
    case INTERACT_STATE_CANCELLED:
    case INTERACT_STATE_IDLE:
        FinishWaiting(taskId, ONLINE_INTERACT_RESULT_PARTNER_CANCELLED);
        break;
    case INTERACT_STATE_REQUESTING:
        if (JOY_NEW(B_BUTTON))
        {
            Send(INTERACT_OP_CANCEL, sKind);
            FinishWaiting(taskId, ONLINE_INTERACT_RESULT_CANCELLED);
        }
        break;
    }
}

// Waits for the handshake to finish (use waitstate). VAR_RESULT is an
// ONLINE_INTERACT_RESULT_*.
void OnlineInteract_WaitForPartner(void)
{
    CreateTask(Task_WaitForPartner, 80);
}

// Asks the partner for VAR_0x8004 (an ONLINE_INTERACT_*), then waits like
// OnlineInteract_WaitForPartner (use waitstate). B backs out while they haven't
// answered.
void OnlineInteract_SendRequest(void)
{
    u8 kind = gSpecialVar_0x8004;

    sKind = kind;
    if (!OnlineLink_IsConnected())
    {
        sState = INTERACT_STATE_CANCELLED;
    }
    else if (sIncomingKind == kind && !sIncomingCancelled && GetMultiplayerId() == 0)
    {
        // The partner already asked for the same thing; take that as a yes
        ClearIncomingRequest();
        Send(INTERACT_OP_START, kind);
        sState = INTERACT_STATE_STARTING;
    }
    else
    {
        // A pending request for the same thing is left to the host, which takes
        // our request as a yes. Anything else is turned down.
        if (sIncomingKind != ONLINE_INTERACT_NONE && sIncomingKind != kind && !sIncomingCancelled)
            Send(INTERACT_OP_DECLINE, sIncomingKind);
        ClearIncomingRequest();
        Send(INTERACT_OP_REQUEST, kind);
        sState = INTERACT_STATE_REQUESTING;
    }
    OnlineInteract_WaitForPartner();
}

// For the partner's prompt: VAR_0x8004 = what they asked for, STR_VAR_1 = their name
void OnlineInteract_BufferIncomingRequest(void)
{
    gSpecialVar_0x8004 = sIncomingKind;
    OnlineInteract_BufferPartnerName();
}

void OnlineInteract_Decline(void)
{
    if (!sIncomingCancelled && OnlineLink_IsConnected())
        Send(INTERACT_OP_DECLINE, sIncomingKind);
    ClearIncomingRequest();
}

// VAR_RESULT is FALSE if the partner has already backed out
void OnlineInteract_Accept(void)
{
    u8 kind = sIncomingKind;
    bool32 stillAsking = !sIncomingCancelled && OnlineLink_IsConnected() && sState == INTERACT_STATE_IDLE;

    ClearIncomingRequest();
    gSpecialVar_Result = stillAsking;
    if (stillAsking)
    {
        sKind = kind;
        Send(INTERACT_OP_ACCEPT, kind);
        sState = INTERACT_STATE_ACCEPTED;
    }
}
