package app.trierarch.ui.glass

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraintsScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

/**
 * Frosted panel layout shared by the launcher orb and glass overlays.
 *
 * Layout flow:
 * - Caller provides [BoxWithConstraints] under a full-screen overlay (see [GlassOverlayLayer]); constraints are usually bounded.
 * - [OrbStyleGlassPanel] centers a width-capped scrollable card; shell clicks go to [panelConsume] so the scrim stays usable.
 * - [OrbStyleGlassFillPanel] fixes card height so column children can use vertical weight.
 * - If width or height constraints are unbounded, layout clamps to [LocalView] size so vertical max constraints stay finite.
 */
@Composable
private fun BoxWithConstraintsScope.glassLayoutBudget(): Pair<Dp, Dp> {
    // Defensive: unbounded max occurs outside a typical AppScreen overlay; host view keeps caps and centering stable.
    if (constraints.hasBoundedWidth && constraints.hasBoundedHeight) {
        return maxWidth to maxHeight
    }
    val density = LocalDensity.current
    val view = LocalView.current
    val w = if (constraints.hasBoundedWidth) {
        maxWidth
    } else {
        with(density) { view.width.coerceAtLeast(1).toDp() }
    }
    val h = if (constraints.hasBoundedHeight) {
        maxHeight
    } else {
        with(density) { view.height.coerceAtLeast(1).toDp() }
    }
    return w to h
}

/**
 * Centered frosted card with vertical scroll and [GlassVerticalScrollbar].
 *
 * Preconditions: [BoxWithConstraintsScope] under an overlay root (typically bounded).
 * [panelConsume]: [MutableInteractionSource] for the shell [clickable] (no-op); isolates panel from scrim gestures.
 */
@Composable
fun BoxWithConstraintsScope.OrbStyleGlassPanel(
    widthCap: Dp,
    panelConsume: MutableInteractionSource,
    columnModifier: Modifier = Modifier,
    shape: Shape = RoundedCornerShape(FloatingGlassCornerDp),
    edge: Dp = GlassDialogScreenInsetDp,
    minPanelWidth: Dp = 48.dp,
    panelMinHeight: Dp = 48.dp,
    viewportMinHeight: Dp = 120.dp,
    contentPadding: PaddingValues = PaddingValues(12.dp),
    verticalArrangement: Arrangement.Vertical = Arrangement.Top,
    content: @Composable ColumnScope.() -> Unit,
) {
    val (effMaxW, effMaxH) = glassLayoutBudget()
    val menuWidth = minOf(widthCap, effMaxW - edge * 2).coerceAtLeast(minPanelWidth)
    val panelMaxHeight = (effMaxH - edge * 2).coerceAtLeast(viewportMinHeight)
    val scrollState = rememberScrollState()
    val viewPadTop = contentPadding.calculateTopPadding()
    val viewPadBottom = contentPadding.calculateBottomPadding()
    val scrollViewport =
        (panelMaxHeight - viewPadTop - viewPadBottom).coerceAtLeast(32.dp)
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Box(
            modifier = Modifier
                .width(menuWidth)
                .wrapContentHeight()
                .heightIn(min = panelMinHeight, max = panelMaxHeight)
                .clip(shape)
                .border(
                    width = FloatingGlassRimDp,
                    color = MaterialTheme.colorScheme.outline.copy(alpha = FloatingGlassRimAlpha),
                    shape = shape
                )
                .clickable(
                    interactionSource = panelConsume,
                    indication = null,
                    onClick = { }
                )
        ) {
            Box(
                modifier = Modifier
                    .matchParentSize()
                    .then(glassBlurModifier())
                    .background(brush = floatingGlassBrush(), shape = shape)
            )
            Box(Modifier.fillMaxWidth()) {
                Column(
                    columnModifier
                        .fillMaxWidth()
                        .padding(end = 10.dp)
                        .heightIn(max = panelMaxHeight)
                        .verticalScroll(scrollState)
                        .padding(contentPadding),
                    verticalArrangement = verticalArrangement
                ) {
                    content()
                }
                GlassVerticalScrollbar(
                    scrollState = scrollState,
                    viewportHeight = scrollViewport,
                    modifier = Modifier
                        .align(Alignment.CenterEnd)
                        .padding(
                            top = viewPadTop + 2.dp,
                            end = 3.dp,
                            bottom = viewPadBottom + 2.dp
                        )
                )
            }
        }
    }
}

/**
 * Centered frosted card with fixed vertical band; inner [Column] fills the card for [Modifier.weight] children.
 *
 * [panelConsume]: same contract as [OrbStyleGlassPanel].
 */
@Composable
fun BoxWithConstraintsScope.OrbStyleGlassFillPanel(
    widthCap: Dp,
    panelConsume: MutableInteractionSource,
    columnModifier: Modifier = Modifier,
    shape: Shape = RoundedCornerShape(FloatingGlassCornerDp),
    edge: Dp = GlassDialogScreenInsetDp,
    minPanelWidth: Dp = 48.dp,
    panelMinHeight: Dp = 200.dp,
    viewportMinHeight: Dp = 200.dp,
    contentPadding: PaddingValues = PaddingValues(horizontal = 20.dp, vertical = 16.dp),
    content: @Composable ColumnScope.() -> Unit,
) {
    val (effMaxW, effMaxH) = glassLayoutBudget()
    val menuWidth = minOf(widthCap, effMaxW - edge * 2).coerceAtLeast(minPanelWidth)
    val panelMaxHeight = (effMaxH - edge * 2).coerceAtLeast(viewportMinHeight)
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Box(
            modifier = Modifier
                .width(menuWidth)
                .heightIn(min = panelMinHeight, max = panelMaxHeight)
                .clip(shape)
                .border(
                    width = FloatingGlassRimDp,
                    color = MaterialTheme.colorScheme.outline.copy(alpha = FloatingGlassRimAlpha),
                    shape = shape
                )
                .clickable(
                    interactionSource = panelConsume,
                    indication = null,
                    onClick = { }
                )
        ) {
            Box(
                modifier = Modifier
                    .matchParentSize()
                    .then(glassBlurModifier())
                    .background(brush = floatingGlassBrush(), shape = shape)
            )
            Column(
                columnModifier
                    .fillMaxSize()
                    .padding(contentPadding)
            ) {
                content()
            }
        }
    }
}
