package app.trierarch.ui

import android.content.Context
import android.content.SharedPreferences
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import android.widget.FrameLayout
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.content.edit
import app.trierarch.R
import kotlin.math.abs
import kotlin.math.roundToInt

/**
 * A draggable, in-app launcher orb.
 *
 * Its position is saved as fractions of the usable host area, so it survives
 * size changes without depending on a fixed screen resolution. This is not a
 * system overlay and needs no special Android permission.
 */
class FloatingMenuOrbView(
    context: Context,
    private val preferences: SharedPreferences,
    private val onClick: () -> Unit,
) : AppCompatImageView(context) {
    private val touchSlop = ViewConfiguration.get(context).scaledTouchSlop
    private val sizePx = context.dp(48)

    private var centerXFraction = preferences.getFloat(PREF_CENTER_X, DEFAULT_CENTER_X)
    private var centerYFraction = preferences.getFloat(PREF_CENTER_Y, DEFAULT_CENTER_Y)
    private var imeBottomInset = 0
    private var downRawX = 0f
    private var downRawY = 0f
    private var startX = 0f
    private var startY = 0f
    private var dragging = false

    init {
        layoutParams = FrameLayout.LayoutParams(sizePx, sizePx)
        contentDescription = "Configuration"
        scaleType = ScaleType.CENTER_INSIDE
        setPadding(context.dp(8), context.dp(8), context.dp(8), context.dp(8))
        setImageResource(R.drawable.ic_launcher_foreground)
        background = GradientDrawable().apply {
            shape = GradientDrawable.OVAL
            setColor(Color.argb(184, 26, 26, 26))
            setStroke(context.dp(1), Color.argb(140, 255, 255, 255))
        }
        elevation = context.dp(8).toFloat()
        isClickable = true
        isFocusable = true
    }

    /** Keeps the orb above the IME while retaining its saved relative position. */
    fun setImeBottomInset(inset: Int) {
        if (imeBottomInset == inset) return
        imeBottomInset = inset.coerceAtLeast(0)
        placeFromSavedPosition()
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        post(::placeFromSavedPosition)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean = when (event.actionMasked) {
        MotionEvent.ACTION_DOWN -> {
            downRawX = event.rawX
            downRawY = event.rawY
            startX = x
            startY = y
            dragging = false
            true
        }

        MotionEvent.ACTION_MOVE -> {
            val deltaX = event.rawX - downRawX
            val deltaY = event.rawY - downRawY
            if (!dragging && (abs(deltaX) > touchSlop || abs(deltaY) > touchSlop)) {
                dragging = true
            }
            if (dragging) moveTo(
                (startX + deltaX).roundToInt(),
                (startY + deltaY).roundToInt(),
            )
            true
        }

        MotionEvent.ACTION_UP -> {
            if (dragging) persistPosition() else performClick()
            true
        }

        MotionEvent.ACTION_CANCEL -> {
            if (dragging) persistPosition()
            true
        }

        else -> super.onTouchEvent(event)
    }

    override fun performClick(): Boolean {
        super.performClick()
        onClick()
        return true
    }

    private fun placeFromSavedPosition() {
        val host = parent as? View ?: return
        if (host.width == 0 || usableHeight(host) == 0) return
        moveTo(
            (centerXFraction * host.width - width / 2f).roundToInt(),
            (centerYFraction * usableHeight(host) - height / 2f).roundToInt(),
        )
    }

    private fun moveTo(requestedLeft: Int, requestedTop: Int) {
        val host = parent as? View ?: return
        val maxLeft = (host.width - width).coerceAtLeast(0)
        val maxTop = (usableHeight(host) - height).coerceAtLeast(0)
        x = requestedLeft.coerceIn(0, maxLeft).toFloat()
        y = requestedTop.coerceIn(0, maxTop).toFloat()
    }

    private fun persistPosition() {
        val host = parent as? View ?: return
        if (host.width == 0 || usableHeight(host) == 0) return
        centerXFraction = ((x + width / 2f) / host.width).coerceIn(0f, 1f)
        centerYFraction = ((y + height / 2f) / usableHeight(host)).coerceIn(0f, 1f)
        preferences.edit {
            putFloat(PREF_CENTER_X, centerXFraction)
            putFloat(PREF_CENTER_Y, centerYFraction)
        }
    }

    private fun usableHeight(host: View): Int = (host.height - imeBottomInset).coerceAtLeast(0)

    private fun Context.dp(value: Int): Int =
        (value * resources.displayMetrics.density).roundToInt()

    private companion object {
        const val PREF_CENTER_X = "menu_orb_center_x_fraction"
        const val PREF_CENTER_Y = "menu_orb_center_y_fraction"
        const val DEFAULT_CENTER_X = 0.88f
        const val DEFAULT_CENTER_Y = 0.42f
    }
}
