package rage.so2.ui;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/** Kill effects drawn over the game: burst, rings, lightning, skull + floating text. */
public final class KillFx {
    private static final class Fx {
        float x, y, size;
        int style, color;
        long start;
        long seed;
        String text;
        boolean headshot;
    }

    private final List<Fx> list = new ArrayList<>();
    private float flash;   // screen flash 0..1

    public boolean active() { return !list.isEmpty() || flash > 0.01f; }

    public void spawn(float x, float y, int style, int color, float size, String text, boolean flashScreen, long nowMs) {
        Fx f = new Fx();
        f.x = x; f.y = y; f.style = style; f.color = color | 0xFF000000; f.size = size; f.start = nowMs; f.text = text;
        f.seed = nowMs * 31L + (long) (x * 7 + y * 13);
        list.add(f);
        if (list.size() > 8) list.remove(0);
        if (flashScreen) flash = 1f;
    }

    public void draw(Gfx g, float u, long nowMs, float dt) {
        if (flash > 0.01f) {
            g.fillRect(0, 0, g.width(), g.height(), Theme.withAlpha(list.isEmpty() ? 0xFFFFFFFF : list.get(list.size() - 1).color, 0.20f * flash * flash));
            flash = Anim.approach(flash, 0f, 5f, dt);
        }
        for (int i = list.size() - 1; i >= 0; i--) {
            Fx f = list.get(i);
            float t = (nowMs - f.start) / 1000f;
            float life = f.style == 1 ? 1.1f : 1.35f;
            if (t > life) { list.remove(i); continue; }
            float k = t / life;
            float s = f.size * u;
            Random rnd = new Random(f.seed);
            switch (f.style) {
                case 0: burst(g, f, rnd, t, k, s); break;
                case 1: rings(g, f, t, k, s); break;
                case 2: lightning(g, f, rnd, t, k, s); break;
                default: skull(g, f, t, k, s); break;
            }
            if (f.text != null) text(g, f, t, k, u);
        }
    }

    private void burst(Gfx g, Fx f, Random r, float t, float k, float s) {
        float e = Anim.easeOutCubic(k);
        g.radial(f.x, f.y, 70 * s * (0.4f + e), Theme.withAlpha(f.color, 0.35f * (1 - k)), Theme.withAlpha(f.color, 0f));
        for (int i = 0; i < 26; i++) {
            float a = (float) (r.nextFloat() * Math.PI * 2), sp = 40 + r.nextFloat() * 120, ln = 6 + r.nextFloat() * 22;
            float d0 = sp * s * e * 0.9f, d1 = d0 + ln * s * (1 - k);
            float cs = (float) Math.cos(a), sn = (float) Math.sin(a);
            g.line(f.x + cs * d0, f.y + sn * d0, f.x + cs * d1, f.y + sn * d1, Math.max(1f, 2.6f * s * (1 - k)), Theme.withAlpha(i % 3 == 0 ? 0xFFFFFFFF : f.color, 1 - k));
        }
        for (int i = 0; i < 10; i++) {
            float a = (float) (r.nextFloat() * Math.PI * 2), sp = 20 + r.nextFloat() * 80;
            g.circle(f.x + (float) Math.cos(a) * sp * s * e, f.y + (float) Math.sin(a) * sp * s * e + 30 * s * k * k, (1.2f + r.nextFloat() * 2.4f) * s * (1 - k), Theme.withAlpha(f.color, 1 - k));
        }
        g.ring(f.x, f.y, 14 * s + 56 * s * e, Math.max(1f, 3f * s * (1 - k)), Theme.withAlpha(0xFFFFFFFF, 0.85f * (1 - k)));
    }

    private void rings(Gfx g, Fx f, float t, float k, float s) {
        for (int i = 0; i < 3; i++) {
            float kk = Anim.clamp01((t - i * 0.12f) / 0.9f);
            if (kk <= 0) continue;
            float e = Anim.easeOutCubic(kk);
            g.ring(f.x, f.y, 8 * s + (30 + i * 22) * s * e * 1.8f, Math.max(1f, (5 - i) * s * (1 - kk)), Theme.withAlpha(i == 0 ? 0xFFFFFFFF : f.color, 0.95f * (1 - kk)));
        }
        g.radial(f.x, f.y, 40 * s, Theme.withAlpha(f.color, 0.5f * (1 - k)), Theme.withAlpha(f.color, 0f));
    }

    private void lightning(Gfx g, Fx f, Random r, float t, float k, float s) {
        int flick = (int) (t * 14);
        if ((flick & 1) == 1 && k > 0.2f) return;
        for (int b = 0; b < 7; b++) {
            float a = (float) (b * Math.PI * 2 / 7 + r.nextFloat() * 0.5f);
            float px = f.x, py = f.y;
            int segs = 7;
            float len = (70 + r.nextFloat() * 70) * s * (0.4f + 0.6f * Anim.easeOutCubic(Math.min(1f, k * 3)));
            for (int i = 0; i < segs; i++) {
                float d = len / segs;
                a += (r.nextFloat() - 0.5f) * 1.1f;
                float nx = px + (float) Math.cos(a) * d, ny = py + (float) Math.sin(a) * d;
                float al = (1 - k) * (1f - i / (float) segs * 0.5f);
                g.line(px, py, nx, ny, Math.max(1f, 5.5f * s * (1 - k)), Theme.withAlpha(f.color, 0.35f * al));
                g.line(px, py, nx, ny, Math.max(1f, 2.2f * s * (1 - k)), Theme.withAlpha(0xFFFFFFFF, al));
                px = nx; py = ny;
            }
        }
        g.radial(f.x, f.y, 36 * s, Theme.withAlpha(0xFFFFFFFF, 0.6f * (1 - k)), Theme.withAlpha(f.color, 0f));
    }

    private void skull(Gfx g, Fx f, float t, float k, float s) {
        float rise = 34 * s * Anim.easeOutCubic(k);
        float pop = Anim.easeOutBack(Math.min(1f, t / 0.25f));
        float a = Anim.clamp01(1f - Math.max(0f, (k - 0.55f) / 0.45f));
        float cx = f.x, cy = f.y - rise, sc = s * 26 * pop;
        g.radial(cx, cy, sc * 2.2f, Theme.withAlpha(f.color, 0.45f * a), Theme.withAlpha(f.color, 0f));
        int white = Theme.withAlpha(0xFFF4F7FF, a), dark = Theme.withAlpha(0xFF0A0F17, a);
        g.circle(cx, cy - sc * 0.12f, sc * 0.82f, white);
        g.fillRound(cx - sc * 0.5f, cy + sc * 0.3f, sc, sc * 0.62f, sc * 0.16f, white);
        g.circle(cx - sc * 0.32f, cy - sc * 0.1f, sc * 0.22f, dark);
        g.circle(cx + sc * 0.32f, cy - sc * 0.1f, sc * 0.22f, dark);
        g.poly(new float[]{cx, cy + sc * 0.12f, cx - sc * 0.09f, cy + sc * 0.32f, cx + sc * 0.09f, cy + sc * 0.32f}, 3, dark);
        for (int i = 0; i < 4; i++) g.line(cx - sc * 0.3f + i * sc * 0.2f, cy + sc * 0.62f, cx - sc * 0.3f + i * sc * 0.2f, cy + sc * 0.9f, Math.max(1f, sc * 0.05f), dark);
        g.ring(cx, cy, sc * 1.25f, Math.max(1f, sc * 0.06f), Theme.withAlpha(f.color, 0.8f * a));
    }

    private void text(Gfx g, Fx f, float t, float k, float u) {
        float rise = 46 * u * Anim.easeOutCubic(k) + 30 * u;
        float a = Anim.clamp01(Math.min(t / 0.12f, 1f)) * Anim.clamp01(1f - Math.max(0f, (k - 0.6f) / 0.4f));
        float pop = 0.85f + 0.15f * Anim.easeOutBack(Math.min(1f, t / 0.2f));
        float fs = 17 * u * pop * Math.max(0.8f, f.size * 0.8f);
        float y = f.y - rise;
        g.text(f.text, f.x + 1.5f, y + 1.5f, fs, Theme.withAlpha(0xFF000000, 0.7f * a), Gfx.CENTER, true);
        g.text(f.text, f.x, y, fs, Theme.withAlpha(0xFFFFFFFF, a), Gfx.CENTER, true);
        g.text(f.text, f.x, y, fs, Theme.withAlpha(f.color, 0.45f * a), Gfx.CENTER, true);
    }
}
