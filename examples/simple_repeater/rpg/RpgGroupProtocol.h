#pragma once

#include <string.h>

#include <Mesh.h>

#include "../RateLimiter.h"
#include "RpgRemoteProtocol.h"
#include "RpgRemoteState.h"
#include "RpgWorldState.h"

class RpgGroupProtocol {
public:
  static const uint8_t MAGIC0 = 'M';
  static const uint8_t MAGIC1 = 'R';
  static const uint8_t TYPE_DISCOVER_REQ = 0x01;
  static const uint8_t TYPE_DISCOVER_RESP = 0x02;
  static const uint8_t TYPE_PRESENCE = 0x03;

  struct Result {
    bool handled;
    bool should_send_response;
    uint8_t response[96];
    size_t response_len;
    uint32_t response_delay_ms;

    Result() : handled(false), should_send_response(false), response_len(0), response_delay_ms(0) { }
  };

  static size_t buildDiscoverRequest(uint8_t* dest, const mesh::LocalIdentity& self_id, uint32_t tag,
                                     const uint8_t* target_prefix, uint8_t target_prefix_len) {
    size_t len = 0;
    dest[len++] = MAGIC0;
    dest[len++] = MAGIC1;
    dest[len++] = rpg::REMOTE_PROTOCOL_VERSION;
    dest[len++] = TYPE_DISCOVER_REQ;
    memcpy(&dest[len], &tag, 4); len += 4;
    memcpy(&dest[len], self_id.pub_key, PUB_KEY_SIZE); len += PUB_KEY_SIZE;
    dest[len++] = target_prefix_len;
    if (target_prefix_len > 0) {
      memcpy(&dest[len], target_prefix, target_prefix_len);
      len += target_prefix_len;
    }
    return len;
  }

  static size_t buildPresence(uint8_t* dest, const mesh::LocalIdentity& self_id, const char* node_name,
                              uint32_t now_ms, RpgWorldState& world_state) {
    size_t len = 0;
    dest[len++] = MAGIC0;
    dest[len++] = MAGIC1;
    dest[len++] = rpg::REMOTE_PROTOCOL_VERSION;
    dest[len++] = TYPE_PRESENCE;
    memcpy(&dest[len], self_id.pub_key, PUB_KEY_SIZE); len += PUB_KEY_SIZE;
    dest[len++] = world_state.getRemoteFlags(now_ms);
    dest[len++] = world_state.getResourceSummary(now_ms);
    size_t name_len = strlen(node_name);
    if (name_len > 16) {
      name_len = 16;
    }
    dest[len++] = (uint8_t)name_len;
    memcpy(&dest[len], node_name, name_len); len += name_len;
    return len;
  }

  static Result handleInbound(const mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel,
                              const mesh::GroupChannel& rpg_channel, const uint8_t* data, size_t len,
                              const mesh::LocalIdentity& self_id, uint32_t now_s, uint32_t now_ms, mesh::RNG& rng,
                              RateLimiter& limiter, RpgWorldState& world_state, RpgRemoteState& remote_state,
                              const char* node_name) {
    Result result;
    if (type != PAYLOAD_TYPE_GRP_DATA || memcmp(channel.hash, rpg_channel.hash, sizeof(channel.hash)) != 0) {
      return result;
    }
    result.handled = true;

    if (len < 8 || data[0] != MAGIC0 || data[1] != MAGIC1 || data[2] != rpg::REMOTE_PROTOCOL_VERSION) {
      return result;
    }

    uint8_t msg_type = data[3];
    if (msg_type == TYPE_DISCOVER_REQ) {
      if (len < 41 || !limiter.allow(now_s)) {
        return result;
      }
      const uint8_t* requester_pub = &data[8];
      uint8_t prefix_len = data[40];
      if (prefix_len > 8 || len < (size_t)(41 + prefix_len)) {
        return result;
      }
      if (memcmp(requester_pub, self_id.pub_key, PUB_KEY_SIZE) == 0) {
        return result;
      }
      if (prefix_len > 0 && memcmp(self_id.pub_key, &data[41], prefix_len) != 0) {
        return result;
      }

      size_t out = 0;
      result.response[out++] = MAGIC0;
      result.response[out++] = MAGIC1;
      result.response[out++] = rpg::REMOTE_PROTOCOL_VERSION;
      result.response[out++] = TYPE_DISCOVER_RESP;
      memcpy(&result.response[out], &data[4], 4); out += 4; // tag
      memcpy(&result.response[out], requester_pub, PUB_KEY_SIZE); out += PUB_KEY_SIZE;
      memcpy(&result.response[out], self_id.pub_key, PUB_KEY_SIZE); out += PUB_KEY_SIZE;
      result.response[out++] = world_state.getRemoteFlags(now_ms);
      result.response[out++] = world_state.getResourceSummary(now_ms);
      size_t name_len = strlen(node_name);
      if (name_len > 16) {
        name_len = 16;
      }
      result.response[out++] = (uint8_t)name_len;
      memcpy(&result.response[out], node_name, name_len); out += name_len;
      result.should_send_response = true;
      result.response_len = out;
      result.response_delay_ms = rng.nextInt(1000, 7000);
      return result;
    }

    if (msg_type == TYPE_DISCOVER_RESP) {
      if (len < 75) {
        return result;
      }
      const uint8_t* requester_pub = &data[8];
      if (memcmp(requester_pub, self_id.pub_key, PUB_KEY_SIZE) != 0) {
        return result;
      }
      mesh::Identity responder(&data[40]);
      if (responder.matches(self_id)) {
        return result;
      }
      uint8_t flags = data[72];
      uint8_t resource_summary = data[73];
      uint8_t name_len = data[74];
      if (name_len > 16 || len < (size_t)(75 + name_len)) {
        return result;
      }
      char name[17];
      name[0] = 0;
      if (name_len > 0) {
        memcpy(name, &data[75], name_len);
        name[name_len] = 0;
      }
      remote_state.onProbeRecv(responder, name, now_s, data[2], flags, resource_summary);
      return result;
    }

    if (msg_type == TYPE_PRESENCE) {
      if (len < 39) {
        return result;
      }
      mesh::Identity responder(&data[4]);
      if (responder.matches(self_id)) {
        return result;
      }
      uint8_t flags = data[36];
      uint8_t resource_summary = data[37];
      uint8_t name_len = data[38];
      if (name_len > 16 || len < (size_t)(39 + name_len)) {
        return result;
      }
      char name[17];
      name[0] = 0;
      if (name_len > 0) {
        memcpy(name, &data[39], name_len);
        name[name_len] = 0;
      }
      remote_state.onProbeRecv(responder, name, now_s, data[2], flags, resource_summary);
      return result;
    }

    return result;
  }
};
