#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RpgRemoteState::RpgRemoteState() {
  memset(_nodes, 0, sizeof(_nodes));
}

void RpgRemoteState::formatKeyPrefix(const uint8_t* key_prefix, char* dest, size_t dest_size) {
  snprintf(dest, dest_size, "%02X%02X%02X%02X",
           (unsigned int)key_prefix[0], (unsigned int)key_prefix[1],
           (unsigned int)key_prefix[2], (unsigned int)key_prefix[3]);
}

bool RpgRemoteState::matchesHexPrefix(const RemoteNode& node, const char* prefix) {
  char hex[17];
  snprintf(hex, sizeof(hex), "%02X%02X%02X%02X%02X%02X%02X%02X",
           (unsigned int)node.key_prefix[0], (unsigned int)node.key_prefix[1],
           (unsigned int)node.key_prefix[2], (unsigned int)node.key_prefix[3],
           (unsigned int)node.key_prefix[4], (unsigned int)node.key_prefix[5],
           (unsigned int)node.key_prefix[6], (unsigned int)node.key_prefix[7]);
  size_t prefix_len = strlen(prefix);
  return prefix_len > 0 && prefix_len <= strlen(hex) && strncasecmp(hex, prefix, prefix_len) == 0;
}

int RpgRemoteState::compareRemoteNodes(const void* lhs, const void* rhs) {
  const RemoteNode* a = (const RemoteNode*)lhs;
  const RemoteNode* b = (const RemoteNode*)rhs;
  if (a->used != b->used) {
    return a->used ? -1 : 1;
  }
  if (!a->used) {
    return 0;
  }
  if (a->last_seen > b->last_seen) {
    return -1;
  }
  if (a->last_seen < b->last_seen) {
    return 1;
  }
  return memcmp(a->key_prefix, b->key_prefix, sizeof(a->key_prefix));
}

uint32_t RpgRemoteState::getAgeSeconds(uint32_t now_s, uint32_t seen_s) {
  return now_s >= seen_s ? (uint32_t)(now_s - seen_s) : 0;
}

int RpgRemoteState::findNode(const mesh::Identity& id) {
  for (uint8_t i = 0; i < MAX_REMOTE_NODES; i++) {
    if (_nodes[i].used && memcmp(_nodes[i].key_prefix, id.pub_key, sizeof(_nodes[i].key_prefix)) == 0) {
      return i;
    }
  }
  return -1;
}

int RpgRemoteState::allocateSlot() {
  int oldest_idx = 0;
  uint32_t oldest_seen = 0xFFFFFFFFUL;
  for (uint8_t i = 0; i < MAX_REMOTE_NODES; i++) {
    if (!_nodes[i].used) {
      return i;
    }
    if (_nodes[i].last_seen < oldest_seen) {
      oldest_seen = _nodes[i].last_seen;
      oldest_idx = i;
    }
  }
  return oldest_idx;
}

void RpgRemoteState::upsertNode(const mesh::Identity& id, const char* name, uint32_t now_s,
                                uint8_t version, uint8_t flags, uint8_t resource_summary) {
  int idx = findNode(id);
  if (idx < 0) {
    idx = allocateSlot();
    memset(&_nodes[idx], 0, sizeof(_nodes[idx]));
  }

  RemoteNode& node = _nodes[idx];
  node.used = true;
  memcpy(node.pub_key, id.pub_key, PUB_KEY_SIZE);
  memcpy(node.key_prefix, id.pub_key, sizeof(node.key_prefix));
  node.last_seen = now_s;
  node.version = version;
  node.flags = flags;
  node.resource_summary = resource_summary;
  if (name != NULL && *name != 0) {
    snprintf(node.name, sizeof(node.name), "%s", name);
  }
}

bool RpgRemoteState::resolveNodeByPrefix(const char* prefix, mesh::Identity& id, char* name, size_t name_size) const {
  for (uint8_t i = 0; i < MAX_REMOTE_NODES; i++) {
    if (_nodes[i].used && matchesHexPrefix(_nodes[i], prefix)) {
      id = mesh::Identity(_nodes[i].pub_key);
      if (name != NULL && name_size > 0) {
        snprintf(name, name_size, "%s", _nodes[i].name[0] != 0 ? _nodes[i].name : "remote");
      }
      return true;
    }
  }
  return false;
}

void RpgRemoteState::onProbeRecv(const mesh::Identity& id, const char* name, uint32_t now_s, uint8_t version,
                                 uint8_t flags, uint8_t resource_summary) {
  upsertNode(id, name, now_s, version, flags, resource_summary);
}

bool RpgRemoteState::handleCommand(const char* command, char* reply, size_t reply_size, uint32_t now_s) const {
  while (*command == ' ') {
    command++;
  }

  if (strcmp(command, "rpg nodes") == 0) {
    RemoteNode ordered[MAX_REMOTE_NODES];
    memcpy(ordered, _nodes, sizeof(ordered));
    qsort(ordered, MAX_REMOTE_NODES, sizeof(RemoteNode), compareRemoteNodes);

    size_t used = snprintf(reply, reply_size, "rpg nodes:");
    bool any = false;
    for (uint8_t i = 0; i < MAX_REMOTE_NODES && used + 1 < reply_size; i++) {
      if (!ordered[i].used) {
        break;
      }
      char key[12];
      formatKeyPrefix(ordered[i].key_prefix, key, sizeof(key));
      const char* label = ordered[i].name[0] != 0 ? ordered[i].name : key;
      bool boss_alive = (ordered[i].flags & rpg::REMOTE_FLAG_BOSS_ALIVE) != 0;
      int written = snprintf(&reply[used], reply_size - used, " %s(v%u,r%u,b%s)",
                             label, (unsigned int)ordered[i].version,
                             (unsigned int)ordered[i].resource_summary, boss_alive ? "+" : "-");
      if (written < 0 || (size_t)written >= reply_size - used) {
        reply[reply_size - 1] = 0;
        return true;
      }
      used += (size_t)written;
      any = true;
    }
    if (!any) {
      snprintf(reply, reply_size, "rpg nodes: none");
    }
    return true;
  }

  if (memcmp(command, "rpg ping ", 9) == 0) {
    const char* prefix = command + 9;
    while (*prefix == ' ') {
      prefix++;
    }
    if (*prefix == 0) {
      strcpy(reply, "rpg: ping <pubkeyprefix>");
      return true;
    }

    for (uint8_t i = 0; i < MAX_REMOTE_NODES; i++) {
      if (_nodes[i].used && matchesHexPrefix(_nodes[i], prefix)) {
        char key[12];
        formatKeyPrefix(_nodes[i].key_prefix, key, sizeof(key));
        const char* label = _nodes[i].name[0] != 0 ? _nodes[i].name : key;
        uint32_t age_s = getAgeSeconds(now_s, _nodes[i].last_seen);
        snprintf(reply, reply_size, "rpg: %s MCRPG v%u seen=%us res=%u%% boss=%s flags=%02X",
                 label, (unsigned int)_nodes[i].version, (unsigned int)age_s,
                 (unsigned int)_nodes[i].resource_summary,
                 (_nodes[i].flags & rpg::REMOTE_FLAG_BOSS_ALIVE) ? "alive" : "cooldown",
                 (unsigned int)_nodes[i].flags);
        return true;
      }
    }
    strcpy(reply, "rpg: node not known, use rpg probe or rpg scan");
    return true;
  }

  return false;
}
