package rage.so2.ui;

import java.util.ArrayList;
import java.util.List;

import rage.so2.gen.Cfg;

/**
 * The settings window: sidebar with tabs, scrollable column(s) of cards, combo / colour popups.
 * Pure Java on top of {@link Gfx}; all sizes are in dp * scale (field {@code u}).
 */
public final class Menu {

    public interface Host {
        void saveSlot(int slot);
        void loadSlot(int slot);
        String exportText();
        int importText(String text);
        void toast(String msg);
        void setCalibration(boolean on);
        String statusText();
        String logText();
        String toolName(int id);
        void copyToClipboard(String s);
    }

    // ---- state ----
    public final Settings s;
    public final Theme th;
    private final Host host;
    public boolean open;
    public float x = Float.NaN, y = Float.NaN;   // window origin, px; NaN = centre on first layout
    float u = 2.5f;                       // px per dp (density * ui.scale)
    private float vw, vh;
    private float w, h;                           // window size px
    private int tab;
    private float tabAnim;                        // animated tab index
    private float openAnim;                       // 0..1
    private float scroll, scrollVel;
    private final float[] tabScroll = new float[Cfg.TAB_IDS.length];
    private int weaponSel = 0;
    private String toast;
    private float toastT;

    // layout cache
    private final List<Card> cards = new ArrayList<>();
    private float contentH;
    private long layoutRev = -1;
    private int layoutTab = -1, layoutWeapon = -1;
    private float layoutW = -1;

    // interaction
    private static final int D_NONE = 0, D_PENDING = 1, D_SCROLL = 2, D_SLIDER = 3, D_WINDOW = 4, D_POPUP = 5;
    private int drag;
    private float downX, downY, lastX, lastY, downT;
    private Row downRow;
    private float downRowX, downRowY, downRowW;
    private Popup popup;

    public Menu(Settings s, Theme th, Host host) {
        this.s = s;
        this.th = th;
        this.host = host;
    }

    public void toast(String m) { toast = m; toastT = 2.2f; }

    public void toggle() { open = !open; if (open) openAnim = Math.min(openAnim, 0.2f); popup = null; }

    // =====================================================================================================
    // layout model
    // =====================================================================================================

    static final class Card {
        String title;
        final List<Row> rows = new ArrayList<>();
        float x, y, w, h;   // content space
    }

    abstract static class Row {
        float h;
        abstract void draw(Menu m, Gfx g, float x, float y, float w);
        void tap(Menu m, float lx, float ly, float w) {}
        boolean slider() { return false; }
        void dragTo(Menu m, float lx, float w) {}
    }

    static final class Bind {
        final Cfg.Item it;
        final int slot;
        Bind(Cfg.Item it, int slot) { this.it = it; this.slot = slot; }
    }

    private float dimOf(Bind b, boolean enabledGate) {
        float a = 1f;
        if (b.it.dep >= 0 && !s.on(b.it.dep)) a = 0.42f;
        if (!enabledGate) a = Math.min(a, 0.42f);
        return a;
    }

    final class ToggleRow extends Row {
        final Bind b; final boolean gate; float anim = -1;
        ToggleRow(Bind b, boolean gate) { this.b = b; this.gate = gate; h = 32 * u; }
        @Override void draw(Menu m, Gfx g, float x, float y, float w) {
            float on = s.on(b.slot) ? 1f : 0f;
            if (anim < 0) anim = on;
            anim = Anim.approach(anim, on, 18f, frameDt);
            float a = dimOf(b, gate);
            g.save(); g.alpha(a);
            g.text(b.it.label, x, y + h / 2, 12 * u, th.text, Gfx.LEFT, false);
            float sw = 34 * u, sh = 18 * u, sx = x + w - sw, sy = y + (h - sh) / 2;
            g.fillRound(sx, sy, sw, sh, sh / 2, Theme.mix(0xFF22304A, th.accent, anim));
            if (anim > 0.02f) g.shadow(sx, sy, sw, sh, sh / 2, 6 * u, Theme.withAlpha(th.accent, 0.35f * anim));
            float kx = sx + sh / 2 + (sw - sh) * anim;
            g.circle(kx, sy + sh / 2, sh / 2 - 2.2f * u, Theme.mix(0xFF8798B2, 0xFFFFFFFF, anim));
            g.restore();
        }
        @Override void tap(Menu m, float lx, float ly, float w) {
            if (!gate) return;
            s.set(b.slot, s.on(b.slot) ? 0f : 1f);
            if (b.it.id.equals("cal.on")) { host.setCalibration(s.on(b.slot)); if (s.on(b.slot)) open = false; }
        }
    }

    final class SliderRow extends Row {
        final Bind b; final boolean gate; float shown = Float.NaN;
        SliderRow(Bind b, boolean gate) { this.b = b; this.gate = gate; h = 46 * u; }
        float frac() { return Anim.clamp01((s.get(b.slot) - b.it.min) / (b.it.max - b.it.min)); }
        @Override void draw(Menu m, Gfx g, float x, float y, float w) {
            float f = frac();
            if (Float.isNaN(shown)) shown = f;
            shown = Anim.approach(shown, f, 24f, frameDt);
            float a = dimOf(b, gate);
            g.save(); g.alpha(a);
            g.text(b.it.label, x, y + 12 * u, 12 * u, th.text, Gfx.LEFT, false);
            g.text(fmt(s.get(b.slot)), x + w, y + 12 * u, 11.5f * u, th.accentText, Gfx.RIGHT, true);
            float ty = y + 33 * u, th2 = 4 * u;
            g.fillRound(x, ty - th2 / 2, w, th2, th2 / 2, 0xFF2A3A57);
            float fw = Math.max(th2, w * shown);
            g.gradRound(x, ty - th2 / 2, fw, th2, th2 / 2, Theme.mix(th.accent, 0xFF000000, 0.25f), th.accent, false);
            float kx = x + w * shown;
            g.shadow(kx - 5 * u, ty - 5 * u, 10 * u, 10 * u, 5 * u, 7 * u, Theme.withAlpha(th.accent, 0.5f));
            g.circle(kx, ty, 6.2f * u, 0xFFFFFFFF);
            g.circle(kx, ty, 3.4f * u, th.accent);
            g.restore();
        }
        @Override boolean slider() { return gate; }
        @Override void dragTo(Menu m, float lx, float w) {
            if (!gate) return;
            float f = Anim.clamp01(lx / w);
            float v = b.it.min + f * (b.it.max - b.it.min);
            float st = b.it.step > 0 ? b.it.step : 1f;
            v = Math.round((v - b.it.min) / st) * st + b.it.min;
            s.set(b.slot, Anim.clamp(v, b.it.min, b.it.max));
        }
        String fmt(float v) {
            String t = b.it.step < 1f ? String.format(java.util.Locale.US, "%.1f", v) : String.valueOf(Math.round(v));
            return t + b.it.unit;
        }
    }

    final class ComboRow extends Row {
        final Bind b; final boolean gate;
        ComboRow(Bind b, boolean gate) { this.b = b; this.gate = gate; h = 34 * u; }
        @Override void draw(Menu m, Gfx g, float x, float y, float w) {
            float a = dimOf(b, gate);
            g.save(); g.alpha(a);
            g.text(b.it.label, x, y + h / 2, 12 * u, th.text, Gfx.LEFT, false);
            int idx = Math.max(0, Math.min(b.it.opts.length - 1, Math.round(s.get(b.slot))));
            String val = b.it.opts[idx];
            float fs = 11.5f * u;
            float tw = g.textW(val, fs, true);
            float bw = Math.min(w * 0.62f, tw + 30 * u), bh = 22 * u;
            float bx = x + w - bw, by = y + (h - bh) / 2;
            g.fillRound(bx, by, bw, bh, 6 * u, 0xFF18233A);
            g.strokeRound(bx, by, bw, bh, 6 * u, 1f, 0xFF27364F);
            g.save(); g.clip(bx + 4 * u, by, bw - 22 * u, bh);
            g.text(val, bx + 9 * u, by + bh / 2, fs, th.text, Gfx.LEFT, true);
            g.unclip(); g.restore();
            float ax = bx + bw - 10 * u, ay = by + bh / 2;
            g.poly(new float[]{ax - 3.2f * u, ay - 1.6f * u, ax + 3.2f * u, ay - 1.6f * u, ax, ay + 2.6f * u}, 3, th.accentText);
            g.restore();
        }
        @Override void tap(Menu m, float lx, float ly, float w) {
            if (!gate) return;
            int n = b.it.opts.length;
            if (n <= 3) { s.set(b.slot, (Math.round(s.get(b.slot)) + 1) % n); return; }
            popup = new ComboPopup(b, ly);
        }
    }

    final class ColorRow extends Row {
        final Bind b; final boolean gate;
        ColorRow(Bind b, boolean gate) { this.b = b; this.gate = gate; h = 32 * u; }
        @Override void draw(Menu m, Gfx g, float x, float y, float w) {
            float a = dimOf(b, gate);
            g.save(); g.alpha(a);
            g.text(b.it.label, x, y + h / 2, 12 * u, th.text, Gfx.LEFT, false);
            float sw = 30 * u, sh = 18 * u, sx = x + w - sw, sy = y + (h - sh) / 2;
            checker(g, sx, sy, sw, sh);
            int c = s.color(b.slot);
            g.fillRound(sx, sy, sw, sh, 5 * u, c);
            g.strokeRound(sx, sy, sw, sh, 5 * u, 1.2f, 0xFF3A4C6A);
            g.restore();
        }
        @Override void tap(Menu m, float lx, float ly, float w) { if (gate) popup = new ColorPopup(b); }
    }

    final class ButtonRow extends Row {
        final String label; final Runnable act; final boolean accent;
        ButtonRow(String label, boolean accent, Runnable act) { this.label = label; this.act = act; this.accent = accent; h = 34 * u; }
        @Override void draw(Menu m, Gfx g, float x, float y, float w) {
            float bh = 26 * u, by = y + (h - bh) / 2;
            if (accent) {
                g.gradRound(x, by, w, bh, 7 * u, th.accent, Theme.mix(th.accent, 0xFF000000, 0.3f), true);
                g.text(label, x + w / 2, by + bh / 2, fit(g, label, w - 12 * u, 11.5f * u), 0xFFFFFFFF, Gfx.CENTER, true);
            } else {
                g.fillRound(x, by, w, bh, 7 * u, 0xFF18233A);
                g.strokeRound(x, by, w, bh, 7 * u, 1f, 0xFF27364F);
                g.text(label, x + w / 2, by + bh / 2, fit(g, label, w - 12 * u, 11.5f * u), th.text, Gfx.CENTER, true);
            }
        }
        float fit(Gfx g, String t, float maxW, float fs) {
            float tw = g.textW(t, fs, true);
            return tw > maxW ? fs * maxW / tw : fs;
        }
        @Override void tap(Menu m, float lx, float ly, float w) { act.run(); }
    }

    final class SplitButtonRow extends Row {
        final String label; final String[] names; final Runnable[] acts;
        SplitButtonRow(String label, String[] names, Runnable[] acts) { this.label = label; this.names = names; this.acts = acts; h = 34 * u; }
        @Override void draw(Menu m, Gfx g, float x, float y, float w) {
            g.text(label, x, y + h / 2, 12 * u, th.text, Gfx.LEFT, false);
            float bh = 24 * u, by = y + (h - bh) / 2, bw = 58 * u, gap = 6 * u;
            for (int i = names.length - 1; i >= 0; i--) {
                float bx = x + w - (names.length - i) * bw - (names.length - 1 - i) * gap;
                g.fillRound(bx, by, bw, bh, 6 * u, i == 0 ? Theme.withAlpha(th.accent, 0.28f) : 0xFF18233A);
                g.strokeRound(bx, by, bw, bh, 6 * u, 1f, i == 0 ? th.accentDim : 0xFF27364F);
                g.text(names[i], bx + bw / 2, by + bh / 2, 11 * u, th.text, Gfx.CENTER, true);
            }
        }
        @Override void tap(Menu m, float lx, float ly, float w) {
            float bw = 58 * u, gap = 6 * u;
            for (int i = names.length - 1; i >= 0; i--) {
                float bx = w - (names.length - i) * bw - (names.length - 1 - i) * gap;
                if (lx >= bx && lx <= bx + bw) { acts[i].run(); return; }
            }
        }
    }

    final class TextRow extends Row {
        final String[] lines; final int color;
        TextRow(String text, int color, float width, float fontDp) {
            this.color = color;
            int maxChars = Math.max(12, (int) (width / (fontDp * 0.56f * u)));
            List<String> out = new ArrayList<>();
            for (String para : text.split("\n")) {
                String cur = "";
                for (String word : para.split(" ")) {
                    if (cur.length() + word.length() + 1 > maxChars && !cur.isEmpty()) { out.add(cur); cur = word; }
                    else cur = cur.isEmpty() ? word : cur + " " + word;
                }
                out.add(cur);
            }
            lines = out.toArray(new String[0]);
            h = (lines.length * (fontDp + 3.5f) + 8) * u;
            this.fontDp = fontDp;
        }
        final float fontDp;
        @Override void draw(Menu m, Gfx g, float x, float y, float w) {
            float ly = y + 8 * u;
            for (String l : lines) { g.text(l, x, ly, fontDp * u, color, Gfx.LEFT, false); ly += (fontDp + 3.5f) * u; }
        }
    }

    final class ChipsRow extends Row {
        final int n; final String[] names; final int[] cats; final int sel;
        ChipsRow(String[] names, int[] cats, int sel, float width) {
            this.names = names; this.cats = cats; this.n = names.length; this.sel = sel;
            h = chipRows(width) * 28 * u + 4 * u;
        }
        int chipRows(float width) {
            float cx = 0; int rows = 1;
            for (int i = 0; i < n; i++) {
                float cw = chipW(i);
                if (cx + cw > width && cx > 0) { rows++; cx = 0; }
                cx += cw + 6 * u;
            }
            return rows;
        }
        float chipW(int i) { return Math.max(40 * u, names[i].length() * 6.4f * u + 20 * u); }
        @Override void draw(Menu m, Gfx g, float x, float y, float w) {
            float cx = 0, cy = 0;
            for (int i = 0; i < n; i++) {
                float cw = chipW(i);
                if (cx + cw > w && cx > 0) { cx = 0; cy += 28 * u; }
                boolean on = i == sel;
                boolean ov = s.on(Cfg.wFlag(i));
                g.fillRound(x + cx, y + cy + 2 * u, cw, 23 * u, 7 * u, on ? Theme.withAlpha(th.accent, 0.30f) : 0xFF18233A);
                g.strokeRound(x + cx, y + cy + 2 * u, cw, 23 * u, 7 * u, 1f, on ? th.accent : 0xFF27364F);
                g.text(names[i], x + cx + cw / 2, y + cy + 13.5f * u, 11 * u, on ? th.text : th.dim, Gfx.CENTER, on);
                if (ov) g.circle(x + cx + cw - 5 * u, y + cy + 7 * u, 2.4f * u, th.good);
                cx += cw + 6 * u;
            }
        }
        @Override void tap(Menu m, float lx, float ly, float w) {
            float cx = 0, cy = 0;
            for (int i = 0; i < n; i++) {
                float cw = chipW(i);
                if (cx + cw > w && cx > 0) { cx = 0; cy += 28 * u; }
                if (lx >= cx && lx <= cx + cw && ly >= cy && ly <= cy + 28 * u) { weaponSel = i; layoutRev = -1; return; }
                cx += cw + 6 * u;
            }
        }
    }

    // =====================================================================================================
    // popups
    // =====================================================================================================

    abstract class Popup {
        float px, py, pw, ph;
        abstract void draw(Gfx g);
        abstract boolean down(float x, float y);   // true if inside (consumed)
        void move(float x, float y) {}
        void up() {}
    }

    final class ComboPopup extends Popup {
        final Bind b;
        ComboPopup(Bind b, float anchorLy) {
            this.b = b;
            int n = b.it.opts.length;
            float ih = 28 * u;
            pw = Math.min(w * 0.5f, 190 * u);
            ph = Math.min(h - 40 * u, n * ih + 12 * u);
            px = x + w - pw - 20 * u;
            py = y + (h - ph) / 2;
        }
        float scrollP;
        @Override void draw(Gfx g) {
            g.shadow(px, py, pw, ph, 10 * u, 22 * u, 0xAA000000);
            g.fillRound(px, py, pw, ph, 10 * u, 0xFF101826);
            g.strokeRound(px, py, pw, ph, 10 * u, 1f, th.strokeHi);
            int n = b.it.opts.length;
            float ih = 28 * u;
            int sel = Math.round(s.get(b.slot));
            g.save(); g.clip(px, py + 4 * u, pw, ph - 8 * u);
            for (int i = 0; i < n; i++) {
                float iy = py + 6 * u + i * ih - scrollP;
                if (i == sel) g.fillRound(px + 5 * u, iy, pw - 10 * u, ih - 2 * u, 7 * u, Theme.withAlpha(th.accent, 0.25f));
                g.text(b.it.opts[i], px + 14 * u, iy + ih / 2 - u, 12 * u, i == sel ? th.text : th.dim, Gfx.LEFT, i == sel);
            }
            g.unclip(); g.restore();
        }
        @Override boolean down(float mx, float my) {
            if (mx < px || mx > px + pw || my < py || my > py + ph) return false;
            int i = (int) ((my - py - 6 * u + scrollP) / (28 * u));
            if (i >= 0 && i < b.it.opts.length) s.set(b.slot, i);
            popup = null;
            return true;
        }
    }

    final class ColorPopup extends Popup {
        final Bind b;
        float hh, ss, vv, aa;
        int part;   // 1 = SV, 2 = hue, 3 = alpha
        ColorPopup(Bind b) {
            this.b = b;
            float[] hsv = Theme.hsv(s.color(b.slot));
            hh = hsv[0]; ss = hsv[1]; vv = hsv[2]; aa = s.get(b.slot + 3);
            pw = Math.min(w - 40 * u, 236 * u);
            ph = 208 * u;
            px = x + (w - pw) / 2;
            py = y + (h - ph) / 2;
        }
        float svX() { return px + 14 * u; } float svY() { return py + 38 * u; } float svW() { return pw - 66 * u; } float svH() { return 112 * u; }
        float hueX() { return px + pw - 42 * u; } float hueW() { return 16 * u; }
        float alY() { return py + ph - 36 * u; }
        void apply() { s.setColor(b.slot, Theme.fromHsv(hh, ss, vv, aa)); }
        @Override void draw(Gfx g) {
            g.shadow(px, py, pw, ph, 12 * u, 26 * u, 0xAA000000);
            g.fillRound(px, py, pw, ph, 12 * u, 0xFF101826);
            g.strokeRound(px, py, pw, ph, 12 * u, 1f, th.strokeHi);
            g.text(b.it.label, px + 14 * u, py + 20 * u, 12.5f * u, th.text, Gfx.LEFT, true);
            int cur = Theme.fromHsv(hh, ss, vv, aa);
            checker(g, px + pw - 52 * u, py + 10 * u, 38 * u, 18 * u);
            g.fillRound(px + pw - 52 * u, py + 10 * u, 38 * u, 18 * u, 5 * u, cur);
            g.strokeRound(px + pw - 52 * u, py + 10 * u, 38 * u, 18 * u, 5 * u, 1f, th.strokeHi);
            // saturation / value square
            int hueCol = Theme.fromHsv(hh, 1f, 1f, 1f);
            g.gradRound(svX(), svY(), svW(), svH(), 6 * u, 0xFFFFFFFF, hueCol, false);
            g.gradRound(svX(), svY(), svW(), svH(), 6 * u, 0x00000000, 0xFF000000, true);
            g.strokeRound(svX(), svY(), svW(), svH(), 6 * u, 1f, th.strokeHi);
            float kx = svX() + ss * svW(), ky = svY() + (1 - vv) * svH();
            g.ring(kx, ky, 5.5f * u, 1.6f, 0xFFFFFFFF);
            g.ring(kx, ky, 7f * u, 1f, 0x88000000);
            // hue bar
            int seg = 12;
            float sh = svH() / seg;
            for (int i = 0; i < seg; i++) {
                g.gradRound(hueX(), svY() + i * sh, hueW(), sh + 0.6f, 0, Theme.fromHsv(i / (float) seg, 1, 1, 1), Theme.fromHsv((i + 1) / (float) seg, 1, 1, 1), true);
            }
            float hy = svY() + hh * svH();
            g.fillRound(hueX() - 3 * u, hy - 2.5f * u, hueW() + 6 * u, 5 * u, 2.5f * u, 0xFFFFFFFF);
            // alpha bar
            float ax = svX(), aw = svW() + 26 * u + hueW();
            checker(g, ax, alY(), aw, 14 * u);
            g.gradRound(ax, alY(), aw, 14 * u, 5 * u, Theme.fromHsv(hh, ss, vv, 0f), Theme.fromHsv(hh, ss, vv, 1f), false);
            g.strokeRound(ax, alY(), aw, 14 * u, 5 * u, 1f, th.strokeHi);
            g.circle(ax + aa * aw, alY() + 7 * u, 8 * u, 0xFFFFFFFF);
            g.circle(ax + aa * aw, alY() + 7 * u, 5 * u, cur | 0xFF000000);
            String hex = String.format("#%02X%02X%02X", (cur >> 16) & 255, (cur >> 8) & 255, cur & 255);
            g.text(hex + "  a " + Math.round(aa * 100) + "%", px + 14 * u, py + ph - 10 * u, 10 * u, th.dim, Gfx.LEFT, false);
        }
        @Override boolean down(float mx, float my) {
            if (mx < px || mx > px + pw || my < py || my > py + ph) { popup = null; return false; }
            part = 0;
            if (mx >= svX() && mx <= svX() + svW() && my >= svY() && my <= svY() + svH()) part = 1;
            else if (mx >= hueX() - 6 * u && mx <= hueX() + hueW() + 6 * u && my >= svY() && my <= svY() + svH()) part = 2;
            else if (my >= alY() - 4 * u && my <= alY() + 18 * u) part = 3;
            move(mx, my);
            return true;
        }
        @Override void move(float mx, float my) {
            if (part == 1) { ss = Anim.clamp01((mx - svX()) / svW()); vv = 1 - Anim.clamp01((my - svY()) / svH()); apply(); }
            else if (part == 2) { hh = Anim.clamp01((my - svY()) / svH()) * 0.9999f; apply(); }
            else if (part == 3) { aa = Anim.clamp01((mx - svX()) / (svW() + 26 * u + hueW())); apply(); }
        }
    }

    private void checker(Gfx g, float x, float y, float w, float h) {
        g.save(); g.clip(x, y, w, h);
        float c = 5 * u;
        for (int j = 0; j * c < h; j++) for (int i = 0; i * c < w; i++) {
            g.fillRect(x + i * c, y + j * c, c, c, ((i + j) & 1) == 0 ? 0xFF2A3446 : 0xFF3A465C);
        }
        g.unclip(); g.restore();
    }

    // =====================================================================================================
    // building the card list for the active tab
    // =====================================================================================================

    float frameDt = 1f / 60f;

    private void rebuild(float contentW) {
        cards.clear();
        layoutRev = s.rev;
        layoutTab = tab;
        layoutWeapon = weaponSel;
        layoutW = contentW;
        String custom = Cfg.TAB_CUSTOM[tab];
        List<Card> built = new ArrayList<>();
        final boolean twoCols = contentW >= 330 * u;
        final float gap = 10 * u;
        final float colW = twoCols ? (contentW - gap) / 2 : contentW;
        final float rowW = colW - 26 * u;
        for (int gi = 0; gi < Cfg.GROUP_TITLES[tab].length; gi++) {
            Card c = new Card();
            c.title = Cfg.GROUP_TITLES[tab][gi];
            for (Cfg.Item it : Cfg.ITEMS) {
                if (it.tab != tab || it.grp != gi || it.type == Cfg.T_HIDDEN) continue;
                c.rows.add(rowFor(new Bind(it, it.slot), true));
            }
            if (c.title.equals("Touch calibration")) {
                c.rows.add(new TextRow("Turn on calibration mode, then drag the FIRE and LOOK markers on screen.", th.dim, rowW, 10.5f));
            }
            if (!c.rows.isEmpty()) built.add(c);
        }
        if (custom.equals("weapons")) {
            Card pick = new Card();
            pick.title = "Weapon profiles";
            pick.rows.add(new TextRow("Pick a weapon and switch on its profile. Green dot = custom profile active.", th.dim, rowW, 10.5f));
            pick.rows.add(new ChipsRow(Cfg.W_NAMES, Cfg.W_CATS, weaponSel, 0));
            built.add(pick);
            Card prof = new Card();
            prof.title = Cfg.W_NAMES[weaponSel] + "  ·  " + Cfg.CATS[Cfg.W_CATS[weaponSel]];
            final int ws = weaponSel;
            prof.rows.add(new SplitToggle(ws));
            boolean ov = s.on(Cfg.wFlag(ws));
            for (Cfg.Item it : Cfg.ITEMS) {
                if (it.widx < 0) continue;
                prof.rows.add(rowFor(new Bind(it, Cfg.wSlot(ws, it)), ov));
            }
            prof.rows.add(new ButtonRow("Copy global values here", false, new Runnable() {
                public void run() {
                    for (Cfg.Item it : Cfg.ITEMS) if (it.widx >= 0) s.set(Cfg.wSlot(ws, it), s.get(it.slot));
                    toast("Copied global values to " + Cfg.W_NAMES[ws]);
                }
            }));
            built.add(prof);
        } else if (custom.equals("config")) {
            Card slots = new Card();
            slots.title = "Config slots";
            for (int i = 0; i < 5; i++) {
                final int slot = i;
                slots.rows.add(new SplitButtonRow("Slot " + (i + 1), new String[]{"Save", "Load"}, new Runnable[]{
                        new Runnable() { public void run() { host.saveSlot(slot); toast("Saved to slot " + (slot + 1)); } },
                        new Runnable() { public void run() { host.loadSlot(slot); layoutRev = -1; } }}));
            }
            built.add(slots);
            Card tr = new Card();
            tr.title = "Share";
            tr.rows.add(new ButtonRow("Copy to clipboard", true, new Runnable() {
                public void run() { host.copyToClipboard(host.exportText()); toast("Config copied"); }
            }));
            tr.rows.add(new ButtonRow("Paste from clipboard", false, new Runnable() {
                public void run() { int n = host.importText(null); toast(n < 0 ? "Clipboard has no config" : "Applied " + n + " values"); layoutRev = -1; }
            }));
            tr.rows.add(new ButtonRow("Reset to defaults", false, new Runnable() {
                public void run() { s.resetAll(); toast("Defaults restored"); layoutRev = -1; }
            }));
            built.add(tr);
            Card st = new Card();
            st.title = "Diagnostics";
            String status = host.statusText();
            if (status == null || status.isEmpty()) status = "no data yet";
            st.rows.add(new TextRow(status, th.dim, rowW, 9.5f));
            st.rows.add(new ButtonRow("Copy log", false, new Runnable() {
                public void run() { host.copyToClipboard(host.statusText() + "\n--- log ---\n" + host.logText()); toast("Log copied"); }
            }));
            built.add(st);
        }
        if (tab == tabIndex("misc") && s.on(Cfg.DBG_PANEL)) {
            Card st = new Card();
            st.title = "Live status";
            String status = host.statusText();
            if (status == null || status.isEmpty()) status = "no data yet";
            st.rows.add(new TextRow(status, th.dim, rowW, 9.5f));
            built.add(st);
        }
        // masonry: shorter column gets the next card
        float[] colH = new float[twoCols ? 2 : 1];
        for (Card c : built) {
            // card height
            float ch = 30 * u;
            for (Row r : c.rows) ch += r.h;
            ch += 8 * u;
            int col = 0;
            for (int i = 1; i < colH.length; i++) if (colH[i] < colH[col]) col = i;
            c.x = col * (colW + gap);
            c.y = colH[col];
            c.w = colW;
            c.h = ch;
            colH[col] += ch + gap;
            cards.add(c);
        }
        float mx = 0;
        for (float v : colH) mx = Math.max(mx, v);
        contentH = mx;
    }

    private int tabIndex(String id) {
        for (int i = 0; i < Cfg.TAB_IDS.length; i++) if (Cfg.TAB_IDS[i].equals(id)) return i;
        return -1;
    }

    final class SplitToggle extends Row {
        final int ws; float anim = -1;
        SplitToggle(int ws) { this.ws = ws; h = 34 * u; }
        @Override void draw(Menu m, Gfx g, float x, float y, float w) {
            float on = s.on(Cfg.wFlag(ws)) ? 1f : 0f;
            if (anim < 0) anim = on;
            anim = Anim.approach(anim, on, 18f, frameDt);
            g.text("Use custom profile", x, y + h / 2, 12 * u, th.text, Gfx.LEFT, true);
            float sw = 34 * u, sh = 18 * u, sx = x + w - sw, sy = y + (h - sh) / 2;
            g.fillRound(sx, sy, sw, sh, sh / 2, Theme.mix(0xFF22304A, th.good, anim));
            g.circle(sx + sh / 2 + (sw - sh) * anim, sy + sh / 2, sh / 2 - 2.2f * u, Theme.mix(0xFF8798B2, 0xFFFFFFFF, anim));
        }
        @Override void tap(Menu m, float lx, float ly, float w) {
            boolean turningOn = !s.on(Cfg.wFlag(ws));
            if (turningOn) {
                // a fresh profile starts from the current global values instead of the factory defaults
                boolean pristine = true;
                for (Cfg.Item it : Cfg.ITEMS) if (it.widx >= 0 && s.get(Cfg.wSlot(ws, it)) != it.def) { pristine = false; break; }
                if (pristine) for (Cfg.Item it : Cfg.ITEMS) if (it.widx >= 0) s.set(Cfg.wSlot(ws, it), s.get(it.slot));
            }
            s.set(Cfg.wFlag(ws), turningOn ? 1f : 0f);
        }
    }

    private Row rowFor(Bind b, boolean gate) {
        switch (b.it.type) {
            case Cfg.T_TOGGLE: return new ToggleRow(b, gate);
            case Cfg.T_SLIDER: return new SliderRow(b, gate);
            case Cfg.T_COMBO: return new ComboRow(b, gate);
            case Cfg.T_COLOR: return new ColorRow(b, gate);
            default: return new TextRow(b.it.label, th.dim, 200 * u, 10.5f);
        }
    }

    // =====================================================================================================
    // frame
    // =====================================================================================================

    /** @return true while something is still animating (caller keeps invalidating) */
    public boolean update(float dt, float viewW, float viewH, float density) {
        frameDt = dt;
        vw = viewW; vh = viewH;
        float newU = density * Anim.clamp(s.get(Cfg.UI_SCALE), 0.7f, 1.6f);
        if (Math.abs(newU - u) > 1e-4f) { u = newU; layoutRev = -1; }
        th.opacity = s.get(Cfg.UI_ALPHA);
        th.setAccent(s.color(Cfg.UI_ACCENT));
        float tw = Math.min(vw - 16 * u, 568 * u), tH = Math.min(vh - 12 * u, 326 * u);
        if (tw != w || tH != h) { w = tw; h = tH; layoutRev = -1; }
        if (Float.isNaN(x)) { x = (vw - w) / 2; y = (vh - h) / 2; }
        x = Anim.clamp(x, -w + 80 * u, vw - 80 * u);
        y = Anim.clamp(y, 0, Math.max(0, vh - 40 * u));
        float target = open ? 1f : 0f;
        boolean anim = Math.abs(openAnim - target) > 0.002f;
        openAnim = Anim.approach(openAnim, target, 16f, dt);
        tabAnim = Anim.approach(tabAnim, tab, 16f, dt);
        if (toastT > 0) toastT -= dt;
        // scroll inertia
        if (drag != D_SCROLL && Math.abs(scrollVel) > 4f) {
            scroll += scrollVel * dt;
            scrollVel *= (float) Math.exp(-5.5f * dt);
            anim = true;
        }
        clampScroll();
        return open || anim || Math.abs(tabAnim - tab) > 0.002f;
    }

    private float sideW() { return 132 * u; }
    private float headH() { return 40 * u; }
    private float viewH() { return h - headH() - 8 * u; }
    private float contentX() { return x + sideW() + 12 * u; }
    private float contentY() { return y + headH(); }
    private float contentW() { return w - sideW() - 24 * u; }

    private void clampScroll() {
        float max = Math.max(0, contentH - viewH() + 6 * u);
        if (scroll < 0) { scroll = 0; scrollVel = 0; }
        if (scroll > max) { scroll = max; scrollVel = 0; }
    }

    public void draw(Gfx g) {
        if (openAnim < 0.01f) return;
        float t = Anim.easeOutCubic(openAnim);
        if (layoutRev != s.rev || layoutTab != tab || layoutWeapon != weaponSel || layoutW != contentW()) { rebuild(contentW()); clampScroll(); }
        g.save();
        g.alpha(Anim.clamp01(openAnim * 1.4f));
        g.scale(0.94f + 0.06f * t, 0.94f + 0.06f * t, x + w / 2, y + h / 2);
        // window
        g.shadow(x, y, w, h, 14 * u, 34 * u, 0xCC000000);
        g.fillRound(x, y, w, h, 14 * u, Theme.withAlpha(th.bg, th.opacity));
        g.strokeRound(x, y, w, h, 14 * u, 1.2f, th.stroke);
        // sidebar
        float sw = sideW();
        g.save(); g.clip(x, y, sw, h);
        g.fillRound(x, y, sw + 20 * u, h, 14 * u, Theme.withAlpha(th.side, th.opacity));
        g.unclip(); g.restore();
        g.line(x + sw, y + 10 * u, x + sw, y + h - 10 * u, 1f, th.stroke);
        drawBrand(g, x + 16 * u, y + 24 * u);
        float ty = y + 58 * u;
        for (int i = 0; i < Cfg.TAB_IDS.length; i++) {
            float iy = ty + i * 40 * u;
            boolean on = i == tab;
            float pill = Anim.clamp01(1f - Math.abs(tabAnim - i));
            if (pill > 0.01f) {
                g.gradRound(x + 8 * u, iy, sw - 16 * u, 34 * u, 9 * u, Theme.withAlpha(th.accent, 0.30f * pill), Theme.withAlpha(th.accent, 0.08f * pill), false);
                g.fillRound(x + 8 * u, iy + 8 * u, 3 * u, 18 * u, 1.5f * u, Theme.withAlpha(th.accent, pill));
            }
            Icons.draw(g, Cfg.TAB_ICONS[i], x + 28 * u, iy + 17 * u, 15 * u, on ? th.accentText : th.dim);
            g.text(Cfg.TAB_TITLES[i], x + 46 * u, iy + 17 * u, 12.5f * u, on ? th.text : th.dim, Gfx.LEFT, on);
        }
        g.text("v1.0  ·  Standoff 2", x + 16 * u, y + h - 16 * u, 9.5f * u, th.dim2, Gfx.LEFT, false);
        // header
        g.text(Cfg.TAB_TITLES[tab], contentX(), y + headH() / 2 + 2 * u, 15 * u, th.text, Gfx.LEFT, true);
        float cx = x + w - 20 * u, cy = y + 22 * u;
        g.circle(cx, cy, 11 * u, 0xFF18233A);
        g.line(cx - 4 * u, cy - 4 * u, cx + 4 * u, cy + 4 * u, 1.6f, th.dim);
        g.line(cx + 4 * u, cy - 4 * u, cx - 4 * u, cy + 4 * u, 1.6f, th.dim);
        // content
        float vx = contentX() - 4 * u, vy = contentY(), vwd = contentW() + 8 * u, vhh = viewH();
        g.save(); g.clip(vx, vy, vwd, vhh);
        for (Card c : cards) {
            float cxx = contentX() + c.x, cyy = vy + c.y - scroll + 2 * u;
            if (cyy > vy + vhh || cyy + c.h < vy) {
                for (Row r : c.rows) r.draw(this, new NoopGfx(g), 0, 0, 0);   // keep animations ticking cheaply: skipped
                continue;
            }
            g.fillRound(cxx, cyy, c.w, c.h, 11 * u, Theme.withAlpha(th.card, th.opacity));
            g.strokeRound(cxx, cyy, c.w, c.h, 11 * u, 1f, th.stroke);
            g.text(c.title.toUpperCase(), cxx + 13 * u, cyy + 16 * u, 9.8f * u, th.accentText, Gfx.LEFT, true);
            g.fillRound(cxx + 13 * u, cyy + 25 * u, 18 * u, 1.6f * u, 0.8f * u, th.accentDim);
            float ry = cyy + 30 * u;
            for (Row r : c.rows) {
                r.draw(this, g, cxx + 13 * u, ry, c.w - 26 * u);
                ry += r.h;
            }
        }
        g.unclip(); g.restore();
        // scrollbar
        float max = Math.max(1, contentH - vhh + 6 * u);
        if (contentH > vhh) {
            float bh = Math.max(24 * u, vhh * vhh / contentH);
            float by = vy + (vhh - bh) * (scroll / max);
            g.fillRound(x + w - 6 * u, by, 3 * u, bh, 1.5f * u, Theme.withAlpha(th.accent, 0.5f));
        }
        if (popup != null) {
            g.fillRound(x, y, w, h, 14 * u, 0x66000000);
            popup.draw(g);
        }
        if (toastT > 0 && toast != null) {
            float a = Anim.clamp01(toastT * 2f);
            float tw2 = g.textW(toast, 11.5f * u, true) + 28 * u;
            g.save(); g.alpha(a);
            g.fillRound(x + (w - tw2) / 2 + sideW() / 2, y + h - 38 * u, tw2, 24 * u, 12 * u, 0xEE0A1220);
            g.strokeRound(x + (w - tw2) / 2 + sideW() / 2, y + h - 38 * u, tw2, 24 * u, 12 * u, 1f, th.accentDim);
            g.text(toast, x + w / 2 + sideW() / 2, y + h - 26 * u, 11.5f * u, th.text, Gfx.CENTER, true);
            g.restore();
        }
        g.restore();
    }

    private void drawBrand(Gfx g, float cx, float cy) {
        float s0 = 24 * u;
        g.shadow(cx - 4 * u, cy - s0 / 2, s0, s0, 7 * u, 10 * u, Theme.withAlpha(th.accent, 0.55f));
        g.gradRound(cx - 4 * u, cy - s0 / 2, s0, s0, 7 * u, Theme.lighten(th.accent, 0.25f), Theme.mix(th.accent, 0xFF000000, 0.35f), true);
        g.text("R", cx - 4 * u + s0 / 2, cy + 0.5f * u, 15 * u, 0xFFFFFFFF, Gfx.CENTER, true);
        g.text("RAGEMOD", cx + 27 * u, cy - 3 * u, 13.5f * u, th.text, Gfx.LEFT, true);
        g.text("standoff 2", cx + 27 * u, cy + 10 * u, 9 * u, th.dim, Gfx.LEFT, false);
    }

    /** cheap stand-in so off-screen rows still advance their animations without drawing */
    static final class NoopGfx implements Gfx {
        final Gfx g;
        NoopGfx(Gfx g) { this.g = g; }
        public float width() { return g.width(); } public float height() { return g.height(); }
        public void fillRect(float x, float y, float w, float h, int c) {} public void fillRound(float x, float y, float w, float h, float r, int c) {}
        public void strokeRound(float x, float y, float w, float h, float r, float sw, int c) {} public void gradRound(float x, float y, float w, float h, float r, int a, int b, boolean v) {}
        public void shadow(float x, float y, float w, float h, float r, float bl, int c) {} public void line(float a, float b, float c, float d, float sw, int col) {}
        public void circle(float x, float y, float r, int c) {} public void ring(float x, float y, float r, float sw, int c) {} public void poly(float[] xy, int n, int c) {}
        public void radial(float x, float y, float r, int a, int b) {}
        public void text(String s, float x, float y, float size, int c, int al, boolean b) {} public float textW(String s, float size, boolean b) { return 0; }
        public void clip(float x, float y, float w, float h) {} public void unclip() {} public void save() {} public void restore() {}
        public void translate(float dx, float dy) {} public void scale(float sx, float sy, float px, float py) {} public void alpha(float a) {}
    }

    // =====================================================================================================
    // touch
    // =====================================================================================================

    public boolean hit(float px, float py) {
        if (openAnim < 0.5f && !open) return false;
        return open && px >= x && px <= x + w && py >= y && py <= y + h;
    }

    public void touchDown(float px, float py) {
        downX = lastX = px; downY = lastY = py; downT = 0; scrollVel = 0; downRow = null; drag = D_NONE;
        if (popup != null) {
            if (popup.down(px, py)) { drag = D_POPUP; return; }
            popup = null;
            return;
        }
        // close button
        float cx = x + w - 20 * u, cy = y + 22 * u;
        if (Math.hypot(px - cx, py - cy) < 17 * u) { open = false; drag = D_NONE; return; }
        // header -> move window
        if (py < y + headH() && px > x + sideW() * 0.0f) { drag = D_WINDOW; return; }
        // sidebar tabs
        if (px < x + sideW()) {
            float ty = y + 58 * u;
            if (py < ty) { drag = D_WINDOW; return; }
            int i = (int) ((py - ty) / (40 * u));
            if (i >= 0 && i < Cfg.TAB_IDS.length && py >= ty && (py - ty) - i * 40 * u < 36 * u) {
                tabScroll[tab] = scroll;
                tab = i;
                scroll = tabScroll[tab];
                scrollVel = 0;
                layoutRev = -1;
            }
            return;
        }
        // content rows
        float vy = contentY();
        if (py < vy || py > vy + viewH()) return;
        for (Card c : cards) {
            float cxx = contentX() + c.x, cyy = vy + c.y - scroll + 2 * u;
            if (px < cxx || px > cxx + c.w || py < cyy || py > cyy + c.h) continue;
            float ry = cyy + 30 * u;
            for (Row r : c.rows) {
                if (py >= ry && py < ry + r.h) {
                    downRow = r; downRowX = cxx + 13 * u; downRowY = ry; downRowW = c.w - 26 * u;
                    if (r.slider() && px >= downRowX - 6 * u && py > ry + 22 * u) { drag = D_SLIDER; r.dragTo(this, px - downRowX, downRowW); return; }
                    break;
                }
                ry += r.h;
            }
            break;
        }
        drag = D_PENDING;
    }

    public void touchMove(float px, float py, float dt) {
        float dx = px - lastX, dy = py - lastY;
        if (drag == D_POPUP && popup != null) { popup.move(px, py); }
        else if (drag == D_SLIDER && downRow != null) { downRow.dragTo(this, px - downRowX, downRowW); }
        else if (drag == D_WINDOW) { x += dx; y += dy; }
        else if (drag == D_PENDING) {
            if (Math.hypot(px - downX, py - downY) > 7 * u) { drag = D_SCROLL; downRow = null; }
        }
        if (drag == D_SCROLL) {
            scroll -= dy;
            if (dt > 0) scrollVel = -dy / dt * 0.6f + scrollVel * 0.4f;
            clampScroll();
        }
        lastX = px; lastY = py;
    }

    public void touchUp(float px, float py) {
        if (drag == D_PENDING && downRow != null) {
            downRow.tap(this, px - downRowX, py - downRowY, downRowW);
        } else if (drag == D_POPUP && popup != null) {
            popup.up();
        }
        drag = D_NONE;
        downRow = null;
    }

    public void cancel() { drag = D_NONE; downRow = null; }
}
