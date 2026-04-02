package app.trierarch.shell

import android.content.Context
import android.graphics.Typeface
import androidx.annotation.FontRes
import androidx.core.content.res.ResourcesCompat
import app.trierarch.R

data class ShellTypefaceOption(
    val id: String,
    val label: String,
    @FontRes val fontRes: Int?
)

object ShellFonts {
    /** SharedPreferences key — stable for existing installs. */
    const val PREF_KEY = "terminal_font"
    const val DEFAULT_ID = "jetbrains_mono"

    val options: List<ShellTypefaceOption> = listOf(
        ShellTypefaceOption("jetbrains_mono", "JetBrains Mono", R.font.jetbrains_mono_regular),
        ShellTypefaceOption("ibm_plex_mono", "IBM Plex Mono", R.font.ibm_plex_mono_regular),
        ShellTypefaceOption("source_code_pro", "Source Code Pro", R.font.source_code_pro_regular),
        ShellTypefaceOption("noto_sans_mono", "Noto Sans Mono", R.font.noto_sans_mono_regular),
        ShellTypefaceOption("droid_sans_mono", "Droid Sans Mono", R.font.droid_sans_mono),
        ShellTypefaceOption("system", "System monospace", null)
    )

    fun indexForPref(prefKey: String?): Int {
        val k = prefKey?.takeIf { it.isNotBlank() } ?: DEFAULT_ID
        val i = options.indexOfFirst { it.id == k }
        return if (i >= 0) i else 0
    }

    fun typefaceForPref(context: Context, prefKey: String?): Typeface {
        val key = prefKey?.takeIf { it.isNotBlank() } ?: DEFAULT_ID
        val choice = options.find { it.id == key } ?: options[0]
        val res = choice.fontRes ?: return Typeface.MONOSPACE
        return ResourcesCompat.getFont(context, res) ?: Typeface.MONOSPACE
    }
}
