package app.trierarch.ui.runtime

import android.app.Activity
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import kotlinx.coroutines.delay

/**
 * Activity Result flow for [StartupStoragePermissions].
 *
 * State is always derived from [StartupStoragePermissions.allGranted] only.
 *
 * - After resume: sync grants (user may have changed toggles in Settings).
 * - Initial request is delayed briefly so the activity window is ready (reduces OEM races).
 * - If the user denies in the system UI, [grantResults] is non-empty; we finish so the next cold start can ask again.
 * - Some devices deliver an empty map when the prompt cannot be shown; we do not finish in that case (avoids crash-loops).
 */
@Composable
fun MandatoryStartupStoragePermissionsEffect(
    onMandatoryGranted: (Boolean) -> Unit,
) {
    val context = LocalContext.current
    val activity = context as? ComponentActivity ?: return

    val launcher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { grantResults ->
        val granted = StartupStoragePermissions.allGranted(context)
        onMandatoryGranted(granted)
        if (!granted && grantResults.isNotEmpty()) {
            val act = context as? Activity ?: return@rememberLauncherForActivityResult
            act.runOnUiThread {
                if (!StartupStoragePermissions.allGranted(act)) {
                    act.finish()
                }
            }
        }
    }

    LaunchedEffect(Unit) {
        delay(120)
        val granted = StartupStoragePermissions.allGranted(context)
        onMandatoryGranted(granted)
        if (!granted) {
            launcher.launch(StartupStoragePermissions.requiredPermissions())
        }
    }

    DisposableEffect(activity) {
        val observer = LifecycleEventObserver { _, event ->
            if (event == Lifecycle.Event.ON_RESUME) {
                onMandatoryGranted(StartupStoragePermissions.allGranted(context))
            }
        }
        activity.lifecycle.addObserver(observer)
        onDispose { activity.lifecycle.removeObserver(observer) }
    }
}
