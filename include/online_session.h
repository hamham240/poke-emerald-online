#ifndef GUARD_ONLINE_SESSION_H
#define GUARD_ONLINE_SESSION_H

// The vanilla link API carried over the online link (see online_session.c)

#define ONLINE_LINK_PLAYER_COUNT 2

void Online_Init(void);
void Online_UpdateLink(void);
bool8 Online_SendBlock(const void *src, u16 size);
bool32 Online_IsLinkTaskFinished(void);
void Online_EnterStandby(void);
void Online_ReadyCloseLink(void);

// Puts both players' info back in gLinkPlayers
void Online_RestoreLinkPlayers(void);

// Pairing for online battles. Trainer ids are the trainer each player is
// waiting at; TRAINER_NONE when there is none.
void OnlinePair_Send(u8 type, u16 trainerId);
u16 OnlinePair_GetPeerReady(void);
u16 OnlinePair_GetPeerCommitted(void);
void OnlinePair_ClearPeer(void);

// Set when an online activity ended because the partner's game was gone
void Online_SetLinkLost(void);
bool32 Online_WasLinkLost(void);

// Script natives
void Online_CheckLinkLost(void);

#endif // GUARD_ONLINE_SESSION_H
