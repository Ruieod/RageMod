// host test for aim::Controller: closed loop vs a simulated game (2-tick latency, unknown gain, optional inverted Y)
#include <cassert>
#include <cstdio>
#include <deque>

#include "../native/src/aim.h"

struct Sim {
    float gx, gy;                // true response: world shift = -g * finger
    Vec2 target{0, 0};           // target position relative to crosshair (err), px
    std::deque<Vec2> pipe;       // finger deltas in flight
    Vec2 lastShift;
    void step(Vec2 finger) {
        pipe.push_back(finger);
        Vec2 applied{0, 0};
        if (pipe.size() > 2) { applied = pipe.front(); pipe.pop_front(); }
        lastShift = {-gx * applied.x, -gy * applied.y};
        target.x += lastShift.x;
        target.y += lastShift.y;
    }
};

static int run(const char* name, float gx, float gy, int mode, float smooth, int ticks, float* finalErr, float* estX, float* estY, int* fireTicks, bool af) {
    aim::Controller c;
    Sim s{gx, gy};
    s.target = {260.f, -90.f};
    int fires = 0;
    aim::Out o;
    for (int t = 0; t < ticks; t++) {
        aim::In in;
        in.dt = 1.f / 60.f;
        in.haveTarget = true;
        in.err = s.target;
        in.inFov = true;
        in.aimEnabled = true;
        in.mode = mode;
        in.smooth = smooth;
        in.camShift = s.lastShift;
        in.afEnabled = af;
        in.afAligned = std::sqrt(in.err.x * in.err.x + in.err.y * in.err.y) <= 14.f;
        in.tolPx = 14;
        in.sens = 1.0f;
        in.haveReturn = false;
        o = c.update(in);
        if (o.fireHeld) fires++;
        s.step(o.finger);
    }
    *finalErr = std::sqrt(s.target.x * s.target.x + s.target.y * s.target.y);
    *estX = o.gain.x; *estY = o.gain.y; *fireTicks = fires;
    printf("%-26s final err %6.2f px   gain est (%5.2f, %5.2f)  true (%5.2f, %5.2f)  fire ticks %d\n", name, *finalErr, *estX, *estY, gx, gy, fires);
    return 0;
}

int main() {
    float e, gx, gy; int f;
    run("snap, gain 0.7/0.55", 0.7f, 0.55f, 1, 0.3f, 120, &e, &gx, &gy, &f, true);
    assert(e < 3.f);
    assert(f > 30);
    run("snap, gain 2.4/2.4", 2.4f, 2.4f, 1, 0.3f, 160, &e, &gx, &gy, &f, false);
    assert(e < 4.f);
    run("snap, inverted Y", 0.8f, -0.8f, 1, 0.3f, 200, &e, &gx, &gy, &f, false);
    assert(e < 4.f); assert(gy < 0);
    run("smooth(0.6), gain 1.3", 1.3f, 1.3f, 0, 0.6f, 240, &e, &gx, &gy, &f, false);
    assert(e < 6.f);
    run("snap, gain 0.25 (slow sens)", 0.25f, 0.25f, 1, 0.3f, 300, &e, &gx, &gy, &f, false);
    assert(e < 6.f);

    // tap autofire pacing: aligned from t=0, tap 45ms / interval 60ms -> down 3 ticks, up 4 ticks
    {
        aim::Controller c;
        int downs = 0, edges = 0; bool prev = false;
        for (int t = 0; t < 120; t++) {
            aim::In in; in.haveTarget = true; in.err = {1, 1}; in.inFov = true; in.aimEnabled = false;
            in.afEnabled = true; in.afAligned = std::sqrt(in.err.x * in.err.x + in.err.y * in.err.y) <= 14.f; in.afMode = 1; in.afTapMs = 45; in.afIntervalMs = 60; in.tolPx = 10;
            aim::Out o = c.update(in);
            if (o.fireHeld) downs++;
            if (o.fireHeld && !prev) edges++;
            prev = o.fireHeld;
        }
        printf("tap pacing: %d down ticks, %d presses over 2s\n", downs, edges);
        assert(edges >= 15 && edges <= 22);
    }
    // silent flick returns to the saved view
    {
        aim::Controller c;
        Sim s{0.9f, 0.9f};
        Vec2 ret{0, 0};   // where the original view direction sits relative to the crosshair
        s.target = {180, 40};
        ret = {0, 0};
        bool fired = false;
        for (int t = 0; t < 200; t++) {
            aim::In in; in.haveTarget = (t < 100); in.err = s.target; in.inFov = true; in.aimEnabled = true; in.mode = 2;
            in.afEnabled = true; in.afAligned = std::sqrt(in.err.x * in.err.x + in.err.y * in.err.y) <= 14.f; in.afMode = 0; in.tolPx = 12; in.camShift = s.lastShift;
            in.haveReturn = true; in.returnErr = ret;
            aim::Out o = c.update(in);
            fired |= o.fireHeld;
            s.step(o.finger);
            // the saved-forward point moves exactly like the target does (it is a world point)
            ret.x += s.lastShift.x; ret.y += s.lastShift.y;
            if (t == 99) { s.target = {500, 500}; }
        }
        printf("silent: fired=%d, saved view residual %.2f px\n", fired, std::sqrt(ret.x * ret.x + ret.y * ret.y));
        assert(fired);
        assert(std::sqrt(ret.x * ret.x + ret.y * ret.y) < 6.f);
    }
    puts("aim tests OK");
    return 0;
}
