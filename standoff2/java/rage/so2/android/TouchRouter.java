package rage.so2.android;

import android.os.SystemClock;
import android.view.ActionMode;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.KeyboardShortcutGroup;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SearchEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;

import java.util.List;

import rage.so2.ui.PointerRouter;

/** Window.Callback wrapper: every touch passes through {@link PointerRouter} before the game sees it. */
public final class TouchRouter implements Window.Callback, PointerRouter.Sink {
    public Window.Callback base;
    public PointerRouter router;

    private final int[] ids = new int[32];
    private final float[] xs = new float[32], ys = new float[32];
    private int deviceId = 0, source = InputDevice.SOURCE_TOUCHSCREEN;
    private final MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[32];
    private final MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[32];

    public TouchRouter(Window.Callback base) {
        this.base = base;
        for (int i = 0; i < 32; i++) { props[i] = new MotionEvent.PointerProperties(); coords[i] = new MotionEvent.PointerCoords(); }
        // pick a real touchscreen device id for events we create before the first real touch
        try {
            for (int id : InputDevice.getDeviceIds()) {
                InputDevice d = InputDevice.getDevice(id);
                if (d != null && (d.getSources() & InputDevice.SOURCE_TOUCHSCREEN) == InputDevice.SOURCE_TOUCHSCREEN) { deviceId = id; break; }
            }
        } catch (Throwable ignored) { }
    }

    // ---- touches ----
    @Override public boolean dispatchTouchEvent(MotionEvent ev) {
        if (router == null || (ev.getSource() & InputDevice.SOURCE_TOUCHSCREEN) != InputDevice.SOURCE_TOUCHSCREEN) return base.dispatchTouchEvent(ev);
        int n = Math.min(32, ev.getPointerCount());
        for (int i = 0; i < n; i++) { ids[i] = ev.getPointerId(i); xs[i] = ev.getX(i); ys[i] = ev.getY(i); }
        deviceId = ev.getDeviceId();
        source = ev.getSource();
        try {
            router.onReal(ev.getActionMasked(), ev.getActionIndex(), n, ids, xs, ys, ev.getEventTime());
        } catch (Throwable t) {
            return base.dispatchTouchEvent(ev);   // never break the game's input because of us
        }
        return true;
    }

    /** emits to the game */
    @Override public void emit(int action, int actionIndex, int count, int[] pid, float[] px, float[] py, long downTime, long eventTime) {
        for (int i = 0; i < count; i++) {
            props[i].clear();
            props[i].id = pid[i];
            props[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
            coords[i].clear();
            coords[i].x = px[i];
            coords[i].y = py[i];
            coords[i].pressure = 1f;
            coords[i].size = 1f;
        }
        int act = action;
        if (action == MotionEvent.ACTION_POINTER_DOWN || action == MotionEvent.ACTION_POINTER_UP) act |= (actionIndex << MotionEvent.ACTION_POINTER_INDEX_SHIFT);
        long now = Math.max(eventTime, SystemClock.uptimeMillis());
        MotionEvent ev = MotionEvent.obtain(downTime, now, act, count, props, coords, 0, 0, 1f, 1f, deviceId, 0, source, 0);
        try { base.dispatchTouchEvent(ev); } finally { ev.recycle(); }
    }

    // ---- everything else is forwarded untouched ----
    @Override public boolean dispatchKeyEvent(KeyEvent e) { return base.dispatchKeyEvent(e); }
    @Override public boolean dispatchKeyShortcutEvent(KeyEvent e) { return base.dispatchKeyShortcutEvent(e); }
    @Override public boolean dispatchTrackballEvent(MotionEvent e) { return base.dispatchTrackballEvent(e); }
    @Override public boolean dispatchGenericMotionEvent(MotionEvent e) { return base.dispatchGenericMotionEvent(e); }
    @Override public boolean dispatchPopulateAccessibilityEvent(AccessibilityEvent e) { return base.dispatchPopulateAccessibilityEvent(e); }
    @Override public View onCreatePanelView(int featureId) { return base.onCreatePanelView(featureId); }
    @Override public boolean onCreatePanelMenu(int featureId, Menu menu) { return base.onCreatePanelMenu(featureId, menu); }
    @Override public boolean onPreparePanel(int featureId, View view, Menu menu) { return base.onPreparePanel(featureId, view, menu); }
    @Override public boolean onMenuOpened(int featureId, Menu menu) { return base.onMenuOpened(featureId, menu); }
    @Override public boolean onMenuItemSelected(int featureId, MenuItem item) { return base.onMenuItemSelected(featureId, item); }
    @Override public void onWindowAttributesChanged(WindowManager.LayoutParams attrs) { base.onWindowAttributesChanged(attrs); }
    @Override public void onContentChanged() { base.onContentChanged(); }
    @Override public void onWindowFocusChanged(boolean hasFocus) { base.onWindowFocusChanged(hasFocus); }
    @Override public void onAttachedToWindow() { base.onAttachedToWindow(); }
    @Override public void onDetachedFromWindow() { base.onDetachedFromWindow(); }
    @Override public void onPanelClosed(int featureId, Menu menu) { base.onPanelClosed(featureId, menu); }
    @Override public boolean onSearchRequested() { return base.onSearchRequested(); }
    @Override public boolean onSearchRequested(SearchEvent searchEvent) { return base.onSearchRequested(searchEvent); }
    @Override public ActionMode onWindowStartingActionMode(ActionMode.Callback callback) { return base.onWindowStartingActionMode(callback); }
    @Override public ActionMode onWindowStartingActionMode(ActionMode.Callback callback, int type) { return base.onWindowStartingActionMode(callback, type); }
    @Override public void onActionModeStarted(ActionMode mode) { base.onActionModeStarted(mode); }
    @Override public void onActionModeFinished(ActionMode mode) { base.onActionModeFinished(mode); }
    @Override public void onProvideKeyboardShortcuts(List<KeyboardShortcutGroup> data, Menu menu, int deviceId) { base.onProvideKeyboardShortcuts(data, menu, deviceId); }
    @Override public void onPointerCaptureChanged(boolean hasCapture) { base.onPointerCaptureChanged(hasCapture); }
}
