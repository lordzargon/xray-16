package org.openxray;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import android.widget.Toast;

import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class OpenXRayActivity extends SDLActivity {
    private static final String TAG = "OpenXRayActivity";
    private static final int PERMISSION_REQUEST_CODE = 1001;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        requestStoragePermissions();
        ensureStorageDirectories();
        super.onCreate(savedInstanceState);
    }

    private void requestStoragePermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                try {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                    intent.addCategory("android.intent.category.DEFAULT");
                    intent.setData(Uri.parse(String.format("package:%s", getApplicationContext().getPackageName())));
                    startActivity(intent);
                } catch (Exception e) {
                    Intent intent = new Intent();
                    intent.setAction(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                    startActivity(intent);
                }
            }
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            List<String> permissionsNeeded = new ArrayList<>();
            if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                permissionsNeeded.add(Manifest.permission.READ_EXTERNAL_STORAGE);
            }
            if (checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                permissionsNeeded.add(Manifest.permission.WRITE_EXTERNAL_STORAGE);
            }
            if (!permissionsNeeded.isEmpty()) {
                requestPermissions(permissionsNeeded.toArray(new String[0]), PERMISSION_REQUEST_CODE);
            }
        }
    }

    private void ensureStorageDirectories() {
        try {
            File externalDir = getExternalFilesDir(null);
            if (externalDir != null && !externalDir.exists()) {
                externalDir.mkdirs();
            }
            File fallbackDir = new File("/sdcard/OpenXRay");
            if (!fallbackDir.exists()) {
                fallbackDir.mkdirs();
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to create storage directories", e);
        }
    }

    @Override
    protected String getMainSharedObject() {
        return getApplicationInfo().nativeLibraryDir + "/xr_3da.so";
    }

    @Override
    protected String getMainFunction() {
        return "SDL_main";
    }

    @Override
    protected String[] getArguments() {
        return new String[] {
            "-fsltx",
            "/sdcard/Android/data/org.openxray/files/fsgame.ltx",
            "-nogameintro"
        };
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {
            // C++ runtime and third-party shared dependencies
            "c++_shared",
            "SDL2",
            "openal",

            // Core subsystems
            "xrAPI",
            "xrCore",
            "xrOPCODE",
            "xrCDB",
            "xrMaterialSystem",
            "xrSound",

            // Scripting layer
            "xrLuaJIT",
            "xrLuabind",
            "xrScriptEngine",

            // Physics, Math & AI
            "xrODE",
            "xrPhysics",
            "xrAICore",

            // Network, UI & Particles
            "xrNetServer",
            "xrGameSpy",
            "xrUICore",
            "xrParticles",

            // Renderer & Gameplay
            "xrRender_GL",
            "xrGame",
            "xrEngine",

            // Main launcher
            "xr_3da"
        };
    }

    @Override
    public void loadLibraries() {
        String nativeDir = getApplicationInfo().nativeLibraryDir;
        for (String lib : getLibraries()) {
            File withLib = new File(nativeDir, "lib" + lib + ".so");
            File raw = new File(nativeDir, lib + ".so");

            try {
                if (withLib.exists()) {
                    Log.i(TAG, "Loading native library from path: " + withLib.getAbsolutePath());
                    System.load(withLib.getAbsolutePath());
                } else if (raw.exists()) {
                    Log.i(TAG, "Loading native library from path: " + raw.getAbsolutePath());
                    System.load(raw.getAbsolutePath());
                } else {
                    Log.i(TAG, "Loading native library via SDL loader: " + lib);
                    SDL.loadLibrary(lib, this);
                }
            } catch (UnsatisfiedLinkError e) {
                Log.e(TAG, "Failed to load library: " + lib, e);
                throw e;
            }
        }
    }
}
