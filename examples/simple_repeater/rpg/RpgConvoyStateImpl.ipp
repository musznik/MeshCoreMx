#include <stdio.h>
#include <string.h>

RpgConvoyState::RpgConvoyState() : _last_completed_convoy_id(0) {
  memset(&_local, 0, sizeof(_local));
  memset(_remote, 0, sizeof(_remote));
}

void RpgConvoyState::copyPlayerKey(uint8_t dest[8], const uint8_t* player_id, size_t player_id_len) {
  memset(dest, 0, 8);
  if (player_id != NULL && player_id_len > 0) {
    memcpy(dest, player_id, min((size_t)8, player_id_len));
  }
}

int RpgConvoyState::findRemote(uint32_t convoy_id, const uint8_t origin_pub_key[PUB_KEY_SIZE]) const {
  for (uint8_t i = 0; i < MAX_REMOTE_CONVOYS; i++) {
    if (_remote[i].used && _remote[i].convoy_id == convoy_id &&
        memcmp(_remote[i].origin_pub_key, origin_pub_key, PUB_KEY_SIZE) == 0) {
      return i;
    }
  }
  return -1;
}

int RpgConvoyState::allocateRemoteSlot() {
  int oldest_idx = 0;
  uint32_t oldest_s = 0xFFFFFFFFUL;
  for (uint8_t i = 0; i < MAX_REMOTE_CONVOYS; i++) {
    if (!_remote[i].used) {
      return i;
    }
    if (_remote[i].arrived_at_s < oldest_s) {
      oldest_s = _remote[i].arrived_at_s;
      oldest_idx = i;
    }
  }
  return oldest_idx;
}

bool RpgConvoyState::hasActiveLocal() const {
  return _local.state != LOCAL_IDLE;
}

uint32_t RpgConvoyState::getLocalConvoyId() const {
  return _local.convoy_id;
}

const uint8_t* RpgConvoyState::getLocalPlayerKey() const {
  return _local.player_key;
}

const uint8_t* RpgConvoyState::getLocalTargetPubKey() const {
  return _local.target_pub_key;
}

bool RpgConvoyState::beginLocal(uint32_t convoy_id, const uint8_t* player_id, size_t player_id_len,
                                const uint8_t target_pub_key[PUB_KEY_SIZE], const char* target_name,
                                uint32_t now_ms, char* reply, size_t reply_size) {
  if (hasActiveLocal()) {
    formatLocalStatus(reply, reply_size, now_ms);
    return false;
  }
  memset(&_local, 0, sizeof(_local));
  _local.state = LOCAL_SENT;
  _local.convoy_id = convoy_id;
  copyPlayerKey(_local.player_key, player_id, player_id_len);
  memcpy(_local.target_pub_key, target_pub_key, PUB_KEY_SIZE);
  snprintf(_local.target_name, sizeof(_local.target_name), "%s", target_name != NULL ? target_name : "remote");
  _local.started_at_ms = now_ms;
  snprintf(reply, reply_size, "rpg convoy: sent to %s id=%08lX", _local.target_name, (unsigned long)convoy_id);
  return true;
}

void RpgConvoyState::formatLocalStatus(char* reply, size_t reply_size, uint32_t now_ms) const {
  if (_local.state == LOCAL_IDLE) {
    strcpy(reply, "rpg convoy: none");
    return;
  }
  uint32_t elapsed_s = (uint32_t)((now_ms - _local.started_at_ms) / 1000UL);
  if (_local.state == LOCAL_SENT) {
    snprintf(reply, reply_size, "rpg convoy: waiting ack from %s id=%08lX age=%lus",
             _local.target_name, (unsigned long)_local.convoy_id, (unsigned long)elapsed_s);
  } else {
    uint32_t ready_in_ms = CONVOY_STAY_MS;
    if (_local.acked_at_ms != 0) {
      uint32_t waited = (uint32_t)(now_ms - _local.acked_at_ms);
      ready_in_ms = waited >= CONVOY_STAY_MS ? 0 : (CONVOY_STAY_MS - waited);
    }
    snprintf(reply, reply_size, "rpg convoy: at %s id=%08lX ready_in=%lum",
             _local.target_name, (unsigned long)_local.convoy_id, (unsigned long)((ready_in_ms + 59999UL) / 60000UL));
  }
}

bool RpgConvoyState::markAcked(uint32_t convoy_id, char* reply, size_t reply_size, uint32_t now_ms) {
  if (_local.state != LOCAL_SENT || _local.convoy_id != convoy_id) {
    return false;
  }
  _local.state = LOCAL_WORKING;
  _local.acked_at_ms = now_ms;
  snprintf(reply, reply_size, "rpg convoy: arrived at %s id=%08lX", _local.target_name, (unsigned long)convoy_id);
  return true;
}

bool RpgConvoyState::shouldIgnoreCompleted(uint32_t convoy_id) const {
  return convoy_id != 0 && convoy_id == _last_completed_convoy_id;
}

bool RpgConvoyState::completeLocal(uint32_t convoy_id) {
  if (_local.convoy_id != convoy_id || _local.state == LOCAL_IDLE) {
    return false;
  }
  _last_completed_convoy_id = convoy_id;
  clearLocal();
  return true;
}

void RpgConvoyState::clearLocal() {
  memset(&_local, 0, sizeof(_local));
}

bool RpgConvoyState::upsertRemote(uint32_t convoy_id, const uint8_t origin_pub_key[PUB_KEY_SIZE],
                                  const uint8_t* player_id, size_t player_id_len, uint32_t now_s) {
  int idx = findRemote(convoy_id, origin_pub_key);
  if (idx < 0) {
    idx = allocateRemoteSlot();
    memset(&_remote[idx], 0, sizeof(_remote[idx]));
  }
  _remote[idx].used = true;
  _remote[idx].convoy_id = convoy_id;
  memcpy(_remote[idx].origin_pub_key, origin_pub_key, PUB_KEY_SIZE);
  copyPlayerKey(_remote[idx].player_key, player_id, player_id_len);
  if (_remote[idx].arrived_at_s == 0) {
    _remote[idx].arrived_at_s = now_s;
  }
  return true;
}

bool RpgConvoyState::getRemote(uint32_t convoy_id, const uint8_t origin_pub_key[PUB_KEY_SIZE], RemoteConvoy& out) const {
  int idx = findRemote(convoy_id, origin_pub_key);
  if (idx < 0) {
    return false;
  }
  out = _remote[idx];
  return true;
}

bool RpgConvoyState::finalizeRemoteResult(uint32_t convoy_id, const uint8_t origin_pub_key[PUB_KEY_SIZE],
                                          uint16_t wood, uint16_t ore, uint16_t herbs,
                                          uint16_t relics, uint16_t gold) {
  int idx = findRemote(convoy_id, origin_pub_key);
  if (idx < 0) {
    return false;
  }
  _remote[idx].result_finalized = true;
  _remote[idx].wood = wood;
  _remote[idx].ore = ore;
  _remote[idx].herbs = herbs;
  _remote[idx].relics = relics;
  _remote[idx].gold = gold;
  return true;
}

void RpgConvoyState::exportRemoteState(PersistedRemoteConvoy out[MAX_REMOTE_CONVOYS]) const {
  for (uint8_t i = 0; i < MAX_REMOTE_CONVOYS; i++) {
    memset(&out[i], 0, sizeof(out[i]));
    out[i].used = _remote[i].used ? 1 : 0;
    out[i].convoy_id = _remote[i].convoy_id;
    memcpy(out[i].origin_pub_key, _remote[i].origin_pub_key, PUB_KEY_SIZE);
    memcpy(out[i].player_key, _remote[i].player_key, sizeof(out[i].player_key));
    out[i].arrived_at_s = _remote[i].arrived_at_s;
    out[i].result_finalized = _remote[i].result_finalized ? 1 : 0;
    out[i].wood = _remote[i].wood;
    out[i].ore = _remote[i].ore;
    out[i].herbs = _remote[i].herbs;
    out[i].relics = _remote[i].relics;
    out[i].gold = _remote[i].gold;
  }
}

void RpgConvoyState::importRemoteState(const PersistedRemoteConvoy in[MAX_REMOTE_CONVOYS]) {
  memset(_remote, 0, sizeof(_remote));
  for (uint8_t i = 0; i < MAX_REMOTE_CONVOYS; i++) {
    _remote[i].used = in[i].used != 0;
    _remote[i].convoy_id = in[i].convoy_id;
    memcpy(_remote[i].origin_pub_key, in[i].origin_pub_key, PUB_KEY_SIZE);
    memcpy(_remote[i].player_key, in[i].player_key, sizeof(_remote[i].player_key));
    _remote[i].arrived_at_s = in[i].arrived_at_s;
    _remote[i].result_finalized = in[i].result_finalized != 0;
    _remote[i].wood = in[i].wood;
    _remote[i].ore = in[i].ore;
    _remote[i].herbs = in[i].herbs;
    _remote[i].relics = in[i].relics;
    _remote[i].gold = in[i].gold;
  }
}
