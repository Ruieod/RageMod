#pragma once
// Pure-logic aim / autofire / recoil controller. No engine calls, no Android - builds and is tested on the host.
//
// The whole thing is a closed loop on *screen pixels*: the game is only ever steered by the (synthetic) finger the
// Java side puts on the look area, so it works for any sensitivity / acceleration / inverted-Y once the response
// gain (view px per finger px) is known. That gain is estimated online from the commanded finger motion and the
// measured camera rotation.
#include "common.h"

namespace aim {

struct In {
    float dt = 1.f / 60.f;
    bool haveTarget = false;
    Vec2 err;                     // target - crosshair, px (x right, y down)
    float tolPx = 12.f;           // alignment threshold used by the silent-flick phases, px
    bool inFov = false;           // target inside the aim FOV circle
    bool aimEnabled = false;
    bool aimAllowed = true;       // trigger rule satisfied (always / while I fire)
    int mode = 0;                 // 0 smooth, 1 snap, 2 silent flick
    float smooth = 0.35f;         // 0..1 (1 = slowest)
    bool afEnabled = false;
    bool afAligned = false;       // crosshair is on an allowed enemy hitbox (visible / team rules already applied)
    int afMode = 0;               // 0 hold, 1 tap
    float afDelayMs = 0, afTapMs = 45, afIntervalMs = 60;
    bool userFiring = false;
    bool userLooking = false;     // a real finger is on the look area
    Vec2 camShift;                // px the world moved on screen since the previous tick (camera rotation)
    bool rcsEnabled = false;
    float rcsX = 0, rcsY = 0.5f;  // 0..1 fraction of the measured kick that is cancelled
    float rcsGainPx = 14.f;
    float sens = 1.0f;            // initial gain guess (view px per finger px)
    bool autoTune = true;
    bool haveReturn = false;      // silent flick: saved forward is available
    Vec2 returnErr;               // (saved forward point) - crosshair, px
    float maxStepPx = 400.f;
};

struct Out {
    bool lookActive = false;      // keep the look finger down
    Vec2 finger;                  // finger delta to apply this tick, px
    bool fireHeld = false;        // keep the fire finger down
    bool engaged = false;         // for the HUD
    Vec2 gain;                    // current response gain estimate
};

class Controller {
public:
    Out update(const In& in);
    void reset();
    Vec2 gain() const { return {gx_, gy_}; }
    int samples() const { return samples_; }
    int lag() const { return lag_; }

private:
    enum Phase { IDLE, ACQUIRE, FIRE, RETURN };
    void estimate(const In& in);
    Vec2 stepToward(Vec2 err, float alpha60, float dt, float maxStep) const;

    // finger deltas commanded 1..4 ticks ago; the input -> camera latency is learned (lag_) and used as a Smith predictor
    struct LagAcc { double nx = 0, dx = 0, ny = 0, dy = 0, sx = 0, sy = 0; };
    Vec2 hist_[4];
    LagAcc acc_[4];
    int lag_ = 2;
    float gx_ = 1.f, gy_ = 1.f;
    int samples_ = 0;
    Vec2 inflight() const;
    bool haveX_ = false, haveY_ = false;
    float lookHold_ = 0;
    Phase phase_ = IDLE;
    float phaseT_ = 0;
    float armedT_ = 0;       // time target has been inside tolerance
    float tapT_ = 0;         // tap state machine clock
    bool tapDown_ = false;
    float cooldown_ = 0;
};

}  // namespace aim
