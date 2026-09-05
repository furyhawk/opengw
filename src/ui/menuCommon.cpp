#include "ui/menuCommon.hpp"

#include "core/game.hpp"
#include "core/controls.hpp"
#include "render/gl3.h"

namespace menuCommon {

void reset(State& s)
{
    s = State {};
}

Point3d combinedStick()
{
    Point3d v;
    for (int p = 0; p < 4; ++p) {
        v += theGame->mControls->getLeftStick(p) + theGame->mControls->getRightStick(p);
    }
    return v;
}

bool anyFire()
{
    for (int p = 0; p < 4; ++p) {
        if (theGame->mControls->getTriggerButton(p))
            return true;
    }
    return false;
}

bool anyStart()
{
    for (int p = 0; p < 4; ++p) {
        if (theGame->mControls->getStartButton(p))
            return true;
    }
    return false;
}

bool anyBack()
{
    for (int p = 0; p < 4; ++p) {
        if (theGame->mControls->getBackButton(p) || theGame->mControls->getOptionsButton(p))
            return true;
    }
    return false;
}

bool anyPause()
{
    for (int p = 0; p < 4; ++p) {
        if (theGame->mControls->getPauseButton(p))
            return true;
    }
    return false;
}

bool anyConfirm()
{
    return anyFire() || anyStart();
}

int pressedStartPlayer()
{
    for (int p = 0; p < 4; ++p) {
        if (theGame->mControls->getStartButton(p))
            return p;
    }
    return -1;
}

int vDir()
{
    const Point3d stick = combinedStick();
    if (stick.x > 0.5f)
        return -1; // up
    if (stick.x < -0.5f)
        return 1; // down
    return 0;
}

int hDir()
{
    const Point3d stick = combinedStick();
    if (stick.y > 0.5f)
        return -1; // left
    if (stick.y < -0.5f)
        return 1; // right
    return 0;
}

bool updateVertical(State& s, int count)
{
    const int v = vDir();

    bool moved = false;
    if (v != 0) {
        if (v != s.prevV) {
            moved = true;
            s.vHold = 0;
        } else if (++s.vHold > 20) {
            moved = true;
            s.vHold = 18;
        }
    } else {
        s.vHold = 0;
    }

    if (moved) {
        s.selection += (v > 0) ? 1 : -1;
        if (s.selection < 0)
            s.selection = count - 1;
        if (s.selection >= count)
            s.selection = 0;
    }
    return moved;
}

int updateHorizontal(State& s)
{
    const int h = hDir();

    bool changed = false;
    if (h != 0) {
        if (h != s.prevH) {
            changed = true;
            s.hHold = 0;
        } else if (++s.hHold > 20) {
            changed = true;
            s.hHold = 18;
        }
    } else {
        s.hHold = 0;
    }

    return changed ? h : 0;
}

bool confirmEdge(const State& s)
{
    return anyConfirm() && !s.prevConfirm;
}

bool backEdge(const State& s)
{
    return anyBack() && !s.prevBack;
}

bool pauseEdge(const State& s)
{
    return anyPause() && !s.prevPause;
}

void snapshot(State& s)
{
    s.prevV = vDir();
    s.prevH = hDir();
    s.prevConfirm = anyConfirm();
    s.prevBack = anyBack();
    s.prevPause = anyPause();
}

void drawScrim(float top, float bottom)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.82f);
    glBegin(GL_QUADS);
    glVertex2d(-1.0, bottom);
    glVertex2d(1.0, bottom);
    glVertex2d(1.0, top);
    glVertex2d(-1.0, top);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
}

} // namespace menuCommon
