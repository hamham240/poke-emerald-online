#ifndef GUARD_ONLINE_LINK_H
#define GUARD_ONLINE_LINK_H

#include "global.h"

// Message pipe to the peer, provided by mgba-online in the general buffer.
// Layout must match mgba-online's src/platform/qt/OnlineLink.h.
#define ONLINE_PIPE_BASE        0x10010000
#define ONLINE_PIPE_RING_SIZE   0x4000
#define ONLINE_PIPE_OUTBOX      (ONLINE_PIPE_BASE + 0x100)
#define ONLINE_PIPE_INBOX       (ONLINE_PIPE_OUTBOX + ONLINE_PIPE_RING_SIZE)
#define ONLINE_PIPE_MAGIC       0x4F42474D // "MGBO"

struct OnlinePipeHeader
{
    vu8 connected;
    vu8 playerId;
    u8 padding[2];
    vu32 outWrite;
    vu32 outRead;
    vu32 inWrite;
    vu32 inRead;
    vu32 magic;
};

enum
{
    ONLINE_MSG_NONE,
    ONLINE_MSG_LINK_PLAYER,
    ONLINE_MSG_BLOCK,
    ONLINE_MSG_STANDBY,
    ONLINE_MSG_CLOSE_LINK,
    ONLINE_MSG_PAIR_READY,
    ONLINE_MSG_PAIR_COMMIT,
    ONLINE_MSG_PAIR_CANCEL,
    ONLINE_MSG_AVATAR_STATE,
    ONLINE_MSG_AVATAR_EVENT,
    ONLINE_MSG_INTERACT,
    ONLINE_MSG_PARTNER_PARTY,
    ONLINE_MSG_TRADE_PARTY,
    ONLINE_MSG_LINK_PLAYER_REQUEST,
};

bool32 OnlineLink_IsConnected(void);
u8 OnlineLink_GetPlayerId(void);
bool32 OnlineLink_Send(u8 type, const void *data, u16 size);
bool32 OnlineLink_IsSendQueueEmpty(void);
u8 OnlineLink_PeekType(void);
u16 OnlineLink_PeekSize(void);
u16 OnlineLink_Receive(void *dest, u16 maxSize);

#endif // GUARD_ONLINE_LINK_H
