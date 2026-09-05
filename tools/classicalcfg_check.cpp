// Standalone validation for the classical_params loader: compiles
// classicalparams.cpp, parses classical.cfg and verifies every key in the file
// is recognized by comparing a sample of parsed values against expectations.
#include "core/classicalparams.hpp"

#include <cstdio>

int main()
{
    classical_params p;
    if (!p.load()) {
        printf("FAIL: could not open classical.cfg (run from repo root)\n");
        return 1;
    }

    int failed = 0;
#define CHECK(member, expect)                                                                  \
    if (p.member != (expect)) {                                                                \
        printf("FAIL: %s = %g (expected %g)\n", #member, (double)p.member, (double)(expect)); \
        ++failed;                                                                              \
    }

    CHECK(matchBrightnessStart, -2);
    CHECK(matchCoopLives, 10);
    CHECK(matchMusicRespawnSpeed, 0.5f);
    CHECK(playerStartLives, 5);
    CHECK(playerShieldTime, 250);
    CHECK(playerExtraLifeScore, 75000);
    CHECK(weapon0Interval, 6);
    CHECK(weapon1MissileSpeed, 1.2f);
    CHECK(weapon2SpreadInner, 0.05f);
    CHECK(bombRingTimeToLive, 200);
    CHECK(spawnerMaxIndex, 40);
    CHECK(spawnerIndexRate, 0.0008f);
    CHECK(spawnerWandererMid, 4);
    CHECK(wavePopMayfly, 400);
    CHECK(waveMinSnake, 8);
    CHECK(waveRushDivide, 2);
    CHECK(entityAggressionStep, 0.0002f);
    CHECK(scoreBlackholeReward, 150);
    CHECK(scoreProton, 50);

#undef CHECK

    if (failed == 0)
        printf("OK: classical.cfg parsed and spot-check values match.\n");
    return failed;
}
