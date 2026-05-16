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
import androidx.compose.ui.Alignment
import app.trierarch.ui.drawer.pages.drawerPageAccent

@Composable
fun DrawerMenu(
    title: String,
    labels: DrawerMenuLabels,
    options: DrawerMenuOptions,
    actions: DrawerMenuActions,
    modifier: Modifier = Modifier,
    visibility: DrawerMenuVisibility = DrawerMenuVisibility(
        launcherDefault = true,
        graphics = true,
        terminalSettings = true,
        showWaylandEntry = true,
        terminalEntry = true,
        desktopView = true,
        keyboard = true,
    ),
    extraContent: (@Composable () -> Unit)? = null,
) {
    val accent = drawerPageAccent()

    Box(modifier = modifier.fillMaxWidth(), contentAlignment = Alignment.TopStart) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 8.dp)
        ) {
            Text(
                text = title,
                style = MaterialTheme.typography.titleLarge,
                color = accent,
                modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp)
            )

            if (visibility.launcherDefault || visibility.showWaylandEntry || visibility.showX11Entry
                || visibility.terminalEntry
            ) {
                DrawerExpandableSection(title = "Common", defaultExpanded = true) {
                    if (visibility.showWaylandEntry) {
                        DrawerPrimaryItem(
                            title = "Wayland",
                            onTap = actions.onWaylandEntryClick,
                            onLongPress = actions.onWaylandEntryLongPress,
                        )
                    }

                    if (visibility.showX11Entry) {
                        DrawerPrimaryItem(
                            title = "X11",
                            onTap = actions.onX11EntryClick,
                            onLongPress = actions.onX11EntryLongPress,
                        )
                    }

                    if (visibility.terminalEntry) {
                        DrawerPrimaryItem(
                            title = "Terminal",
                            onTap = actions.onTerminalClick,
                        )
                    }

                    if (visibility.launcherDefault) {
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
                }
            }

            if (visibility.graphics || visibility.desktopView || visibility.keyboard) {
                Spacer(Modifier.height(6.dp))

                DrawerExpandableSection(title = "Desktop", defaultExpanded = true) {
                    if (visibility.graphics) {
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
                    }

                    if (visibility.desktopView) {
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
                    }
                    if (visibility.keyboard) {
                        DrawerTextItem(title = "Keyboard", onClick = actions.onKeyboardClick)
                    }
                }
            }

            if (visibility.terminalSettings) {
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
    val accent = drawerPageAccent()
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
        color = accent,
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
    val accent = drawerPageAccent()
    Text(
        text = title,
        style = MaterialTheme.typography.bodyLarge,
        color = accent,
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(vertical = 12.dp, horizontal = 12.dp)
    )
}

