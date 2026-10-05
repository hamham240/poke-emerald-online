#ifndef GUARD_MULTIPLAYER_H
#define GUARD_MULTIPLAYER_H

#include "global.h"

extern u8 gMultiplayerAvatarObjId;
extern bool8 gDisableMonSelectCancel;

struct ObjectEvent;

// Local id of the partner's avatar object event
#define MULTIPLAYER_AVATAR_LOCAL_ID (OBJ_EVENT_ID_PLAYER - 1)

// ONLINE_MSG_AVATAR_STATE: where the player is and how they look
struct OnlineAvatarState {
    u8 mapGroup;
    u8 mapNum;
    s16 x;
    s16 y;
    u8 facingDirection;
    u8 graphicsId;
    u8 elevation;
    bool8 invisible;
};

enum {
    AVATAR_EVENT_MOVE,          // arg: movement action, at the tile it started from
    AVATAR_EVENT_SET_INVISIBLE, // arg: invisible
    AVATAR_EVENT_DOOR_SET_OPEN, // at the door tile
    AVATAR_EVENT_DOOR_OPEN,     // at the door tile
    AVATAR_EVENT_DOOR_CLOSE,    // at the door tile
};

// ONLINE_MSG_AVATAR_EVENT: something the player did, for their avatar to replay
struct OnlineAvatarEvent {
    u8 mapGroup;
    u8 mapNum;
    s16 x;
    s16 y;
    u8 kind;
    u8 arg;
};

void InitMultiplayerAvatarIds(void);
void InitMultiplayerData(void);
void ResetMultiplayerAvatarIds(void);
void Multiplayer_OnLinkReset(void);
bool32 IsOnlineBattle(void);
bool32 IsMultiplayerAvatar(const struct ObjectEvent *objectEvent);
bool32 IsMultiplayerAvatarAt(s16 x, s16 y);
void UpdateMultiplayerAvatar(void);
void Multiplayer_SendAvatarState(void);
void Multiplayer_SendPlayerMovement(const struct ObjectEvent *player, u8 movementActionId);
void Multiplayer_SendPlayerInvisibility(bool8 invisible);
void Multiplayer_SendDoorEvent(u8 kind, s16 x, s16 y);
void Multiplayer_ReceiveAvatarState(void);
void Multiplayer_ReceiveAvatarEvent(void);
void DisableMonSelectCancel(void);
void EnableMonSelectCancel(void);

#endif
