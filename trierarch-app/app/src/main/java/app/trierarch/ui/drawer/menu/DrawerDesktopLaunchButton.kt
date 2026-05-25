package app.trierarch.ui.drawer.menu

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import app.trierarch.ui.drawer.pages.drawerPageAccent

@Composable
fun DrawerDesktopLaunchButton(
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    label: String = "Launch",
) {
    val accent = drawerPageAccent()
    val level = LocalDrawerHierarchyLevel.current
    Text(
        text = label,
        style = drawerSectionTextStyle(level),
        fontWeight = FontWeight.SemiBold,
        color = accent,
        modifier = modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .padding(
                start = drawerStartPadding(level),
                end = drawerEndPadding(),
                top = 16.dp,
                bottom = 16.dp,
            ),
    )
}
