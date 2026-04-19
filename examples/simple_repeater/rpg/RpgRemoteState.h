#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include "RpgRemoteProtocol.h"

class RpgRemoteState {
public:
  static const uint8_t MAX_REMOTE_NODES = 12;

  struct RemoteNode {
    bool used;
    uint8_t pub_key[PUB_KEY_SIZE];
    uint8_t key_prefix[8];
    char name[17];
    uint32_t last_seen;
    uint8_t version;
    uint8_t flags;
    uint8_t resource_summary;
  };

private:
  RemoteNode _nodes[MAX_REMOTE_NODES];

  static void formatKeyPrefix(const uint8_t* key_prefix, char* dest, size_t dest_size);
  static bool matchesHexPrefix(const RemoteNode& node, const char* prefix);
  static int compareRemoteNodes(const void* lhs, const void* rhs);
  static uint32_t getAgeSeconds(uint32_t now_s, uint32_t seen_s);
  int findNode(const mesh::Identity& id);
  int allocateSlot();
  void upsertNode(const mesh::Identity& id, const char* name, uint32_t now_s, uint8_t version,
                  uint8_t flags, uint8_t resource_summary);

public:
  RpgRemoteState();

  bool resolveNodeByPrefix(const char* prefix, mesh::Identity& id, char* name, size_t name_size) const;
  void onProbeRecv(const mesh::Identity& id, const char* name, uint32_t now_s, uint8_t version,
                   uint8_t flags, uint8_t resource_summary);
  bool handleCommand(const char* command, char* reply, size_t reply_size, uint32_t now_s) const;
};
