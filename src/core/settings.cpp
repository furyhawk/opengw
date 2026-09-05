#include "core/settings.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

static std::unique_ptr<settings> instance;

const settings& settings::get()
{
    if (!instance) {
        instance = std::make_unique<settings>();
    }
    return *instance;
}

settings& settings::edit()
{
    if (!instance) {
        instance = std::make_unique<settings>();
    }
    return *instance;
}

settings::settings()
{
    mCreditsPerGame = false; // false = Freeplay

    // --- Audio defaults ---------------------------------------------
    mSoundVolume = 100;

    // --- Graphics defaults --------------------------------------------
    mEnableGlow = true; // GPU bloom
    mFullscreen = false;
    mVsync = true; // vsync on by default

    mGridSmoothing = false;
    mParticleSmoothing = false;
    mEnemySmoothing = false;
    mPlayerSmoothing = true;
    mStarSmoothing = true;

    mAttractors = 50;
    mParticles = 3000;

    mGridPasses = 4;

    displayWidth = 800;
    displayHeight = 600;

    // Overwrite the defaults with any saved settings.cfg.
    load();
}

namespace {

bool parseBool(const std::string& value)
{
    return (value == "1") || (value == "true") || (value == "on");
}

void trim(std::string& s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r'))
        s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.pop_back();
}

} // namespace

void settings::load()
{
    std::ifstream in("settings.cfg");
    if (!in.is_open())
        return;

    std::string line;
    while (std::getline(in, line)) {
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        trim(key);
        trim(value);

        if (key == "glow")
            mEnableGlow = parseBool(value);
        else if (key == "fullscreen")
            mFullscreen = parseBool(value);
        else if (key == "vsync")
            mVsync = parseBool(value);
        else if (key == "gridSmoothing")
            mGridSmoothing = parseBool(value);
        else if (key == "particleSmoothing")
            mParticleSmoothing = parseBool(value);
        else if (key == "enemySmoothing")
            mEnemySmoothing = parseBool(value);
        else if (key == "playerSmoothing")
            mPlayerSmoothing = parseBool(value);
        else if (key == "starSmoothing")
            mStarSmoothing = parseBool(value);
        else if (key == "volume")
            mSoundVolume = std::atoi(value.c_str());
        else if (key == "width")
            displayWidth = std::atoi(value.c_str());
        else if (key == "height")
            displayHeight = std::atoi(value.c_str());
    }

    // Sanity check the resolution.
    if (displayWidth < 640 || displayWidth > 7680)
        displayWidth = 800;
    if (displayHeight < 480 || displayHeight > 4320)
        displayHeight = 600;

    // Sanity check the master volume.
    if (mSoundVolume < 0 || mSoundVolume > 100)
        mSoundVolume = 100;
}

void settings::save() const
{
    std::ofstream out("settings.cfg");
    if (!out.is_open()) {
        printf("settings: could not write settings.cfg\n");
        return;
    }

    out << "glow=" << (mEnableGlow ? 1 : 0) << "\n";
    out << "fullscreen=" << (mFullscreen ? 1 : 0) << "\n";
    out << "vsync=" << (mVsync ? 1 : 0) << "\n";
    out << "gridSmoothing=" << (mGridSmoothing ? 1 : 0) << "\n";
    out << "particleSmoothing=" << (mParticleSmoothing ? 1 : 0) << "\n";
    out << "enemySmoothing=" << (mEnemySmoothing ? 1 : 0) << "\n";
    out << "playerSmoothing=" << (mPlayerSmoothing ? 1 : 0) << "\n";
    out << "starSmoothing=" << (mStarSmoothing ? 1 : 0) << "\n";
    out << "volume=" << mSoundVolume << "\n";
    out << "width=" << displayWidth << "\n";
    out << "height=" << displayHeight << "\n";
}
