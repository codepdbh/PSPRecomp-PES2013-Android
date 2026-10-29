package com.psprecomp.pes2013;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.view.*;
import android.widget.*;
import android.graphics.Color;
import java.io.File;

public final class MainActivity extends Activity implements SurfaceHolder.Callback {
    static { System.loadLibrary("pes2013_android"); }
    private static native String nativeStatus();
    private static native void nativeSurface(Surface surface);
    private static native void nativeStart(String root, String settings);
    private static native void nativeStop();
    private static native void nativeInput(int buttons, int x, int y);
    private final Handler handler = new Handler();
    private SurfaceView screen;
    private TextView status;
    private boolean foreground;
    private int touchButtons, padButtons, touchX = 128, touchY = 128, padX = 128, padY = 128;
    private final Runnable refresh = new Runnable() {
        @Override public void run() {
            if (!foreground) return;
            String value = nativeStatus();
            status.setText(value);
            status.setVisibility(value.equals("Imagen recibida.") ? View.GONE : View.VISIBLE);
            handler.postDelayed(this, 500);
        }
    };
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        FrameLayout layout = new FrameLayout(this);
        screen = new SurfaceView(this);
        screen.getHolder().addCallback(this);
        layout.addView(screen, new FrameLayout.LayoutParams(-1, -1));
        TouchControls controls = new TouchControls(this, (buttons, x, y) -> {
            touchButtons = buttons; touchX = x; touchY = y; sendInput();
        });
        layout.addView(controls, new FrameLayout.LayoutParams(-1, -1));
        status = new TextView(this);
        status.setTextColor(Color.WHITE); status.setBackgroundColor(0xB0000000);
        status.setPadding(18, 10, 18, 10); status.setTextSize(14);
        FrameLayout.LayoutParams message = new FrameLayout.LayoutParams(-1, -2, Gravity.TOP);
        layout.addView(status, message);
        setContentView(layout);
    }
    private void start() {
        if (!foreground || !screen.getHolder().getSurface().isValid()) return;
        File external = getExternalFilesDir(null);
        if (external == null) { status.setText("No se pudo acceder al almacenamiento."); return; }
        File root = new File(external, "game"); root.mkdirs();
        nativeStart(root.getAbsolutePath(), getFilesDir().getAbsolutePath());
    }
    @Override protected void onResume() {
        super.onResume(); foreground = true;
        getWindow().getDecorView().setSystemUiVisibility(5894);
        handler.post(refresh); start();
    }
    @Override protected void onPause() {
        foreground = false; handler.removeCallbacks(refresh);
        touchButtons = padButtons = 0; touchX = touchY = padX = padY = 128; sendInput();
        nativeStop(); super.onPause();
    }
    @Override public void surfaceCreated(SurfaceHolder holder) { nativeSurface(holder.getSurface()); start(); }
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) { nativeSurface(holder.getSurface()); }
    @Override public void surfaceDestroyed(SurfaceHolder holder) { nativeStop(); nativeSurface(null); }
    private void sendInput() {
        int x = Math.abs(padX - 128) > Math.abs(touchX - 128) ? padX : touchX;
        int y = Math.abs(padY - 128) > Math.abs(touchY - 128) ? padY : touchY;
        nativeInput(touchButtons | padButtons, x, y);
    }
    private int button(int key) {
        switch (key) {
            case KeyEvent.KEYCODE_BUTTON_A: return 0x4000;
            case KeyEvent.KEYCODE_BUTTON_B: return 0x2000;
            case KeyEvent.KEYCODE_BUTTON_X: return 0x8000;
            case KeyEvent.KEYCODE_BUTTON_Y: return 0x1000;
            case KeyEvent.KEYCODE_DPAD_UP: return 0x10;
            case KeyEvent.KEYCODE_DPAD_RIGHT: return 0x20;
            case KeyEvent.KEYCODE_DPAD_DOWN: return 0x40;
            case KeyEvent.KEYCODE_DPAD_LEFT: return 0x80;
            case KeyEvent.KEYCODE_BUTTON_L1: return 0x100;
            case KeyEvent.KEYCODE_BUTTON_R1: return 0x200;
            case KeyEvent.KEYCODE_BUTTON_START: return 8;
            case KeyEvent.KEYCODE_BUTTON_SELECT: return 1;
            default: return 0;
        }
    }
    @Override public boolean onKeyDown(int key, KeyEvent event) {
        int mask = button(key); if (mask == 0) return super.onKeyDown(key, event);
        padButtons |= mask; sendInput(); return true;
    }
    @Override public boolean onKeyUp(int key, KeyEvent event) {
        int mask = button(key); if (mask == 0) return super.onKeyUp(key, event);
        padButtons &= ~mask; sendInput(); return true;
    }
    @Override public boolean onGenericMotionEvent(MotionEvent event) {
        if ((event.getSource() & InputDevice.SOURCE_JOYSTICK) != 0 && event.getAction() == MotionEvent.ACTION_MOVE) {
            padX = Math.max(0, Math.min(255, Math.round(128 + event.getAxisValue(MotionEvent.AXIS_X) * 127)));
            padY = Math.max(0, Math.min(255, Math.round(128 + event.getAxisValue(MotionEvent.AXIS_Y) * 127)));
            sendInput(); return true;
        }
        return super.onGenericMotionEvent(event);
    }
}
