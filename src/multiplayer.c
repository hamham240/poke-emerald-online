#include "global.h"
#include "global.fieldmap.h"
#include "multiplayer.h"
#include "event_object_movement.h"
#include "field_door.h"
#include "field_effect.h"
#include "field_effect_helpers.h"
#include "field_player_avatar.h"
#include "field_control_avatar.h"
#include "field_screen_effect.h"
#include "field_weather.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "fldeff_misc.h"
#include "item.h"
#include "task.h"
#include "metatile_behavior.h"
#include "overworld.h"
#include "script.h"
#include "sprite.h"
#include "event_data.h"
#include "sound.h"
#include "trig.h"
#include "metatile_behavior.h"
#include "constants/event_object_movement.h"
#include "constants/event_objects.h"
#include "constants/songs.h"
#include "constants/map_types.h"
#include "constants/field_effects.h"
#include "constants/metatile_behaviors.h"
#include "constants/trainer_types.h"

u8 gMultiplayerAvatarObjId;
u8 gMultiplayerAvatarSpriteId;

static bool8 DummyMovementCallback(struct ObjectEvent *objectEvent, struct Sprite *sprite)
{
    return 0;
}

void InitMultiplayerAvatarIds(void)
{
    gMultiplayerAvatarObjId = OBJECT_EVENTS_COUNT;
    gMultiplayerAvatarSpriteId = MAX_SPRITES;
}

void ResetMultiplayerAvatarIds(void) {
    InitMultiplayerAvatarIds();
}

u8 ReadConnectedByte(void) {
    // The first byte in the general buffer marks a "connected" status
    // This can be handy when trying to see if there are any packets to read and process
    // A value of 0 == not connected, 1 == currently connected
    // NOTE: This byte does not get modified by the ROM, only by the emulator
    return *((u8*) GENERAL_BUFFER_BEGIN_ADDRESS);
}

void SpawnMultiplayerAvatar(struct MultiplayerPacket* multiplayerPacket) {
    struct ObjectEvent* player;
    struct ObjectEventTemplate objTemplate;

    // Grab the player's object event
    player = &gObjectEvents[gPlayerAvatar.objectEventId];

    // Create an object template, almost verbatim as how InitPlayerAvatar does it
    objTemplate.localId = OBJ_EVENT_ID_PLAYER - 1;
    objTemplate.graphicsId = 89;
    objTemplate.kind = 0;
    objTemplate.x = multiplayerPacket->x;
    objTemplate.y = multiplayerPacket->y;
    objTemplate.elevation = 0;
    objTemplate.movementType = 0xB; // found in event_object_movement.h, makes character look in random directions
    objTemplate.movementRangeX = 0;
    objTemplate.movementRangeY = 0;
    objTemplate.trainerType = TRAINER_TYPE_NONE;
    objTemplate.trainerRange_berryTreeId = 0;
    objTemplate.script = NULL;
    objTemplate.flagId = 0;

    switch (GetPlayerFacingDirection())
    {
    case DIR_NORTH:
        objTemplate.movementType = MOVEMENT_TYPE_FACE_UP;
        break;
    case DIR_WEST:
        objTemplate.movementType = MOVEMENT_TYPE_FACE_LEFT;
        break;
    case DIR_EAST:
        objTemplate.movementType = MOVEMENT_TYPE_FACE_RIGHT;
        break;
    }

    // Create object event from template
    gMultiplayerAvatarObjId = TrySpawnObjectEventTemplate(
        &objTemplate, 
        multiplayerPacket->mapNum, 
        multiplayerPacket->mapGroup,
        objTemplate.x, 
        objTemplate.y
    );

    if (gMultiplayerAvatarObjId != OBJECT_EVENTS_COUNT)
    {
        gObjectEvents[gMultiplayerAvatarObjId].invisible = FALSE;
        gMultiplayerAvatarSpriteId = gObjectEvents[gMultiplayerAvatarObjId].spriteId;
    }
    else
    {
        gMultiplayerAvatarSpriteId = MAX_SPRITES;
    }
}

void TrySpawnMultiplayerAvatar(void) {
    if (ReadConnectedByte() == 0) {
        return;
    }

    struct MultiplayerPacket* multiplayerPacket = GetPeerPacket();

    if (multiplayerPacket->mapNum != gSaveBlock1Ptr->location.mapNum) {
        return;
    }

    if (multiplayerPacket->mapGroup != gSaveBlock1Ptr->location.mapGroup) {
        return;
    }

    if (gMultiplayerAvatarObjId != OBJECT_EVENTS_COUNT) {
        return;
    }

    if (gMultiplayerAvatarSpriteId != MAX_SPRITES) {
        return;
    }

    SpawnMultiplayerAvatar(multiplayerPacket);
}

void TryMoveMultiplayerSprite(void) {
    if (gMultiplayerAvatarObjId == OBJECT_EVENTS_COUNT) {
        return;
    }

    if (gMultiplayerAvatarSpriteId == MAX_SPRITES) {
        return;
    }

    struct MultiplayerPacket* multiplayerPacket = GetPeerPacket();

    if (multiplayerPacket->mapNum != gSaveBlock1Ptr->location.mapNum) {
        return;
    }

    if (multiplayerPacket->mapGroup != gSaveBlock1Ptr->location.mapGroup) {
        return;
    }

    // TODO: call InitMultiplayerAvatarIds in main possibly
    // TODO: Make sure this function gets called continuously? 
    // TODO: Make sure multiplayer avatar obj event gets de-rendered if off screen (undo the changes I made)

    struct ObjectEvent* playerObjEvent = &gObjectEvents[gPlayerAvatar.objectEventId];

    struct ObjectEvent* objEvent = &gObjectEvents[gMultiplayerAvatarObjId];
    struct Sprite* sprite = &gSprites[gMultiplayerAvatarSpriteId];

    int totalDistance = 0;
    totalDistance += abs(objEvent->currentCoords.x - multiplayerPacket->x);
    totalDistance += abs(objEvent->currentCoords.y - multiplayerPacket->y);

    if (totalDistance > 3) {
        TryMoveObjectEventToMapCoords(
            254, 
            multiplayerPacket->mapNum, 
            multiplayerPacket->mapGroup, 
            multiplayerPacket->x - 7,
            multiplayerPacket->y - 7
        );
        //TODO: This causes issues when player moves to a different mapNum (or mapGroup I believe)
    }

    int currentX = objEvent->currentCoords.x;
    int currentY = objEvent->currentCoords.y;
    int packetX = multiplayerPacket->x;
    int packetY = multiplayerPacket->y;

    int diffX = packetX - currentX; // A positive value indicates player moved to the right
    int diffY = packetY - currentY; // A positive value indicates player moved down

    if (diffX != 0 || diffY != 0)
    {
        u8 maId = objEvent->movementActionId;

        if ((maId < 0x8 || maId > 0xB) && (maId < 0x35 || maId > 0x38))
        {
            ObjectEventClearHeldMovementIfActive(objEvent);
        }
        else 
        {
            ObjectEventClearHeldMovementIfFinished(objEvent);
        }
    }
    else 
    {
        ObjectEventClearHeldMovementIfFinished(objEvent);
    }

    if (!objEvent->heldMovementActive)
    {
        if (diffX != 0 || diffY != 0)
        {
            if (diffX > 0)
            {
                if (diffX == 1)
                {
                    ObjectEventSetHeldMovement(objEvent, 0xB);
                }
                else
                {
                    ObjectEventSetHeldMovement(objEvent, 0x38);
                }
            }
            else if (diffX < 0)
            {
                if (diffX == -1)
                {
                    ObjectEventSetHeldMovement(objEvent, 0xA);
                }
                else 
                {
                    ObjectEventSetHeldMovement(objEvent, 0x37);
                }
            }

            if (diffY > 0)
            {
                if (diffY == 1)
                {
                    ObjectEventSetHeldMovement(objEvent, 0x8);
                }
                else
                {
                    ObjectEventSetHeldMovement(objEvent, 0x35);
                }
            }
            else if (diffY < 0)
            {
                if (diffY == -1)
                {
                    ObjectEventSetHeldMovement(objEvent, 0x9);
                }
                else
                {
                    ObjectEventSetHeldMovement(objEvent, 0x36);
                }
            }
        }
        else 
        {
            if (
                (multiplayerPacket->movementActionId >= 0x0 && multiplayerPacket->movementActionId <= 0x3) ||
                (multiplayerPacket->movementActionId >= 0x19 && multiplayerPacket->movementActionId <= 0x28) ||
                (multiplayerPacket->movementActionId >= 0x40 && multiplayerPacket->movementActionId <= 0x43)
            )
            {
                ObjectEventSetHeldMovement(objEvent, multiplayerPacket->movementActionId);
            }
        }
    }
}

void WriteMultiplayerPacketToBuffer(void) {
    if (ReadConnectedByte() != 0) {
        struct MultiplayerPacket* packet = ((struct MultiplayerPacket*) GENERAL_BUFFER_BEGIN_PERSONAL_ADDRESS);
        
        struct ObjectEvent* playerObjEvent = &gObjectEvents[gPlayerAvatar.objectEventId];

        // DMA Write to General Buffer
        packet->mapGroup = gSaveBlock1Ptr->location.mapGroup;
        packet->mapNum = gSaveBlock1Ptr->location.mapNum;
        packet->x = playerObjEvent->currentCoords.x;
        packet->y = playerObjEvent->currentCoords.y;
        packet->movementActionId = playerObjEvent->movementActionId;
    }
}

struct MultiplayerPacket* GetPeerPacket(void) {
    if (ReadConnectedByte() != 0) {
        return ((struct MultiplayerPacket*) GENERAL_BUFFER_BEGIN_PEER_ADDRESS);
    }
}
