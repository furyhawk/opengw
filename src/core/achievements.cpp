#include "core/achievements.hpp"

#include "core/settings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace achievements {

namespace {

// ---------------------------------------------------------------------------
// Catalog
// ---------------------------------------------------------------------------

const BadgeInfo kBadges[BADGE_COUNT] = {
    { "classical_champion", "CLASSICAL CHAMPION", "assets/images/achievements/badge_trophy.png", TRACK_MODE,
      "Complete a run in Classical mode." },
    { "endless_starlight", "ENDLESS STARLIGHT", "assets/images/achievements/badge_star.png", TRACK_MODE,
      "Complete a run in Endless mode." },
    { "pressure_breaker", "PRESSURE BREAKER", "assets/images/achievements/badge_lightning.png", TRACK_MODE,
      "Complete a run in Pressure Ladder mode." },
    { "mode_conqueror", "MODE CONQUEROR", "assets/images/achievements/badge_crown.png", TRACK_MODE,
      "Complete every gameplay mode." },
    { "elite_decorated", "ELITE DECORATED", "assets/images/achievements/badge_medal.png", TRACK_PERFORMANCE,
      "Earn a performance milestone in a match." },
    { "lifelong_legend", "LIFELONG LEGEND", "assets/images/achievements/badge_gem.png", TRACK_LIFELONG,
      "Reach lifetime rank 3 across all sessions." },
};

// Lifetime points needed per rank. Rank 1 starts at 0 points.
constexpr int kPointsPerRank = 5000;
constexpr int kMaxRank = 99;

const CosmeticInfo kCosmetics[] = {
    // id, name, unlock, threshold, requirement, hull, exhaust, missile, hud
    { "classic", "CLASSIC WHITE", UNLOCK_ALWAYS, 0, "DEFAULT",
      1.00f, 1.00f, 1.00f,
      0.90f, 1.00f, 0.35f,
      0.90f, 1.00f, 0.35f,
      1.00f, 1.00f, 0.00f },

    { "neon_cyan", "NEON CYAN", UNLOCK_MODE, 0, "FINISH CLASSICAL",
      0.20f, 0.95f, 1.00f,
      0.10f, 0.80f, 1.00f,
      0.35f, 0.95f, 1.00f,
      0.40f, 1.00f, 1.00f },

    { "solar_flare", "SOLAR FLARE", UNLOCK_MODE, 1, "FINISH ENDLESS",
      1.00f, 0.72f, 0.20f,
      1.00f, 0.45f, 0.05f,
      1.00f, 0.80f, 0.30f,
      1.00f, 0.85f, 0.30f },

    { "ion_violet", "ION VIOLET", UNLOCK_MODE, 2, "FINISH PRESSURE",
      0.75f, 0.45f, 1.00f,
      0.55f, 0.20f, 1.00f,
      0.80f, 0.55f, 1.00f,
      0.85f, 0.60f, 1.00f },

    { "crown_gold", "CROWN GOLD", UNLOCK_ALL_MODES, 0, "FINISH ALL MODES",
      1.00f, 0.88f, 0.35f,
      1.00f, 0.70f, 0.10f,
      1.00f, 0.92f, 0.45f,
      1.00f, 0.95f, 0.50f },

    { "radar_green", "RADAR GREEN", UNLOCK_MILESTONES, 1, "1 MILESTONE",
      0.40f, 1.00f, 0.50f,
      0.15f, 0.90f, 0.35f,
      0.45f, 1.00f, 0.55f,
      0.45f, 1.00f, 0.55f },

    { "crimson_ace", "CRIMSON ACE", UNLOCK_MILESTONES, 3, "3 MILESTONES",
      1.00f, 0.30f, 0.35f,
      1.00f, 0.10f, 0.20f,
      1.00f, 0.40f, 0.45f,
      1.00f, 0.45f, 0.50f },

    { "prism_legend", "PRISM LEGEND", UNLOCK_LIFETIME, 5, "LIFETIME RANK 5",
      0.85f, 0.70f, 1.00f,
      0.30f, 1.00f, 0.90f,
      1.00f, 0.85f, 0.35f,
      0.70f, 1.00f, 1.00f },
};

constexpr int kCosmeticCount = static_cast<int>(sizeof(kCosmetics) / sizeof(kCosmetics[0]));

int modesCompleted()
{
    const settings& s = settings::get();
    return (s.mModeCompleteClassical ? 1 : 0)
        + (s.mModeCompleteEndless ? 1 : 0)
        + (s.mModeCompletePressureLadder ? 1 : 0);
}

} // namespace

// ---------------------------------------------------------------------------
// Badges
// ---------------------------------------------------------------------------

const BadgeInfo& badge(int index)
{
    if (index < 0)
        index = 0;
    if (index >= BADGE_COUNT)
        index = BADGE_COUNT - 1;
    return kBadges[index];
}

int badgeCount() { return BADGE_COUNT; }

bool badgeUnlocked(int index)
{
    const settings& s = settings::get();
    switch (index) {
    case BADGE_MODE_TROPHY:
        return s.mModeCompleteClassical;
    case BADGE_MODE_STAR:
        return s.mModeCompleteEndless;
    case BADGE_MODE_LIGHTNING:
        return s.mModeCompletePressureLadder;
    case BADGE_MODE_CROWN:
        return modesCompleted() >= 3;
    case BADGE_PERF_MEDAL:
        return s.mPerformanceMilestones >= 1;
    case BADGE_LIFE_GEM:
        return lifetimeRank() >= 3;
    default:
        return false;
    }
}

int badgesUnlocked()
{
    int count = 0;
    for (int i = 0; i < BADGE_COUNT; ++i) {
        if (badgeUnlocked(i))
            ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Tracks
// ---------------------------------------------------------------------------

int trackValue(Track track)
{
    switch (track) {
    case TRACK_MODE:
        return modesCompleted();
    case TRACK_PERFORMANCE:
        return settings::get().mPerformanceMilestones;
    case TRACK_LIFELONG:
        return lifetimeRank();
    default:
        return 0;
    }
}

int trackGoal(Track track)
{
    switch (track) {
    case TRACK_MODE:
        return 3;
    case TRACK_PERFORMANCE:
        return 5;
    case TRACK_LIFELONG:
        return 10;
    default:
        return 1;
    }
}

const char* trackLabel(Track track)
{
    switch (track) {
    case TRACK_MODE:
        return "MODE MASTERY";
    case TRACK_PERFORMANCE:
        return "PERFORMANCE";
    case TRACK_LIFELONG:
        return "LIFELONG";
    default:
        return "";
    }
}

// ---------------------------------------------------------------------------
// Lifetime profile
// ---------------------------------------------------------------------------

int lifetimePoints()
{
    return std::max(0, settings::get().mLifetimeAchievementPoints);
}

int lifetimeRank()
{
    const int rank = 1 + (lifetimePoints() / kPointsPerRank);
    return std::min(rank, kMaxRank);
}

void addLifetimePoints(int amount)
{
    if (amount <= 0)
        return;
    settings::edit().mLifetimeAchievementPoints = lifetimePoints() + amount;
}

void recordModeCompletion(int modeIndex)
{
    settings& s = settings::edit();
    switch (modeIndex) {
    case 0:
        s.mModeCompleteClassical = true;
        break;
    case 1:
        s.mModeCompleteEndless = true;
        break;
    case 2:
        s.mModeCompletePressureLadder = true;
        break;
    default:
        break;
    }
}

void recordPerformanceMilestone()
{
    ++settings::edit().mPerformanceMilestones;
}

void recordInMatchRank(int rank)
{
    // Milestone bands: every 10 in-match ranks is worth one milestone.
    if (rank < 10)
        return;
    const int band = rank / 10;
    const int current = settings::get().mPerformanceMilestones;
    if (band > current) {
        // Never jump more than the bands actually earned.
        settings::edit().mPerformanceMilestones = band;
    }
}

// ---------------------------------------------------------------------------
// Cosmetics
// ---------------------------------------------------------------------------

int cosmeticCount() { return kCosmeticCount; }

const CosmeticInfo& cosmetic(int index)
{
    if (index < 0)
        index = 0;
    if (index >= kCosmeticCount)
        index = kCosmeticCount - 1;
    return kCosmetics[index];
}

bool cosmeticUnlocked(int index)
{
    const CosmeticInfo& c = cosmetic(index);
    switch (c.unlock) {
    case UNLOCK_ALWAYS:
        return true;
    case UNLOCK_MODE:
        switch (c.threshold) {
        case 0:
            return settings::get().mModeCompleteClassical;
        case 1:
            return settings::get().mModeCompleteEndless;
        case 2:
            return settings::get().mModeCompletePressureLadder;
        default:
            return false;
        }
    case UNLOCK_ALL_MODES:
        return modesCompleted() >= 3;
    case UNLOCK_MILESTONES:
        return settings::get().mPerformanceMilestones >= c.threshold;
    case UNLOCK_LIFETIME:
        return lifetimeRank() >= c.threshold;
    default:
        return false;
    }
}

int equippedCosmetic()
{
    const int idx = settings::get().mEquippedCosmetic;
    if (idx < 0 || idx >= kCosmeticCount)
        return 0;
    // Never leave the player stuck on a palette they no longer own.
    if (!cosmeticUnlocked(idx))
        return 0;
    return idx;
}

void setEquippedCosmetic(int index)
{
    if (index < 0 || index >= kCosmeticCount)
        return;
    if (!cosmeticUnlocked(index))
        return;
    settings::edit().mEquippedCosmetic = index;
}

int cycleCosmetic(int dir)
{
    if (dir == 0)
        return equippedCosmetic();

    int idx = equippedCosmetic();
    for (int step = 0; step < kCosmeticCount; ++step) {
        idx = (idx + (dir > 0 ? 1 : -1) + kCosmeticCount) % kCosmeticCount;
        if (cosmeticUnlocked(idx)) {
            settings::edit().mEquippedCosmetic = idx;
            return idx;
        }
    }
    return equippedCosmetic();
}

int nextLockedCosmetic()
{
    for (int i = 0; i < kCosmeticCount; ++i) {
        if (!cosmeticUnlocked(i))
            return i;
    }
    return -1;
}

void applyCosmetic(vector::pen& hull, vector::pen& exhaust, vector::pen& missiles, vector::pen& font)
{
    const CosmeticInfo& c = cosmetic(equippedCosmetic());

    hull.r = c.hullR;
    hull.g = c.hullG;
    hull.b = c.hullB;

    exhaust.r = c.exhaustR;
    exhaust.g = c.exhaustG;
    exhaust.b = c.exhaustB;

    missiles.r = c.missileR;
    missiles.g = c.missileG;
    missiles.b = c.missileB;

    font.r = c.hudR;
    font.g = c.hudG;
    font.b = c.hudB;
}

} // namespace achievements
