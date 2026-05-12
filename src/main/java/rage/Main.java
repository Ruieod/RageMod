package rage;
import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.fabricmc.fabric.api.client.keybinding.v1.KeyBindingHelper;
import net.minecraft.client.option.KeyBinding;
import net.minecraft.client.util.InputUtil;
import org.lwjgl.glfw.GLFW;
import rage.module.KillAura;
import rage.ui.ClickGUI;

public class Main implements ClientModInitializer {
    private static KeyBinding guiKey;
    private static KeyBinding toggleKey;

    public void onInitializeClient() {
        guiKey = KeyBindingHelper.registerKeyBinding(new KeyBinding("GUI", InputUtil.Type.KEYSYM, GLFW.GLFW_KEY_LEFT_SHIFT, "RAGE"));
        toggleKey = KeyBindingHelper.registerKeyBinding(new KeyBinding("Toggle", InputUtil.Type.KEYSYM, GLFW.GLFW_KEY_R, "RAGE"));

        ClientTickEvents.END_CLIENT_TICK.register(client -> {
            while (guiKey.wasPressed()) ClickGUI.open();
            while (toggleKey.wasPressed()) KillAura.enabled = !KillAura.enabled;
            KillAura.onTick();
        });
    }
}