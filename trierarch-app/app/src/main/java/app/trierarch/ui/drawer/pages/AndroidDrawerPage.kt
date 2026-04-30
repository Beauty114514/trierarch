package app.trierarch.ui.drawer.pages

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

/** Host-side drawer tab (Android); no container rootfs session here. */
@Composable
fun AndroidDrawerPage() {
    Column(modifier = Modifier.fillMaxWidth().padding(16.dp)) {
        Text("Android", color = Color.White.copy(alpha = 0.9f), style = MaterialTheme.typography.titleLarge)
        Spacer(Modifier.height(12.dp))
        Text(
            text = "Host settings for this device live here when added.",
            color = Color.White.copy(alpha = 0.65f),
            style = MaterialTheme.typography.bodyMedium,
        )
        Spacer(Modifier.height(520.dp))
    }
}
