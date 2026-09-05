#pragma once

// classical_params ----------------------------------------------------------
// Gameplay balance / difficulty parameters for the "Classical" match mode,
// loaded from the plain-text file "classical.cfg" (key=value lines; '#' lines
// are comments) when the game starts.
//
// The file is optional: every field has a compiled-in default below that
// mirrors the original hardcoded tuning of the classical mode, so a missing,
// partial or hand-edited file simply overrides the values that are present
// and the game always runs with the defaults it needs.
//
// Keys are matched by exact name in load() (see classicalparams.cpp); unknown
// keys and malformed lines are ignored so the file can grow comments and new
// fields without breaking. Values are read through get(), so gameplay code can
// re-read the current set at any time.

class classical_params
{
  public:
    // Read-only access to the singleton (gameplay code). Loads "classical.cfg"
    // on first use and applies it over the compiled-in defaults.
    static const classical_params& get();

    // (Re)read "classical.cfg" over the defaults. Returns true if the file was
    // found and opened.
    bool load();

    // ---- Match / flow rules ------------------------------------------------
    int matchBrightnessStart { -2 };    // grid fade-in start value on begin_match
    float matchBrightnessRamp { 0.05f };   // grid brightness ramp per frame toward 1
    float matchMusicNormalSpeed { 1.0f };  // match music pitch when nobody respawns
    float matchMusicRespawnSpeed { 0.5f }; // instant music pitch while a player respawns
    float matchMusicSpeedUpStep { 0.005f }; // slow ramp back up toward target
    float matchMusicSpeedDownStep { 0.01f }; // faster drop toward target
    int matchCoopLives { 10 };           // co-op (>1 player) shared life pool start
    int matchCoopBombs { 0 };            // co-op shared bomb pool start

    // ---- Player ship & single-player economy --------------------------------
    int playerStartLives { 5 };          // starting lives, single player
    int playerStartBombs { 5 };          // starting bombs, single player
    int playerSpawnTime { 40 };          // frames of the spawn transition
    int playerDestroyTime { 40 };        // frames of the death wipe
    int playerShieldTime { 250 };        // spawn-shield duration (frames)
    int playerShieldWarnTime { 60 };     // frames left when shield starts blinking/sfx
    float playerMoveSpeed { 0.6f };      // top ship speed (world units / frame)
    float playerMoveTurnRate { 0.2f };   // ship turn response (fraction per frame)
    float playerStickDeadZone { 0.1f };  // stick magnitude below which no input
    float playerStickFullZone { 0.6f };  // stick magnitude above which = full speed
    int playerBombCooldown { 50 };       // frames between bombs
    int playerWeaponChangeScore { 10000 };   // points that trigger a weapon change
    int playerExtraLifeScore { 75000 };      // points between extra lives
    int playerExtraBombScore { 100000 };     // points between extra bombs
    int playerMultiplierKillCount { 25 };    // kills per multiplier step (per life)
    int playerMultiplierMax { 6 };           // effective multiplier ceiling (x1 base +5)
    int playerWeapon1Chance { 50 };          // % to reroll to weapon 1 (else 2)

    // ---- Weapon patterns ----------------------------------------------------
    // weapon 0 "twin": interval / missile speed / spread half-angle / player
    // velocity inheritance added to missiles.
    int weapon0Interval { 6 };
    float weapon0MissileSpeed { 0.7f };
    float weapon0Spread { 0.4f };
    float weapon0InheritSpeed { 0.5f };
    // weapon 1 "heavy alternating": double-tap cadence (intervalA/intervalB).
    int weapon1IntervalA { 4 };
    int weapon1IntervalB { 1 };
    float weapon1MissileSpeed { 1.2f };
    float weapon1Spread { 0.8f };
    float weapon1InheritSpeed { 0.5f };
    // weapon 2 "5-way": inner/outer off-axis starts and spreads.
    int weapon2Interval { 7 };
    float weapon2MissileSpeed { 0.9f };
    float weapon2StartInner { 0.1f };
    float weapon2StartOuter { 0.15f };
    float weapon2SpreadInner { 0.05f };
    float weapon2SpreadOuter { 0.09f };
    float weapon2InheritSpeed { 0.5f };

    // ---- Bomb -----------------------------------------------------------------
    int bombMaxRings { 20 };            // pooled ring count (buffer ceiling)
    float bombRingRadius { 1.0f };      // starting blast-ring radius
    float bombRingThickness { 6.0f };   // ring line thickness (visual)
    float bombRingSpeed { 2.0f };       // ring expansion per frame
    int bombRingTimeToLive { 200 };     // ring lifetime (frames)
    float bombRingMaxRadius { 100.0f }; // ring disappears past this radius
    float bombRingKillBand { 10.0f };   // kill-band width just inside the ring front
    float bombRingThicknessStep { 0.03f }; // ring visual thickening per frame

    // ---- Spawner: difficulty clock & cadence ---------------------------------
    int spawnerMaxIndex { 40 };         // difficulty index ceiling
    float spawnerIndexRate { 0.0008f }; // difficulty clock speed per frame
    int spawnerRespawnWaitTimer { 50 }; // spawner pause while a SP player is down
    int spawnerRespawnDelay { 50 };     // frames between respawn attempts
    int spawnerScatterInterval { 100 }; // frames between scatter spawns (early)
    int spawnerScatterEveryFrameIndex { 10 }; // scatter spawns every frame past this index
    int spawnerWaveCadence { 20 };      // frames between new waves
    int spawnerWaveChoices { 13 };      // how many weighted wave choices there are
    int spawnerWaveCap1Index { 1 };     // index past which 1 concurrent wave is allowed
    int spawnerWaveCap2Index { 12 };    // index past which 2 concurrent waves allowed
    int spawnerWaveUnlimitedIndex { 20 }; // index past which waves are unlimited
    int spawnerBlackholeChance { 4 };   // % per scatter check once black holes open
    int spawnerBlackholeIndex { 2 };    // index past which scatter black holes open

    // ---- Spawner: scatter target populations (index-gated) -------------------
    int spawnerWandererEarly { 2 };     // wanderer population at index <= 1
    int spawnerWandererMid { 4 };       // wanderer population while index is 2
    int spawnerGruntEarly { 2 };        // grunt population at index <= 1
    int spawnerGruntLate { 4 };         // grunt population past index 1
    int spawnerSpinnerScatter { 2 };    // spinner population past index 0
    int spawnerWeaverScatter { 2 };     // weaver population past index 0

    // ---- Spawner: wave sizing -------------------------------------------------
    // Population scalars scale wave sizes with difficulty progress. Keep these
    // at or below the matching numEnemy* hard pool ceilings in enemies.hpp.
    int wavePopGrunt { 200 };
    int wavePopWeaver { 200 };
    int wavePopSnake { 50 };
    int wavePopSpinner { 100 };
    int wavePopBlackhole { 8 };
    int wavePopMayfly { 400 };
    int wavePopRepulsor { 4 };
    // Wave size floors (min enemies per generated wave).
    int waveMinGrunt { 20 };
    int waveMinWeaver { 20 };
    int waveMinSnake { 8 };
    int waveMinSpinner { 20 };
    int waveMinBlackhole { 4 };
    int waveMinMayfly { 50 };
    // Difficulty index gates for the rarer wave types / rush divide.
    int waveSnakeIndex { 4 };
    int waveBlackholeIndex { 4 };
    int waveMayflyIndex { 8 };
    int waveRepulsorIndex { 4 };
    int waveRushDivide { 2 };           // rush waves spawn half the swarm amount

    // ---- Spawner: placement ------------------------------------------------
    float spawnerScatterMargin { 15.0f };   // scatter spawn inset from the grid
    float spawnerWaveMargin { 2.0f };       // wave corner spawn inset from the grid
    float spawnerRushJitter { 4.0f };       // rush spawn jitter spread (rand*4-2)
    float spawnerRushRadiusPlayer { 40.0f };   // rush ring radius for normal enemies
    float spawnerRushRadiusBlackhole { 80.0f }; // rush ring radius for black holes
    float spawnerSwarmJitter { 10.0f };     // swarm corner jitter spread (rand*10-5)
    int spawnerSwarmWaveCadence { 10 };     // frames between swarm corner dumps

    // ---- Base entity pacing & global difficulty creep ------------------------
    float entityAggressionStep { 0.0002f }; // per-frame global enemy speed creep
    int entitySpawnTime { 40 };         // default spawn transition length (frames)
    int entityDestroyTime { 3 };        // default destroy transition length
    int entityIndicateTime { 75 };      // default "indicate" (spawn warning) length

    // ---- Per-enemy kill scores -----------------------------------------------
    int scoreGrunt { 50 };
    int scoreWanderer { 25 };
    int scoreWeaver { 100 };
    int scoreSpinner { 100 };
    int scoreTinySpinner { 50 };
    int scoreSnake { 50 };
    int scoreSnakeSegment { 0 };
    int scoreMayfly { 50 };
    int scoreRepulsor { 100 };
    int scoreBlackhole { 50 };          // black hole feed score (not normally killed)
    int scoreBlackholeReward { 150 };   // points paid when a black hole is destroyed
    int scoreProton { 50 };
};
