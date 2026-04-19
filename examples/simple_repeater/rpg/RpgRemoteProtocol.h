#pragma once

#include <stdint.h>

namespace rpg {

static const uint8_t REMOTE_PROTOCOL_VERSION = 1;

static const uint8_t REMOTE_FLAG_REMOTE_API = 0x01;
static const uint8_t REMOTE_FLAG_BOSS_ALIVE = 0x02;
static const uint8_t REMOTE_FLAG_CONVOY_PLANNED = 0x04;
static const uint8_t REMOTE_FLAG_SHARED_WORLD = 0x08;

}
