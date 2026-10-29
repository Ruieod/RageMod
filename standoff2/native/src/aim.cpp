#include "aim.h"

#include <algorithm>
#include <cmath>

namespace aim {
namespace {
float clampMag(float g, float lo, float hi) {
    float a = std::fabs(g);
    if (a < lo) a = lo;
    if (a > hi) a = hi;
    return g < 0 ? -a : a;
}
float len2(Vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }
}  // namespace

void Controller::reset() { *this = Controller(); }

void Controller::estimate(const In& in) {
    const float d = std::pow(0.985f, in.dt * 60.f);
    for (auto& a : acc_) { a.nx *= d; a.dx *= d; a.ny *= d; a.dy *= d; a.sx *= d; a.sy *= d; }
    if (in.autoTune && !in.userLooking) {
        for (int l = 0; l < 4; l++) {
            const Vec2 c = hist_[l];
            LagAcc& a = acc_[l];
            if (std::fabs(c.x) > 1.5f) { a.nx += -in.camShift.x * c.x; a.dx += double(c.x) * c.x; a.sx += double(in.camShift.x) * in.camShift.x; }
            if (std::fabs(c.y) > 1.5f) { a.ny += -in.camShift.y * c.y; a.dy += double(c.y) * c.y; a.sy += double(in.camShift.y) * in.camShift.y; }
        }
        auto score = [&](const LagAcc& a) {
            double s = 0;
            if (a.dx > 150 && a.sx > 1e-3) s += a.nx * a.nx / (a.dx * a.sx);
            if (a.dy > 150 && a.sy > 1e-3) s += a.ny * a.ny / (a.dy * a.sy);
            return s;
        };
        int best = lag_;
        double bs = score(acc_[lag_]) * 1.15 + 1e-6;
        for (int l = 0; l < 4; l++) {
            const double sc = score(acc_[l]);
            if (sc > bs) { bs = sc; best = l; }
        }
        lag_ = best;
    }
    const LagAcc& a = acc_[lag_];
    if (a.dx > 150.0) { gx_ = clampMag(float(a.nx / a.dx), 0.05f, 25.f); haveX_ = true; samples_++; }
    else if (!haveX_) gx_ = in.sens;
    if (a.dy > 150.0) { gy_ = clampMag(float(a.ny / a.dy), 0.05f, 25.f); haveY_ = true; }
    else if (!haveY_) gy_ = in.sens;
}

// camera rotation (in view px) that is already commanded but not visible in the measurement yet
Vec2 Controller::inflight() const {
    Vec2 s{0, 0};
    for (int i = 0; i < lag_ && i < 4; i++) { s.x += hist_[i].x * gx_; s.y += hist_[i].y * gy_; }
    return s;
}

Vec2 Controller::stepToward(Vec2 err, float alpha60, float dt, float maxStep) const {
    const float a = 1.f - std::pow(1.f - alpha60, dt * 60.f);
    Vec2 v{err.x * a, err.y * a};
    const float l = len2(v);
    if (l > maxStep && l > 0) { v.x *= maxStep / l; v.y *= maxStep / l; }
    return v;
}

Out Controller::update(const In& in0) {
    In in = in0;
    in.dt = clampf(in.dt, 0.004f, 0.1f);
    const float dt = in.dt;
    estimate(in);

    Out o;
    Vec2 view{0, 0};
    bool engaged = false;
    cooldown_ = std::max(0.f, cooldown_ - dt);

    const bool aimOn = in.aimEnabled && in.aimAllowed && in.haveTarget && in.inFov;
    const Vec2 fl = inflight();
    const Vec2 errP{in.err.x - fl.x, in.err.y - fl.y};                                  // where the target will be once in-flight input lands
    const Vec2 retP{in.returnErr.x - fl.x, in.returnErr.y - fl.y};

    if (in.mode == 2) {   // silent flick: acquire -> shoot -> return to where the player was looking
        switch (phase_) {
            case IDLE:
                if (aimOn && cooldown_ <= 0) { phase_ = ACQUIRE; phaseT_ = 0; }
                break;
            case ACQUIRE:
                phaseT_ += dt;
                if (!aimOn) { phase_ = RETURN; phaseT_ = 0; break; }
                view = stepToward(errP, 0.65f, dt, in.maxStepPx);
                engaged = true;
                if (len2(in.err) < std::max(3.f, in.tolPx * 0.8f) || phaseT_ > 0.30f) { phase_ = FIRE; phaseT_ = 0; }
                break;
            case FIRE: {
                phaseT_ += dt;
                if (!aimOn) { phase_ = RETURN; phaseT_ = 0; break; }
                view = stepToward(errP, 0.75f, dt, in.maxStepPx);
                engaged = true;
                float hold = 0.14f;
                if (in.afEnabled && in.afMode == 1) hold = (in.afTapMs + 20.f) / 1000.f;
                if (phaseT_ > hold && !(in.userFiring && !in.afEnabled)) { phase_ = RETURN; phaseT_ = 0; }
                break;
            }
            case RETURN:
                phaseT_ += dt;
                if (in.haveReturn) view = stepToward(retP, 0.6f, dt, in.maxStepPx);
                engaged = true;
                if (!in.haveReturn || len2(in.returnErr) < 3.f || phaseT_ > 0.40f) { phase_ = IDLE; cooldown_ = 0.08f; }
                break;
        }
    } else {
        phase_ = IDLE;
        if (aimOn) {
            const float alpha60 = in.mode == 1 ? 0.6f : (0.55f + (0.035f - 0.55f) * clampf(in.smooth, 0.f, 1.f));
            if (len2(errP) > 1.0f) view = stepToward(errP, alpha60, dt, in.maxStepPx);
            engaged = true;
        }
    }

    // ---- autofire ----
    const bool aligned = in.afAligned;
    const bool wantFire = in.afEnabled && aligned && (in.mode != 2 || phase_ == FIRE);
    armedT_ = wantFire ? armedT_ + dt : 0.f;
    const bool armed = wantFire && armedT_ * 1000.f >= in.afDelayMs;
    bool fire = false;
    if (in.afMode == 0) {
        fire = armed;
        tapT_ = in.afIntervalMs;
        tapDown_ = false;
    } else if (armed) {
        tapT_ += dt * 1000.f;
        if (tapDown_) {
            if (tapT_ >= in.afTapMs) { tapDown_ = false; tapT_ = 0; }
        } else if (tapT_ >= in.afIntervalMs) {
            tapDown_ = true; tapT_ = 0;
        }
        fire = tapDown_;
    } else {
        tapDown_ = false;
        tapT_ = in.afIntervalMs;   // first tap after arming is immediate
    }

    // ---- recoil control: cancel a fraction of the measured kick ----
    bool rcsActive = false;
    if (in.rcsEnabled && !engaged && (in.userFiring || fire)) {
        const float ky = in.camShift.y > 0 ? in.camShift.y : 0.f;
        float vy = ky * in.rcsY;
        float vx = in.userLooking ? 0.f : in.camShift.x * in.rcsX;
        const float cap = std::max(1.f, in.rcsGainPx);
        vy = clampf(vy, 0.f, cap);
        vx = clampf(vx, -cap, cap);
        if (vx != 0 || vy != 0) { view.x += vx; view.y += vy; rcsActive = true; }
    }

    o.finger.x = view.x / gx_;
    o.finger.y = view.y / gy_;
    o.finger.x = clampf(o.finger.x, -in.maxStepPx, in.maxStepPx);
    o.finger.y = clampf(o.finger.y, -in.maxStepPx, in.maxStepPx);

    const bool moving = std::fabs(o.finger.x) > 0.05f || std::fabs(o.finger.y) > 0.05f;
    lookHold_ = (engaged || rcsActive || moving) ? 0.15f : std::max(0.f, lookHold_ - dt);
    o.lookActive = lookHold_ > 0.f;
    o.fireHeld = fire;
    o.engaged = engaged;
    o.gain = {gx_, gy_};

    for (int i = 3; i > 0; i--) hist_[i] = hist_[i - 1];
    hist_[0] = o.finger;
    return o;
}

}  // namespace aim
