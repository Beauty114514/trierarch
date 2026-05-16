package com.winlator.widget;

import android.content.Context;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import com.winlator.xserver.Keyboard;
import com.winlator.xserver.XServer;

/**
 * 1×1 focus target for the Android IME when Winlator is embedded in Trierarch's MainActivity.
 * Forwards key events and committed text into the in-process X server keyboard.
 */
public class WinlatorImeSinkView extends View {
    private final XServer xServer;

    public WinlatorImeSinkView(Context context, XServer xServer) {
        super(context);
        this.xServer = xServer;
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        setMeasuredDimension(1, 1);
    }

    @Override
    public boolean onCheckIsTextEditor() {
        return true;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        outAttrs.inputType = EditorInfo.TYPE_CLASS_TEXT;
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN;
        final Keyboard keyboard = xServer.keyboard;
        return new BaseInputConnection(this, true) {
            @Override
            public boolean sendKeyEvent(KeyEvent event) {
                return keyboard.onKeyEvent(event);
            }

            @Override
            public boolean commitText(CharSequence text, int newCursorPosition) {
                if (text == null || text.length() == 0) return true;
                for (int i = 0; i < text.length(); i++) {
                    keyboard.injectCharacter(text.charAt(i));
                }
                return true;
            }

            @Override
            public boolean deleteSurroundingText(int beforeLength, int afterLength) {
                int before = Math.max(0, Math.min(beforeLength, 256));
                for (int i = 0; i < before; i++) {
                    long t = System.currentTimeMillis();
                    keyboard.onKeyEvent(new KeyEvent(t, t, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0));
                    keyboard.onKeyEvent(new KeyEvent(t, t, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL, 0));
                }
                return true;
            }
        };
    }
}
