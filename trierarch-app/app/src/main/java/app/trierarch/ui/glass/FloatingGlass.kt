package app.trierarch.ui.glass

import android.os.Build
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.blur
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

val FloatingGlassCornerDp = 20.dp
val FloatingGlassRimDp = 1.dp
const val FloatingGlassRimAlpha = 0.55f

/** Margin subtracted from layout max width/height inside [OrbStyleGlassPanel] (dp math); aligns with orb menu margins. */
val GlassDialogScreenInsetDp = 8.dp

val GlassDialogWidthStandardDp = 400.dp
val GlassDialogWidthPickerDp = 280.dp
val GlassDialogWidthScriptDp = 520.dp
const val FloatingOverlayScrimAlpha = 0.22f

/** Scrim color for [GlassOverlayLayer] and [GlassSubOverlay]; alpha from [FloatingOverlayScrimAlpha]. */
fun floatingOverlayScrimColor(): Color = Color.Black.copy(alpha = FloatingOverlayScrimAlpha)

/**
 * Fills the receiver then applies [WindowInsets.safeDrawing] padding.
 *
 * Trade-off: keeps content inside cutouts; menu-style overlays that must match [app.trierarch.ui.orb.FloatingMenuOrb]
 * centering use plain [fillMaxSize] instead — see [GlassOverlayLayer].
 */
@Composable
fun Modifier.glassDialogFullscreen(): Modifier =
    fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing)

/** Blur radius when [Build.VERSION_CODES.S] or newer; absent on older APIs (no heavy software blur). */
internal val FloatingGlassBlurDp = 14.dp

private const val GlassHighlightApi31 = 0.065f
private const val GlassFillApi31 = 0.022f
private const val GlassHighlightLegacy = 0.09f
private const val GlassFillLegacy = 0.035f

/** Modifier chain step for frosted stack: real blur on API 31+, identity below. */
fun glassBlurModifier(): Modifier =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        Modifier.blur(FloatingGlassBlurDp)
    } else {
        Modifier
    }

/** Gloss gradient drawn over the blur layer; strengths tuned per API tier in private constants. */
fun floatingGlassBrush(): Brush =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        Brush.verticalGradient(
            colors = listOf(
                Color.White.copy(alpha = GlassHighlightApi31),
                Color.White.copy(alpha = GlassFillApi31)
            )
        )
    } else {
        Brush.verticalGradient(
            colors = listOf(
                Color.White.copy(alpha = GlassHighlightLegacy),
                Color.White.copy(alpha = GlassFillLegacy)
            )
        )
    }
