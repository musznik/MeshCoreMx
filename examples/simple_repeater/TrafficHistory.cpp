#include "TrafficHistory.h"

#include <stdio.h>
#include <string.h>

TrafficHistory::TrafficHistory() {
  memset(_rx, 0, sizeof(_rx));
  memset(_tx, 0, sizeof(_tx));
  _current_bucket_id = 0;
  _current_index = 0;
  _initialized = false;
}

void TrafficHistory::rotateTo(uint32_t now_ms) {
  uint32_t bucket_id = now_ms / BUCKET_MS;
  if (!_initialized) {
    _initialized = true;
    _current_bucket_id = bucket_id;
    _current_index = 0;
    return;
  }

  if (bucket_id <= _current_bucket_id) {
    return;
  }

  uint32_t step_count = bucket_id - _current_bucket_id;
  if (step_count >= NUM_BUCKETS) {
    memset(_rx, 0, sizeof(_rx));
    memset(_tx, 0, sizeof(_tx));
    _current_index = 0;
  } else {
    while (step_count-- > 0) {
      _current_index = (_current_index + 1) % NUM_BUCKETS;
      _rx[_current_index] = 0;
      _tx[_current_index] = 0;
    }
  }

  _current_bucket_id = bucket_id;
}

void TrafficHistory::recordRx(uint32_t now_ms) {
  rotateTo(now_ms);
  if (_rx[_current_index] < UINT16_MAX) {
    _rx[_current_index]++;
  }
}

void TrafficHistory::recordTx(uint32_t now_ms) {
  rotateTo(now_ms);
  if (_tx[_current_index] < UINT16_MAX) {
    _tx[_current_index]++;
  }
}

void TrafficHistory::appendArray(char* dest, size_t dest_size, const uint16_t* values, uint8_t value_count, uint16_t clamp_value) const {
  if (dest_size == 0) {
    return;
  }

  dest[0] = 0;
  size_t used = 0;

  for (uint8_t i = 0; i < value_count; i++) {
    uint16_t value = values[i];
    if (value > clamp_value) {
      value = clamp_value;
    }

    int written = snprintf(&dest[used], dest_size - used, i == 0 ? "%u" : ",%u", (unsigned int)value);
    if (written < 0 || (size_t)written >= dest_size - used) {
      dest[dest_size - 1] = 0;
      return;
    }
    used += (size_t)written;
  }
}

void TrafficHistory::buildCombinedBuckets(uint16_t rx_dest[], uint16_t tx_dest[], uint16_t clamp_value) const {
  uint8_t start = (_current_index + 1) % NUM_BUCKETS;

  for (uint8_t i = 0; i < NUM_BUCKETS / 2; i++) {
    uint16_t rx_sum = 0;
    uint16_t tx_sum = 0;

    for (uint8_t j = 0; j < 2; j++) {
      uint8_t idx = (start + i * 2 + j) % NUM_BUCKETS;
      rx_sum += _rx[idx];
      tx_sum += _tx[idx];
    }

    rx_dest[i] = rx_sum > clamp_value ? clamp_value : rx_sum;
    tx_dest[i] = tx_sum > clamp_value ? clamp_value : tx_sum;
  }
}

void TrafficHistory::formatRx(char* dest, size_t dest_size, uint32_t now_ms) {
  rotateTo(now_ms);
  uint16_t ordered[NUM_BUCKETS];
  uint8_t start = (_current_index + 1) % NUM_BUCKETS;
  for (uint8_t i = 0; i < NUM_BUCKETS; i++) {
    ordered[i] = _rx[(start + i) % NUM_BUCKETS];
  }
  appendArray(dest, dest_size, ordered, NUM_BUCKETS, 9999);
}

void TrafficHistory::formatTx(char* dest, size_t dest_size, uint32_t now_ms) {
  rotateTo(now_ms);
  uint16_t ordered[NUM_BUCKETS];
  uint8_t start = (_current_index + 1) % NUM_BUCKETS;
  for (uint8_t i = 0; i < NUM_BUCKETS; i++) {
    ordered[i] = _tx[(start + i) % NUM_BUCKETS];
  }
  appendArray(dest, dest_size, ordered, NUM_BUCKETS, 9999);
}

void TrafficHistory::formatRxTx(char* dest, size_t dest_size, uint32_t now_ms) {
  rotateTo(now_ms);

  uint16_t rx_combined[NUM_BUCKETS / 2];
  uint16_t tx_combined[NUM_BUCKETS / 2];
  char rx_buf[80];
  char tx_buf[80];

  buildCombinedBuckets(rx_combined, tx_combined, 999);
  appendArray(rx_buf, sizeof(rx_buf), rx_combined, NUM_BUCKETS / 2, 999);
  appendArray(tx_buf, sizeof(tx_buf), tx_combined, NUM_BUCKETS / 2, 999);

  snprintf(dest, dest_size, "rx30=%s;tx30=%s", rx_buf, tx_buf);
}
