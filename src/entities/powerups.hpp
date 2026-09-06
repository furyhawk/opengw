#pragma once

#include "math/point3d.hpp"

class game;
class player;

// powerups -------------------------------------------------------------------
// Pickup drops used by the Endless gameplay mode. They are spawned periodically
// at random grid positions, bob in place, and are collected when a joined,
// running player touches one. Each drop grants either a weapon upgrade, a
// shield charge, the laser weapon or a homing-missile secondary weapon (see
// player.hpp for the weapon ladder and shield-charge rules).
//
// The pool is mode-scoped: the Endless mode owns one instance and drives
// update()/draw() only while its match is active, so pickups naturally vanish
// with the mode (they are never part of the shared enemy pools).
class powerups
{
  public:
    typedef enum
    {
        PICKUP_WEAPON = 0, // advance the player's weapon one step
        PICKUP_SHIELD,     // +1 shield charge (absorbs one hit)
        PICKUP_LASER,      // grant the laser weapon immediately
        PICKUP_HOMING,     // grant the homing-missile secondary weapon
        PICKUP_NUM_TYPES
    } PickupType;

    powerups() = default;

    // Clear all live pickups (called at the start of a match).
    void reset();

    // Advance the live pickups by one logic tick (drift/bob, expiry, pickup).
    void update(game& owner);

    // Draw the live pickups for the current render pass.
    void draw(game& owner, int pass);

    // Spawn a random drop somewhere on the grid (away from the players).
    void spawnRandom(game& owner);
    // Spawn a specific drop at an explicit position.
    void spawnAt(game& owner, PickupType type, const Point3d& pos);

    int activeCount() const { return mActive; }

  private:
    struct Pickup
    {
        bool enabled { false };
        PickupType type { PICKUP_WEAPON };
        Point3d pos;
        float phase { 0.0f }; // bob animation
        int ttl { 0 };
    };

    static const int kPoolSize = 24;
    static const int kMaxOnScreen = 5; // cap concurrent live drops
    static const int kLifetime = 60 * 15; // ~15 seconds on the floor
    static const int kBlinkTime = 60 * 3; // blink during the last ~3s
    static const float kCollectRadius;

    void collect(game& owner, Pickup& pk, player* p);
    void disable(Pickup& pk);

    Pickup mPool[kPoolSize];
    int mActive { 0 };
};
