package com.winlator.core;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.winlator.math.Mathf;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.Closeable;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.CharBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Locale;

/**
 * Winlator-shaped registry editor.
 *
 * Note: upstream has a {@code System.loadLibrary("winlator")} side effect here; Trierarch does not
 * ship that library. The implementation below is pure-Java and does not require native code.
 */
public class WineRegistryEditor implements Closeable {
    private final File file;
    private final File cloneFile;
    private boolean modified = false;
    private boolean createKeyIfNotExist = true;

    public static class Location {
        public final int offset;
        public final int start;
        public final int end;
        public int mbCount;
        private Object tag;

        public Location(int offset, int start, int end) {
            this.offset = offset;
            this.start = start;
            this.end = end;
        }

        public int length() {
            return end - start;
        }

        @NonNull
        @Override
        public String toString() {
            return offset + "," + start + "," + end;
        }

        public int[] toIntArray() {
            return new int[]{offset, start, end, mbCount};
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof Location)) return false;
            Location other = (Location)obj;
            return this.offset == other.offset && this.start == other.start && this.end == other.end;
        }
    }

    public WineRegistryEditor(File file) {
        this.file = file;
        cloneFile = FileUtils.createTempFile(file.getParentFile(), FileUtils.getBasename(file.getPath()));
        if (!file.isFile()) {
            try {
                cloneFile.createNewFile();
            }
            catch (IOException e) {}
        }
        else FileUtils.copy(file, cloneFile);
    }

    private static String escape(String str) {
        return str.replace("\\", "\\\\").replace("\"", "\\\"");
    }

    private static String unescape(String str) {
        return str.replace("\\\"", "\"").replace("\\\\", "\\");
    }

    @Override
    public void close() {
        if (modified && cloneFile.exists()) {
            //noinspection ResultOfMethodCallIgnored
            cloneFile.renameTo(file);
        }
        else cloneFile.delete();
    }

    public void setCreateKeyIfNotExist(boolean createKeyIfNotExist) {
        this.createKeyIfNotExist = createKeyIfNotExist;
    }

    private Location createKey(String key) {
        Location location = getParentKeyLocation(key);
        boolean success = false;

        char[] buffer = new char[StreamUtils.BUFFER_SIZE];
        File tempFile = FileUtils.createTempFile(file.getParentFile(), FileUtils.getBasename(file.getPath()));

        try (BufferedReader reader = new BufferedReader(new FileReader(cloneFile), StreamUtils.BUFFER_SIZE);
             BufferedWriter writer = new BufferedWriter(new FileWriter(tempFile), StreamUtils.BUFFER_SIZE)) {

            int length;
            if (location != null) {
                for (int i = 0, end = location.end + 1; i < end; i += length) {
                    length = Math.min(buffer.length, end - i);
                    //noinspection ResultOfMethodCallIgnored
                    reader.read(buffer, 0, length);
                    writer.write(buffer, 0, length);
                }
            }
            else while ((length = reader.read(buffer)) != -1) writer.write(buffer, 0, length);

            // Section header must be exactly one line "[...]"; #time must be on the next line so
            // findKeyLocations() can match the header with equals().
            long ticks1601To1970 = 86400L * (369 * 365 + 89) * 10000000;
            long currentTime = System.currentTimeMillis() + ticks1601To1970;
            String content =
                "\n[" + escape(key) + "]\n" +
                    String.format(Locale.ENGLISH, "#time=%x%08x", currentTime >> 32, (int)currentTime) +
                    "\n";
            writer.write(content);

            while ((length = reader.read(buffer)) != -1) writer.write(buffer, 0, length);
            success = true;
        }
        catch (IOException e) {}

        if (success) {
            modified = true;
            //noinspection ResultOfMethodCallIgnored
            tempFile.renameTo(cloneFile);
            return getKeyLocation(key);
        }
        else {
            tempFile.delete();
            return null;
        }
    }

    public String getStringValue(String key, String name) {
        return getStringValue(key, name, null);
    }

    public String getStringValue(String key, String name, String fallback) {
        String value = getRawValue(key, name);
        return value != null ? value.substring(1, value.length() - 1) : fallback;
    }

    public void setStringValue(String key, String name, String value) {
        setRawValue(key, name, value != null ? "\"" + escape(value) + "\"" : "\"\"");
    }

    public void setStringValues(String key, String[]... items) {
        String[][] escapedItems = new String[items.length][];
        for (int i = 0; i < items.length; i++) {
            escapedItems[i] = new String[]{items[i][0], items[i][1] != null ? "\"" + escape(items[i][1]) + "\"" : "\"\""};
        }
        setRawValues(key, escapedItems);
    }

    public Integer getDwordValue(String key, String name) {
        return getDwordValue(key, name, null);
    }

    public Integer getDwordValue(String key, String name, Integer fallback) {
        String value = getRawValue(key, name);
        return value != null ? Integer.decode("0x" + value.substring(6)) : fallback;
    }

    public void setDwordValue(String key, String name, int value) {
        setRawValue(key, name, "dword:" + String.format("%08x", value));
    }

    public void setHexValue(String key, String name, String value) {
        int start = (int)Mathf.roundTo(name.length(), 2) + 7;
        StringBuilder lines = new StringBuilder();
        for (int i = 0, j = start; i < value.length(); i++) {
            if (i > 0 && (i % 2) == 0) lines.append(",");
            if (j++ > 56) {
                lines.append("\\\n  ");
                j = 8;
            }
            lines.append(value.charAt(i));
        }
        setRawValue(key, name, "hex:" + lines);
    }

    public void setHexValues(String key, String name, byte[] bytes) {
        StringBuilder data = new StringBuilder();
        for (byte b : bytes) data.append(String.format(Locale.ENGLISH, "%02x", Byte.toUnsignedInt(b)));
        setHexValue(key, name, data.toString());
    }

    public byte[] getHexValues(String key, String name) {
        String value = getRawValue(key, name);
        if (value != null && (value.startsWith("hex:") || value.startsWith("hex("))) {
            String[] items = value.replaceAll("hex[\\(\\)0-9]*:", "").replace("\\\n  ", "").split(",");
            byte[] bytes = new byte[items.length];
            for (int i = 0; i < items.length; i++) {
                try {
                    bytes[i] = Integer.decode("0x" + items[i]).byteValue();
                }
                catch (NumberFormatException e) {}
            }
            return bytes;
        }
        return null;
    }

    public String getSymlinkValue(String key, String name) {
        byte[] symlinkBytes = getHexValues(key, name);
        if (symlinkBytes != null) {
            CharBuffer buffer = ByteBuffer.wrap(symlinkBytes).order(ByteOrder.LITTLE_ENDIAN).asCharBuffer();
            return buffer.toString().replace("\\Registry\\Machine\\", "");
        }
        else return null;
    }

    private String getRawValue(String key, String name) {
        Location keyLocation = getKeyLocation(key);
        if (keyLocation == null) return null;

        Location valueLocation = getValueLocation(keyLocation, name);
        if (valueLocation == null) return null;
        boolean success = false;
        char[] buffer = new char[valueLocation.length()];

        try (BufferedReader reader = new BufferedReader(new FileReader(cloneFile), StreamUtils.BUFFER_SIZE)) {
            //noinspection ResultOfMethodCallIgnored
            reader.skip(valueLocation.start);
            success = reader.read(buffer) == buffer.length;
        }
        catch (IOException e) {}
        return success ? unescape(new String(buffer)) : null;
    }

    private void setRawValue(String key, String name, String value) {
        Location keyLocation = getKeyLocation(key);
        if (keyLocation == null) {
            if (createKeyIfNotExist) {
                keyLocation = createKey(key);
            }
            else return;
        }
        if (keyLocation == null) return;

        Location valueLocation = getValueLocation(keyLocation, name);
        char[] buffer = new char[StreamUtils.BUFFER_SIZE];
        boolean success = false;

        File tempFile = FileUtils.createTempFile(file.getParentFile(), FileUtils.getBasename(file.getPath()));

        try (BufferedReader reader = new BufferedReader(new FileReader(cloneFile), StreamUtils.BUFFER_SIZE);
             BufferedWriter writer = new BufferedWriter(new FileWriter(tempFile), StreamUtils.BUFFER_SIZE)) {

            int length;
            for (int i = 0, end = valueLocation != null ? valueLocation.start : keyLocation.end; i < end; i += length) {
                length = Math.min(buffer.length, end - i);
                //noinspection ResultOfMethodCallIgnored
                reader.read(buffer, 0, length);
                writer.write(buffer, 0, length);
            }

            String content = "\""+escape(name)+"\"="+value;
            if (valueLocation != null) {
                writer.write(content);
                //noinspection ResultOfMethodCallIgnored
                reader.skip(valueLocation.length());
            }
            else writer.write("\n" + content);

            while ((length = reader.read(buffer)) != -1) writer.write(buffer, 0, length);
            success = true;
        }
        catch (IOException e) {}

        if (success) {
            modified = true;
            //noinspection ResultOfMethodCallIgnored
            tempFile.renameTo(cloneFile);
        }
        else tempFile.delete();
    }

    public void removeValue(String key, String name) {
        Location keyLocation = getKeyLocation(key);
        if (keyLocation == null) return;

        Location valueLocation = getValueLocation(keyLocation, name);
        if (valueLocation == null) return;

        char[] buffer = new char[StreamUtils.BUFFER_SIZE];
        boolean success = false;
        File tempFile = FileUtils.createTempFile(file.getParentFile(), FileUtils.getBasename(file.getPath()));

        try (BufferedReader reader = new BufferedReader(new FileReader(cloneFile), StreamUtils.BUFFER_SIZE);
             BufferedWriter writer = new BufferedWriter(new FileWriter(tempFile), StreamUtils.BUFFER_SIZE)) {

            int length;
            for (int i = 0, end = valueLocation.start - 1; i < end; i += length) {
                length = Math.min(buffer.length, end - i);
                //noinspection ResultOfMethodCallIgnored
                reader.read(buffer, 0, length);
                writer.write(buffer, 0, length);
            }
            //noinspection ResultOfMethodCallIgnored
            reader.skip(valueLocation.length() + 1);
            while ((length = reader.read(buffer)) != -1) writer.write(buffer, 0, length);
            success = true;
        }
        catch (IOException e) {}

        if (success) {
            modified = true;
            //noinspection ResultOfMethodCallIgnored
            tempFile.renameTo(cloneFile);
        }
        else tempFile.delete();
    }

    public void removeKey(String key) {
        Location keyLocation = getKeyLocation(key);
        if (keyLocation == null) return;

        char[] buffer = new char[StreamUtils.BUFFER_SIZE];
        boolean success = false;
        File tempFile = FileUtils.createTempFile(file.getParentFile(), FileUtils.getBasename(file.getPath()));

        try (BufferedReader reader = new BufferedReader(new FileReader(cloneFile), StreamUtils.BUFFER_SIZE);
             BufferedWriter writer = new BufferedWriter(new FileWriter(tempFile), StreamUtils.BUFFER_SIZE)) {

            int length;
            for (int i = 0, end = keyLocation.start - 1; i < end; i += length) {
                length = Math.min(buffer.length, end - i);
                //noinspection ResultOfMethodCallIgnored
                reader.read(buffer, 0, length);
                writer.write(buffer, 0, length);
            }

            //noinspection ResultOfMethodCallIgnored
            reader.skip(keyLocation.length() + 1);
            while ((length = reader.read(buffer)) != -1) writer.write(buffer, 0, length);
            success = true;
        }
        catch (IOException e) {}

        if (success) {
            modified = true;
            //noinspection ResultOfMethodCallIgnored
            tempFile.renameTo(cloneFile);
        }
        else tempFile.delete();
    }

    private void setRawValues(String key, String[][] items) {
        Arrays.sort(items, Comparator.comparing(a -> a[0].toLowerCase(Locale.ENGLISH)));
        Location keyLocation = getKeyLocation(key);
        if (keyLocation == null) {
            if (createKeyIfNotExist) {
                keyLocation = createKey(key);
            }
            else return;
        }

        char[] buffer = new char[StreamUtils.BUFFER_SIZE];
        boolean success = false;
        File tempFile = FileUtils.createTempFile(file.getParentFile(), FileUtils.getBasename(file.getPath()));

        try (BufferedReader reader = new BufferedReader(new FileReader(cloneFile), StreamUtils.BUFFER_SIZE);
             BufferedWriter writer = new BufferedWriter(new FileWriter(tempFile), StreamUtils.BUFFER_SIZE)) {

            int length;
            for (int i = 0, end = keyLocation.end; i < end; i += length) {
                length = Math.min(buffer.length, end - i);
                //noinspection ResultOfMethodCallIgnored
                reader.read(buffer, 0, length);
                writer.write(buffer, 0, length);
            }

            for (String[] item : items) {
                String name = item[0];
                String value = item[1];
                Location valueLocation = getValueLocation(keyLocation, name);
                if (valueLocation == null) {
                    writer.write("\n\"" + escape(name) + "\"=" + value);
                }
                else {
                    writer.write("\n\"" + escape(name) + "\"=" + value);
                    //noinspection ResultOfMethodCallIgnored
                    reader.skip(valueLocation.length() + 1);
                }
            }

            while ((length = reader.read(buffer)) != -1) writer.write(buffer, 0, length);
            success = true;
        }
        catch (IOException e) {}

        if (success) {
            modified = true;
            //noinspection ResultOfMethodCallIgnored
            tempFile.renameTo(cloneFile);
        }
        else tempFile.delete();
    }

    private Location getKeyLocation(String key) {
        return getKeyLocation(key, null);
    }

    private Location getParentKeyLocation(String key) {
        if (!key.contains("\\")) return null;
        return getKeyLocation(key.substring(0, key.lastIndexOf("\\")), null);
    }

    private Location getKeyLocation(String key, Location parentLocation) {
        Location[] locations = findKeyLocations(key, parentLocation);
        return locations != null && locations.length > 0 ? locations[0] : null;
    }

    private Location getValueLocation(Location keyLocation, String name) {
        if (keyLocation == null) return null;
        Location[] locations = findValueLocations(keyLocation, name);
        return locations != null && locations.length > 0 ? locations[0] : null;
    }

    private Location[] findKeyLocations(String key, Location parentLocation) {
        ArrayList<Location> locations = new ArrayList<>();
        String escapedKey = "[" + escape(key) + "]";
        int offset = 0;
        boolean success = false;

        try (BufferedReader reader = new BufferedReader(new FileReader(cloneFile), StreamUtils.BUFFER_SIZE)) {
            if (parentLocation != null) {
                //noinspection ResultOfMethodCallIgnored
                reader.skip(parentLocation.start);
                offset = parentLocation.start;
            }

            String line;
            int start = -1;
            int end = -1;
            while ((line = reader.readLine()) != null) {
                int length = line.length() + 1;
                if (line.startsWith("[")) {
                    if (start != -1 && end == -1) {
                        end = offset - 1;
                        locations.add(new Location(start, start, end));
                        start = -1;
                        end = -1;
                    }

                    if (line.equals(escapedKey)) {
                        start = offset;
                    }
                }
                offset += length;
            }

            if (start != -1 && end == -1) {
                end = offset;
                locations.add(new Location(start, start, end));
            }
            success = true;
        }
        catch (IOException e) {}

        return success ? locations.toArray(new Location[0]) : null;
    }

    private Location[] findValueLocations(Location keyLocation, String name) {
        ArrayList<Location> locations = new ArrayList<>();
        int offset = 0;
        boolean success = false;
        String escapedName = "\"" + escape(name) + "\"=";

        try (BufferedReader reader = new BufferedReader(new FileReader(cloneFile), StreamUtils.BUFFER_SIZE)) {
            //noinspection ResultOfMethodCallIgnored
            reader.skip(keyLocation.start);
            offset = keyLocation.start;

            String line;
            while ((line = reader.readLine()) != null) {
                int length = line.length() + 1;
                if (offset >= keyLocation.end) break;
                if (line.startsWith(escapedName)) {
                    int start = offset + escapedName.length();
                    int end = offset + line.length();
                    locations.add(new Location(offset, start, end));
                }
                offset += length;
            }
            success = true;
        }
        catch (IOException e) {}

        return success ? locations.toArray(new Location[0]) : null;
    }
}

