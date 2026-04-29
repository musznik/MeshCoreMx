// Some simple_repeater variants compile only top-level example .cpp files,
// so this translation unit pulls in the RPG implementation once for all builds.
#include "rpg/RpgGame.h"
#include "rpg/RpgGameImpl.ipp"
#include "rpg/RpgRemoteState.h"
#include "rpg/RpgRemoteStateImpl.ipp"
#include "rpg/RpgWorldState.h"
#include "rpg/RpgWorldStateImpl.ipp"
#include "rpg/RpgConvoyState.h"
#include "rpg/RpgConvoyStateImpl.ipp"
#include "rpg/RpgConvoyPersistence.h"
#include "rpg/RpgConvoyPersistenceImpl.ipp"
#include "rpg/RpgConvoyProtocol.h"
#include "rpg/RpgConvoyProtocolImpl.ipp"
