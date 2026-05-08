package app.trierarch.ui.containers

import android.widget.Toast
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.winlator.container.ContainerManager
import com.winlator.core.FileUtils
import org.json.JSONObject

private data class ContainerRow(
    val id: Int,
    val name: String,
    val dirName: String,
)

@Composable
fun ContainersScreen(
    modifier: Modifier = Modifier,
    onRunContainer: (containerId: Int) -> Unit = {},
) {
    val context = LocalContext.current
    val manager = remember(context) { ContainerManager(context) }

    var containers by remember { mutableStateOf<List<ContainerRow>>(emptyList()) }
    var createDialogOpen by remember { mutableStateOf(false) }
    var editPrefill by remember { mutableStateOf<JSONObject?>(null) }
    var removeTarget by remember { mutableStateOf<ContainerRow?>(null) }

    fun refresh() {
        containers = manager.containers.map { c ->
            ContainerRow(
                id = c.id,
                name = c.name,
                dirName = c.rootDir?.name ?: "xuser-${c.id}",
            )
        }
    }

    LaunchedEffect(Unit) {
        refresh()
    }

    Box(modifier = modifier.fillMaxSize()) {
        Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
            Text(
                text = "Containers",
                color = Color.White.copy(alpha = 0.9f),
                style = MaterialTheme.typography.titleLarge,
            )
            Spacer(Modifier.height(12.dp))

            if (containers.isEmpty()) {
                Text(
                    text = "No containers yet. Tap + to create one.",
                    color = Color.White.copy(alpha = 0.65f),
                    style = MaterialTheme.typography.bodyMedium,
                )
            } else {
                LazyColumn(
                    verticalArrangement = Arrangement.spacedBy(10.dp),
                    modifier = Modifier.fillMaxSize(),
                ) {
                    items(containers, key = { it.id }) { c ->
                        var rowMenuOpen by remember(c.id) { mutableStateOf(false) }
                        Surface(
                            tonalElevation = 0.dp,
                            shadowElevation = 0.dp,
                            color = Color.White.copy(alpha = 0.06f),
                            modifier = Modifier.fillMaxWidth(),
                        ) {
                            Row(
                                modifier = Modifier.fillMaxWidth().padding(14.dp),
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.SpaceBetween,
                            ) {
                                Column(modifier = Modifier.weight(1f)) {
                                    Text(
                                        text = c.name,
                                        color = Color.White.copy(alpha = 0.92f),
                                        style = MaterialTheme.typography.titleMedium,
                                    )
                                    Spacer(Modifier.height(4.dp))
                                    Text(
                                        text = "id=${c.id}  dir=${c.dirName}",
                                        color = Color.White.copy(alpha = 0.6f),
                                        style = MaterialTheme.typography.bodySmall,
                                    )
                                }
                                Box {
                                    IconButton(onClick = { rowMenuOpen = true }) {
                                        Icon(
                                            Icons.Default.MoreVert,
                                            contentDescription = "Container actions",
                                            tint = Color.White.copy(alpha = 0.85f),
                                        )
                                    }
                                    DropdownMenu(
                                        expanded = rowMenuOpen,
                                        onDismissRequest = { rowMenuOpen = false },
                                    ) {
                                        DropdownMenuItem(
                                            text = { Text("Run") },
                                            onClick = {
                                                rowMenuOpen = false
                                                onRunContainer(c.id)
                                            },
                                        )
                                        DropdownMenuItem(
                                            text = { Text("Edit") },
                                            onClick = {
                                                rowMenuOpen = false
                                                val cont = manager.getContainerById(c.id)
                                                if (cont == null) return@DropdownMenuItem
                                                val raw = FileUtils.readString(cont.configFile)
                                                if (raw.isNullOrBlank()) {
                                                    Toast.makeText(context, "Could not read container config", Toast.LENGTH_SHORT).show()
                                                    return@DropdownMenuItem
                                                }
                                                try {
                                                    editPrefill = JSONObject(raw)
                                                } catch (_: Exception) {
                                                    Toast.makeText(context, "Invalid container config", Toast.LENGTH_SHORT).show()
                                                }
                                            },
                                        )
                                        DropdownMenuItem(
                                            text = { Text("Duplicate") },
                                            onClick = {
                                                rowMenuOpen = false
                                                val cont = manager.getContainerById(c.id) ?: return@DropdownMenuItem
                                                manager.duplicateContainerAsync(cont) {
                                                    refresh()
                                                }
                                            },
                                        )
                                        DropdownMenuItem(
                                            text = { Text("Remove") },
                                            onClick = {
                                                rowMenuOpen = false
                                                removeTarget = c
                                            },
                                        )
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        FloatingActionButton(
            onClick = {
                createDialogOpen = true
            },
            containerColor = Color(0xFF8B1E3F),
            modifier = Modifier
                .align(Alignment.BottomEnd)
                .padding(18.dp),
        ) {
            Icon(Icons.Default.Add, contentDescription = "Add container")
        }
    }

    removeTarget?.let { target ->
        AlertDialog(
            onDismissRequest = { removeTarget = null },
            title = { Text("Remove container?") },
            text = { Text("Remove \"${target.name}\"? This cannot be undone.") },
            confirmButton = {
                TextButton(
                    onClick = {
                        val cont = manager.getContainerById(target.id)
                        removeTarget = null
                        if (cont != null) {
                            manager.removeContainerAsync(cont) {
                                refresh()
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

    if (createDialogOpen) {
        CreateContainerRoute(
            initialName = "Container-${manager.containers.size + 1}",
            prefillData = null,
            onDismiss = { createDialogOpen = false },
            onCreate = { data: JSONObject ->
                manager.createContainerAsync(data) { container ->
                    if (container != null) {
                        refresh()
                        createDialogOpen = false
                    } else {
                        Toast.makeText(context, "Could not create container", Toast.LENGTH_LONG).show()
                    }
                }
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
                val cont = manager.getContainerById(id)
                if (cont == null) {
                    editPrefill = null
                    return@CreateContainerRoute
                }
                manager.updateContainerAsync(cont, data) {
                    refresh()
                    editPrefill = null
                }
            },
        )
    }
}
