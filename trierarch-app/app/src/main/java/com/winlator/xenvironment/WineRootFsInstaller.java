package com.winlator.xenvironment;

import android.content.Context;
import android.content.res.AssetManager;

import com.winlator.core.FileUtils;
import com.winlator.core.TarCompressorUtils;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Bundled {@code rootfs.tzst} → {@link RootFS#find(Context)} ({@code files/wine}), aligned with
 * Winlator {@code RootFSInstaller.install} but synchronous (caller supplies progress on any thread).
 */
public final class WineRootFsInstaller {

    private WineRootFsInstaller() {}

    /** Bump when replacing bundled {@link #FILENAME}; keep in sync with upstream Winlator when merging. */
    public static final byte LATEST_VERSION = 19;
    public static final String FILENAME = "rootfs.tzst";

    public interface ProgressSink {
        void onProgress(int pct, String msg);
    }

    public static boolean isWineReady(Context context) {
        RootFS rootFS = RootFS.find(context);
        return rootFS.isValid() && rootFS.getVersion() >= LATEST_VERSION;
    }

    public static boolean ensureInstalled(Context context, ProgressSink sink) {
        try {
            RootFS rootFS = RootFS.find(context);
            if (rootFS.isValid() && rootFS.getVersion() >= LATEST_VERSION) {
                return true;
            }
            return install(context, sink);
        } catch (Throwable t) {
            if (sink != null) {
                sink.onProgress(0, "Wine setup crashed: " + t.getClass().getSimpleName());
            }
            return false;
        }
    }

    private static boolean assetExists(Context context) {
        AssetManager assets = context.getAssets();
        try {
            assets.open(FILENAME).close();
            return true;
        } catch (IOException e) {
            return false;
        }
    }

    private static boolean install(Context context, ProgressSink sink) {
        try {
            if (!assetExists(context)) {
                if (sink != null) {
                    sink.onProgress(0, "Missing asset rootfs.tzst (add under app/src/main/assets/)");
                }
                return false;
            }

            RootFS rootFS = RootFS.find(context);
            final File rootDir = rootFS.getRootDir();
            if (sink != null) {
                sink.onProgress(0, "Installing Wine environment...");
            }
            clearRootDir(rootDir);

            final long contentLength =
                    TarCompressorUtils.getContentLength(TarCompressorUtils.Type.ZSTD, context, FILENAME, rootDir);
            AtomicLong totalSizeRef = new AtomicLong();

            boolean success =
                    TarCompressorUtils.extract(
                            TarCompressorUtils.Type.ZSTD,
                            context,
                            FILENAME,
                            rootDir,
                            (file, size) -> {
                                if (file != null) {
                                    File parent = file.getParentFile();
                                    if (parent != null && !parent.isDirectory()) {
                                        // Some tarballs don't always include explicit directory entries first.
                                        parent.mkdirs();
                                    }
                                }
                                if (sink != null && size > 0) {
                                    long totalSize = totalSizeRef.addAndGet(size);
                                    final int progress =
                                            contentLength > 0
                                                    ? (int) (((float) totalSize / contentLength) * 100f)
                                                    : 0;
                                    sink.onProgress(
                                            Math.min(progress, 99),
                                            "Extracting Wine environment...");
                                }
                                return file;
                            });

            if (success) {
                rootFS.createRFSVersionFile(LATEST_VERSION);
                if (sink != null) {
                    sink.onProgress(100, "Wine environment ready");
                }
                return true;
            }

            if (sink != null) {
                sink.onProgress(0, "Failed to extract Wine environment");
            }
            return false;
        } catch (Throwable t) {
            if (sink != null) {
                String msg = t.getMessage();
                sink.onProgress(
                        0,
                        "Wine extract crashed: "
                                + t.getClass().getSimpleName()
                                + (msg != null && !msg.isEmpty() ? (": " + msg) : ""));
            }
            return false;
        }
    }

    private static void clearOptDir(File optDir) {
        File[] files = optDir.listFiles();
        if (files != null) {
            for (File file : files) {
                if ("installed-wine".equals(file.getName())) continue;
                try {
                    FileUtils.delete(file);
                } catch (Throwable ignored) {
                }
            }
        }
    }

    private static void clearRootDir(File rootDir) {
        if (rootDir.isDirectory()) {
            File[] files = rootDir.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (file.isDirectory()) {
                        String name = file.getName();
                        if ("home".equals(name) || "opt".equals(name)) {
                            if ("opt".equals(name)) clearOptDir(file);
                            continue;
                        }
                    }
                    try {
                        FileUtils.delete(file);
                    } catch (Throwable ignored) {
                    }
                }
            }
        } else {
            rootDir.mkdirs();
        }
    }
}
