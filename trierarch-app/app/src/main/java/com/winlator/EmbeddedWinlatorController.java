package com.winlator;

import app.trierarch.R;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.view.ContextThemeWrapper;

import androidx.annotation.Nullable;
import androidx.drawerlayout.widget.DrawerLayout;
import androidx.preference.PreferenceManager;

import com.winlator.contentdialog.DebugDialog;
import com.winlator.renderer.GLRenderer;
import com.winlator.container.AudioDrivers;
import com.winlator.container.Container;
import com.winlator.container.ContainerManager;
import com.winlator.container.DXWrappers;
import com.winlator.container.GraphicsDrivers;
import com.winlator.core.AppUtils;
import com.winlator.core.EnvVars;
import com.winlator.core.FileUtils;
import com.winlator.core.KeyValueSet;
import com.winlator.core.TarCompressorUtils;
import com.winlator.core.WineInfo;
import com.winlator.core.WineUtils;
import com.winlator.core.GeneralComponents;
import com.winlator.core.WineRegistryEditor;
import com.winlator.core.DefaultVersion;
import com.winlator.core.ProcessHelper;
import com.winlator.core.WineThemeManager;
import com.winlator.core.WineStartMenuCreator;
import com.winlator.xconnector.UnixSocketConfig;
import com.winlator.xenvironment.components.GuestProgramLauncherComponent;
import com.winlator.xenvironment.components.NetworkInfoUpdateComponent;
import com.winlator.xenvironment.components.PulseAudioComponent;
import com.winlator.xenvironment.components.SysVSharedMemoryComponent;
import com.winlator.xenvironment.components.VirGLRendererComponent;
import com.winlator.xenvironment.components.VortekRendererComponent;
import com.winlator.xenvironment.components.XServerComponent;
import com.winlator.alsaserver.ALSAClient;
import com.winlator.xenvironment.components.ALSAServerComponent;
import com.winlator.contentdialog.AudioDriverConfigDialog;
import com.winlator.contentdialog.TurnipConfigDialog;
import com.winlator.contentdialog.VirGLConfigDialog;
import com.winlator.widget.InputControlsView;
import com.winlator.widget.WinlatorImeSinkView;
import com.winlator.widget.XServerView;
import com.winlator.winhandler.WinHandler;
import com.winlator.xenvironment.RootFS;
import com.winlator.xenvironment.XEnvironment;
import com.winlator.xserver.ScreenInfo;
import com.winlator.xserver.XServer;
import com.winlator.widget.TouchpadView;
import com.winlator.widget.MagnifierView;
import com.winlator.inputcontrols.InputControlsManager;
import com.winlator.inputcontrols.ControlsProfile;
import com.winlator.contentdialog.ContentDialog;
import com.winlator.contentdialog.ScreenEffectDialog;
import com.winlator.winhandler.TaskManagerDialog;
import com.winlator.contentdialog.ActiveWindowsDialog;
import com.winlator.math.Mathf;
import android.widget.FrameLayout;
import android.widget.Spinner;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.content.Intent;
import androidx.core.view.GravityCompat;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.Executors;

/**
 * Embedded variant of {@link XServerDisplayActivity}.
 *
 * This keeps Winlator's internal components (XServer/XServerView/WinHandler/XEnvironment)
 * but hosts them inside an existing Activity view tree so Compose overlays remain visible.
 */
public final class EmbeddedWinlatorController implements WinlatorHost {
    private static final String TAG = "Trierarch-WinlatorEmbedded";
    private final Activity activity;
    private final Context themedContext;
    private final View rootView;
    private final DrawerLayout drawerLayout;
    private final SharedPreferences preferences;

    private XServerView xServerView;
    private InputControlsView inputControlsView;
    private TouchpadView touchpadView;
    private WinlatorImeSinkView imeSinkView;
    private XEnvironment environment;
    private Container container;
    private XServer xServer;
    private RootFS rootFS;
    private WineInfo wineInfo;

    private String[] graphicsDriver = {GraphicsDrivers.DEFAULT_VULKAN_DRIVER, GraphicsDrivers.DEFAULT_OPENGL_DRIVER};
    private String audioDriver = Container.DEFAULT_AUDIO_DRIVER;
    private String dxwrapper = Container.DEFAULT_DXWRAPPER;
    private ScreenInfo screenInfo = new ScreenInfo(Container.DEFAULT_SCREEN_SIZE);
    private KeyValueSet[] dxwrapperConfig;
    private KeyValueSet[] graphicsDriverConfig;
    private KeyValueSet audioDriverConfig;
    private String wincomponents;
    private final EnvVars envVars = new EnvVars();
    private EnvVars overrideEnvVars;
    private float globalCursorSpeed = 1.0f;
    private InputControlsManager inputControlsManager;
    private MagnifierView magnifierView;
    private DebugDialog debugDialog;
    private String screenEffectProfile;

    private final WinHandler winHandler = new WinHandler(this);

    private boolean capturePointerOnExternalMouse = true;

    public EmbeddedWinlatorController(Activity activity, ViewGroup parent) {
        this.activity = activity;
        this.themedContext = new ContextThemeWrapper(activity, R.style.AppThemeFullscreenDark);
        this.preferences = PreferenceManager.getDefaultSharedPreferences(activity);
        // XServerDisplayActivity uses a Winlator-specific theme (MaterialComponents). When embedding inside
        // Trierarch's Compose activity, we must inflate the Winlator layout with that theme, otherwise
        // Material widgets (NavigationView) crash resolving theme attributes.
        this.rootView = LayoutInflater.from(themedContext).inflate(R.layout.xserver_display_activity, parent, false);
        parent.addView(rootView);
        this.drawerLayout = rootView.findViewById(R.id.DrawerLayout);
    }

    public View getRootView() {
        return rootView;
    }

    /**
     * Start Winlator session for a container id.
     *
     * NOTE: Implementation intentionally mirrors {@link XServerDisplayActivity#onCreate(Bundle)}
     * but without launching a new Activity.
     */
    public void start(int containerId) {
        ContainerManager manager = new ContainerManager(activity);
        container = manager.getContainerById(containerId);
        if (container == null) {
            throw new IllegalStateException("Container not found: " + containerId);
        }
        // Match XServerDisplayActivity: activate container so /home/xuser symlink points to the
        // container's home directory (home/xuser-<id>). Wine relies on this for WINEPREFIX.
        manager.activateContainer(container);

        rootFS = RootFS.find(activity);
        screenInfo = new ScreenInfo(container.getScreenSize());

        inputControlsManager = new InputControlsManager(activity);
        globalCursorSpeed = preferences.getFloat("cursor_speed", 1.0f);
        debugDialog = new DebugDialog(themedContext);

        wineInfo = WineInfo.fromIdentifier(activity, container.getWineVersion());
        // Defensive fallback: some custom wine identifiers can exist in container config while the
        // installed wine metadata/path is missing. In that case, run with main wine (/opt/wine)
        // instead of crashing or starting with an invalid wine path.
        if (wineInfo == null || wineInfo.path == null || wineInfo.path.isEmpty()) {
            Log.w(TAG, "wineInfo path is missing for wineVersion=" + container.getWineVersion() + ", fallback to main wine");
            wineInfo = WineInfo.MAIN_WINE_INFO;
        }
        // Match upstream: RootFS defaults to /opt/wine; only override when using an installed custom wine.
        if (wineInfo != WineInfo.MAIN_WINE_INFO) {
            rootFS.setWinePath(wineInfo.path);
        }

        graphicsDriver = GraphicsDrivers.parseIdentifiers(container.getGraphicsDriver());
        graphicsDriverConfig = GraphicsDrivers.parseConfigs(container.getGraphicsDriver(), container.getGraphicsDriverConfig());
        audioDriver = container.getAudioDriver();
        audioDriverConfig = new KeyValueSet(container.getAudioDriverConfig());
        dxwrapper = DXWrappers.parseIdentifier(container.getDXWrapper());
        dxwrapperConfig = DXWrappers.parseConfigs(container.getDXWrapper(), container.getDXWrapperConfig());
        wincomponents = container.getWinComponents();

        xServer = new XServer(this, screenInfo);
        xServer.setWinHandler(winHandler);

        setupUI();

        Executors.newSingleThreadExecutor().execute(() -> {
            try {
                setupXEnvironment();
            } catch (Throwable t) {
                Log.e(TAG, "startup failed", t);
            }
        });
    }

    public void onHostResume() {
        if (environment != null && xServerView != null) {
            xServerView.onResume();
            environment.onResume();
        }
    }

    public void onHostPause() {
        if (environment != null && xServerView != null) {
            environment.onPause();
            xServerView.onPause();
        }
    }

    public void stop() {
        if (winHandler != null) winHandler.stop();
        if (environment != null) environment.stopEnvironmentComponents();
    }

    private void setupUI() {
        ViewGroup container = rootView.findViewById(R.id.FLXServerDisplay);
        xServerView = new XServerView(activity, xServer);
        final GLRenderer renderer = xServerView.getRenderer();
        // In embedded mode, we keep the cursor visible by default so the user always has a visual
        // pointer. (XServerDisplayActivity may delay showing it until the first mapped window.)
        renderer.setCursorVisible(true);
        renderer.setCursorColor(preferences.getInt("cursor_color", 0xffffff));
        renderer.setCursorScale(preferences.getFloat("cursor_scale", 1.0f));

        xServer.setRenderer(renderer);
        container.addView(xServerView);

        globalCursorSpeed = preferences.getFloat("cursor_speed", 1.0f);
        capturePointerOnExternalMouse = preferences.getBoolean("capture_pointer_on_external_mouse", true);
        touchpadView = new TouchpadView(activity, xServer, capturePointerOnExternalMouse);
        touchpadView.setSensitivity(globalCursorSpeed);
        touchpadView.setMoveCursorToTouchpoint(preferences.getBoolean("move_cursor_to_touchpoint", false));
        touchpadView.setFourFingersTapCallback(() -> {
            // Native Winlator drawer is disabled in Trierarch embedded mode.
            // Functionality is moved to FloatingMenuOrb (Compose).
        });
        drawerLayout.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED);
        container.addView(touchpadView);

        inputControlsView = new InputControlsView(activity);
        inputControlsView.setOverlayOpacity(preferences.getFloat("overlay_opacity", InputControlsView.DEFAULT_OVERLAY_OPACITY));
        inputControlsView.setTouchpadView(touchpadView);
        inputControlsView.setXServer(xServer);
        inputControlsView.setVisibility(View.GONE);
        container.addView(inputControlsView);

        imeSinkView = new WinlatorImeSinkView(activity, xServer);
        container.addView(imeSinkView, new FrameLayout.LayoutParams(1, 1));

        // Embedded in Trierarch MainActivity (adjustResize): do not apply Winlator's extra
        // GL vertical offset — the host window already resizes and double-shifting breaks layout.
    }

    private void setupXEnvironment() {
        setupWineSystemFiles();
        extractGraphicsDriverFiles();
        String rootPath = rootFS.getRootDir().getPath();
        envVars.put("MESA_DEBUG", "silent");
        envVars.put("MESA_NO_ERROR", "1");
        // Do not depend on /home/xuser symlink: some ROMs/filesystems disallow symlinks under app data.
        // Use the concrete container directory instead.
        envVars.put("WINEPREFIX", new File(container.getRootDir(), ".wine").getPath());
        envVars.put("WINE_DO_NOT_CREATE_DXGI_DEVICE_MANAGER", "1");

        boolean enableWineDebug = preferences.getBoolean("enable_wine_debug", false);
        String wineDebugChannels = preferences.getString("wine_debug_channels", SettingsFragment.DEFAULT_WINE_DEBUG_CHANNELS);
        envVars.put("WINEDEBUG", enableWineDebug && wineDebugChannels != null && !wineDebugChannels.isEmpty()
            ? "+" + wineDebugChannels.replace(",", ",+")
            : "-all");

        FileUtils.clear(rootFS.getTmpDir());

        GuestProgramLauncherComponent guestProgramLauncherComponent = new GuestProgramLauncherComponent();

        if (container != null) {
            String desktopName = "shell";
            String guestExecutable = "wine explorer /desktop=" + desktopName + "," + xServer.screenInfo + " " + getWineStartCommand();
            guestProgramLauncherComponent.setGuestExecutable(guestExecutable);

            envVars.putAll(container.getEnvVars());
            applyGraphicsDriverEnvVars();
            if (!envVars.has("WINEESYNC")) envVars.put("WINEESYNC", "1");

            guestProgramLauncherComponent.setBox64Preset(container.getBox64Preset());
        }

        environment = new XEnvironment(activity, rootFS);
        environment.addComponent(new SysVSharedMemoryComponent(xServer, UnixSocketConfig.create(rootPath, UnixSocketConfig.SYSVSHM_SERVER_PATH)));
        environment.addComponent(new XServerComponent(xServer, UnixSocketConfig.create(rootPath, UnixSocketConfig.XSERVER_PATH)));
        environment.addComponent(new NetworkInfoUpdateComponent());

        if (audioDriver.equals(AudioDrivers.ALSA)) {
            envVars.put("ANDROID_ALSA_SERVER", rootPath + UnixSocketConfig.ALSA_SERVER_PATH);
            envVars.put("ANDROID_ASERVER_USE_SHM", ALSAClient.USE_SHARED_MEMORY ? "true" : "false");

            ALSAClient.Options options = ALSAClient.Options.fromKeyValueSet(audioDriverConfig);
            environment.addComponent(new ALSAServerComponent(UnixSocketConfig.create(rootPath, UnixSocketConfig.ALSA_SERVER_PATH), options));
        } else if (audioDriver.equals(AudioDrivers.PULSEAUDIO)) {
            PulseAudioComponent pulseAudioComponent = new PulseAudioComponent(UnixSocketConfig.create(rootPath, UnixSocketConfig.PULSE_SERVER_PATH));
            envVars.put("PULSE_SERVER", rootPath + UnixSocketConfig.PULSE_SERVER_PATH);

            if (!audioDriverConfig.isEmpty()) {
                envVars.put("PULSE_LATENCY_MSEC", audioDriverConfig.getInt("latencyMillis", AudioDriverConfigDialog.DEFAULT_LATENCY_MILLIS));
                pulseAudioComponent.setVolume(audioDriverConfig.getFloat("volume", AudioDriverConfigDialog.DEFAULT_VOLUME));
                pulseAudioComponent.setPerformanceMode(audioDriverConfig.getInt("performanceMode", AudioDriverConfigDialog.DEFAULT_PERFORMANCE_MODE));
            } else {
                envVars.put("PULSE_LATENCY_MSEC", AudioDriverConfigDialog.DEFAULT_LATENCY_MILLIS);
            }
            environment.addComponent(pulseAudioComponent);
        }

        if (graphicsDriver[0].equals(GraphicsDrivers.VORTEK)) {
            VortekRendererComponent.Options options = VortekRendererComponent.Options.fromKeyValueSet(activity, graphicsDriverConfig[0]);
            VortekRendererComponent vortekRendererComponent =
                new VortekRendererComponent(xServer, UnixSocketConfig.create(rootPath, UnixSocketConfig.VORTEK_SERVER_PATH), options);
            environment.addComponent(vortekRendererComponent);
        }
        if (graphicsDriver[1].equals(GraphicsDrivers.VIRGL)) {
            environment.addComponent(new VirGLRendererComponent(xServer, UnixSocketConfig.create(rootPath, UnixSocketConfig.VIRGL_SERVER_PATH)));
        }

        guestProgramLauncherComponent.setEnvVars(envVars);
        guestProgramLauncherComponent.setTerminationCallback((status) -> stop());
        environment.addComponent(guestProgramLauncherComponent);

        // Fresh containers should already have wineprefix via container pattern extraction.
        // If missing, re-extract the pattern (upstream behavior) instead of running WineInstaller here.
        ensureWineprefixExistsOrThrow();

        if (overrideEnvVars != null) {
            envVars.putAll(overrideEnvVars);
            overrideEnvVars = null;
        }

        environment.startEnvironmentComponents();
        winHandler.start();
        envVars.clear();
    }

    private void ensureWineprefixExistsOrThrow() {
        if (container == null || container.getRootDir() == null) return;
        File wineprefixDir = new File(container.getRootDir(), ".wine");
        if (wineprefixDir.isDirectory()) return;

        if (!extractContainerPatternFile(container.getWineVersion(), container.getRootDir())) {
            throw new IllegalStateException("Failed to extract container pattern for wineVersion=" + container.getWineVersion());
        }
    }

    private boolean extractContainerPatternFile(String wineVersion, File containerDir) {
        if (WineInfo.isMainWineVersion(wineVersion)) {
            boolean result = TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, activity, "container_pattern.tzst", containerDir);
            if (result) {
                try {
                    JSONObject commonDlls = new JSONObject(FileUtils.readString(activity, "common_dlls.json"));
                    copyCommonDlls("x86_64-windows", "system32", commonDlls, containerDir);
                    copyCommonDlls("i386-windows", "syswow64", commonDlls, containerDir);
                } catch (JSONException e) {
                    return false;
                }
            }
            return result;
        }

        File installedWineDir = RootFS.find(activity).getInstalledWineDir();
        WineInfo wi = WineInfo.fromIdentifier(activity, wineVersion);
        if (wi == null || wi.path == null || wi.path.isEmpty()) {
            Log.w(TAG, "extractContainerPatternFile: missing WineInfo path for wineVersion=" + wineVersion + ", fallback to main pattern");
            boolean result = TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, activity, "container_pattern.tzst", containerDir);
            if (result) {
                try {
                    JSONObject commonDlls = new JSONObject(FileUtils.readString(activity, "common_dlls.json"));
                    copyCommonDlls("x86_64-windows", "system32", commonDlls, containerDir);
                    copyCommonDlls("i386-windows", "syswow64", commonDlls, containerDir);
                } catch (JSONException e) {
                    return false;
                }
            }
            return result;
        }
        File file = new File(installedWineDir, "container-pattern-" + wi.fullVersion() + ".tzst");
        return TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, file, containerDir);
    }

    private void copyCommonDlls(String srcName, String dstName, JSONObject commonDlls, File containerDir) throws JSONException {
        File srcDir = new File(RootFS.find(activity).getRootDir(), "/opt/wine/lib/wine/" + srcName);
        JSONArray dlnames = commonDlls.getJSONArray(dstName);

        for (int i = 0; i < dlnames.length(); i++) {
            String dlname = dlnames.getString(i);
            File dstFile = new File(containerDir, ".wine/drive_c/windows/" + dstName + "/" + dlname);
            FileUtils.copy(new File(srcDir, dlname), dstFile);
        }
    }

    private void setupWineSystemFiles() {
        String appVersion = String.valueOf(AppUtils.getVersionCode(activity));
        String rfsVersion = String.valueOf(rootFS.getVersion());
        boolean containerDataChanged = false;

        boolean wineprefixWasUpdated = WineUtils.isWineprefixWasUpdated(container);
        if (!container.getExtra("appVersion").equals(appVersion) ||
            !container.getExtra("rfsVersion").equals(rfsVersion) ||
            wineprefixWasUpdated) {
            applyGeneralPatches(container);
            container.putExtra("appVersion", appVersion);
            container.putExtra("rfsVersion", rfsVersion);
            containerDataChanged = true;
        }

        if (verifyUserRegistry()) containerDataChanged = true;
        if (extractDXWrapperFiles()) containerDataChanged = true;

        if (wincomponents != null && !wincomponents.equals(container.getExtra("wincomponents"))) {
            extractWinComponentFiles();
            container.putExtra("wincomponents", wincomponents);
            containerDataChanged = true;
        }

        String desktopTheme = container.getDesktopTheme();
        if (!(desktopTheme + "," + xServer.screenInfo).equals(container.getExtra("desktopTheme"))) {
            WineThemeManager.apply(activity, new WineThemeManager.ThemeInfo(desktopTheme), xServer.screenInfo);
            container.putExtra("desktopTheme", desktopTheme + "," + xServer.screenInfo);
            containerDataChanged = true;
        }

        WineStartMenuCreator.create(activity, container);
        WineUtils.createDosdevicesSymlinks(container, true);

        String startupSelection = String.valueOf(container.getStartupSelection());
        if (!startupSelection.equals(container.getExtra("startupSelection")) || wineprefixWasUpdated) {
            WineUtils.changeServicesStatus(container, container.getStartupSelection() != Container.STARTUP_SELECTION_NORMAL);
            container.putExtra("startupSelection", startupSelection);
            containerDataChanged = true;
        }

        boolean openAndroidBrowserFromWine = preferences.getBoolean("open_android_browser_from_wine", true);
        String openAndroidBrowserFromWineStr = openAndroidBrowserFromWine ? "t" : "f";
        if (!openAndroidBrowserFromWineStr.equals(container.getExtra("openAndroidBrowserFromWine")) || wineprefixWasUpdated) {
            WineUtils.changeBrowsersRegistryKey(container, openAndroidBrowserFromWine);
            container.putExtra("openAndroidBrowserFromWine", openAndroidBrowserFromWineStr);
            containerDataChanged = true;
        }

        if (containerDataChanged) container.saveData();
    }

    private void extractGraphicsDriverFiles() {
        String cacheId = "";
        if (graphicsDriver[0].equals(GraphicsDrivers.TURNIP)) {
            cacheId += graphicsDriver[0] + "-" + graphicsDriverConfig[0].get("version", DefaultVersion.TURNIP);
        } else cacheId += graphicsDriver[0] + "-" + DefaultVersion.valueOf(graphicsDriver[0]);
        cacheId += "-" + graphicsDriver[1] + "-" + DefaultVersion.valueOf(graphicsDriver[1]);

        boolean changed = !cacheId.equals(container.getExtra("graphicsDriver"));
        File rootDir = rootFS.getRootDir();
        File libDir = rootFS.getLibDir();

        if (changed) {
            FileUtils.delete(new File(libDir, "libvulkan_freedreno.so"));
            FileUtils.delete(new File(libDir, "libvulkan_vortek.so"));
            FileUtils.delete(new File(libDir, "libGL.so.1.7.0"));

            File vulkanICDDir = new File(rootDir, "/usr/share/vulkan/icd.d");
            FileUtils.delete(vulkanICDDir);
            vulkanICDDir.mkdirs();

            container.putExtra("graphicsDriver", cacheId);
            container.saveData();
        }

        if (graphicsDriver[0].equals(GraphicsDrivers.TURNIP)) {
            if (changed) {
                String version = graphicsDriverConfig[0].get("version", DefaultVersion.TURNIP);
                GeneralComponents.extractFile(GeneralComponents.Type.TURNIP, activity, version, DefaultVersion.TURNIP);
            }
        } else if (graphicsDriver[0].equals(GraphicsDrivers.VORTEK) && (changed || MainActivity.DEBUG_MODE)) {
            TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, activity, "graphics_driver/vortek-" + DefaultVersion.VORTEK + ".tzst", rootDir);
        }

        switch (graphicsDriver[1]) {
            case GraphicsDrivers.ZINK:
                if (changed) TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, activity, "graphics_driver/zink-" + DefaultVersion.ZINK + ".tzst", rootDir);
                break;
            case GraphicsDrivers.VIRGL:
                if (changed) TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, activity, "graphics_driver/virgl-" + DefaultVersion.VIRGL + ".tzst", rootDir);
                break;
            case GraphicsDrivers.GLADIO:
                if (changed || MainActivity.DEBUG_MODE) TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, activity, "graphics_driver/gladio-" + DefaultVersion.GLADIO + ".tzst", rootDir);
                break;
        }
    }

    private boolean extractDXWrapperFiles() {
        // Mirror XServerDisplayActivity behavior; also populates envVars for DXVK/VKD3D.
        String cacheId = "";
        if (dxwrapper.equals(DXWrappers.DXVK)) {
            com.winlator.contentdialog.DXVKConfigDialog.setEnvVars(activity, dxwrapperConfig[0], envVars);
            cacheId += dxwrapper + "-" + dxwrapperConfig[0].get("version", DefaultVersion.DXVK(graphicsDriver[0]));
        } else if (dxwrapper.equals(DXWrappers.WINED3D)) {
            com.winlator.contentdialog.WineD3DConfigDialog.setEnvVars(dxwrapperConfig[0], envVars);
            cacheId += dxwrapper + "-" + dxwrapperConfig[0].get("version", DefaultVersion.WINED3D);
        }

        String ddrawWrapper = dxwrapperConfig[0].get("ddrawWrapper", DXWrappers.WINED3D);
        cacheId += "-" + DXWrappers.VKD3D + "-" + dxwrapperConfig[1].get("version", DefaultVersion.VKD3D) + "-" + ddrawWrapper;
        boolean changed = !cacheId.equals(container.getExtra("dxwrapper"));
        com.winlator.contentdialog.VKD3DConfigDialog.setEnvVars(dxwrapperConfig[1], envVars);

        if (ddrawWrapper.equals(DXWrappers.CNC_DDRAW)) envVars.put("CNC_DDRAW_CONFIG_FILE", "C:\\ProgramData\\cnc-ddraw\\ddraw.ini");
        if (!changed) return false;

        container.putExtra("dxwrapper", cacheId);

        File rootDir = rootFS.getRootDir();
        File windowsDir = new File(rootDir, RootFS.WINEPREFIX + "/drive_c/windows");

        if (dxwrapper.equals(DXWrappers.WINED3D)) {
            String version = dxwrapperConfig[0].get("version", DefaultVersion.WINED3D);
            if (version.equals(WineInfo.MAIN_WINE_VERSION)) {
                final String[] dlls = {"d3d8.dll","d3d9.dll","d3d10.dll","d3d10_1.dll","d3d10core.dll","d3d11.dll","d3d12.dll","d3d12core.dll","dxgi.dll","ddraw.dll","wined3d.dll"};
                restoreBuiltinDllFiles(dlls);
            } else GeneralComponents.extractFile(GeneralComponents.Type.WINED3D, activity, version, DefaultVersion.WINED3D);
        } else if (dxwrapper.equals(DXWrappers.DXVK)) {
            final boolean[] hasD3D8DllFile = {false};
            final boolean[] hasD3D10DllFile = {false};

            GeneralComponents.extractFile(
                GeneralComponents.Type.DXVK,
                activity,
                dxwrapperConfig[0].get("version"),
                DefaultVersion.DXVK(graphicsDriver[0]),
                (destination, size) -> {
                    String name = destination.getName();
                    if (name.equals("d3d10.dll")) hasD3D10DllFile[0] = true;
                    else if (name.equals("d3d8.dll")) hasD3D8DllFile[0] = true;
                    return destination;
                }
            );

            if (!hasD3D8DllFile[0]) {
                TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, activity, "dxwrapper/d8vk-" + DefaultVersion.D8VK + ".tzst", windowsDir);
            }
            if (!hasD3D10DllFile[0]) restoreBuiltinDllFiles("d3d10.dll", "d3d10_1.dll");
        }

        GeneralComponents.extractFile(GeneralComponents.Type.VKD3D, activity, dxwrapperConfig[1].get("version"), DefaultVersion.VKD3D);

        if (ddrawWrapper.equals(DXWrappers.CNC_DDRAW)) {
            final String assetDir = "dxwrapper/cnc-ddraw-" + DefaultVersion.CNC_DDRAW;
            File configFile = new File(rootDir, RootFS.WINEPREFIX + "/drive_c/ProgramData/cnc-ddraw/ddraw.ini");
            if (!configFile.isFile()) FileUtils.copy(activity, assetDir + "/ddraw.ini", configFile);
            File shadersDir = new File(rootDir, RootFS.WINEPREFIX + "/drive_c/ProgramData/cnc-ddraw/Shaders");
            FileUtils.delete(shadersDir);
            FileUtils.copy(activity, assetDir + "/Shaders", shadersDir);
            TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, activity, assetDir + "/ddraw.tzst", windowsDir);
        } else restoreBuiltinDllFiles("ddraw.dll");

        return true;
    }

    private void extractWinComponentFiles() {
        File rootDir = rootFS.getRootDir();
        File windowsDir = new File(rootDir, RootFS.WINEPREFIX + "/drive_c/windows");
        File systemRegFile = new File(rootDir, RootFS.WINEPREFIX + "/system.reg");

        try {
            JSONObject wincomponentsJSONObject = new JSONObject(FileUtils.readString(activity, "wincomponents/wincomponents.json"));
            Iterator<String[]> oldWinComponentsIter = new KeyValueSet(container.getExtra("wincomponents", Container.FALLBACK_WINCOMPONENTS)).iterator();
            ArrayList<String> builtinDlls = new ArrayList<>();

            for (String[] wincomponent : new KeyValueSet(wincomponents)) {
                if (wincomponent[1].equals(oldWinComponentsIter.next()[1])) continue;
                String identifier = wincomponent[0];
                boolean useNative = wincomponent[1].equals("1");

                if (useNative) {
                    TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, activity, "wincomponents/" + identifier + ".tzst", windowsDir);
                } else {
                    JSONObject wincomponentJSONObject = wincomponentsJSONObject.getJSONObject(identifier);
                    if (wincomponentJSONObject.getBoolean("restoreBuiltinDlls")) {
                        JSONArray dlnames = wincomponentJSONObject.getJSONArray("dlnames");
                        for (int i = 0; i < dlnames.length(); i++) {
                            String dlname = dlnames.getString(i);
                            builtinDlls.add(!dlname.endsWith(".exe") ? dlname + ".dll" : dlname);
                        }
                    } else {
                        TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, activity, "wincomponents/" + identifier + ".tzst", windowsDir, (destination, size) -> {
                            String name = destination.getName();
                            if (name.endsWith(".dll") || name.endsWith(".manifest") || name.endsWith("_deadbeef")) FileUtils.delete(destination);
                            return null;
                        });
                    }
                }

                WineUtils.setWinComponentRegistryKeys(systemRegFile, identifier, useNative);
            }

            if (!builtinDlls.isEmpty()) restoreBuiltinDllFiles(builtinDlls.toArray(new String[0]));
            WineUtils.overrideWinComponentDlls(activity, container, wincomponents);
        } catch (JSONException ignored) {}
    }

    private void restoreBuiltinDllFiles(final String... dlls) {
        File rootDir = rootFS.getRootDir();
        File wineDir = new File(rootDir, rootFS.getWinePath());
        File wineSystem32Dir = new File(wineDir, "/lib/wine/x86_64-windows");
        File wineSysWoW64Dir = new File(wineDir, "/lib/wine/i386-windows");
        File containerSystem32Dir = new File(rootDir, RootFS.WINEPREFIX + "/drive_c/windows/system32");
        File containerSysWoW64Dir = new File(rootDir, RootFS.WINEPREFIX + "/drive_c/windows/syswow64");

        for (String dll : dlls) {
            FileUtils.copy(new File(wineSysWoW64Dir, dll), new File(containerSysWoW64Dir, dll));
            FileUtils.copy(new File(wineSystem32Dir, dll), new File(containerSystem32Dir, dll));
        }
    }

    private void applyGeneralPatches(Container container) {
        File rootDir = rootFS.getRootDir();
        FileUtils.delete(new File(rootDir, "/opt/apps"));
        TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, activity, "rootfs_patches.tzst", rootDir);
        TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, activity, "pulseaudio.tzst", new File(activity.getFilesDir(), "pulseaudio"));
        if (wineInfo != null) WineUtils.applySystemTweaks(activity, wineInfo);
        container.putExtra("graphicsDriver", null);
        container.putExtra("dxwrapper", null);
        container.putExtra("desktopTheme", null);
        if (activity instanceof androidx.appcompat.app.AppCompatActivity) {
            SettingsFragment.resetBox64Version((androidx.appcompat.app.AppCompatActivity) activity);
        }
    }

    private boolean verifyUserRegistry() {
        File userRegFile = new File(rootFS.getRootDir(), RootFS.WINEPREFIX + "/user.reg");
        String lastModified = String.valueOf(userRegFile.lastModified());

        if (!lastModified.equals(container.getExtra("userRegLastModified"))) {
            try (WineRegistryEditor registryEditor = new WineRegistryEditor(userRegFile)) {
                registryEditor.removeKey("Software\\Wow6432Node\\Wine", true);
            }
            container.putExtra("userRegLastModified", lastModified);
            return true;
        }
        return false;
    }

    private void applyGraphicsDriverEnvVars() {
        if (graphicsDriver == null || graphicsDriver.length < 2 || graphicsDriverConfig == null || graphicsDriverConfig.length < 2) return;

        envVars.put("vblank_mode", "0");

        if (graphicsDriver[0].equals(GraphicsDrivers.TURNIP)) {
            envVars.put("MESA_VK_WSI_PRESENT_MODE", "mailbox");
            TurnipConfigDialog.setEnvVars(activity, graphicsDriverConfig[0], envVars);
        }

        switch (graphicsDriver[1]) {
            case GraphicsDrivers.ZINK:
                envVars.put("GALLIUM_DRIVER", "zink");
                envVars.put("ZINK_CONTEXT_THREADED", "1");
                if (graphicsDriver[0].equals(GraphicsDrivers.VORTEK)) envVars.put("MESA_GL_VERSION_OVERRIDE", "3.3");
                break;
            case GraphicsDrivers.VIRGL:
                envVars.put("GALLIUM_DRIVER", "virpipe");
                envVars.put("VIRGL_NO_READBACK", "true");
                envVars.put("VIRGL_SERVER_PATH", rootFS.getRootDir() + UnixSocketConfig.VIRGL_SERVER_PATH);
                VirGLConfigDialog.setEnvVars(graphicsDriverConfig[1], envVars);
                break;
            case GraphicsDrivers.GLADIO:
                envVars.put("GLADIO_NO_ERROR", "1");
                break;
        }
    }

    private String getWineStartCommand() {
        String cmdArgs = "/dir C:\\\\windows \"wfm.exe\"";
        if (overrideEnvVars != null && overrideEnvVars.has("EXTRA_EXEC_ARGS")) {
            cmdArgs += " " + overrideEnvVars.get("EXTRA_EXEC_ARGS");
            overrideEnvVars.remove("EXTRA_EXEC_ARGS");
        }
        return "C:\\\\windows\\\\winhandler.exe " + cmdArgs;
    }

    @Override
    public Context getContext() {
        return themedContext;
    }

    @Override
    public Activity getActivity() {
        return activity;
    }

    @Override
    public SharedPreferences getPreferences() {
        return preferences;
    }

    @Nullable
    @Override
    public InputControlsView getInputControlsView() {
        return inputControlsView;
    }

    @Nullable
    @Override
    public XServerView getXServerView() {
        return xServerView;
    }

    @Nullable
    @Override
    public XServer getXServer() {
        return xServer;
    }

    @Nullable
    @Override
    public WinHandler getWinHandler() {
        return winHandler;
    }

    @Nullable
    @Override
    public DebugDialog getDebugDialog() {
        return debugDialog;
    }

    @Nullable
    @Override
    public String getScreenEffectProfile() {
        return screenEffectProfile;
    }

    @Override
    public void setScreenEffectProfile(@Nullable String value) {
        this.screenEffectProfile = value;
    }

    @Override
    public EnvVars getOverrideEnvVars() {
        return overrideEnvVars;
    }

    @Override
    public void setScreenInfo(ScreenInfo screenInfo) {
        this.screenInfo = screenInfo;
    }

    @Override
    public void setDXWrapper(String dxWrapper) {
        this.dxwrapper = DXWrappers.parseIdentifier(dxWrapper);
    }

    @Override
    public void setWinComponents(String wincomponents) {
        this.wincomponents = wincomponents;
    }

    @Override
    public void showKeyboard() {
        if (imeSinkView == null) return;
        imeSinkView.post(() -> {
            imeSinkView.requestFocus();
            InputMethodManager imm = (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.showSoftInput(imeSinkView, InputMethodManager.SHOW_IMPLICIT);
            }
        });
    }

    /**
     * Route Activity-level key events to Winlator (mirrors {@link XServerDisplayActivity#dispatchKeyEvent}).
     */
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (inputControlsView != null && inputControlsView.onKeyEvent(event)) return true;
        if (winHandler != null && winHandler.onKeyEvent(event)) return true;
        if (xServer != null && xServer.keyboard.onKeyEvent(event)) return true;
        return false;
    }

    @Override
    public void showInputControlsDialog() {
        final ContentDialog dialog = new ContentDialog(themedContext, R.layout.input_controls_dialog);
        dialog.setTitle(R.string.input_controls);
        dialog.setIcon(R.drawable.icon_input_controls);

        final Spinner sProfile = dialog.findViewById(R.id.SProfile);
        Runnable loadProfileSpinner = () -> {
            ArrayList<ControlsProfile> profiles = inputControlsManager.getProfiles(true);
            ArrayList<String> profileItems = new ArrayList<>();
            int selectedPosition = 0;
            profileItems.add("-- "+themedContext.getString(R.string.disabled)+" --");
            for (int i = 0; i < profiles.size(); i++) {
                ControlsProfile profile = profiles.get(i);
                if (profile == inputControlsView.getProfile()) selectedPosition = i + 1;
                profileItems.add(profile.getName());
            }

            sProfile.setAdapter(new ArrayAdapter<>(themedContext, android.R.layout.simple_spinner_dropdown_item, profileItems));
            sProfile.setSelection(selectedPosition);
        };
        loadProfileSpinner.run();

        final CheckBox cbRelativeMouseMovement = dialog.findViewById(R.id.CBRelativeMouseMovement);
        cbRelativeMouseMovement.setChecked(xServer.isRelativeMouseMovement());

        final CheckBox cbShowTouchscreenControls = dialog.findViewById(R.id.CBShowTouchscreenControls);
        cbShowTouchscreenControls.setChecked(inputControlsView.isShowTouchscreenControls());

        dialog.findViewById(R.id.BTSettings).setOnClickListener((v) -> {
            int position = sProfile.getSelectedItemPosition();
            Intent intent = new Intent(activity, MainActivity.class);
            intent.putExtra("edit_input_controls", true);
            intent.putExtra("selected_profile_id", position > 0 ? inputControlsManager.getProfiles().get(position - 1).id : 0);
            // In embedded mode, we don't have a direct callback from MainActivity yet, 
            // but we can at least open the settings.
            activity.startActivity(intent);
        });

        dialog.setOnConfirmCallback(() -> {
            xServer.setRelativeMouseMovement(cbRelativeMouseMovement.isChecked());
            inputControlsView.setShowTouchscreenControls(cbShowTouchscreenControls.isChecked());
            int position = sProfile.getSelectedItemPosition();
            if (position > 0) {
                showInputControls(inputControlsManager.getProfiles().get(position - 1));
            }
            else hideInputControls();
        });

        dialog.show();
    }

    private void showInputControls(ControlsProfile profile) {
        inputControlsView.setVisibility(View.VISIBLE);
        inputControlsView.requestFocus();
        inputControlsView.setProfile(profile);

        touchpadView.setSensitivity(profile.getCursorSpeed() * globalCursorSpeed);
        touchpadView.setPointerButtonRightEnabled(false);

        GLRenderer renderer = xServerView.getRenderer();
        if (profile.isDisableMouseInput()) {
            renderer.setCursorVisible(false);
            touchpadView.setEnabled(false);
        }
        else {
            renderer.setCursorVisible(true);
            touchpadView.setEnabled(true);
        }

        inputControlsView.invalidate();
    }

    private void hideInputControls() {
        inputControlsView.setShowTouchscreenControls(true);
        inputControlsView.setVisibility(View.GONE);
        inputControlsView.setProfile(null);

        touchpadView.setSensitivity(globalCursorSpeed);
        touchpadView.setPointerButtonLeftEnabled(true);
        touchpadView.setPointerButtonRightEnabled(true);

        if (!touchpadView.isEnabled()) {
            touchpadView.setEnabled(true);
            xServerView.getRenderer().setCursorVisible(true);
        }

        inputControlsView.invalidate();
    }

    @Override
    public void toggleFullscreen() {
        xServerView.getRenderer().toggleFullscreen();
    }

    @Override
    public void showTaskManagerDialog() {
        (new TaskManagerDialog(this)).show();
    }

    @Override
    public void showActiveWindowsDialog() {
        (new ActiveWindowsDialog(this)).show();
    }

    @Override
    public void toggleMagnifier() {
        if (magnifierView == null) {
            final FrameLayout containerView = rootView.findViewById(R.id.FLXServerDisplay);
            magnifierView = new MagnifierView(themedContext);
            magnifierView.setZoomButtonCallback((value) -> {
                final GLRenderer renderer = xServerView.getRenderer();
                renderer.setMagnifierZoom(Mathf.clamp(renderer.getMagnifierZoom() + value, 1.0f, 3.0f));
                magnifierView.setZoomValue(renderer.getMagnifierZoom());
            });
            magnifierView.setZoomValue(xServerView.getRenderer().getMagnifierZoom());
            magnifierView.setHideButtonCallback(() -> {
                containerView.removeView(magnifierView);
                magnifierView = null;
            });
            containerView.addView(magnifierView);
        }
        else {
            final FrameLayout containerView = rootView.findViewById(R.id.FLXServerDisplay);
            containerView.removeView(magnifierView);
            magnifierView = null;
        }
    }

    @Override
    public void showScreenEffectDialog() {
        (new ScreenEffectDialog(this)).show();
    }

    @Override
    public void showDebugDialog() {
        debugDialog.show();
    }

    @Override
    public void showTouchpadHelpDialog() {
        ContentDialog dialog = new ContentDialog(themedContext, R.layout.touchpad_help_dialog);
        dialog.setTitle(R.string.touchpad_help);
        dialog.setIcon(R.drawable.icon_help);
        dialog.findViewById(R.id.BTCancel).setVisibility(View.GONE);
        dialog.show();
    }

    @Override
    public void exit() {
        stop();
        // In embedded mode, we might want to switch UI mode back to CONTAINERS
        // This is handled by the caller or by observing winlatorController state.
    }

    @Override
    public void runOnUiThread(Runnable r) {
        activity.runOnUiThread(r);
    }
}

