package rage.so2.ui;

/**
 * Sits between the OS touch stream and the game.
 *
 * - real pointers that start on our UI (menu / floating button / calibration markers) are kept away from the game
 * - synthetic fingers (aimbot "look" finger, autofire "fire" finger) are merged into the stream the game sees
 * - while hold-fire is set (silent aim has a target), real fingers that went down on the fire button are kept away from
 *   the game too: the native side fires through the synthetic fire finger once the real view sits on the target
 *
 * The game-side stream is always re-emitted as a canonical sequence (DOWN / POINTER_DOWN / MOVE / POINTER_UP / UP) built from
 * the diff between the previous and the desired pointer set, so the game never sees an inconsistent gesture.
 */
public final class PointerRouter {

    public interface Ui {
        boolean hit(float x, float y);
        void down(int id, float x, float y);
        void move(int id, float x, float y);
        void up(int id, float x, float y, boolean cancel);
    }

    /** Receives events for the game. `action` is a MotionEvent masked action (without the index bits). */
    public interface Sink {
        void emit(int action, int actionIndex, int count, int[] ids, float[] xs, float[] ys, long downTime, long eventTime);
    }

    public static final int ACTION_DOWN = 0, ACTION_UP = 1, ACTION_MOVE = 2, ACTION_CANCEL = 3, ACTION_POINTER_DOWN = 5, ACTION_POINTER_UP = 6;
    public static final int SYN_LOOK = 0, SYN_FIRE = 1, SYN_COUNT = 2;
    public static final int SYN_ID_BASE = 24;

    private static final int MAXID = 32;
    private static final int OWN_NONE = 0, OWN_UI = 1, OWN_GAME = 2;

    private final Ui ui;
    private final Sink sink;

    // real pointers
    private final boolean[] rDown = new boolean[MAXID];
    private final int[] rOwner = new int[MAXID];
    private final boolean[] rFire = new boolean[MAXID];   // went down on the fire button
    private final float[] rx = new float[MAXID], ry = new float[MAXID];

    // synthetic pointers
    private final boolean[] sDown = new boolean[SYN_COUNT];
    private final float[] sx = new float[SYN_COUNT], sy = new float[SYN_COUNT];

    // what the game currently believes
    private final boolean[] gDown = new boolean[MAXID];
    private final float[] gx = new float[MAXID], gy = new float[MAXID];
    private int gCount;
    private long gDownTime;

    // scratch
    private final int[] ids = new int[MAXID];
    private final float[] xs = new float[MAXID], ys = new float[MAXID];

    // anchors for classification (px)
    private float fireX, fireY, fireR = 80, viewW = 1920, viewH = 1080;
    private boolean holdFire;

    public PointerRouter(Ui ui, Sink sink) { this.ui = ui; this.sink = sink; }

    public void setGeometry(float viewW, float viewH, float fireX, float fireY, float fireR) {
        this.viewW = viewW; this.viewH = viewH; this.fireX = fireX; this.fireY = fireY; this.fireR = fireR;
    }

    // ---------------- real events ----------------

    /** @param count pointers in the event; ids/xs/ys describe all of them; actionIndex only matters for POINTER_DOWN/UP */
    public void onReal(int action, int actionIndex, int count, int[] pid, float[] px, float[] py, long time) {
        switch (action) {
            case ACTION_DOWN:
            case ACTION_POINTER_DOWN: {
                int id = pid[actionIndex];
                if (id < 0 || id >= MAXID) return;
                for (int i = 0; i < count; i++) if (pid[i] >= 0 && pid[i] < MAXID) { rx[pid[i]] = px[i]; ry[pid[i]] = py[i]; }
                rDown[id] = true;
                rFire[id] = onFire(id);
                if (ui.hit(px[actionIndex], py[actionIndex])) {
                    rOwner[id] = OWN_UI;
                    ui.down(id, px[actionIndex], py[actionIndex]);
                } else rOwner[id] = OWN_GAME;
                break;
            }
            case ACTION_MOVE:
                for (int i = 0; i < count; i++) {
                    int id = pid[i];
                    if (id < 0 || id >= MAXID || !rDown[id]) continue;
                    rx[id] = px[i]; ry[id] = py[i];
                    if (rOwner[id] == OWN_UI) ui.move(id, px[i], py[i]);
                }
                break;
            case ACTION_POINTER_UP:
            case ACTION_UP: {
                int idx = action == ACTION_UP ? 0 : actionIndex;
                int id = pid[idx];
                if (id < 0 || id >= MAXID) return;
                rx[id] = px[idx]; ry[id] = py[idx];
                if (rOwner[id] == OWN_UI) ui.up(id, px[idx], py[idx], false);
                rDown[id] = false;
                rOwner[id] = OWN_NONE;
                if (action == ACTION_UP) releaseAllReal();
                break;
            }
            case ACTION_CANCEL:
                for (int id = 0; id < MAXID; id++) if (rDown[id] && rOwner[id] == OWN_UI) ui.up(id, rx[id], ry[id], true);
                releaseAllReal();
                break;
            default:
                return;
        }
        sync(time, action == ACTION_CANCEL);
    }

    private void releaseAllReal() {
        for (int id = 0; id < MAXID; id++) {
            if (rDown[id] && rOwner[id] == OWN_UI) ui.up(id, rx[id], ry[id], false);
            rDown[id] = false;
            rOwner[id] = OWN_NONE;
        }
    }

    // ---------------- synthetic fingers ----------------

    public void setSynthetic(int slot, boolean down, float x, float y) {
        sDown[slot] = down; sx[slot] = x; sy[slot] = y;
    }

    public boolean syntheticDown(int slot) { return sDown[slot]; }

    /** silent aim: keep real fire presses from the game (applied on the next sync / flush) */
    public void setHoldFire(boolean hold) { holdFire = hold; }

    public boolean holdFire() { return holdFire; }

    /** apply synthetic changes; call once per frame after setSynthetic */
    public void flush(long time) { sync(time, false); }

    // ---------------- classification for the native side ----------------

    private boolean onFire(int id) {
        float dx = rx[id] - fireX, dy = ry[id] - fireY;
        return dx * dx + dy * dy <= fireR * fireR;
    }

    public boolean userFiring() {
        for (int id = 0; id < MAXID; id++) if (rDown[id] && rOwner[id] == OWN_GAME && onFire(id)) return true;
        return false;
    }

    public boolean userLooking() {
        for (int id = 0; id < MAXID; id++) if (rDown[id] && rOwner[id] == OWN_GAME && rx[id] > viewW * 0.38f && !onFire(id)) return true;
        return false;
    }

    public int gamePointers() { return gCount; }

    // ---------------- core ----------------

    private void sync(long time, boolean cancel) {
        // desired pointer set for the game
        boolean[] want = new boolean[MAXID];
        float[] wx = new float[MAXID], wy = new float[MAXID];
        for (int id = 0; id < MAXID; id++) {
            if (rDown[id] && rOwner[id] == OWN_GAME && !(holdFire && rFire[id])) { want[id] = true; wx[id] = rx[id]; wy[id] = ry[id]; }
        }
        for (int k = 0; k < SYN_COUNT; k++) if (sDown[k]) { int id = SYN_ID_BASE + k; want[id] = true; wx[id] = sx[k]; wy[id] = sy[k]; }

        if (cancel && gCount > 0) {
            int n = collect(gDown, gx, gy);
            sink.emit(ACTION_CANCEL, 0, n, ids, xs, ys, gDownTime, time);
            for (int id = 0; id < MAXID; id++) gDown[id] = false;
            gCount = 0;
            for (int id = 0; id < MAXID; id++) if (want[id]) { /* re-press next sync */ }
        }

        // 1) releases
        for (int id = 0; id < MAXID; id++) {
            if (gDown[id] && !want[id]) {
                int n = collect(gDown, gx, gy);
                int idx = indexOf(n, id);
                // report the last known coordinates of the finger that leaves
                sink.emit(gCount == 1 ? ACTION_UP : ACTION_POINTER_UP, idx, n, ids, xs, ys, gDownTime, time);
                gDown[id] = false;
                gCount--;
            }
        }
        // 2) presses
        for (int id = 0; id < MAXID; id++) {
            if (!gDown[id] && want[id]) {
                gx[id] = wx[id]; gy[id] = wy[id];
                gDown[id] = true;
                if (gCount == 0) gDownTime = time;
                gCount++;
                int n = collect(gDown, gx, gy);
                int idx = indexOf(n, id);
                sink.emit(gCount == 1 ? ACTION_DOWN : ACTION_POINTER_DOWN, idx, n, ids, xs, ys, gDownTime, time);
            }
        }
        // 3) movement
        boolean moved = false;
        for (int id = 0; id < MAXID; id++) {
            if (gDown[id] && (gx[id] != wx[id] || gy[id] != wy[id])) { gx[id] = wx[id]; gy[id] = wy[id]; moved = true; }
        }
        if (moved && gCount > 0) {
            int n = collect(gDown, gx, gy);
            sink.emit(ACTION_MOVE, 0, n, ids, xs, ys, gDownTime, time);
        }
    }

    private int collect(boolean[] down, float[] x, float[] y) {
        int n = 0;
        for (int id = 0; id < MAXID; id++) if (down[id]) { ids[n] = id; xs[n] = x[id]; ys[n] = y[id]; n++; }
        return n;
    }

    private int indexOf(int n, int id) {
        for (int i = 0; i < n; i++) if (ids[i] == id) return i;
        return 0;
    }
}
