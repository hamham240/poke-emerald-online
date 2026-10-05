@ Online co-op battle against the trainer the player was spotted by, chosen at
@ the trainer's battle prompt (see EventScript_ShowTrainerIntroMsg and
@ src/online_battle.c)
EventScript_DoOnlineDoubleBattle::
	@ Save the player's party so that we can restore it after the battle
	special SavePlayerParty

	@ Make the player choose MAX_PARTY/2 pokemons for battle
	callnative DisableMonSelectCancel
	special ChooseHalfPartyForBattle
	waitstate

	@ Reduce their party to these selected mons
	special ReducePlayerPartyToSelectedMons

	callnative OnlineBattle_Start
	waitstate

	@ The battle ended early if the partner disconnected; it didn't count
	callnative Online_CheckLinkLost
	goto_if_eq VAR_RESULT, TRUE, EventScript_OnlineActivityLinkLost

	@ VAR_0x8006 is TRUE if the battle was won, with the prize in STR_VAR_1
	call_if_eq VAR_0x8006, TRUE, EventScript_OnlineBattlePrizeMoney

	@ End the battle
	goto EventScript_EndTrainerBattle

EventScript_OnlineBattlePrizeMoney:
	message EventScript_Text_OnlineBattlePrizeMoney
	waitmessage
	waitbuttonpress
	closemessage
	return

EventScript_Text_OnlineBattlePrizeMoney:
	.string "{PLAYER} got ¥{STR_VAR_1}\nfor winning!$"
