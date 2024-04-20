#ifndef GUARD_MULTIPLAYER_H
#define GUARD_MULTIPLAYER_H

#include "global.h"

extern u8 gMultiplayerAvatarObjId;
extern u8 gMultiplayerAvatarSpriteId;
extern bool8 gDisableMonSelectCancel;
extern bool8 gIsWaitingOnOtherPlayer;

struct Pokemon;

struct MultiplayerPacket {
    u8 mapGroup, mapNum;
    s16 x, y;
    u8 movementActionId;
    u16 trainerBattleOppA;
    bool8 isWaitingForOtherPlayer;
} __attribute__((packed));

void InitMultiplayerAvatarIds(void);
void InitMultiplayerData(void);
void ResetMultiplayerAvatarIds(void);
u8 ReadConnectedByte(void);
void SpawnMultiplayerAvatar(struct MultiplayerPacket* multiplayerPacket);
void TrySpawnMultiplayerAvatar(void);
void TryMoveMultiplayerSprite(void);
void WriteMultiplayerPacketToBuffer(void);
void WritePartyPacketToBuffer(void);
struct MultiplayerPacket* GetPeerPacket(void);
struct Pokemon* getPeerParty(void);

#endif
