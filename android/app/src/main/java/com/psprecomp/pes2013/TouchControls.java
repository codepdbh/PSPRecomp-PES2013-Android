package com.psprecomp.pes2013;
import android.content.Context;
import android.graphics.*;
import android.view.*;

final class TouchControls extends View {
    interface Listener { void update(int buttons, int x, int y); }
    private final Listener listener;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private float stickX, stickY;
    private int buttons;
    private static final String[] labels = {"△", "○", "×", "□", "↑", "→", "↓", "←", "L", "R", "SELECT", "START"};
    private static final int[] masks = {0x1000,0x2000,0x4000,0x8000,0x10,0x20,0x40,0x80,0x100,0x200,1,8};
    private final float[][] positions = new float[12][2];
    TouchControls(Context context, Listener listener) { super(context); this.listener = listener; setFocusable(true); }
    private void layoutControls() {
        float w = getWidth(), h = getHeight(), d = h * .13f;
        float[][] p = {{w*.86f,h*.73f-d},{w*.86f+d,h*.73f},{w*.86f,h*.73f+d},{w*.86f-d,h*.73f},
            {w*.31f,h*.73f-d},{w*.31f+d,h*.73f},{w*.31f,h*.73f+d},{w*.31f-d,h*.73f},
            {w*.11f,h*.16f},{w*.89f,h*.16f},{w*.45f,h*.88f},{w*.56f,h*.88f}};
        for (int i=0;i<p.length;i++) positions[i] = p[i];
    }
    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas); layoutControls();
        float r = getHeight()*.06f, ax=getWidth()*.12f, ay=getHeight()*.76f, ar=getHeight()*.12f;
        paint.setColor(0x5599BBDD); canvas.drawCircle(ax,ay,ar,paint);
        paint.setColor(0xAAE0E9F0); canvas.drawCircle(ax+stickX*ar,ay+stickY*ar,ar*.4f,paint);
        paint.setTextAlign(Paint.Align.CENTER);
        for (int i=0;i<positions.length;i++) {
            paint.setColor((buttons & masks[i])!=0 ? 0xBB68CAAB : 0x55778899);
            canvas.drawCircle(positions[i][0],positions[i][1],r,paint);
            paint.setColor(0xDDEEF4FA); paint.setTextSize(i>=10 ? r*.40f : r*.8f);
            canvas.drawText(labels[i],positions[i][0],positions[i][1]+r*.25f,paint);
        }
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        layoutControls(); buttons=0; stickX=stickY=0;
        int action=event.getActionMasked(), removed=event.getActionIndex();
        if (action != MotionEvent.ACTION_CANCEL && action != MotionEvent.ACTION_UP) {
            float r=getHeight()*.075f, ax=getWidth()*.12f, ay=getHeight()*.76f, ar=getHeight()*.12f;
            for (int p=0;p<event.getPointerCount();p++) {
                if (action == MotionEvent.ACTION_POINTER_UP && p==removed) continue;
                float x=event.getX(p), y=event.getY(p);
                if (x<getWidth()*.22f && y>getHeight()*.45f) {
                    float dx=(x-ax)/ar, dy=(y-ay)/ar, length=(float)Math.sqrt(dx*dx+dy*dy);
                    stickX=dx/Math.max(1,length); stickY=dy/Math.max(1,length);
                } else for (int i=0;i<positions.length;i++) {
                    float dx=x-positions[i][0],dy=y-positions[i][1];
                    if (dx*dx+dy*dy<r*r) buttons|=masks[i];
                }
            }
        }
        listener.update(buttons,Math.round(128+127*stickX),Math.round(128+127*stickY));
        invalidate(); return true;
    }
}
