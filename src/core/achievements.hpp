#pragma once

#include "math/vector.hpp"

// achievements ---------------------------------------------------------------
// Central, session-independent achievement/reward system.
//
// Design goals (long-term retention):
//   * Three separate tracks give players short, medium and very long goals:
//       - MODE MASTERY : finish each game mode (short/medium).
//       - PERFORMANCE  : in-match skill milestones (medium).
//       - LIFELONG     : cumulative points across all sessions (very long).
//   * Rewards are *cosmetic* (ship palettes) so they never unbalance the game
//     but stay visible forever, which is what keeps veterans playing.
//   * Everything is derived from a handful of persisted counters in
//     settings.cfg, so saves stay tiny and forward compatible.
//
// Gameplay code only needs to call the record*() functions; the UI reads the
// badge/cosmetic catalog and the derived ranks.
namespace achievements {

// --- Tracks ----------------------------------------------------------------
enum Track
{
    TRACK_MODE = 0,  // complete each gameplay mode
    TRACK_PERFORMANCE, // elite waves / high in-match achievement ranks
    TRACK_LIFELONG,  // cumulative points over all sessions
    TRACK_COUNT
};

// --- Badges ----------------------------------------------------------------
enum Badge
{
    BADGE_MODE_TROPHY = 0, // Classical completed
    BADGE_MODE_STAR,       // Endless completed
    BADGE_MODE_LIGHTNING,  // Pressure Ladder completed
    BADGE_MODE_CROWN,      // every mode completed
    BADGE_PERF_MEDAL,      // performance milestone
    BADGE_LIFE_GEM,        // lifelong rank milestone
    BADGE_COUNT
};

struct BadgeInfo
{
    const char* id;
    const char* name;
    const char* icon; // PNG under assets/images/achievements/
    Track track;
    const char* description;
};

const BadgeInfo& badge(int index);
int badgeCount();
bool badgeUnlocked(int index);
int badgesUnlocked(); // total across all tracks

// --- Tracks ----------------------------------------------------------------
// Current value and goal for a track (e.g. 2 of 3 modes).
int trackValue(Track track);
int trackGoal(Track track);
// Short human readable progress string ("2/3", "LV 4").
const char* trackLabel(Track track);

// --- Lifetime profile ------------------------------------------------------
int lifetimePoints();
int lifetimeRank(); // 1-based, derived from lifetime points
void addLifetimePoints(int amount);

// Record events. All are idempotent for one-shot unlocks.
void recordModeCompletion(int modeIndex);
void recordPerformanceMilestone();
void recordInMatchRank(int rank); // best in-match achievement rank this run

// --- Cosmetics -------------------------------------------------------------
// A cosmetic is a ship palette (hull / exhaust / missiles / HUD text colour).
enum CosmeticUnlock
{
    UNLOCK_ALWAYS = 0,
    UNLOCK_MODE,       // `threshold` = gameplay mode index
    UNLOCK_ALL_MODES,  // every mode completed
    UNLOCK_MILESTONES, // `threshold` = performance milestones
    UNLOCK_LIFETIME,   // `threshold` = lifetime rank
};

struct CosmeticInfo
{
    const char* id;
    const char* name;
    CosmeticUnlock unlock;
    int threshold;
    const char* requirement; // shown while locked

    float hullR, hullG, hullB;
    float exhaustR, exhaustG, exhaustB;
    float missileR, missileG, missileB;
    float hudR, hudG, hudB;
};

int cosmeticCount();
const CosmeticInfo& cosmetic(int index);
bool cosmeticUnlocked(int index);
int equippedCosmetic();
void setEquippedCosmetic(int index);
// Cycle the equipped cosmetic by `dir`, skipping locked entries. Returns the
// newly equipped index.
int cycleCosmetic(int dir);
// The index of the next cosmetic that is still locked (or -1 if all owned).
int nextLockedCosmetic();

// Apply the equipped palette to a ship's pens. Alphas/line radii are kept so
// gameplay readability is unchanged.
void applyCosmetic(vector::pen& hull, vector::pen& exhaust, vector::pen& missiles, vector::pen& font);

} // namespace achievements
