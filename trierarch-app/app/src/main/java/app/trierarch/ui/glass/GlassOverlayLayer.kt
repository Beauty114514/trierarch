package app.trierarch.ui.glass

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.MutableTransitionState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.BoxWithConstraintsScope
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier

/**
 * Full-screen glass overlay in the same composition as [app.trierarch.ui.AppScreen], not a separate dialog window.
 *
 * Trade-off: root uses [Modifier.fillMaxSize] only without applying cutout or system-bar padding, so
 * centered panels share the same measure box as [app.trierarch.ui.orb.FloatingMenuOrb]. May overlap status/nav regions.
 *
 * Dismiss: [onDismissRequest] from system back and scrim tap.
 */
@Composable
fun GlassOverlayLayer(
    onDismissRequest: () -> Unit,
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit,
) {
    BackHandler(onBack = onDismissRequest)
    val scrimSource = remember { MutableInteractionSource() }
    val overlayShown = remember { MutableTransitionState(false) }
    LaunchedEffect(Unit) { overlayShown.targetState = true }
    Box(modifier.fillMaxSize()) {
        AnimatedVisibility(
            visibleState = overlayShown,
            enter = glassScrimEnter(),
            exit = glassScrimExit()
        ) {
            Box(
                Modifier
                    .fillMaxSize()
                    .background(floatingOverlayScrimColor())
                    .clickable(
                        interactionSource = scrimSource,
                        indication = null,
                        onClick = onDismissRequest
                    )
            )
        }
        content()
    }
}

/**
 * Full-screen layer above [GlassOverlayLayer] content (e.g. option list). Same [fillMaxSize] as the parent overlay.
 *
 * Dismiss: [onDismissRequest] from system back and scrim tap.
 */
@Composable
fun GlassSubOverlay(
    onDismissRequest: () -> Unit,
    content: @Composable BoxWithConstraintsScope.() -> Unit,
) {
    BackHandler(onBack = onDismissRequest)
    val scrimSource = remember { MutableInteractionSource() }
    val overlayShown = remember { MutableTransitionState(false) }
    LaunchedEffect(Unit) { overlayShown.targetState = true }
    Box(Modifier.fillMaxSize()) {
        AnimatedVisibility(
            visibleState = overlayShown,
            enter = glassScrimEnter(),
            exit = glassScrimExit()
        ) {
            Box(
                Modifier
                    .fillMaxSize()
                    .background(floatingOverlayScrimColor())
                    .clickable(
                        interactionSource = scrimSource,
                        indication = null,
                        onClick = onDismissRequest
                    )
            )
        }
        BoxWithConstraints(Modifier.fillMaxSize()) {
            content()
        }
    }
}
