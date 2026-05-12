package rage.ui;
import net.minecraft.client.MinecraftClient;
import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.gui.screen.Screen;
import net.minecraft.text.Text;
import net.minecraft.util.math.MathHelper;
import rage.module.KillAura;
import java.awt.Color;

public class ClickGUI extends Screen {
    private static final MinecraftClient mc = MinecraftClient.getInstance();
    private int panelX = 10, panelY = 20, panelW = 180, panelH = 100;
    private boolean dragging = false;
    private int dragOffX, dragOffY;

    public ClickGUI() { super(Text.literal("RAGE")); }

    public boolean mouseClicked(double mx, double my, int btn) {
        if (btn == 0 && mx >= panelX && mx <= panelX + panelW && my >= panelY && my <= panelY + 20) {
            dragging = true; dragOffX = (int)mx - panelX; dragOffY = (int)my - panelY; return true;
        }
        if (btn == 0 && mx >= panelX + 5 && mx <= panelX + panelW - 5 && my >= panelY + 35 && my <= panelY + 55) {
            KillAura.enabled = !KillAura.enabled; return true;
        }
        if (btn == 0 && mx >= panelX + 5 && mx <= panelX + panelW - 5 && my >= panelY + 65 && my <= panelY + 85) {
            KillAura.range = KillAura.range == 3 ? 4.5 : KillAura.range == 4.5 ? 6 : 3; return true;
        }
        return super.mouseClicked(mx, my, btn);
    }

    public boolean mouseReleased(double mx, double my, int btn) { dragging = false; return super.mouseReleased(mx, my, btn); }
    public boolean mouseDragged(double mx, double my, int btn, double dx, double dy) {
        if (dragging) { panelX = MathHelper.clamp((int)mx - dragOffX, 0, width - panelW); panelY = MathHelper.clamp((int)my - dragOffY, 0, height - 20); return true; }
        return super.mouseDragged(mx, my, btn, dx, dy);
    }

    public void render(DrawContext ctx, int mx, int my, float delta) {
        this.renderDarkening(ctx);
        ctx.fill(panelX, panelY, panelX + panelW, panelY + 20, new Color(15, 15, 20, 240).getRGB());
        ctx.drawHorizontalLine(panelX, panelX + panelW, panelY + 19, new Color(255, 40, 40).getRGB());
        ctx.drawTextWithShadow(textRenderer, "⚡ RAGE CLIENT", panelX + panelW/2 - textRenderer.getWidth("⚡ RAGE CLIENT")/2, panelY + 6, new Color(255, 50, 50).getRGB());
        ctx.fill(panelX, panelY + 20, panelX + panelW, panelY + panelH, new Color(20, 20, 28, 235).getRGB());
        ctx.fill(panelX, panelY + panelH, panelX + panelW, panelY + panelH + 2, new Color(255, 40, 40, 200).getRGB());

        int by = panelY + 35;
        boolean hKA = mx >= panelX + 5 && mx <= panelX + panelW - 5 && my >= by && my <= by + 20;
        ctx.fill(panelX + 5, by, panelX + panelW - 5, by + 20, hKA ? new Color(50, 50, 65, 255).getRGB() : new Color(30, 30, 40, 255).getRGB());
        if (hKA) ctx.drawHorizontalLine(panelX + 5, panelX + panelW - 6, by, new Color(255, 60, 60).getRGB());
        ctx.drawTextWithShadow(textRenderer, "KillAura", panelX + 12, by + 6, Color.WHITE.getRGB());
        String ka = KillAura.enabled ? "§aON" : "§cOFF";
        ctx.drawTextWithShadow(textRenderer, ka, panelX + panelW - 10 - textRenderer.getWidth(ka), by + 6, KillAura.enabled ? new Color(0, 255, 100).getRGB() : new Color(255, 60, 60).getRGB());

        by = panelY + 65;
        boolean hR = mx >= panelX + 5 && mx <= panelX + panelW - 5 && my >= by && my <= by + 20;
        ctx.fill(panelX + 5, by, panelX + panelW - 5, by + 20, hR ? new Color(50, 50, 65, 255).getRGB() : new Color(30, 30, 40, 255).getRGB());
        if (hR) ctx.drawHorizontalLine(panelX + 5, panelX + panelW - 6, by, new Color(255, 60, 60).getRGB());
        ctx.drawTextWithShadow(textRenderer, "Range", panelX + 12, by + 6, Color.WHITE.getRGB());
        String rv = String.valueOf(KillAura.range);
        ctx.drawTextWithShadow(textRenderer, rv, panelX + panelW - 10 - textRenderer.getWidth(rv), by + 6, new Color(255, 200, 50).getRGB());
    }

    public boolean shouldPause() { return false; }
    public static void open() { mc.setScreen(new ClickGUI()); }
}