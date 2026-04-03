package app.trierarch.ui.glass

import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut

// Durations for glass scrim/panel AnimatedVisibility (GlassOverlayLayer, GlassSubOverlay).
private const val ScrimEnterMs = 120
private const val ScrimExitMs = 100
private const val PanelEnterMs = 140
private const val PanelExitMs = 110

/** Scrim layer: fade in when overlay opens. */
fun glassScrimEnter(): EnterTransition =
    fadeIn(animationSpec = tween(ScrimEnterMs, easing = FastOutSlowInEasing)        )

/** Scrim layer: fade out when overlay closes. */
fun glassScrimExit(): ExitTransition =
    fadeOut(animationSpec = tween(ScrimExitMs, easing = FastOutSlowInEasing)        )

/** Panel stack: fade + slight scale up when overlay content appears. */
fun glassPanelEnter(): EnterTransition =
    fadeIn(animationSpec = tween(PanelEnterMs, easing = FastOutSlowInEasing)) +
        scaleIn(
            initialScale = 0.96f,
            animationSpec = tween(PanelEnterMs, easing = FastOutSlowInEasing)
        )

/** Panel stack: fade + slight scale down when overlay content hides. */
fun glassPanelExit(): ExitTransition =
    fadeOut(animationSpec = tween(PanelExitMs, easing = FastOutSlowInEasing)) +
        scaleOut(
            targetScale = 0.97f,
            animationSpec = tween(PanelExitMs, easing = FastOutSlowInEasing)
        )
