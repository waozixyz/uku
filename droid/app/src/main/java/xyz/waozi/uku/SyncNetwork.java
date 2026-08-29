package xyz.waozi.uku;

import android.util.Log;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

final class SyncNetwork {
    private SyncNetwork() {
    }

    static String httpRequest(String logTag, String method, String urlText, String body,
                              String[] headers) {
        HttpURLConnection connection = null;
        int status = 0;

        try {
            byte[] bodyBytes = body != null ? body.getBytes(StandardCharsets.UTF_8) : new byte[0];
            connection = (HttpURLConnection)new URL(urlText).openConnection();
            connection.setInstanceFollowRedirects(false);
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(30000);
            connection.setRequestMethod(method);
            connection.setRequestProperty("User-Agent", "uku-sync/1");
            if (headers != null) {
                for (String header : headers) {
                    if (header == null) continue;
                    int colon = header.indexOf(':');
                    if (colon <= 0) continue;
                    String key = header.substring(0, colon).trim();
                    String value = header.substring(colon + 1).trim();
                    if (!key.isEmpty()) {
                        connection.setRequestProperty(key, value);
                    }
                }
            }
            if (bodyBytes.length > 0) {
                connection.setDoOutput(true);
                connection.setFixedLengthStreamingMode(bodyBytes.length);
                try (OutputStream output = connection.getOutputStream()) {
                    output.write(bodyBytes);
                }
            }

            status = connection.getResponseCode();
            InputStream stream = status >= 400 ? connection.getErrorStream() : connection.getInputStream();
            String response = "";
            if (stream != null) {
                try (InputStream input = stream) {
                    response = new String(readAllBytesCompat(input), StandardCharsets.UTF_8);
                }
            }
            return status + "\n" + response;
        } catch (Exception e) {
            Log.e(logTag, "Sync HTTP request failed", e);
            return status + "\n" + (e.getMessage() != null ? e.getMessage() : "request failed");
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private static byte[] readAllBytesCompat(InputStream input) throws java.io.IOException {
        byte[] buffer = new byte[8192];
        int read;
        java.io.ByteArrayOutputStream output = new java.io.ByteArrayOutputStream();
        while ((read = input.read(buffer)) != -1) {
            output.write(buffer, 0, read);
        }
        return output.toByteArray();
    }
}
