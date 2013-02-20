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

#define LOG_TAG "Terminal"

#include <utils/Log.h>
#include "jni.h"
#include "JNIHelp.h"

#include <vterm.h>

#include <string.h>

namespace android {

class Terminal;

/*
 * VTerm event handlers
 */

static int term_damage(VTermRect rect, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_damage");
    return 1;
}

static int term_prescroll(VTermRect rect, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_prescroll");
    return 1;
}

static int term_moverect(VTermRect dest, VTermRect src, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_moverect");
    return 1;
}

static int term_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_movecursor");
    return 1;
}

static int term_settermprop(VTermProp prop, VTermValue *val, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_settermprop");
    return 1;
}

static int term_setmousefunc(VTermMouseFunc func, void *data, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_setmousefunc");
    return 1;
}

static int term_bell(void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_bell");
    return 1;
}

static int term_resize(int rows, int cols, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_resize");
    return 1;
}

static VTermScreenCallbacks cb = {
    .damage = term_damage,
    .prescroll = term_prescroll,
    .moverect = term_moverect,
    .movecursor = term_movecursor,
    .settermprop = term_settermprop,
    .setmousefunc = term_setmousefunc,
    .bell = term_bell,
    .resize = term_resize,
};

/*
 * Terminal session
 */

class Terminal {
public:
    Terminal(int rows, int cols);
    ~Terminal();

    int getRows();

private:
    VTerm *mVt;
    VTermScreen *mVts;

    int mRows;
    int mCols;
};

Terminal::Terminal(int rows, int cols) : mRows(rows), mCols(cols) {
//    pt->writefn = NULL;
//    pt->resizedfn = NULL;

    /* Create VTerm */
    mVt = vterm_new(rows, cols);
    vterm_parser_set_utf8(mVt, 1);

    /* Set up screen */
    mVts = vterm_obtain_screen(mVt);
    vterm_screen_enable_altscreen(mVts, 1);
    vterm_screen_set_callbacks(mVts, &cb, this);
    vterm_screen_set_damage_merge(mVts, VTERM_DAMAGE_SCROLL);

    // TODO: finish setup and forkpty() here
}

Terminal::~Terminal() {
    vterm_free(mVt);
}

int Terminal::getRows() {
    return mRows;
}

/*
 * JNI glue
 */

static jint com_android_terminal_Terminal_nativeInit(JNIEnv* env, jclass clazz,
        jint rows, jint cols) {
    return reinterpret_cast<jint>(new Terminal(rows, cols));
}

static jint com_android_terminal_Terminal_nativeGetRows(JNIEnv* env, jclass clazz, jint ptr) {
    Terminal* term = reinterpret_cast<Terminal*>(ptr);
    return term->getRows();
}

static JNINativeMethod gMethods[] = {
    { "nativeInit", "(II)I", (void*)com_android_terminal_Terminal_nativeInit },
    { "nativeGetRows", "(I)I", (void*)com_android_terminal_Terminal_nativeGetRows },
};

int register_com_android_terminal_Terminal(JNIEnv* env) {
    return jniRegisterNativeMethods(env, "com/android/terminal/Terminal",
            gMethods, NELEM(gMethods));
}

} /* namespace android */
