package rage.so2.ui;

import java.util.Calendar;
import java.util.Locale;

import rage.so2.gen.Cfg;

/** Always-visible furniture: watermark, floating menu button, calibration markers. */
public final class Hud {
    private final Settings s;
    private final Theme th;
    public String user = "user";

    // floating button
    private boolean btnDrag;
    private float btnDownX, btnDownY, btnStartX, btnStartY;
    private boolean btnMoved;
    private float btnPulse;

    // calibration
    private int calDrag;   // 0 none, 1 fire, 2 look, 3 done
    private float calT;

    public Hud(Settings s, Theme th) { this.s = s; this.th = th; }

    // ---------- watermark ----------
    public void drawWatermark(Gfx g, float u, float fps, int ping) {
        if (!s.on(Cfg.WM_ENABLE)) return;
        boolean compact = s.get(Cfg.WM_STYLE) > 0.5f;
        Calendar c = Calendar.getInstance();
        String time = String.format(Locale.US, "%02d:%02d", c.get(Calendar.HOUR_OF_DAY), c.get(Calendar.MINUTE));
        String[] parts = compact ? new String[]{"ragemod", Math.round(fps) + " fps"} :
                new String[]{"ragemod", user, Math.round(fps) + " fps", ping >= 0 ? ping + " ms" : time, ping >= 0 ? time : null};
        float fs = 10.5f * u, padX = 10 * u, gap = 9 * u, h = 24 * u;
        float logo = h - 6 * u;
        float tw = logo + 8 * u;
        int np = 0;
        for (String p : parts) { if (p == null) continue; tw += g.textW(p, fs, np == 0) + gap * 2 + 1; np++; }
        tw += padX - gap;
        float x = g.width() - tw - 12 * u, y = 10 * u;
        g.shadow(x, y, tw, h, 8 * u, 14 * u, 0x88000000);
        g.fillRound(x, y, tw, h, 8 * u, 0xE60A111B);
        g.strokeRound(x, y, tw, h, 8 * u, 1f, th.stroke);
        // accent underline with glow
        g.gradRound(x + 8 * u, y + h - 1.6f * u, tw - 16 * u, 1.6f * u, 0.8f * u, Theme.withAlpha(th.accent, 0f), th.accent, false);
        g.shadow(x + 14 * u, y + h - 1.4f * u, tw - 28 * u, 1.4f * u, 0.7f * u, 7 * u, Theme.withAlpha(th.accent, 0.45f));
        // logo
        g.gradRound(x + 4 * u, y + 3 * u, logo, logo, 6 * u, Theme.lighten(th.accent, 0.3f), Theme.mix(th.accent, 0xFF000000, 0.35f), true);
        g.text("R", x + 4 * u + logo / 2, y + h / 2 + 0.5f * u, 11 * u, 0xFFFFFFFF, Gfx.CENTER, true);
        float cx = x + 4 * u + logo + 8 * u;
        int idx = 0;
        for (String p : parts) {
            if (p == null) continue;
            boolean first = idx == 0;
            if (!first) { g.line(cx, y + 6 * u, cx, y + h - 6 * u, 1f, 0x33FFFFFF); cx += gap; }
            if (first) {
                g.text("rage", cx, y + h / 2, fs, 0xFFFFFFFF, Gfx.LEFT, true);
                float rw = g.textW("rage", fs, true);
                g.text("mod", cx + rw, y + h / 2, fs, th.accentText, Gfx.LEFT, true);
                cx += g.textW("ragemod", fs, true);
            } else {
                g.text(p, cx, y + h / 2, fs, idx == 2 || idx == 1 && compact ? fpsColor(fps) : 0xFFE6ECF7, Gfx.LEFT, false);
                cx += g.textW(p, fs, false);
            }
            cx += gap;
            idx++;
        }
    }

    private int fpsColor(float fps) { return fps >= 50 ? 0xFFE6ECF7 : (fps >= 30 ? th.warn : th.bad); }

    // ---------- floating button ----------
    public float btnX(float vw) { float f = s.get(Cfg.UI_BX); return Float.isNaN(f) ? 0.03f * vw : f * vw; }
    public float btnY(float vh) { return s.get(Cfg.UI_BY) * vh; }
    private float btnR(float u) { return 17 * u * s.get(Cfg.UI_BTN); }

    public void drawButton(Gfx g, float u, boolean menuOpen, float dt) {
        float vw = g.width(), vh = g.height();
        float cx = btnX(vw) + btnR(u), cy = btnY(vh), r = btnR(u);
        btnPulse = Anim.approach(btnPulse, menuOpen ? 1f : 0f, 12f, dt);
        g.shadow(cx - r, cy - r, r * 2, r * 2, r, 12 * u, Theme.withAlpha(th.accent, 0.40f + 0.2f * btnPulse));
        g.gradRound(cx - r, cy - r, r * 2, r * 2, r, Theme.lighten(th.accent, 0.22f), Theme.mix(th.accent, 0xFF000000, 0.4f), true);
        g.ring(cx, cy, r - 1f, 1.4f, Theme.withAlpha(0xFFFFFFFF, 0.35f));
        g.text("R", cx, cy + 0.5f * u, r * 0.95f, 0xFFFFFFFF, Gfx.CENTER, true);
    }

    public boolean btnHit(float px, float py, float vw, float vh, float u) {
        float cx = btnX(vw) + btnR(u), cy = btnY(vh);
        return Math.hypot(px - cx, py - cy) <= btnR(u) * 1.35f;
    }

    /** @return true when the gesture ended as a tap (toggle the menu) */
    public void btnDown(float px, float py, float vw, float vh, float u) {
        btnDrag = true; btnMoved = false; btnDownX = px; btnDownY = py;
        btnStartX = btnX(vw); btnStartY = btnY(vh);
    }

    public void btnMove(float px, float py, float vw, float vh, float u) {
        if (!btnDrag) return;
        if (Math.hypot(px - btnDownX, py - btnDownY) > 8 * u) btnMoved = true;
        if (btnMoved) {
            float r = btnR(u);
            s.v[Cfg.UI_BX] = Anim.clamp((btnStartX + px - btnDownX) / vw, 0f, (vw - 2 * r) / vw);
            s.v[Cfg.UI_BY] = Anim.clamp((btnStartY + py - btnDownY) / vh, r / vh, 1f - r / vh);
            s.rev++;
        }
    }

    public boolean btnUp() {
        boolean tap = btnDrag && !btnMoved;
        btnDrag = false;
        return tap;
    }

    // ---------- calibration ----------
    public boolean calibrating() { return s.on(Cfg.CAL_ON); }

    public void drawCalibration(Gfx g, float u, float dt) {
        if (!calibrating()) return;
        calT += dt;
        float vw = g.width(), vh = g.height();
        g.fillRect(0, 0, vw, vh, 0x55000000);
        float fx = s.get(Cfg.CAL_FX) * vw, fy = s.get(Cfg.CAL_FY) * vh, lx = s.get(Cfg.CAL_LX) * vw, ly = s.get(Cfg.CAL_LY) * vh;
        // look swipe zone hint
        float zr = Math.min(vw, vh) * 0.22f;
        g.strokeRound(lx - zr, ly - zr * 0.7f, zr * 2, zr * 1.4f, 14 * u, 1.2f, Theme.withAlpha(th.accent, 0.55f));
        marker(g, u, lx, ly, "LOOK", th.accent, calDrag == 2);
        marker(g, u, fx, fy, "FIRE", 0xFFFF5470, calDrag == 1);
        // crosshair
        float cx = vw / 2, cy = vh / 2 + s.get(Cfg.CAL_CROSS);
        g.line(cx - 12 * u, cy, cx + 12 * u, cy, 1.2f, 0xCCFFFFFF);
        g.line(cx, cy - 12 * u, cx, cy + 12 * u, 1.2f, 0xCCFFFFFF);
        // instructions + done
        String msg = "Drag LOOK onto a free spot on the right side (not on a button).\nDrag FIRE exactly onto your fire button.";
        float ty = vh * 0.12f;
        for (String l : msg.split("\n")) { g.text(l, vw / 2, ty, 12.5f * u, 0xFFFFFFFF, Gfx.CENTER, true); ty += 17 * u; }
        float bw = 130 * u, bh = 34 * u, bx = (vw - bw) / 2, by = vh - bh - 18 * u;
        g.gradRound(bx, by, bw, bh, 10 * u, th.accent, Theme.mix(th.accent, 0xFF000000, 0.3f), true);
        g.text("Done", bx + bw / 2, by + bh / 2, 13 * u, 0xFFFFFFFF, Gfx.CENTER, true);
    }

    private void marker(Gfx g, float u, float x, float y, String label, int col, boolean active) {
        float r = 24 * u;
        g.shadow(x - r, y - r, r * 2, r * 2, r, 16 * u, Theme.withAlpha(col, active ? 0.8f : 0.45f));
        g.circle(x, y, r, Theme.withAlpha(col, 0.28f));
        g.ring(x, y, r, 2f, col);
        g.circle(x, y, 3.5f * u, col);
        g.text(label, x, y - r - 9 * u, 11 * u, 0xFFFFFFFF, Gfx.CENTER, true);
    }

    public boolean calHit(float px, float py) { return calibrating(); }

    public void calDown(float px, float py, float vw, float vh, float u) {
        float fx = s.get(Cfg.CAL_FX) * vw, fy = s.get(Cfg.CAL_FY) * vh, lx = s.get(Cfg.CAL_LX) * vw, ly = s.get(Cfg.CAL_LY) * vh;
        float bw = 130 * u, bh = 34 * u, bx = (vw - bw) / 2, by = vh - bh - 18 * u;
        calDrag = 0;
        if (px >= bx && px <= bx + bw && py >= by && py <= by + bh) { calDrag = 3; return; }
        float d1 = (float) Math.hypot(px - fx, py - fy), d2 = (float) Math.hypot(px - lx, py - ly);
        if (d1 < 40 * u && d1 <= d2) calDrag = 1;
        else if (d2 < 40 * u) calDrag = 2;
    }

    public void calMove(float px, float py, float vw, float vh) {
        if (calDrag == 1) { s.v[Cfg.CAL_FX] = Anim.clamp01(px / vw); s.v[Cfg.CAL_FY] = Anim.clamp01(py / vh); s.rev++; }
        else if (calDrag == 2) { s.v[Cfg.CAL_LX] = Anim.clamp01(px / vw); s.v[Cfg.CAL_LY] = Anim.clamp01(py / vh); s.rev++; }
    }

    /** @return true when "Done" was tapped */
    public boolean calUp(float px, float py, float vw, float vh, float u) {
        boolean done = false;
        if (calDrag == 3) {
            float bw = 130 * u, bh = 34 * u, bx = (vw - bw) / 2, by = vh - bh - 18 * u;
            done = px >= bx && px <= bx + bw && py >= by && py <= by + bh;
        }
        calDrag = 0;
        if (done) s.set(Cfg.CAL_ON, 0f);
        return done;
    }
}
