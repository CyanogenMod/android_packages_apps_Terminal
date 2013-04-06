LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_STATIC_JAVA_LIBRARIES := android-support-v4

# If this is an unbundled build (to install seprately) then include
# the libraries in the APK, otherwise just put them in /system/lib and
# leave them out of the APK
ifneq (,$(TARGET_BUILD_APPS))
    LOCAL_JNI_SHARED_LIBRARIES := libjni_terminal
else
    LOCAL_REQUIRED_MODULES := libjni_terminal
endif

# TODO: enable proguard once development has settled down
#LOCAL_PROGUARD_FLAG_FILES := proguard.flags
LOCAL_PROGUARD_ENABLED := disabled

LOCAL_PACKAGE_NAME := Terminal

include $(BUILD_PACKAGE)

include $(call all-makefiles-under,$(LOCAL_PATH))
