#ifndef GUARD_MULTIPLAYER_H
#define GUARD_MULTIPLAYER_H

#include "global.h"

extern u8 gMultiplayerAvatarObjId;
extern u8 gMultiplayerAvatarSpriteId;
extern bool8 gDisableMonSelectCancel;
extern bool8 gIsWaitingOnOtherPlayer;
extern u32 gPacketIdCounter;
extern u32 gOldPacketId;
extern u32 gHasPeerFinishedTask;
extern u32 gMultiplayerExecFlags[4];

struct Pokemon;

struct MultiplayerPacket {
    u8 mapGroup, mapNum;
    s16 x, y;
    u8 movementActionId;
    u16 trainerBattleOppA;
    bool8 isWaitingForOtherPlayer;
} __attribute__((packed));

struct LinkPacket {
    u32 packetId;
    u32 receivedPacketId;
    u32 execCompleted;
    u32 playerId;
    u32 bufferId;
    u32 battler;
    u8 battlerAttacker;
    u8 battlerTarget;
    u8 absentBattlerFlags;
    u8 effectBattler;
    u8 data[512];
    u32 size;
} __attribute__((packed));

enum {
    TASK_FINISHED = 0,
    TASK_NOT_FINISHED = 1,
};

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
void DisableMonSelectCancel(void);
void EnableMonSelectCancel(void);
void WriteTransferDataToBuffer(u32 battler, u32 bufferId, u16 size, u8 *data, u32 packetId);
void writeExecCompleted(u32 execCompleted);
struct LinkPacket* getPeerLinkPacket(void);
u32 incrementPacketIdCounter(void);
void writeReceivedPacketId(u32 packetId);
u8 hasPeerReceivedLatestPacket(void);
u8 hasReceivedLatestPacket(u32 packetId);
void clearExecFlags(void);
u8 execFlagsAreCleared(void);
void markExecFlag(u32 battler, u32 value);
u32 getExecFlag(u32 battler);

#endif
