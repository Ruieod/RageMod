package rage.so2.ui;

/** Tiny vector icons for the sidebar (drawn with Gfx primitives, so they stay crisp and need no assets). */
public final class Icons {
    private Icons() {}

    public static void draw(Gfx g, String name, float cx, float cy, float s, int col) {
        float sw = Math.max(1.4f, s * 0.085f);
        switch (name) {
            case "aim": {
                g.ring(cx, cy, s * 0.30f, sw, col);
                g.circle(cx, cy, s * 0.05f, col);
                float a = s * 0.12f, b = s * 0.48f;
                g.line(cx, cy - b, cx, cy - a - s * 0.06f, sw, col);
                g.line(cx, cy + b, cx, cy + a + s * 0.06f, sw, col);
                g.line(cx - b, cy, cx - a - s * 0.06f, cy, sw, col);
                g.line(cx + b, cy, cx + a + s * 0.06f, cy, sw, col);
                break;
            }
            case "weapon": {
                float x0 = cx - s * 0.5f, y0 = cy - s * 0.18f;
                float[] body = {x0, y0, x0 + s * 0.74f, y0, x0 + s * 0.74f, y0 + s * 0.12f, x0 + s * 1.0f, y0 + s * 0.12f, x0 + s * 1.0f, y0 + s * 0.28f,
                        x0 + s * 0.52f, y0 + s * 0.28f, x0 + s * 0.40f, y0 + s * 0.62f, x0 + s * 0.20f, y0 + s * 0.62f, x0 + s * 0.28f, y0 + s * 0.28f, x0, y0 + s * 0.28f};
                g.poly(body, 10, col);
                break;
            }
            case "eye": {
                float w = s * 0.5f, h = s * 0.26f;
                float[] top = new float[14 * 2];
                int n = 0;
                for (int i = 0; i <= 6; i++) { float t = i / 6f; top[n++] = cx - w + 2 * w * t; top[n++] = cy - (float) Math.sin(Math.PI * t) * h * 1.6f; }
                for (int i = 6; i >= 0; i--) { float t = i / 6f; top[n++] = cx - w + 2 * w * t; top[n++] = cy + (float) Math.sin(Math.PI * t) * h * 1.6f; }
                for (int i = 0; i < 13; i++) g.line(top[i * 2], top[i * 2 + 1], top[i * 2 + 2], top[i * 2 + 3], sw, col);
                g.line(top[26], top[27], top[0], top[1], sw, col);
                g.circle(cx, cy, s * 0.15f, col);
                break;
            }
            case "gear": {
                g.ring(cx, cy, s * 0.26f, sw * 1.2f, col);
                g.circle(cx, cy, s * 0.07f, col);
                for (int i = 0; i < 8; i++) {
                    double a = i * Math.PI / 4;
                    g.line(cx + (float) Math.cos(a) * s * 0.33f, cy + (float) Math.sin(a) * s * 0.33f, cx + (float) Math.cos(a) * s * 0.47f, cy + (float) Math.sin(a) * s * 0.47f, sw * 1.5f, col);
                }
                break;
            }
            case "disk": {
                float x0 = cx - s * 0.4f, y0 = cy - s * 0.42f;
                g.strokeRound(x0, y0, s * 0.8f, s * 0.84f, s * 0.08f, sw, col);
                g.fillRect(x0 + s * 0.14f, y0, s * 0.42f, s * 0.26f, col);
                g.fillRound(x0 + s * 0.14f, y0 + s * 0.50f, s * 0.52f, s * 0.34f, s * 0.04f, col);
                break;
            }
            default:
                g.circle(cx, cy, s * 0.3f, col);
        }
    }
}
