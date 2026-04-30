package app.trierarch.ui.runtime

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat

/**
 * Mandatory runtime storage/media permissions for Trierarch before native install.
 *
 * Decision rule: [allGranted] only ([ContextCompat.checkSelfPermission] vs [PackageManager.PERMISSION_GRANTED]).
 *
 * API 33+: [READ_MEDIA_IMAGES], [READ_MEDIA_VIDEO], [READ_MEDIA_AUDIO] (see app manifest).
 * API 32 and below: [READ_EXTERNAL_STORAGE] and [WRITE_EXTERNAL_STORAGE].
 *
 * Optional special access ([MANAGE_EXTERNAL_STORAGE]) is defined in [StartupManageExternalPermissions].
 */
object StartupStoragePermissions {

    fun requiredPermissions(): Array<String> =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            arrayOf(
                Manifest.permission.READ_MEDIA_IMAGES,
                Manifest.permission.READ_MEDIA_VIDEO,
                Manifest.permission.READ_MEDIA_AUDIO,
            )
        } else {
            arrayOf(
                Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
            )
        }

    fun allGranted(context: Context): Boolean =
        requiredPermissions().all { perm ->
            ContextCompat.checkSelfPermission(context, perm) == PackageManager.PERMISSION_GRANTED
        }
}
