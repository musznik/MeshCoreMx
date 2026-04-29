#pragma once

#include <stdint.h>

namespace rpg {

static const uint8_t PLAYER_KEY_SIZE = 8;
static const uint8_t PLAYER_NAME_SIZE = 16;
static const uint8_t MAX_PLAYERS = 12;
static const uint32_t ACTION_COOLDOWN_MS = 4000;

enum Location : uint8_t {
  LOCATION_CAMP = 0,
  LOCATION_FOREST = 1,
  LOCATION_MINE = 2,
  LOCATION_RUINS = 3
};

struct PlayerState {
  bool used;
  uint8_t key[PLAYER_KEY_SIZE];
  char display_name[PLAYER_NAME_SIZE + 1];
  bool custom_name;
  uint8_t level;
  uint8_t hp;
  uint8_t max_hp;
  uint8_t weapon_tier;
  uint8_t armor_tier;
  uint8_t tool_tier;
  uint8_t campfire_level;
  uint8_t workbench_level;
  uint8_t storage_level;
  uint8_t cart_level;
  uint8_t watchtower_level;
  uint8_t boss_wins;
  uint16_t xp;
  uint16_t gold;
  uint16_t wood;
  uint16_t ore;
  uint16_t herbs;
  uint16_t relics;
  uint32_t last_action_ms;
  uint32_t last_collect_ms;
  uint8_t location;
};

}
