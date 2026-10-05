#include "global.h"
#include "link.h"
#include "menu.h"
#include "online_menu.h"
#include "online_session.h"
#include "string_util.h"
#include "text.h"
#include "window.h"

static const u8 sText_OnlinePlayers[] = _("PLAYERS ONLINE");
static const u8 sText_WaitingForPlayers[] = _("Waiting for players…");

// Lists the other players connected online. Room for more than the one
// partner there is today.
u8 Online_ShowPlayersWindow(void)
{
    struct WindowTemplate template = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 1,
        .width = 16,
        .height = 4,
        .paletteNum = 15,
        .baseBlock = 0x8,
    };
    u8 windowId;
    u32 i, y;
    u32 numPlayers = 0;

    Online_RestoreLinkPlayers();
    if (gReceivedRemoteLinkPlayers)
    {
        for (i = 0; i < GetLinkPlayerCount(); i++)
        {
            if (i != GetMultiplayerId())
                numPlayers++;
        }
    }
    template.height = 2 * (1 + max(numPlayers, 1));

    windowId = AddWindow(&template);
    DrawStdWindowFrame(windowId, FALSE);
    FillWindowPixelBuffer(windowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(windowId, FONT_NORMAL, sText_OnlinePlayers, 0, 1, TEXT_SKIP_DRAW, NULL);

    y = 17;
    if (numPlayers == 0)
    {
        AddTextPrinterParameterized(windowId, FONT_NORMAL, sText_WaitingForPlayers, 8, y, TEXT_SKIP_DRAW, NULL);
    }
    else
    {
        for (i = 0; i < GetLinkPlayerCount(); i++)
        {
            if (i == GetMultiplayerId())
                continue;
            StringCopyN(gStringVar1, gLinkPlayers[i].name, PLAYER_NAME_LENGTH);
            gStringVar1[PLAYER_NAME_LENGTH] = EOS;
            AddTextPrinterParameterized(windowId, FONT_NORMAL, gStringVar1, 8, y, TEXT_SKIP_DRAW, NULL);
            y += 16;
        }
    }

    CopyWindowToVram(windowId, COPYWIN_FULL);
    return windowId;
}
