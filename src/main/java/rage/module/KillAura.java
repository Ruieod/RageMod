package rage.module;
import net.minecraft.client.MinecraftClient;
import net.minecraft.entity.LivingEntity;
import net.minecraft.entity.player.PlayerEntity;
import net.minecraft.util.Hand;
import net.minecraft.network.packet.c2s.play.PlayerMoveC2SPacket;

public class KillAura {
    private static final MinecraftClient mc = MinecraftClient.getInstance();
    public static boolean enabled = true;
    public static double range = 3.0;
    private static int tick = 0;

    public static void onTick() {
        if (!enabled || mc.player == null || mc.world == null || mc.interactionManager == null) return;
        if (mc.player.isDead()) return;
        tick++;
        if (tick < 10) return;
        tick = 0;

        LivingEntity target = null;
        double closest = range;
        for (var ent : mc.world.getEntities()) {
            if (!(ent instanceof PlayerEntity e)) continue;
            if (e == mc.player || !e.isAlive() || e.isDead()) continue;
            double d = mc.player.distanceTo(e);
            if (d > range) continue;
            if (d < closest) { closest = d; target = e; }
        }
        if (target == null) return;

        if (mc.player.isOnGround() && !mc.player.isTouchingWater() && !mc.player.isInLava()) {
            double x = mc.player.getX();
            double y = mc.player.getY();
            double z = mc.player.getZ();
            mc.getNetworkHandler().sendPacket(new PlayerMoveC2SPacket.PositionAndOnGround(x, y + 0.0625, z, false, true));
            mc.getNetworkHandler().sendPacket(new PlayerMoveC2SPacket.PositionAndOnGround(x, y, z, false, true));
            mc.getNetworkHandler().sendPacket(new PlayerMoveC2SPacket.PositionAndOnGround(x, y + 0.0625, z, false, true));
        }

        mc.interactionManager.attackEntity(mc.player, target);
        mc.player.swingHand(Hand.MAIN_HAND);
    }
}