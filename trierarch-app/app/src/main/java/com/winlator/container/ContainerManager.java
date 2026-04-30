package com.winlator.container;

import android.content.Context;
import android.os.Handler;

import com.winlator.core.Callback;
import com.winlator.core.FileUtils;
import com.winlator.core.TarCompressorUtils;
import com.winlator.core.WineInfo;
import com.winlator.core.WineRegistryEditor;
import com.winlator.core.WineUtils;
import com.winlator.win32.WinVersions;
import com.winlator.xenvironment.RootFS;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.concurrent.Executors;

/**
 * Upstream-shaped container manager, adapted to Trierarch's {@code files/wine} root.
 *
 * Network-download installable components are intentionally not supported here: container creation
 * uses only packaged assets (e.g. {@code container_pattern.tzst}).
 */
public class ContainerManager {
    private final ArrayList<Container> containers = new ArrayList<>();
    private int maxContainerId = 0;
    private final File homeDir;
    private final Context context;

    public ContainerManager(Context context) {
        this.context = context;
        File rootDir = RootFS.find(context).getRootDir();
        homeDir = new File(rootDir, "home");
        loadContainers();
    }

    public ArrayList<Container> getContainers() {
        return containers;
    }

    public Context getContext() {
        return context;
    }

    private void loadContainers() {
        containers.clear();
        maxContainerId = 0;

        File[] files = homeDir.listFiles();
        if (files == null) return;

        for (File file : files) {
            if (!file.isDirectory()) continue;
            if (!file.getName().startsWith(RootFS.USER + "-")) continue;

            int id;
            try {
                id = Integer.parseInt(file.getName().replace(RootFS.USER + "-", ""));
            }
            catch (NumberFormatException e) {
                continue;
            }

            Container container = new Container(id);
            container.setRootDir(new File(homeDir, RootFS.USER + "-" + container.id));
            File configFile = container.getConfigFile();

            try {
                String raw = FileUtils.readString(configFile);
                if (raw == null || raw.trim().isEmpty()) {
                    // Skip dirs with no config (e.g. interrupted create).
                    continue;
                }
                JSONObject data = new JSONObject(raw);
                container.loadData(data);
            }
            catch (Throwable e) {
                // Legacy: plain-text name only; rewrite as JSON.
                String name = FileUtils.readString(configFile);
                if (name != null) container.setName(name.trim().isEmpty() ? container.getName() : name.trim());
                container.saveData();
            }

            containers.add(container);
            maxContainerId = Math.max(maxContainerId, container.id);
        }
    }

    public void activateContainer(Container container) {
        container.setRootDir(new File(homeDir, RootFS.USER + "-" + container.id));
        File file = new File(homeDir, RootFS.USER);
        file.delete();
        FileUtils.symlink(RootFS.USER + "-" + container.id, file.getPath());
    }

    public void createContainerAsync(final JSONObject data, Callback<Container> callback) {
        final Handler handler = new Handler();
        Executors.newSingleThreadExecutor().execute(() -> {
            final Container container = createContainer(data);
            handler.post(() -> callback.call(container));
        });
    }

    public void duplicateContainerAsync(Container container, Runnable callback) {
        final Handler handler = new Handler();
        Executors.newSingleThreadExecutor().execute(() -> {
            duplicateContainer(container);
            handler.post(callback);
        });
    }

    public void removeContainerAsync(Container container, Runnable callback) {
        final Handler handler = new Handler();
        Executors.newSingleThreadExecutor().execute(() -> {
            removeContainer(container);
            handler.post(callback);
        });
    }

    /**
     * Applies the same JSON shape as container creation to an existing container, refreshes
     * {@code .wine/user.reg} from {@code data.registry}, and rewrites {@code .container}.
     */
    public void updateContainerAsync(final Container container, final JSONObject data, final Runnable callback) {
        final Handler handler = new Handler();
        Executors.newSingleThreadExecutor().execute(() -> {
            try {
                data.put("id", container.id);
                container.loadData(data);
                applyInitialRegistrySettings(container, data);
                container.saveData();
            }
            catch (Throwable ignored) {}
            handler.post(callback);
        });
    }

    private Container createContainer(JSONObject data) {
        File containerDir = null;
        try {
            int id = maxContainerId + 1;
            data.put("id", id);

            containerDir = new File(homeDir, RootFS.USER + "-" + id);
            if (!containerDir.mkdirs()) return null;

            Container container = new Container(id);
            container.setRootDir(containerDir);
            container.loadData(data);

            boolean isMainWineVersion = !data.has("wineVersion") || WineInfo.isMainWineVersion(data.getString("wineVersion"));
            if (!isMainWineVersion) container.setWineVersion(data.getString("wineVersion"));

            if (!extractContainerPatternFile(container.getWineVersion(), containerDir)) {
                FileUtils.delete(containerDir);
                return null;
            }

            applyInitialRegistrySettings(container, data);
            container.saveData();
            maxContainerId++;
            containers.add(container);
            return container;
        }
        catch (Throwable ignored) {
            if (containerDir != null) {
                FileUtils.delete(containerDir);
            }
        }
        return null;
    }

    private static void applyInitialRegistrySettings(Container container, JSONObject data) {
        // Default registry tweaks applied on every new container (font, DPI, D3D, etc.).
        File userRegFile = new File(container.getRootDir(), ".wine/user.reg");

        String systemFont = "Tahoma";
        int logPixels = 96;
        String mouseWarpOverride = "disable"; // disable|enable|force
        int winVersionIdx = -1;

        try {
            JSONObject registry = data.optJSONObject("registry");
            if (registry != null) {
                systemFont = registry.optString("systemFont", systemFont);
                logPixels = registry.optInt("logPixels", logPixels);
                mouseWarpOverride = registry.optString("mouseWarpOverride", mouseWarpOverride);
                winVersionIdx = registry.optInt("winVersionIdx", -1);
            }
        }
        catch (Throwable ignored) {}

        try (WineRegistryEditor registryEditor = new WineRegistryEditor(userRegFile)) {
            WineUtils.setSystemFont(registryEditor, systemFont);
            registryEditor.setDwordValue("Control Panel\\Desktop", "LogPixels", logPixels);
            registryEditor.setStringValue("Software\\Wine\\DirectInput", "MouseWarpOverride", mouseWarpOverride);
            registryEditor.setStringValue("Software\\Wine\\Direct3D", "shader_backend", "glsl");
            registryEditor.setStringValue("Software\\Wine\\Direct3D", "UseGLSL", "enabled");
        }

        if (winVersionIdx >= 0 && winVersionIdx < WinVersions.getWinVersions().length) {
            WineUtils.setWinVersion(container, winVersionIdx);
        }
    }

    private void duplicateContainer(Container srcContainer) {
        int id = maxContainerId + 1;
        File dstDir = new File(homeDir, RootFS.USER + "-" + id);
        if (!dstDir.mkdirs()) return;

        if (!FileUtils.copy(srcContainer.getRootDir(), dstDir, (file) -> FileUtils.chmod(file, 0771))) {
            FileUtils.delete(dstDir);
            return;
        }

        Container dstContainer = new Container(id);
        dstContainer.setRootDir(dstDir);
        dstContainer.setName(srcContainer.getName() + " (copy)");
        dstContainer.setScreenSize(srcContainer.getScreenSize());
        dstContainer.setEnvVars(srcContainer.getEnvVars());
        dstContainer.setCPUList(srcContainer.getCPUList());
        dstContainer.setCPUListWoW64(srcContainer.getCPUListWoW64());
        dstContainer.setGraphicsDriver(srcContainer.getGraphicsDriver());
        dstContainer.setGraphicsDriverConfig(srcContainer.getGraphicsDriverConfig());
        dstContainer.setDXWrapper(srcContainer.getDXWrapper());
        dstContainer.setDXWrapperConfig(srcContainer.getDXWrapperConfig());
        dstContainer.setAudioDriver(srcContainer.getAudioDriver());
        dstContainer.setAudioDriverConfig(srcContainer.getAudioDriverConfig());
        dstContainer.setWinComponents(srcContainer.getWinComponents());
        dstContainer.setDrives(srcContainer.getDrives());
        dstContainer.setHUDMode(srcContainer.getHUDMode());
        dstContainer.setStartupSelection(srcContainer.getStartupSelection());
        dstContainer.setBox64Preset(srcContainer.getBox64Preset());
        dstContainer.setDesktopTheme(srcContainer.getDesktopTheme());
        dstContainer.saveData();

        maxContainerId++;
        containers.add(dstContainer);
    }

    private void removeContainer(Container container) {
        if (FileUtils.delete(container.getRootDir())) containers.remove(container);
    }

    public int getNextContainerId() {
        return maxContainerId + 1;
    }

    public Container getContainerById(int id) {
        for (Container container : containers) if (container.id == id) return container;
        return null;
    }

    private void copyCommonDlls(String srcName, String dstName, JSONObject commonDlls, File containerDir) throws JSONException {
        File srcDir = new File(RootFS.find(context).getRootDir(), "/opt/wine/lib/wine/" + srcName);
        JSONArray dlnames = commonDlls.getJSONArray(dstName);

        for (int i = 0; i < dlnames.length(); i++) {
            String dlname = dlnames.getString(i);
            File dstFile = new File(containerDir, ".wine/drive_c/windows/" + dstName + "/" + dlname);
            FileUtils.copy(new File(srcDir, dlname), dstFile);
        }
    }

    private boolean extractContainerPatternFile(String wineVersion, File containerDir) {
        if (WineInfo.isMainWineVersion(wineVersion)) {
            boolean result = TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, context, "container_pattern.tzst", containerDir);

            if (result) {
                try {
                    JSONObject commonDlls = new JSONObject(FileUtils.readString(context, "common_dlls.json"));
                    copyCommonDlls("x86_64-windows", "system32", commonDlls, containerDir);
                    copyCommonDlls("i386-windows", "syswow64", commonDlls, containerDir);
                }
                catch (JSONException e) {
                    return false;
                }
            }

            return result;
        }
        else {
            File installedWineDir = RootFS.find(context).getInstalledWineDir();
            WineInfo wineInfo = WineInfo.fromIdentifier(context, wineVersion);
            File file = new File(installedWineDir, "container-pattern-" + wineInfo.fullVersion() + ".tzst");
            return TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, file, containerDir);
        }
    }
}

