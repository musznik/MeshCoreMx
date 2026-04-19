#include <stdio.h>
#include <string.h>

#include "RpgRemoteProtocol.h"

namespace {

static const uint32_t WORLD_TICK_MS = 10UL * 60UL * 1000UL;
static const uint32_t BOSS_RESPAWN_MS = 90UL * 60UL * 1000UL;

static const uint16_t WORLD_WOOD_CAP = 24;
static const uint16_t WORLD_ORE_CAP = 18;
static const uint16_t WORLD_HERBS_CAP = 12;
static const uint16_t WORLD_RELICS_CAP = 6;
static const uint16_t WORLD_GOLD_CAP = 30;

static uint16_t refillPool(uint16_t value, uint16_t cap, uint16_t gain_per_tick, uint32_t ticks) {
  uint32_t next = (uint32_t)value + ((uint32_t)gain_per_tick * ticks);
  return next > cap ? cap : (uint16_t)next;
}

}

RpgWorldState::RpgWorldState()
  : _wood(WORLD_WOOD_CAP), _ore(WORLD_ORE_CAP), _herbs(WORLD_HERBS_CAP), _relics(WORLD_RELICS_CAP),
    _gold(WORLD_GOLD_CAP), _last_update_ms(0), _boss_respawn_at_ms(0) {
}

void RpgWorldState::sync(uint32_t now_ms) {
  if (_last_update_ms == 0) {
    _last_update_ms = now_ms;
  } else {
    uint32_t elapsed = (uint32_t)(now_ms - _last_update_ms);
    uint32_t ticks = elapsed / WORLD_TICK_MS;
    if (ticks > 0) {
      _last_update_ms += ticks * WORLD_TICK_MS;
      _wood = refillPool(_wood, WORLD_WOOD_CAP, 2, ticks);
      _ore = refillPool(_ore, WORLD_ORE_CAP, 2, ticks);
      _herbs = refillPool(_herbs, WORLD_HERBS_CAP, 1, ticks);
      _relics = refillPool(_relics, WORLD_RELICS_CAP, 1, ticks / 2);
      _gold = refillPool(_gold, WORLD_GOLD_CAP, 3, ticks);
    }
  }

  if (_boss_respawn_at_ms != 0 && (int32_t)(now_ms - _boss_respawn_at_ms) >= 0) {
    _boss_respawn_at_ms = 0;
  }
}

uint16_t RpgWorldState::takeFromPool(uint16_t& pool, uint16_t amount, uint32_t now_ms) {
  sync(now_ms);
  uint16_t taken = amount < pool ? amount : pool;
  pool -= taken;
  return taken;
}

uint16_t RpgWorldState::takeWood(uint16_t amount, uint32_t now_ms) {
  return takeFromPool(_wood, amount, now_ms);
}

uint16_t RpgWorldState::takeOre(uint16_t amount, uint32_t now_ms) {
  return takeFromPool(_ore, amount, now_ms);
}

uint16_t RpgWorldState::takeHerbs(uint16_t amount, uint32_t now_ms) {
  return takeFromPool(_herbs, amount, now_ms);
}

uint16_t RpgWorldState::takeRelics(uint16_t amount, uint32_t now_ms) {
  return takeFromPool(_relics, amount, now_ms);
}

uint16_t RpgWorldState::takeGold(uint16_t amount, uint32_t now_ms) {
  return takeFromPool(_gold, amount, now_ms);
}

bool RpgWorldState::isBossAlive(uint32_t now_ms) {
  sync(now_ms);
  return _boss_respawn_at_ms == 0;
}

void RpgWorldState::defeatBoss(uint32_t now_ms) {
  sync(now_ms);
  _boss_respawn_at_ms = now_ms + BOSS_RESPAWN_MS;
}

uint32_t RpgWorldState::getBossCooldownRemainingMs(uint32_t now_ms) {
  sync(now_ms);
  if (_boss_respawn_at_ms == 0) {
    return 0;
  }
  return (uint32_t)(_boss_respawn_at_ms - now_ms);
}

uint8_t RpgWorldState::getRemoteFlags(uint32_t now_ms) {
  uint8_t flags = rpg::REMOTE_FLAG_REMOTE_API | rpg::REMOTE_FLAG_SHARED_WORLD;
  if (isBossAlive(now_ms)) {
    flags |= rpg::REMOTE_FLAG_BOSS_ALIVE;
  }
  return flags;
}

uint8_t RpgWorldState::getResourceSummary(uint32_t now_ms) {
  sync(now_ms);
  const uint32_t total = (uint32_t)_wood + _ore + _herbs + _relics + _gold;
  const uint32_t cap = (uint32_t)WORLD_WOOD_CAP + WORLD_ORE_CAP + WORLD_HERBS_CAP + WORLD_RELICS_CAP + WORLD_GOLD_CAP;
  return (uint8_t)((total * 100U + (cap / 2U)) / cap);
}

void RpgWorldState::formatStatus(char* reply, size_t reply_size, uint32_t now_ms) {
  sync(now_ms);
  uint32_t boss_cd_ms = getBossCooldownRemainingMs(now_ms);
  if (boss_cd_ms == 0) {
    snprintf(reply, reply_size, "rpg world: res=%u%% wood=%u ore=%u herbs=%u relics=%u gold=%u boss=alive",
             (unsigned int)getResourceSummary(now_ms), (unsigned int)_wood, (unsigned int)_ore,
             (unsigned int)_herbs, (unsigned int)_relics, (unsigned int)_gold);
  } else {
    uint32_t mins = (boss_cd_ms + 59999UL) / 60000UL;
    snprintf(reply, reply_size, "rpg world: res=%u%% wood=%u ore=%u herbs=%u relics=%u gold=%u boss=cd%um",
             (unsigned int)getResourceSummary(now_ms), (unsigned int)_wood, (unsigned int)_ore,
             (unsigned int)_herbs, (unsigned int)_relics, (unsigned int)_gold, (unsigned int)mins);
  }
}
