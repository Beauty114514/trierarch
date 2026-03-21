package app.trierarch.ui.screens

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.dp

private const val LEFT_EDGE_FRACTION = 0.12f
private val SWIPE_THRESHOLD_DP = 40.dp
private const val MIN_POINTERS = 2
private val MENU_WIDTH_MAX_DP = 320.dp
private const val MENU_WIDTH_FRACTION = 0.75f

/**
 * Detects two-finger swipe from left edge to the right; calls [onSwipeRight] when triggered.
 */
fun Modifier.twoFingerSwipeFromLeft(onSwipeRight: () -> Unit): Modifier = composed {
    val density = LocalDensity.current
    val thresholdPx = with(density) { SWIPE_THRESHOLD_DP.toPx() }
    pointerInput(onSwipeRight) {
        val initialX = mutableMapOf<Long, Float>()
        awaitPointerEventScope {
            while (true) {
                val event = awaitPointerEvent()
                val width = size.width.toFloat()
                val edgePx = width * LEFT_EDGE_FRACTION
                for (change in event.changes) {
                    when {
                        change.pressed && change.id.value !in initialX ->
                            if (change.position.x <= edgePx) initialX[change.id.value] = change.position.x
                        !change.pressed -> initialX.remove(change.id.value)
                    }
                }
                if (initialX.size >= MIN_POINTERS) {
                    var movedRight = 0
                    for (change in event.changes) {
                        val start = initialX[change.id.value] ?: continue
                        if (change.position.x - start >= thresholdPx) movedRight++
                    }
                    if (movedRight >= MIN_POINTERS) {
                        onSwipeRight()
                        event.changes.forEach { it.consume() }
                        initialX.clear()
                    }
                }
            }
        }
    }
}

@Composable
fun SideMenu(
    visible: Boolean,
    onDismiss: () -> Unit,
    waylandOn: Boolean,
    onWaylandClick: () -> Unit,
    onDisplayClick: () -> Unit,
    onDisplayLongPress: () -> Unit,
    onViewClick: () -> Unit,
    onKeyboardClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    AnimatedVisibility(
        visible = visible,
        enter = slideInHorizontally(initialOffsetX = { -it }),
        exit = slideOutHorizontally(targetOffsetX = { -it }),
        modifier = modifier.fillMaxSize()
    ) {
        BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
            val menuWidth = minOf(MENU_WIDTH_MAX_DP, maxWidth * MENU_WIDTH_FRACTION)
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .clickable(onClick = onDismiss)
            ) {
                Column(
                    modifier = Modifier
                        .width(menuWidth)
                        .fillMaxHeight()
                        .align(Alignment.CenterStart)
                        .background(MaterialTheme.colorScheme.surfaceVariant)
                        .clickable { }  // Consume taps so backdrop click does not close menu
                        .padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text(
                        "Menu",
                        style = MaterialTheme.typography.titleMedium,
                        color = MaterialTheme.colorScheme.onSurface
                    )
                    Box(
                        modifier = Modifier
                            .clickable { onWaylandClick() }
                            .padding(vertical = 12.dp),
                        contentAlignment = Alignment.CenterStart
                    ) {
                        Text(
                            "Wayland: ${if (waylandOn) "On" else "Off"}",
                            style = MaterialTheme.typography.bodyLarge,
                            color = MaterialTheme.colorScheme.primary
                        )
                    }
                    Box(
                        modifier = Modifier
                            .pointerInput(waylandOn) {
                                detectTapGestures(
                                    onLongPress = { onDisplayLongPress() },
                                    onTap = { if (waylandOn) onDisplayClick() }
                                )
                            }
                            .padding(vertical = 12.dp),
                        contentAlignment = Alignment.CenterStart
                    ) {
                        Text(
                            "Display",
                            style = MaterialTheme.typography.bodyLarge,
                            color = if (waylandOn) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.5f)
                        )
                    }
                    Box(
                        modifier = Modifier
                            .clickable { onViewClick() }
                            .padding(vertical = 12.dp),
                        contentAlignment = Alignment.CenterStart
                    ) {
                        Text(
                            "View",
                            style = MaterialTheme.typography.bodyLarge,
                            color = MaterialTheme.colorScheme.primary
                        )
                    }
                    Box(
                        modifier = Modifier
                            .pointerInput(waylandOn) {
                                detectTapGestures(
                                    onTap = { if (waylandOn) onKeyboardClick() }
                                )
                            }
                            .padding(vertical = 12.dp),
                        contentAlignment = Alignment.CenterStart
                    ) {
                        Text(
                            "Keyboard",
                            style = MaterialTheme.typography.bodyLarge,
                            color = if (waylandOn) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.5f)
                        )
                    }
                }
            }
        }
    }
}
