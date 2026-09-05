#include "core/classicalparams.hpp"

#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

static std::unique_ptr<classical_params> instance;

const classical_params& classical_params::get()
{
    if (!instance) {
        instance = std::make_unique<classical_params>();
        instance->load();
    }
    return *instance;
}

namespace {

void trim(std::string& s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r'))
        s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.pop_back();
}

} // namespace

bool classical_params::load()
{
    std::ifstream in("classical.cfg");
    if (!in.is_open())
        return false;

    std::string line;
    while (std::getline(in, line)) {
        // Strip comments and blank lines.
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos)
            line.erase(hash);
        trim(line);
        if (line.empty())
            continue;

        const std::size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        trim(key);
        trim(value);

        if (key == "match.brightnessStart")
            matchBrightnessStart = std::atoi(value.c_str());
        else if (key == "match.brightnessRamp")
            matchBrightnessRamp = std::atof(value.c_str());
        else if (key == "match.musicNormalSpeed")
            matchMusicNormalSpeed = std::atof(value.c_str());
        else if (key == "match.musicRespawnSpeed")
            matchMusicRespawnSpeed = std::atof(value.c_str());
        else if (key == "match.musicSpeedUpStep")
            matchMusicSpeedUpStep = std::atof(value.c_str());
        else if (key == "match.musicSpeedDownStep")
            matchMusicSpeedDownStep = std::atof(value.c_str());
        else if (key == "match.coopLives")
            matchCoopLives = std::atoi(value.c_str());
        else if (key == "match.coopBombs")
            matchCoopBombs = std::atoi(value.c_str());

        else if (key == "player.startLives")
            playerStartLives = std::atoi(value.c_str());
        else if (key == "player.startBombs")
            playerStartBombs = std::atoi(value.c_str());
        else if (key == "player.spawnTime")
            playerSpawnTime = std::atoi(value.c_str());
        else if (key == "player.destroyTime")
            playerDestroyTime = std::atoi(value.c_str());
        else if (key == "player.shieldTime")
            playerShieldTime = std::atoi(value.c_str());
        else if (key == "player.shieldWarnTime")
            playerShieldWarnTime = std::atoi(value.c_str());
        else if (key == "player.moveSpeed")
            playerMoveSpeed = std::atof(value.c_str());
        else if (key == "player.moveTurnRate")
            playerMoveTurnRate = std::atof(value.c_str());
        else if (key == "player.stickDeadZone")
            playerStickDeadZone = std::atof(value.c_str());
        else if (key == "player.stickFullZone")
            playerStickFullZone = std::atof(value.c_str());
        else if (key == "player.bombCooldown")
            playerBombCooldown = std::atoi(value.c_str());
        else if (key == "player.weaponChangeScore")
            playerWeaponChangeScore = std::atoi(value.c_str());
        else if (key == "player.extraLifeScore")
            playerExtraLifeScore = std::atoi(value.c_str());
        else if (key == "player.extraBombScore")
            playerExtraBombScore = std::atoi(value.c_str());
        else if (key == "player.multiplierKillCount")
            playerMultiplierKillCount = std::atoi(value.c_str());
        else if (key == "player.multiplierMax")
            playerMultiplierMax = std::atoi(value.c_str());
        else if (key == "player.weapon1Chance")
            playerWeapon1Chance = std::atoi(value.c_str());

        else if (key == "weapon0.interval")
            weapon0Interval = std::atoi(value.c_str());
        else if (key == "weapon0.missileSpeed")
            weapon0MissileSpeed = std::atof(value.c_str());
        else if (key == "weapon0.spread")
            weapon0Spread = std::atof(value.c_str());
        else if (key == "weapon0.inheritSpeed")
            weapon0InheritSpeed = std::atof(value.c_str());
        else if (key == "weapon1.intervalA")
            weapon1IntervalA = std::atoi(value.c_str());
        else if (key == "weapon1.intervalB")
            weapon1IntervalB = std::atoi(value.c_str());
        else if (key == "weapon1.missileSpeed")
            weapon1MissileSpeed = std::atof(value.c_str());
        else if (key == "weapon1.spread")
            weapon1Spread = std::atof(value.c_str());
        else if (key == "weapon1.inheritSpeed")
            weapon1InheritSpeed = std::atof(value.c_str());
        else if (key == "weapon2.interval")
            weapon2Interval = std::atoi(value.c_str());
        else if (key == "weapon2.missileSpeed")
            weapon2MissileSpeed = std::atof(value.c_str());
        else if (key == "weapon2.startInner")
            weapon2StartInner = std::atof(value.c_str());
        else if (key == "weapon2.startOuter")
            weapon2StartOuter = std::atof(value.c_str());
        else if (key == "weapon2.spreadInner")
            weapon2SpreadInner = std::atof(value.c_str());
        else if (key == "weapon2.spreadOuter")
            weapon2SpreadOuter = std::atof(value.c_str());
        else if (key == "weapon2.inheritSpeed")
            weapon2InheritSpeed = std::atof(value.c_str());

        else if (key == "bomb.maxRings")
            bombMaxRings = std::atoi(value.c_str());
        else if (key == "bomb.ringRadius")
            bombRingRadius = std::atof(value.c_str());
        else if (key == "bomb.ringThickness")
            bombRingThickness = std::atof(value.c_str());
        else if (key == "bomb.ringSpeed")
            bombRingSpeed = std::atof(value.c_str());
        else if (key == "bomb.ringTimeToLive")
            bombRingTimeToLive = std::atoi(value.c_str());
        else if (key == "bomb.ringMaxRadius")
            bombRingMaxRadius = std::atof(value.c_str());
        else if (key == "bomb.ringKillBand")
            bombRingKillBand = std::atof(value.c_str());
        else if (key == "bomb.ringThicknessStep")
            bombRingThicknessStep = std::atof(value.c_str());

        else if (key == "spawner.maxIndex")
            spawnerMaxIndex = std::atoi(value.c_str());
        else if (key == "spawner.indexRate")
            spawnerIndexRate = std::atof(value.c_str());
        else if (key == "spawner.respawnWaitTimer")
            spawnerRespawnWaitTimer = std::atoi(value.c_str());
        else if (key == "spawner.respawnDelay")
            spawnerRespawnDelay = std::atoi(value.c_str());
        else if (key == "spawner.scatterInterval")
            spawnerScatterInterval = std::atoi(value.c_str());
        else if (key == "spawner.scatterEveryFrameIndex")
            spawnerScatterEveryFrameIndex = std::atoi(value.c_str());
        else if (key == "spawner.waveCadence")
            spawnerWaveCadence = std::atoi(value.c_str());
        else if (key == "spawner.waveChoices")
            spawnerWaveChoices = std::atoi(value.c_str());
        else if (key == "spawner.waveCap1Index")
            spawnerWaveCap1Index = std::atoi(value.c_str());
        else if (key == "spawner.waveCap2Index")
            spawnerWaveCap2Index = std::atoi(value.c_str());
        else if (key == "spawner.waveUnlimitedIndex")
            spawnerWaveUnlimitedIndex = std::atoi(value.c_str());
        else if (key == "spawner.blackholeChance")
            spawnerBlackholeChance = std::atoi(value.c_str());
        else if (key == "spawner.blackholeIndex")
            spawnerBlackholeIndex = std::atoi(value.c_str());
        else if (key == "spawner.wandererEarly")
            spawnerWandererEarly = std::atoi(value.c_str());
        else if (key == "spawner.wandererMid")
            spawnerWandererMid = std::atoi(value.c_str());
        else if (key == "spawner.gruntEarly")
            spawnerGruntEarly = std::atoi(value.c_str());
        else if (key == "spawner.gruntLate")
            spawnerGruntLate = std::atoi(value.c_str());
        else if (key == "spawner.spinnerScatter")
            spawnerSpinnerScatter = std::atoi(value.c_str());
        else if (key == "spawner.weaverScatter")
            spawnerWeaverScatter = std::atoi(value.c_str());
        else if (key == "spawner.scatterMargin")
            spawnerScatterMargin = std::atof(value.c_str());
        else if (key == "spawner.waveMargin")
            spawnerWaveMargin = std::atof(value.c_str());
        else if (key == "spawner.rushJitter")
            spawnerRushJitter = std::atof(value.c_str());
        else if (key == "spawner.rushRadiusPlayer")
            spawnerRushRadiusPlayer = std::atof(value.c_str());
        else if (key == "spawner.rushRadiusBlackhole")
            spawnerRushRadiusBlackhole = std::atof(value.c_str());
        else if (key == "spawner.swarmJitter")
            spawnerSwarmJitter = std::atof(value.c_str());
        else if (key == "spawner.swarmWaveCadence")
            spawnerSwarmWaveCadence = std::atoi(value.c_str());

        else if (key == "wave.popGrunt")
            wavePopGrunt = std::atoi(value.c_str());
        else if (key == "wave.popWeaver")
            wavePopWeaver = std::atoi(value.c_str());
        else if (key == "wave.popSnake")
            wavePopSnake = std::atoi(value.c_str());
        else if (key == "wave.popSpinner")
            wavePopSpinner = std::atoi(value.c_str());
        else if (key == "wave.popBlackhole")
            wavePopBlackhole = std::atoi(value.c_str());
        else if (key == "wave.popMayfly")
            wavePopMayfly = std::atoi(value.c_str());
        else if (key == "wave.popRepulsor")
            wavePopRepulsor = std::atoi(value.c_str());
        else if (key == "wave.minGrunt")
            waveMinGrunt = std::atoi(value.c_str());
        else if (key == "wave.minWeaver")
            waveMinWeaver = std::atoi(value.c_str());
        else if (key == "wave.minSnake")
            waveMinSnake = std::atoi(value.c_str());
        else if (key == "wave.minSpinner")
            waveMinSpinner = std::atoi(value.c_str());
        else if (key == "wave.minBlackhole")
            waveMinBlackhole = std::atoi(value.c_str());
        else if (key == "wave.minMayfly")
            waveMinMayfly = std::atoi(value.c_str());
        else if (key == "wave.snakeIndex")
            waveSnakeIndex = std::atoi(value.c_str());
        else if (key == "wave.blackholeIndex")
            waveBlackholeIndex = std::atoi(value.c_str());
        else if (key == "wave.mayflyIndex")
            waveMayflyIndex = std::atoi(value.c_str());
        else if (key == "wave.repulsorIndex")
            waveRepulsorIndex = std::atoi(value.c_str());
        else if (key == "wave.rushDivide")
            waveRushDivide = std::atoi(value.c_str());

        else if (key == "entity.aggressionStep")
            entityAggressionStep = std::atof(value.c_str());
        else if (key == "entity.spawnTime")
            entitySpawnTime = std::atoi(value.c_str());
        else if (key == "entity.destroyTime")
            entityDestroyTime = std::atoi(value.c_str());
        else if (key == "entity.indicateTime")
            entityIndicateTime = std::atoi(value.c_str());

        else if (key == "score.grunt")
            scoreGrunt = std::atoi(value.c_str());
        else if (key == "score.wanderer")
            scoreWanderer = std::atoi(value.c_str());
        else if (key == "score.weaver")
            scoreWeaver = std::atoi(value.c_str());
        else if (key == "score.spinner")
            scoreSpinner = std::atoi(value.c_str());
        else if (key == "score.tinySpinner")
            scoreTinySpinner = std::atoi(value.c_str());
        else if (key == "score.snake")
            scoreSnake = std::atoi(value.c_str());
        else if (key == "score.snakeSegment")
            scoreSnakeSegment = std::atoi(value.c_str());
        else if (key == "score.mayfly")
            scoreMayfly = std::atoi(value.c_str());
        else if (key == "score.repulsor")
            scoreRepulsor = std::atoi(value.c_str());
        else if (key == "score.blackhole")
            scoreBlackhole = std::atoi(value.c_str());
        else if (key == "score.blackholeReward")
            scoreBlackholeReward = std::atoi(value.c_str());
        else if (key == "score.proton")
            scoreProton = std::atoi(value.c_str());
    }

    return true;
}
