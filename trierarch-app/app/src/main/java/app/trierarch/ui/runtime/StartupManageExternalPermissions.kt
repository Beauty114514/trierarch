package app.trierarch.ui.runtime

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.Settings

/**
 * Optional “All files access” ([MANAGE_EXTERNAL_STORAGE]) special access (API 30+).
 * Distinct from [StartupStoragePermissions] runtime grants; use [isGranted] / system settings only.
 */
object StartupManageExternalPermissions {

    /** Pre-Android 11 does not use this special access; treated as satisfied. */
    fun isGranted(context: Context): Boolean =
        Build.VERSION.SDK_INT < Build.VERSION_CODES.R || Environment.isExternalStorageManager()

    fun openAllFilesAccessSettings(context: Context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return
        try {
            context.startActivity(
                Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).apply {
                    data = Uri.parse("package:${context.packageName}")
                },
            )
        } catch (_: Throwable) {
            try {
                context.startActivity(
                    Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS).apply {
                        data = Uri.fromParts("package", context.packageName, null)
                    },
                )
            } catch (_: Throwable) {
            }
        }
    }
}
