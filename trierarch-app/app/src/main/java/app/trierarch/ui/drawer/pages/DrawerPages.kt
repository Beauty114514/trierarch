package app.trierarch.ui.drawer.pages

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.unit.dp
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState

enum class DrawerPage(val accent: Color) {
    TRIERARCH(DrawerPageColors.Trierarch),
    ARCH(DrawerPageColors.Arch),
    WINE(DrawerPageColors.Wine),
    DEBIAN(DrawerPageColors.Debian),
}

@Composable
fun DrawerPagedHost(
    trierarchContent: @Composable () -> Unit,
    archContent: @Composable () -> Unit,
    wineContent: @Composable () -> Unit,
    debianContent: @Composable () -> Unit,
    modifier: Modifier = Modifier,
) {
    val pages = remember {
        listOf(DrawerPage.TRIERARCH, DrawerPage.ARCH, DrawerPage.DEBIAN, DrawerPage.WINE)
    }
    val pagerState = rememberPagerState(initialPage = 0) { pages.size }
    val page = pages[pagerState.currentPage.coerceIn(0, pages.lastIndex)]

    Box(
        modifier = modifier
            .fillMaxSize()
            .drawBehind {
                val h = 4.dp.toPx()
                val segW = size.width / pages.size
                val accents = pages.map { it.accent }

                accents.forEachIndexed { i, accent ->
                    val left = segW * i
                    val alpha = if (pages[i] == page) 1f else 0.22f
                    drawRect(
                        color = accent.copy(alpha = alpha),
                        topLeft = Offset(left, 0f),
                        size = Size(segW, h),
                    )
                }
            }
            .padding(top = 4.dp) // top bar: Trierarch | Arch | Debian | Wine
    ) {
        HorizontalPager(
            state = pagerState,
            modifier = Modifier.fillMaxSize(),
        ) { idx ->
            val scroll = rememberScrollState()
            CompositionLocalProvider(LocalDrawerPageAccent provides pages[idx].accent) {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .verticalScroll(scroll)
                        .background(Color.Transparent),
                    contentAlignment = Alignment.TopStart,
                ) {
                    when (pages[idx]) {
                        DrawerPage.TRIERARCH -> trierarchContent()
                        DrawerPage.ARCH -> archContent()
                        DrawerPage.DEBIAN -> debianContent()
                        DrawerPage.WINE -> wineContent()
                    }
                }
            }
        }
    }
}
