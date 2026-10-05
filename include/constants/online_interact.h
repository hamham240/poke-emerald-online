#ifndef GUARD_CONSTANTS_ONLINE_INTERACT_H
#define GUARD_CONSTANTS_ONLINE_INTERACT_H

// What one online player asks another to do (VAR_0x8004 in the scripts)
#define ONLINE_INTERACT_NONE  0
#define ONLINE_INTERACT_DUEL  1
#define ONLINE_INTERACT_TRADE 2

// How a request ended (VAR_RESULT after OnlineInteract_SendRequest / _WaitForPartner)
#define ONLINE_INTERACT_RESULT_START             0 // Both agreed, go
#define ONLINE_INTERACT_RESULT_DECLINED          1 // The partner said no
#define ONLINE_INTERACT_RESULT_PARTNER_CANCELLED 2 // The partner backed out
#define ONLINE_INTERACT_RESULT_CANCELLED         3 // This player backed out
#define ONLINE_INTERACT_RESULT_DISCONNECTED      4

#endif // GUARD_CONSTANTS_ONLINE_INTERACT_H
