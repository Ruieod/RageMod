package rage.so2.ui;

/** Java view of the float block published by native/src/frame.h. Keep the constants in sync with that header. */
public final class FrameData {
    public static final int MAX_PLAYERS = 16, JOINTS = 16, HEADER = 24, STRIDE = 52, TOTAL = HEADER + MAX_PLAYERS * STRIDE;

    public static final int H_SEQ = 0, H_W = 1, H_H = 2, H_COUNT = 3, H_FLAGS = 4, H_FOVPX = 5, H_CX = 6, H_CY = 7, H_TARGET = 8,
            H_KILLSEQ = 9, H_KILLX = 10, H_KILLY = 11, H_KILLHS = 12, H_LOCALTEAM = 13, H_WEAPON = 14, H_GAINX = 15, H_GAINY = 16,
            H_LAG = 17, H_TICKMS = 18, H_CHARS = 19, H_ZONES = 20;
    public static final int P_ID = 0, P_FLAGS = 1, P_DIST = 2, P_WEAPON = 3, P_BX1 = 4, P_BY1 = 5, P_BX2 = 6, P_BY2 = 7, P_HX = 8, P_HY = 9,
            P_J = 10, P_GX1 = 42, P_GY1 = 43, P_GX2 = 44, P_GY2 = 45, P_AX = 46, P_AY = 47, P_TEAM = 48, P_HP = 49;
    public static final int PF_ENEMY = 1, PF_VISIBLE = 2, PF_TARGET = 4, PF_BOX = 8, PF_TEAMKNOWN = 16, PF_GHOST = 32, PF_OFFSCREEN = 64;
    public static final int HF_CAMERA = 1, HF_TICK = 2, HF_LOCAL = 4, HF_ENGAGED = 8, HF_FIRE = 16;

    public final float[] f = new float[TOTAL];
    public long receivedAtMs;

    public int seq() { return (int) f[H_SEQ]; }
    public int count() { return Math.max(0, Math.min(MAX_PLAYERS, (int) f[H_COUNT])); }
    public float camW() { return f[H_W]; }
    public float camH() { return f[H_H]; }
    public int flags() { return (int) f[H_FLAGS]; }
    public boolean has(int flag) { return (flags() & flag) != 0; }
    public int base(int i) { return HEADER + i * STRIDE; }
    public float p(int i, int off) { return f[base(i) + off]; }
    public int pflags(int i) { return (int) f[base(i) + P_FLAGS]; }
    public boolean valid() { return f[H_SEQ] > 0 && f[H_W] > 0 && f[H_H] > 0; }
}
