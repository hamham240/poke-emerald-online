#ifndef GUARD_ONLINE_INTERACT_H
#define GUARD_ONLINE_INTERACT_H

#include "constants/online_interact.h"

extern const u8 EventScript_OnlinePlayerInteract[];
extern const u8 EventScript_OnlineInteractRequested[];

void OnlineInteract_Receive(void);
void OnlineInteract_OnLinkReset(void);
void OnlineInteract_OnDisconnect(void);
bool32 OnlineInteract_TryStartIncomingRequestScript(void);

// Script natives
void OnlineInteract_BufferPartnerName(void);
void OnlineInteract_SendRequest(void);
void OnlineInteract_BufferIncomingRequest(void);
void OnlineInteract_Decline(void);
void OnlineInteract_Accept(void);
void OnlineInteract_WaitForPartner(void);

#endif // GUARD_ONLINE_INTERACT_H
