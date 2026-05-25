package app.trierarch.ui.drawer.menu

import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.height
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
    topContent: (@Composable () -> Unit)? = null,
    desktopHeaderContent: (@Composable () -> Unit)? = null,
    desktopViewContent: (@Composable () -> Unit)? = null,
    extraContent: (@Composable () -> Unit)? = null,
    footerContent: (@Composable () -> Unit)? = null,
) {
    Box(modifier = modifier.fillMaxWidth(), contentAlignment = Alignment.TopStart) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 8.dp)
        ) {
            DrawerPageHeader(
                title = title,
                onClose = actions.onCloseDrawerRequest,
            )

            if (visibility.terminalEntry) {
                DrawerLaunchEntry(
                    title = "Terminal",
                    onTap = actions.onTerminalClick,
                )
            }

            if (topContent != null) {
                Spacer(Modifier.height(6.dp))
                topContent()
            }

            if (visibility.launcherDefault || visibility.showWaylandEntry || visibility.showX11Entry) {
                Spacer(Modifier.height(6.dp))

                DrawerExpandableSection(title = "Common", defaultExpanded = true) {
                    if (visibility.showWaylandEntry) {
                        DrawerLaunchEntry(
                            title = "Wayland",
                            onTap = actions.onWaylandEntryClick,
                            onLongPress = actions.onWaylandEntryLongPress,
                        )
                    }

                    if (visibility.showX11Entry) {
                        DrawerLaunchEntry(
                            title = "X11",
                            onTap = actions.onX11EntryClick,
                            onLongPress = actions.onX11EntryLongPress,
                        )
                    }

                    if (visibility.launcherDefault) {
                        DrawerDropdownField(
                            label = "Launcher default",
                            value = labels.launcherDefaultLabel,
                            options = options.launcherDefaultOptions,
                            onSelect = actions.onLauncherDefaultSelect,
                        )
                    }
                }
            }

            if (visibility.graphics || visibility.desktopView || visibility.keyboard
                || desktopHeaderContent != null
            ) {
                Spacer(Modifier.height(6.dp))

                DrawerExpandableSection(title = "Desktop", defaultExpanded = true) {
                    if (desktopHeaderContent != null) {
                        desktopHeaderContent()
                    }

                    if (visibility.graphics) {
                        DrawerExpandableSection(title = "Graphics Driver", defaultExpanded = true) {
                            if (visibility.vulkanDropdown) {
                                DrawerDropdownField(
                                    label = "Vulkan",
                                    value = labels.desktopVulkanLabel,
                                    options = options.desktopVulkanOptions,
                                    onSelect = actions.onDesktopVulkanSelect,
                                )
                            }

                            DrawerDropdownField(
                                label = "OpenGL",
                                value = labels.desktopOpenGLLabel,
                                options = options.desktopOpenGLOptions,
                                onSelect = actions.onDesktopOpenGLSelect,
                            )
                        }
                    }

                    if (visibility.desktopView) {
                        if (desktopViewContent != null) {
                            desktopViewContent()
                        } else {
                            DrawerExpandableSection(title = "View", defaultExpanded = true) {
                                DrawerDropdownField(
                                    label = "Mouse mode",
                                    value = labels.mouseModeLabel,
                                    options = options.mouseModeOptions,
                                    onSelect = actions.onMouseModeSelect,
                                )
                                DrawerDropdownField(
                                    label = "Resolution",
                                    value = labels.resolutionPercentLabel,
                                    options = options.resolutionPercentOptions,
                                    onSelect = actions.onResolutionPercentSelect,
                                )
                                DrawerDropdownField(
                                    label = "Scale",
                                    value = labels.scalePercentLabel,
                                    options = options.scalePercentOptions,
                                    onSelect = actions.onScalePercentSelect,
                                )
                            }
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
                    if (visibility.terminalFont) {
                        DrawerDropdownField(
                            label = "Appearance",
                            value = labels.terminalFontLabel,
                            options = options.terminalFontOptions,
                            onSelect = actions.onTerminalFontSelect,
                        )
                    }

                    if (visibility.terminalSession) {
                        DrawerDropdownField(
                            label = "Session",
                            value = labels.terminalSessionLabel,
                            options = options.terminalSessionOptions,
                            onSelect = actions.onTerminalSessionSelect,
                        )
                    }
                }
            }

            if (extraContent != null) {
                Spacer(Modifier.height(6.dp))
                extraContent()
            }

            if (footerContent != null) {
                Spacer(Modifier.height(6.dp))
                footerContent()
            }
        }
    }
}

@Composable
fun DrawerLaunchEntry(
    title: String,
    onTap: () -> Unit,
    onLongPress: (() -> Unit)? = null,
) {
    val accent = drawerPageAccent()
    val level = LocalDrawerHierarchyLevel.current
    val mod = Modifier
        .fillMaxWidth()
        .pointerInput(title) {
            detectTapGestures(
                onTap = { onTap() },
                onLongPress = { onLongPress?.invoke() }
            )
        }
        .padding(
            start = drawerStartPadding(level),
            end = drawerEndPadding(),
            top = drawerRowVerticalPadding(level),
            bottom = drawerRowVerticalPadding(level),
        )
    Text(
        text = title,
        style = drawerRowTextStyle(level),
        color = accent,
        modifier = mod
    )
}

@Composable
private fun DrawerTextItem(
    title: String,
    onClick: () -> Unit,
) {
    val accent = drawerPageAccent()
    val level = LocalDrawerHierarchyLevel.current
    Text(
        text = title,
        style = drawerRowTextStyle(level),
        color = accent,
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(
                start = drawerStartPadding(level),
                end = drawerEndPadding(),
                top = drawerRowVerticalPadding(level),
                bottom = drawerRowVerticalPadding(level),
            )
    )
}

