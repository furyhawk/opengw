#include "ui/menuAchievements.hpp"

#include "core/achievements.hpp"
#include "core/game.hpp"
#include "core/settings.hpp"
#include "math/vector.hpp"
#include "render/font.hpp"
#include "render/gl3.h"
#include "render/texture.hpp"
#include "ui/menuCommon.hpp"
#include "ui/menuMain.hpp"

#include <cstdio>

namespace menuAchievements {

namespace {

menuCommon::State sState;

// One texture per badge, loaded lazily (PNG, generated from the neon hex SVGs).
texture sBadgeTextures[achievements::BADGE_COUNT];
bool sBadgeTexturesAttempted = false;
bool sBadgeTexturesLoaded[achievements::BADGE_COUNT] = { false };

void ensureBadgeTextures()
{
    if (sBadgeTexturesAttempted)
        return;

    for (int i = 0; i < achievements::BADGE_COUNT; ++i) {
        sBadgeTextures[i].load(achievements::badge(i).icon);
        sBadgeTexturesLoaded[i] = sBadgeTextures[i].mLoaded;
    }

    sBadgeTexturesAttempted = true;
}

void drawBadge(int index, float x, float y, float size)
{
    const bool unlocked = achievements::badgeUnlocked(index);
    vector::pen pen = unlocked ? vector::pen(1.0f, 1.0f, 1.0f, 1.0f, 3)
                               : vector::pen(0.45f, 0.5f, 0.6f, 0.35f, 3);

    if (sBadgeTexturesLoaded[index]) {
        sBadgeTextures[index].draw(x, y, size, size);
        if (!unlocked) {
            // Dim locked badges so the grid still reads at a glance.
            font::AlphanumericsPrint(font::ALIGN_CENTER, 0.012f, x, y, &pen, "-");
        }
        return;
    }

    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.012f, x, y, &pen, "%s", unlocked ? "O" : "-");
}

void drawTrack(achievements::Track track, float centerX, float headerY)
{
    vector::pen headerPen(0.55f, 0.85f, 1.0f, 0.85f, 2);
    vector::pen valuePen(0.9f, 1.0f, 0.95f, 0.75f, 2);

    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.014f, centerX, headerY, &headerPen, "%s", achievements::trackLabel(track));

    // Collect the badges that belong to this track and lay them out in a row.
    constexpr float kBadgeSize = 0.075f;
    constexpr float kBadgeGap = 0.085f;
    const float badgeY = headerY - 0.11f;

    int inTrack = 0;
    for (int i = 0; i < achievements::BADGE_COUNT; ++i) {
        if (achievements::badge(i).track != track)
            continue;
        const float x = centerX + ((inTrack - 0.5f) * kBadgeGap);
        drawBadge(i, x, badgeY, kBadgeSize);
        ++inTrack;
    }

    char progress[48];
    snprintf(progress, sizeof(progress), "%d / %d", achievements::trackValue(track), achievements::trackGoal(track));
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.012f, centerX, badgeY - 0.075f, &valuePen, "%s", progress);
}

} // namespace

void init()
{
    menuCommon::reset(sState);
}

void run()
{
    // Swallow the button that opened this screen.
    if (!sState.ready) {
        if (!menuCommon::anyConfirm() && !menuCommon::anyBack() && !menuCommon::anyPause()
            && menuCommon::vDir() == 0 && menuCommon::hDir() == 0) {
            sState.ready = true;
        }
        menuCommon::snapshot(sState);
        return;
    }

    // Left/Right equips the next owned cosmetic.
    const int hDir = menuCommon::updateHorizontal(sState);
    if (hDir != 0) {
        achievements::cycleCosmetic(hDir);
        settings::edit().save();
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
    }

    if (menuCommon::confirmEdge(sState) || menuCommon::backEdge(sState) || menuCommon::pauseEdge(sState)) {
        theGame->mSound->playTrack(SOUNDID_MENU_SELECT);
        menuMain::init();
        game::mMenuScreen = game::MENU_TITLE;
        menuCommon::snapshot(sState);
        return;
    }

    menuCommon::snapshot(sState);
}

void draw()
{
    ensureBadgeTextures();

    menuCommon::drawScrim(0.95f, -0.95f);

    vector::pen titlePen(0.75f, 0.95f, 1.0f, 0.95f, 3);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.026f, 0.0f, 0.86f, &titlePen, "ACHIEVEMENTS");

    vector::pen subPen(0.55f, 0.7f, 0.85f, 0.55f, 2);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.011f, 0.0f, 0.78f, &subPen,
                             "LIFETIME RANK %d   BADGES %d/%d",
                             achievements::lifetimeRank(),
                             achievements::badgesUnlocked(),
                             achievements::badgeCount());

    // Three track columns.
    drawTrack(achievements::TRACK_MODE, -0.62f, 0.60f);
    drawTrack(achievements::TRACK_PERFORMANCE, 0.0f, 0.60f);
    drawTrack(achievements::TRACK_LIFELONG, 0.62f, 0.60f);

    // Divider.
    {
        vector::pen linePen(0.25f, 0.55f, 0.75f, 0.5f, 2);
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.011f, 0.0f, 0.06f, &linePen, "-----------------------------");
    }

    // Cosmetic selector.
    const int equipped = achievements::equippedCosmetic();
    const achievements::CosmeticInfo& c = achievements::cosmetic(equipped);

    vector::pen labelPen(0.5f, 0.75f, 1.0f, 0.6f, 2);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.011f, 0.0f, -0.04f, &labelPen, "SHIP COSMETIC");

    vector::pen namePen(c.hullR, c.hullG, c.hullB, 1.0f, 3);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.022f, 0.0f, -0.16f, &namePen, "<  %s  >", c.name);

    char owned[48];
    // Count only owned entries for an accurate readout.
    int ownedCount = 0;
    for (int i = 0; i < achievements::cosmeticCount(); ++i) {
        if (achievements::cosmeticUnlocked(i))
            ++ownedCount;
    }
    snprintf(owned, sizeof(owned), "UNLOCKED %d/%d", ownedCount, achievements::cosmeticCount());
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.011f, 0.0f, -0.26f, &labelPen, "%s", owned);

    const int nextLocked = achievements::nextLockedCosmetic();
    if (nextLocked >= 0) {
        vector::pen lockPen(0.85f, 0.65f, 0.35f, 0.8f, 2);
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.011f, 0.0f, -0.34f, &lockPen,
                                 "NEXT: %s  (%s)",
                                 achievements::cosmetic(nextLocked).name,
                                 achievements::cosmetic(nextLocked).requirement);
    } else {
        vector::pen donePen(0.45f, 1.0f, 0.6f, 0.9f, 2);
        font::AlphanumericsPrint(font::ALIGN_CENTER, 0.011f, 0.0f, -0.34f, &donePen, "ALL COSMETICS UNLOCKED");
    }

    vector::pen footPen(0.6f, 0.8f, 1.0f, 0.5f, 2);
    font::AlphanumericsPrint(font::ALIGN_CENTER, 0.011f, 0.0f, -0.92f, &footPen,
                             "Left Right Equip Cosmetic   Back or Start Return");
}

} // namespace menuAchievements
