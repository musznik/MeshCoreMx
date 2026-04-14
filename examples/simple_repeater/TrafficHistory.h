#pragma once

#include <Arduino.h>
#include <stdint.h>

class TrafficHistory {
  static const uint32_t BUCKET_MS = 15UL * 60UL * 1000UL;
  static const uint8_t NUM_BUCKETS = 32;

  uint16_t _rx[NUM_BUCKETS];
  uint16_t _tx[NUM_BUCKETS];
  uint32_t _current_bucket_id;
  uint8_t _current_index;
  bool _initialized;

  void rotateTo(uint32_t now_ms);
  void appendArray(char* dest, size_t dest_size, const uint16_t* values, uint8_t value_count, uint16_t clamp_value) const;
  void buildCombinedBuckets(uint16_t rx_dest[], uint16_t tx_dest[], uint16_t clamp_value) const;

public:
  TrafficHistory();

  void recordRx(uint32_t now_ms);
  void recordTx(uint32_t now_ms);

  void formatRx(char* dest, size_t dest_size, uint32_t now_ms);
  void formatTx(char* dest, size_t dest_size, uint32_t now_ms);
  void formatRxTx(char* dest, size_t dest_size, uint32_t now_ms);
};
