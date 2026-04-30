package com.winlator.core;

import com.winlator.container.Container;
import com.winlator.win32.MSLogFont;
import com.winlator.win32.WinVersions;

import java.io.File;

/**
 * Minimal Winlator-shaped Wine utilities needed for container creation parity.
 *
 * This intentionally excludes any "download/install component" logic.
 */
public abstract class WineUtils {
    public static void setSystemFont(WineRegistryEditor userRegistry, String faceName) {
        byte[] fontNormalData = (new MSLogFont()).setFaceName(faceName).toByteArray();
        byte[] fontBoldData = (new MSLogFont()).setFaceName(faceName).setWeight(700).toByteArray();
        userRegistry.setHexValues("Control Panel\\Desktop\\WindowMetrics", "CaptionFont", fontBoldData);
        userRegistry.setHexValues("Control Panel\\Desktop\\WindowMetrics", "IconFont", fontNormalData);
        userRegistry.setHexValues("Control Panel\\Desktop\\WindowMetrics", "MenuFont", fontNormalData);
        userRegistry.setHexValues("Control Panel\\Desktop\\WindowMetrics", "MessageFont", fontNormalData);
        userRegistry.setHexValues("Control Panel\\Desktop\\WindowMetrics", "SmCaptionFont", fontNormalData);
        userRegistry.setHexValues("Control Panel\\Desktop\\WindowMetrics", "StatusFont", fontNormalData);
    }

    public static void setWinVersion(Container container, int winVersionIdx) {
        WinVersions.WinVersion winVersion = WinVersions.getWinVersions()[winVersionIdx];
        String currentBuild = String.valueOf(winVersion.buildNumber);
        String currentVersion = winVersion.currentVersion != null ? winVersion.currentVersion : winVersion.majorVersion + "." + winVersion.minorVersion;

        File systemRegFile = new File(container.getRootDir(), ".wine/system.reg");
        try (WineRegistryEditor registryEditor = new WineRegistryEditor(systemRegFile)) {
            String key64 = "Software\\Microsoft\\Windows NT\\CurrentVersion";
            String key32 = "Software\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion";

            registryEditor.setStringValue(key32, "CurrentVersion", currentVersion);
            registryEditor.setDwordValue(key32, "CurrentMajorVersionNumber", winVersion.majorVersion);
            registryEditor.setDwordValue(key32, "CurrentMinorVersionNumber", winVersion.minorVersion);
            registryEditor.setStringValue(key32, "CSDVersion", winVersion.csdVersion);
            registryEditor.setStringValue(key32, "CurrentBuild", currentBuild);
            registryEditor.setStringValue(key32, "CurrentBuildNumber", currentBuild);
            registryEditor.setStringValue(key32, "ProductName", "Microsoft " + winVersion.description);

            registryEditor.setStringValue(key64, "CurrentVersion", currentVersion);
            registryEditor.setDwordValue(key64, "CurrentMajorVersionNumber", winVersion.majorVersion);
            registryEditor.setDwordValue(key64, "CurrentMinorVersionNumber", winVersion.minorVersion);
            registryEditor.setStringValue(key64, "CSDVersion", winVersion.csdVersion);
            registryEditor.setStringValue(key64, "CurrentBuild", currentBuild);
            registryEditor.setStringValue(key64, "CurrentBuildNumber", currentBuild);
            registryEditor.setStringValue(key64, "ProductName", "Microsoft " + winVersion.description);
        }
    }
}

