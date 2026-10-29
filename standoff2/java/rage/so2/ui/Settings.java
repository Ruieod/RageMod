package rage.so2.ui;

import rage.so2.gen.Cfg;

/** The whole configuration as one float[] (layout generated from config/schema.json), plus (de)serialisation. */
public final class Settings {
    public final float[] v = Cfg.defaults();
    public long rev = 1;

    public float get(int slot) { return v[slot]; }
    public boolean on(int slot) { return v[slot] > 0.5f; }

    public void set(int slot, float x) {
        if (v[slot] != x) { v[slot] = x; rev++; }
    }

    public int color(int slot) { return Theme.rgba(v[slot], v[slot + 1], v[slot + 2], v[slot + 3]); }

    public void setColor(int slot, int argb) {
        v[slot] = ((argb >> 16) & 0xFF) / 255f;
        v[slot + 1] = ((argb >> 8) & 0xFF) / 255f;
        v[slot + 2] = (argb & 0xFF) / 255f;
        v[slot + 3] = ((argb >>> 24) & 0xFF) / 255f;
        rev++;
    }

    public void resetAll() {
        float[] d = Cfg.defaults();
        // keep touch calibration anchors, they are per device layout
        float fx = v[Cfg.CAL_FX], fy = v[Cfg.CAL_FY], lx = v[Cfg.CAL_LX], ly = v[Cfg.CAL_LY];
        System.arraycopy(d, 0, v, 0, v.length);
        v[Cfg.CAL_FX] = fx; v[Cfg.CAL_FY] = fy; v[Cfg.CAL_LX] = lx; v[Cfg.CAL_LY] = ly;
        rev++;
    }

    public void copyFrom(Settings o) {
        System.arraycopy(o.v, 0, v, 0, v.length);
        rev++;
    }

    // ---- text format: "id=value" lines; weapon overrides as "w.<weapon>.<id>=value" and "w.<weapon>.override=1" ----
    public String serialize() {
        StringBuilder sb = new StringBuilder("ragemod-cfg 1\n");
        for (Cfg.Item it : Cfg.ITEMS) {
            if (it.type == Cfg.T_COLOR) {
                sb.append(it.id).append('=').append(v[it.slot]).append(',').append(v[it.slot + 1]).append(',').append(v[it.slot + 2]).append(',').append(v[it.slot + 3]).append('\n');
            } else {
                sb.append(it.id).append('=').append(v[it.slot]).append('\n');
            }
        }
        for (int w = 0; w < Cfg.W_COUNT; w++) {
            boolean ov = v[Cfg.wFlag(w)] > 0.5f;
            if (!ov) continue;
            sb.append("w.").append(Cfg.W_IDS[w]).append(".override=1\n");
            for (Cfg.Item it : Cfg.ITEMS) {
                if (it.widx >= 0) sb.append("w.").append(Cfg.W_IDS[w]).append('.').append(it.id).append('=').append(v[Cfg.wSlot(w, it)]).append('\n');
            }
        }
        return sb.toString();
    }

    /** @return number of values applied, -1 if the text is not a config */
    public int parse(String text) {
        if (text == null) return -1;
        String[] lines = text.split("\\r?\\n");
        if (lines.length == 0 || !lines[0].startsWith("ragemod-cfg")) return -1;
        int n = 0;
        for (int i = 1; i < lines.length; i++) {
            String ln = lines[i].trim();
            int eq = ln.indexOf('=');
            if (eq <= 0) continue;
            String key = ln.substring(0, eq), val = ln.substring(eq + 1);
            try {
                if (key.startsWith("w.")) {
                    int d1 = key.indexOf('.', 2);
                    if (d1 < 0) continue;
                    String wid = key.substring(2, d1), rest = key.substring(d1 + 1);
                    int w = indexOf(Cfg.W_IDS, wid);
                    if (w < 0) continue;
                    if (rest.equals("override")) { v[Cfg.wFlag(w)] = Float.parseFloat(val); n++; continue; }
                    for (Cfg.Item it : Cfg.ITEMS) if (it.widx >= 0 && it.id.equals(rest)) { v[Cfg.wSlot(w, it)] = clampItem(it, Float.parseFloat(val)); n++; break; }
                } else {
                    for (Cfg.Item it : Cfg.ITEMS) {
                        if (!it.id.equals(key)) continue;
                        if (it.type == Cfg.T_COLOR) {
                            String[] p = val.split(",");
                            if (p.length == 4) for (int k = 0; k < 4; k++) v[it.slot + k] = Anim.clamp01(Float.parseFloat(p[k]));
                        } else v[it.slot] = clampItem(it, Float.parseFloat(val));
                        n++;
                        break;
                    }
                }
            } catch (NumberFormatException ignored) {}
        }
        rev++;
        return n;
    }

    static float clampItem(Cfg.Item it, float x) {
        if (it.type == Cfg.T_TOGGLE) return x > 0.5f ? 1f : 0f;
        if (it.type == Cfg.T_COMBO) return Math.max(0, Math.min(it.opts.length - 1, Math.round(x)));
        if (it.type == Cfg.T_SLIDER) return Anim.clamp(x, it.min, it.max);
        return x;
    }

    static int indexOf(String[] a, String s) {
        for (int i = 0; i < a.length; i++) if (a[i].equals(s)) return i;
        return -1;
    }
}
