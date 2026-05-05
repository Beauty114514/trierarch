package com.winlator.core;

import androidx.annotation.NonNull;

import java.util.Iterator;
import java.util.LinkedHashMap;

public class EnvVars implements Iterable<String> {
    private final LinkedHashMap<String, String> data = new LinkedHashMap<>();

    /** TMP/TMPDIR/TEMP/WINE_TMPDIR: merged from serialized container env they can override launcher defaults. */
    private static final String[] EPHEMERAL_TMP_KEYS = {
        "TMPDIR", "TMP", "TEMP", "WINE_TMPDIR",
    };

    /** Serialized envVars string with ephemeral tmp keys removed (for {@link com.winlator.container.Container} load/save). */
    public static String stripEphemeralTmpKeysFromSerialized(String spaceSeparatedEnv) {
        if (spaceSeparatedEnv == null || spaceSeparatedEnv.isEmpty()) return "";
        EnvVars ev = new EnvVars(spaceSeparatedEnv);
        for (String k : EPHEMERAL_TMP_KEYS) ev.remove(k);
        return ev.toString();
    }

    /** Copy without tmp overrides so guest launch can apply RootFS tmp after merge. */
    public static EnvVars copyWithoutEphemeralTmpKeys(EnvVars src) {
        if (src == null || src.isEmpty()) return new EnvVars();
        EnvVars out = new EnvVars();
        for (String name : src) {
            boolean drop = false;
            for (String k : EPHEMERAL_TMP_KEYS) {
                if (k.equals(name)) {
                    drop = true;
                    break;
                }
            }
            if (!drop) out.put(name, src.get(name));
        }
        return out;
    }

    public EnvVars() {}

    public EnvVars(String values) {
        putAll(values);
    }

    public EnvVars put(String name, Object value) {
        data.put(name, String.valueOf(value));
        return this;
    }

    public void putAll(String[] items) {
        if (items == null) return;
        for (String item : items) {
            int index = item.indexOf("=");
            String name = item.substring(0, index);
            String value = item.substring(index+1);
            data.put(name, value);
        }
    }

    public void putAll(String values) {
        if (values == null || values.isEmpty()) return;
        putAll(values.split(" "));
    }

    public void putAll(EnvVars envVars) {
        data.putAll(envVars.data);
    }

    public String get(String name) {
        return data.getOrDefault(name, "");
    }

    public void remove(String name) {
        data.remove(name);
    }

    public boolean has(String name) {
        return data.containsKey(name);
    }

    public void clear() {
        data.clear();
    }

    public boolean isEmpty() {
        return data.isEmpty();
    }

    @NonNull
    @Override
    public String toString() {
        return String.join(" ", toStringArray());
    }

    public String toEscapedString() {
        String result = "";
        for (String key : data.keySet()) {
            if (!result.isEmpty()) result += " ";
            String value = data.get(key);
            result += key+"="+value.replace(" ", "\\ ");
        }
        return result;
    }

    public String[] toStringArray() {
        String[] stringArray = new String[data.size()];
        int index = 0;
        for (String key : data.keySet()) stringArray[index++] = key+"="+data.get(key);
        return stringArray;
    }

    @NonNull
    @Override
    public Iterator<String> iterator() {
        return data.keySet().iterator();
    }
}
