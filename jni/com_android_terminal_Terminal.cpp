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
#include "forkpty.h"
#include "jni.h"
#include "JNIHelp.h"
#include "ScopedLocalRef.h"
#include "ScopedPrimitiveArray.h"

#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>
#include <utmp.h>

#include <vterm.h>

#include <string.h>

namespace android {

/*
 * JavaVM reference
 */
static JavaVM* gJavaVM;

/*
 * Callback class reference
 */
static jclass terminalCallbacksClass;

/*
 * Callback methods
 */
static jmethodID damageMethod;
static jmethodID prescrollMethod;
static jmethodID moveRectMethod;
static jmethodID moveCursorMethod;
static jmethodID setTermPropBooleanMethod;
static jmethodID setTermPropIntMethod;
static jmethodID setTermPropStringMethod;
static jmethodID setTermPropColorMethod;
static jmethodID bellMethod;
static jmethodID resizeMethod;

/*
 * CellRun class
 */
static jclass cellRunClass;
static jfieldID cellRunDataField;
static jfieldID cellRunDataSizeField;
static jfieldID cellRunColSizeField;
/*
 * Terminal session
 */
class Terminal {
public:
    Terminal(jobject callbacks, int rows, int cols);
    ~Terminal();

    int run();

    size_t write(const char *bytes, size_t len);

    int resize(short unsigned int rows, short unsigned int cols);
    int getCell(VTermPos pos, VTermScreenCell* cell);

    int getRows() const;
    int getCols() const;

    jobject getCallbacks() const;

private:
    int mMasterFd;
    VTerm *mVt;
    VTermScreen *mVts;

    jobject mCallbacks;
    short unsigned int mRows;
    short unsigned int mCols;
};

static JNIEnv* getEnv() {
    JNIEnv* env;

    if (gJavaVM->AttachCurrentThread(&env, NULL) < 0) {
        return NULL;
    }

    return env;
}

/*
 * VTerm event handlers
 */

static int term_damage(VTermRect rect, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_damage");

    JNIEnv* env = getEnv();
    if (env == NULL) {
        ALOGE("term_damage: couldn't get JNIEnv");
        return 0;
    }

    return env->CallIntMethod(term->getCallbacks(), damageMethod, rect.start_row, rect.end_row,
            rect.start_col, rect.end_col);
}

static int term_prescroll(VTermRect rect, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_prescroll");

    JNIEnv* env = getEnv();
    if (env == NULL) {
        ALOGE("term_prescroll: couldn't get JNIEnv");
        return 0;
    }

    return env->CallIntMethod(term->getCallbacks(), prescrollMethod);
}

static int term_moverect(VTermRect dest, VTermRect src, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_moverect");

    JNIEnv* env = getEnv();
    if (env == NULL) {
        ALOGE("term_moverect: couldn't get JNIEnv");
        return 0;
    }

    return env->CallIntMethod(term->getCallbacks(), moveRectMethod);
}

static int term_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_movecursor");

    JNIEnv* env = getEnv();
    if (env == NULL) {
        ALOGE("term_movecursor: couldn't get JNIEnv");
        return 0;
    }

    return env->CallIntMethod(term->getCallbacks(), moveCursorMethod);
}

static int term_settermprop(VTermProp prop, VTermValue *val, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_settermprop");

    JNIEnv* env = getEnv();
    if (env == NULL) {
        ALOGE("term_settermprop: couldn't get JNIEnv");
        return 0;
    }

    switch (vterm_get_prop_type(prop)) {
    case VTERM_VALUETYPE_BOOL:
        return env->CallIntMethod(term->getCallbacks(), setTermPropBooleanMethod,
                static_cast<jboolean>(val->boolean));
    case VTERM_VALUETYPE_INT:
        return env->CallIntMethod(term->getCallbacks(), setTermPropIntMethod, prop, val->number);
    case VTERM_VALUETYPE_STRING:
        return env->CallIntMethod(term->getCallbacks(), setTermPropStringMethod, prop,
                env->NewStringUTF(val->string));
    case VTERM_VALUETYPE_COLOR:
        return env->CallIntMethod(term->getCallbacks(), setTermPropIntMethod, prop, val->color.red,
                val->color.green, val->color.blue);
    default:
        ALOGE("unknown callback type");
        return 0;
    }
}

static int term_setmousefunc(VTermMouseFunc func, void *data, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_setmousefunc");
    return 1;
}

static int term_bell(void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_bell");

    JNIEnv* env = getEnv();
    if (env == NULL) {
        ALOGE("term_bell: couldn't get JNIEnv");
        return 0;
    }

    return env->CallIntMethod(term->getCallbacks(), bellMethod);
}

static int term_resize(int rows, int cols, void *user) {
    Terminal* term = reinterpret_cast<Terminal*>(user);
    ALOGW("term_resize");

    JNIEnv* env = getEnv();
    if (env == NULL) {
        ALOGE("term_bell: couldn't get JNIEnv");
        return 0;
    }

    return env->CallIntMethod(term->getCallbacks(), resizeMethod);
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

Terminal::Terminal(jobject callbacks, int rows, int cols) :
        mCallbacks(callbacks), mRows(rows), mCols(cols) {
    /* Create VTerm */
    mVt = vterm_new(rows, cols);
    vterm_parser_set_utf8(mVt, 1);

    /* Set up screen */
    mVts = vterm_obtain_screen(mVt);
    vterm_screen_enable_altscreen(mVts, 1);
    vterm_screen_set_callbacks(mVts, &cb, this);
    vterm_screen_set_damage_merge(mVts, VTERM_DAMAGE_SCROLL);
    vterm_screen_reset(mVts, 1);
}

Terminal::~Terminal() {
    close(mMasterFd);
    vterm_free(mVt);
}

int Terminal::run() {
    /* None of the docs about termios explain how to construct a new one of
     * these, so this is largely a guess */
    struct termios termios = {
        .c_iflag = ICRNL|IXON|IUTF8,
        .c_oflag = OPOST|ONLCR|NL0|CR0|TAB0|BS0|VT0|FF0,
        .c_cflag = CS8|CREAD,
        .c_lflag = ISIG|ICANON|IEXTEN|ECHO|ECHOE|ECHOK,
        /* c_cc later */
    };

    cfsetispeed(&termios, B38400);
    cfsetospeed(&termios, B38400);

    termios.c_cc[VINTR]    = 0x1f & 'C';
    termios.c_cc[VQUIT]    = 0x1f & '\\';
    termios.c_cc[VERASE]   = 0x7f;
    termios.c_cc[VKILL]    = 0x1f & 'U';
    termios.c_cc[VEOF]     = 0x1f & 'D';
    termios.c_cc[VSTART]   = 0x1f & 'Q';
    termios.c_cc[VSTOP]    = 0x1f & 'S';
    termios.c_cc[VSUSP]    = 0x1f & 'Z';
    termios.c_cc[VREPRINT] = 0x1f & 'R';
    termios.c_cc[VWERASE]  = 0x1f & 'W';
    termios.c_cc[VLNEXT]   = 0x1f & 'V';
    termios.c_cc[VMIN]     = 1;
    termios.c_cc[VTIME]    = 0;

    struct winsize size = { mRows, mCols, 0, 0 };

    int stderr_save_fd = dup(2);
    if (stderr_save_fd < 0) {
        ALOGE("failed to dup stderr - %s", strerror(errno));
    }

    pid_t kid = forkpty(&mMasterFd, NULL, &termios, &size);
    if (kid == 0) {
        /* Restore the ISIG signals back to defaults */
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGSTOP, SIG_DFL);
        signal(SIGCONT, SIG_DFL);

        FILE *stderr_save = fdopen(stderr_save_fd, "a");

        if (!stderr_save) {
            ALOGE("failed to open stderr - %s", strerror(errno));
        }

        char *shell = "/system/bin/sh"; //getenv("SHELL");
        char *args[2] = {shell, NULL};
        execvp(shell, args);
        fprintf(stderr_save, "Cannot exec(%s) - %s\n", shell, strerror(errno));
        _exit(1);
    }

    while (1) {
        char buffer[4096];
        ssize_t bytes = ::read(mMasterFd, buffer, sizeof buffer);
        ALOGD("Read %d bytes:", bytes);

        if (bytes == 0) {
            ALOGD("read() found EOF");
            break;
        }
        if (bytes == -1) {
            ALOGE("read() failed: %s", strerror(errno));
            return 1;
        }

        vterm_push_bytes(mVt, buffer, bytes);
    }

    return 1;
}

size_t Terminal::write(const char *bytes, size_t len) {
    return ::write(mMasterFd, bytes, len);
}

int Terminal::resize(short unsigned int rows, short unsigned int cols) {
    ALOGD("resize(%d, %d)", rows, cols);
    // TODO: wait for resize event to propegate back from shell?
    mRows = rows;
    mCols = cols;
    struct winsize size = { rows, cols, 0, 0 };
    ioctl(mMasterFd, TIOCSWINSZ, &size);
    // TODO: vterm_set_size?
    return 0;
}

int Terminal::getCell(VTermPos pos, VTermScreenCell* cell) {
    return vterm_screen_get_cell(mVts, pos, cell);
}

int Terminal::getRows() const {
    return mRows;
}

int Terminal::getCols() const {
    return mCols;
}

jobject Terminal::getCallbacks() const {
    return mCallbacks;
}

/*
 * JNI glue
 */

static jint com_android_terminal_Terminal_nativeInit(JNIEnv* env, jclass clazz, jobject callbacks,
        jint rows, jint cols) {
    return reinterpret_cast<jint>(new Terminal(env->NewGlobalRef(callbacks), rows, cols));
}

static jint com_android_terminal_Terminal_nativeRun(JNIEnv* env, jclass clazz, jint ptr) {
    Terminal* term = reinterpret_cast<Terminal*>(ptr);
    return term->run();
}

static jint com_android_terminal_Terminal_nativeResize(JNIEnv* env,
        jclass clazz, jint ptr, jint rows, jint cols) {
    Terminal* term = reinterpret_cast<Terminal*>(ptr);
    return term->resize(rows, cols);
}

static jint com_android_terminal_Terminal_nativeGetCellRun(JNIEnv* env,
        jclass clazz, jint ptr, jint row, jint col, jobject run) {
    Terminal* term = reinterpret_cast<Terminal*>(ptr);

    jcharArray dataArray = (jcharArray) env->GetObjectField(run, cellRunDataField);
    ScopedCharArrayRW data(env, dataArray);
    if (data.get() == NULL) {
        return -1;
    }

    VTermScreenCell cell;
    memset(&cell, 0, sizeof(VTermScreenCell));

    VTermPos pos = {
        .row = row,
        .col = col,
    };

    unsigned int dataSize = 0;
    unsigned int colSize = 0;
    while (pos.col < term->getCols()) {
        int res = term->getCell(pos, &cell);

        // TODO: terminate this loop once text style changes

        // TODO: remove this once terminal is resized
        if (cell.width == 0) {
            cell.width = 1;
        }

        // TODO: support full UTF-32 characters
        // for testing, 0x00020000 should become 0xD840 0xDC00
        unsigned int size = 1;

        // Only include cell chars if they fit into run
        if (dataSize + size <= data.size()) {
            data[dataSize] = cell.chars[0];
            dataSize += size;
            colSize += cell.width;
            pos.col += cell.width;
        } else {
            break;
        }
    }

    env->SetIntField(run, cellRunDataSizeField, dataSize);
    env->SetIntField(run, cellRunColSizeField, colSize);

    return 0;
}

static jint com_android_terminal_Terminal_nativeGetRows(JNIEnv* env, jclass clazz, jint ptr) {
    Terminal* term = reinterpret_cast<Terminal*>(ptr);
    return term->getRows();
}

static jint com_android_terminal_Terminal_nativeGetCols(JNIEnv* env, jclass clazz, jint ptr) {
    Terminal* term = reinterpret_cast<Terminal*>(ptr);
    return term->getCols();
}

static JNINativeMethod gMethods[] = {
    { "nativeInit", "(Lcom/android/terminal/TerminalCallbacks;II)I", (void*)com_android_terminal_Terminal_nativeInit },
    { "nativeRun", "(I)I", (void*)com_android_terminal_Terminal_nativeRun },
    { "nativeResize", "(III)I", (void*)com_android_terminal_Terminal_nativeResize },
    { "nativeGetCellRun", "(IIILcom/android/terminal/Terminal$CellRun;)I", (void*)com_android_terminal_Terminal_nativeGetCellRun },
    { "nativeGetRows", "(I)I", (void*)com_android_terminal_Terminal_nativeGetRows },
    { "nativeGetCols", "(I)I", (void*)com_android_terminal_Terminal_nativeGetCols },
};

int register_com_android_terminal_Terminal(JNIEnv* env) {
    ScopedLocalRef<jclass> localClass(env,
            env->FindClass("com/android/terminal/TerminalCallbacks"));

    android::terminalCallbacksClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass.get()));

    android::damageMethod = env->GetMethodID(terminalCallbacksClass, "damage", "(IIII)I");
    android::prescrollMethod = env->GetMethodID(terminalCallbacksClass, "prescroll", "(IIII)I");
    android::moveRectMethod = env->GetMethodID(terminalCallbacksClass, "moveRect", "(IIIIIIII)I");
    android::moveCursorMethod = env->GetMethodID(terminalCallbacksClass, "moveCursor",
            "(IIIIIIIII)I");
    android::setTermPropBooleanMethod = env->GetMethodID(terminalCallbacksClass,
            "setTermPropBoolean", "(IZ)I");
    android::setTermPropIntMethod = env->GetMethodID(terminalCallbacksClass, "setTermPropInt",
            "(II)I");
    android::setTermPropStringMethod = env->GetMethodID(terminalCallbacksClass, "setTermPropString",
            "(ILjava/lang/String;)I");
    android::setTermPropColorMethod = env->GetMethodID(terminalCallbacksClass, "setTermPropColor",
            "(IIII)I");
    android::bellMethod = env->GetMethodID(terminalCallbacksClass, "bell", "()I");
    android::resizeMethod = env->GetMethodID(terminalCallbacksClass, "resize", "(II)I");

    ScopedLocalRef<jclass> cellRunLocal(env,
            env->FindClass("com/android/terminal/Terminal$CellRun"));
    cellRunClass = reinterpret_cast<jclass>(env->NewGlobalRef(cellRunLocal.get()));
    cellRunDataField = env->GetFieldID(cellRunClass, "data", "[C");
    cellRunDataSizeField = env->GetFieldID(cellRunClass, "dataSize", "I");
    cellRunColSizeField = env->GetFieldID(cellRunClass, "colSize", "I");

    env->GetJavaVM(&gJavaVM);

    return jniRegisterNativeMethods(env, "com/android/terminal/Terminal",
            gMethods, NELEM(gMethods));
}

} /* namespace android */
