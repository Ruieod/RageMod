import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.GradientPaint;
import java.awt.Graphics2D;
import java.awt.Point;
import java.awt.RadialGradientPaint;
import java.awt.RenderingHints;
import java.awt.Shape;
import java.awt.geom.*;
import java.awt.image.BufferedImage;
import java.io.File;
import java.util.ArrayDeque;
import java.util.Deque;
import javax.imageio.ImageIO;

import rage.so2.gen.Cfg;
import rage.so2.ui.*;

/** Desktop renderer for design review: draws the real UI classes through Java2D and writes PNGs. Not shipped. */
public class Preview {

    static final class J2D implements Gfx {
        final Graphics2D g; final float w, h;
        float aMul = 1f;
        final Deque<Object[]> stack = new ArrayDeque<>();
        final Deque<Shape> clips = new ArrayDeque<>();
        J2D(Graphics2D g, float w, float h) {
            this.g = g; this.w = w; this.h = h;
            g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
            g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
            g.setRenderingHint(RenderingHints.KEY_STROKE_CONTROL, RenderingHints.VALUE_STROKE_PURE);
            g.setRenderingHint(RenderingHints.KEY_FRACTIONALMETRICS, RenderingHints.VALUE_FRACTIONALMETRICS_ON);
        }
        Color col(int argb) {
            int a = Math.round(((argb >>> 24) & 0xFF) * aMul);
            return new Color((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, Math.max(0, Math.min(255, a)));
        }
        public float width() { return w; } public float height() { return h; }
        public void fillRect(float x, float y, float ww, float hh, int c) { g.setPaint(col(c)); g.fill(new Rectangle2D.Float(x, y, ww, hh)); }
        public void fillRound(float x, float y, float ww, float hh, float r, int c) { g.setPaint(col(c)); g.fill(new RoundRectangle2D.Float(x, y, ww, hh, r * 2, r * 2)); }
        public void strokeRound(float x, float y, float ww, float hh, float r, float sw, int c) {
            g.setPaint(col(c)); g.setStroke(new BasicStroke(sw));
            if (r <= 0) g.draw(new Rectangle2D.Float(x, y, ww, hh)); else g.draw(new RoundRectangle2D.Float(x, y, ww, hh, r * 2, r * 2));
        }
        public void gradRound(float x, float y, float ww, float hh, float r, int ca, int cb, boolean v) {
            g.setPaint(v ? new GradientPaint(x, y, col(ca), x, y + Math.max(hh, 1), col(cb)) : new GradientPaint(x, y, col(ca), x + Math.max(ww, 1), y, col(cb)));
            if (r <= 0) g.fill(new Rectangle2D.Float(x, y, ww, hh)); else g.fill(new RoundRectangle2D.Float(x, y, ww, hh, r * 2, r * 2));
        }
        public void shadow(float x, float y, float ww, float hh, float r, float blur, int argb) {
            int base = (argb >>> 24) & 0xFF; int steps = 6;
            for (int i = steps; i >= 1; i--) {
                float t = i / (float) steps, grow = blur * t;
                float al = base * aMul * (1f - t) * (1f - t) * 0.55f + base * aMul * 0.04f;
                g.setPaint(new Color((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (int) Math.min(255, al)));
                g.fill(new RoundRectangle2D.Float(x - grow, y - grow * 0.6f, ww + 2 * grow, hh + grow * 1.8f, (r + grow) * 2, (r + grow) * 2));
            }
        }
        public void line(float x1, float y1, float x2, float y2, float sw, int c) { g.setPaint(col(c)); g.setStroke(new BasicStroke(sw, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND)); g.draw(new Line2D.Float(x1, y1, x2, y2)); }
        public void circle(float cx, float cy, float r, int c) { g.setPaint(col(c)); g.fill(new Ellipse2D.Float(cx - r, cy - r, r * 2, r * 2)); }
        public void ring(float cx, float cy, float r, float sw, int c) { g.setPaint(col(c)); g.setStroke(new BasicStroke(sw)); g.draw(new Ellipse2D.Float(cx - r, cy - r, r * 2, r * 2)); }
        public void poly(float[] xy, int n, int c) { Path2D.Float p = new Path2D.Float(); p.moveTo(xy[0], xy[1]); for (int i = 1; i < n; i++) p.lineTo(xy[i * 2], xy[i * 2 + 1]); p.closePath(); g.setPaint(col(c)); g.fill(p); }
        public void radial(float cx, float cy, float r, int c0, int c1) {
            g.setPaint(new RadialGradientPaint(new Point2D.Float(cx, cy), Math.max(1f, r), new float[]{0f, 1f}, new Color[]{col(c0), col(c1)}));
            g.fill(new Ellipse2D.Float(cx - r, cy - r, r * 2, r * 2));
        }
        Font font(float size, boolean bold) { return new Font("DejaVu Sans", bold ? Font.BOLD : Font.PLAIN, 1).deriveFont(size); }
        public void text(String s, float x, float y, float size, int c, int align, boolean bold) {
            g.setFont(font(size, bold)); FontMetrics fm = g.getFontMetrics();
            float tw = fm.stringWidth(s);
            float px = align == CENTER ? x - tw / 2 : (align == RIGHT ? x - tw : x);
            float py = y + (fm.getAscent() - fm.getDescent()) / 2f;
            g.setPaint(col(c)); g.drawString(s, px, py);
        }
        public float textW(String s, float size, boolean bold) { g.setFont(font(size, bold)); return g.getFontMetrics().stringWidth(s); }
        public void clip(float x, float y, float ww, float hh) { clips.push(g.getClip() == null ? new Rectangle2D.Float(-1e6f, -1e6f, 2e6f, 2e6f) : g.getClip()); g.clip(new Rectangle2D.Float(x, y, ww, hh)); }
        public void unclip() { g.setClip(clips.pop()); }
        public void save() { stack.push(new Object[]{g.getTransform(), aMul}); }
        public void restore() { Object[] o = stack.pop(); g.setTransform((AffineTransform) o[0]); aMul = (Float) o[1]; }
        public void translate(float dx, float dy) { g.translate(dx, dy); }
        public void scale(float sx, float sy, float px, float py) { g.translate(px, py); g.scale(sx, sy); g.translate(-px, -py); }
        public void alpha(float a) { aMul *= a; }
    }

    static final Menu.Host HOST = new Menu.Host() {
        public void saveSlot(int s) {} public void loadSlot(int s) {} public String exportText() { return ""; } public int importText(String t) { return 0; }
        public void toast(String m) {} public void setCalibration(boolean on) {}
        public String statusText() { return "overlay 2400x1080  dpi x2.75  fps 60\nnative: linked  frames: 18342  age 6 ms\ntick hooks: 18342 (0.41 ms)  exceptions: 0\nresolved: 52 ok / 0 failed\ncharacters: 9  zones: 2  local: yes  team: 2\nweapon: AKR (tool 'AKR(Clone)')\naim gain: 0.84 / 0.79  lag: 2  samples: 311"; }
        public String logText() { return "log"; }
        public String toolName(int id) { return "AKR(Clone)"; }
        public void copyToClipboard(String s) {}
    };

    static FrameData sampleFrame(int w, int h) {
        FrameData fd = new FrameData();
        float[] f = fd.f;
        f[FrameData.H_SEQ] = 5; f[FrameData.H_W] = w; f[FrameData.H_H] = h; f[FrameData.H_COUNT] = 3;
        f[FrameData.H_FLAGS] = FrameData.HF_CAMERA | FrameData.HF_TICK | FrameData.HF_LOCAL | FrameData.HF_ENGAGED;
        f[FrameData.H_FOVPX] = 260; f[FrameData.H_CX] = w / 2f; f[FrameData.H_CY] = h / 2f; f[FrameData.H_TARGET] = 0;
        player(f, 0, 1, w * 0.56f, h * 0.30f, 330, 18.4f, FrameData.PF_ENEMY | FrameData.PF_VISIBLE | FrameData.PF_TARGET | FrameData.PF_BOX | FrameData.PF_GHOST, 12);
        player(f, 1, 2, w * 0.25f, h * 0.38f, 240, 31.2f, FrameData.PF_ENEMY | FrameData.PF_BOX, 14);
        player(f, 2, 3, w * 0.82f, h * 0.44f, 170, 9.0f, FrameData.PF_BOX | FrameData.PF_TEAMKNOWN, 15);
        fd.receivedAtMs = System.currentTimeMillis();
        return fd;
    }

    static void player(float[] f, int i, int id, float hx, float hy, float hgt, float dist, int flags, int weapon) {
        int b = FrameData.HEADER + i * FrameData.STRIDE;
        f[b + FrameData.P_ID] = id; f[b + FrameData.P_FLAGS] = flags; f[b + FrameData.P_DIST] = dist; f[b + FrameData.P_WEAPON] = weapon;
        float s = hgt;
        float[][] j = {{0, 0}, {0, 0.07f}, {0, 0.22f}, {0, 0.46f}, {-0.11f, 0.14f}, {-0.16f, 0.30f}, {-0.12f, 0.43f}, {0.11f, 0.14f}, {0.16f, 0.30f}, {0.12f, 0.43f},
                {-0.06f, 0.48f}, {-0.07f, 0.72f}, {-0.07f, 0.95f}, {0.06f, 0.48f}, {0.07f, 0.72f}, {0.07f, 0.95f}};
        for (int k = 0; k < 16; k++) { f[b + FrameData.P_J + k * 2] = hx + j[k][0] * s; f[b + FrameData.P_J + k * 2 + 1] = hy + j[k][1] * s; }
        f[b + FrameData.P_HX] = hx; f[b + FrameData.P_HY] = hy;
        f[b + FrameData.P_BX1] = hx - 0.17f * s; f[b + FrameData.P_BX2] = hx + 0.17f * s;
        f[b + FrameData.P_BY1] = hy - 0.06f * s; f[b + FrameData.P_BY2] = hy + 1.0f * s;
        f[b + FrameData.P_GX1] = hx - 0.17f * s - 22; f[b + FrameData.P_GX2] = hx + 0.17f * s - 22; f[b + FrameData.P_GY1] = hy - 0.05f * s; f[b + FrameData.P_GY2] = hy + 1.0f * s;
    }

    static void background(Graphics2D g, int w, int h) {
        g.setPaint(new GradientPaint(0, 0, new Color(0x6a8aa8), 0, h * 0.55f, new Color(0xc9b99a)));
        g.fillRect(0, 0, w, (int) (h * 0.55f));
        g.setPaint(new GradientPaint(0, h * 0.55f, new Color(0x8a7a63), 0, h, new Color(0x4a4236)));
        g.fillRect(0, (int) (h * 0.55f), w, h);
        g.setColor(new Color(0x7d6f5a)); g.fillRect((int) (w * 0.05f), (int) (h * 0.22f), (int) (w * 0.14f), (int) (h * 0.4f));
        g.setColor(new Color(0x6d6050)); g.fillRect((int) (w * 0.62f), (int) (h * 0.18f), (int) (w * 0.2f), (int) (h * 0.5f));
        g.setColor(new Color(40, 40, 40, 160)); g.fillOval((int) (w * 0.47f), (int) (h * 0.80f), (int) (w * 0.08f), (int) (h * 0.06f));
        // fake HUD buttons
        g.setColor(new Color(255, 255, 255, 60));
        g.fillOval((int) (w * 0.84f), (int) (h * 0.62f), (int) (h * 0.2f), (int) (h * 0.2f));
        g.fillOval((int) (w * 0.78f), (int) (h * 0.48f), (int) (h * 0.11f), (int) (h * 0.11f));
        g.fillOval((int) (w * 0.08f), (int) (h * 0.55f), (int) (h * 0.30f), (int) (h * 0.30f));
    }

    public static void main(String[] a) throws Exception {
        String out = a.length > 0 ? a[0] : "build/preview";
        new File(out).mkdirs();
        final int W = 2400, H = 1080;
        final float density = 2.75f;

        Settings s = new Settings();
        Theme th = new Theme();
        Menu menu = new Menu(s, th, HOST);
        Hud hud = new Hud(s, th);
        Esp esp = new Esp();
        FrameData fd = sampleFrame(W, H);
        Esp.Labels labels = (idx, id) -> idx >= 0 && idx < Cfg.W_NAMES.length ? Cfg.W_NAMES[idx] : "AKR";

        // (1) ESP + watermark only
        {
            BufferedImage img = new BufferedImage(W, H, BufferedImage.TYPE_INT_ARGB);
            Graphics2D g2 = img.createGraphics(); background(g2, W, H);
            J2D g = new J2D(g2, W, H);
            float u = density;
            esp.draw(g, fd, s, th, u, W, H, labels);
            th.setAccent(s.color(Cfg.UI_ACCENT));
            hud.drawWatermark(g, u, 143, -1);
            hud.drawButton(g, u, false, 0.016f);
            ImageIO.write(img, "png", new File(out, "esp.png"));
        }
        // (2) menu tabs
        for (int t = 0; t < Cfg.TAB_IDS.length; t++) {
            BufferedImage img = new BufferedImage(W, H, BufferedImage.TYPE_INT_ARGB);
            Graphics2D g2 = img.createGraphics(); background(g2, W, H);
            J2D g = new J2D(g2, W, H);
            Menu m = new Menu(s, th, HOST);
            m.open = true;
            for (int i = 0; i < 40; i++) m.update(0.016f, W, H, density);
            // switch tab through the touch path
            float tx = m.x + 20 * density, ty = m.y + (58 + 40 * t + 10) * density;
            m.touchDown(tx, ty); m.touchUp(tx, ty);
            for (int i = 0; i < 40; i++) m.update(0.016f, W, H, density);
            esp.draw(g, fd, s, th, density, W, H, labels);
            hud.drawButton(g, density, true, 0.016f);
            m.draw(g);
            hud.drawWatermark(g, density, 143, -1);
            ImageIO.write(img, "png", new File(out, "menu_" + Cfg.TAB_IDS[t] + ".png"));
        }
        // (3) popups on the Visuals tab
        {
            BufferedImage img = new BufferedImage(W, H, BufferedImage.TYPE_INT_ARGB);
            Graphics2D g2 = img.createGraphics(); background(g2, W, H);
            J2D g = new J2D(g2, W, H);
            Menu m = new Menu(s, th, HOST);
            m.open = true;
            for (int i = 0; i < 40; i++) m.update(0.016f, W, H, density);
            float tx = m.x + 20 * density, ty = m.y + (58 + 40 * 2 + 10) * density;
            m.touchDown(tx, ty); m.touchUp(tx, ty);
            for (int i = 0; i < 40; i++) m.update(0.016f, W, H, density);
            m.draw(g);
            // find a colour row: tap roughly where the first colour swatch of the ESP card is
            float cx = m.x + (132 + 12) * density + 150 * density, cy = m.y + 40 * density + 30 * density + 15 * 32.5f * density;
            ImageIO.write(img, "png", new File(out, "menu_visuals_raw.png"));
        }
        // (4) kill effects
        for (int st = 0; st < 4; st++) {
            BufferedImage img = new BufferedImage(W, H, BufferedImage.TYPE_INT_ARGB);
            Graphics2D g2 = img.createGraphics(); background(g2, W, H);
            J2D g = new J2D(g2, W, H);
            KillFx k = new KillFx();
            esp.draw(g, fd, s, th, density, W, H, labels);
            k.spawn(W * 0.56f, H * 0.34f, st, 0xFF3AA0FF, 1.3f, "+1 KILL", true, 0);
            k.draw(g, density, 330, 0.016f);
            ImageIO.write(img, "png", new File(out, "kill_" + st + ".png"));
        }
        // (5) calibration
        {
            BufferedImage img = new BufferedImage(W, H, BufferedImage.TYPE_INT_ARGB);
            Graphics2D g2 = img.createGraphics(); background(g2, W, H);
            J2D g = new J2D(g2, W, H);
            s.set(Cfg.CAL_ON, 1f);
            hud.drawCalibration(g, density, 0.016f);
            s.set(Cfg.CAL_ON, 0f);
            ImageIO.write(img, "png", new File(out, "calibration.png"));
        }
        System.out.println("wrote previews to " + out);
    }
}
