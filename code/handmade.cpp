#include "handmade.h"
#include "handmade_render_group.h"
#include "handmade_render_group.cpp"
#include "handmade_random.h"
#include "handmade_world.cpp"
#include "handmade_entity.cpp"
#include "handmade_sim_region.cpp"

inline world_position
WorldPositionFromTilePosition(game_world *world,
  int32 tileX,
  int32 tileY,
  int32 tileZ,
  v3 offset = {})
{
  real32 tileSizeInMeters = 1.4f;

  world_position base = {};
  v3 point = Hadamard(
    V3(tileSizeInMeters, tileSizeInMeters, world->typicalFloorHeight),
    V3((real32)tileX, (real32)tileY, (real32)tileZ));

  point += offset;

  world_position result = MapIntoWorldSpace(world, base, point);

  Assert(IsValidCanonical(world, &result));

  return result;
}

internal loaded_bitmap
MakeEmptyBitmap(int32 width, int32 height, void *memory)
{
  loaded_bitmap result = {};
  result.width = width;
  result.height = height;
  result.pitch = width * BYTES_PER_PIXEL;
  result.memory = memory;
  return result;
}

internal loaded_bitmap
MakeEmptyBitmap(memory_arena *arena,
  int32 width,
  int32 height,
  bool32 clearToZero)
{
  loaded_bitmap result = MakeEmptyBitmap(width, height, NULL);
  int32 totalMemorySize = width * height * BYTES_PER_PIXEL;
  result.memory = PushSize(arena, totalMemorySize);

  if(clearToZero) {
    ZeroSize(result.memory, totalMemorySize);
  }

  return result;
}

internal sim_entity_collision_volume_group *
MakeSimpleCollision(game_state *state, real32 dimX, real32 dimY, real32 dimZ)
{
  memory_arena *arena = &state->worldArena;
  // TODO: Change to using types arena
  sim_entity_collision_volume_group *group
    = PushStruct(arena, sim_entity_collision_volume_group);
  group->volumeCount = 1;
  group->volumes
    = PushArray(arena, group->volumeCount, sim_entity_collision_volume);
  group->totalVolume.offset = V3(0, 0, 0.5f * dimZ);
  group->totalVolume.dim = V3(dimX, dimY, dimZ);
  group->volumes[0] = group->totalVolume;

  return group;
}

internal sim_entity_collision_volume_group *
MakeNullCollision(game_state *state)
{
  memory_arena *arena = &state->worldArena;
  sim_entity_collision_volume_group *group
    = PushStruct(arena, sim_entity_collision_volume_group);
  group->volumeCount = 0;
  group->volumes = 0;
  group->totalVolume.offset = v3{ 0, 0, 0 };
  group->totalVolume.dim = v3{ 0, 0, 0 };
  return group;
}

internal stored_entity *
AddStoredEntity(game_state *state, entity_type type, world_position p)
{
  Assert(state->entityCount < ArrayCount(state->entities));
  stored_entity *stored = state->entities + state->entityCount++;
  stored->p = NullPosition();
  stored->sim.type = type;
  stored->sim.collision = state->nullCollision;

  ChangeEntityLocation(&state->worldArena, &state->world, stored, p);

  return stored;
}

internal stored_entity *
AddStoredEntity(game_state *state,
  entity_type type,
  int32 tileX,
  int32 tileY,
  int32 tileZ,
  v3 offset = {})
{
  world_position p
    = WorldPositionFromTilePosition(&state->world, tileX, tileY, tileZ, offset);
  return AddStoredEntity(state, type, p);
}

internal stored_entity *
AddGroundedEntity(game_state *state,
  entity_type type,
  world_position p,
  sim_entity_collision_volume_group *collision)
{
  stored_entity *entity = AddStoredEntity(state, type, p);
  entity->sim.collision = collision;
  return entity;
}

internal void
InitHitPoints(stored_entity *stored, int value)
{
  stored->sim.hitPointCount = value;

  for(int i = 0; i < value; i++) {
    stored->sim.hitPoints[i].amount = HIT_POINT_AMOUNT;
  }
}

internal stored_entity *
AddStandardRoom(game_state *state, int32 tileX, int32 tileY, int32 tileZ)
{
  world_position p
    = WorldPositionFromTilePosition(&state->world, tileX, tileY, tileZ);
  stored_entity *stored = AddGroundedEntity(state,
    EntityType_Space,
    p,
    state->standardRoomCollision);

  AddFlags(&stored->sim, EntityFlag_Traversable);

  return stored;
}

internal stored_entity *
AddWall(game_state *state, int32 tileX, int32 tileY, int32 tileZ)
{
  world_position p
    = WorldPositionFromTilePosition(&state->world, tileX, tileY, tileZ);
  stored_entity *stored
    = AddGroundedEntity(state, EntityType_Wall, p, state->wallCollision);

  return stored;
}

internal stored_entity *
AddSword(game_state *state)
{
  stored_entity *sword = AddGroundedEntity(state,
    EntityType_Sword,
    NullPosition(),
    state->swordCollision);

  AddFlags(&sword->sim, EntityFlag_Moveable);

  return sword;
}

internal stored_entity *
AddHero(game_state *state)
{
  stored_entity *hero = AddGroundedEntity(state,
    EntityType_Hero,
    state->cameraP,
    state->heroCollision);

  AddFlags(&hero->sim, EntityFlag_Moveable);

  InitHitPoints(hero, 3);

  hero->sim.facingDirection = 0;
  hero->sim.sword.stored = AddSword(state);

  return hero;
}

internal void
AddMonster(game_state *state, int32 tileX, int32 tileY, int32 tileZ)
{
  world_position p
    = WorldPositionFromTilePosition(&state->world, tileX, tileY, tileZ);
  stored_entity *monster
    = AddGroundedEntity(state, EntityType_Monster, p, state->monsterCollision);

  AddFlags(&monster->sim, EntityFlag_Moveable);

  InitHitPoints(monster, 3);
}

internal void
AddStairwell(game_state *state, int32 tileX, int32 tileY, int32 tileZ)
{
  world_position p
    = WorldPositionFromTilePosition(&state->world, tileX, tileY, tileZ);
  stored_entity *stored
    = AddGroundedEntity(state, EntityType_Stairwell, p, state->stairCollision);
  stored->sim.walkableDim = stored->sim.collision->totalVolume.dim.xy;
  stored->sim.walkableHeight = state->world.typicalFloorHeight;
}

internal void
AddFamiliar(game_state *state, int32 tileX, int32 tileY, int32 tileZ)
{
  world_position p
    = WorldPositionFromTilePosition(&state->world, tileX, tileY, tileZ);
  stored_entity *familiar = AddGroundedEntity(state,
    EntityType_Familiar,
    p,
    state->familiarCollision);

  AddFlags(&familiar->sim, EntityFlag_Moveable);
}

#pragma pack(push, 1)
struct bitmap_header {
  char signature[2];
  uint32 fileSize;
  uint32 reserved;
  uint32 dataOffset;

  uint32 size;
  int32 width;
  int32 height;
  uint16 planes;
  uint16 bitsPerPixel;
  uint32 compression;
  uint32 sizeOfBitmap;
  uint32 horzResolution;
  uint32 vertResolution;
  uint32 colorsUsed;
  uint32 colorsImportant;
  uint32 redMask;
  uint32 greenMask;
  uint32 blueMask;
  uint32 alphaMask;
};
#pragma pack(pop)

inline uint8
ProcessPixelWithMask(uint32 Pixel, uint32 Mask)
{
  bit_scan_result result = FindLeastSignificantSetBit(Mask);
  Assert(result.found);
  return (uint8)(Pixel >> result.index);
}

// this is not a compelete BMP loading procedure
// we just need to load sutff from ourself's
// pixels from bottom to top
internal loaded_bitmap
LoadBMP(thread_context *thread,
  debug_platform_read_file ReadFile,
  char *fileName)
{
  loaded_bitmap result = {};

  debug_read_file_result ReadResult = ReadFile(thread, fileName);
  if(ReadResult.memory) {
    bitmap_header *header = (bitmap_header *)ReadResult.memory;
    result.width = header->width;
    result.height = header->height;
    Assert(header->compression == 3);

    result.memory = (uint32 *)((uint8 *)ReadResult.memory + header->dataOffset);

    // we have to restructure pixels
    // expected byte order: BB GG RR AA, 0xAARRGGBB
    uint32 *pixel = (uint32 *)result.memory;
    for(int y = 0; y < header->height; y++) {
      for(int x = 0; x < header->width; x++) {
        bit_scan_result redScan = FindLeastSignificantSetBit(header->redMask);
        bit_scan_result greenScan
          = FindLeastSignificantSetBit(header->greenMask);
        bit_scan_result blueScan = FindLeastSignificantSetBit(header->blueMask);
        bit_scan_result alphaScan
          = FindLeastSignificantSetBit(header->alphaMask);

        Assert(redScan.found);
        Assert(greenScan.found);
        Assert(blueScan.found);
        Assert(alphaScan.found);

        int32 redShift = (int32)redScan.index;
        int32 greenShift = (int32)greenScan.index;
        int32 blueShift = (int32)blueScan.index;
        int32 alphaShift = (int32)alphaScan.index;

        uint32 c = *pixel;

        real32 a = (real32)((c & header->alphaMask) >> alphaShift);
        real32 ra = a / 255.0f;

        real32 r = ra * (real32)((c & header->redMask) >> redShift);
        real32 g = ra * (real32)((c & header->greenMask) >> greenShift);
        real32 b = ra * (real32)((c & header->blueMask) >> blueShift);

        // clang-format off
        *pixel++ = ((uint32)(a + 0.5f) << 24) |
          ((uint32)(r + 0.5f) << 16) |
          ((uint32)(g + 0.5f) << 8) |
          ((uint32)(b + 0.5f));
        // clang-format on
      }
    }
  }

  result.pitch = result.width * BYTES_PER_PIXEL;

  return result;
}

internal void
InitializeArena(memory_arena *arena, size_t size, void *base)
{
  arena->size = size;
  arena->base = (uint8 *)base;
  arena->used = 0;
}

internal void
FillGroundBuffer(game_state *state,
  transient_state *tranState,
  ground_buffer *groundBuffer,
  world_position chunkP)
{
  loaded_bitmap *buffer = &groundBuffer->bitmap;
  groundBuffer->p = chunkP;

  for(int offsetY = -1; offsetY <= 1; offsetY++) {
    for(int offsetX = -1; offsetX <= 1; offsetX++) {
      int chunkX = chunkP.chunkX + offsetX;
      int chunkY = chunkP.chunkY + offsetY;
      int chunkZ = chunkP.chunkZ;

      random_series series
        = RandomSeed(139 * chunkX + 593 * chunkY + 329 * chunkZ);

      real32 width = (real32)buffer->width;
      real32 height = (real32)buffer->height;
      v2 chunkOffset = { width * offsetX, height * offsetY };

      for(uint32 i = 0; i < 100; i++) {
        loaded_bitmap *stamp;

        if(RandomChoice(&series, 2)) {
          stamp
            = state->ground + RandomChoice(&series, ArrayCount(state->ground));
        } else {
          stamp
            = state->grass + RandomChoice(&series, ArrayCount(state->grass));
        }

        v2 center = V2(RandomUnilateral(&series) * width,
          RandomUnilateral(&series) * height);
        v2 offset = 0.5f * V2(stamp->width, stamp->height);
        v2 p = chunkOffset + center - offset;
        DrawBitmap(buffer, stamp, p);
      }
    }
  }

  for(int offsetY = -1; offsetY <= 1; offsetY++) {
    for(int offsetX = -1; offsetX <= 1; offsetX++) {
      int chunkX = chunkP.chunkX + offsetX;
      int chunkY = chunkP.chunkY + offsetY;
      int chunkZ = chunkP.chunkZ;

      random_series series
        = RandomSeed(139 * chunkX + 593 * chunkY + 329 * chunkZ);

      real32 width = (real32)buffer->width;
      real32 height = (real32)buffer->height;
      v2 chunkOffset = { width * offsetX, height * offsetY };

      for(uint32 i = 0; i < 100; i++) {
        loaded_bitmap *stamp
          = state->tuft + RandomChoice(&series, ArrayCount(state->tuft));

        v2 center = V2(RandomUnilateral(&series) * width,
          RandomUnilateral(&series) * height);
        v2 offset = 0.5f * V2(stamp->width, stamp->height);
        v2 p = chunkOffset + center - offset;
        DrawBitmap(buffer, stamp, p);
      }
    }
  }
}

internal void
DrawHitPoints(sim_entity *entity, render_group *renderGroup)
{
  if(entity->hitPointCount > 0) {
    v2 dim = { 0.2f, 0.16f };
    real32 spanX = 1.5f * dim.x;
    real32 startX = -0.5f * (entity->hitPointCount - 1) * spanX;
    real32 startY = -0.75f * entity->collision->totalVolume.dim.y;
    for(uint32 hitPointIndex = 0; hitPointIndex < entity->hitPointCount;
        hitPointIndex++) {
      hit_point *hitPoint = entity->hitPoints + hitPointIndex;
      v3 color = v3{ 0.25f, 0.25f, 0.25f };
      if(hitPoint->amount > 0) {
        color = v3{ 1.0f, 0.0f, 0.0f };
      }
      PushRect(renderGroup,
        V2(startX + spanX * hitPointIndex, startY),
        V2(0, 0),
        dim,
        color);
    }
  }
}

internal void
InitializeWorld(game_world *world,
  real32 typicalFloorHeight,
  v3 chunkDimInMeters)
{
  world->typicalFloorHeight = typicalFloorHeight;
  world->chunkDimInMeters = chunkDimInMeters;
}

extern "C" GAME_UPDATE_VIDEO(GameUpdateVideo)
{
  Assert((&input->controllers[0].terminator - &input->controllers[0].buttons[0])
    == ArrayCount(input->controllers[0].buttons));
  Assert(sizeof(game_state) <= memory->permanentStorageSize);

  game_state *state = (game_state *)(memory->permanentStorage);
  memory_arena *worldArena = &state->worldArena;
  game_world *world = &state->world;

  // in pixels
  uint32 groundBufferWidth = 256;
  uint32 groundBufferHeight = 256;

  real32 typicalFloorHeight = 3.0f;

  // real32 TileSizeInPixels = 60.0f;
  // real32 tileSizeInMeters = 1.4f;
  // real32 tileDepthInMeters = 3.0f;

  real32 metersToPixels = 42.0f;
  real32 pixelsToMeters = 1 / metersToPixels;
  state->metersToPixels = metersToPixels;
  state->pixelsToMeters = pixelsToMeters;

  v3 chunkDimInMeters = { pixelsToMeters * groundBufferWidth,
    pixelsToMeters * groundBufferHeight,
    typicalFloorHeight };

  if(!memory->isInitialized) {
    memory->isInitialized = true;

    state->background = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_background.bmp");
    state->shadow = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_shadow.bmp");
    state->tree
      = LoadBMP(thread, memory->debugPlatformReadFile, "test2/tree00.bmp");
    state->sword
      = LoadBMP(thread, memory->debugPlatformReadFile, "test2/rock03.bmp");

    state->grass[0]
      = LoadBMP(thread, memory->debugPlatformReadFile, "test2/grass00.bmp");
    state->grass[1]
      = LoadBMP(thread, memory->debugPlatformReadFile, "test2/grass01.bmp");

    state->ground[0]
      = LoadBMP(thread, memory->debugPlatformReadFile, "test2/ground00.bmp");
    state->ground[1]
      = LoadBMP(thread, memory->debugPlatformReadFile, "test2/ground01.bmp");
    state->ground[2]
      = LoadBMP(thread, memory->debugPlatformReadFile, "test2/ground02.bmp");
    state->ground[3]
      = LoadBMP(thread, memory->debugPlatformReadFile, "test2/ground03.bmp");

    state->tuft[0]
      = LoadBMP(thread, memory->debugPlatformReadFile, "test2/tuft00.bmp");
    state->tuft[1]
      = LoadBMP(thread, memory->debugPlatformReadFile, "test2/tuft01.bmp");
    state->tuft[2]
      = LoadBMP(thread, memory->debugPlatformReadFile, "test2/tuft02.bmp");

    hero_bitmaps *heroBitmaps = state->heroBitmaps;
    heroBitmaps->align = V2(-72, -35);
    heroBitmaps->head = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_right_head.bmp");
    heroBitmaps->cape = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_right_cape.bmp");
    heroBitmaps->torso = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_right_torso.bmp");

    heroBitmaps++;
    heroBitmaps->align = V2(-72, -35);
    heroBitmaps->head = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_back_head.bmp");
    heroBitmaps->cape = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_back_cape.bmp");
    heroBitmaps->torso = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_back_torso.bmp");

    heroBitmaps++;
    heroBitmaps->align = V2(-72, -35);
    heroBitmaps->head = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_left_head.bmp");
    heroBitmaps->cape = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_left_cape.bmp");
    heroBitmaps->torso = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_left_torso.bmp");

    heroBitmaps++;
    heroBitmaps->align = V2(-72, -35);
    heroBitmaps->head = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_front_head.bmp");
    heroBitmaps->cape = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_front_cape.bmp");
    heroBitmaps->torso = LoadBMP(thread,
      memory->debugPlatformReadFile,
      "test/test_hero_front_torso.bmp");

    InitializeArena(worldArena,
      memory->permanentStorageSize - sizeof(game_state),
      (uint8 *)memory->permanentStorage + sizeof(game_state));

    InitializeWorld(world, typicalFloorHeight, chunkDimInMeters);

    real32 tileSizeInMeters = 1.4f;
    real32 tileDepthInMeters = typicalFloorHeight;
    int tilesPerWidth = 17;
    int tilesPerHeight = 9;

    state->wallCollision = MakeSimpleCollision(state,
      tileSizeInMeters,
      tileSizeInMeters,
      typicalFloorHeight);
    state->standardRoomCollision = MakeSimpleCollision(state,
      tilesPerWidth * tileSizeInMeters,
      tilesPerHeight * tileSizeInMeters,
      0.9f * tileDepthInMeters);
    state->heroCollision = MakeSimpleCollision(state, 1.0f, 0.5f, 1.2f);
    state->familiarCollision = MakeSimpleCollision(state, 1.0f, 0.5f, 0.5f);
    state->swordCollision = MakeSimpleCollision(state, 1.0f, 0.5f, 0.1f);
    state->monsterCollision = MakeSimpleCollision(state, 1.0f, 0.5f, 0.5f);
    state->stairCollision = MakeSimpleCollision(state,
      tileSizeInMeters,
      2.0f * tileSizeInMeters,
      1.1f * tileDepthInMeters);
    state->nullCollision = MakeNullCollision(state);

    int32 screenBaseX = 0;
    int32 screenBaseY = 0;
    int32 screenBaseZ = 0;
    int32 screenX = screenBaseX;
    int32 screenY = screenBaseY;

    int32 randomIndex = 0;
    bool32 doorLeft = false;
    bool32 doorRight = false;
    bool32 doorTop = false;
    bool32 doorBottom = false;
    bool32 doorUp = false;
    bool32 doorDown = false;
    int32 absTileZ = 0;

    // world generation
    random_series series = RandomSeed(0x100);
    for(int screenIndex = 0; screenIndex < 10; screenIndex++) {
      // 0: left
      // 1: bottom
      // 2: up/down

      // int randomValue = RandomChoice(&series, (doorUp || doorDown) ? 2 : 3);
      // if(screenIndex == 0) { // make a stairwell
      //   randomValue = 2;
      // }

      int randomValue = RandomChoice(&series, 2);

      switch(randomValue) {
        case 0: {
          doorLeft = true;
        } break;

        case 1: {
          doorBottom = true;
        } break;

        case 2: {
          if(absTileZ == screenBaseZ) {
            doorUp = true;
          } else {
            doorDown = true;
          }
        } break;
      }

      AddStandardRoom(state,
        screenX * tilesPerWidth + tilesPerWidth / 2,
        screenY * tilesPerHeight + tilesPerHeight / 2,
        absTileZ);

      // 1: empty
      // 2: wall
      for(int32 tileY = 0; tileY < tilesPerHeight; tileY++) {
        for(int32 tileX = 0; tileX < tilesPerWidth; tileX++) {
          int32 absTileX = screenX * tilesPerWidth + tileX;
          int32 absTileY = screenY * tilesPerHeight + tileY;
          int32 tileValue = 1;

          if(tileX == 0) {
            tileValue = 2;
            if(doorLeft && tileY == (tilesPerHeight / 2)) {
              tileValue = 1;
            }
          }
          if(tileX == (tilesPerWidth - 1)) {
            tileValue = 2;
            if(doorRight && tileY == (tilesPerHeight / 2)) {
              tileValue = 1;
            }
          }
          if(tileY == 0) {
            tileValue = 2;
            if(doorBottom && tileX == (tilesPerWidth / 2)) {
              tileValue = 1;
            }
          }
          if(tileY == (tilesPerHeight - 1)) {
            tileValue = 2;
            if(doorTop && tileX == (tilesPerWidth / 2)) {
              tileValue = 1;
            }
          }

          if(tileValue == 2) {
            // if(screenIndex == 0) {
            AddWall(state, absTileX, absTileY, absTileZ);
            // }
          }

          if(tileX == 10 && tileY == 4 && (doorUp || doorDown)) {
            AddStairwell(state,
              absTileX,
              absTileY,
              doorDown ? absTileZ - 1 : absTileZ);
          }
        }
      }

      doorRight = doorLeft;
      doorTop = doorBottom;
      doorLeft = false;
      doorBottom = false;

      if(randomValue == 0) {
        screenX--;
      } else if(randomValue == 1) {
        screenY--;
      } else if(randomValue == 2) {
        if(absTileZ == screenBaseZ) {
          doorDown = true;
          doorUp = false;
          absTileZ++;
        } else {
          doorUp = true;
          doorDown = false;
          absTileZ = screenBaseZ;
        }
      } else {
        doorDown = false;
        doorUp = false;
      }
    }

    world_position newCameraP = {};
    int32 cameraTileX = screenBaseX * tilesPerWidth + tilesPerWidth / 2;
    int32 cameraTileY = screenBaseY * tilesPerHeight + tilesPerHeight / 2;
    int32 cameraTileZ = 0;
    newCameraP = WorldPositionFromTilePosition(world,
      cameraTileX,
      cameraTileY,
      cameraTileZ);
    state->cameraP = newCameraP;

    AddMonster(state, cameraTileX - 2, cameraTileY + 2, cameraTileZ);

    // AddFamiliar(state, cameraTileX - 2, cameraTileY - 2, cameraTileZ);
  }

  transient_state *tranState = (transient_state *)(memory->transientStorage);
  if(!tranState->isInitialized) {
    tranState->isInitialized = true;
    InitializeArena(&tranState->tranArena,
      memory->transientStorageSize - sizeof(transient_state),
      (uint8 *)memory->transientStorage + sizeof(transient_state));

    int32 groundWidth = 256;
    int32 groundHeight = 256;
    int32 totalMemorySize = groundWidth * groundHeight * BYTES_PER_PIXEL;
    tranState->groundWidth = groundWidth;
    tranState->groundHeight = groundHeight;
    tranState->groundPitch = groundWidth * BYTES_PER_PIXEL;
    tranState->groundBufferCount = 32;
    tranState->groundBuffers = PushArray(&tranState->tranArena,
      tranState->groundBufferCount,
      ground_buffer);

    for(uint32 groundIndex = 0; groundIndex < tranState->groundBufferCount;
        groundIndex++) {
      ground_buffer *groundBuffer = tranState->groundBuffers + groundIndex;
      groundBuffer->p = NullPosition();
      groundBuffer->bitmap = MakeEmptyBitmap(groundWidth,
        groundHeight,
        PushSize(&tranState->tranArena, totalMemorySize));
    }
  }

  for(int controllerIndex = 0; controllerIndex < ArrayCount(input->controllers);
      controllerIndex++) {
    game_controller_input *controller = &input->controllers[controllerIndex];
    game_controller_input *oldController
      = &oldInput->controllers[controllerIndex];
    controlled_hero *conHero = state->controlledHeroes + controllerIndex;

    if(oldController->leftShoulder.isEndedDown == 0
      && controller->leftShoulder.isEndedDown != 0) {
      state->debugDrawBoundary = !state->debugDrawBoundary;
    }

    if(conHero->stored) {
      conHero->ddP = {};
      conHero->dSword = {};
      conHero->dZ = 0;

      if(controller->isAnalog) {
        conHero->ddP = { controller->stickAverageX, controller->stickAverageY };
      } else {
        if(controller->moveUp.isEndedDown) {
          conHero->ddP.y = 1.0f;
        }
        if(controller->moveDown.isEndedDown) {
          conHero->ddP.y = -1.0f;
        }
        if(controller->moveLeft.isEndedDown) {
          conHero->ddP.x = -1.0f;
        }
        if(controller->moveRight.isEndedDown) {
          conHero->ddP.x = 1.0f;
        }
      }

      if(controller->start.isEndedDown) {
        conHero->dZ = 3.0f;
      }

      if(controller->actionUp.isEndedDown) {
        conHero->dSword.y = 1.0f;
      }
      if(controller->actionDown.isEndedDown) {
        conHero->dSword.y = -1.0f;
      }
      if(controller->actionLeft.isEndedDown) {
        conHero->dSword.x = -1.0f;
      }
      if(controller->actionRight.isEndedDown) {
        conHero->dSword.x = 1.0f;
      }
    } else {
      if(controller->start.isEndedDown) {
        stored_entity *hero = AddHero(state);
        conHero->stored = hero;

        if(!state->cameraFollowingEntity) {
          state->cameraFollowingEntity = hero;
        }
      }
    }
  }

  loaded_bitmap _drawBuffer = {};
  loaded_bitmap *drawBuffer = &_drawBuffer;
  drawBuffer->memory = buffer->memory;
  drawBuffer->width = buffer->width;
  drawBuffer->height = buffer->height;
  drawBuffer->pitch = buffer->pitch;

  //
  // Render
  //
  SaveArena(&tranState->tranArena);

  render_group *renderGroup
    = AllocateRenderGroup(&tranState->tranArena, Megabytes(4), metersToPixels);

  v2 screenCenter = 0.5f * v2{ (real32)buffer->width, (real32)buffer->height };

  // background
  DrawRectangle(drawBuffer,
    V2(0, 0),
    V2((real32)buffer->width, (real32)buffer->height),
    V4(1.0f, 0.5f, 0.0f, 1.0f));

  real32 screenWidthInMeters = drawBuffer->width * pixelsToMeters;
  real32 screenHeightInMeters = drawBuffer->height * pixelsToMeters;
  rectangle3 cameraBounds = RectCenterDim(V3(0, 0, 0),
    V3(screenWidthInMeters, screenHeightInMeters, 0));

  // clear ground buffers when reloaded
  if(input->executableReloaded) {
    for(uint32 i = 0; i < tranState->groundBufferCount; i++) {
      ground_buffer *buffer = tranState->groundBuffers + i;
      buffer->p = NullPosition();
    }
  }

  // render ground buffers
  {
    for(uint32 groundIndex = 0; groundIndex < tranState->groundBufferCount;
        groundIndex++) {
      ground_buffer *buffer = tranState->groundBuffers + groundIndex;
      if(IsValid(&buffer->p)) {
        loaded_bitmap *bitmap = &buffer->bitmap;
        v3 delta = SubtractPosition(&state->world, buffer->p, state->cameraP);

        PushBitmap(renderGroup,
          bitmap,
          delta.xy,
          -0.5f * V2(bitmap->width, bitmap->height));
      }
    }
  }

  // fill ground buffers
  {

    world_position minChunk
      = MapIntoWorldSpace(world, state->cameraP, GetMinCorner(cameraBounds));
    world_position maxChunk
      = MapIntoWorldSpace(world, state->cameraP, GetMaxCorner(cameraBounds));

    for(int32 chunkZ = minChunk.chunkZ; chunkZ <= maxChunk.chunkZ; chunkZ++) {
      for(int32 chunkY = minChunk.chunkY; chunkY <= maxChunk.chunkY; chunkY++) {
        for(int32 chunkX = minChunk.chunkX; chunkX <= maxChunk.chunkX;
            chunkX++) {
          world_chunk *chunk = GetWorldChunk(world, chunkX, chunkY, chunkZ);

          {
            world_position chunkCenterP
              = CenteredChunkPoint(world, chunkX, chunkY, chunkZ);
            v3 chunkRel = SubtractPosition(world, chunkCenterP, state->cameraP);

            v2 renderCenter = screenCenter + metersToPixels * chunkRel.xy;
            v2 renderDim = metersToPixels * world->chunkDimInMeters.xy;

            // DrawRectangleOutline(drawBuffer,
            //   renderCenter - 0.5f * renderDim,
            //   renderCenter + 0.5f * renderDim,
            //   V3(1, 1, 0));

            ground_buffer *furthestBuffer = 0;
            real32 furthestLengthSq = 0.0f;

            for(uint32 i = 0; i < tranState->groundBufferCount; i++) {
              ground_buffer *buffer = tranState->groundBuffers + i;

              if(AreInSameChunk(world, &chunkCenterP, &buffer->p)) {
                furthestBuffer = 0;
                break;
              }

              if(!IsValid(&buffer->p)) {
                furthestBuffer = buffer;
                break;
              } else {
                v3 bufferRel
                  = SubtractPosition(world, buffer->p, state->cameraP);
                real32 lengthSq = LengthSq(bufferRel.xy);
                if(lengthSq > furthestLengthSq) {
                  furthestBuffer = buffer;
                  furthestLengthSq = lengthSq;
                }
              }
            }

            if(furthestBuffer != 0) {
              FillGroundBuffer(state, tranState, furthestBuffer, chunkCenterP);
            }
          }
        }
      }
    }
  }

  v3 simExpansion = { 15.0f, 15.0f, 15.0f };
  rectangle3 simBounds = AddRadius(cameraBounds, simExpansion);

  sim_region *simRegion = BeginSim(&tranState->tranArena,
    state,
    state->cameraP,
    simBounds,
    input->dt);

  // entities
  for(uint32 index = 0; index < simRegion->entityCount; index++) {
    sim_entity *entity = simRegion->entities + index;

    if(!entity->updatable) {
      continue;
    }

    hero_bitmaps *heroBitmaps = &state->heroBitmaps[entity->facingDirection];
    render_basis *basis = PushStruct(&tranState->tranArena, render_basis);
    renderGroup->defaultBasis = basis;

    move_spec moveSpec = {};
    v3 ddP = {};
    v3 oldSwordP = {};

    switch(entity->type) {
      case EntityType_Hero: {
        for(int conIndex = 0; conIndex < ArrayCount(state->controlledHeroes);
            conIndex++) {
          controlled_hero *conHero = state->controlledHeroes + conIndex;
          if(entity->stored == conHero->stored) {
            moveSpec = HeroMoveSpec();
            ddP = V3(conHero->ddP, 0.0f);

            if(conHero->dZ != 0.0f) {
              entity->dP.z = conHero->dZ;
            }

            if(conHero->dSword.x != 0.0f || conHero->dSword.y != 0.0f) {
              sim_entity *sword = entity->sword.entity;
              Assert(sword);

              if(HasFlag(sword, EntityFlag_NonSpatial)) {
                sword->distanceLimit = 5.0f;
                MakeEntitySpatial(sword, entity->p, 8.0f * conHero->dSword);
              }
            }
          }
        }

        real32 shadowAlpha = 1.0f - 0.5f * entity->p.z;
        if(shadowAlpha < 0.0f) {
          shadowAlpha = 0.0f;
        }

        PushBitmap(renderGroup,
          &state->shadow,
          V2(0, 0),
          V2(-70, -37),
          0.0f,
          shadowAlpha);

        PushBitmap(renderGroup,
          &heroBitmaps->cape,
          V2(0, 0),
          heroBitmaps->align);

        PushBitmap(renderGroup,
          &heroBitmaps->head,
          V2(0, 0),
          heroBitmaps->align);

        DrawHitPoints(entity, renderGroup);
      } break;

      case EntityType_Sword: {
        moveSpec.ddPScale = 0.0f;
        moveSpec.drag = 0.0f;

        if(entity->distanceLimit <= 0.0f) {
          MakeEntityNonSpatial(entity);
          ClearCollisionRulesFor(state, entity->stored);
        }

        PushBitmap(renderGroup, &state->sword, V2(0, 0), V2(-28, -22));
      } break;

      case EntityType_Wall: {
        PushBitmap(renderGroup, &state->tree, V2(0, 0), V2(-40, -40), 1);
      } break;

      case EntityType_Space: {
        for(uint32 volumeIndex = 0;
            volumeIndex < entity->collision->volumeCount;
            volumeIndex++) {
          sim_entity_collision_volume *volume
            = entity->collision->volumes + volumeIndex;
          // PushRectOutline(&renderGroup,
          //   v2{},
          //   volume->dim.xy,
          //   v3{ 0.0f, 0, 1.0f });
        }
      } break;

      case EntityType_Monster: {
        PushBitmap(renderGroup, &state->shadow, V2(0, 0), V2(-70, -37));
        PushBitmap(renderGroup,
          &heroBitmaps->torso,
          V2(0, 0),
          heroBitmaps->align);
        DrawHitPoints(entity, renderGroup);
      } break;

      case EntityType_Stairwell: {
        PushRect(renderGroup,
          V2(0, 0),
          V2(0, 0),
          entity->walkableDim,
          V3(1.0f, 0, 0));
      } break;

      case EntityType_Familiar: {
        sim_entity *closestHero = NULL;
        real32 closestDSq = Square(10.0f);

        for(uint32 i = 0; i < simRegion->entityCount; i++) {
          sim_entity *testEntity = simRegion->entities + i;
          if(testEntity->type == EntityType_Hero) {
            real32 testDSq = LengthSq(testEntity->p - entity->p);
            if(testDSq < closestDSq) {
              closestDSq = testDSq;
              closestHero = testEntity;
            }
          }
        }

        if(closestHero && (closestDSq >= Square(3.0f))) {
          ddP = closestHero->p - entity->p;
        }

        moveSpec = HeroMoveSpec();
        PushBitmap(renderGroup, &state->shadow, V2(0, 0), V2(-70, -37), 0.0f);
        PushBitmap(renderGroup,
          &heroBitmaps->head,
          V2(0, 0),
          heroBitmaps->align);
      } break;

      default: {
        InvalidCodePath;
      }
    }

    if(!HasFlag(entity, EntityFlag_NonSpatial)
      && HasFlag(entity, EntityFlag_Moveable)) {
      MoveEntity(state, simRegion, &moveSpec, entity, input->dt, ddP);
    }

    basis->p = entity->p;

    // debug draw boundary
    if(state->debugDrawBoundary && entity->type != EntityType_Space) {
      v2 dim = entity->collision->totalVolume.dim.xy;
      PushRect(renderGroup, V2(0, 0), V2(0, 0), dim, V3(1.0f, 1.0f, 0.0f));
    }
  }

  RenderGroupToOutput(renderGroup, drawBuffer);

  // debug: show current floors
  for(int i = 0; i < simRegion->origin.chunkZ + 1; i++) {
    v2 start = { 10 + (real32)i * 15, (real32)buffer->height - 20 };
    DrawRectangle(drawBuffer,
      start,
      start + v2{ 10, 10 },
      V4(0.0f, 1.0f, 0.0f, 1.0f));
  }

  EndSim(simRegion, state);
  RestoreArena(&tranState->tranArena);
  CheckArena(&tranState->tranArena);
}

extern "C" GAME_UPDATE_AUDIO(GameUpdateAudio) {}
