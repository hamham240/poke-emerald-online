#ifndef GUARD_ONLINE_BATTLE_H
#define GUARD_ONLINE_BATTLE_H

extern bool8 gDisableMonSelectCancel;

bool32 IsOnlineBattle(void);
void OnlineBattle_Init(void);

// The picks a player brings to an online co-op battle
void OnlineBattle_ReceivePartnerParty(void);

// Asks the owner of a leveling-up Pokémon about learning a move, on their own
// screen. Past the end of the controller command tables: controllers without a
// hook for it just complete it.
#define CONTROLLER_ONLINELEARNMOVE CONTROLLER_CMDS_COUNT

enum
{
    ONLINE_LEARN_MOVE_ASK,      // "Delete a move?" yes/no, then pick the move to forget
    ONLINE_LEARN_MOVE_PICK,     // Straight to picking the move to forget
    ONLINE_LEARN_MOVE_ASK_STOP, // "Stop learning?" yes/no
};

// Hooks in the battle engine (online_battle_engine.c)
bool32 OnlineBattle_TryAbortDisconnected(void);
void OnlineBattle_SetOpponentLinkPlayers(void);
bool32 OnlineBattle_TrySetEndCallbacks(void);
bool32 OnlineBattle_IsPartnerMon(u32 monId);
u32 OnlineBattle_GetExpGetterBattler(u32 monId);
bool32 OnlineBattle_IsTradedMon(u32 monId);
void OnlineBattle_LinkPartnerHandleExpUpdate(u32 battler);
void OnlineBattle_SyncMoves(u32 monId);
bool32 OnlineBattle_YesNoBoxLearnMove(const u8 *forgotMovePtr, const u8 *nextInstr);
bool32 OnlineBattle_YesNoBoxStopLearningMove(const u8 *nextInstr, const u8 *noInstr);
void OnlineBattle_PlayerHandleLearnMove(u32 battler);
void BtlController_EmitOnlineLearnMove(u32 battler, u32 bufferId, u8 mode, u8 monId, u16 move);

// Script natives
void OnlineBattle_Start(void);
void DisableMonSelectCancel(void);
void EnableMonSelectCancel(void);

#endif // GUARD_ONLINE_BATTLE_H
