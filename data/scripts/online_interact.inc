@ Pressing A on the online partner's avatar (see src/online_interact.c)
EventScript_OnlinePlayerInteract::
	lockall
	special OnlineInteract_BufferPartnerName
	message EventScript_Text_OnlineInteractWhat
	waitmessage
	multichoice 0, 0, MULTI_ONLINE_INTERACT, FALSE
	switch VAR_RESULT
	case 0, EventScript_OnlineRequestDuel
	case 1, EventScript_OnlineRequestTrade
	closemessage
	releaseall
	end

EventScript_OnlineRequestDuel::
	setvar VAR_0x8004, ONLINE_INTERACT_DUEL
	goto EventScript_OnlineSendRequest

EventScript_OnlineRequestTrade::
	setvar VAR_0x8004, ONLINE_INTERACT_TRADE
	goto EventScript_OnlineSendRequest

EventScript_OnlineSendRequest::
	message EventScript_Text_OnlineWaitingForAnswer
	waitmessage
	special OnlineInteract_SendRequest
	waitstate
	goto EventScript_OnlineInteractResult

@ Shown to the partner, once they're free to act
EventScript_OnlineInteractRequested::
	lockall
	special OnlineInteract_BufferIncomingRequest
	goto_if_eq VAR_0x8004, ONLINE_INTERACT_TRADE, EventScript_OnlineAskTrade
	msgbox EventScript_Text_OnlineDuelRequested, MSGBOX_YESNO
	goto EventScript_OnlineAnswerRequest

EventScript_OnlineAskTrade::
	msgbox EventScript_Text_OnlineTradeRequested, MSGBOX_YESNO
EventScript_OnlineAnswerRequest::
	goto_if_eq VAR_RESULT, NO, EventScript_OnlineDeclineRequest
	special OnlineInteract_Accept
	goto_if_eq VAR_RESULT, FALSE, EventScript_OnlineRequestWithdrawn
	message EventScript_Text_OnlineWaitingForPartner
	waitmessage
	special OnlineInteract_WaitForPartner
	waitstate
	goto EventScript_OnlineInteractResult

EventScript_OnlineDeclineRequest::
	special OnlineInteract_Decline
	closemessage
	releaseall
	end

EventScript_OnlineRequestWithdrawn::
	msgbox EventScript_Text_OnlinePartnerCancelled, MSGBOX_DEFAULT
	closemessage
	releaseall
	end

@ VAR_RESULT is an ONLINE_INTERACT_RESULT_*, VAR_0x8004 what was asked for
EventScript_OnlineInteractResult::
	switch VAR_RESULT
	case ONLINE_INTERACT_RESULT_START, EventScript_OnlineInteractStart
	case ONLINE_INTERACT_RESULT_DECLINED, EventScript_OnlinePartnerDeclined
	case ONLINE_INTERACT_RESULT_PARTNER_CANCELLED, EventScript_OnlineRequestWithdrawn
	case ONLINE_INTERACT_RESULT_DISCONNECTED, EventScript_OnlineInteractDisconnected
	closemessage
	releaseall
	end

EventScript_OnlinePartnerDeclined::
	msgbox EventScript_Text_OnlinePartnerDeclined, MSGBOX_DEFAULT
	closemessage
	releaseall
	end

EventScript_OnlineInteractDisconnected::
	msgbox EventScript_Text_OnlineDisconnected, MSGBOX_DEFAULT
	closemessage
	releaseall
	end

EventScript_OnlineInteractStart::
	closemessage
	goto_if_eq VAR_0x8004, ONLINE_INTERACT_TRADE, EventScript_OnlineStartTrade
	special OnlineDuel_Start
	waitstate
	releaseall
	end

EventScript_OnlineStartTrade::
	special OnlineTrade_Start
	waitstate
	releaseall
	end

EventScript_Text_OnlineInteractWhat:
	.string "What would you like to do\n"
	.string "with {STR_VAR_1}?$"

EventScript_Text_OnlineWaitingForAnswer:
	.string "Waiting for {STR_VAR_1} to answer…\n"
	.string "Press B to cancel.$"

EventScript_Text_OnlineWaitingForPartner:
	.string "Waiting for {STR_VAR_1}…$"

EventScript_Text_OnlineDuelRequested:
	.string "{STR_VAR_1} wants to battle!\n"
	.string "Will you accept?$"

EventScript_Text_OnlineTradeRequested:
	.string "{STR_VAR_1} wants to trade!\n"
	.string "Will you accept?$"

EventScript_Text_OnlinePartnerDeclined:
	.string "{STR_VAR_1} declined.$"

EventScript_Text_OnlinePartnerCancelled:
	.string "{STR_VAR_1} cancelled.$"

EventScript_Text_OnlineDisconnected:
	.string "The link was lost.$"
