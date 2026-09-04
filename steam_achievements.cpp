#include "steam_achievements.h"
#include "achievement_id.h"

#ifdef USE_STEAMWORKS

#include "extern/steamworks/public/steam/isteamuserstats.h"
#include "extern/steamworks/public/steam/isteamuser.h"

SteamAchievements::SteamAchievements() {
  if (auto stats = SteamUserStats())
    if (auto user = SteamUser())
      if (user->BLoggedOn()) {
        stats->RequestCurrentStats();
      }
}

void SteamAchievements::achieve(AchievementId id) {
  if (auto stats = SteamUserStats()) {
    stats->SetAchievement(id.data());
    stats->StoreStats();
  }
}

#else // USE_STEAMWORKS

// No Steamworks SDK available in this build -- achievements are a no-op.
SteamAchievements::SteamAchievements() {}
void SteamAchievements::achieve(AchievementId) {}

#endif // USE_STEAMWORKS
