#pragma once

class settings
{
  public:
    settings();
    ~settings() = default;

    // Read-only access to the singleton (gameplay code).
    static const settings& get();

    // Mutable access (graphics options UI, host app).
    static settings& edit();

    // Persist / load the settings file ("settings.cfg" in the working dir).
    void load();
    void save() const;

    bool mCreditsPerGame;

    // --- Audio options ------------------------------------------------
    int mSoundVolume; // 0..100 master volume

    // --- Graphics options ---------------------------------------------
    bool mEnableGlow; // GPU bloom / glow effect
    bool mFullscreen; // start / switch to fullscreen
    bool mVsync;      // cap frame rate to the display refresh

    bool mGridSmoothing;
    bool mParticleSmoothing;
    bool mEnemySmoothing;
    bool mPlayerSmoothing;
    bool mStarSmoothing;

    int mAttractors;
    int mParticles;
    int mGridPasses;

    // --- Achievement meta progression ------------------------------------
    // Session-independent totals used by lifelong achievements.
    int mLifetimeAchievementPoints; // cumulative points earned over all runs
    int mLifetimeAchievementLevel;  // cumulative achievement rank-ups

    // Mode-completion badges.
    bool mModeCompleteClassical;
    bool mModeCompleteEndless;
    bool mModeCompletePressureLadder;

    // Performance milestones unlocked (e.g. high in-match rank reached).
    int mPerformanceMilestones;

    int displayWidth;
    int displayHeight;
};
