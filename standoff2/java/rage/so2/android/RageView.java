package rage.so2.android;

import android.content.Context;
import android.graphics.Canvas;
import android.view.View;

/** Transparent full-screen canvas on top of the game. Touches never reach it (they are routed in {@link TouchRouter}). */
final class RageView extends View {
    private final Overlay overlay;

    RageView(Context ctx, Overlay overlay) {
        super(ctx);
        this.overlay = overlay;
        setWillNotDraw(false);
        setFocusable(false);
        setClickable(false);
        setLayerType(LAYER_TYPE_HARDWARE, null);
    }

    @Override protected void onDraw(Canvas canvas) {
        overlay.draw(canvas, getWidth(), getHeight());
    }
}
