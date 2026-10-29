package rage.so2.ui;

/** Drawing surface. Two implementations: Android Canvas (device) and Java2D (desktop preview used for design review). */
public interface Gfx {
    int LEFT = 0, CENTER = 1, RIGHT = 2;

    float width();
    float height();

    void fillRect(float x, float y, float w, float h, int argb);
    void fillRound(float x, float y, float w, float h, float r, int argb);
    void strokeRound(float x, float y, float w, float h, float r, float sw, int argb);
    void gradRound(float x, float y, float w, float h, float r, int argbA, int argbB, boolean vertical);
    void shadow(float x, float y, float w, float h, float r, float blur, int argb);
    void line(float x1, float y1, float x2, float y2, float sw, int argb);
    void circle(float cx, float cy, float r, int argb);
    void ring(float cx, float cy, float r, float sw, int argb);
    void poly(float[] xy, int n, int argb);
    void radial(float cx, float cy, float r, int argbCenter, int argbEdge);

    /** y is the vertical centre of the text line. */
    void text(String s, float x, float y, float size, int argb, int align, boolean bold);
    float textW(String s, float size, boolean bold);

    void clip(float x, float y, float w, float h);
    void unclip();
    void save();
    void restore();
    void translate(float dx, float dy);
    void scale(float sx, float sy, float px, float py);
    void alpha(float a);   // multiplies everything drawn until restore()
}
