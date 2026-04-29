#pragma once

#include <Arduino.h>
#include <Mesh.h>

class RpgConvoyState {
public:
  static const uint8_t MAX_REMOTE_CONVOYS = 6;
  static const uint32_t CONVOY_STAY_MS = 5UL * 60UL * 1000UL;
  static const uint32_t CONVOY_STAY_SECS = 5UL * 60UL;
  static const uint8_t PERSIST_VERSION = 1;

  enum LocalState : uint8_t {
    LOCAL_IDLE = 0,
    LOCAL_SENT = 1,
    LOCAL_WORKING = 2
  };

  struct LocalConvoy {
    uint8_t state;
    uint32_t convoy_id;
    uint8_t player_key[8];
    uint8_t target_pub_key[PUB_KEY_SIZE];
    char target_name[17];
    uint32_t started_at_ms;
    uint32_t acked_at_ms;
  };

  struct RemoteConvoy {
    bool used;
    uint32_t convoy_id;
    uint8_t origin_pub_key[PUB_KEY_SIZE];
    uint8_t player_key[8];
    uint32_t arrived_at_s;
    bool result_finalized;
    uint16_t wood;
    uint16_t ore;
    uint16_t herbs;
    uint16_t relics;
    uint16_t gold;
  };

  struct PersistedRemoteConvoy {
    uint8_t used;
    uint32_t convoy_id;
    uint8_t origin_pub_key[PUB_KEY_SIZE];
    uint8_t player_key[8];
    uint32_t arrived_at_s;
    uint8_t result_finalized;
    uint16_t wood;
    uint16_t ore;
    uint16_t herbs;
    uint16_t relics;
    uint16_t gold;
  } __attribute__((packed));

private:
  LocalConvoy _local;
  RemoteConvoy _remote[MAX_REMOTE_CONVOYS];
  uint32_t _last_completed_convoy_id;

  static void copyPlayerKey(uint8_t dest[8], const uint8_t* player_id, size_t player_id_len);
  int findRemote(uint32_t convoy_id, const uint8_t origin_pub_key[PUB_KEY_SIZE]) const;
  int allocateRemoteSlot();

public:
  RpgConvoyState();

  bool hasActiveLocal() const;
  uint32_t getLocalConvoyId() const;
  const uint8_t* getLocalPlayerKey() const;
  const uint8_t* getLocalTargetPubKey() const;

  bool beginLocal(uint32_t convoy_id, const uint8_t* player_id, size_t player_id_len,
                  const uint8_t target_pub_key[PUB_KEY_SIZE], const char* target_name, uint32_t now_ms,
                  char* reply, size_t reply_size);
  void formatLocalStatus(char* reply, size_t reply_size, uint32_t now_ms) const;
  bool markAcked(uint32_t convoy_id, char* reply, size_t reply_size, uint32_t now_ms);
  bool shouldIgnoreCompleted(uint32_t convoy_id) const;
  bool completeLocal(uint32_t convoy_id);
  void clearLocal();

  bool upsertRemote(uint32_t convoy_id, const uint8_t origin_pub_key[PUB_KEY_SIZE],
                    const uint8_t* player_id, size_t player_id_len, uint32_t now_s);
  bool getRemote(uint32_t convoy_id, const uint8_t origin_pub_key[PUB_KEY_SIZE], RemoteConvoy& out) const;
  bool finalizeRemoteResult(uint32_t convoy_id, const uint8_t origin_pub_key[PUB_KEY_SIZE],
                            uint16_t wood, uint16_t ore, uint16_t herbs, uint16_t relics, uint16_t gold);
  void exportRemoteState(PersistedRemoteConvoy out[MAX_REMOTE_CONVOYS]) const;
  void importRemoteState(const PersistedRemoteConvoy in[MAX_REMOTE_CONVOYS]);
};
