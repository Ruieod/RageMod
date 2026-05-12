package rage.mixin;
import net.minecraft.entity.player.PlayerEntity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

@Mixin(PlayerEntity.class)
public class MixinPlayerEntity {
    @Inject(method = "isInvisible", at = @At("HEAD"), cancellable = true)
    public void forceVisible(CallbackInfoReturnable<Boolean> cir) { cir.setReturnValue(false); }
}