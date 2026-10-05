#ifndef GUARD_ONLINE_INTERACT_H
#define GUARD_ONLINE_INTERACT_H

#include "constants/online_interact.h"

void OnlineInteract_Receive(void);
void OnlineInteract_OnLinkReset(void);
void OnlineInteract_OnDisconnect(void);
bool32 OnlineInteract_TryStartIncomingRequestScript(void);
bool32 IsOnlineDuel(void);
bool32 IsOnlineTrade(void);
void Online_SetLinkLost(void);
bool32 Online_WasLinkLost(void);
void OnlineTrade_End(void);

// Specials
void OnlineInteract_BufferPartnerName(void);
void OnlineInteract_SendRequest(void);
void OnlineInteract_BufferIncomingRequest(void);
void OnlineInteract_Decline(void);
void OnlineInteract_Accept(void);
void OnlineInteract_WaitForPartner(void);
void OnlineDuel_Start(void);
void OnlineTrade_Start(void);
void Online_CheckLinkLost(void);

#endif // GUARD_ONLINE_INTERACT_H
