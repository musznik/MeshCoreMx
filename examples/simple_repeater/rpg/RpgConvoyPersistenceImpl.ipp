#include <string.h>

namespace {

static const char* RPG_CONVOY_FILE = "/rpg_convoys";

static File openConvoyWriteFile(FILESYSTEM* fs) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  fs->remove(RPG_CONVOY_FILE);
  return fs->open(RPG_CONVOY_FILE, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return fs->open(RPG_CONVOY_FILE, "w");
#else
  return fs->open(RPG_CONVOY_FILE, "w", true);
#endif
}

}

bool RpgConvoyPersistence::exists(FILESYSTEM* fs) {
  return fs != NULL && fs->exists(RPG_CONVOY_FILE);
}

bool RpgConvoyPersistence::load(FILESYSTEM* fs, RpgConvoyState& convoy_state) {
  if (!exists(fs)) {
    return false;
  }
#if defined(RP2040_PLATFORM)
  File file = fs->open(RPG_CONVOY_FILE, "r");
#else
  File file = fs->open(RPG_CONVOY_FILE);
#endif
  if (!file) {
    return false;
  }

  uint8_t magic[4];
  uint8_t version = 0;
  RpgConvoyState::PersistedRemoteConvoy state[RpgConvoyState::MAX_REMOTE_CONVOYS];
  bool ok = file.read(magic, sizeof(magic)) == sizeof(magic);
  ok = ok && file.read(&version, sizeof(version)) == sizeof(version);
  ok = ok && memcmp(magic, "RCVY", sizeof(magic)) == 0;
  ok = ok && version == RpgConvoyState::PERSIST_VERSION;
  ok = ok && file.read((uint8_t*)state, sizeof(state)) == sizeof(state);
  file.close();
  if (!ok) {
    return false;
  }

  convoy_state.importRemoteState(state);
  return true;
}

bool RpgConvoyPersistence::save(FILESYSTEM* fs, const RpgConvoyState& convoy_state) {
  if (fs == NULL) {
    return false;
  }
  File file = openConvoyWriteFile(fs);
  if (!file) {
    return false;
  }

  const uint8_t magic[4] = {'R', 'C', 'V', 'Y'};
  const uint8_t version = RpgConvoyState::PERSIST_VERSION;
  RpgConvoyState::PersistedRemoteConvoy state[RpgConvoyState::MAX_REMOTE_CONVOYS];
  convoy_state.exportRemoteState(state);

  bool ok = file.write(magic, sizeof(magic)) == sizeof(magic);
  ok = ok && file.write(&version, sizeof(version)) == sizeof(version);
  ok = ok && file.write((const uint8_t*)state, sizeof(state)) == sizeof(state);
  file.close();
  return ok;
}
