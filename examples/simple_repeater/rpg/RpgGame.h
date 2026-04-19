#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include "RpgTypes.h"

class RpgWorldState;

class RpgGame {
  rpg::PlayerState players[rpg::MAX_PLAYERS];

  static bool startsWithCommand(const char* text, const char* command);
  static bool hasCooldownElapsed(uint32_t now_ms, uint32_t last_action_ms);
  static const char* getLocationName(uint8_t location);
  static uint16_t clampAdd(uint16_t value, uint16_t amount);
  static uint16_t clampResource(uint16_t value, uint16_t amount, uint16_t cap);
  static uint16_t getStorageCap(const rpg::PlayerState& player);
  static uint8_t getMaxEquipmentTier(const rpg::PlayerState& player);
  static uint16_t getXpNeededForLevel(uint8_t level);
  static bool isBlank(const char* text);
  static bool isHigherRank(const rpg::PlayerState& lhs, const rpg::PlayerState& rhs);
  static void formatPlayerTag(const rpg::PlayerState& player, char* dest, size_t dest_size);

  rpg::PlayerState* findPlayer(const uint8_t* player_id, size_t player_id_len);
  rpg::PlayerState* getOrCreatePlayer(const uint8_t* player_id, size_t player_id_len);
  void resetPlayer(rpg::PlayerState& player, const uint8_t* player_id, size_t player_id_len);
  void updateKnownName(rpg::PlayerState& player, const char* known_name);
  void awardXp(rpg::PlayerState& player, uint16_t xp_amount);
  uint8_t buildRankedOrder(uint8_t order[], uint8_t max_count) const;
  int findPlayerIndex(const rpg::PlayerState* player) const;

  void formatHelp(char* reply, size_t reply_size) const;
  void formatStats(const rpg::PlayerState& player, char* reply, size_t reply_size) const;
  void formatInventory(const rpg::PlayerState& player, char* reply, size_t reply_size) const;
  void formatCamp(const rpg::PlayerState& player, char* reply, size_t reply_size) const;
  void formatShop(char* reply, size_t reply_size) const;
  void formatCraft(char* reply, size_t reply_size) const;
  void formatBoss(const rpg::PlayerState& player, RpgWorldState* world_state, uint32_t now_ms,
                  char* reply, size_t reply_size) const;
  void formatTop(char* reply, size_t reply_size) const;
  void formatPlayers(char* reply, size_t reply_size) const;
  void formatMe(const rpg::PlayerState& player, char* reply, size_t reply_size) const;
  void handleTravel(rpg::PlayerState& player, const char* arg, char* reply, size_t reply_size) const;
  void handleGather(rpg::PlayerState& player, uint32_t now_ms, mesh::RNG& rng, RpgWorldState* world_state,
                    char* reply, size_t reply_size);
  void handleFight(rpg::PlayerState& player, uint32_t now_ms, mesh::RNG& rng, char* reply, size_t reply_size);
  void handleHeal(rpg::PlayerState& player, char* reply, size_t reply_size);
  void handleBuy(rpg::PlayerState& player, const char* arg, char* reply, size_t reply_size);
  void handleCraft(rpg::PlayerState& player, const char* arg, char* reply, size_t reply_size);
  void handleCollect(rpg::PlayerState& player, uint32_t now_ms, mesh::RNG& rng, char* reply, size_t reply_size);
  void handleBossFight(rpg::PlayerState& player, uint32_t now_ms, mesh::RNG& rng, RpgWorldState* world_state,
                       char* reply, size_t reply_size);
  void handleRename(rpg::PlayerState& player, const char* arg, char* reply, size_t reply_size);

public:
  RpgGame();
  bool hasPlayer(const uint8_t* player_id, size_t player_id_len) const;
  bool applyConvoyLoot(const uint8_t player_key[8], uint16_t wood, uint16_t ore, uint16_t herbs,
                       uint16_t relics, uint16_t gold, char* reply, size_t reply_size);

  bool handleCommand(const uint8_t* player_id, size_t player_id_len, uint32_t now_ms, mesh::RNG& rng,
                     const char* command, char* reply, size_t reply_size, const char* known_name = NULL,
                     RpgWorldState* world_state = NULL);
};
