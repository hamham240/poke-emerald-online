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

#define MultiplayerPacket_TRUE_SIZE ( \
    sizeof(u8) \
    + sizeof(u8) \
    + sizeof(s16) \
    + sizeof(s16) \
    + sizeof(u8) \
    + sizeof(u16) \
    + sizeof(u16) \
    + sizeof(bool8) \
)

// TODO: Figure out what all I need to send to initiate a pokemon battle
//      Do I even need to send over all the information about EVs, IVs, etc.,
//      or should I just send over the sprite and during each turn just send over
//      which move the trainer used. Basically, narrow down what I have to send.
//      Let's not just send the entire world over.
struct PokemonPacket {
    u16 species;
    u8 level;
    u32 Ivs;
    u8 hasFixedPersonality;
    u32 fixedPersonality;
    u8 otIdType;
    u32 fixedOtId;
} __attribute__((packed));

extern struct PokemonPacket gPartyPacket[MULTI_PARTY_SIZE];

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
