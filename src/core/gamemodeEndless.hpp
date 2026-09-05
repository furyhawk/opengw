#pragma once

#include "core/gamemode.hpp"
#include "core/gamemodeClassical.hpp"

#include <memory>

class game;
class powerups;

// endless_mode --------------------------------------------------------------
// "Endless": a faster-pace survival variant of Classical. It runs the same
// arena and spawning systems as classical_mode (via composition -- the game
// shell only ever drives gameplay_modes, so no shell changes were needed) but
// differs in the rules:
//   * The difficulty clock ramps up much faster than the classical rate
//     (owner.mSpawner->accelerateSpawnIndex()).
//   * Weapons never auto-reroll at score milestones. Instead the player
//     collects power-up drops that upgrade the weapon one step at a time up
//     to a brand-new laser weapon, grant shield charges that each absorb one
//     hit, or hand out the laser immediately.
//   * Co-op shared life/bomb pool, music, respawns etc. behave like Classical
//     (forwarded to the embedded classical_mode).
class endless_mode : public gameplay_mode
{
  public:
    endless_mode();
    ~endless_mode() override;

    const char* name() const override;

    void begin_match(game& owner) override;
    void end_match(game& owner) override;
    void update(game& owner) override;
    void draw(game& owner, int pass) override;

    // Endless hands out weapons via pickups rather than milestone rerolls.
    bool weapon_auto_advances() const override { return false; }

    // Co-op shared life/bomb pool, owned by the embedded classical rules.
    int shared_lives() const override { return mClassical.shared_lives(); }
    int shared_bombs() const override { return mClassical.shared_bombs(); }
    void add_shared_life() override { mClassical.add_shared_life(); }
    void take_shared_life() override { mClassical.take_shared_life(); }
    void add_shared_bomb() override { mClassical.add_shared_bomb(); }
    void take_shared_bomb() override { mClassical.take_shared_bomb(); }

  private:
    // Frames between power-up drops; shrinks as the difficulty index rises.
    int dropInterval(const game& owner) const;

    // The Classical match rules that Endless builds on.
    classical_mode mClassical;

    // Pickup drops (weapon / shield / laser).
    std::unique_ptr<powerups> mPowerUps;

    // Frames until the next power-up drop.
    int mDropTimer { 0 };

    // Extra difficulty-clock speed per frame (added on top of the spawner's
    // normal index rate) that gives Endless its faster pace.
    float mDifficultyAccel { 0.008f };
};
