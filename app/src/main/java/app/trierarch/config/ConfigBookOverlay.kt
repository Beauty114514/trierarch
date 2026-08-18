package app.trierarch.config

import android.app.AlertDialog
import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import kotlin.math.abs

/** An adaptive in-window glass book; each ordinary page is one TOML file. */
class ConfigBookOverlay(
    context: Context,
    private val onDismiss: () -> Unit,
    private val onStartInternalShell: () -> Unit,
    private val onStartProot: (ProfileStore.ProotProfile) -> Unit,
    private val onStartChroot: (ProfileStore.ChrootProfile) -> Unit,
    private val onStartDroidspaces: (ProfileStore.DroidspacesProfile) -> Unit,
    private val isRuntimeRunning: () -> Boolean,
    private val onStopRuntime: () -> Unit,
    private val onStartX11: (onReady: () -> Unit, onFailure: (String) -> Unit) -> Unit,
    private val onShowTerminal: () -> Unit,
) : FrameLayout(context) {
    private val store = ProfileStore(context.applicationContext)
    private val title = label("Configuration", 19f, Typeface.BOLD)
    private val counter = label("", 12f, Typeface.NORMAL)
    private val page = FrameLayout(context)
    private lateinit var panel: LinearLayout
    private val actions = LinearLayout(context).apply { gravity = Gravity.CENTER }
    private val delete = button("Delete") { deleteCurrent() }
    private val config = button("Config") { editCurrent() }
    private val confirm = button("Confirm") { confirmCurrent() }
    private val start = button("Start") { startCurrent() }
    private val stop = button("Stop") { stopCurrent() }
    private var files = emptyList<java.io.File>()
    private var index = 0
    private var replacing: java.io.File? = null
    private var editor: EditText? = null
    private var touchX = 0f

    init {
        setBackgroundColor(Color.argb(56, 0, 0, 0))
        isClickable = true
        setOnClickListener { dismiss() }

        panel = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), dp(12), dp(16), dp(12))
            background = glassBackground()
            elevation = dp(18).toFloat()
            setOnClickListener { }
            setOnTouchListener { _, event ->
                if (editor != null) return@setOnTouchListener false
                when (event.actionMasked) {
                    MotionEvent.ACTION_DOWN -> touchX = event.rawX
                    MotionEvent.ACTION_UP -> {
                        val distance = event.rawX - touchX
                        if (abs(distance) > dp(72)) {
                            if (distance < 0) next() else previous()
                            return@setOnTouchListener true
                        }
                    }
                }
                false
            }
        }
        val header = LinearLayout(context).apply { gravity = Gravity.CENTER_VERTICAL }
        header.addView(button("‹") { previous() }, weight(0.16f))
        title.gravity = Gravity.CENTER
        header.addView(title, weight(0.68f))
        header.addView(button("›") { next() }, weight(0.16f))
        panel.addView(header, LinearLayout.LayoutParams.MATCH_PARENT, dp(46))
        counter.gravity = Gravity.CENTER
        panel.addView(counter, LinearLayout.LayoutParams.MATCH_PARENT, dp(22))
        panel.addView(page, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
        listOf(delete, config, confirm, start, stop).forEach { actions.addView(it, weight(1f)) }
        panel.addView(actions, LinearLayout.LayoutParams.MATCH_PARENT, dp(52))

        // onMeasure replaces this fallback before the first frame.
        addView(panel, LayoutParams(initialPanelWidth(), initialPanelHeight()).apply {
            gravity = Gravity.CENTER
        })
        reload()
    }

    /**
     * The sole authority for book geometry. Android invokes this with the current
     * content bounds after rotation, folding, and IME resize, so no posted layout
     * work or stale display metrics can race with it.
     */
    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val availableWidth = View.MeasureSpec.getSize(widthMeasureSpec)
        val availableHeight = View.MeasureSpec.getSize(heightMeasureSpec)
        val (bookWidth, bookHeight) = bookSize(availableWidth, availableHeight)
        val params = panel.layoutParams as LayoutParams
        if (params.width != bookWidth || params.height != bookHeight) {
            params.width = bookWidth
            params.height = bookHeight
            panel.layoutParams = params
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec)
    }

    fun dismiss() {
        (parent as? FrameLayout)?.removeView(this)
        onDismiss()
    }

    private fun reload(selected: java.io.File? = null) {
        files = store.list()
        index = selected?.let { files.indexOf(it).takeIf { found -> found >= 0 } } ?: index
        show(index.coerceIn(0, files.size))
    }

    private fun show(pageIndex: Int) {
        index = pageIndex.coerceIn(0, files.size)
        editor = null
        replacing = null
        page.removeAllViews()
        counter.text = "${index + 1} / ${files.size + 1}"
        if (index == files.size) showPlus() else showFile(files[index])
    }

    private fun showPlus() {
        title.text = "New configuration"
        actions.visibility = View.GONE
        page.addView(label("+", 72f, Typeface.NORMAL).apply {
            gravity = Gravity.CENTER
            setOnClickListener { edit(null, TEMPLATE) }
        }, FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT)
    }

    private fun showFile(file: java.io.File) {
        title.text = file.name
        actions.visibility = View.VISIBLE
        delete.isEnabled = true; config.isEnabled = true; confirm.isEnabled = false; start.isEnabled = true; stop.isEnabled = false
        val content = runCatching { store.read(file) }.getOrElse { "Unable to read ${file.name}: ${it.message}" }
        page.addView(ScrollView(context).apply {
            addView(label(content, 14f, Typeface.NORMAL).apply {
                typeface = Typeface.MONOSPACE
                setTextIsSelectable(true)
                setPadding(dp(10), dp(10), dp(10), dp(10))
            })
        }, FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT)
        Thread {
            val running = runCatching { isRuntimeRunning() }.getOrDefault(false)
            post {
                if (editor == null && files.getOrNull(index) == file) stop.isEnabled = running
            }
        }.start()
    }

    private fun edit(file: java.io.File?, content: String) {
        replacing = file
        title.text = file?.name ?: "New configuration"
        actions.visibility = View.VISIBLE
        delete.isEnabled = true; config.isEnabled = false; confirm.isEnabled = true; start.isEnabled = false; stop.isEnabled = false
        page.removeAllViews()
        editor = EditText(context).apply {
            setText(content); setTextColor(Color.rgb(240, 240, 240)); setTextSize(14f)
            typeface = Typeface.MONOSPACE; gravity = Gravity.TOP or Gravity.START
            setPadding(dp(10), dp(10), dp(10), dp(10)); setSingleLine(false); minLines = 14
            setBackgroundColor(Color.argb(100, 0, 0, 0))
        }
        page.addView(editor, FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT)
        editor?.requestFocus()
    }

    private fun editCurrent() { files.getOrNull(index)?.let { edit(it, store.read(it)) } }

    private fun confirmCurrent() {
        val content = editor?.text?.toString() ?: return
        runCatching { store.save(content, replacing) }.onSuccess { reload(it) }.onFailure { toast(it.message ?: "Unable to save") }
    }

    private fun deleteCurrent() {
        if (editor != null && replacing == null) { show(files.size); return }
        val file = replacing ?: files.getOrNull(index) ?: return
        AlertDialog.Builder(context).setMessage("Delete ${file.name}?").setNegativeButton("Cancel", null)
            .setPositiveButton("Delete") { _, _ -> runCatching { store.delete(file) }.onSuccess { reload() }.onFailure { toast(it.message ?: "Unable to delete") } }.show()
    }

    private fun startCurrent() {
        val file = files.getOrNull(index) ?: return
        runCatching { store.read(file) }.onSuccess { content ->
            runCatching { store.validate(content); store.runtime(content) to store.display(content) }.onSuccess { (runtime, display) ->
                val withDisplay: (() -> Unit) -> Unit = { startRuntime ->
                    if (display == ProfileStore.DISPLAY_X11) {
                        onStartX11(
                            {
                                runCatching(startRuntime).onFailure { error ->
                                    onShowTerminal()
                                    toast(error.message ?: "Unable to start configuration")
                                }
                            },
                            { message ->
                                onShowTerminal()
                                toast(message)
                            },
                        )
                        dismiss()
                    } else {
                        launch {
                            onShowTerminal()
                            startRuntime()
                        }
                    }
                }
                when (runtime) {
                    ProfileStore.RUNTIME_INTERNAL_SHELL -> withDisplay(onStartInternalShell)
                    ProfileStore.RUNTIME_PROOT -> runCatching { store.prootProfile(content) }
                        .onSuccess { profile -> withDisplay { onStartProot(profile) } }
                        .onFailure { toast(it.message ?: "Invalid PRoot configuration") }
                    ProfileStore.RUNTIME_CHROOT -> runCatching { store.chrootProfile(content) }
                        .onSuccess { profile -> withDisplay { onStartChroot(profile) } }
                        .onFailure { toast(it.message ?: "Invalid chroot configuration") }
                    ProfileStore.RUNTIME_DROIDSPACES -> runCatching { store.droidspacesProfile(content) }
                        .onSuccess { profile -> withDisplay { onStartDroidspaces(profile) } }
                        .onFailure { toast(it.message ?: "Invalid DroidSpaces configuration") }
                    else -> toast("Runtime '$runtime' cannot be started yet")
                }
            }.onFailure { toast(it.message ?: "Invalid configuration") }
        }.onFailure { toast(it.message ?: "Unable to read") }
    }

    private fun stopCurrent() {
        if (!isRuntimeRunning()) {
            stop.isEnabled = false
            return
        }
        launch {
            onStopRuntime()
            onShowTerminal()
        }
    }

    private fun previous() { if (editor == null && index > 0) show(index - 1) }
    private fun next() { if (editor == null && index < files.size) show(index + 1) }
    private fun launch(action: () -> Unit) {
        runCatching(action)
            .onSuccess { dismiss() }
            .onFailure { toast(it.message ?: "Unable to start configuration") }
    }
    private fun button(text: String, action: () -> Unit) = Button(context).apply { this.text = text; isAllCaps = false; setOnClickListener { action() } }
    private fun label(text: String, size: Float, style: Int) = TextView(context).apply { this.text = text; setTextColor(Color.rgb(242, 242, 242)); setTextSize(size); typeface = Typeface.create(Typeface.MONOSPACE, style) }
    private fun weight(value: Float) = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, value)
    private fun initialPanelWidth() = bookSize(resources.displayMetrics.widthPixels, resources.displayMetrics.heightPixels).first
    private fun initialPanelHeight() = bookSize(resources.displayMetrics.widthPixels, resources.displayMetrics.heightPixels).second

    private fun bookSize(availableWidth: Int, availableHeight: Int): Pair<Int, Int> {
        val maxWidth = availableWidth * BOOK_MAX_WIDTH_FRACTION
        val maxHeight = availableHeight * BOOK_MAX_HEIGHT_FRACTION
        return if (availableWidth <= availableHeight) {
            val width = minOf(maxWidth, maxHeight / ROOT_TWO).toInt().coerceAtLeast(1)
            width to (width * ROOT_TWO).toInt().coerceAtLeast(1)
        } else {
            val height = minOf(maxHeight, maxWidth / ROOT_TWO).toInt().coerceAtLeast(1)
            (height * ROOT_TWO).toInt().coerceAtLeast(1) to height
        }
    }

    private fun dp(value: Int) = (value * resources.displayMetrics.density).toInt()
    private fun toast(message: String) = Toast.makeText(context, message, Toast.LENGTH_LONG).show()
    private fun glassBackground() = GradientDrawable().apply {
        shape = GradientDrawable.RECTANGLE
        setColor(Color.argb(220, 32, 32, 36))
        cornerRadius = dp(20).toFloat()
        setStroke(dp(1), Color.argb(145, 255, 255, 255))
    }
    private companion object {
        const val BOOK_MAX_WIDTH_FRACTION = .88f
        const val BOOK_MAX_HEIGHT_FRACTION = .78f
        const val ROOT_TWO = 1.41421356f
        val TEMPLATE = "id = \"\"\nname = \"\"\nruntime = \"internal-shell\"\n\n[display]\ntype = \"none\"\n"
    }
}
