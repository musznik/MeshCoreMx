#pragma once

#include <Arduino.h>
#include <stdint.h>

class RpgWorldState {
  uint16_t _wood;
  uint16_t _ore;
  uint16_t _herbs;
  uint16_t _relics;
  uint16_t _gold;
  uint32_t _last_update_ms;
  uint32_t _boss_respawn_at_ms;

  void sync(uint32_t now_ms);
  uint16_t takeFromPool(uint16_t& pool, uint16_t amount, uint32_t now_ms);

public:
  RpgWorldState();

  uint16_t takeWood(uint16_t amount, uint32_t now_ms);
  uint16_t takeOre(uint16_t amount, uint32_t now_ms);
  uint16_t takeHerbs(uint16_t amount, uint32_t now_ms);
  uint16_t takeRelics(uint16_t amount, uint32_t now_ms);
  uint16_t takeGold(uint16_t amount, uint32_t now_ms);

  bool isBossAlive(uint32_t now_ms);
  void defeatBoss(uint32_t now_ms);
  uint32_t getBossCooldownRemainingMs(uint32_t now_ms);

  uint8_t getRemoteFlags(uint32_t now_ms);
  uint8_t getResourceSummary(uint32_t now_ms);
  void formatStatus(char* reply, size_t reply_size, uint32_t now_ms);
};
