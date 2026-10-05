#include "global.h"
#include "online_link.h"

// Each message in a ring is a u16 length, then a u8 type, then the payload.
// The length covers the type byte and the payload.
#define sPipe   ((struct OnlinePipeHeader *)ONLINE_PIPE_BASE)
#define sOutbox ((vu8 *)ONLINE_PIPE_OUTBOX)
#define sInbox  ((vu8 *)ONLINE_PIPE_INBOX)

static u8 RingRead(vu8 *ring, u32 index)
{
    return ring[index % ONLINE_PIPE_RING_SIZE];
}

static void RingWrite(vu8 *ring, u32 index, u8 value)
{
    ring[index % ONLINE_PIPE_RING_SIZE] = value;
}

bool32 OnlineLink_IsConnected(void)
{
    return sPipe->magic == ONLINE_PIPE_MAGIC && sPipe->connected != 0;
}

u8 OnlineLink_GetPlayerId(void)
{
    return sPipe->playerId;
}

// Queues a message for the peer. Returns FALSE if the outbox is full.
bool32 OnlineLink_Send(u8 type, const void *data, u16 size)
{
    const u8 *src = data;
    u32 write = sPipe->outWrite;
    u32 length = 1 + size;
    u32 i;

    if (ONLINE_PIPE_RING_SIZE - (write - sPipe->outRead) < 2 + length)
    {
        DebugPrintf("OnlineLink_Send: outbox full, dropping type %u message of %u bytes", type, size);
        return FALSE;
    }

    RingWrite(sOutbox, write, length & 0xFF);
    RingWrite(sOutbox, write + 1, length >> 8);
    RingWrite(sOutbox, write + 2, type);
    for (i = 0; i < size; i++)
        RingWrite(sOutbox, write + 3 + i, src[i]);

    // Publish only after the whole message is in the ring.
    sPipe->outWrite = write + 2 + length;
    return TRUE;
}

bool32 OnlineLink_IsSendQueueEmpty(void)
{
    return sPipe->outWrite == sPipe->outRead;
}

// Type of the next received message, or ONLINE_MSG_NONE if there is none.
u8 OnlineLink_PeekType(void)
{
    u32 read = sPipe->inRead;

    if (read == sPipe->inWrite)
        return ONLINE_MSG_NONE;

    return RingRead(sInbox, read + 2);
}

// Payload size of the next received message.
u16 OnlineLink_PeekSize(void)
{
    u32 read = sPipe->inRead;
    u16 length;

    if (read == sPipe->inWrite)
        return 0;

    length = RingRead(sInbox, read) | (RingRead(sInbox, read + 1) << 8);
    return length - 1;
}

// Pops the next received message's payload into dest and returns its size.
// Bytes beyond maxSize are discarded.
u16 OnlineLink_Receive(void *dest, u16 maxSize)
{
    u8 *dst = dest;
    u32 read = sPipe->inRead;
    u16 length, size;
    u32 i;

    if (read == sPipe->inWrite)
        return 0;

    length = RingRead(sInbox, read) | (RingRead(sInbox, read + 1) << 8);
    size = length - 1;
    for (i = 0; i < size && i < maxSize; i++)
        dst[i] = RingRead(sInbox, read + 3 + i);

    sPipe->inRead = read + 2 + length;
    return size;
}
