package app.trierarch.terminal

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context

/** Clipboard operations required by a terminal session, independent of Android UI classes. */
interface TerminalClipboard {
    fun copy(text: String)

    fun paste(): String?
}

/** Android clipboard adapter for the activity-owned default terminal session. */
class AndroidTerminalClipboard(context: Context) : TerminalClipboard {
    private val applicationContext = context.applicationContext
    private val clipboard = applicationContext.getSystemService(ClipboardManager::class.java)

    override fun copy(text: String) {
        clipboard.setPrimaryClip(ClipData.newPlainText(null, text))
    }

    override fun paste(): String? {
        if (!clipboard.hasPrimaryClip()) return null
        return clipboard.primaryClip
            ?.takeIf { it.itemCount > 0 }
            ?.getItemAt(0)
            ?.coerceToText(applicationContext)
            ?.toString()
    }
}
