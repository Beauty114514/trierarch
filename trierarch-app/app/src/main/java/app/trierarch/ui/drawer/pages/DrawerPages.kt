package app.trierarch.ui.drawer.pages

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
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
    ARCH(Color(0xFF1793D1)),
    ANDROID(Color(0xFF004E8C)),
    DEBIAN(Color(0xFFD70A53)),
}

@Composable
fun DrawerPagedHost(
    archContent: @Composable () -> Unit,
    androidContent: @Composable () -> Unit,
    debianContent: @Composable () -> Unit,
    modifier: Modifier = Modifier,
) {
    val pages = remember { listOf(DrawerPage.ARCH, DrawerPage.ANDROID, DrawerPage.DEBIAN) }
    val pagerState = rememberPagerState(initialPage = 0) { pages.size }
    val page = pages[pagerState.currentPage.coerceIn(0, pages.lastIndex)]

    Box(
        modifier = modifier
            .fillMaxSize()
            .drawBehind {
                val w = 4.dp.toPx()
                val segH = size.height / 3f
                val inactive = Color.White.copy(alpha = 0.14f)

                drawRect(color = inactive, topLeft = Offset(0f, 0f), size = Size(w, segH))
                drawRect(color = inactive, topLeft = Offset(0f, segH), size = Size(w, segH))
                drawRect(color = inactive, topLeft = Offset(0f, segH * 2f), size = Size(w, segH))

                val activeTop = when (page) {
                    DrawerPage.ARCH -> 0f
                    DrawerPage.ANDROID -> segH
                    DrawerPage.DEBIAN -> segH * 2f
                }
                drawRect(color = page.accent, topLeft = Offset(0f, activeTop), size = Size(w, segH))
            }
            .padding(start = 4.dp) // leave space for the accent border
    ) {
        HorizontalPager(
            state = pagerState,
            modifier = Modifier.fillMaxSize(),
        ) { idx ->
            val scroll = rememberScrollState()
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .verticalScroll(scroll)
                    .background(Color.Transparent),
                contentAlignment = Alignment.TopStart,
            ) {
                when (pages[idx]) {
                    DrawerPage.ARCH -> archContent()
                    DrawerPage.ANDROID -> androidContent()
                    DrawerPage.DEBIAN -> debianContent()
                }
            }
        }
    }
}
