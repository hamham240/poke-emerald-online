#ifndef GUARD_MULTIPLAYER_H
#define GUARD_MULTIPLAYER_H

extern u8 gMultiplayerAvatarObjId;
extern u8 gMultiplayerAvatarSpriteId;

enum { // Buffer Sizes
    GENERAL_BUFFER_SIZE = 0x00F00000
};

enum { // Addresses
    GENERAL_BUFFER_BEGIN_ADDRESS = 0x10000000,

    GENERAL_BUFFER_BEGIN_PERSONAL_ADDRESS = 0x10000001, // Where the local client can place data
    GENERAL_BUFFER_BEGIN_PEER_ADDRESS = 0x10000041, // Where the emulator will place the peer's data

    GENERAL_BUFFER_END_ADDRESS = (GENERAL_BUFFER_BEGIN_ADDRESS + GENERAL_BUFFER_SIZE)
};

struct MultiplayerPacket {
    u8 mapGroup, mapNum;
    s16 x, y;
    u8 movementActionId;
    u16 facingDirection;
};

void InitMultiplayerAvatarIds(void);
void ResetMultiplayerAvatarIds(void);
u8 ReadConnectedByte(void);
void SpawnMultiplayerAvatar(struct MultiplayerPacket* multiplayerPacket);
void TrySpawnMultiplayerAvatar(void);
void TryMoveMultiplayerSprite(void);
void WriteMultiplayerPacketToBuffer(void);
struct MultiplayerPacket* GetPeerPacket(void);

#endif
