package rage.so2;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Map;

import rage.so2.android.Overlay;

/** Entry point called from native (jni_bridge.cpp). Waits for the game activity and installs the overlay on it. */
public final class Boot {
    private static final String TAG = "RageMod";
    private static boolean started;

    private Boot() {}

    public static synchronized void start() {
        if (started) return;
        started = true;
        Thread t = new Thread(new Runnable() {
            @Override public void run() { loop(); }
        }, "rage-boot");
        t.setDaemon(true);
        t.start();
    }

    private static void loop() {
        final Handler main = new Handler(Looper.getMainLooper());
        Activity installed = null;
        while (true) {
            try {
                final Activity a = findActivity();
                if (a != null && a != installed && !a.isFinishing()) {
                    installed = a;
                    main.post(new Runnable() {
                        @Override public void run() {
                            try { Overlay.install(a); } catch (Throwable t) { Log.e(TAG, "overlay install failed", t); }
                        }
                    });
                }
                Thread.sleep(installed == null ? 400 : 1500);
            } catch (InterruptedException e) {
                return;
            } catch (Throwable t) {
                Log.e(TAG, "boot loop", t);
                try { Thread.sleep(2000); } catch (InterruptedException e) { return; }
            }
        }
    }

    private static Activity findActivity() {
        try {
            Class<?> c = Class.forName("com.unity3d.player.UnityPlayer");
            Field f = c.getField("currentActivity");
            Object a = f.get(null);
            if (a instanceof Activity) return (Activity) a;
        } catch (Throwable ignored) { }
        try {
            Class<?> at = Class.forName("android.app.ActivityThread");
            Object cur = at.getMethod("currentActivityThread").invoke(null);
            Field af = at.getDeclaredField("mActivities");
            af.setAccessible(true);
            Map<?, ?> m = (Map<?, ?>) af.get(cur);
            for (Object rec : m.values()) {
                Class<?> rc = rec.getClass();
                Field pf = rc.getDeclaredField("paused");
                pf.setAccessible(true);
                if (pf.getBoolean(rec)) continue;
                Field actf = rc.getDeclaredField("activity");
                actf.setAccessible(true);
                return (Activity) actf.get(rec);
            }
        } catch (Throwable ignored) { }
        return null;
    }
}
