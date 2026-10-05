#include "constants/global.h"
#include "constants/online_interact.h"
#include "constants/script_menu.h"
#include "constants/vars.h"
	.include "asm/macros.inc"
	.include "asm/macros/event.inc"
	.include "constants/constants.inc"

	.section script_data, "aw", %progbits

	.include "data/scripts/online_battle.inc"
	.include "data/scripts/online_interact.inc"
