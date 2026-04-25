package com.termux.x11;

import static android.system.Os.getuid;
import static android.system.Os.getenv;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import android.view.Surface;

import androidx.annotation.Keep;

import app.trierarch.BuildConfig;

import java.io.OutputStream;
import java.io.PrintStream;
import java.net.URL;

@Keep @SuppressLint({"StaticFieldLeak", "UnsafeDynamicallyLoadedCode"})
public class CmdEntryPoint extends ICmdEntryInterface.Stub {
    public static final String ACTION_START = "com.termux.x11.CmdEntryPoint.ACTION_START";
    static final Handler handler;
    public static Context ctx;
    private final Intent intent = createIntent();

    /**
     * Command-line entry point.
     *
     * @param args The command-line arguments
     */
    public static void main(String[] args) {
        android.util.Log.i("CmdEntryPoint", "commit " + BuildConfig.COMMIT);
        handler.post(() -> new CmdEntryPoint(args));
        Looper.loop();
    }

    CmdEntryPoint(String[] args) {
        if (!start(args)) {
            Log.e("CmdEntryPoint", "native start() failed (args=" + java.util.Arrays.toString(args) + ")");
            // Termux in `app_process` expects process death; in Trierarch the same process runs the
            // Activity — System.exit(1) kills the app and the OS restarts it → apparent hang / log spam.
            if (!"app.trierarch".equals(BuildConfig.APPLICATION_ID)) {
                System.exit(1);
            }
            return;
        }

        spawnListeningThread();
        sendBroadcastDelayed();
    }

    @SuppressLint({"WrongConstant", "PrivateApi"})
    private Intent createIntent() {
        String targetPackage = getenv("TERMUX_X11_OVERRIDE_PACKAGE");
        if (targetPackage == null)
            targetPackage = "app.trierarch";
        // We should not care about multiple instances, it should be called only by `Termux:X11` app
        // which is single instance...
        Bundle bundle = new Bundle();
        bundle.putBinder(null, this);

        Intent intent = new Intent(ACTION_START);
        intent.putExtra(null, bundle);
        intent.setPackage(targetPackage);

        if (getuid() == 0 || getuid() == 2000)
            intent.setFlags(0x00400000 /* FLAG_RECEIVER_FROM_SHELL */);

        return intent;
    }

    private void sendBroadcast() {
        sendBroadcast(intent);
    }

    static void sendBroadcast(Intent intent) {
        if (ctx == null) {
            Log.e("Broadcast", "Context is null; cannot send " + intent.getAction());
            return;
        }
        try {
            ctx.sendBroadcast(intent);
        } catch (Exception e) {
            Log.e("Broadcast", "sendBroadcast failed for " + intent.getAction(), e);
        }
    }

    // In some cases Android Activity part can not connect opened port.
    // In this case opened port works like a lock file.
    private void sendBroadcastDelayed() {
        if (!connected())
            sendBroadcast(intent);

        handler.postDelayed(this::sendBroadcastDelayed, 1000);
    }

    void spawnListeningThread() {
        new Thread(this::listenForConnections).start();
    }

    /** @noinspection DataFlowIssue*/
    @SuppressLint("DiscouragedPrivateApi")
    public static Context createContext() {
        Context context;
        PrintStream err = System.err;
        try {
            java.lang.reflect.Field f = Class.forName("sun.misc.Unsafe").getDeclaredField("theUnsafe");
            f.setAccessible(true);
            Object unsafe = f.get(null);
            // Hiding harmless framework errors, like this:
            // java.io.FileNotFoundException: /data/system/theme_config/theme_compatibility.xml: open failed: ENOENT (No such file or directory)
            System.setErr(new PrintStream(new OutputStream() { public void write(int arg0) {} }));
            Class<?> activityThreadClass = Class.forName("android.app.ActivityThread");
            Object activityThread;
            if (System.getenv("OLD_CONTEXT") != null) {
                activityThread = activityThreadClass.getMethod("systemMain").invoke(null);
            } else {
                activityThread = Class.forName("sun.misc.Unsafe")
                        .getMethod("allocateInstance", Class.class)
                        .invoke(unsafe, activityThreadClass);
            }
            context = (Context) activityThreadClass.getMethod("getSystemContext").invoke(activityThread);
        } catch (Exception e) {
            Log.e("Context", "Failed to instantiate context:", e);
            context = null;
        } finally {
            System.setErr(err);
        }
        return context;
    }

    public static native boolean start(String[] args);
    public native ParcelFileDescriptor getXConnection();
    public native ParcelFileDescriptor getLogcatOutput();
    private static native boolean connected();
    private native void listenForConnections();

    static {
        try {
            if (Looper.getMainLooper() == null)
                Looper.prepareMainLooper();
        } catch (Exception e) {
            Log.e("CmdEntryPoint", "Something went wrong when preparing MainLooper", e);
        }
        // In-process: class may be first loaded on a worker after Looper.prepare(); in Termux: same.
        // `new Handler()` with no arg requires a Looper on the current thread — fall back to main.
        {
            Looper l = Looper.myLooper();
            if (l != null) {
                handler = new Handler(l);
            } else {
                handler = new Handler(Looper.getMainLooper());
            }
        }
        ctx = createContext();

        // Standard Android (jniLibs in APK): always use loadLibrary — same as [app.trierarch.X11Runtime].
        // The Termux getResource+System.load("...base.apk!/lib/...") path often fails on modern Android
        // (dlopen from zip path), so it must be fallback only, not first.
        try {
            System.loadLibrary("Xlorie");
        } catch (UnsatisfiedLinkError first) {
            String subPath = "lib/" + Build.SUPPORTED_ABIS[0] + "/libXlorie.so";
            ClassLoader loader = CmdEntryPoint.class.getClassLoader();
            URL res = loader != null ? loader.getResource(subPath) : null;
            String libPath = res != null ? res.getFile().replace("file:", "") : null;
            if (libPath == null) {
                Log.e("CmdEntryPoint", "Failed to load libXlorie (no ClassLoader path)", first);
                System.err.println("Failed to acquire native library (libXlorie).");
                System.exit(134);
            } else {
                try {
                    System.load(libPath);
                } catch (UnsatisfiedLinkError | Exception e) {
                    Log.e("CmdEntryPoint", "Failed to dlopen " + libPath, e);
                    System.err.println("Failed to load native library. Did you install the right apk? Try the universal one.");
                    System.exit(134);
                }
            }
        }
    }
}
