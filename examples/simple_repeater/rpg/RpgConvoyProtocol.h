#pragma once

#include <Arduino.h>
#include <Mesh.h>

#include "RpgConvoyPersistence.h"
#include "RpgConvoyState.h"
#include "RpgGame.h"
#include "RpgWorldState.h"

class RpgConvoyProtocolHost {
public:
  virtual ~RpgConvoyProtocolHost() {}

  virtual const mesh::LocalIdentity& getRpgSelfId() const = 0;
  virtual uint32_t getRpgNowMs() const = 0;
  virtual uint32_t getRpgNowS() const = 0;
  virtual uint32_t getRpgUniqueTime() const = 0;
  virtual mesh::RNG& getRpgRng() = 0;
  virtual bool resolveRpgTargetByPrefix(const char* prefix, mesh::Identity& id, char* name, size_t name_size) const = 0;
  virtual mesh::Packet* createRpgAnonRequest(const mesh::Identity& target, const uint8_t* data, size_t len) = 0;
  virtual mesh::Packet* createRpgRawReply(const mesh::Identity& dest, const uint8_t* secret, uint8_t type,
                                          const uint8_t* body, size_t body_len) = 0;
  virtual void sendRpgFlood(mesh::Packet* packet, uint32_t delay_ms) = 0;
  virtual bool sendDirectBackToFloodSender(mesh::Packet* packet, mesh::Packet* reply) = 0;
  virtual void debugPrintRpg(const char* text) = 0;
};

class RpgConvoyProtocol {
public:
  static const uint8_t ANON_REQ_TYPE = 0x04;
  static const uint8_t RAW_MAGIC = 0xA7;
  static const uint8_t RAW_TYPE_ACK = 0x01;
  static const uint8_t RAW_TYPE_WAIT = 0x02;
  static const uint8_t RAW_TYPE_RESULT = 0x03;
  static const uint8_t RAW_TYPE_ERROR = 0x04;
  static const uint8_t REQ_SEND = 0x01;
  static const uint8_t REQ_COLLECT = 0x02;

  static bool handleCommand(RpgConvoyProtocolHost& host, const uint8_t* player_id, size_t player_id_len,
                            char* command, char* reply, size_t reply_size, RpgGame& game,
                            RpgConvoyState& convoy_state);
  static bool handleAnonRequest(RpgConvoyProtocolHost& host, FILESYSTEM* fs, mesh::Packet* packet,
                                const uint8_t* secret, const mesh::Identity& sender, const uint8_t* data,
                                size_t len, RpgConvoyState& convoy_state, RpgWorldState& world_state);
  static bool handleRawData(RpgConvoyProtocolHost& host, mesh::Packet* packet, RpgConvoyState& convoy_state,
                            RpgGame& game);
};
