#pragma once

#include <Arduino.h>
#include <helpers/IdentityStore.h>

#include "RpgConvoyState.h"

class RpgConvoyPersistence {
public:
  static bool exists(FILESYSTEM* fs);
  static bool load(FILESYSTEM* fs, RpgConvoyState& convoy_state);
  static bool save(FILESYSTEM* fs, const RpgConvoyState& convoy_state);
};
