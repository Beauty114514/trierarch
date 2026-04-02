package app.trierarch.ui.orb

import android.content.SharedPreferences
import app.trierarch.R
import app.trierarch.ui.glass.FloatingGlassCornerDp
import app.trierarch.ui.glass.FloatingGlassRimAlpha
import app.trierarch.ui.glass.FloatingGlassRimDp
import app.trierarch.ui.glass.floatingGlassBrush
import app.trierarch.ui.glass.floatingOverlayScrimColor
import app.trierarch.ui.glass.glassBlurModifier
import app.trierarch.ui.glass.glassPanelEnter
import app.trierarch.ui.glass.glassPanelExit
import app.trierarch.ui.glass.glassScrimEnter
import app.trierarch.ui.glass.glassScrimExit
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/** Draggable launcher orb: tap toggles frosted menu; drag persists anchor in [prefs]. */
@Composable
fun FloatingMenuOrb(
    prefs: SharedPreferences,
    menuExpanded: Boolean,
    onMenuOpenRequest: () -> Unit,
    onDismissMenu: () -> Unit,
    onDisplayClick: () -> Unit,
    onDisplayLongPress: () -> Unit,
    onViewClick: () -> Unit,
    onAppearanceClick: () -> Unit,
    onKeyboardClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    val density = LocalDensity.current
    val scope = rememberCoroutineScope()
    val openMenuAction by rememberUpdatedState(onMenuOpenRequest)
    val dismissMenuAction by rememberUpdatedState(onDismissMenu)
    val onDisplayTap by rememberUpdatedState(onDisplayClick)
    val onDisplayLong by rememberUpdatedState(onDisplayLongPress)
    val onView by rememberUpdatedState(onViewClick)
    val onAppearance by rememberUpdatedState(onAppearanceClick)
    val onKeyboard by rememberUpdatedState(onKeyboardClick)

    BoxWithConstraints(modifier.graphicsLayer { clip = false }) {
        val maxWpx = with(density) { maxWidth.toPx() }
        val maxHpx = with(density) { maxHeight.toPx() }
        val orbPx = with(density) { ORB_SIZE_DP.toPx() }
        val minCx = orbPx / 2f
        val maxCx = maxOf(minCx, maxWpx - orbPx / 2f)
        val minCy = orbPx / 2f
        val maxCy = maxOf(minCy, maxHpx - orbPx / 2f)

        var centerXFrac by remember {
            mutableFloatStateOf(
                prefs.getFloat(PREF_ORB_CENTER_X_FRAC, DEFAULT_CENTER_X_FRAC)
            )
        }
        var centerYFrac by remember {
            mutableFloatStateOf(
                prefs.getFloat(PREF_ORB_CENTER_Y_FRAC, DEFAULT_CENTER_Y_FRAC)
            )
        }

        fun persistCenter() {
            prefs.edit()
                .putFloat(PREF_ORB_CENTER_X_FRAC, centerXFrac)
                .putFloat(PREF_ORB_CENTER_Y_FRAC, centerYFrac)
                .apply()
        }

        val orbTxPx = centerXFrac * maxWpx - orbPx / 2f
        val orbTyPx = centerYFrac * maxHpx - orbPx / 2f
        val orbOffsetXDp = with(density) { orbTxPx.toDp() }
        val orbOffsetYDp = with(density) { orbTyPx.toDp() }

        AnimatedVisibility(
            visible = menuExpanded,
            enter = glassScrimEnter(),
            exit = glassScrimExit()
        ) {
            val scrimSource = remember { MutableInteractionSource() }
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(floatingOverlayScrimColor())
                    .clickable(
                        interactionSource = scrimSource,
                        indication = null,
                        onClick = { dismissMenuAction() }
                    )
            )
        }

        val tapSource = remember { MutableInteractionSource() }
        var isOrbDragging by remember { mutableStateOf(false) }
        Box(
            modifier = Modifier.offset(x = orbOffsetXDp, y = orbOffsetYDp)
        ) {
            Box(
                modifier = Modifier
                    .size(ORB_SIZE_DP)
                    .clickable(
                        enabled = !isOrbDragging,
                        interactionSource = tapSource,
                        indication = null,
                        onClick = {
                            if (menuExpanded) dismissMenuAction()
                            else openMenuAction()
                        }
                    )
                    .pointerInput(maxWpx, maxHpx) {
                        var movedOrbThisGesture = false

                        fun endDragGesture() {
                            if (movedOrbThisGesture) persistCenter()
                            movedOrbThisGesture = false
                            scope.launch {
                                delay(EDIT_TO_TAP_GAP_MS)
                                isOrbDragging = false
                            }
                        }

                        detectDragGestures(
                            onDragStart = { isOrbDragging = true },
                            onDragCancel = { endDragGesture() },
                            onDragEnd = { endDragGesture() },
                            onDrag = { _, dragAmount ->
                                val newCx =
                                    (centerXFrac * maxWpx + dragAmount.x).coerceIn(minCx, maxCx)
                                val newCy =
                                    (centerYFrac * maxHpx + dragAmount.y).coerceIn(minCy, maxCy)
                                centerXFrac = newCx / maxWpx
                                centerYFrac = newCy / maxHpx
                                movedOrbThisGesture = true
                            }
                        )
                    }
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .clip(CircleShape)
                        .then(glassBlurModifier())
                        .background(brush = floatingGlassBrush(), shape = CircleShape)
                )
                Image(
                    painter = painterResource(id = R.drawable.ic_launcher_foreground),
                    contentDescription = null,
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(ORB_LOGO_INSET_DP),
                    contentScale = ContentScale.Fit
                )
                Box(
                    Modifier
                        .fillMaxSize()
                        .border(
                            width = FloatingGlassRimDp,
                            color = MaterialTheme.colorScheme.outline.copy(alpha = FloatingGlassRimAlpha),
                            shape = CircleShape
                        )
                )
            }
        }

        AnimatedVisibility(
            visible = menuExpanded,
            enter = glassPanelEnter(),
            exit = glassPanelExit()
        ) {
            val orbLeft = orbOffsetXDp
            val orbTop = orbOffsetYDp
            val orbRight = orbLeft + ORB_SIZE_DP
            val orbBottom = orbTop + ORB_SIZE_DP
            val edge = HORIZONTAL_MARGIN_DP
            val gap = ORB_PANEL_GAP_DP
            val menuWidth = minOf(MENU_PANEL_MAX_WIDTH, maxWidth - edge * 2)
            val menuH = MENU_PANEL_ESTIMATED_HEIGHT_DP

            val availBelow = maxHeight - orbBottom - gap - edge
            val availAbove = orbTop - gap - edge
            val fitsBelow = menuH <= availBelow
            val fitsAbove = menuH <= availAbove
            val spaceRight = maxWidth - orbRight - gap - edge
            val spaceLeft = orbLeft - gap - edge
            val fitsRight = menuWidth <= spaceRight
            val fitsLeft = menuWidth <= spaceLeft

            fun hCenteredUnderOrb(): Dp =
                (orbLeft + ORB_SIZE_DP / 2 - menuWidth / 2).coerceIn(edge, maxWidth - menuWidth - edge)

            fun vCenteredBesideOrb(): Dp =
                (orbTop + ORB_SIZE_DP / 2 - menuH / 2).coerceIn(edge, maxHeight - menuH - edge)

            val (menuLeftDp, menuTopDp) = when {
                fitsBelow -> hCenteredUnderOrb() to orbBottom + gap
                fitsAbove -> hCenteredUnderOrb() to orbTop - gap - menuH
                fitsRight && fitsLeft -> {
                    val v = vCenteredBesideOrb()
                    if (spaceLeft > spaceRight) {
                        (orbLeft - gap - menuWidth) to v
                    } else {
                        (orbRight + gap) to v
                    }
                }
                fitsRight -> (orbRight + gap) to vCenteredBesideOrb()
                fitsLeft -> (orbLeft - gap - menuWidth) to vCenteredBesideOrb()
                else -> {
                    val left = hCenteredUnderOrb()
                    val top = (orbBottom + gap).coerceIn(edge, maxHeight - menuH - edge)
                    left to top
                }
            }

            val panelConsume = remember { MutableInteractionSource() }
            val menuShape = RoundedCornerShape(FloatingGlassCornerDp)
            Box(
                modifier = Modifier
                    .offset(x = menuLeftDp, y = menuTopDp)
                    .width(menuWidth)
                    .height(IntrinsicSize.Min)
                    .clip(menuShape)
                    .border(
                        width = FloatingGlassRimDp,
                        color = MaterialTheme.colorScheme.outline.copy(alpha = FloatingGlassRimAlpha),
                        shape = menuShape
                    )
                    .clickable(
                        interactionSource = panelConsume,
                        indication = null,
                        onClick = { }
                    )
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .fillMaxHeight()
                        .then(glassBlurModifier())
                        .background(brush = floatingGlassBrush(), shape = menuShape)
                )
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(12.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text(
                        "Menu",
                        style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.SemiBold),
                        color = Color.White,
                        modifier = Modifier.padding(bottom = 4.dp)
                    )
                    Box(
                        modifier = Modifier
                            .pointerInput(Unit) {
                                detectTapGestures(
                                    onLongPress = { onDisplayLong() },
                                    onTap = { onDisplayTap() }
                                )
                            }
                            .padding(vertical = 12.dp)
                    ) {
                        Text(
                            "Display",
                            style = MaterialTheme.typography.bodyLarge,
                            color = MaterialTheme.colorScheme.primary
                        )
                    }
                    Box(
                        modifier = Modifier
                            .clickable { onView() }
                            .padding(vertical = 12.dp)
                    ) {
                        Text(
                            "View",
                            style = MaterialTheme.typography.bodyLarge,
                            color = MaterialTheme.colorScheme.primary
                        )
                    }
                    Box(
                        modifier = Modifier
                            .clickable { onAppearance() }
                            .padding(vertical = 12.dp)
                    ) {
                        Text(
                            "Appearance",
                            style = MaterialTheme.typography.bodyLarge,
                            color = MaterialTheme.colorScheme.primary
                        )
                    }
                    Box(
                        modifier = Modifier
                            .pointerInput(Unit) {
                                detectTapGestures(onTap = { onKeyboard() })
                            }
                            .padding(vertical = 12.dp)
                    ) {
                        Text(
                            "Keyboard",
                            style = MaterialTheme.typography.bodyLarge,
                            color = MaterialTheme.colorScheme.primary
                        )
                    }
                }
            }
        }
    }
}

private val ORB_SIZE_DP = 48.dp
private val ORB_LOGO_INSET_DP = 0.dp
private const val EDIT_TO_TAP_GAP_MS = 24L
private val MENU_PANEL_MAX_WIDTH = 280.dp
private val MENU_PANEL_ESTIMATED_HEIGHT_DP = 288.dp
private val ORB_PANEL_GAP_DP = 8.dp
private val HORIZONTAL_MARGIN_DP = 8.dp
private const val PREF_ORB_CENTER_X_FRAC = "menu_orb_center_x_frac"
private const val PREF_ORB_CENTER_Y_FRAC = "menu_orb_center_y_frac"
private const val DEFAULT_CENTER_X_FRAC = 0.88f
private const val DEFAULT_CENTER_Y_FRAC = 0.42f
