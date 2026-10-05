#include "global.h"
#include "battle.h"
#include "event_object_movement.h"
#include "field_door.h"
#include "field_effect.h"
#include "field_effect_helpers.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "multiplayer.h"
#include "online_link.h"
#include "overworld.h"
#include "sprite.h"
#include "constants/event_object_movement.h"
#include "constants/event_objects.h"
#include "constants/field_effects.h"
#include "constants/trainer_types.h"

u8 gMultiplayerAvatarObjId;
bool8 gDisableMonSelectCancel;

// The partner's avatar is driven by two messages:
//  - ONLINE_MSG_AVATAR_STATE: where the partner is and how they look. Sent when
//    it changes, and periodically. Decides whether the avatar is shown at all.
//  - ONLINE_MSG_AVATAR_EVENT: each movement, visibility change and door
//    animation, replayed in order so the avatar does exactly what they did.

// Events that the avatar hasn't replayed yet
#define AVATAR_EVENT_QUEUE_SIZE 16
// With more than this many queued, the avatar plays at double speed
#define AVATAR_CATCH_UP_BACKLOG 1
// With more than this many queued, the oldest are skipped
#define AVATAR_MAX_BACKLOG 6
// How long the avatar must sit idle before it is corrected from the state
#define AVATAR_IDLE_SYNC_FRAMES 30
// The state is resent this often even when unchanged
#define AVATAR_STATE_RESEND_FRAMES 60

static EWRAM_DATA struct OnlineAvatarEvent sAvatarEvents[AVATAR_EVENT_QUEUE_SIZE] = {0};
static EWRAM_DATA u8 sAvatarEventHead = 0;
static EWRAM_DATA u8 sAvatarEventCount = 0;
static EWRAM_DATA u8 sAvatarIdleFrames = 0;
static EWRAM_DATA u8 sAvatarSurfBlobSpriteId = 0;
static EWRAM_DATA bool8 sAvatarDoorAnimating = FALSE;
static EWRAM_DATA struct OnlineAvatarState sPeerState = {0};
static EWRAM_DATA bool8 sHasPeerState = FALSE;
static EWRAM_DATA struct OnlineAvatarState sSentState = {0};
static EWRAM_DATA u8 sStateResendTimer = 0;

void InitMultiplayerData(void) {
    gDisableMonSelectCancel = FALSE;
}

void InitMultiplayerAvatarIds(void)
{
    gMultiplayerAvatarObjId = OBJECT_EVENTS_COUNT;
    sAvatarEventHead = 0;
    sAvatarEventCount = 0;
    sAvatarIdleFrames = 0;
    sAvatarDoorAnimating = FALSE;
}

void ResetMultiplayerAvatarIds(void) {
    InitMultiplayerAvatarIds();
}

// Called when the link connects or drops
void Multiplayer_OnLinkReset(void)
{
    sHasPeerState = FALSE;
    sStateResendTimer = AVATAR_STATE_RESEND_FRAMES; // Send ours right away
}

bool32 IsMultiplayerAvatar(const struct ObjectEvent *objectEvent)
{
    return gMultiplayerAvatarObjId < OBJECT_EVENTS_COUNT
        && objectEvent == &gObjectEvents[gMultiplayerAvatarObjId];
}

// Whether the partner's avatar is standing at (x, y), for talking to it
bool32 IsMultiplayerAvatarAt(s16 x, s16 y)
{
    struct ObjectEvent *objEvent;

    if (gMultiplayerAvatarObjId == OBJECT_EVENTS_COUNT)
        return FALSE;

    objEvent = &gObjectEvents[gMultiplayerAvatarObjId];
    return objEvent->active && !objEvent->invisible
        && objEvent->currentCoords.x == x && objEvent->currentCoords.y == y;
}

// Online co-op battles are flagged as a link battle with an in-game partner,
// a combination vanilla never uses.
bool32 IsOnlineBattle(void) {
    return (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_INGAME_PARTNER)) == (BATTLE_TYPE_LINK | BATTLE_TYPE_INGAME_PARTNER);
}

static u8 GetPeerFacingDirection(void)
{
    if (sPeerState.facingDirection < DIR_SOUTH || sPeerState.facingDirection > DIR_EAST)
        return DIR_SOUTH;
    return sPeerState.facingDirection;
}

// Where the given map's tiles sit in the local map's coordinates: (0, 0) for the
// local map itself, or the shift for a map connected to it. FALSE for any other
// map, as the partner can't be drawn there. Mirrors how fieldmap.c lays out the
// connected maps around the local one.
static bool32 GetMapOffsetFromLocalMap(u8 mapGroup, u8 mapNum, s16 *dx, s16 *dy)
{
    const struct MapConnections *connections = gMapHeader.connections;
    s32 i;

    if (mapGroup == gSaveBlock1Ptr->location.mapGroup && mapNum == gSaveBlock1Ptr->location.mapNum)
    {
        *dx = 0;
        *dy = 0;
        return TRUE;
    }

    if (connections == NULL)
        return FALSE;

    for (i = 0; i < connections->count; i++)
    {
        const struct MapConnection *connection = &connections->connections[i];
        const struct MapHeader *connectedMap;
        s32 offset = (s32)connection->offset;

        if (connection->mapGroup != mapGroup || connection->mapNum != mapNum)
            continue;

        connectedMap = GetMapHeaderFromConnection(connection);
        switch (connection->direction)
        {
        case CONNECTION_SOUTH:
            *dx = offset;
            *dy = gMapHeader.mapLayout->height;
            return TRUE;
        case CONNECTION_NORTH:
            *dx = offset;
            *dy = -connectedMap->mapLayout->height;
            return TRUE;
        case CONNECTION_EAST:
            *dx = gMapHeader.mapLayout->width;
            *dy = offset;
            return TRUE;
        case CONNECTION_WEST:
            *dx = -connectedMap->mapLayout->width;
            *dy = offset;
            return TRUE;
        }
        // Dive/emerge connections aren't laid out next to the map
    }
    return FALSE;
}

// The avatar is saved along with the other object events, so a save made while
// the partner was nearby comes back with an avatar we aren't tracking yet. Its
// map can be a neighbor of the local one, as it survives crossing a connection.
static void TryAdoptSavedMultiplayerAvatar(void)
{
    u8 i;

    if (gMultiplayerAvatarObjId != OBJECT_EVENTS_COUNT)
        return;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (gObjectEvents[i].active && !gObjectEvents[i].isPlayer
         && gObjectEvents[i].localId == MULTIPLAYER_AVATAR_LOCAL_ID)
        {
            gMultiplayerAvatarObjId = i;
            return;
        }
    }
}

static void SpawnMultiplayerAvatar(s16 dx, s16 dy)
{
    struct ObjectEventTemplate objTemplate = {0};
    u8 objId;

    objTemplate.localId = MULTIPLAYER_AVATAR_LOCAL_ID;
    objTemplate.graphicsId = GetRivalAvatarGraphicsIdByPlayerGraphicsId(sPeerState.graphicsId);
    objTemplate.kind = OBJ_KIND_NORMAL;
    objTemplate.x = sPeerState.x + dx - MAP_OFFSET;
    objTemplate.y = sPeerState.y + dy - MAP_OFFSET;
    objTemplate.elevation = sPeerState.elevation;
    objTemplate.movementType = GetTrainerFacingDirectionMovementType(GetPeerFacingDirection());
    objTemplate.trainerType = TRAINER_TYPE_NONE;

    objId = SpawnSpecialObjectEvent(&objTemplate);
    if (objId < OBJECT_EVENTS_COUNT)
    {
        gMultiplayerAvatarObjId = objId;
        gObjectEvents[objId].invisible = sPeerState.invisible;
    }
    sAvatarSurfBlobSpriteId = MAX_SPRITES;
}

static bool32 IsAvatarSurfBlob(u8 spriteId)
{
    return spriteId < MAX_SPRITES
        && gSprites[spriteId].inUse
        && gSprites[spriteId].callback == UpdateSurfBlobFieldEffect
        && gSprites[spriteId].data[2] == gMultiplayerAvatarObjId;
}

static void DestroyAvatarSurfBlob(void)
{
    if (IsAvatarSurfBlob(sAvatarSurfBlobSpriteId))
        DestroySprite(&gSprites[sAvatarSurfBlobSpriteId]);
    sAvatarSurfBlobSpriteId = MAX_SPRITES;
}

// Gives the avatar a surf blob while it is drawn surfing. Checked every frame,
// so the blob also comes back when sprites are rebuilt (after a battle, a menu, ...).
static void UpdateAvatarSurfBlob(struct ObjectEvent *objEvent)
{
    bool32 isSurfing = objEvent->graphicsId == OBJ_EVENT_GFX_RIVAL_BRENDAN_SURFING
                    || objEvent->graphicsId == OBJ_EVENT_GFX_RIVAL_MAY_SURFING;

    if (isSurfing && !IsAvatarSurfBlob(sAvatarSurfBlobSpriteId))
    {
        s32 savedArgs[3];

        // Don't clobber the arguments of a field effect the player has going
        memcpy(savedArgs, gFieldEffectArguments, sizeof(savedArgs));
        gFieldEffectArguments[0] = objEvent->currentCoords.x;
        gFieldEffectArguments[1] = objEvent->currentCoords.y;
        gFieldEffectArguments[2] = gMultiplayerAvatarObjId;
        sAvatarSurfBlobSpriteId = FieldEffectStart(FLDEFF_SURF_BLOB);
        memcpy(gFieldEffectArguments, savedArgs, sizeof(savedArgs));
        if (sAvatarSurfBlobSpriteId < MAX_SPRITES)
            SetSurfBlob_BobState(sAvatarSurfBlobSpriteId, BOB_PLAYER_AND_MON);
    }
    else if (!isSurfing)
    {
        DestroyAvatarSurfBlob();
    }

    if (IsAvatarSurfBlob(sAvatarSurfBlobSpriteId))
        gSprites[sAvatarSurfBlobSpriteId].invisible = objEvent->invisible;
}

static void RemoveMultiplayerAvatar(void)
{
    DestroyAvatarSurfBlob();
    RemoveMultiplayerAvatarObjectEvent();
}

static struct OnlineAvatarEvent *PopAvatarEvent(void)
{
    struct OnlineAvatarEvent *event = &sAvatarEvents[sAvatarEventHead];

    sAvatarEventHead = (sAvatarEventHead + 1) % AVATAR_EVENT_QUEUE_SIZE;
    sAvatarEventCount--;
    return event;
}

static bool32 IsAvatarDoorAnimating(void)
{
    if (sAvatarDoorAnimating && !FieldIsDoorAnimationRunning())
        sAvatarDoorAnimating = FALSE;
    return sAvatarDoorAnimating;
}

// Whether the next event has to wait. Visibility changes don't wait for doors:
// going in, the player vanishes as the door starts closing.
static bool32 IsAvatarBusy(struct ObjectEvent *objEvent, const struct OnlineAvatarEvent *next)
{
    if (objEvent->heldMovementActive)
        return TRUE;
    return next->kind != AVATAR_EVENT_SET_INVISIBLE && IsAvatarDoorAnimating();
}

static void PlayAvatarEvent(struct ObjectEvent *objEvent, const struct OnlineAvatarEvent *event, s16 x, s16 y)
{
    switch (event->kind)
    {
    case AVATAR_EVENT_MOVE:
        // Each movement carries the tile it started from, so the avatar is
        // put back on track if it ever drifts.
        if (objEvent->currentCoords.x != x || objEvent->currentCoords.y != y)
        {
            MoveObjectEventToMapCoords(objEvent, x, y);
            ObjectEventUpdateElevation(objEvent);
        }
        ObjectEventSetHeldMovement(objEvent, event->arg);
        break;
    case AVATAR_EVENT_SET_INVISIBLE:
        objEvent->invisible = event->arg;
        break;
    // Door coordinates are the door tile. Off screen these do nothing, and if
    // the local player's door is animating they are skipped.
    case AVATAR_EVENT_DOOR_SET_OPEN:
        if (x >= 0 && y >= 0)
            FieldSetDoorOpened(x, y);
        break;
    case AVATAR_EVENT_DOOR_OPEN:
        if (x >= 0 && y >= 0 && FieldAnimateDoorOpen(x, y) >= 0)
            sAvatarDoorAnimating = TRUE;
        break;
    case AVATAR_EVENT_DOOR_CLOSE:
        if (x >= 0 && y >= 0 && FieldAnimateDoorClose(x, y) >= 0)
            sAvatarDoorAnimating = TRUE;
        break;
    }
}

// Replays the partner's events in order
static void PlayAvatarEvents(struct ObjectEvent *objEvent)
{
    struct OnlineAvatarEvent *event;
    s16 dx, dy;

    ObjectEventClearHeldMovementIfFinished(objEvent);

    while (sAvatarEventCount > AVATAR_MAX_BACKLOG)
        PopAvatarEvent();

    while (sAvatarEventCount != 0 && !IsAvatarBusy(objEvent, &sAvatarEvents[sAvatarEventHead]))
    {
        event = PopAvatarEvent();
        if (GetMapOffsetFromLocalMap(event->mapGroup, event->mapNum, &dx, &dy))
            PlayAvatarEvent(objEvent, event, event->x + dx, event->y + dy);
    }

    if (sAvatarEventCount > AVATAR_CATCH_UP_BACKLOG)
        ObjectEventAdvanceHeldMovement(objEvent);
}

// Safety net for anything the events don't cover (fly, escape rope, skipped
// events, ...): once the avatar has been idle for a bit, make it match the
// partner's state.
static void SyncIdleAvatarWithState(struct ObjectEvent *objEvent, s16 x, s16 y)
{
    u8 facing = GetPeerFacingDirection();

    if (objEvent->heldMovementActive || sAvatarEventCount != 0 || IsAvatarDoorAnimating())
    {
        sAvatarIdleFrames = 0;
        return;
    }

    if (sAvatarIdleFrames < AVATAR_IDLE_SYNC_FRAMES)
    {
        sAvatarIdleFrames++;
        return;
    }

    if (objEvent->currentCoords.x != x || objEvent->currentCoords.y != y)
    {
        MoveObjectEventToMapCoords(objEvent, x, y);
        objEvent->currentElevation = sPeerState.elevation;
        objEvent->previousElevation = sPeerState.elevation;
    }
    if (objEvent->facingDirection != facing)
        ObjectEventTurn(objEvent, facing);
    objEvent->invisible = sPeerState.invisible;
}

static void UpdateAvatarFromState(struct ObjectEvent *objEvent, s16 dx, s16 dy)
{
    u8 graphicsId = GetRivalAvatarGraphicsIdByPlayerGraphicsId(sPeerState.graphicsId);

    // Mounting a bike, surfing, etc.
    if (objEvent->graphicsId != graphicsId)
    {
        ObjectEventSetGraphicsId(objEvent, graphicsId);
        ObjectEventTurn(objEvent, objEvent->facingDirection);
    }

    PlayAvatarEvents(objEvent);
    SyncIdleAvatarWithState(objEvent, sPeerState.x + dx, sPeerState.y + dy);
    UpdateAvatarSurfBlob(objEvent);
}

// Runs every overworld frame. The partner's avatar is a "ghost" object event:
// it is never despawned for being off screen and nothing collides with it
// (see IsMultiplayerAvatar in event_object_movement.c). It is shown while the
// partner is on the local map or one connected to it, and is kept through
// connection crossings, which shift it along with every other object event.
void UpdateMultiplayerAvatar(void)
{
    s16 dx, dy;

    TryAdoptSavedMultiplayerAvatar();

    if (!OnlineLink_IsConnected() || !sHasPeerState
     || !GetMapOffsetFromLocalMap(sPeerState.mapGroup, sPeerState.mapNum, &dx, &dy))
    {
        if (gMultiplayerAvatarObjId != OBJECT_EVENTS_COUNT)
            RemoveMultiplayerAvatar();
        return;
    }

    if (gMultiplayerAvatarObjId == OBJECT_EVENTS_COUNT)
        SpawnMultiplayerAvatar(dx, dy);
    else
        UpdateAvatarFromState(&gObjectEvents[gMultiplayerAvatarObjId], dx, dy);
}

// Runs every overworld frame
void Multiplayer_SendAvatarState(void)
{
    struct OnlineAvatarState state;
    struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];

    if (!OnlineLink_IsConnected())
        return;

    state.mapGroup = gSaveBlock1Ptr->location.mapGroup;
    state.mapNum = gSaveBlock1Ptr->location.mapNum;
    state.x = player->currentCoords.x;
    state.y = player->currentCoords.y;
    state.facingDirection = player->facingDirection;
    state.graphicsId = player->graphicsId;
    state.elevation = player->currentElevation;
    state.invisible = player->invisible;

    if (memcmp(&state, &sSentState, sizeof(state)) == 0 && ++sStateResendTimer < AVATAR_STATE_RESEND_FRAMES)
        return;

    if (OnlineLink_Send(ONLINE_MSG_AVATAR_STATE, &state, sizeof(state)))
    {
        sSentState = state;
        sStateResendTimer = 0;
    }
}

static void SendAvatarEvent(u8 kind, u8 arg, s16 x, s16 y)
{
    struct OnlineAvatarEvent event;

    if (!OnlineLink_IsConnected())
        return;

    event.mapGroup = gSaveBlock1Ptr->location.mapGroup;
    event.mapNum = gSaveBlock1Ptr->location.mapNum;
    event.x = x;
    event.y = y;
    event.kind = kind;
    event.arg = arg;
    OnlineLink_Send(ONLINE_MSG_AVATAR_EVENT, &event, sizeof(event));
}

void Multiplayer_SendPlayerMovement(const struct ObjectEvent *player, u8 movementActionId)
{
    if (movementActionId < MOVEMENT_ACTION_STEP_END)
        SendAvatarEvent(AVATAR_EVENT_MOVE, movementActionId, player->currentCoords.x, player->currentCoords.y);
}

void Multiplayer_SendPlayerInvisibility(bool8 invisible)
{
    struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];

    SendAvatarEvent(AVATAR_EVENT_SET_INVISIBLE, invisible, player->currentCoords.x, player->currentCoords.y);
}

// For the doors the player warps through; x, y is the door tile
void Multiplayer_SendDoorEvent(u8 kind, s16 x, s16 y)
{
    SendAvatarEvent(kind, 0, x, y);
}

// Called from Online_UpdateLink with an ONLINE_MSG_AVATAR_STATE at the front of
// the inbox.
void Multiplayer_ReceiveAvatarState(void)
{
    OnlineLink_Receive(&sPeerState, sizeof(sPeerState));
    sHasPeerState = TRUE;
}

// Called from Online_UpdateLink with an ONLINE_MSG_AVATAR_EVENT at the front of
// the inbox.
void Multiplayer_ReceiveAvatarEvent(void)
{
    struct OnlineAvatarEvent event;
    s16 dx, dy;

    OnlineLink_Receive(&event, sizeof(event));

    // Only events the avatar could show are kept. They're kept even before it
    // spawns: coming out of a door, the partner sends some before their state.
    if (!GetMapOffsetFromLocalMap(event.mapGroup, event.mapNum, &dx, &dy))
        return;

    if (sAvatarEventCount == AVATAR_EVENT_QUEUE_SIZE)
        PopAvatarEvent();
    sAvatarEvents[(sAvatarEventHead + sAvatarEventCount) % AVATAR_EVENT_QUEUE_SIZE] = event;
    sAvatarEventCount++;
}

void DisableMonSelectCancel(void) {
    gDisableMonSelectCancel = TRUE;
}

void EnableMonSelectCancel(void) {
    gDisableMonSelectCancel = FALSE;
}
