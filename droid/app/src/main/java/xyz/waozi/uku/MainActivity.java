package xyz.waozi.uku;

import android.app.NativeActivity;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;
import android.widget.Toast;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;

public class MainActivity extends NativeActivity {
    private static final String TAG = "Uku";
    private static final int REQUEST_SCAN_QR = 4201;

    static {
        System.loadLibrary("main");
    }

    private TextInputBridge textInputBridge;

    private native void nativeSetInsets(int left, int top, int right, int bottom,
        int ime, int cutoutLeft, int cutoutTop, int cutoutRight, int cutoutBottom);
    private native void nativeSetDeviceDensity(float density);
    private native void nativeTextInputCommit(int codepoint);
    private native void nativeTextInputBackspace();
    private native void nativeTextInputEnter();
    private native void nativeQrScanResult(String text);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        applySystemBars();
        setupInsetsListener();
        setupTextInputBridge();
    }

    public int[] systemThemeColors() {
        boolean dark = (getResources().getConfiguration().uiMode
                & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
        int background = dark ? 0xFF141218 : 0xFFFFFBFE;
        int surface = dark ? 0xFF211F26 : 0xFFF7F2FA;
        int text = dark ? 0xFFE6E0E9 : 0xFF1D1B20;
        int accent = dark ? 0xFFD0BCFF : 0xFF6750A4;
        int control = dark ? 0xFFE6E0E9 : 0xFF1D1B20;
        int button = blend(accent, background, dark ? 65 : 80);
        int buttonHover = blend(accent, background, dark ? 45 : 60);

        return new int[] {
            dark ? 1 : 0,
            background,
            surface,
            text,
            accent,
            button,
            buttonHover,
            control,
            accent
        };
    }

    private void applySystemBars() {
        int[] colors = systemThemeColors();
        boolean dark = colors[0] != 0;
        getWindow().setStatusBarColor(colors[1]);
        getWindow().setNavigationBarColor(colors[1]);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            int flags = getWindow().getDecorView().getSystemUiVisibility();
            if (!dark) {
                flags |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    flags |= View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
                }
            } else {
                flags &= ~View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    flags &= ~View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
                }
            }
            getWindow().getDecorView().setSystemUiVisibility(flags);
        }
    }

    private static int blend(int from, int to, int percentTo) {
        int p = Math.max(0, Math.min(100, percentTo));
        int a = (((from >>> 24) & 0xff) * (100 - p) + ((to >>> 24) & 0xff) * p) / 100;
        int r = (((from >>> 16) & 0xff) * (100 - p) + ((to >>> 16) & 0xff) * p) / 100;
        int g = (((from >>> 8) & 0xff) * (100 - p) + ((to >>> 8) & 0xff) * p) / 100;
        int b = ((from & 0xff) * (100 - p) + (to & 0xff) * p) / 100;
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    private void setupTextInputBridge() {
        textInputBridge = new TextInputBridge(this, new TextInputBridge.Callbacks() {
            @Override
            public void commitText(int codepoint) {
                nativeTextInputCommit(codepoint);
            }

            @Override
            public void backspace() {
                nativeTextInputBackspace();
            }

            @Override
            public void enter() {
                nativeTextInputEnter();
            }
        });
        addContentView(textInputBridge.getView(), new ViewGroup.LayoutParams(1, 1));
    }

    public void setSoftKeyboardVisible(final boolean visible) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (textInputBridge != null) {
                    textInputBridge.setVisible(visible);
                }
            }
        });
    }

    public String syncHttpRequest(String method, String urlText, String body, String[] headers) {
        return SyncNetwork.httpRequest(TAG, method, urlText, body, headers);
    }

    public void shareText(final String text, final String chooserTitle) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Intent sendIntent = new Intent(Intent.ACTION_SEND);
                sendIntent.setType("text/plain");
                sendIntent.putExtra(Intent.EXTRA_TEXT, text != null ? text : "");
                startActivity(Intent.createChooser(sendIntent,
                        chooserTitle != null && !chooserTitle.isEmpty()
                                ? chooserTitle
                                : getString(R.string.share_chooser_title)));
            }
        });
    }

    public void shareFile(final String path, final String mimeType,
                          final String chooserTitle, final String extraText) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    File source = new File(path != null ? path : "");
                    if (!source.isFile()) {
                        Toast.makeText(MainActivity.this, R.string.qr_share_failed,
                                Toast.LENGTH_SHORT).show();
                        return;
                    }
                    File dir = ShareProvider.shareDir(MainActivity.this);
                    if (!dir.isDirectory() && !dir.mkdirs()) {
                        Toast.makeText(MainActivity.this, R.string.qr_share_failed,
                                Toast.LENGTH_SHORT).show();
                        return;
                    }
                    File cacheFile = new File(dir, source.getName());
                    copyFile(source, cacheFile);
                    Uri contentUri = ShareProvider.uriForFile(cacheFile);

                    Intent sendIntent = new Intent(Intent.ACTION_SEND);
                    sendIntent.setType(mimeType != null && !mimeType.isEmpty()
                            ? mimeType
                            : "application/octet-stream");
                    sendIntent.putExtra(Intent.EXTRA_STREAM, contentUri);
                    if (extraText != null && !extraText.isEmpty()) {
                        sendIntent.putExtra(Intent.EXTRA_TEXT, extraText);
                    }
                    sendIntent.setClipData(ClipData.newUri(getContentResolver(),
                            cacheFile.getName(), contentUri));
                    sendIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    startActivity(Intent.createChooser(sendIntent,
                            chooserTitle != null && !chooserTitle.isEmpty()
                                    ? chooserTitle
                                    : getString(R.string.share_chooser_title)));
                } catch (Exception e) {
                    Toast.makeText(MainActivity.this, R.string.qr_share_failed,
                            Toast.LENGTH_SHORT).show();
                }
            }
        });
    }

    public void scanQrCode() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent("com.google.zxing.client.android.SCAN");
                    intent.putExtra("SCAN_MODE", "QR_CODE_MODE");
                    startActivityForResult(intent, REQUEST_SCAN_QR);
                } catch (ActivityNotFoundException e) {
                    Toast.makeText(MainActivity.this, R.string.qr_scan_unavailable,
                            Toast.LENGTH_LONG).show();
                    nativeQrScanResult("");
                }
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_SCAN_QR) {
            String result = "";
            if (resultCode == RESULT_OK && data != null) {
                String scanned = data.getStringExtra("SCAN_RESULT");
                if (scanned != null) result = scanned;
            }
            nativeQrScanResult(result);
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    private static void copyFile(File source, File dest) throws java.io.IOException {
        try (FileInputStream input = new FileInputStream(source);
             FileOutputStream output = new FileOutputStream(dest)) {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = input.read(buffer)) != -1) {
                output.write(buffer, 0, read);
            }
        }
    }

    private void setupInsetsListener() {
        final View decorView = getWindow().getDecorView();

        nativeSetDeviceDensity(getResources().getDisplayMetrics().density);
        decorView.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                updateInsets(insets);
                return insets;
            }
        });

        decorView.getViewTreeObserver().addOnGlobalLayoutListener(
                new ViewTreeObserver.OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    WindowInsets insets = decorView.getRootWindowInsets();
                    if (insets != null) {
                        updateInsets(insets);
                    }
                }
            }
        });

        decorView.post(new Runnable() {
            @Override
            public void run() {
                decorView.requestApplyInsets();
            }
        });
    }

    private void updateInsets(WindowInsets insets) {
        if (insets == null) return;

        nativeSetDeviceDensity(getResources().getDisplayMetrics().density);

        int systemLeft = 0;
        int systemTop = 0;
        int systemRight = 0;
        int systemBottom = 0;
        int imeBottom = 0;
        int cLeft = 0, cTop = 0, cRight = 0, cBottom = 0;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Insets systemBars = insets.getInsetsIgnoringVisibility(
                    WindowInsets.Type.systemBars());
            Insets ime = insets.getInsets(WindowInsets.Type.ime());
            systemLeft = systemBars.left;
            systemTop = systemBars.top;
            systemRight = systemBars.right;
            systemBottom = systemBars.bottom;
            imeBottom = ime.bottom;
        } else {
            systemLeft = insets.getSystemWindowInsetLeft();
            systemTop = insets.getSystemWindowInsetTop();
            systemRight = insets.getSystemWindowInsetRight();
            systemBottom = insets.getSystemWindowInsetBottom();
            imeBottom = inferImeBottom(systemBottom);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            DisplayCutout cutout = insets.getDisplayCutout();
            if (cutout != null) {
                cLeft = cutout.getSafeInsetLeft();
                cTop = cutout.getSafeInsetTop();
                cRight = cutout.getSafeInsetRight();
                cBottom = cutout.getSafeInsetBottom();
            }
        }

        nativeSetInsets(systemLeft, systemTop, systemRight, systemBottom,
                imeBottom, cLeft, cTop, cRight, cBottom);
    }

    private int inferImeBottom(int navBar) {
        View decorView = getWindow().getDecorView();
        Rect visible = new Rect();
        decorView.getWindowVisibleDisplayFrame(visible);
        int rootHeight = decorView.getRootView().getHeight();
        int hiddenBottom = rootHeight - visible.bottom;

        if (hiddenBottom <= navBar) return 0;
        return hiddenBottom;
    }
}
