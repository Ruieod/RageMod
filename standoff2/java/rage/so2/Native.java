package rage.so2;

/** JNI surface implemented by libragemod.so (registered from jni_bridge.cpp). */
public final class Native {
    private Native() {}

    public static native void setConfig(float[] v);
    /** copies the latest frame block, returns its sequence number (0 = nothing published yet) */
    public static native int readFrame(float[] out);
    /** out: [seq, lookActive, dx, dy, fireHeld, vibrate, holdFire, 0]; dx/dy are accumulated since the previous poll */
    public static native void pollCmd(float[] out);
    public static native void setInput(float userFiring, float userLooking, float viewW, float viewH, float fps, float menuOpen);
    public static native String status();
    public static native String log(int lines);
    public static native String toolName(int id);
}
