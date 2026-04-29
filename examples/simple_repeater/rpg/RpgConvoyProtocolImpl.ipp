#include <stdio.h>
#include <string.h>

namespace {

static void buildConvoyRequest(uint8_t* data, uint32_t timestamp, uint8_t req_type, uint32_t convoy_id,
                               const uint8_t player_key[8]) {
  memcpy(data, &timestamp, 4);
  data[4] = RpgConvoyProtocol::ANON_REQ_TYPE;
  data[5] = req_type;
  memcpy(&data[6], &convoy_id, 4);
  memcpy(&data[10], player_key, 8);
}

}

bool RpgConvoyProtocol::handleCommand(RpgConvoyProtocolHost& host, const uint8_t* player_id, size_t player_id_len,
                                      char* command, char* reply, size_t reply_size, RpgGame& game,
                                      RpgConvoyState& convoy_state) {
  const char* sub = command + 3;
  while (*sub == ' ') {
    sub++;
  }
  if (memcmp(sub, "convoy", 6) != 0 || (sub[6] != 0 && sub[6] != ' ')) {
    return false;
  }

  sub += 6;
  while (*sub == ' ') {
    sub++;
  }

  if (*sub == 0 || strcmp(sub, "status") == 0) {
    convoy_state.formatLocalStatus(reply, reply_size, host.getRpgNowMs());
    return true;
  }

  if (memcmp(sub, "send ", 5) == 0) {
    if (player_id == NULL || player_id_len == 0 || !game.hasPlayer(player_id, player_id_len)) {
      strcpy(reply, "rpg convoy: use 'rpg create' first");
      return true;
    }
    if (convoy_state.hasActiveLocal()) {
      convoy_state.formatLocalStatus(reply, reply_size, host.getRpgNowMs());
      return true;
    }

    const char* prefix = sub + 5;
    while (*prefix == ' ') {
      prefix++;
    }
    if (*prefix == 0) {
      strcpy(reply, "rpg convoy: send <pubkeyprefix>");
      return true;
    }

    mesh::Identity target;
    char target_name[17];
    if (!host.resolveRpgTargetByPrefix(prefix, target, target_name, sizeof(target_name))) {
      strcpy(reply, "rpg convoy: node not known, use rpg probe or neighbors");
      return true;
    }
    if (target.matches(host.getRpgSelfId())) {
      strcpy(reply, "rpg convoy: target must be remote");
      return true;
    }

    uint32_t convoy_id;
    host.getRpgRng().random((uint8_t*)&convoy_id, sizeof(convoy_id));
    if (convoy_id == 0) {
      convoy_id = 1;
    }

    uint8_t player_key[8];
    memset(player_key, 0, sizeof(player_key));
    memcpy(player_key, player_id, min((size_t)8, player_id_len));

    uint8_t data[18];
    buildConvoyRequest(data, host.getRpgUniqueTime(), REQ_SEND, convoy_id, player_key);
    mesh::Packet* pkt = host.createRpgAnonRequest(target, data, sizeof(data));
    if (pkt == NULL) {
      strcpy(reply, "rpg convoy: unable to create request");
      return true;
    }

    convoy_state.beginLocal(convoy_id, player_id, player_id_len, target.pub_key, target_name, host.getRpgNowMs(),
                            reply, reply_size);
    host.sendRpgFlood(pkt, 0);
    return true;
  }

  if (strcmp(sub, "collect") == 0) {
    if (!convoy_state.hasActiveLocal()) {
      strcpy(reply, "rpg convoy: none");
      return true;
    }

    uint8_t data[18];
    uint8_t player_key[8];
    memcpy(player_key, convoy_state.getLocalPlayerKey(), sizeof(player_key));
    buildConvoyRequest(data, host.getRpgUniqueTime(), REQ_COLLECT, convoy_state.getLocalConvoyId(), player_key);

    mesh::Identity target(convoy_state.getLocalTargetPubKey());
    mesh::Packet* pkt = host.createRpgAnonRequest(target, data, sizeof(data));
    if (pkt == NULL) {
      strcpy(reply, "rpg convoy: unable to create collect request");
      return true;
    }

    host.sendRpgFlood(pkt, 0);
    strcpy(reply, "rpg convoy: collect request sent");
    return true;
  }

  strcpy(reply, "rpg convoy: send <pubkeyprefix>|collect|status");
  return true;
}

bool RpgConvoyProtocol::handleAnonRequest(RpgConvoyProtocolHost& host, FILESYSTEM* fs, mesh::Packet* packet,
                                          const uint8_t* secret, const mesh::Identity& sender, const uint8_t* data,
                                          size_t len, RpgConvoyState& convoy_state, RpgWorldState& world_state) {
  if (len < 6 || data[4] != ANON_REQ_TYPE || !packet->isRouteFlood()) {
    return false;
  }

  uint8_t req_type = data[5];
  if (len >= 18 && req_type == REQ_SEND) {
    uint32_t convoy_id;
    memcpy(&convoy_id, &data[6], 4);
    const uint8_t* player_key = &data[10];
    convoy_state.upsertRemote(convoy_id, sender.pub_key, player_key, 8, host.getRpgNowS());
    if (!RpgConvoyPersistence::save(fs, convoy_state)) {
      host.debugPrintRpg("rpg convoy: failed to persist remote convoy");
    }

    uint8_t body[4];
    memcpy(body, &convoy_id, 4);
    mesh::Packet* raw = host.createRpgRawReply(sender, secret, RAW_TYPE_ACK, body, sizeof(body));
    if (raw) {
      host.sendDirectBackToFloodSender(packet, raw);
    }
    return true;
  }

  if (len >= 18 && req_type == REQ_COLLECT) {
    uint32_t convoy_id;
    memcpy(&convoy_id, &data[6], 4);
    RpgConvoyState::RemoteConvoy remote;
    uint8_t body[16];
    memcpy(body, &convoy_id, 4);
    if (!convoy_state.getRemote(convoy_id, sender.pub_key, remote)) {
      body[4] = 1;
      mesh::Packet* raw = host.createRpgRawReply(sender, secret, RAW_TYPE_ERROR, body, 5);
      if (raw) {
        host.sendDirectBackToFloodSender(packet, raw);
      }
      return true;
    }

    uint32_t now_s = host.getRpgNowS();
    uint32_t elapsed_s = now_s >= remote.arrived_at_s ? (uint32_t)(now_s - remote.arrived_at_s) : 0;
    if (elapsed_s < RpgConvoyState::CONVOY_STAY_SECS) {
      uint32_t remaining_s = RpgConvoyState::CONVOY_STAY_SECS - elapsed_s;
      uint16_t mins = (uint16_t)((remaining_s + 59UL) / 60UL);
      memcpy(&body[4], &mins, 2);
      mesh::Packet* raw = host.createRpgRawReply(sender, secret, RAW_TYPE_WAIT, body, 6);
      if (raw) {
        host.sendDirectBackToFloodSender(packet, raw);
      }
      return true;
    }

    if (!remote.result_finalized) {
      uint16_t wood = world_state.takeWood((uint16_t)host.getRpgRng().nextInt(1, 4), host.getRpgNowMs());
      uint16_t ore = world_state.takeOre((uint16_t)host.getRpgRng().nextInt(1, 4), host.getRpgNowMs());
      uint16_t herbs = world_state.takeHerbs((uint16_t)host.getRpgRng().nextInt(0, 2), host.getRpgNowMs());
      uint16_t relics = world_state.takeRelics((uint16_t)host.getRpgRng().nextInt(0, 2), host.getRpgNowMs());
      uint16_t gold = world_state.takeGold((uint16_t)host.getRpgRng().nextInt(2, 7), host.getRpgNowMs());
      convoy_state.finalizeRemoteResult(convoy_id, sender.pub_key, wood, ore, herbs, relics, gold);
      if (!RpgConvoyPersistence::save(fs, convoy_state)) {
        host.debugPrintRpg("rpg convoy: failed to persist finalized result");
      }
      convoy_state.getRemote(convoy_id, sender.pub_key, remote);
    }

    memcpy(&body[4], &remote.wood, 2);
    memcpy(&body[6], &remote.ore, 2);
    memcpy(&body[8], &remote.herbs, 2);
    memcpy(&body[10], &remote.relics, 2);
    memcpy(&body[12], &remote.gold, 2);
    mesh::Packet* raw = host.createRpgRawReply(sender, secret, RAW_TYPE_RESULT, body, 14);
    if (raw) {
      host.sendDirectBackToFloodSender(packet, raw);
    }
    return true;
  }

  return true;
}

bool RpgConvoyProtocol::handleRawData(RpgConvoyProtocolHost& host, mesh::Packet* packet,
                                      RpgConvoyState& convoy_state, RpgGame& game) {
  if (packet->payload_len < 2 + PUB_KEY_SIZE + CIPHER_MAC_SIZE || packet->payload[0] != RAW_MAGIC) {
    return false;
  }

  uint8_t type = packet->payload[1];
  mesh::Identity sender(&packet->payload[2]);
  uint8_t secret[PUB_KEY_SIZE];
  host.getRpgSelfId().calcSharedSecret(secret, sender);

  uint8_t data[MAX_PACKET_PAYLOAD];
  int len = mesh::Utils::MACThenDecrypt(secret, data, &packet->payload[2 + PUB_KEY_SIZE],
                                        packet->payload_len - (2 + PUB_KEY_SIZE));
  if (len < 4) {
    return true;
  }

  uint32_t convoy_id;
  memcpy(&convoy_id, data, 4);
  if (!convoy_state.hasActiveLocal() || convoy_id != convoy_state.getLocalConvoyId()) {
    return true;
  }
  if (convoy_state.shouldIgnoreCompleted(convoy_id)) {
    return true;
  }
  if (memcmp(convoy_state.getLocalTargetPubKey(), sender.pub_key, PUB_KEY_SIZE) != 0) {
    return true;
  }

  char temp[160];
  if (type == RAW_TYPE_ACK) {
    if (convoy_state.markAcked(convoy_id, temp, sizeof(temp), host.getRpgNowMs())) {
      host.debugPrintRpg(temp);
    }
  } else if (type == RAW_TYPE_WAIT && len >= 6) {
    uint16_t mins;
    memcpy(&mins, &data[4], 2);
    snprintf(temp, sizeof(temp), "rpg convoy: target says wait %u min", (unsigned int)mins);
    host.debugPrintRpg(temp);
  } else if (type == RAW_TYPE_RESULT && len >= 14) {
    uint16_t wood, ore, herbs, relics, gold;
    memcpy(&wood, &data[4], 2);
    memcpy(&ore, &data[6], 2);
    memcpy(&herbs, &data[8], 2);
    memcpy(&relics, &data[10], 2);
    memcpy(&gold, &data[12], 2);
    if (game.applyConvoyLoot(convoy_state.getLocalPlayerKey(), wood, ore, herbs, relics, gold, temp, sizeof(temp))) {
      convoy_state.completeLocal(convoy_id);
      host.debugPrintRpg(temp);
    }
  } else if (type == RAW_TYPE_ERROR && len >= 5) {
    snprintf(temp, sizeof(temp), "rpg convoy: remote error=%u", (unsigned int)data[4]);
    host.debugPrintRpg(temp);
  }

  return true;
}
