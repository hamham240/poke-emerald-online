EventScript_StartTrainerApproach::
	selectapproachingtrainer
	lockfortrainer
EventScript_TrainerApproach::
	special PlayTrainerEncounterMusic
	special DoTrainerApproach
	waitstate
	goto EventScript_ShowTrainerIntroMsg

EventScript_TryDoNormalTrainerBattle::
	lock
	faceplayer
	applymovement VAR_LAST_TALKED, Movement_RevealTrainer
	waitmovement 0
	specialvar VAR_RESULT, GetTrainerFlag
	goto_if_ne VAR_RESULT, FALSE, EventScript_NoNormalTrainerBattle
	special PlayTrainerEncounterMusic
	special SetTrainerFacingDirection
	goto EventScript_ShowTrainerIntroMsg

EventScript_NoNormalTrainerBattle::
	gotopostbattlescript

EventScript_TryDoDoubleTrainerBattle::
	lock
	faceplayer
	call EventScript_RevealTrainer
	specialvar VAR_RESULT, GetTrainerFlag
	goto_if_ne VAR_RESULT, FALSE, EventScript_NoDoubleTrainerBattle
	special HasEnoughMonsForDoubleBattle
	goto_if_ne VAR_RESULT, PLAYER_HAS_TWO_USABLE_MONS, EventScript_NotEnoughMonsForDoubleBattle
	special PlayTrainerEncounterMusic
	special SetTrainerFacingDirection
	goto EventScript_ShowTrainerIntroMsg

EventScript_NotEnoughMonsForDoubleBattle::
	special ShowTrainerCantBattleSpeech
	waitmessage
	waitbuttonpress
	release
	end

EventScript_NoDoubleTrainerBattle::
	gotopostbattlescript

EventScript_DoNoIntroTrainerBattle::
	applymovement VAR_LAST_TALKED, Movement_RevealTrainer
	waitmovement 0
	special PlayTrainerEncounterMusic
	dotrainerbattle
	gotopostbattlescript

EventScript_TryDoRematchBattle::
	call EventScript_RevealTrainer
	specialvar VAR_RESULT, IsTrainerReadyForRematch
	goto_if_eq VAR_RESULT, FALSE, EventScript_NoRematchTrainerBattle
	special PlayTrainerEncounterMusic
	special SetTrainerFacingDirection
	special ShowTrainerIntroSpeech
	waitmessage
	waitbuttonpress
	special BattleSetup_StartRematchBattle
	waitstate
	releaseall
	end

EventScript_NoRematchTrainerBattle::
	gotopostbattlescript

EventScript_TryDoDoubleRematchBattle::
	specialvar VAR_RESULT, IsTrainerReadyForRematch
	goto_if_eq VAR_RESULT, FALSE, EventScript_NoDoubleRematchTrainerBattle
	special HasEnoughMonsForDoubleBattle
	goto_if_ne VAR_RESULT, PLAYER_HAS_TWO_USABLE_MONS, EventScript_NotEnoughMonsForDoubleRematchBattle
	special PlayTrainerEncounterMusic
	special SetTrainerFacingDirection
	special ShowTrainerIntroSpeech
	waitmessage
	waitbuttonpress
	special BattleSetup_StartRematchBattle
	waitstate
	releaseall
	end

EventScript_NoDoubleRematchTrainerBattle::
	gotopostbattlescript

EventScript_NotEnoughMonsForDoubleRematchBattle::
	special ShowTrainerCantBattleSpeech
	waitmessage
	waitbuttonpress
	release
	end

EventScript_RevealTrainer::
	applymovement VAR_LAST_TALKED, Movement_RevealTrainer
	waitmovement 0
	return

Movement_RevealTrainer:
	reveal_trainer
	step_end

EventScript_ShowTrainerIntroMsg::
	special ShowTrainerIntroSpeech
	waitmessage

	@ Allow player to choose normal battle or online partner double battle
	multichoice 0, 0, MULTI_MULTIPLAYER_BATTLE_CHOICE, TRUE
	compare VAR_RESULT 0
	goto_if_eq EventScript_DoOnlineDoubleBattle

	special TryPrepareSecondApproachingTrainer
	goto_if_eq VAR_RESULT, TRUE, EventScript_TrainerApproach
	goto EventScript_DoTrainerBattle

EventScript_DoOnlineDoubleBattle::
	@ Save the player's party so that we can restore it after the battle
	special SavePlayerParty

	@ Make the player choose MAX_PARTY/2 pokemons for battle
	special DisableMonSelectCancel
	special ChooseHalfPartyForBattle
	waitstate

	@ Reduce their party to these selected mons
	special ReducePlayerPartyToSelectedMons

	@ TODO: figure out if this is necessary
	frontier_set FRONTIER_DATA_SELECTED_MON_ORDER

	@ Set special variable to indicate online double battle
	setvar VAR_0x8004, SPECIAL_BATTLE_ONLINE_DOUBLE

	@ Doesn't seem necessary but we'll keep it in
	setvar VAR_0x8005, 0

	@ Call DoSpecialTrainerBattle() in battle_tower.c
	special DoSpecialTrainerBattle
	waitstate

	@ End the battle
	goto EventScript_EndTrainerBattle

EventScript_DoTrainerBattle::
	dotrainerbattle
	@ Below battle mode check only needed in FRLG
	specialvar VAR_RESULT, GetTrainerBattleMode
	goto_if_eq VAR_RESULT, TRAINER_BATTLE_SINGLE, EventScript_EndTrainerBattle
	goto_if_eq VAR_RESULT, TRAINER_BATTLE_CONTINUE_SCRIPT, EventScript_EndTrainerBattle
	goto_if_eq VAR_RESULT, TRAINER_BATTLE_CONTINUE_SCRIPT_NO_MUSIC, EventScript_EndTrainerBattle
	goto_if_eq VAR_RESULT, TRAINER_BATTLE_CONTINUE_SCRIPT_DOUBLE, EventScript_EndTrainerBattle
	goto_if_eq VAR_RESULT, TRAINER_BATTLE_CONTINUE_SCRIPT_DOUBLE_NO_MUSIC, EventScript_EndTrainerBattle

EventScript_EndTrainerBattle::
	gotobeatenscript
	releaseall
	end

Std_MsgboxAutoclose::
	message NULL
	waitmessage
	waitbuttonpress
	release
	return
