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

public abstract class TerminalCallbacks {
    public int damage(int start_row, int end_row, int start_col, int end_col) {
        return 1;
    }

    public int prescroll(int start_row, int end_row, int start_col, int end_col) {
        return 1;
    }

    public int moveRect(int dest_start_row, int dest_end_row, int dest_start_col, int dest_end_col,
            int src_start_row, int src_end_row, int src_start_col, int src_end_col) {
        return 1;
    }

    public int moveCursor(int dest_start_row, int dest_end_row, int dest_start_col,
            int dest_end_col, int src_start_row, int src_end_row, int src_start_col,
            int src_end_col, int visible) {
        return 1;
    }

    public int setTermPropBoolean(int prop, boolean value) {
        return 1;
    }

    public int setTermPropInt(int prop, int value) {
        return 1;
    }

    public int setTermPropString(int prop, String value) {
        return 1;
    }

    public int setTermPropColor(int prop, int red, int green, int blue) {
        return 1;
    }

    public int bell() {
        return 1;
    }

    public int resize(int rows, int cols) {
        return 1;
    }
}
