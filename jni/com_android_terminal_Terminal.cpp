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
#include <ScopedLocalRef.h>

#include <termios.h>
#include <util.h>
//#include <pty.h>
#include <utmp.h>
#include <unistd.h>
//#include <stdlib.h>

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
 * Terminal session
 */

class Terminal {
public:
    Terminal(jobject callbacks, int rows, int cols);
    ~Terminal();

    size_t write(const char *bytes, size_t len);
    void resize(short unsigned int rows, short unsigned int cols);

    int getRows() const;

    jobject getCallbacks() const;

private:
    int mMasterFd;
    VTerm *mVt;
    VTermScreen *mVts;

    short unsigned int mRows;
    short unsigned int mCols;

    jobject mCallbacks;
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

    /* None of the docs about termios explain how to construct a new one of
     * these, so this is largely a guess */
    struct termios termios = {
        .c_iflag = ICRNL|IXON|IUTF8,
        .c_oflag = OPOST|ONLCR|NL0|CR0|TAB0|BS0|VT0|FF0,
        .c_cflag = CS8|CREAD,
        .c_lflag = ISIG|ICANON|IEXTEN|ECHO|ECHOE|ECHOK,
        /* c_cc later */
    };

//    cfsetspeed(&termios, 38400);
//
//    termios.c_cc[VINTR]    = 0x1f & 'C';
//    termios.c_cc[VQUIT]    = 0x1f & '\\';
//    termios.c_cc[VERASE]   = 0x7f;
//    termios.c_cc[VKILL]    = 0x1f & 'U';
//    termios.c_cc[VEOF]     = 0x1f & 'D';
//    termios.c_cc[VEOL]     = _POSIX_VDISABLE;
//    termios.c_cc[VEOL2]    = _POSIX_VDISABLE;
//    termios.c_cc[VSTART]   = 0x1f & 'Q';
//    termios.c_cc[VSTOP]    = 0x1f & 'S';
//    termios.c_cc[VSUSP]    = 0x1f & 'Z';
//    termios.c_cc[VREPRINT] = 0x1f & 'R';
//    termios.c_cc[VWERASE]  = 0x1f & 'W';
//    termios.c_cc[VLNEXT]   = 0x1f & 'V';
//    termios.c_cc[VMIN]     = 1;
//    termios.c_cc[VTIME]    = 0;
//
//    struct winsize size = { mRows, mCols, 0, 0 };
//
//    pid_t kid = forkpty(&mMasterFd, NULL, &termios, &size);
//    if(kid == 0) {
//        /* Restore the ISIG signals back to defaults */
//        signal(SIGINT, SIG_DFL);
//        signal(SIGQUIT, SIG_DFL);
//        signal(SIGSTOP, SIG_DFL);
//        signal(SIGCONT, SIG_DFL);
//
//        gchar *term = g_strdup_printf("TERM=%s", CONF_term);
//        putenv(term);
//        /* Do not free 'term', it is part of the environment */
//
//        char *shell = getenv("SHELL");
//        char *args[2] = {shell, NULL};
//        execvp(shell, args);
//        fprintf(stderr_save, "Cannot exec(%s) - %s\n", shell, strerror(errno));
//        _exit(1);
//    }
//
//    fcntl(mMasterFd, F_SETFL, fcntl(mMasterFd, F_GETFL) | O_NONBLOCK);
}

Terminal::~Terminal() {
    close(mMasterFd);
    vterm_free(mVt);
}

size_t Terminal::write(const char *bytes, size_t len) {
    return ::write(mMasterFd, bytes, len);
}

void Terminal::resize(short unsigned int rows, short unsigned int cols) {
    mRows = rows;
    mCols = cols;
    struct winsize size = { rows, cols, 0, 0 };
    ioctl(mMasterFd, TIOCSWINSZ, &size);
}

int Terminal::getRows() const {
    return mRows;
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

static void com_android_terminal_Terminal_nativeWrite(JNIEnv* env,
        jclass clazz, jint ptr) {
    Terminal* term = reinterpret_cast<Terminal*>(ptr);
    // const char *bytes, size_t len
    term->write(NULL, 0);
}

static void com_android_terminal_Terminal_nativeResize(JNIEnv* env,
        jclass clazz, jint ptr, jint rows, jint cols) {
    Terminal* term = reinterpret_cast<Terminal*>(ptr);
    term->resize(rows, cols);
}

static jint com_android_terminal_Terminal_nativeGetRows(JNIEnv* env, jclass clazz, jint ptr) {
    Terminal* term = reinterpret_cast<Terminal*>(ptr);
    return term->getRows();
}

static JNINativeMethod gMethods[] = {
    { "nativeInit", "(Lcom/android/terminal/TerminalCallbacks;II)I", (void*)com_android_terminal_Terminal_nativeInit },
    { "nativeResize", "(III)", (void*)com_android_terminal_Terminal_nativeResize },
    { "nativeGetRows", "(I)I", (void*)com_android_terminal_Terminal_nativeGetRows },
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

    env->GetJavaVM(&gJavaVM);

    return jniRegisterNativeMethods(env, "com/android/terminal/Terminal",
            gMethods, NELEM(gMethods));
}

} /* namespace android */
