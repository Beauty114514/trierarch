package com.winlator.xenvironment;

import android.content.Context;

import androidx.annotation.NonNull;

import com.winlator.core.FileUtils;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Locale;

public class RootFS {
    public static final String USER = "xuser";
    public static final String HOME_PATH = "/home/"+USER;
    public static final String USER_CACHE_PATH = "/home/"+USER+"/.cache";
    public static final String USER_CONFIG_PATH = "/home/"+USER+"/.config";
    public static final String WINEPREFIX = "/home/"+USER+"/.wine";
    private final File rootDir;
    private String winePath = "/opt/wine";

    private RootFS(File rootDir) {
        this.rootDir = rootDir;
    }

    /**
     * Wine root lives under {@code files/wine}. Migrates legacy Winlator dirs {@code imagefs} /
     * {@code rootfs} into {@code wine} when present (upstream parity + single canonical path).
     */
    public static RootFS find(Context context) {
        File filesDir = context.getFilesDir();
        File wineDir = new File(filesDir, "wine");
        File legacyImagefs = new File(filesDir, "imagefs");
        File legacyRootfs = new File(filesDir, "rootfs");
        if (legacyImagefs.isDirectory()) {
            if (!wineDir.exists()) {
                legacyImagefs.renameTo(wineDir);
            }
        }
        if (legacyRootfs.isDirectory() && !wineDir.exists()) {
            legacyRootfs.renameTo(wineDir);
        }
        return new RootFS(wineDir);
    }

    public File getRootDir() {
        return rootDir;
    }

    public boolean isValid() {
        return rootDir.isDirectory() && getRFSVersionFile().exists();
    }

    public int getVersion() {
        File rfsVersionFile = getRFSVersionFile();
        return rfsVersionFile.exists() ? Integer.parseInt(FileUtils.readLines(rfsVersionFile).get(0)) : 0;
    }

    public String getFormattedVersion() {
        return String.format(Locale.ENGLISH, "%.1f", (float)getVersion());
    }

    public void createRFSVersionFile(int version) {
        getImageInfoDir().mkdirs();
        File file = getRFSVersionFile();
        try {
            file.createNewFile();
            FileUtils.writeString(file, String.valueOf(version));
        }
        catch (IOException e) {
            e.printStackTrace();
        }
    }

    public String getWinePath() {
        return winePath;
    }

    public void setWinePath(String winePath) {
        this.winePath = FileUtils.toRelativePath(rootDir.getPath(), winePath);
    }

    private File getImageInfoDir() {
        return new File(rootDir, ".winlator");
    }

    public File getRFSVersionFile() {
        return new File(getImageInfoDir(), ".rfs_version");
    }

    public File getInstalledWineDir() {
        return new File(rootDir, "/opt/installed-wine");
    }

    public File getTmpDir() {
        return new File(rootDir, "/tmp");
    }

    public File getLibDir() {
        return new File(rootDir, "/usr/lib");
    }

    /**
     * Guest {@code LD_LIBRARY_PATH} for box64/Wine.
     * <p><b>winlator-app</b> sets only {@link #getLibDir()} ({@code usr/lib}). Some tarballs place Mesa
     * ({@code libEGL.so.1}, etc.) only under multiarch dirs; we prepend those when present so behavior matches
     * upstream releases where those libs already live in {@code usr/lib} (often via symlinks in the shipped tzst).
     * Order: multiarch first, then {@code usr/lib} as in Debian loader defaults.</p>
     */
    @NonNull
    public String getGuestLdLibraryPath() {
        ArrayList<String> parts = new ArrayList<>(6);
        String[] relativeDirs = {
            "usr/lib/aarch64-linux-gnu",
            "lib/aarch64-linux-gnu",
            "usr/lib/arm-linux-gnueabihf",
            "lib/arm-linux-gnueabihf",
        };
        for (String rel : relativeDirs) {
            File d = new File(rootDir, rel);
            if (d.isDirectory()) parts.add(d.getAbsolutePath());
        }
        parts.add(getLibDir().getAbsolutePath());
        return String.join(":", parts);
    }

    @NonNull
    @Override
    public String toString() {
        return rootDir.getPath();
    }

    public static String getDosUserCachePath() {
        return "Z:"+USER_CACHE_PATH.replace("/", "\\");
    }

    public static String getDosUserConfigPath() {
        return "Z:"+USER_CONFIG_PATH.replace("/", "\\");
    }
}
