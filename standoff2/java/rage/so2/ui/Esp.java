package rage.so2.ui;

import rage.so2.gen.Cfg;

/** Draws players (boxes, skeleton, labels, snaplines, arrows, backtrack ghosts) and the aim FOV circle from a {@link FrameData}. */
public final class Esp {
    public interface Labels {
        /** text shown for a player's held weapon, or null */
        String weaponLabel(int weaponIdx, int playerId);
    }

    // joint indices: 0 head 1 neck 2 chest 3 hips 4 lUpperArm 5 lElbow 6 lHand 7 rUpperArm 8 rElbow 9 rHand 10 lThigh 11 lKnee 12 lFoot 13 rThigh 14 rKnee 15 rFoot
    private static final int[][] BONES = {
            {0, 1}, {1, 2}, {2, 3}, {2, 4}, {4, 5}, {5, 6}, {2, 7}, {7, 8}, {8, 9}, {3, 10}, {10, 11}, {11, 12}, {3, 13}, {13, 14}, {14, 15}
    };

    private final float[] tmp = new float[6];

    public void draw(Gfx g, FrameData fr, Settings s, Theme th, float u, float vw, float vh, Labels labels) {
        if (!fr.valid()) return;
        final float sx = vw / fr.camW(), sy = vh / fr.camH();
        final float ccx = fr.f[FrameData.H_CX] * sx, ccy = fr.f[FrameData.H_CY] * sy;

        // aim FOV circle
        if (s.on(Cfg.AIM_ENABLE) && s.on(Cfg.AIM_CIRCLE) && fr.f[FrameData.H_FOVPX] > 1f) {
            int c = s.color(Cfg.AIM_CIRCLEC);
            boolean engaged = fr.has(FrameData.HF_ENGAGED);
            g.ring(ccx, ccy, fr.f[FrameData.H_FOVPX] * sy, Math.max(1f, u * 0.55f), Theme.withAlpha(c, engaged ? 1f : 0.7f));
            if (engaged) g.ring(ccx, ccy, fr.f[FrameData.H_FOVPX] * sy + u * 2f, Math.max(1f, u * 0.4f), Theme.withAlpha(c, 0.25f));
        }
        if (!s.on(Cfg.ESP_ENABLE)) return;

        final int n = fr.count();
        final int box = (int) s.get(Cfg.ESP_BOX);
        final int snap = (int) s.get(Cfg.ESP_SNAP);
        final float thick = Math.max(1f, s.get(Cfg.ESP_THICK) * u * 0.55f);
        final float maxDist = s.get(Cfg.ESP_MAXDIST);
        final int cVis = s.color(Cfg.ESP_CVIS), cHid = s.color(Cfg.ESP_CHID), cTeam = s.color(Cfg.ESP_CTEAM);
        final int cGhost = s.color(Cfg.BT_GHOSTC);

        for (int i = 0; i < n; i++) {
            int pf = fr.pflags(i);
            boolean enemy = (pf & FrameData.PF_ENEMY) != 0;
            if (s.on(Cfg.ESP_ENEMY) && !enemy) continue;
            float dist = fr.p(i, FrameData.P_DIST);
            if (dist > maxDist) continue;
            boolean vis = (pf & FrameData.PF_VISIBLE) != 0;
            boolean target = (pf & FrameData.PF_TARGET) != 0;
            int col = !enemy ? cTeam : (vis ? cVis : cHid);

            if ((pf & FrameData.PF_OFFSCREEN) != 0 && s.on(Cfg.ESP_ARROWS)) {
                float R = Math.min(vw, vh) * 0.40f;
                float ax = fr.p(i, FrameData.P_AX), ay = fr.p(i, FrameData.P_AY);
                float px = vw / 2 + ax * R * 1.25f, py = vh / 2 + ay * R;
                float a = u * 7.5f;
                float nx = -ay, ny = ax;
                g.poly(new float[]{px + ax * a * 1.4f, py + ay * a * 1.4f, px - ax * a * 0.6f + nx * a, py - ay * a * 0.6f + ny * a, px - ax * a * 0.6f - nx * a, py - ay * a * 0.6f - ny * a}, 3, Theme.withAlpha(0xFF000000, 0.6f));
                float b = a * 0.8f;
                g.poly(new float[]{px + ax * b * 1.4f, py + ay * b * 1.4f, px - ax * b * 0.6f + nx * b, py - ay * b * 0.6f + ny * b, px - ax * b * 0.6f - nx * b, py - ay * b * 0.6f - ny * b}, 3, col);
            }

            if ((pf & FrameData.PF_BOX) == 0) continue;
            float x1 = fr.p(i, FrameData.P_BX1) * sx, y1 = fr.p(i, FrameData.P_BY1) * sy;
            float x2 = fr.p(i, FrameData.P_BX2) * sx, y2 = fr.p(i, FrameData.P_BY2) * sy;
            float w = x2 - x1, h = y2 - y1;
            if (w < 2 || h < 2) continue;
            if (x2 < -50 || x1 > vw + 50 || y2 < -50 || y1 > vh + 50) continue;

            // snapline
            if (snap > 0) {
                float ax = vw / 2, ay = snap == 1 ? 0 : (snap == 2 ? vh / 2 : vh);
                g.line(ax, ay, (x1 + x2) / 2, y2, Math.max(1f, thick * 0.6f), Theme.withAlpha(col, 0.55f));
            }
            // fill
            if (s.on(Cfg.ESP_FILL) && box > 0) g.gradRound(x1, y1, w, h, Math.min(u * 2, w / 4), Theme.withAlpha(col, 0.17f), Theme.withAlpha(col, 0.02f), true);
            // box
            if (box == 1) {
                g.strokeRound(x1, y1, w, h, 0, thick + 2f, Theme.withAlpha(0xFF000000, 0.55f));
                g.strokeRound(x1, y1, w, h, 0, thick, col);
            } else if (box == 2) {
                float L = Math.min(w, h) * 0.30f;
                corner(g, x1, y1, L, L, thick, col);
                corner(g, x2, y1, -L, L, thick, col);
                corner(g, x1, y2, L, -L, thick, col);
                corner(g, x2, y2, -L, -L, thick, col);
            }
            // skeleton
            if (s.on(Cfg.ESP_SKEL)) {
                int base = fr.base(i) + FrameData.P_J;
                for (int[] b : BONES) {
                    float ax = fr.f[base + b[0] * 2], ay = fr.f[base + b[0] * 2 + 1], bx = fr.f[base + b[1] * 2], by = fr.f[base + b[1] * 2 + 1];
                    if (Float.isNaN(ax) || Float.isNaN(bx)) continue;
                    ax *= sx; ay *= sy; bx *= sx; by *= sy;
                    g.line(ax, ay, bx, by, thick + 1.6f, Theme.withAlpha(0xFF000000, 0.45f));
                    g.line(ax, ay, bx, by, thick, Theme.withAlpha(col, 0.95f));
                }
            }
            // head
            float hx = fr.p(i, FrameData.P_HX) * sx, hy = fr.p(i, FrameData.P_HY) * sy;
            if (s.on(Cfg.ESP_HEAD) && !Float.isNaN(hx)) {
                float r = Math.max(u * 2.2f, h * 0.045f);
                g.ring(hx, hy, r + 1f, thick + 1.4f, Theme.withAlpha(0xFF000000, 0.5f));
                g.ring(hx, hy, r, thick, col);
                if (target) g.circle(hx, hy, r * 0.5f, col);
            }
            // labels
            float fs = Math.max(8.5f * u, Math.min(12f * u, h * 0.10f + 6f * u));
            if (s.on(Cfg.ESP_WEAPON)) {
                String wl = s.on(Cfg.DBG_TOOL) ? labels.weaponLabel(-2, (int) fr.p(i, FrameData.P_ID)) : labels.weaponLabel((int) fr.p(i, FrameData.P_WEAPON), (int) fr.p(i, FrameData.P_ID));
                if (wl != null && !wl.isEmpty()) pill(g, wl, (x1 + x2) / 2, y1 - fs * 0.95f, fs, u, th, col);
            }
            if (s.on(Cfg.ESP_DIST)) {
                String d = Math.round(dist) + "m";
                g.text(d, (x1 + x2) / 2 + 1, y2 + fs * 0.95f + 1, fs, Theme.withAlpha(0xFF000000, 0.8f), Gfx.CENTER, true);
                g.text(d, (x1 + x2) / 2, y2 + fs * 0.95f, fs, 0xFFFFFFFF, Gfx.CENTER, true);
            }
            // backtrack ghost (dashed box)
            if ((pf & FrameData.PF_GHOST) != 0 && s.on(Cfg.BT_ENABLE) && s.on(Cfg.BT_GHOST)) {
                float gx1 = fr.p(i, FrameData.P_GX1) * sx, gy1 = fr.p(i, FrameData.P_GY1) * sy, gx2 = fr.p(i, FrameData.P_GX2) * sx, gy2 = fr.p(i, FrameData.P_GY2) * sy;
                dashed(g, gx1, gy1, gx2, gy1, u, cGhost); dashed(g, gx2, gy1, gx2, gy2, u, cGhost);
                dashed(g, gx2, gy2, gx1, gy2, u, cGhost); dashed(g, gx1, gy2, gx1, gy1, u, cGhost);
            }
            // aim link
            if (target && s.on(Cfg.AIM_ENABLE) && !Float.isNaN(hx)) {
                g.line(ccx, ccy, hx, hy, Math.max(1f, u * 0.5f), Theme.withAlpha(th.accent, 0.55f));
            }
        }
    }

    private static void corner(Gfx g, float x, float y, float dx, float dy, float t, int col) {
        g.line(x, y, x + dx, y, t + 2f, Theme.withAlpha(0xFF000000, 0.5f));
        g.line(x, y, x, y + dy, t + 2f, Theme.withAlpha(0xFF000000, 0.5f));
        g.line(x, y, x + dx, y, t, col);
        g.line(x, y, x, y + dy, t, col);
    }

    private static void dashed(Gfx g, float x1, float y1, float x2, float y2, float u, int col) {
        float dx = x2 - x1, dy = y2 - y1;
        float len = (float) Math.sqrt(dx * dx + dy * dy);
        if (len < 1f) return;
        float step = u * 4f;
        for (float d = 0; d < len; d += step * 2) {
            float e = Math.min(len, d + step);
            g.line(x1 + dx * d / len, y1 + dy * d / len, x1 + dx * e / len, y1 + dy * e / len, Math.max(1f, u * 0.6f), col);
        }
    }

    private static void pill(Gfx g, String text, float cx, float cy, float fs, float u, Theme th, int accent) {
        float tw = g.textW(text, fs, true) + fs * 1.1f;
        float ph = fs * 1.5f;
        g.fillRound(cx - tw / 2, cy - ph / 2, tw, ph, ph / 2, 0xB30A1019);
        g.strokeRound(cx - tw / 2, cy - ph / 2, tw, ph, ph / 2, 1f, Theme.withAlpha(accent, 0.55f));
        g.text(text, cx, cy, fs, 0xFFFFFFFF, Gfx.CENTER, true);
    }
}
