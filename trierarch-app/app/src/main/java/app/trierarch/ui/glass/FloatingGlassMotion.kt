package app.trierarch.ui.glass

import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut

private const val ScrimEnterMs = 120
private const val ScrimExitMs = 100
private const val PanelEnterMs = 140
private const val PanelExitMs = 110

fun glassScrimEnter(): EnterTransition =
    fadeIn(animationSpec = tween(ScrimEnterMs, easing = FastOutSlowInEasing))

fun glassScrimExit(): ExitTransition =
    fadeOut(animationSpec = tween(ScrimExitMs, easing = FastOutSlowInEasing))

fun glassPanelEnter(): EnterTransition =
    fadeIn(animationSpec = tween(PanelEnterMs, easing = FastOutSlowInEasing)) +
        scaleIn(
            initialScale = 0.96f,
            animationSpec = tween(PanelEnterMs, easing = FastOutSlowInEasing)
        )

fun glassPanelExit(): ExitTransition =
    fadeOut(animationSpec = tween(PanelExitMs, easing = FastOutSlowInEasing)) +
        scaleOut(
            targetScale = 0.97f,
            animationSpec = tween(PanelExitMs, easing = FastOutSlowInEasing)
        )
