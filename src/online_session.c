#include "global.h"
#include "battle.h"
#include "characters.h"
#include "decompress.h"
#include "event_data.h"
#include "link.h"
#include "multiplayer.h"
#include "online_interact.h"
#include "online_battle.h"
#include "online_link.h"
#include "online_session.h"
#include "online_trade.h"
#include "constants/trainers.h"

// The vanilla link API (blocks, standby, link players) carried over the
// mgba-online message pipe instead of the serial cable. link.c hands its calls
// here while OnlineLink_IsConnected(), and Online_UpdateLink runs every frame
// from the main loop, delivering the partner's messages.

static bool8 sOnlineWasConnected;
static bool8 sOnlineWaitingForStandby;
static bool8 sOnlineClosingLink;
static bool8 sOnlineResendLinkPlayer;
static u8 sOnlineLinkPlayerRequestTimer;
static u16 sOnlinePeerReadyTrainer;
static u16 sOnlinePeerCommittedTrainer;
static EWRAM_DATA bool8 sLinkLost = FALSE;

// Called once at boot
void Online_Init(void)
{
    InitMultiplayerAvatarIds();
    OnlineBattle_Init();
}

static void Online_ReceiveInto(u32 who, u16 size)
{
    void *dest;

    if (size > sizeof(gDecompressionBuffer))
    {
        DebugPrintf("Online link: dropping %u byte block from player %u, too large", size, who);
        OnlineLink_Receive(NULL, 0);
        return;
    }

    // Same rule as the cable: blocks larger than the block buffer land in the
    // decompression buffer.
    if (size > BLOCK_BUFFER_SIZE)
        dest = gDecompressionBuffer;
    else
        dest = gBlockRecvBuffer[who];

    OnlineLink_Receive(dest, size);

    // The VBlank handler reads gBlockRecvBuffer as soon as the flag is set,
    // so the copy has to land before the flag does.
    asm volatile("" ::: "memory");
    gBlockReceivedStatus[who] = TRUE;
}

bool8 Online_SendBlock(const void *src, u16 size)
{
    u8 myId = GetMultiplayerId();

    if (!OnlineLink_Send(ONLINE_MSG_BLOCK, src, size))
        return FALSE;

    // A cable delivers every block to its sender too; callers wait on
    // GetBlockReceivedStatus() including their own bit.
    if (size > BLOCK_BUFFER_SIZE)
        memcpy(gDecompressionBuffer, src, size);
    else
        memcpy(gBlockRecvBuffer[myId], src, size);
    asm volatile("" ::: "memory");
    gBlockReceivedStatus[myId] = TRUE;
    return TRUE;
}

// The partner's link player info as received. Vanilla code that runs between
// receiving it and using it can overwrite gLinkPlayers (seen when linking at
// the title screen), and battles rearrange it, so online activities restore
// it from here (Online_RestoreLinkPlayers).
static EWRAM_DATA struct LinkPlayer sOnlinePeerLinkPlayer = {0};

// The trainer info last sent to the partner, to resend it when it changes
static EWRAM_DATA u8 sOnlineSentName[PLAYER_NAME_LENGTH] = {0};
static EWRAM_DATA u8 sOnlineSentTrainerId[TRAINER_ID_LENGTH] = {0};
static EWRAM_DATA u8 sOnlineSentGender = 0;

static bool32 Online_HasTrainerInfoChanged(void)
{
    return memcmp(sOnlineSentName, gSaveBlock2Ptr->playerName, sizeof(sOnlineSentName)) != 0
        || memcmp(sOnlineSentTrainerId, gSaveBlock2Ptr->playerTrainerId, sizeof(sOnlineSentTrainerId)) != 0
        || sOnlineSentGender != gSaveBlock2Ptr->playerGender;
}

// Puts both players' info back in gLinkPlayers for an online activity
void Online_RestoreLinkPlayers(void)
{
    u8 myId = GetMultiplayerId();

    if (!OnlineLink_IsConnected())
        return;

    InitLocalLinkPlayer();
    gLocalLinkPlayer.id = myId;
    gLinkPlayers[myId] = gLocalLinkPlayer;
    gLinkPlayers[myId ^ 1] = sOnlinePeerLinkPlayer;
    gLinkPlayers[myId ^ 1].id = myId ^ 1;
}

static void Online_SendLinkPlayer(void)
{
    u8 myId = GetMultiplayerId();

    memcpy(sOnlineSentName, gSaveBlock2Ptr->playerName, sizeof(sOnlineSentName));
    memcpy(sOnlineSentTrainerId, gSaveBlock2Ptr->playerTrainerId, sizeof(sOnlineSentTrainerId));
    sOnlineSentGender = gSaveBlock2Ptr->playerGender;

    InitLocalLinkPlayer();
    gLocalLinkPlayer.id = myId;
    gLinkPlayers[myId] = gLocalLinkPlayer;
    OnlineLink_Send(ONLINE_MSG_LINK_PLAYER, &gLocalLinkPlayer, sizeof(gLocalLinkPlayer));
}

void Online_EnterStandby(void)
{
    gReadyToExitStandby[GetMultiplayerId()] = TRUE;
    sOnlineWaitingForStandby = TRUE;
    OnlineLink_Send(ONLINE_MSG_STANDBY, NULL, 0);
}

// Also waits for our own echoed block to be consumed, so the next send can't
// overwrite it
bool32 Online_IsLinkTaskFinished(void)
{
    return !sOnlineWaitingForStandby
        && !sOnlineClosingLink
        && OnlineLink_IsSendQueueEmpty()
        && !gBlockReceivedStatus[GetMultiplayerId()];
}

static void Online_UpdateStandby(void)
{
    u32 i;

    if (!sOnlineWaitingForStandby)
        return;

    for (i = 0; i < ONLINE_LINK_PLAYER_COUNT; i++)
    {
        if (!gReadyToExitStandby[i])
            return;
    }

    for (i = 0; i < MAX_LINK_PLAYERS; i++)
        gReadyToExitStandby[i] = FALSE;
    sOnlineWaitingForStandby = FALSE;
}

// The cable version tells every player it is done, waits for all of them, then
// shuts the link down. Online, the connection stays up; "closing" only clears
// gReceivedRemoteLinkPlayers (which is what callers wait on) and the link
// players are exchanged again for the next link activity.
void Online_ReadyCloseLink(void)
{
    if (sOnlineClosingLink)
        return;

    gReadyToCloseLink[GetMultiplayerId()] = TRUE;
    sOnlineClosingLink = TRUE;
    OnlineLink_Send(ONLINE_MSG_CLOSE_LINK, NULL, 0);
}

static bool32 Online_TryFinishCloseLink(void)
{
    u32 i;

    if (!sOnlineClosingLink)
        return FALSE;

    for (i = 0; i < ONLINE_LINK_PLAYER_COUNT; i++)
    {
        if (!gReadyToCloseLink[i])
            return FALSE;
    }

    for (i = 0; i < MAX_LINK_PLAYERS; i++)
        gReadyToCloseLink[i] = FALSE;
    sOnlineClosingLink = FALSE;
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK_IN_BATTLE;
    gReceivedRemoteLinkPlayers = FALSE;
    sOnlineResendLinkPlayer = TRUE;
    return TRUE;
}

void OnlinePair_Send(u8 type, u16 trainerId)
{
    OnlineLink_Send(type, &trainerId, sizeof(trainerId));
}

u16 OnlinePair_GetPeerReady(void)
{
    return sOnlinePeerReadyTrainer;
}

u16 OnlinePair_GetPeerCommitted(void)
{
    return sOnlinePeerCommittedTrainer;
}

// Called by both players as their battle starts, so the next pairing starts
// from the partner's next messages
void OnlinePair_ClearPeer(void)
{
    sOnlinePeerReadyTrainer = TRAINER_NONE;
    sOnlinePeerCommittedTrainer = TRAINER_NONE;
}

static void Online_HandlePairMessage(u8 type, u16 trainerId)
{
    switch (type)
    {
    case ONLINE_MSG_PAIR_READY:
        sOnlinePeerReadyTrainer = trainerId;
        sOnlinePeerCommittedTrainer = TRAINER_NONE;
        break;
    case ONLINE_MSG_PAIR_COMMIT:
        sOnlinePeerReadyTrainer = trainerId;
        sOnlinePeerCommittedTrainer = trainerId;
        break;
    case ONLINE_MSG_PAIR_CANCEL:
        OnlinePair_ClearPeer();
        break;
    }
}

static void Online_Reset(void)
{
    u32 i;

    for (i = 0; i < MAX_LINK_PLAYERS; i++)
    {
        gBlockReceivedStatus[i] = FALSE;
        gReadyToExitStandby[i] = FALSE;
        gReadyToCloseLink[i] = FALSE;
    }
    sOnlineWaitingForStandby = FALSE;
    sOnlineClosingLink = FALSE;
    sOnlineResendLinkPlayer = FALSE;
    sOnlineLinkPlayerRequestTimer = 0;
    gReceivedRemoteLinkPlayers = FALSE;

    // The partner's info is unknown until they send it. Never leave a name
    // without its EOS for the screens that show it.
    for (i = 0; i < MAX_LINK_PLAYERS; i++)
    {
        if (i == GetMultiplayerId())
            continue;
        memset(&gLinkPlayers[i], 0, sizeof(gLinkPlayers[i]));
        gLinkPlayers[i].name[0] = EOS;
    }
    memset(&sOnlinePeerLinkPlayer, 0, sizeof(sOnlinePeerLinkPlayer));
    sOnlinePeerLinkPlayer.name[0] = EOS;

    OnlinePair_ClearPeer();
    Multiplayer_OnLinkReset();
    OnlineInteract_OnDisconnect();
}

// Called once per frame from the main loop, in place of HandleLinkConnection.
void Online_UpdateLink(void)
{
    u8 peerId;
    u8 type;

    if (!OnlineLink_IsConnected())
    {
        if (sOnlineWasConnected)
        {
            DebugPrintf("Online link: disconnected");
            Online_Reset();
            sOnlineWasConnected = FALSE;
        }
        return;
    }

    if (!sOnlineWasConnected)
    {
        DebugPrintf("Online link: connected as player %u", GetMultiplayerId());
        Online_Reset();
        Online_SendLinkPlayer();
        OnlineLink_Send(ONLINE_MSG_LINK_PLAYER_REQUEST, NULL, 0);
        sOnlineWasConnected = TRUE;
    }

    // Without the partner's info (e.g. this game restarted while the link
    // stayed up, so the partner never saw a new connection), ask for it.
    // Not while a link close is in progress, which waits for the partner to
    // resend it on its own.
    if (!gReceivedRemoteLinkPlayers && !sOnlineClosingLink && !sOnlineResendLinkPlayer)
    {
        if (++sOnlineLinkPlayerRequestTimer >= 60)
        {
            OnlineLink_Send(ONLINE_MSG_LINK_PLAYER_REQUEST, NULL, 0);
            sOnlineLinkPlayerRequestTimer = 0;
        }
    }
    else
    {
        sOnlineLinkPlayerRequestTimer = 0;
    }

    // Sent a frame after the link closes, so whatever is waiting for
    // gReceivedRemoteLinkPlayers to clear gets to see it. Also resent when
    // the trainer info changes: linking at the title screen sends a blank
    // save's, which loading a save or starting a new game replaces.
    if (sOnlineResendLinkPlayer || (!sOnlineClosingLink && Online_HasTrainerInfoChanged()))
    {
        Online_SendLinkPlayer();
        sOnlineResendLinkPlayer = FALSE;
    }

    peerId = GetMultiplayerId() ^ 1;

    while ((type = OnlineLink_PeekType()) != ONLINE_MSG_NONE)
    {
        switch (type)
        {
        case ONLINE_MSG_LINK_PLAYER:
            OnlineLink_Receive(&gLinkPlayers[peerId], sizeof(gLinkPlayers[peerId]));
            gLinkPlayers[peerId].id = peerId;
            gLinkPlayers[peerId].name[PLAYER_NAME_LENGTH] = EOS;
            sOnlinePeerLinkPlayer = gLinkPlayers[peerId];
            gReceivedRemoteLinkPlayers = TRUE;
            // Sent when the partner (re)connects, e.g. after a soft reset, and
            // after each link battle. Any pairing or request they had is gone.
            OnlinePair_ClearPeer();
            OnlineInteract_OnLinkReset();
            break;
        case ONLINE_MSG_BLOCK:
            // Keep blocks in order: wait until the previous one from this
            // player has been consumed.
            if (gBlockReceivedStatus[peerId])
                goto done;
            Online_ReceiveInto(peerId, OnlineLink_PeekSize());
            break;
        case ONLINE_MSG_STANDBY:
            OnlineLink_Receive(NULL, 0);
            gReadyToExitStandby[peerId] = TRUE;
            break;
        case ONLINE_MSG_PAIR_READY:
        case ONLINE_MSG_PAIR_COMMIT:
        case ONLINE_MSG_PAIR_CANCEL:
        {
            u16 trainerId = TRAINER_NONE;

            OnlineLink_Receive(&trainerId, sizeof(trainerId));
            Online_HandlePairMessage(type, trainerId);
            break;
        }
        case ONLINE_MSG_LINK_PLAYER_REQUEST:
            OnlineLink_Receive(NULL, 0);
            // A close in progress resends it once done
            if (!sOnlineClosingLink)
                Online_SendLinkPlayer();
            break;
        case ONLINE_MSG_AVATAR_STATE:
            Multiplayer_ReceiveAvatarState();
            break;
        case ONLINE_MSG_AVATAR_EVENT:
            Multiplayer_ReceiveAvatarEvent();
            break;
        case ONLINE_MSG_INTERACT:
            OnlineInteract_Receive();
            break;
        case ONLINE_MSG_PARTNER_PARTY:
            OnlineBattle_ReceivePartnerParty();
            break;
        case ONLINE_MSG_TRADE_PARTY:
            // Read by the trade menu once it's ready for it. Everything after
            // it waits too, but the partner sends nothing else meanwhile.
            if (IsOnlineTrade())
                goto done;
            OnlineLink_Receive(NULL, 0);
            break;
        case ONLINE_MSG_CLOSE_LINK:
            OnlineLink_Receive(NULL, 0);
            gReadyToCloseLink[peerId] = TRUE;
            // Stop here so the link reads as closed for at least one frame
            // before the peer's next link player message reopens it.
            if (Online_TryFinishCloseLink())
                goto done;
            break;
        default:
            DebugPrintf("Online link: dropping message with unknown type %u", type);
            OnlineLink_Receive(NULL, 0);
            break;
        }
    }
    Online_TryFinishCloseLink();
done:
    Online_UpdateStandby();
}

// Losing the link mid-activity
//
// A co-op battle, duel or trade can't go on once the partner's game is gone.
// They end early and return to the field (see OnlineBattle_TryAbortDisconnected
// and TryAbortDisconnectedOnlineTrade), noting it here for the script to tell
// the player.

void Online_SetLinkLost(void)
{
    sLinkLost = TRUE;
}

bool32 Online_WasLinkLost(void)
{
    return sLinkLost;
}

// VAR_RESULT = whether the last online activity ended because the link was lost
void Online_CheckLinkLost(void)
{
    gSpecialVar_Result = sLinkLost;
    sLinkLost = FALSE;
}
