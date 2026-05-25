package app.trierarch.ui.drawer.pages

import android.widget.Toast
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.unit.dp
import com.winlator.WinlatorHost
import com.winlator.container.ContainerManager
import com.winlator.core.FileUtils
import org.json.JSONObject
import app.trierarch.ui.containers.CreateContainerRoute
import app.trierarch.ui.drawer.menu.DrawerExpandableSection
import app.trierarch.ui.drawer.menu.DrawerPageHeader
import app.trierarch.ui.drawer.menu.drawerEndPadding
import app.trierarch.ui.drawer.menu.drawerRowTextStyle
import app.trierarch.ui.drawer.menu.drawerRowVerticalPadding
import app.trierarch.ui.drawer.menu.drawerStartPadding

private data class DrawerContainerRow(
    val id: Int,
    val name: String,
)

@Composable
fun WineDrawerPage(
    winlatorHost: WinlatorHost? = null,
    onCloseDrawer: () -> Unit = {},
    onRunContainer: (Int) -> Unit = {},
    onExitContainer: () -> Unit = {},
) {
    val accent = drawerPageAccent()
    val context = LocalContext.current
    val containerManager = remember(context) { ContainerManager(context) }
    var containers by remember { mutableStateOf<List<DrawerContainerRow>>(emptyList()) }
    var createDialogOpen by remember { mutableStateOf(false) }
    var editPrefill by remember { mutableStateOf<JSONObject?>(null) }
    var removeTarget by remember { mutableStateOf<DrawerContainerRow?>(null) }

    fun refreshContainers() {
        containers = containerManager.containers.map { container ->
            DrawerContainerRow(
                id = container.id,
                name = container.name,
            )
        }
    }

    fun openContainerEditor(containerId: Int) {
        val container = containerManager.getContainerById(containerId) ?: return
        val rawConfig = FileUtils.readString(container.configFile)
        if (rawConfig.isNullOrBlank()) {
            Toast.makeText(context, "Could not read container config", Toast.LENGTH_SHORT).show()
            return
        }
        try {
            editPrefill = JSONObject(rawConfig)
        } catch (_: Exception) {
            Toast.makeText(context, "Invalid container config", Toast.LENGTH_SHORT).show()
        }
    }

    fun duplicateContainer(containerId: Int) {
        val container = containerManager.getContainerById(containerId) ?: return
        containerManager.duplicateContainerAsync(container) {
            refreshContainers()
        }
    }

    LaunchedEffect(Unit) {
        refreshContainers()
    }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp)
    ) {
        DrawerPageHeader(
            title = "Wine",
            onClose = onCloseDrawer,
        )

        if (winlatorHost == null) {
            DrawerExpandableSection(title = "List", defaultExpanded = false) {
                if (containers.isEmpty()) {
                    Text(
                        text = "No containers yet",
                        style = drawerRowTextStyle(),
                        color = accent.copy(alpha = 0.72f),
                        modifier = Modifier.padding(
                            start = drawerStartPadding(),
                            end = drawerEndPadding(),
                            top = drawerRowVerticalPadding(),
                            bottom = drawerRowVerticalPadding(),
                        ),
                    )
                } else {
                    containers.forEach { container ->
                        DrawerContainerItem(
                            name = container.name,
                            onRunClick = {
                                onRunContainer(container.id)
                                onCloseDrawer()
                            },
                            onEditClick = { openContainerEditor(container.id) },
                            onDuplicateClick = { duplicateContainer(container.id) },
                            onRemoveClick = { removeTarget = container },
                        )
                    }
                }
                DrawerContainerAddItem(
                    onClick = { createDialogOpen = true },
                )
            }
        } else {
            DrawerTextItem("Keyboard") { winlatorHost.showKeyboard(); onCloseDrawer() }
            DrawerTextItem("Input Controls") { winlatorHost.showInputControlsDialog(); onCloseDrawer() }
            DrawerTextItem("Toggle Fullscreen") { winlatorHost.toggleFullscreen(); onCloseDrawer() }
            DrawerTextItem("Task Manager") { winlatorHost.showTaskManagerDialog(); onCloseDrawer() }
            DrawerTextItem("Active Windows") { winlatorHost.showActiveWindowsDialog(); onCloseDrawer() }
            DrawerTextItem("Magnifier") { winlatorHost.toggleMagnifier(); onCloseDrawer() }
            DrawerTextItem("Screen Effect") { winlatorHost.showScreenEffectDialog(); onCloseDrawer() }
            DrawerTextItem("Logs") { winlatorHost.showDebugDialog(); onCloseDrawer() }
            DrawerTextItem("Touchpad Help") { winlatorHost.showTouchpadHelpDialog(); onCloseDrawer() }
            DrawerTextItem("Exit") {
                winlatorHost.exit()
                onExitContainer()
                onCloseDrawer()
            }
        }
    }

    if (createDialogOpen) {
        CreateContainerRoute(
            initialName = "Container-${containerManager.containers.size + 1}",
            prefillData = null,
            onDismiss = { createDialogOpen = false },
            onCreate = { data: JSONObject ->
                containerManager.createContainerAsync(data) { container ->
                    if (container != null) {
                        refreshContainers()
                        createDialogOpen = false
                    } else {
                        Toast.makeText(context, "Could not create container", Toast.LENGTH_LONG).show()
                    }
                }
            },
        )
    }

    removeTarget?.let { target ->
        AlertDialog(
            onDismissRequest = { removeTarget = null },
            title = { Text("Remove container?") },
            text = { Text("Remove \"${target.name}\"? This cannot be undone.") },
            confirmButton = {
                TextButton(
                    onClick = {
                        val container = containerManager.getContainerById(target.id)
                        removeTarget = null
                        if (container != null) {
                            containerManager.removeContainerAsync(container) {
                                refreshContainers()
                            }
                        }
                    },
                ) { Text("Remove") }
            },
            dismissButton = {
                TextButton(onClick = { removeTarget = null }) { Text("Cancel") }
            },
        )
    }

    editPrefill?.let { prefill ->
        val id = prefill.optInt("id", -1)
        CreateContainerRoute(
            initialName = prefill.optString("name", "Container"),
            prefillData = prefill,
            onDismiss = { editPrefill = null },
            onCreate = { data: JSONObject ->
                if (id <= 0) {
                    editPrefill = null
                    return@CreateContainerRoute
                }
                val container = containerManager.getContainerById(id)
                if (container == null) {
                    editPrefill = null
                    return@CreateContainerRoute
                }
                containerManager.updateContainerAsync(container, data) {
                    refreshContainers()
                    editPrefill = null
                }
            },
        )
    }
}

@Composable
private fun DrawerContainerAddItem(
    onClick: () -> Unit,
) {
    val accent = drawerPageAccent()
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = drawerStartPadding(), end = 4.dp, top = 4.dp, bottom = 4.dp)
            .height(48.dp)
            .clickable(onClick = onClick)
            .border(
                width = 1.dp,
                color = accent.copy(alpha = 0.85f),
                shape = RoundedCornerShape(10.dp),
            ),
        contentAlignment = Alignment.Center,
    ) {
        Icon(
            imageVector = Icons.Filled.Add,
            contentDescription = "Add container",
            tint = accent,
        )
    }
}

@Composable
private fun DrawerContainerItem(
    name: String,
    onRunClick: () -> Unit,
    onEditClick: () -> Unit,
    onDuplicateClick: () -> Unit,
    onRemoveClick: () -> Unit,
) {
    val accent = drawerPageAccent()
    var menuOpen by remember { mutableStateOf(false) }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = drawerStartPadding(), end = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = name,
            style = drawerRowTextStyle(),
            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.9f),
            modifier = Modifier
                .weight(1f)
                .padding(
                    top = drawerRowVerticalPadding(),
                    bottom = drawerRowVerticalPadding(),
                ),
        )
        IconButton(onClick = onRunClick) {
            Icon(
                imageVector = Icons.Filled.PlayArrow,
                contentDescription = "Run container",
                tint = accent,
            )
        }
        Box {
            IconButton(onClick = { menuOpen = true }) {
                Icon(
                    imageVector = Icons.Filled.MoreVert,
                    contentDescription = "Container actions",
                    tint = accent,
                )
            }
            DropdownMenu(
                expanded = menuOpen,
                onDismissRequest = { menuOpen = false },
            ) {
                DropdownMenuItem(
                    text = { Text("Edit") },
                    leadingIcon = {
                        Icon(Icons.Filled.Edit, contentDescription = null)
                    },
                    onClick = {
                        menuOpen = false
                        onEditClick()
                    },
                )
                DropdownMenuItem(
                    text = { Text("Duplicate") },
                    leadingIcon = {
                        Icon(Icons.Filled.ContentCopy, contentDescription = null)
                    },
                    onClick = {
                        menuOpen = false
                        onDuplicateClick()
                    },
                )
                DropdownMenuItem(
                    text = { Text("Remove") },
                    leadingIcon = {
                        Icon(Icons.Filled.Delete, contentDescription = null)
                    },
                    onClick = {
                        menuOpen = false
                        onRemoveClick()
                    },
                )
            }
        }
    }
}

@Composable
private fun DrawerTextItem(
    title: String,
    onClick: () -> Unit,
) {
    val accent = drawerPageAccent()
    Text(
        text = title,
        style = drawerRowTextStyle(),
        color = accent,
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(
                start = drawerStartPadding(),
                end = drawerEndPadding(),
                top = drawerRowVerticalPadding(),
                bottom = drawerRowVerticalPadding(),
            )
    )
}

