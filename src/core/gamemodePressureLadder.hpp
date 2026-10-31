#pragma once

#include "core/gamemode.hpp"
#include "core/gamemodeClassical.hpp"

class game;

// pressure_ladder_mode ------------------------------------------------------
// A structured survival mode: the arena escalates through fixed danger tiers,
// with elite waves and short hazard pulses instead of a flat endless ramp. It
// composes the existing classical rules and adds a tiered pressure system on
// top of the standard spawner/player loop.
class pressure_ladder_mode : public gameplay_mode
{
  public:
    const char* name() const override;

    void begin_match(game& owner) override;
    void end_match(game& owner) override;
    void update(game& owner) override;
    void draw(game& owner, int pass) override;

    // The mode rewards weapon pickups rather than score-driven auto-rerolls.
    bool weapon_auto_advances() const override { return false; }

    // Shared lives/bombs follow the same co-op rules as the classical mode.
    int shared_lives() const override { return mClassical.shared_lives(); }
    int shared_bombs() const override { return mClassical.shared_bombs(); }
    void add_shared_life() override { mClassical.add_shared_life(); }
    void take_shared_life() override { mClassical.take_shared_life(); }
    void add_shared_bomb() override { mClassical.add_shared_bomb(); }
    void take_shared_bomb() override { mClassical.take_shared_bomb(); }

  public:
    int tier() const { return mTier; }
    float scoreMultiplier() const { return mScoreMultiplier; }
    float eliteTimer() const { return mEliteTimer; }
    float pulseTimer() const { return mPulseTimer; }

  private:
    void triggerPressurePulse(game& owner);
    void awardTierBonus(game& owner);
    void awardEliteBonus(game& owner);
    void advanceTier(game& owner);
    void applyArenaModifier(game& owner);
    void triggerEliteWave(game& owner);

    classical_mode mClassical;

    int mTier { 1 };
    float mTierTimer { 0.0f };
    float mEliteTimer { 0.0f };
    float mPulseTimer { 0.0f };
    float mEliteWaveTimer { 0.0f };
    float mScoreMultiplier { 1.0f };
    bool mEliteWaveActive { false };
};
