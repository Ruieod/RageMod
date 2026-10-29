import rage.so2.ui.PointerRouter;

import java.util.ArrayList;
import java.util.List;

public class RouterTest {
    static final List<String> log = new ArrayList<>();
    static boolean uiHit;
    static int uiDown, uiUp;

    static String name(int a) {
        switch (a) { case 0: return "DOWN"; case 1: return "UP"; case 2: return "MOVE"; case 3: return "CANCEL"; case 5: return "PDOWN"; case 6: return "PUP"; default: return "?" + a; }
    }

    public static void main(String[] args) {
        PointerRouter r = new PointerRouter(new PointerRouter.Ui() {
            public boolean hit(float x, float y) { return x < 100 && y < 100; }
            public void down(int id, float x, float y) { uiDown++; }
            public void move(int id, float x, float y) {}
            public void up(int id, float x, float y, boolean c) { uiUp++; }
        }, (action, idx, n, ids, xs, ys, dt, t) -> {
            StringBuilder sb = new StringBuilder(name(action)).append('[').append(idx).append("]");
            for (int i = 0; i < n; i++) sb.append(' ').append(ids[i]).append('@').append((int) xs[i]).append(',').append((int) ys[i]);
            log.add(sb.toString());
        });
        r.setGeometry(2000, 1000, 1700, 700, 90);

        // joystick finger (id 0) goes down in the game
        r.onReal(PointerRouter.ACTION_DOWN, 0, 1, new int[]{0}, new float[]{300}, new float[]{700}, 10);
        // synthetic look finger joins
        r.setSynthetic(PointerRouter.SYN_LOOK, true, 1300, 300);
        r.flush(20);
        r.setSynthetic(PointerRouter.SYN_LOOK, true, 1340, 310);
        r.flush(30);
        // real joystick moves -> game sees both
        r.onReal(PointerRouter.ACTION_MOVE, 0, 1, new int[]{0}, new float[]{320}, new float[]{690}, 40);
        // fire finger
        r.setSynthetic(PointerRouter.SYN_FIRE, true, 1700, 700);
        r.flush(50);
        // second real finger on the UI (menu button) must not reach the game
        r.onReal(PointerRouter.ACTION_POINTER_DOWN, 1, 2, new int[]{0, 1}, new float[]{320, 50}, new float[]{690, 50}, 60);
        r.onReal(PointerRouter.ACTION_POINTER_UP, 1, 2, new int[]{0, 1}, new float[]{320, 50}, new float[]{690, 50}, 70);
        r.setSynthetic(PointerRouter.SYN_FIRE, false, 0, 0);
        r.setSynthetic(PointerRouter.SYN_LOOK, false, 0, 0);
        r.flush(80);
        // real finger lifts
        r.onReal(PointerRouter.ACTION_UP, 0, 1, new int[]{0}, new float[]{320}, new float[]{690}, 90);

        for (String l : log) System.out.println(l);
        String[] expect = {
            "DOWN[0] 0@300,700",
            "PDOWN[1] 0@300,700 24@1300,300",
            "MOVE[0] 0@300,700 24@1340,310",
            "MOVE[0] 0@320,690 24@1340,310",
            "PDOWN[2] 0@320,690 24@1340,310 25@1700,700",
            "PUP[1] 0@320,690 24@1340,310 25@1700,700",
            "PUP[1] 0@320,690 25@1700,700",
            "UP[0] 0@320,690",
        };
        if (log.size() != expect.length) throw new AssertionError("event count " + log.size() + " != " + expect.length);
        for (int i = 0; i < expect.length; i++) if (!log.get(i).equals(expect[i])) throw new AssertionError("event " + i + ": " + log.get(i) + " != " + expect[i]);
        if (uiDown != 1 || uiUp != 1) throw new AssertionError("ui down/up " + uiDown + "/" + uiUp);
        if (r.userFiring()) throw new AssertionError("userFiring should be false");

        // user firing detection
        r.onReal(PointerRouter.ACTION_DOWN, 0, 1, new int[]{3}, new float[]{1710}, new float[]{690}, 100);
        if (!r.userFiring()) throw new AssertionError("userFiring should be true on the fire button");
        r.onReal(PointerRouter.ACTION_UP, 0, 1, new int[]{3}, new float[]{1710}, new float[]{690}, 110);
        System.out.println("router tests OK");
    }
}
