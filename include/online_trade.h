#ifndef GUARD_ONLINE_TRADE_H
#define GUARD_ONLINE_TRADE_H

bool32 IsOnlineTrade(void);
void OnlineTrade_Prepare(void);
void OnlineTrade_End(void);
void OnlineTrade_ReturnToField(void);
void OnlineTrade_Abort(void);
void OnlineTrade_SetContinueGameWarp(void);
bool32 OnlineTrade_BufferParties(u8 *state, u8 *giftRibbons);

// Script natives
void OnlineTrade_Start(void);

#endif // GUARD_ONLINE_TRADE_H
