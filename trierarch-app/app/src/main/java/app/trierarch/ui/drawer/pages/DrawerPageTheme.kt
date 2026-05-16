package app.trierarch.ui.drawer.pages

import androidx.compose.runtime.Composable
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.ui.graphics.Color

/**
 * Drawer accent colors.
 *
 * - Trierarch: app brand primary ([Theme.kt] `Color(0xFF004A9E)`)
 * - Arch: [archlinux.org artwork](https://archlinux.org/art/) — `#1793D1`
 * - Wine: [WineHQ wine_logo.svg](https://dl.winehq.org/wine/logos/wine_logo.svg) — `#8D0D2B`
 * - Debian: [debian.org openlogo-nd.svg](https://www.debian.org/logos/openlogo-nd.svg) — `#A80030`
 */
object DrawerPageColors {
    /** Trierarch app hub (deeper blue than Arch logo blue). */
    val Trierarch = Color(0xFF004A9E)

    val Arch = Color(0xFF1793D1)

    val Wine = Color(0xFF8D0D2B)

    val Debian = Color(0xFFA80030)
}

val LocalDrawerPageAccent = compositionLocalOf { DrawerPageColors.Trierarch }

@Composable
fun drawerPageAccent(): Color = LocalDrawerPageAccent.current
