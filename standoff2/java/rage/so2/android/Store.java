package rage.so2.android;

import android.content.Context;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

/** Config files in the app's private storage (nothing needs a permission). */
final class Store {
    private final File dir;

    Store(Context ctx) {
        dir = new File(ctx.getFilesDir(), "ragemod");
        dir.mkdirs();
    }

    String read(String name) {
        File f = new File(dir, name);
        if (!f.isFile()) return null;
        try (FileInputStream in = new FileInputStream(f)) {
            ByteArrayOutputStream bo = new ByteArrayOutputStream();
            byte[] buf = new byte[4096];
            int n;
            while ((n = in.read(buf)) > 0) bo.write(buf, 0, n);
            return bo.toString("UTF-8");
        } catch (IOException e) {
            return null;
        }
    }

    boolean write(String name, String text) {
        File f = new File(dir, name + ".tmp");
        try (FileOutputStream out = new FileOutputStream(f)) {
            out.write(text.getBytes("UTF-8"));
        } catch (IOException e) {
            return false;
        }
        return f.renameTo(new File(dir, name)) || new File(dir, name).exists();
    }
}
