#include <stdio.h>
#include <string.h>

#include "RpgWorldState.h"

RpgGame::RpgGame() {
  memset(players, 0, sizeof(players));
}

bool RpgGame::hasPlayer(const uint8_t* player_id, size_t player_id_len) const {
  if (player_id == NULL || player_id_len == 0) {
    return false;
  }
  uint8_t key[rpg::PLAYER_KEY_SIZE];
  memset(key, 0, sizeof(key));
  memcpy(key, player_id, min((size_t)rpg::PLAYER_KEY_SIZE, player_id_len));
  for (uint8_t i = 0; i < rpg::MAX_PLAYERS; i++) {
    if (players[i].used && memcmp(players[i].key, key, sizeof(key)) == 0) {
      return true;
    }
  }
  return false;
}

bool RpgGame::startsWithCommand(const char* text, const char* command) {
  size_t cmd_len = strlen(command);
  return memcmp(text, command, cmd_len) == 0 && (text[cmd_len] == 0 || text[cmd_len] == ' ');
}

bool RpgGame::hasCooldownElapsed(uint32_t now_ms, uint32_t last_action_ms) {
  return last_action_ms == 0 || (uint32_t)(now_ms - last_action_ms) >= rpg::ACTION_COOLDOWN_MS;
}

const char* RpgGame::getLocationName(uint8_t location) {
  switch (location) {
    case rpg::LOCATION_FOREST:
      return "forest";
    case rpg::LOCATION_MINE:
      return "mine";
    case rpg::LOCATION_RUINS:
      return "ruins";
    default:
      return "camp";
  }
}

uint16_t RpgGame::clampAdd(uint16_t value, uint16_t amount) {
  uint32_t sum = (uint32_t)value + amount;
  return sum > 65535U ? 65535U : (uint16_t)sum;
}

uint16_t RpgGame::clampResource(uint16_t value, uint16_t amount, uint16_t cap) {
  return min(cap, clampAdd(value, amount));
}

uint16_t RpgGame::getStorageCap(const rpg::PlayerState& player) {
  return (uint16_t)(20 + player.storage_level * 20);
}

uint8_t RpgGame::getMaxEquipmentTier(const rpg::PlayerState& player) {
  return (uint8_t)min(3, 1 + (int)player.workbench_level);
}

uint16_t RpgGame::getXpNeededForLevel(uint8_t level) {
  return (uint16_t)(level * 10);
}

bool RpgGame::isBlank(const char* text) {
  if (text == NULL) {
    return true;
  }
  while (*text != 0) {
    if (*text != ' ') {
      return false;
    }
    text++;
  }
  return true;
}

bool RpgGame::isHigherRank(const rpg::PlayerState& lhs, const rpg::PlayerState& rhs) {
  if (lhs.boss_wins != rhs.boss_wins) {
    return lhs.boss_wins > rhs.boss_wins;
  }
  if (lhs.level != rhs.level) {
    return lhs.level > rhs.level;
  }
  if (lhs.xp != rhs.xp) {
    return lhs.xp > rhs.xp;
  }
  if (lhs.gold != rhs.gold) {
    return lhs.gold > rhs.gold;
  }
  return memcmp(lhs.key, rhs.key, rpg::PLAYER_KEY_SIZE) < 0;
}

void RpgGame::formatPlayerTag(const rpg::PlayerState& player, char* dest, size_t dest_size) {
  if (player.display_name[0] != 0) {
    snprintf(dest, dest_size, "%s", player.display_name);
  } else {
    snprintf(dest, dest_size, "%02X%02X%02X%02X",
             (unsigned int)player.key[0], (unsigned int)player.key[1],
             (unsigned int)player.key[2], (unsigned int)player.key[3]);
  }
}

rpg::PlayerState* RpgGame::findPlayer(const uint8_t* player_id, size_t player_id_len) {
  if (player_id == NULL || player_id_len == 0) {
    return NULL;
  }

  uint8_t key[rpg::PLAYER_KEY_SIZE];
  memset(key, 0, sizeof(key));
  memcpy(key, player_id, min((size_t)rpg::PLAYER_KEY_SIZE, player_id_len));

  for (uint8_t i = 0; i < rpg::MAX_PLAYERS; i++) {
    if (players[i].used && memcmp(players[i].key, key, sizeof(key)) == 0) {
      return &players[i];
    }
  }
  return NULL;
}

void RpgGame::resetPlayer(rpg::PlayerState& player, const uint8_t* player_id, size_t player_id_len) {
  memset(&player, 0, sizeof(player));
  player.used = true;
  memcpy(player.key, player_id, min((size_t)rpg::PLAYER_KEY_SIZE, player_id_len));
  player.level = 1;
  player.hp = 12;
  player.max_hp = 12;
  player.weapon_tier = 0;
  player.armor_tier = 0;
  player.tool_tier = 0;
  player.location = rpg::LOCATION_CAMP;
}

void RpgGame::updateKnownName(rpg::PlayerState& player, const char* known_name) {
  if (player.custom_name || isBlank(known_name)) {
    return;
  }

  snprintf(player.display_name, sizeof(player.display_name), "%s", known_name);
}

rpg::PlayerState* RpgGame::getOrCreatePlayer(const uint8_t* player_id, size_t player_id_len) {
  rpg::PlayerState* existing = findPlayer(player_id, player_id_len);
  if (existing) {
    return existing;
  }

  for (uint8_t i = 0; i < rpg::MAX_PLAYERS; i++) {
    if (!players[i].used) {
      resetPlayer(players[i], player_id, player_id_len);
      return &players[i];
    }
  }
  return NULL;
}

void RpgGame::awardXp(rpg::PlayerState& player, uint16_t xp_amount) {
  if (xp_amount == 0) {
    return;
  }

  player.xp = clampAdd(player.xp, xp_amount);
  while (player.level < 20) {
    uint16_t need = getXpNeededForLevel(player.level);
    if (player.xp < need) {
      break;
    }
    player.xp -= need;
    player.level++;
    player.max_hp = (uint8_t)min(60, player.max_hp + 3);
    player.hp = player.max_hp;
  }
}

uint8_t RpgGame::buildRankedOrder(uint8_t order[], uint8_t max_count) const {
  uint8_t count = 0;
  bool used[rpg::MAX_PLAYERS];
  memset(used, 0, sizeof(used));

  while (count < max_count) {
    int best = -1;
    for (uint8_t i = 0; i < rpg::MAX_PLAYERS; i++) {
      if (!players[i].used || used[i]) {
        continue;
      }
      if (best < 0 || isHigherRank(players[i], players[best])) {
        best = i;
      }
    }
    if (best < 0) {
      break;
    }
    used[best] = true;
    order[count++] = (uint8_t)best;
  }

  return count;
}

int RpgGame::findPlayerIndex(const rpg::PlayerState* player) const {
  if (player == NULL) {
    return -1;
  }
  for (uint8_t i = 0; i < rpg::MAX_PLAYERS; i++) {
    if (&players[i] == player) {
      return i;
    }
  }
  return -1;
}

bool RpgGame::applyConvoyLoot(const uint8_t player_key[8], uint16_t wood, uint16_t ore, uint16_t herbs,
                              uint16_t relics, uint16_t gold, char* reply, size_t reply_size) {
  rpg::PlayerState* player = findPlayer(player_key, rpg::PLAYER_KEY_SIZE);
  if (player == NULL) {
    strcpy(reply, "rpg convoy: player not found");
    return false;
  }

  uint16_t cap = getStorageCap(*player);
  uint16_t before_wood = player->wood;
  uint16_t before_ore = player->ore;
  uint16_t before_herbs = player->herbs;
  uint16_t before_relics = player->relics;
  uint16_t before_gold = player->gold;

  player->wood = clampResource(player->wood, wood, cap);
  player->ore = clampResource(player->ore, ore, cap);
  player->herbs = clampResource(player->herbs, herbs, cap);
  player->relics = clampResource(player->relics, relics, cap);
  player->gold = clampAdd(player->gold, gold);

  snprintf(reply, reply_size, "rpg convoy: returned +%u wood +%u ore +%u herbs +%u relics +%u gold",
           (unsigned int)(player->wood - before_wood), (unsigned int)(player->ore - before_ore),
           (unsigned int)(player->herbs - before_herbs), (unsigned int)(player->relics - before_relics),
           (unsigned int)(player->gold - before_gold));
  return true;
}

void RpgGame::formatHelp(char* reply, size_t reply_size) const {
  snprintf(reply, reply_size,
           "rpg: create, rename, stats, inv, camp, world, shop, craft, travel, gather, fight, heal, collect, boss, convoy, nodes, ping, probe, scan, discover, advert");
}

void RpgGame::formatStats(const rpg::PlayerState& player, char* reply, size_t reply_size) const {
  snprintf(reply, reply_size, "rpg: lvl=%u hp=%u/%u xp=%u gold=%u loc=%s eq=%u/%u/%u",
           (unsigned int)player.level, (unsigned int)player.hp, (unsigned int)player.max_hp,
           (unsigned int)player.xp, (unsigned int)player.gold, getLocationName(player.location),
           (unsigned int)player.weapon_tier, (unsigned int)player.armor_tier, (unsigned int)player.tool_tier);
}

void RpgGame::formatInventory(const rpg::PlayerState& player, char* reply, size_t reply_size) const {
  snprintf(reply, reply_size, "rpg inv: wood=%u ore=%u herbs=%u relics=%u gold=%u cap=%u",
           (unsigned int)player.wood, (unsigned int)player.ore, (unsigned int)player.herbs,
           (unsigned int)player.relics, (unsigned int)player.gold, (unsigned int)getStorageCap(player));
}

void RpgGame::formatCamp(const rpg::PlayerState& player, char* reply, size_t reply_size) const {
  snprintf(reply, reply_size, "rpg camp: fire=%u bench=%u store=%u cart=%u tower=%u boss=%u",
           (unsigned int)player.campfire_level, (unsigned int)player.workbench_level,
           (unsigned int)player.storage_level, (unsigned int)player.cart_level,
           (unsigned int)player.watchtower_level, (unsigned int)player.boss_wins);
}

void RpgGame::formatShop(char* reply, size_t reply_size) const {
  snprintf(reply, reply_size,
           "rpg shop: herb=2g potion=5g sword=%ug armor=%ug tool=%ug",
           8U, 8U, 7U);
}

void RpgGame::formatCraft(char* reply, size_t reply_size) const {
  snprintf(reply, reply_size,
           "rpg craft: campfire, workbench, storage, cart, tower, sword, armor, tool");
}

void RpgGame::formatBoss(const rpg::PlayerState& player, RpgWorldState* world_state, uint32_t now_ms,
                         char* reply, size_t reply_size) const {
  if (world_state != NULL) {
    uint32_t boss_cd_ms = world_state->getBossCooldownRemainingMs(now_ms);
    if (boss_cd_ms == 0) {
      snprintf(reply, reply_size, "rpg boss: alive, need lvl3+ at ruins, tower=%u, wins=%u",
               (unsigned int)player.watchtower_level, (unsigned int)player.boss_wins);
    } else {
      uint32_t mins = (boss_cd_ms + 59999UL) / 60000UL;
      snprintf(reply, reply_size, "rpg boss: cooldown %um, need lvl3+ at ruins, wins=%u",
               (unsigned int)mins, (unsigned int)player.boss_wins);
    }
  } else {
    snprintf(reply, reply_size, "rpg boss: need lvl3+ at ruins, tower=%u, wins=%u",
             (unsigned int)player.watchtower_level, (unsigned int)player.boss_wins);
  }
}

void RpgGame::formatTop(char* reply, size_t reply_size) const {
  uint8_t order[10];
  uint8_t count = buildRankedOrder(order, 10);
  if (count == 0) {
    strcpy(reply, "rpg top: no players yet");
    return;
  }

  size_t used_len = snprintf(reply, reply_size, "rpg top:");
  for (uint8_t i = 0; i < count && used_len + 1 < reply_size; i++) {
    char tag[8];
    formatPlayerTag(players[order[i]], tag, sizeof(tag));
    int written = snprintf(&reply[used_len], reply_size - used_len, " %u.%s-L%u-B%u",
                           (unsigned int)(i + 1), tag,
                           (unsigned int)players[order[i]].level,
                           (unsigned int)players[order[i]].boss_wins);
    if (written < 0 || (size_t)written >= reply_size - used_len) {
      reply[reply_size - 1] = 0;
      return;
    }
    used_len += (size_t)written;
  }
}

void RpgGame::formatPlayers(char* reply, size_t reply_size) const {
  uint8_t order[10];
  uint8_t count = buildRankedOrder(order, 10);
  if (count == 0) {
    strcpy(reply, "rpg players: no players yet");
    return;
  }

  size_t used_len = snprintf(reply, reply_size, "rpg players:");
  for (uint8_t i = 0; i < count && used_len + 1 < reply_size; i++) {
    char tag[8];
    formatPlayerTag(players[order[i]], tag, sizeof(tag));
    int written = snprintf(&reply[used_len], reply_size - used_len, " %s(lv%u,b%u)",
                           tag, (unsigned int)players[order[i]].level,
                           (unsigned int)players[order[i]].boss_wins);
    if (written < 0 || (size_t)written >= reply_size - used_len) {
      reply[reply_size - 1] = 0;
      return;
    }
    used_len += (size_t)written;
  }
}

void RpgGame::formatMe(const rpg::PlayerState& player, char* reply, size_t reply_size) const {
  int player_idx = findPlayerIndex(&player);
  if (player_idx < 0) {
    strcpy(reply, "rpg: player not found");
    return;
  }

  uint8_t order[rpg::MAX_PLAYERS];
  uint8_t count = buildRankedOrder(order, rpg::MAX_PLAYERS);
  char tag[8];
  formatPlayerTag(player, tag, sizeof(tag));

  for (uint8_t i = 0; i < count; i++) {
    if (order[i] == player_idx) {
      snprintf(reply, reply_size, "rpg me: rank=%u id=%s lvl=%u boss=%u xp=%u gold=%u",
               (unsigned int)(i + 1), tag, (unsigned int)player.level,
               (unsigned int)player.boss_wins, (unsigned int)player.xp,
               (unsigned int)player.gold);
      return;
    }
  }

  strcpy(reply, "rpg: no ranking yet");
}

void RpgGame::handleTravel(rpg::PlayerState& player, const char* arg, char* reply, size_t reply_size) const {
  uint8_t new_location = player.location;

  if (strcmp(arg, "camp") == 0) {
    new_location = rpg::LOCATION_CAMP;
  } else if (strcmp(arg, "forest") == 0) {
    new_location = rpg::LOCATION_FOREST;
  } else if (strcmp(arg, "mine") == 0) {
    new_location = rpg::LOCATION_MINE;
  } else if (strcmp(arg, "ruins") == 0) {
    new_location = rpg::LOCATION_RUINS;
  } else {
    strcpy(reply, "rpg: travel camp|forest|mine|ruins");
    return;
  }

  if (player.location == new_location) {
    snprintf(reply, reply_size, "rpg: already at %s", getLocationName(player.location));
    return;
  }

  player.location = new_location;
  snprintf(reply, reply_size, "rpg: traveled to %s", getLocationName(player.location));
}

void RpgGame::handleGather(rpg::PlayerState& player, uint32_t now_ms, mesh::RNG& rng, RpgWorldState* world_state,
                           char* reply, size_t reply_size) {
  if (!hasCooldownElapsed(now_ms, player.last_action_ms)) {
    strcpy(reply, "rpg: rest before next action");
    return;
  }

  if (player.location == rpg::LOCATION_CAMP) {
    strcpy(reply, "rpg: gather in forest, mine or ruins");
    return;
  }

  player.last_action_ms = now_ms;
  uint16_t cap = getStorageCap(player);
  uint16_t amount = (uint16_t)rng.nextInt(1, 4 + player.tool_tier);

  if (player.location == rpg::LOCATION_FOREST) {
    if (rng.nextInt(0, 100) < 70) {
      uint16_t before = player.wood;
      uint16_t gain = amount;
      if (world_state != NULL) {
        gain = world_state->takeWood(amount, now_ms);
      }
      player.wood = clampResource(player.wood, gain, cap);
      if (gain == 0) {
        strcpy(reply, "rpg: forest depleted, wait for regen");
      } else if (player.wood == before) {
        strcpy(reply, "rpg: storage full, craft storage");
      } else {
        snprintf(reply, reply_size, "rpg: found %u wood in forest", (unsigned int)(player.wood - before));
      }
    } else {
      uint16_t before = player.herbs;
      uint16_t gain = 1;
      if (world_state != NULL) {
        gain = world_state->takeHerbs(1, now_ms);
      }
      player.herbs = clampResource(player.herbs, gain, cap);
      if (gain == 0) {
        strcpy(reply, "rpg: herb patch empty, wait for regen");
      } else {
        strcpy(reply, player.herbs == before ? "rpg: herb pouch full" : "rpg: found 1 herb in forest");
      }
    }
  } else if (player.location == rpg::LOCATION_MINE) {
    uint16_t before = player.ore;
    uint16_t gain = amount;
    if (world_state != NULL) {
      gain = world_state->takeOre(amount, now_ms);
    }
    player.ore = clampResource(player.ore, gain, cap);
    if (gain == 0) {
      strcpy(reply, "rpg: mine depleted, wait for regen");
    } else if (player.ore == before) {
      strcpy(reply, "rpg: ore storage full");
    } else {
      snprintf(reply, reply_size, "rpg: mined %u ore", (unsigned int)(player.ore - before));
    }
  } else {
    if (rng.nextInt(0, 100) < 45) {
      uint16_t before = player.relics;
      uint16_t gain = 1;
      if (world_state != NULL) {
        gain = world_state->takeRelics(1, now_ms);
      }
      player.relics = clampResource(player.relics, gain, cap);
      if (gain == 0) {
        strcpy(reply, "rpg: ruins already stripped of relics");
      } else {
        strcpy(reply, player.relics == before ? "rpg: relic storage full" : "rpg: uncovered 1 relic in ruins");
      }
    } else {
      uint16_t gain = amount;
      if (world_state != NULL) {
        gain = world_state->takeGold(amount, now_ms);
      }
      if (gain == 0) {
        strcpy(reply, "rpg: ruins picked clean, wait for regen");
      } else {
        player.gold = clampAdd(player.gold, gain);
        snprintf(reply, reply_size, "rpg: scavenged %u gold in ruins", (unsigned int)gain);
      }
    }
  }
}

void RpgGame::handleFight(rpg::PlayerState& player, uint32_t now_ms, mesh::RNG& rng, char* reply, size_t reply_size) {
  if (!hasCooldownElapsed(now_ms, player.last_action_ms)) {
    strcpy(reply, "rpg: rest before next action");
    return;
  }
  if (player.location == rpg::LOCATION_CAMP) {
    strcpy(reply, "rpg: no enemies at camp");
    return;
  }
  if (player.hp == 0) {
    strcpy(reply, "rpg: you are down, use heal");
    return;
  }

  player.last_action_ms = now_ms;

  uint8_t difficulty = player.location == rpg::LOCATION_FOREST ? 35 :
                       player.location == rpg::LOCATION_MINE ? 50 : 68;
  uint8_t power = (uint8_t)min(95, 45 + player.level * 8 + player.weapon_tier * 8 + player.tool_tier * 2 +
                               (int)(player.herbs > 0 ? 5 : 0));
  uint8_t chance = (uint8_t)min(95, power > difficulty ? 55 + (power - difficulty) : 30);

  if (rng.nextInt(0, 100) < chance) {
    uint16_t xp_gain = player.location == rpg::LOCATION_RUINS ? 5 : 3;
    uint16_t gold_gain = (uint16_t)rng.nextInt(1, player.location == rpg::LOCATION_RUINS ? 5 : 4);
    uint8_t old_level = player.level;
    awardXp(player, xp_gain);
    player.gold = clampAdd(player.gold, gold_gain);

    if (player.level > old_level) {
      snprintf(reply, reply_size, "rpg: victory, +%u xp +%u gold, level up to %u",
               (unsigned int)xp_gain, (unsigned int)gold_gain, (unsigned int)player.level);
    } else {
      snprintf(reply, reply_size, "rpg: victory, +%u xp +%u gold", (unsigned int)xp_gain, (unsigned int)gold_gain);
    }
  } else {
    uint8_t dmg = (uint8_t)rng.nextInt(1, player.location == rpg::LOCATION_RUINS ? 6 : 5);
    dmg = dmg > player.armor_tier ? (uint8_t)(dmg - player.armor_tier) : 1;
    player.hp = player.hp > dmg ? (uint8_t)(player.hp - dmg) : 0;
    snprintf(reply, reply_size, "rpg: defeated, -%u hp, hp=%u/%u",
             (unsigned int)dmg, (unsigned int)player.hp, (unsigned int)player.max_hp);
  }
}

void RpgGame::handleHeal(rpg::PlayerState& player, char* reply, size_t reply_size) {
  if (player.hp >= player.max_hp) {
    strcpy(reply, "rpg: hp already full");
    return;
  }
  if (player.location != rpg::LOCATION_CAMP) {
    strcpy(reply, "rpg: heal works only at camp");
    return;
  }

  uint8_t herb_heal = (uint8_t)(6 + player.campfire_level * 2);
  uint8_t gold_cost = player.campfire_level >= 2 ? 1 : (player.campfire_level == 1 ? 2 : 3);

  if (player.herbs > 0) {
    player.herbs--;
    uint8_t healed = (uint8_t)min((int)player.max_hp, player.hp + herb_heal);
    player.hp = healed;
    snprintf(reply, reply_size, "rpg: healed with herbs, hp=%u/%u", (unsigned int)player.hp,
             (unsigned int)player.max_hp);
  } else if (player.gold >= gold_cost) {
    player.gold -= gold_cost;
    player.hp = player.max_hp;
    snprintf(reply, reply_size, "rpg: rested at camp, hp=%u/%u", (unsigned int)player.hp,
             (unsigned int)player.max_hp);
  } else {
    snprintf(reply, reply_size, "rpg: need 1 herb or %u gold to heal", (unsigned int)gold_cost);
  }
}

void RpgGame::handleBuy(rpg::PlayerState& player, const char* arg, char* reply, size_t reply_size) {
  if (strcmp(arg, "herb") == 0) {
    if (player.gold < 2) {
      strcpy(reply, "rpg: need 2 gold");
    } else if (player.herbs >= getStorageCap(player)) {
      strcpy(reply, "rpg: herb pouch full");
    } else {
      player.gold -= 2;
      player.herbs++;
      strcpy(reply, "rpg: bought 1 herb");
    }
    return;
  }

  if (strcmp(arg, "potion") == 0) {
    if (player.gold < 5) {
      strcpy(reply, "rpg: need 5 gold");
    } else {
      player.gold -= 5;
      player.hp = (uint8_t)min((int)player.max_hp, player.hp + 8);
      snprintf(reply, reply_size, "rpg: potion used, hp=%u/%u", (unsigned int)player.hp,
               (unsigned int)player.max_hp);
    }
    return;
  }

  if (strcmp(arg, "sword") == 0) {
    uint8_t max_tier = getMaxEquipmentTier(player);
    if (player.weapon_tier >= max_tier) {
      snprintf(reply, reply_size, "rpg: sword capped at tier %u", (unsigned int)max_tier);
    } else {
      uint8_t cost = (uint8_t)(8 + player.weapon_tier * 6);
      if (player.gold < cost) {
        snprintf(reply, reply_size, "rpg: need %u gold", (unsigned int)cost);
      } else {
        player.gold -= cost;
        player.weapon_tier++;
        snprintf(reply, reply_size, "rpg: bought sword tier %u", (unsigned int)player.weapon_tier);
      }
    }
    return;
  }

  if (strcmp(arg, "armor") == 0) {
    uint8_t max_tier = getMaxEquipmentTier(player);
    if (player.armor_tier >= max_tier) {
      snprintf(reply, reply_size, "rpg: armor capped at tier %u", (unsigned int)max_tier);
    } else {
      uint8_t cost = (uint8_t)(8 + player.armor_tier * 6);
      if (player.gold < cost) {
        snprintf(reply, reply_size, "rpg: need %u gold", (unsigned int)cost);
      } else {
        player.gold -= cost;
        player.armor_tier++;
        snprintf(reply, reply_size, "rpg: bought armor tier %u", (unsigned int)player.armor_tier);
      }
    }
    return;
  }

  if (strcmp(arg, "tool") == 0 || strcmp(arg, "pickaxe") == 0) {
    uint8_t max_tier = getMaxEquipmentTier(player);
    if (player.tool_tier >= max_tier) {
      snprintf(reply, reply_size, "rpg: tool capped at tier %u", (unsigned int)max_tier);
    } else {
      uint8_t cost = (uint8_t)(7 + player.tool_tier * 5);
      if (player.gold < cost) {
        snprintf(reply, reply_size, "rpg: need %u gold", (unsigned int)cost);
      } else {
        player.gold -= cost;
        player.tool_tier++;
        snprintf(reply, reply_size, "rpg: bought tool tier %u", (unsigned int)player.tool_tier);
      }
    }
    return;
  }

  strcpy(reply, "rpg: buy herb|potion|sword|armor|tool");
}

void RpgGame::handleCraft(rpg::PlayerState& player, const char* arg, char* reply, size_t reply_size) {
  if (*arg == 0) {
    formatCraft(reply, reply_size);
    return;
  }

  if (strcmp(arg, "campfire") == 0) {
    if (player.campfire_level >= 2) {
      strcpy(reply, "rpg: campfire maxed");
    } else if (player.wood < 6 || player.herbs < 1) {
      strcpy(reply, "rpg: need 6 wood and 1 herb");
    } else {
      player.wood -= 6;
      player.herbs -= 1;
      player.campfire_level++;
      snprintf(reply, reply_size, "rpg: campfire upgraded to %u", (unsigned int)player.campfire_level);
    }
    return;
  }

  if (strcmp(arg, "workbench") == 0) {
    if (player.workbench_level >= 2) {
      strcpy(reply, "rpg: workbench maxed");
    } else if (player.wood < 8 || player.ore < 4) {
      strcpy(reply, "rpg: need 8 wood and 4 ore");
    } else {
      player.wood -= 8;
      player.ore -= 4;
      player.workbench_level++;
      snprintf(reply, reply_size, "rpg: workbench upgraded to %u", (unsigned int)player.workbench_level);
    }
    return;
  }

  if (strcmp(arg, "storage") == 0) {
    if (player.storage_level >= 2) {
      strcpy(reply, "rpg: storage maxed");
    } else if (player.wood < 10 || player.ore < 2) {
      strcpy(reply, "rpg: need 10 wood and 2 ore");
    } else {
      player.wood -= 10;
      player.ore -= 2;
      player.storage_level++;
      snprintf(reply, reply_size, "rpg: storage upgraded to %u", (unsigned int)player.storage_level);
    }
    return;
  }

  if (strcmp(arg, "cart") == 0) {
    if (player.cart_level >= 2) {
      strcpy(reply, "rpg: cart maxed");
    } else if (player.wood < 8 || player.ore < 6) {
      strcpy(reply, "rpg: need 8 wood and 6 ore");
    } else {
      player.wood -= 8;
      player.ore -= 6;
      player.cart_level++;
      snprintf(reply, reply_size, "rpg: cart upgraded to %u", (unsigned int)player.cart_level);
    }
    return;
  }

  if (strcmp(arg, "tower") == 0 || strcmp(arg, "watchtower") == 0) {
    if (player.watchtower_level >= 2) {
      strcpy(reply, "rpg: tower maxed");
    } else if (player.wood < 6 || player.relics < 1) {
      strcpy(reply, "rpg: need 6 wood and 1 relic");
    } else {
      player.wood -= 6;
      player.relics -= 1;
      player.watchtower_level++;
      snprintf(reply, reply_size, "rpg: watchtower upgraded to %u", (unsigned int)player.watchtower_level);
    }
    return;
  }

  if (strcmp(arg, "sword") == 0) {
    if (player.workbench_level == 0) {
      strcpy(reply, "rpg: build workbench first");
    } else if (player.weapon_tier >= getMaxEquipmentTier(player)) {
      strcpy(reply, "rpg: sword already at max tier");
    } else {
      uint8_t ore_cost = (uint8_t)(4 + player.weapon_tier * 2);
      uint8_t wood_cost = 2;
      if (player.ore < ore_cost || player.wood < wood_cost) {
        snprintf(reply, reply_size, "rpg: need %u ore and %u wood", (unsigned int)ore_cost, (unsigned int)wood_cost);
      } else {
        player.ore -= ore_cost;
        player.wood -= wood_cost;
        player.weapon_tier++;
        snprintf(reply, reply_size, "rpg: crafted sword tier %u", (unsigned int)player.weapon_tier);
      }
    }
    return;
  }

  if (strcmp(arg, "armor") == 0) {
    if (player.workbench_level == 0) {
      strcpy(reply, "rpg: build workbench first");
    } else if (player.armor_tier >= getMaxEquipmentTier(player)) {
      strcpy(reply, "rpg: armor already at max tier");
    } else {
      uint8_t ore_cost = (uint8_t)(3 + player.armor_tier * 2);
      uint8_t wood_cost = (uint8_t)(3 + player.armor_tier);
      if (player.ore < ore_cost || player.wood < wood_cost) {
        snprintf(reply, reply_size, "rpg: need %u ore and %u wood", (unsigned int)ore_cost, (unsigned int)wood_cost);
      } else {
        player.ore -= ore_cost;
        player.wood -= wood_cost;
        player.armor_tier++;
        snprintf(reply, reply_size, "rpg: crafted armor tier %u", (unsigned int)player.armor_tier);
      }
    }
    return;
  }

  if (strcmp(arg, "tool") == 0) {
    if (player.workbench_level == 0) {
      strcpy(reply, "rpg: build workbench first");
    } else if (player.tool_tier >= getMaxEquipmentTier(player)) {
      strcpy(reply, "rpg: tool already at max tier");
    } else {
      uint8_t ore_cost = (uint8_t)(4 + player.tool_tier);
      uint8_t wood_cost = 2;
      if (player.ore < ore_cost || player.wood < wood_cost) {
        snprintf(reply, reply_size, "rpg: need %u ore and %u wood", (unsigned int)ore_cost, (unsigned int)wood_cost);
      } else {
        player.ore -= ore_cost;
        player.wood -= wood_cost;
        player.tool_tier++;
        snprintf(reply, reply_size, "rpg: crafted tool tier %u", (unsigned int)player.tool_tier);
      }
    }
    return;
  }

  formatCraft(reply, reply_size);
}

void RpgGame::handleCollect(rpg::PlayerState& player, uint32_t now_ms, mesh::RNG& rng, char* reply, size_t reply_size) {
  if (player.cart_level == 0) {
    strcpy(reply, "rpg: build cart first");
    return;
  }

  uint32_t tick_ms = 15UL * 60UL * 1000UL;
  uint32_t elapsed = player.last_collect_ms == 0 ? tick_ms : (uint32_t)(now_ms - player.last_collect_ms);
  uint32_t ticks32 = elapsed / tick_ms;
  if (ticks32 > 6UL) {
    ticks32 = 6UL;
  }
  uint8_t ticks = (uint8_t)ticks32;
  if (ticks == 0) {
    strcpy(reply, "rpg: cart not ready yet");
    return;
  }

  player.last_collect_ms = now_ms;
  uint16_t cap = getStorageCap(player);
  uint16_t wood_gain = 0;
  uint16_t ore_gain = 0;
  uint16_t gold_gain = 0;

  for (uint8_t i = 0; i < ticks; i++) {
    wood_gain += (uint16_t)rng.nextInt(0, 1 + player.cart_level);
    ore_gain += (uint16_t)rng.nextInt(0, 1 + player.cart_level);
    gold_gain += (uint16_t)rng.nextInt(0, 2 + player.watchtower_level);
  }

  player.wood = clampResource(player.wood, wood_gain, cap);
  player.ore = clampResource(player.ore, ore_gain, cap);
  player.gold = clampAdd(player.gold, gold_gain);

  snprintf(reply, reply_size, "rpg: cart collected +%u wood +%u ore +%u gold",
           (unsigned int)wood_gain, (unsigned int)ore_gain, (unsigned int)gold_gain);
}

void RpgGame::handleBossFight(rpg::PlayerState& player, uint32_t now_ms, mesh::RNG& rng, RpgWorldState* world_state,
                              char* reply, size_t reply_size) {
  if (!hasCooldownElapsed(now_ms, player.last_action_ms)) {
    strcpy(reply, "rpg: rest before next action");
    return;
  }
  if (player.level < 3) {
    strcpy(reply, "rpg: boss unlocks at level 3");
    return;
  }
  if (player.location != rpg::LOCATION_RUINS) {
    strcpy(reply, "rpg: boss waits in ruins");
    return;
  }
  if (world_state != NULL && !world_state->isBossAlive(now_ms)) {
    uint32_t mins = (world_state->getBossCooldownRemainingMs(now_ms) + 59999UL) / 60000UL;
    snprintf(reply, reply_size, "rpg: boss already down, back in %um", (unsigned int)mins);
    return;
  }
  if (player.hp == 0) {
    strcpy(reply, "rpg: you are down, use heal");
    return;
  }

  player.last_action_ms = now_ms;
  int boss_power = 55 + player.boss_wins * 6;
  int hero_power = 35 + player.level * 9 + player.weapon_tier * 10 + player.armor_tier * 6 +
                   player.watchtower_level * 5 + min((int)player.relics, 2) * 3;
  uint8_t chance = (uint8_t)max(20, min(88, 40 + hero_power - boss_power));

  if (rng.nextInt(0, 100) < chance) {
    uint16_t xp_gain = (uint16_t)(10 + player.boss_wins * 2);
    uint16_t gold_gain = (uint16_t)rng.nextInt(8, 15 + player.watchtower_level * 2);
    uint8_t old_level = player.level;
    awardXp(player, xp_gain);
    player.gold = clampAdd(player.gold, gold_gain);
    player.relics = clampResource(player.relics, 1, getStorageCap(player));
    player.boss_wins++;
    if (world_state != NULL) {
      world_state->defeatBoss(now_ms);
    }

    if (player.level > old_level) {
      snprintf(reply, reply_size, "rpg: boss defeated, +%u xp +%u gold +1 relic, lvl %u",
               (unsigned int)xp_gain, (unsigned int)gold_gain, (unsigned int)player.level);
    } else {
      snprintf(reply, reply_size, "rpg: boss defeated, +%u xp +%u gold +1 relic",
               (unsigned int)xp_gain, (unsigned int)gold_gain);
    }
  } else {
    uint8_t dmg = (uint8_t)rng.nextInt(4, 9);
    dmg = dmg > player.armor_tier ? (uint8_t)(dmg - player.armor_tier) : 2;
    player.hp = player.hp > dmg ? (uint8_t)(player.hp - dmg) : 0;
    snprintf(reply, reply_size, "rpg: boss crushed you, -%u hp, hp=%u/%u",
             (unsigned int)dmg, (unsigned int)player.hp, (unsigned int)player.max_hp);
  }
}

void RpgGame::handleRename(rpg::PlayerState& player, const char* arg, char* reply, size_t reply_size) {
  while (*arg == ' ') {
    arg++;
  }

  if (*arg == 0) {
    strcpy(reply, "rpg: rename <name>");
    return;
  }

  if (strcmp(arg, "clear") == 0 || strcmp(arg, "-") == 0) {
    player.display_name[0] = 0;
    player.custom_name = false;
    strcpy(reply, "rpg: custom name cleared");
    return;
  }

  snprintf(player.display_name, sizeof(player.display_name), "%s", arg);
  player.custom_name = true;
  snprintf(reply, reply_size, "rpg: renamed to %s", player.display_name);
}

bool RpgGame::handleCommand(const uint8_t* player_id, size_t player_id_len, uint32_t now_ms, mesh::RNG& rng,
                            const char* command, char* reply, size_t reply_size, const char* known_name,
                            RpgWorldState* world_state) {
  while (*command == ' ') {
    command++;
  }

  if (!startsWithCommand(command, "rpg")) {
    return false;
  }

  const char* sub = command + 3;
  while (*sub == ' ') {
    sub++;
  }

  if (*sub == 0 || strcmp(sub, "help") == 0) {
    formatHelp(reply, reply_size);
    return true;
  }

  if (strcmp(sub, "top") == 0) {
    formatTop(reply, reply_size);
    return true;
  }

  if (strcmp(sub, "players") == 0) {
    formatPlayers(reply, reply_size);
    return true;
  }

  if (strcmp(sub, "world") == 0) {
    if (world_state != NULL) {
      world_state->formatStatus(reply, reply_size, now_ms);
    } else {
      strcpy(reply, "rpg world: unavailable");
    }
    return true;
  }

  if (player_id == NULL || player_id_len == 0) {
    strcpy(reply, "rpg: missing player identity");
    return true;
  }

  if (strcmp(sub, "create") == 0 || strcmp(sub, "new") == 0) {
    rpg::PlayerState* player = getOrCreatePlayer(player_id, player_id_len);
    if (player == NULL) {
      strcpy(reply, "rpg: player slots full");
    } else {
      resetPlayer(*player, player_id, player_id_len);
      updateKnownName(*player, known_name);
      strcpy(reply, "rpg: hero created at camp, use rpg stats");
    }
    return true;
  }

  rpg::PlayerState* player = findPlayer(player_id, player_id_len);
  if (player == NULL) {
    strcpy(reply, "rpg: use 'rpg create' first");
    return true;
  }
  updateKnownName(*player, known_name);

  if (strcmp(sub, "stats") == 0) {
    formatStats(*player, reply, reply_size);
  } else if (startsWithCommand(sub, "rename")) {
    const char* arg = sub + 6;
    handleRename(*player, arg, reply, reply_size);
  } else if (strcmp(sub, "me") == 0) {
    formatMe(*player, reply, reply_size);
  } else if (strcmp(sub, "inventory") == 0 || strcmp(sub, "inv") == 0 || strcmp(sub, "bag") == 0) {
    formatInventory(*player, reply, reply_size);
  } else if (strcmp(sub, "camp") == 0) {
    formatCamp(*player, reply, reply_size);
  } else if (strcmp(sub, "shop") == 0) {
    formatShop(reply, reply_size);
  } else if (startsWithCommand(sub, "buy")) {
    const char* arg = sub + 3;
    while (*arg == ' ') {
      arg++;
    }
    handleBuy(*player, arg, reply, reply_size);
  } else if (strcmp(sub, "craft") == 0) {
    formatCraft(reply, reply_size);
  } else if (startsWithCommand(sub, "craft")) {
    const char* arg = sub + 5;
    while (*arg == ' ') {
      arg++;
    }
    handleCraft(*player, arg, reply, reply_size);
  } else if (startsWithCommand(sub, "travel")) {
    const char* arg = sub + 6;
    while (*arg == ' ') {
      arg++;
    }
    handleTravel(*player, arg, reply, reply_size);
  } else if (strcmp(sub, "gather") == 0 || strcmp(sub, "mine") == 0 || strcmp(sub, "dig") == 0) {
    handleGather(*player, now_ms, rng, world_state, reply, reply_size);
  } else if (strcmp(sub, "fight") == 0) {
    handleFight(*player, now_ms, rng, reply, reply_size);
  } else if (strcmp(sub, "heal") == 0 || strcmp(sub, "rest") == 0) {
    handleHeal(*player, reply, reply_size);
  } else if (strcmp(sub, "collect") == 0) {
    handleCollect(*player, now_ms, rng, reply, reply_size);
  } else if (strcmp(sub, "boss") == 0) {
    formatBoss(*player, world_state, now_ms, reply, reply_size);
  } else if (strcmp(sub, "boss fight") == 0) {
    handleBossFight(*player, now_ms, rng, world_state, reply, reply_size);
  } else {
    formatHelp(reply, reply_size);
  }

  return true;
}
