package rage.so2.android;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.graphics.Canvas;
import android.os.Build;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Choreographer;
import android.view.HapticFeedbackConstants;
import android.view.ViewGroup;
import android.view.Window;

import java.util.HashMap;
import java.util.Map;

import rage.so2.Native;
import rage.so2.gen.Cfg;
import rage.so2.ui.Anim;
import rage.so2.ui.Esp;
import rage.so2.ui.FrameData;
import rage.so2.ui.Hud;
import rage.so2.ui.KillFx;
import rage.so2.ui.Menu;
import rage.so2.ui.PointerRouter;
import rage.so2.ui.Settings;
import rage.so2.ui.Theme;

/** Owns everything that lives on top of the game: view, menu, HUD, ESP, effects, touch routing and the per-frame native sync. */
public final class Overlay implements Menu.Host, PointerRouter.Ui, Choreographer.FrameCallback, Esp.Labels {
    private static final String TAG = "RageMod";
    private static Overlay inst;

    public static synchronized void install(Activity a) {
        if (inst != null) inst.detach();
        inst = new Overlay(a);
        inst.attach();
    }

    // ---- core objects ----
    private final Activity act;
    private final Settings s = new Settings();
    private final Theme th = new Theme();
    private final Store store;
    private final Menu menu;
    private final Hud hud;
    private final Esp esp = new Esp();
    private final KillFx kfx = new KillFx();
    private final FrameData fd = new FrameData();
    private final AndroidGfx gfx = new AndroidGfx();
    private final TouchRouter touch;
    private final PointerRouter router;
    private RageView view;
    private float density = 2.5f;

    // ---- sync state ----
    private boolean attached;
    private boolean nativeOk = true;
    private long syncedRev = -1, savedRev = -1, lastRevChange;
    private int lastSeq, lastKillSeq = -1;
    private long lastFrameNanos;
    private float dt = 1f / 60f, fps = 60f;
    private int framesSeen, exceptions;
    private final float[] cmd = new float[8];
    private boolean lookDown, fireDown;
    private float lookOx, lookOy;
    private long lastStatusMs;
    private String statusCache = "";
    private int uiOwner, uiPointer = -1;
    private long lastDrawReq;
    private final Map<Integer, String> toolLabels = new HashMap<>();

    private Overlay(Activity a) {
        act = a;
        store = new Store(a);
        String saved = store.read("autosave.cfg");
        if (saved != null) s.parse(saved);
        savedRev = s.rev;
        menu = new Menu(s, th, this);
        hud = new Hud(s, th);
        touch = new TouchRouter(a.getWindow().getCallback());
        router = new PointerRouter(this, touch);
        touch.router = router;
        try { hud.user = android.os.Build.MODEL != null ? shortName(android.os.Build.MODEL) : "user"; } catch (Throwable ignored) { }
    }

    private static String shortName(String m) { return m.length() > 14 ? m.substring(0, 14) : m; }

    private void attach() {
        DisplayMetrics dm = act.getResources().getDisplayMetrics();
        density = dm.density;
        view = new RageView(act, this);
        act.addContentView(view, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        Window w = act.getWindow();
        if (w.getCallback() != touch) { touch.base = w.getCallback(); w.setCallback(touch); }
        attached = true;
        lastFrameNanos = 0;
        Choreographer.getInstance().postFrameCallback(this);
        Log.i(TAG, "overlay attached to " + act.getClass().getName());
    }

    private void detach() {
        attached = false;
        try { Choreographer.getInstance().removeFrameCallback(this); } catch (Throwable ignored) { }
        try {
            Window w = act.getWindow();
            if (w.getCallback() == touch) w.setCallback(touch.base);
            if (view != null && view.getParent() instanceof ViewGroup) ((ViewGroup) view.getParent()).removeView(view);
        } catch (Throwable ignored) { }
    }

    // =====================================================================================================
    // frame loop
    // =====================================================================================================

    @Override public void doFrame(long nanos) {
        if (!attached) return;
        try {
            step(nanos);
        } catch (Throwable t) {
            if (exceptions++ < 5) Log.e(TAG, "frame error", t);
        }
        if (attached) Choreographer.getInstance().postFrameCallback(this);
    }

    private float u() { return density * Anim.clamp(s.get(Cfg.UI_SCALE), 0.7f, 1.6f); }

    private void step(long nanos) {
        if (lastFrameNanos != 0) {
            dt = Anim.clamp((nanos - lastFrameNanos) / 1e9f, 0.001f, 0.1f);
            fps += (1f / dt - fps) * 0.08f;
        }
        lastFrameNanos = nanos;
        final long nowMs = SystemClock.uptimeMillis();
        final float W = view.getWidth(), H = view.getHeight();
        if (W < 10 || H < 10) { view.invalidate(); return; }

        // the activity may have swapped our callback (rare); keep routing alive
        Window win = act.getWindow();
        if (win.getCallback() != touch) { touch.base = win.getCallback(); win.setCallback(touch); }

        if (nativeOk) {
            try {
                if (syncedRev != s.rev) { Native.setConfig(s.v); syncedRev = s.rev; lastRevChange = nowMs; }
                router.setGeometry(W, H, s.get(Cfg.CAL_FX) * W, s.get(Cfg.CAL_FY) * H, 0.085f * Math.min(W, H));
                boolean block = menu.open || hud.calibrating();
                Native.setInput(router.userFiring() ? 1f : 0f, router.userLooking() ? 1f : 0f, W, H, fps, block ? 1f : 0f);
                Native.pollCmd(cmd);
                applyCommand(W, H, nowMs);
                int seq = Native.readFrame(fd.f);
                if (seq != 0 && seq != lastSeq) {
                    lastSeq = seq;
                    framesSeen++;
                    fd.receivedAtMs = nowMs;
                    onNewFrame(W, H, nowMs);
                }
                if (cmd[5] > 0.5f && s.on(Cfg.KILL_VIBE)) view.performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP);
            } catch (UnsatisfiedLinkError e) {
                nativeOk = false;
                Log.e(TAG, "native not available: " + e);
            }
        }
        if (nowMs - lastRevChange > 1500 && savedRev != s.rev) { store.write("autosave.cfg", s.serialize()); savedRev = s.rev; }

        boolean menuAnim = menu.update(dt, W, H, density);
        boolean live = fd.valid() && nowMs - fd.receivedAtMs < 400 && fd.count() > 0;
        boolean need = menuAnim || live || kfx.active() || hud.calibrating() || nowMs - lastDrawReq > 120 || s.on(Cfg.DBG_PANEL);
        if (need) { view.invalidate(); lastDrawReq = nowMs; }
    }

    private void onNewFrame(float W, float H, long nowMs) {
        int ks = (int) fd.f[FrameData.H_KILLSEQ];
        if (lastKillSeq < 0) lastKillSeq = ks;
        if (ks != lastKillSeq) {
            lastKillSeq = ks;
            if (s.on(Cfg.KILL_ENABLE) && fd.camW() > 0) {
                float sx = W / fd.camW(), sy = H / fd.camH();
                kfx.spawn(fd.f[FrameData.H_KILLX] * sx, fd.f[FrameData.H_KILLY] * sy, (int) s.get(Cfg.KILL_STYLE), s.color(Cfg.KILL_COLOR), s.get(Cfg.KILL_SIZE),
                        s.on(Cfg.KILL_TEXT) ? "+1 KILL" : null, s.on(Cfg.KILL_FLASH), nowMs);
            }
        }
    }

    private void applyCommand(float W, float H, long now) {
        boolean block = menu.open || hud.calibrating();
        boolean wantLook = cmd[1] > 0.5f && !block;
        boolean wantFire = cmd[4] > 0.5f && !block;
        float rx = fd.valid() ? W / fd.camW() : 1f, ry = fd.valid() ? H / fd.camH() : 1f;
        float dx = cmd[2] * rx, dy = cmd[3] * ry;
        float ax = s.get(Cfg.CAL_LX) * W, ay = s.get(Cfg.CAL_LY) * H;
        float fx = s.get(Cfg.CAL_FX) * W, fy = s.get(Cfg.CAL_FY) * H;

        if (wantLook) {
            if (!lookDown) {
                router.setSynthetic(PointerRouter.SYN_LOOK, true, ax, ay);
                router.flush(now);
                lookDown = true; lookOx = 0; lookOy = 0;
            }
            lookOx += dx; lookOy += dy;
            float maxOff = 0.20f * Math.min(W, H);
            if (Math.abs(lookOx) > maxOff || Math.abs(lookOy) > maxOff) {
                // lift and re-plant at the anchor so the finger can never run off the look area
                router.setSynthetic(PointerRouter.SYN_LOOK, false, 0, 0);
                router.flush(now);
                router.setSynthetic(PointerRouter.SYN_LOOK, true, ax, ay);
                router.flush(now);
                lookOx = dx; lookOy = dy;
            }
            router.setSynthetic(PointerRouter.SYN_LOOK, true, Anim.clamp(ax + lookOx, 2f, W - 2f), Anim.clamp(ay + lookOy, 2f, H - 2f));
            router.flush(now);
        } else if (lookDown) {
            router.setSynthetic(PointerRouter.SYN_LOOK, false, 0, 0);
            router.flush(now);
            lookDown = false;
        }
        if (wantFire != fireDown) {
            router.setSynthetic(PointerRouter.SYN_FIRE, wantFire, fx, fy);
            router.flush(now);
            fireDown = wantFire;
        }
    }

    // =====================================================================================================
    // drawing
    // =====================================================================================================

    void draw(Canvas c, int w, int h) {
        gfx.begin(c, w, h);
        final long nowMs = SystemClock.uptimeMillis();
        final float uu = u();
        if (fd.valid() && nowMs - fd.receivedAtMs < 400) esp.draw(gfx, fd, s, th, uu, w, h, this);
        kfx.draw(gfx, uu, nowMs, dt);
        if (hud.calibrating()) {
            hud.drawCalibration(gfx, uu, dt);
        } else {
            hud.drawButton(gfx, uu, menu.open, dt);
            menu.draw(gfx);
        }
        hud.drawWatermark(gfx, uu, fps, -1);
        if (s.on(Cfg.DBG_PANEL) && !menu.open) drawDebug(uu);
    }

    private void drawDebug(float uu) {
        long now = SystemClock.uptimeMillis();
        if (now - lastStatusMs > 400) { lastStatusMs = now; statusCache = statusText(); }
        String[] ls = statusCache.split("\n");
        float y = 44 * uu, fs = 9.5f * uu;
        float wmax = 0;
        for (String l : ls) wmax = Math.max(wmax, gfx.textW(l, fs, false));
        gfx.fillRound(8 * uu, y - 10 * uu, wmax + 16 * uu, ls.length * 12.5f * uu + 12 * uu, 8 * uu, 0xB0070C14);
        for (String l : ls) { gfx.text(l, 16 * uu, y + 2 * uu, fs, 0xFFB9C6DC, rage.so2.ui.Gfx.LEFT, false); y += 12.5f * uu; }
    }

    // =====================================================================================================
    // PointerRouter.Ui
    // =====================================================================================================

    @Override public boolean hit(float x, float y) {
        if (hud.calibrating()) return true;
        float W = view.getWidth(), H = view.getHeight();
        return menu.hit(x, y) || hud.btnHit(x, y, W, H, u());
    }

    @Override public void down(int id, float x, float y) {
        if (uiPointer >= 0) return;
        float W = view.getWidth(), H = view.getHeight();
        uiPointer = id;
        if (hud.calibrating()) { uiOwner = 3; hud.calDown(x, y, W, H, u()); }
        else if (menu.hit(x, y)) { uiOwner = 1; menu.touchDown(x, y); }
        else { uiOwner = 2; hud.btnDown(x, y, W, H, u()); }
        view.invalidate();
    }

    @Override public void move(int id, float x, float y) {
        if (id != uiPointer) return;
        float W = view.getWidth(), H = view.getHeight();
        if (uiOwner == 1) menu.touchMove(x, y, dt);
        else if (uiOwner == 2) hud.btnMove(x, y, W, H, u());
        else if (uiOwner == 3) hud.calMove(x, y, W, H);
        view.invalidate();
    }

    @Override public void up(int id, float x, float y, boolean cancel) {
        if (id != uiPointer) return;
        float W = view.getWidth(), H = view.getHeight();
        if (uiOwner == 1) { if (cancel) menu.cancel(); else menu.touchUp(x, y); }
        else if (uiOwner == 2) { if (hud.btnUp() && !cancel) menu.toggle(); }
        else if (uiOwner == 3) { if (hud.calUp(x, y, W, H, u()) && !cancel) menu.toast("Anchors saved"); }
        uiPointer = -1; uiOwner = 0;
        view.invalidate();
    }

    // =====================================================================================================
    // Menu.Host / Esp.Labels
    // =====================================================================================================

    @Override public void saveSlot(int slot) { store.write("slot" + (slot + 1) + ".cfg", s.serialize()); }

    @Override public void loadSlot(int slot) {
        String t = store.read("slot" + (slot + 1) + ".cfg");
        if (t == null) { menu.toast("Slot " + (slot + 1) + " is empty"); return; }
        int n = s.parse(t);
        menu.toast(n < 0 ? "Slot " + (slot + 1) + " is broken" : "Loaded slot " + (slot + 1));
    }

    @Override public String exportText() { return s.serialize(); }

    @Override public int importText(String text) {
        if (text == null) {
            try {
                ClipboardManager cm = (ClipboardManager) act.getSystemService(Context.CLIPBOARD_SERVICE);
                ClipData d = cm.getPrimaryClip();
                if (d == null || d.getItemCount() == 0) return -1;
                CharSequence cs = d.getItemAt(0).coerceToText(act);
                text = cs == null ? null : cs.toString();
            } catch (Throwable t) { return -1; }
        }
        return s.parse(text);
    }

    @Override public void toast(String msg) { menu.toast(msg); }
    @Override public void setCalibration(boolean on) { view.invalidate(); }

    @Override public String statusText() {
        StringBuilder sb = new StringBuilder();
        sb.append("overlay ").append(view == null ? 0 : view.getWidth()).append('x').append(view == null ? 0 : view.getHeight())
                .append("  dpi x").append(String.format(java.util.Locale.US, "%.2f", density)).append("  fps ").append(Math.round(fps)).append('\n');
        sb.append("native: ").append(nativeOk ? "linked" : "NOT linked").append("  frames: ").append(framesSeen)
                .append("  age ").append(fd.receivedAtMs == 0 ? -1 : SystemClock.uptimeMillis() - fd.receivedAtMs).append(" ms\n");
        if (nativeOk) {
            try { sb.append(Native.status()); } catch (Throwable t) { sb.append("status unavailable"); }
        }
        return sb.toString();
    }

    @Override public String logText() {
        try { return Native.log(120); } catch (Throwable t) { return ""; }
    }

    @Override public String toolName(int id) {
        try { return Native.toolName(id); } catch (Throwable t) { return ""; }
    }

    @Override public void copyToClipboard(String text) {
        try {
            ClipboardManager cm = (ClipboardManager) act.getSystemService(Context.CLIPBOARD_SERVICE);
            cm.setPrimaryClip(ClipData.newPlainText("ragemod", text));
        } catch (Throwable ignored) { }
    }

    @Override public String weaponLabel(int weaponIdx, int playerId) {
        if (weaponIdx >= 0 && weaponIdx < Cfg.W_NAMES.length) return Cfg.W_NAMES[weaponIdx];
        String raw = toolLabels.get(playerId);
        long now = SystemClock.uptimeMillis();
        if (raw == null || (now & 1023) < 17) {
            raw = toolName(playerId);
            if (raw != null) raw = raw.replace("(Clone)", "").trim();
            toolLabels.put(playerId, raw == null ? "" : raw);
        }
        return (weaponIdx == -2 || raw == null || raw.isEmpty()) ? (raw == null ? "" : raw) : raw;
    }
}
