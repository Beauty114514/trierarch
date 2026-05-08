package com.winlator;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.Nullable;

import com.winlator.contentdialog.DebugDialog;
import com.winlator.core.EnvVars;
import com.winlator.winhandler.WinHandler;
import com.winlator.widget.InputControlsView;
import com.winlator.widget.XServerView;
import com.winlator.xserver.ScreenInfo;
import com.winlator.xserver.XServer;

/**
 * Thin host surface for Winlator core so it can run either in the original
 * {@link XServerDisplayActivity} or embedded inside Trierarch's Compose activity.
 *
 * Keep this intentionally small: only what non-UI core needs to call back into.
 */
public interface WinlatorHost {
    Context getContext();
    Activity getActivity();
    SharedPreferences getPreferences();

    @Nullable InputControlsView getInputControlsView();
    @Nullable XServerView getXServerView();
    @Nullable XServer getXServer();
    @Nullable WinHandler getWinHandler();
    @Nullable DebugDialog getDebugDialog();

    @Nullable String getScreenEffectProfile();
    void setScreenEffectProfile(@Nullable String value);

    EnvVars getOverrideEnvVars();
    void setScreenInfo(ScreenInfo screenInfo);
    void setDXWrapper(String dxWrapper);
    void setWinComponents(String wincomponents);

    void runOnUiThread(Runnable r);
}

