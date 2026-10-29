package rage.so2.ui;

/** Small animation helpers (exponential approach, easing). */
public final class Anim {
    private Anim() {}

    /** Moves `cur` towards `target`; rate ~ 12 feels snappy. */
    public static float approach(float cur, float target, float rate, float dt) {
        float k = 1f - (float) Math.exp(-rate * dt);
        float v = cur + (target - cur) * k;
        return Math.abs(v - target) < 0.0015f ? target : v;
    }

    public static float easeOutCubic(float t) { t = clamp01(t); float u = 1 - t; return 1 - u * u * u; }
    public static float easeOutBack(float t) { t = clamp01(t); float c1 = 1.70158f, c3 = c1 + 1; float u = t - 1; return 1 + c3 * u * u * u + c1 * u * u; }
    public static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
    public static float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
}
