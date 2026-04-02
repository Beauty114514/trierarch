package app.trierarch.ui.glass

import android.os.Build
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.blur
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

val FloatingGlassCornerDp = 20.dp
val FloatingGlassRimDp = 1.dp
const val FloatingGlassRimAlpha = 0.55f
const val FloatingOverlayScrimAlpha = 0.22f

fun floatingOverlayScrimColor(): Color = Color.Black.copy(alpha = FloatingOverlayScrimAlpha)

internal val FloatingGlassBlurDp = 14.dp

private const val GlassHighlightApi31 = 0.065f
private const val GlassFillApi31 = 0.022f
private const val GlassHighlightLegacy = 0.09f
private const val GlassFillLegacy = 0.035f

fun glassBlurModifier(): Modifier =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        Modifier.blur(FloatingGlassBlurDp)
    } else {
        Modifier
    }

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
