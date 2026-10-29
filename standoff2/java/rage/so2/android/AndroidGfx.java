package rage.so2.android;

import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RadialGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;

import rage.so2.ui.Gfx;

/** Gfx on top of android.graphics.Canvas. */
public final class AndroidGfx implements Gfx {
    private Canvas c;
    private float w, h;
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint txt = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rf = new RectF();
    private final Path path = new Path();
    private final float[] aStack = new float[64];
    private int aTop;
    private float aMul = 1f;
    private Typeface regular = Typeface.create("sans-serif", Typeface.NORMAL);
    private Typeface bold = Typeface.create("sans-serif-medium", Typeface.BOLD);

    public AndroidGfx() {
        stroke.setStyle(Paint.Style.STROKE);
        stroke.setStrokeCap(Paint.Cap.ROUND);
        txt.setSubpixelText(true);
    }

    public void begin(Canvas canvas, float width, float height) {
        c = canvas; w = width; h = height; aTop = 0; aMul = 1f;
    }

    @Override public float width() { return w; }
    @Override public float height() { return h; }

    private int a(int argb) {
        if (aMul >= 0.999f) return argb;
        int al = Math.round(((argb >>> 24) & 0xFF) * aMul);
        return (al << 24) | (argb & 0xFFFFFF);
    }

    @Override public void fillRect(float x, float y, float ww, float hh, int argb) {
        fill.setShader(null); fill.setColor(a(argb));
        c.drawRect(x, y, x + ww, y + hh, fill);
    }

    @Override public void fillRound(float x, float y, float ww, float hh, float r, int argb) {
        fill.setShader(null); fill.setColor(a(argb));
        rf.set(x, y, x + ww, y + hh);
        c.drawRoundRect(rf, r, r, fill);
    }

    @Override public void strokeRound(float x, float y, float ww, float hh, float r, float sw, int argb) {
        stroke.setShader(null); stroke.setColor(a(argb)); stroke.setStrokeWidth(sw);
        rf.set(x, y, x + ww, y + hh);
        if (r <= 0) c.drawRect(rf, stroke); else c.drawRoundRect(rf, r, r, stroke);
    }

    @Override public void gradRound(float x, float y, float ww, float hh, float r, int ca, int cb, boolean vertical) {
        fill.setShader(vertical ? new LinearGradient(x, y, x, y + hh, a(ca), a(cb), Shader.TileMode.CLAMP)
                : new LinearGradient(x, y, x + ww, y, a(ca), a(cb), Shader.TileMode.CLAMP));
        fill.setColor(0xFFFFFFFF);
        rf.set(x, y, x + ww, y + hh);
        if (r <= 0) c.drawRect(rf, fill); else c.drawRoundRect(rf, r, r, fill);
        fill.setShader(null);
    }

    @Override public void shadow(float x, float y, float ww, float hh, float r, float blur, int argb) {
        int base = (argb >>> 24) & 0xFF;
        final int steps = 6;
        fill.setShader(null);
        for (int i = steps; i >= 1; i--) {
            float t = i / (float) steps;
            float grow = blur * t;
            float al = base * aMul * (1f - t) * (1f - t) * 0.55f + base * aMul * 0.04f;
            fill.setColor(((int) Math.min(255, al) << 24) | (argb & 0xFFFFFF));
            rf.set(x - grow, y - grow * 0.6f, x + ww + grow, y + hh + grow * 1.2f);
            c.drawRoundRect(rf, r + grow, r + grow, fill);
        }
    }

    @Override public void line(float x1, float y1, float x2, float y2, float sw, int argb) {
        stroke.setShader(null); stroke.setColor(a(argb)); stroke.setStrokeWidth(sw);
        c.drawLine(x1, y1, x2, y2, stroke);
    }

    @Override public void circle(float cx, float cy, float r, int argb) {
        fill.setShader(null); fill.setColor(a(argb));
        c.drawCircle(cx, cy, r, fill);
    }

    @Override public void ring(float cx, float cy, float r, float sw, int argb) {
        stroke.setShader(null); stroke.setColor(a(argb)); stroke.setStrokeWidth(sw);
        c.drawCircle(cx, cy, r, stroke);
    }

    @Override public void poly(float[] xy, int n, int argb) {
        if (n < 3) return;
        path.rewind();
        path.moveTo(xy[0], xy[1]);
        for (int i = 1; i < n; i++) path.lineTo(xy[i * 2], xy[i * 2 + 1]);
        path.close();
        fill.setShader(null); fill.setColor(a(argb));
        c.drawPath(path, fill);
    }

    @Override public void radial(float cx, float cy, float r, int centre, int edge) {
        fill.setShader(new RadialGradient(cx, cy, Math.max(1f, r), a(centre), a(edge), Shader.TileMode.CLAMP));
        fill.setColor(0xFFFFFFFF);
        c.drawCircle(cx, cy, r, fill);
        fill.setShader(null);
    }

    private void prep(float size, boolean b) {
        txt.setTextSize(size);
        txt.setTypeface(b ? bold : regular);
    }

    @Override public void text(String s, float x, float y, float size, int argb, int align, boolean b) {
        prep(size, b);
        txt.setColor(a(argb));
        txt.setTextAlign(align == CENTER ? Paint.Align.CENTER : (align == RIGHT ? Paint.Align.RIGHT : Paint.Align.LEFT));
        Paint.FontMetrics fm = txt.getFontMetrics();
        c.drawText(s, x, y - (fm.ascent + fm.descent) / 2f, txt);
    }

    @Override public float textW(String s, float size, boolean b) {
        prep(size, b);
        return txt.measureText(s);
    }

    @Override public void clip(float x, float y, float ww, float hh) { c.save(); c.clipRect(x, y, x + ww, y + hh); }
    @Override public void unclip() { c.restore(); }
    @Override public void save() { c.save(); if (aTop < aStack.length) aStack[aTop++] = aMul; }
    @Override public void restore() { c.restore(); if (aTop > 0) aMul = aStack[--aTop]; }
    @Override public void translate(float dx, float dy) { c.translate(dx, dy); }
    @Override public void scale(float sx, float sy, float px, float py) { c.scale(sx, sy, px, py); }
    @Override public void alpha(float al) { aMul *= al; }
}
