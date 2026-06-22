package com.example.kyrospades;

import org.libsdl.app.SDLActivity;

import android.view.View;

public class KyroSpadesActivity extends SDLActivity {
    // SDL2 is statically linked into libmain.so — don't try to load libSDL2.so separately
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }

    /* Called from native (window.c) via JNI when the in-game Fullscreen toggle
       changes. Hides only the top status bar and leaves the bottom navigation
       bar visible, unlike SDL's built-in fullscreen which hides both and leaves
       the player with no back button to exit the menu. Must run on the UI
       thread: system-UI changes from the SDL thread are illegal. */
    public static void setStatusBarHidden(final boolean hidden) {
        final SDLActivity activity = mSingleton;
        if (activity == null)
            return;
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                int flags = hidden
                    ? (View.SYSTEM_UI_FLAG_FULLSCREEN
                       | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                       | View.SYSTEM_UI_FLAG_LAYOUT_STABLE)
                    : (View.SYSTEM_UI_FLAG_VISIBLE
                       | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
                activity.getWindow().getDecorView().setSystemUiVisibility(flags);
            }
        });
    }
}
