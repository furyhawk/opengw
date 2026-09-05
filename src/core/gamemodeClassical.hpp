#pragma once

#include "core/gamemode.hpp"

class game;

// classical_mode ------------------------------------------------------------
// The original Trigonometry Wars match mode, now exposed as a gameplay_mode named
// "Classical": twin-stick arena survival with ever-tightening enemy waves,
// multiplier / weapon / extra-life / extra-bomb scoring and a shared co-op
// life pool.
//
// The mode owns its match-scoped state (level & skill pacing, co-op shared
// pool, match music speed) and drives the shared gameplay systems (spawner,
// players, stars, black holes, bomb, ...) that remain owned by the game
// shell, so the shell has no mode-specific rules in it.
class classical_mode : public gameplay_mode
{
  public:
    const char* name() const override;

    void begin_match(game& owner) override;
    void end_match(game& owner) override;
    void update(game& owner) override;
    void draw(game& owner, int pass) override;

    // Co-op shared life/bomb pool (only used when >1 player is in the match).
    int shared_lives() const override { return mSharedLives; }
    int shared_bombs() const override { return mSharedBombs; }
    void add_shared_life() override { ++mSharedLives; }
    void take_shared_life() override
    {
        if (mSharedLives > 0)
            --mSharedLives;
    }
    void add_shared_bomb() override { ++mSharedBombs; }
    void take_shared_bomb() override
    {
        if (mSharedBombs > 0)
            --mSharedBombs;
    }

    // Difficulty bookkeeping (reserved for wave tuning).
    int level() const { return mLevel; }
    int skill_level() const { return mSkillLevel; }

  private:
    int mSharedLives { 0 };
    int mSharedBombs { 0 };

    int mLevel { 0 };
    int mSkillLevel { 0 };

    // Match music pacing.
    float mMusicSpeed { 1.0f };
    float mMusicSpeedTarget { 1.0f };
};
