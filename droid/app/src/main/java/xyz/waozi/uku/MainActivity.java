package xyz.waozi.uku;

import android.app.NativeActivity;
import android.os.Bundle;
import android.view.ViewGroup;

public final class MainActivity extends NativeActivity {
    static { System.loadLibrary("main"); }
    private TextInputBridge input;
    private native void nativeInput(int value);

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        input = new TextInputBridge(this, new TextInputBridge.Callbacks() {
            public void commitText(int codepoint) { nativeInput(codepoint); }
            public void backspace() { nativeInput(-259); }
            public void enter() { nativeInput(-257); }
        });
        addContentView(input.getView(), new ViewGroup.LayoutParams(1, 1));
    }

    public void setKeyboardVisible(boolean visible) {
        runOnUiThread(() -> {
            if (input != null) input.setVisible(visible);
        });
    }

    @Override
    protected void onDestroy() {
        if (input != null) input.setVisible(false);
        super.onDestroy();
    }
}
