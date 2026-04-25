package app.trierarch.ui.drawer.menu

import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.height
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import androidx.compose.foundation.clickable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.Alignment

@Composable
fun DrawerMenu(
    title: String,
    labels: DrawerMenuLabels,
    options: DrawerMenuOptions,
    actions: DrawerMenuActions,
    modifier: Modifier = Modifier,
    showDebianDesktop: Boolean = false,
    extraContent: (@Composable () -> Unit)? = null,
) {
    Box(modifier = modifier.fillMaxWidth(), contentAlignment = Alignment.TopStart) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 8.dp)
        ) {
            Text(
                text = title,
                style = MaterialTheme.typography.titleLarge,
                color = Color.White,
                modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp)
            )

            DrawerExpandableSection(title = "Common", defaultExpanded = true) {
                DrawerPrimaryItem(
                    title = "Wayland",
                    onTap = actions.onDesktopClick,
                    onLongPress = actions.onDesktopLongPress,
                )

                if (showDebianDesktop) {
                    DrawerPrimaryItem(
                        title = "X11",
                        onTap = actions.onDebianDesktopClick,
                        onLongPress = actions.onDebianDesktopLongPress,
                    )
                }

                DrawerPrimaryItem(
                    title = "Terminal",
                    onTap = actions.onTerminalClick,
                )

                DrawerDropdownField(
                    label = "Launcher default",
                    value = labels.launcherDefaultLabel,
                    options = options.launcherDefaultOptions,
                    onSelect = {
                        actions.onLauncherDefaultSelect(it)
                        actions.onCloseDrawerRequest()
                    },
                )
            }

            Spacer(Modifier.height(6.dp))

            DrawerExpandableSection(title = "Desktop", defaultExpanded = true) {
                DrawerDropdownField(
                    label = "Vulkan",
                    value = labels.desktopVulkanLabel,
                    options = options.desktopVulkanOptions,
                    onSelect = {
                        actions.onDesktopVulkanSelect(it)
                        actions.onCloseDrawerRequest()
                    },
                )

                DrawerDropdownField(
                    label = "OpenGL",
                    value = labels.desktopOpenGLLabel,
                    options = options.desktopOpenGLOptions,
                    onSelect = {
                        actions.onDesktopOpenGLSelect(it)
                        actions.onCloseDrawerRequest()
                    },
                )

                DrawerExpandableSection(title = "View", defaultExpanded = true) {
                    DrawerDropdownField(
                        label = "Mouse mode",
                        value = labels.mouseModeLabel,
                        options = options.mouseModeOptions,
                        onSelect = {
                            actions.onMouseModeSelect(it)
                            actions.onCloseDrawerRequest()
                        },
                    )
                    DrawerDropdownField(
                        label = "Resolution",
                        value = labels.resolutionPercentLabel,
                        options = options.resolutionPercentOptions,
                        onSelect = {
                            actions.onResolutionPercentSelect(it)
                            actions.onCloseDrawerRequest()
                        },
                    )
                    DrawerDropdownField(
                        label = "Scale",
                        value = labels.scalePercentLabel,
                        options = options.scalePercentOptions,
                        onSelect = {
                            actions.onScalePercentSelect(it)
                            actions.onCloseDrawerRequest()
                        },
                    )
                }
                DrawerTextItem(title = "Keyboard", onClick = actions.onKeyboardClick)
            }

            Spacer(Modifier.height(6.dp))

            DrawerExpandableSection(title = "Terminal", defaultExpanded = true) {
                DrawerDropdownField(
                    label = "Appearance",
                    value = labels.terminalFontLabel,
                    options = options.terminalFontOptions,
                    onSelect = {
                        actions.onTerminalFontSelect(it)
                        actions.onCloseDrawerRequest()
                    },
                )

                DrawerDropdownField(
                    label = "Session",
                    value = labels.terminalSessionLabel,
                    options = options.terminalSessionOptions,
                    onSelect = {
                        actions.onTerminalSessionSelect(it)
                        actions.onCloseDrawerRequest()
                    },
                )
            }

            if (extraContent != null) {
                Spacer(Modifier.height(6.dp))
                extraContent()
            }
        }
    }
}

@Composable
private fun DrawerPrimaryItem(
    title: String,
    onTap: () -> Unit,
    onLongPress: (() -> Unit)?,
) {
    val mod = Modifier
        .fillMaxWidth()
        .pointerInput(title) {
            detectTapGestures(
                onTap = { onTap() },
                onLongPress = { onLongPress?.invoke() }
            )
        }
        .padding(vertical = 12.dp, horizontal = 12.dp)
    Text(
        text = title,
        style = MaterialTheme.typography.bodyLarge,
        color = MaterialTheme.colorScheme.primary,
        modifier = mod
    )
}

@Composable
private fun DrawerPrimaryItem(
    title: String,
    onTap: () -> Unit,
) = DrawerPrimaryItem(title = title, onTap = onTap, onLongPress = null)

@Composable
private fun DrawerTextItem(
    title: String,
    onClick: () -> Unit,
) {
    Text(
        text = title,
        style = MaterialTheme.typography.bodyLarge,
        color = MaterialTheme.colorScheme.primary,
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(vertical = 12.dp, horizontal = 12.dp)
    )
}

