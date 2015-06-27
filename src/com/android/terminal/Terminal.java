/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.terminal;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.graphics.Color;
import android.util.Log;

import java.util.List;

/**
 * Single terminal session backed by a pseudo terminal on the local device.
 */
public class Terminal {
    public static final String TAG = "Terminal";

    public final int key;
    private StringBuilder mStringBuilder = new StringBuilder();
    private List<ResolveInfo> mLauncherApps;
    private Context mContext;

    private static int sNumber = 0;

    static {
        System.loadLibrary("jni_terminal");
    }

    /**
     * Represents a run of one or more {@code VTermScreenCell} which all have
     * the same formatting.
     */
    public static class CellRun {
        char[] data;
        int dataSize;
        int colSize;

        boolean bold;
        int underline;
        boolean blink;
        boolean reverse;
        boolean strike;
        int font;

        int fg = Color.CYAN;
        int bg = Color.DKGRAY;
    }

    // NOTE: clients must not call back into terminal while handling a callback,
    // since native mutex isn't reentrant.
    public interface TerminalClient {
        public void onDamage(int startRow, int endRow, int startCol, int endCol);
        public void onMoveRect(int destStartRow, int destEndRow, int destStartCol, int destEndCol,
                int srcStartRow, int srcEndRow, int srcStartCol, int srcEndCol);
        public void onMoveCursor(int posRow, int posCol, int oldPosRow, int oldPosCol, int visible);
        public void onBell();
    }

    private final long mNativePtr;
    private final Thread mThread;

    private String mTitle;

    private TerminalClient mClient;

    private boolean mCursorVisible;
    private int mCursorRow;
    private int mCursorCol;

    private final TerminalCallbacks mCallbacks = new TerminalCallbacks() {
        @Override
        public int damage(int startRow, int endRow, int startCol, int endCol) {
            if (mClient != null) {
                mClient.onDamage(startRow, endRow, startCol, endCol);
            }
            return 1;
        }

        @Override
        public int moveRect(int destStartRow, int destEndRow, int destStartCol, int destEndCol,
                int srcStartRow, int srcEndRow, int srcStartCol, int srcEndCol) {
            if (mClient != null) {
                mClient.onMoveRect(destStartRow, destEndRow, destStartCol, destEndCol, srcStartRow,
                        srcEndRow, srcStartCol, srcEndCol);
            }
            return 1;
        }

        @Override
        public int moveCursor(int posRow, int posCol, int oldPosRow, int oldPosCol, int visible) {
            mCursorVisible = (visible != 0);
            mCursorRow = posRow;
            mCursorCol = posCol;
            if (mClient != null) {
                mClient.onMoveCursor(posRow, posCol, oldPosRow, oldPosCol, visible);
            }
            return 1;
        }

        @Override
        public int bell() {
            if (mClient != null) {
                mClient.onBell();
            }
            return 1;
        }
    };

    public Terminal() {
        mNativePtr = nativeInit(mCallbacks);
        key = sNumber++;
        mTitle = TAG + " " + key;
        mThread = new Thread(mTitle) {
            @Override
            public void run() {
                nativeRun(mNativePtr);
            }
        };
    }

    public void setContext(Context context) {
        mContext = context;
    }

    public void setApps(List<ResolveInfo> resolveInfoList) {
        mLauncherApps = resolveInfoList;
    }

    /**
     * Start thread which internally forks and manages the pseudo terminal.
     */
    public void start() {
        mThread.start();
    }

    public void destroy() {
        if (nativeDestroy(mNativePtr) != 0) {
            throw new IllegalStateException("destroy failed");
        }
    }

    public void setClient(TerminalClient client) {
        mClient = client;
    }

    public void resize(int rows, int cols, int scrollRows) {
        if (nativeResize(mNativePtr, rows, cols, scrollRows) != 0) {
            throw new IllegalStateException("resize failed");
        }
    }

    public void setColors(int fg, int bg) {
        if (nativeSetColors(mNativePtr, fg, bg) != 0) {
            throw new IllegalStateException("setColors failed");
        }
    }

    public int getRows() {
        return nativeGetRows(mNativePtr);
    }

    public int getCols() {
        return nativeGetCols(mNativePtr);
    }

    public int getScrollRows() {
        return nativeGetScrollRows(mNativePtr);
    }

    public void getCellRun(int row, int col, CellRun run) {
        if (nativeGetCellRun(mNativePtr, row, col, run) != 0) {
            throw new IllegalStateException("getCell failed");
        }
    }

    public boolean getCursorVisible() {
        return mCursorVisible;
    }

    public int getCursorRow() {
        return mCursorRow;
    }

    public int getCursorCol() {
        return mCursorCol;
    }

    public String getTitle() {
        // TODO: hook up to title passed through termprop
        return mTitle;
    }

    public boolean dispatchKey(int modifiers, int key) {
        boolean appLaunched = false;
        if (key == TerminalKeys.VTERM_KEY_ENTER) {
            appLaunched = launchAppIfEntered();
        }

        if (appLaunched) {
            return true;
        } else {
            return nativeDispatchKey(mNativePtr, modifiers, key);
        }
    }

    public boolean launchAppIfEntered() {
        String command = mStringBuilder.toString();
        mStringBuilder = new StringBuilder();
        if (command.startsWith("launch")) {
            launchApp(command);
            return true;
        }
        return false;
    }

    public void launchApp(String command) {
        String name = command.substring(6).trim();
        for (ResolveInfo info : mLauncherApps) {
            if (info.activityInfo.name.contains(name) ||
                info.activityInfo.packageName.contains(name)) {
                ComponentName componentName = new ComponentName(info.activityInfo.packageName,
                                                       info.activityInfo.name);
                Intent i=new Intent(Intent.ACTION_MAIN);

                i.addCategory(Intent.CATEGORY_LAUNCHER);
                i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK |
                           Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
                i.setComponent(componentName);

                mContext.startActivity(i);
            }
        }
        Log.d("TEST", "app name: " + name);
    }

    public boolean dispatchCharacter(int modifiers, int character) {
        mStringBuilder.append(Character.toChars(character));
        return nativeDispatchCharacter(mNativePtr, modifiers, character);
    }

    private static native long nativeInit(TerminalCallbacks callbacks);
    private static native int nativeDestroy(long ptr);

    private static native int nativeRun(long ptr);
    private static native int nativeResize(long ptr, int rows, int cols, int scrollRows);
    private static native int nativeSetColors(long ptr, int fg, int bg);
    private static native int nativeGetCellRun(long ptr, int row, int col, CellRun run);
    private static native int nativeGetRows(long ptr);
    private static native int nativeGetCols(long ptr);
    private static native int nativeGetScrollRows(long ptr);

    private static native boolean nativeDispatchKey(long ptr, int modifiers, int key);
    private static native boolean nativeDispatchCharacter(long ptr, int modifiers, int character);
}
