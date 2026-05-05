package com.winlator.core;

import android.os.Process;
import android.system.Os;
import android.util.Log;

import androidx.annotation.NonNull;

import com.winlator.MainActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Scanner;
import java.util.concurrent.Executors;
import java.util.regex.Pattern;

public abstract class ProcessHelper {
    private static final String TAG = "Trierarch-ProcessHelper";
    /** Guest (Wine/box64) stdout+stderr — filter: adb logcat -s Trierarch-Guest:I */
    public static final String GUEST_LOG_TAG = "Trierarch-Guest";

    public enum PState {RUNNING, SLEEPING, WAITING, ZOMBIE, STOPPED, DEAD, OTHER}
    private static final ArrayList<Callback<String>> debugCallbacks = new ArrayList<>();
    private static final byte SIGCONT = 18;
    private static final byte SIGSTOP = 19;

    public static class PStat {
        public int pid = 0;
        public String name = "";
        public PState state = PState.OTHER;
        public int parentPID = 0;
        public boolean guestProcess = false;

        @NonNull
        @Override
        public String toString() {
            return pid+" "+name+" "+state+" "+parentPID+" "+guestProcess;
        }
    }

    public static void suspendProcess(int pid) {
        Process.sendSignal(pid, SIGSTOP);
    }

    public static void resumeProcess(int pid) {
        Process.sendSignal(pid, SIGCONT);
    }

    public static int exec(String command) {
        return exec(command, null);
    }

    public static int exec(String command, EnvVars envVars) {
        return exec(command, envVars, null);
    }

    public static int exec(String command, EnvVars envVars, File workingDir) {
        return exec(command, envVars, workingDir, null);
    }

    public static int exec(String command, EnvVars envVars, File workingDir, Callback<Integer> terminationCallback) {
        return exec(command, envVars, workingDir, terminationCallback, false);
    }

    /**
     * @param mirrorGuestStreamsToLogcat if true (and no in-app debug UI callbacks), guest stdout/stderr are merged
     *                                   and copied to {@link #GUEST_LOG_TAG} so adb works without enabling sandboxed prefs.
     */
    public static int exec(String command, EnvVars envVars, File workingDir, Callback<Integer> terminationCallback,
                           boolean mirrorGuestStreamsToLogcat) {
        ProcessBuilder direct = new ProcessBuilder(splitCommand(command)).directory(workingDir);
        int pid = startGuestProcess(direct, envVars, terminationCallback, mirrorGuestStreamsToLogcat, true);
        if (pid != -1) return pid;

        // Some OEM ROMs return EACCES (error=13) when the Java child execve's an ELF under files/wine, while the same
        // binary runs under `run-as app …` from adb (different domain / seccomp ancestry). Wrap with system sh + exec.
        Log.w(TAG, "Direct exec failed; retrying via /system/bin/sh -c 'cd … && exec …' (ROM workaround)");
        ProcessBuilder wrapped = buildShellWrappedCommand(command, workingDir, envVars);
        // Do not merge guest EnvVars into ProcessBuilder.environment(): LD_LIBRARY_PATH would make bionic load
        // glibc linker scripts from wine/usr/lib when linking /system/bin/sh (bad ELF magic 2f2a = "/*").
        pid = startGuestProcess(wrapped, envVars, terminationCallback, mirrorGuestStreamsToLogcat, false);
        if (pid == -1) {
            Log.e(TAG, "Both direct and shell-wrapped exec failed. cmd preview: "
                + (command != null && command.length() > 300 ? command.substring(0, 300) + "…" : command));
        }
        return pid;
    }

    /**
     * Guest env is exported inside the script so /system/bin/sh starts with a normal bionic environment; after cd,
     * exports apply only to the exec'd box64/wine tree.
     */
    private static ProcessBuilder buildShellWrappedCommand(String command, File workingDir, EnvVars envVars) {
        String[] argv = splitCommand(command);
        String wd = workingDir != null ? workingDir.getAbsolutePath() : ".";
        // Clear inherited linker vars inside the script too (before cd), in case the shell binary ignores stripped PB env.
        StringBuilder sb = new StringBuilder("unset LD_LIBRARY_PATH LD_PRELOAD LD_AUDIT LD_DEBUG 2>/dev/null; cd ");
        sb.append(shellSingleQuoted(wd));
        if (envVars != null) {
            for (String name : envVars) {
                sb.append(" && export ");
                sb.append(name);
                sb.append("=");
                sb.append(shellSingleQuoted(envVars.get(name)));
            }
        }
        sb.append(" && exec");
        for (String a : argv) {
            sb.append(' ').append(shellSingleQuoted(a));
        }
        return new ProcessBuilder("/system/bin/sh", "-c", sb.toString()).directory(workingDir);
    }

    private static final String[] DYNAMIC_LINKER_ENV_KEYS_TO_STRIP = {
        "LD_LIBRARY_PATH", "LD_PRELOAD", "LD_AUDIT", "LD_DEBUG", "LD_HWCAP_MASK",
        "ANDROID_LD_LIBRARY_PATH", "LD_ASSUME_KERNEL", "LD_BIND_NOW", "LD_SHOW_AUXV",
    };

    /**
     * The JVM inherits Android shell env; keys like {@code VK_ICD_FILENAMES} often point at host paths.
     * The Wine/box64 guest uses Mesa ICDs under {@code files/wine/usr/share/vulkan/icd.d} — inherited overrides
     * make {@code vkCreateInstance} fail and tools like GPUInfo show "Unavailable". Strip unless the guest env
     * explicitly sets the same key (container env / launcher).
     */
    private static final String[] INHERITED_VULKAN_LOADER_KEYS_TO_STRIP = {
        "VK_ICD_FILENAMES", "VK_DRIVER_FILES", "VK_LAYER_PATH",
    };

    /** Remove vars that make bionic/glibc loaders pick up guest rootfs libs while exec'ing /system/bin/sh or box64. */
    private static void stripDynamicLinkerPoison(Map<String, String> environment) {
        for (String key : DYNAMIC_LINKER_ENV_KEYS_TO_STRIP) {
            environment.remove(key);
        }
    }

    private static void stripInheritedVulkanLoaderOverrides(Map<String, String> environment, EnvVars guestEnvVars) {
        if (environment == null || environment.isEmpty()) return;
        EnvVars guest = guestEnvVars != null ? guestEnvVars : new EnvVars();
        for (String key : INHERITED_VULKAN_LOADER_KEYS_TO_STRIP) {
            if (guest.has(key)) continue;
            String old = environment.get(key);
            if (old != null) {
                Log.i(TAG, "Removed inherited " + key + " (value was set from Android/JVM; breaks guest ICD paths)");
                environment.remove(key);
            }
        }
    }

    /** POSIX-style single-quote for sh -c (embedded ' -> '\'' ). */
    private static String shellSingleQuoted(String s) {
        if (s == null) return "''";
        return "'" + s.replace("'", "'\"'\"'") + "'";
    }

    /**
     * @param mergeGuestEnvIntoProcessEnvironment false for shell-wrapper path (guest env is only in -c script).
     */
    private static int startGuestProcess(ProcessBuilder processBuilder, EnvVars envVars, Callback<Integer> terminationCallback,
                                         boolean mirrorGuestStreamsToLogcat, boolean mergeGuestEnvIntoProcessEnvironment) {
        try {
            if (!debugCallbacks.isEmpty()) {
                // Debug UI: keep separate pipes (default ProcessBuilder).
            }
            else if (mirrorGuestStreamsToLogcat) {
                processBuilder.redirectErrorStream(true);
            }
            else {
                processBuilder.redirectOutput(new File("/dev/null")).redirectErrorStream(true);
            }

            Map<String, String> environment = processBuilder.environment();
            if (mergeGuestEnvIntoProcessEnvironment && envVars != null) {
                for (String name : envVars) environment.put(name, envVars.get(name));
            }
            else if (!mergeGuestEnvIntoProcessEnvironment) {
                // Shell-wrapper path: inherited JVM/env may still set LD_* so bionic resolves libc from guest usr/lib
                // before the -c script runs — strip so /system/bin/sh and the inner exec box64 see a clean linker view.
                stripDynamicLinkerPoison(environment);
            }
            stripInheritedVulkanLoaderOverrides(environment, envVars);

            java.lang.Process process = processBuilder.start();
            Field pidField = process.getClass().getDeclaredField("pid");
            pidField.setAccessible(true);
            int pid = pidField.getInt(process);
            pidField.setAccessible(false);

            if (!debugCallbacks.isEmpty()) {
                createDebugThread(process.getInputStream());
                createDebugThread(process.getErrorStream());
            }
            else if (mirrorGuestStreamsToLogcat) {
                createGuestLogcatMirrorThread(process.getInputStream());
            }

            if (terminationCallback != null) createWaitForThread(process, terminationCallback);
            return pid;
        }
        catch (Exception e) {
            Log.d(TAG, "ProcessBuilder.start failed: " + e.getMessage());
            return -1;
        }
    }

    private static void createGuestLogcatMirrorThread(final InputStream inputStream) {
        Executors.newSingleThreadExecutor().execute(() -> {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    Log.i(GUEST_LOG_TAG, line);
                }
            }
            catch (IOException e) {
                Log.d(GUEST_LOG_TAG, "(stream end: " + e.getMessage() + ")");
            }
        });
    }

    private static void createDebugThread(final InputStream inputStream) {
        Executors.newSingleThreadExecutor().execute(() -> {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    synchronized (debugCallbacks) {
                        if (!debugCallbacks.isEmpty()) {
                            for (Callback<String> callback : debugCallbacks) callback.call(line);
                        }
                        else if (MainActivity.DEBUG_MODE) System.out.println(line);
                    }
                }
            }
            catch (IOException e) {}
        });
    }

    private static void createWaitForThread(java.lang.Process process, final Callback<Integer> terminationCallback) {
        Executors.newSingleThreadExecutor().execute(() -> {
            try {
                int status = process.waitFor();
                terminationCallback.call(status);
            }
            catch (InterruptedException e) {}
        });
    }

    public static void removeAllDebugCallbacks() {
        synchronized (debugCallbacks) {
            debugCallbacks.clear();
        }
    }

    public static void addDebugCallback(Callback<String> callback) {
        synchronized (debugCallbacks) {
            if (!debugCallbacks.contains(callback)) debugCallbacks.add(callback);
        }
    }

    public static void removeDebugCallback(Callback<String> callback) {
        synchronized (debugCallbacks) {
            debugCallbacks.remove(callback);
        }
    }

    public static String[] splitCommand(String command) {
        ArrayList<String> result = new ArrayList<>();
        boolean startedQuotes = false;
        String value = "";
        char currChar, nextChar;
        for (int i = 0, count = command.length(); i < count; i++) {
            currChar = command.charAt(i);

            if (startedQuotes) {
                if (currChar == '"') {
                    startedQuotes = false;
                    if (!value.isEmpty()) {
                        value += '"';
                        result.add(value);
                        value = "";
                    }
                }
                else value += currChar;
            }
            else if (currChar == '"') {
                startedQuotes = true;
                value += '"';
            }
            else {
                nextChar = i < count-1 ? command.charAt(i+1) : '\0';
                if (currChar == ' ' || (currChar == '\\' && nextChar == ' ')) {
                    if (currChar == '\\') {
                        value += ' ';
                        i++;
                    }
                    else if (!value.isEmpty()) {
                        result.add(value);
                        value = "";
                    }
                }
                else {
                    value += currChar;
                    if (i == count-1) {
                        result.add(value);
                        value = "";
                    }
                }
            }
        }

        return result.toArray(new String[0]);
    }

    public static String getAffinityMaskAsHexString(String cpuList) {
        String[] values = cpuList.split(",");
        int affinityMask = 0;
        for (String value : values) {
            byte index = Byte.parseByte(value);
            affinityMask |= (int)Math.pow(2, index);
        }
        return Integer.toHexString(affinityMask);
    }

    public static int getAffinityMask(String cpuList) {
        if (cpuList == null || cpuList.isEmpty()) return 0;
        String[] values = cpuList.split(",");
        int affinityMask = 0;
        for (String value : values) {
            byte index = Byte.parseByte(value);
            affinityMask |= (int)Math.pow(2, index);
        }
        return affinityMask;
    }

    public static int getAffinityMask(boolean[] cpuList) {
        int affinityMask = 0;
        for (int i = 0; i < cpuList.length; i++) {
            if (cpuList[i]) affinityMask |= (int)Math.pow(2, i);
        }
        return affinityMask;
    }

    public static int getAffinityMask(int from, int to) {
        int affinityMask = 0;
        for (int i = from; i < to; i++) affinityMask |= (int)Math.pow(2, i);
        return affinityMask;
    }

    public static List<PStat> getChildProcesses() {
        File procFile = new File("/proc");
        String[] pids = procFile.list((file, name) -> (new File(file, name)).isDirectory() && name.matches("[0-9]+"));
        if (pids == null) return Collections.emptyList();
        ArrayList<PStat> result = new ArrayList<>();
        int parentPID = Os.getpid();

        for (String pid : pids) {
            try (Scanner scanner = new Scanner(new FileInputStream("/proc/"+pid+"/stat"))) {
                PStat pstat = new PStat();
                int index = 0;

                while (scanner.hasNext() && index < 4) {
                    switch (index++) {
                        case 0:
                            pstat.pid = scanner.nextInt();
                            break;
                        case 1:
                            Pattern oldDelimiter = scanner.delimiter();
                            scanner.useDelimiter("\\)");
                            pstat.name = scanner.hasNext() ? scanner.next().substring(2) : "";
                            scanner.useDelimiter(oldDelimiter);
                            if (scanner.hasNext()) scanner.next();
                            break;
                        case 2: {
                            switch (scanner.next()) {
                                case "R":
                                    pstat.state = PState.RUNNING;
                                    break;
                                case "S":
                                    pstat.state = PState.SLEEPING;
                                    break;
                                case "D":
                                    pstat.state = PState.WAITING;
                                    break;
                                case "Z":
                                    pstat.state = PState.ZOMBIE;
                                    break;
                                case "T":
                                    pstat.state = PState.STOPPED;
                                    break;
                                case "X":
                                    pstat.state = PState.DEAD;
                                    break;
                            }
                            break;
                        }
                        case 3:
                            pstat.parentPID = scanner.nextInt();
                            break;
                    }
                }

                if (pstat.parentPID == parentPID || pstat.pid > parentPID) {
                    pstat.guestProcess = pstat.name.contains("wine") || pstat.name.contains(".exe");
                    result.add(pstat);
                }
            }
            catch (Exception e) {
                return Collections.emptyList();
            }
        }

        return result;
    }
}
