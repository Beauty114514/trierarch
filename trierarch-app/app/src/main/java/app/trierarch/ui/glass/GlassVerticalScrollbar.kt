package app.trierarch.ui.glass

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.width
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

/**
 * Vertical track and thumb driven by [scrollState].
 *
 * Preconditions: [scrollState] is the same instance as the scrollable container.
 * Visibility: composable returns immediately when [ScrollState.maxValue] is zero.
 * [viewportHeight]: visible column height in dp (thumb ratio vs. scrollbar travel).
 */
@Composable
fun GlassVerticalScrollbar(
    scrollState: ScrollState,
    viewportHeight: Dp,
    modifier: Modifier = Modifier,
    trackWidth: Dp = 4.dp,
) {
    val maxVal = scrollState.maxValue
    if (maxVal <= 0) return
    val scrollValue = scrollState.value
    val density = LocalDensity.current
    val trackColor = MaterialTheme.colorScheme.outline.copy(alpha = 0.28f)
    val thumbColor = MaterialTheme.colorScheme.outline.copy(alpha = 0.72f)
    val viewportPx = with(density) { viewportHeight.toPx() }.coerceAtLeast(1f)
    val extent = viewportPx + maxVal
    val thumbFraction = (viewportPx / extent).coerceIn(0.08f, 1f)
    Canvas(
        modifier = modifier
            .width(trackWidth)
            .fillMaxHeight()
    ) {
        val trackH = size.height
        if (trackH <= 0f) return@Canvas
        val w = size.width
        val corner = CornerRadius(w / 2f, w / 2f)
        drawRoundRect(color = trackColor, cornerRadius = corner)
        val thumbH = trackH * thumbFraction
        val travel = (trackH - thumbH).coerceAtLeast(0f)
        val progress = scrollValue / maxVal.toFloat()
        val y = travel * progress.coerceIn(0f, 1f)
        drawRoundRect(
            color = thumbColor,
            topLeft = Offset(0f, y),
            size = Size(w, thumbH),
            cornerRadius = corner
        )
    }
}
