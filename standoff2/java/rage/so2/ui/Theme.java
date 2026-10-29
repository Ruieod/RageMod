package rage.so2.ui;

/** Colours derived from the accent. Dark navy palette in the spirit of Neverlose. */
public final class Theme {
    public int accent = 0xFF3AA0FF;
    public int accentDim, accentGlow, accentText;
    public int bg = 0xFF0D131D, side = 0xFF0A0F17, card = 0xFF121A27, cardHi = 0xFF172234;
    public int stroke = 0xFF1C283A, strokeHi = 0xFF2A3a52;
    public int text = 0xFFE8EEF8, dim = 0xFF8798B2, dim2 = 0xFF55657E;
    public int good = 0xFF4BE08A, bad = 0xFFFF5470, warn = 0xFFFFC857;
    public float opacity = 0.96f;

    public void setAccent(int argb) {
        accent = argb | 0xFF000000;
        accentDim = withAlpha(accent, 0.35f);
        accentGlow = withAlpha(accent, 0.18f);
        accentText = lighten(accent, 0.25f);
    }

    public static int withAlpha(int argb, float a) {
        int base = (argb >>> 24) & 0xFF;
        int na = Math.max(0, Math.min(255, Math.round(base * a)));
        return (na << 24) | (argb & 0xFFFFFF);
    }

    public static int rgba(float r, float g, float b, float a) {
        return (clamp8(a) << 24) | (clamp8(r) << 16) | (clamp8(g) << 8) | clamp8(b);
    }

    public static int clamp8(float v) { return Math.max(0, Math.min(255, Math.round(v * 255f))); }

    public static int lighten(int argb, float t) {
        int r = (argb >> 16) & 0xFF, g = (argb >> 8) & 0xFF, b = argb & 0xFF;
        r += (255 - r) * t; g += (255 - g) * t; b += (255 - b) * t;
        return (argb & 0xFF000000) | (r << 16) | (g << 8) | b;
    }

    public static int mix(int a, int b, float t) {
        int aa = (a >>> 24) & 0xFF, ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
        int ba = (b >>> 24) & 0xFF, br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
        return (Math.round(aa + (ba - aa) * t) << 24) | (Math.round(ar + (br - ar) * t) << 16) | (Math.round(ag + (bg - ag) * t) << 8) | Math.round(ab + (bb - ab) * t);
    }

    public static float[] hsv(int argb) {
        float r = ((argb >> 16) & 0xFF) / 255f, g = ((argb >> 8) & 0xFF) / 255f, b = (argb & 0xFF) / 255f;
        float max = Math.max(r, Math.max(g, b)), min = Math.min(r, Math.min(g, b));
        float d = max - min, h = 0;
        if (d > 1e-6f) {
            if (max == r) h = ((g - b) / d) % 6f;
            else if (max == g) h = (b - r) / d + 2f;
            else h = (r - g) / d + 4f;
            h /= 6f;
            if (h < 0) h += 1f;
        }
        return new float[]{h, max <= 0 ? 0 : d / max, max};
    }

    public static int fromHsv(float h, float s, float v, float a) {
        h = (h - (float) Math.floor(h)) * 6f;
        int i = (int) h;
        float f = h - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
        float r, g, b;
        switch (i % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
        return rgba(r, g, b, a);
    }
}
