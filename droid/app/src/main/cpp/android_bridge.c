#include "android_bridge.h"

#if ANDROID_BUILD

#include "kryon.h"
#include "theme.h"
#include "ui_dpi.h"

#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

extern struct android_app *GetAndroidApp(void);

#define LOG_TAG "UKU_JNI"

static pthread_mutex_t bridge_mutex = PTHREAD_MUTEX_INITIALIZER;
static float device_density = 0.0f;
static int insets_system_bottom = 0;
static int insets_ime_bottom = 0;
static int insets_cutout_bottom = 0;
static char qr_scan_result[512];
static int qr_scan_pending = 0;

static int
activity_env(JNIEnv **env_out, JavaVM **jvm_out, jobject *activity_out)
{
    struct android_app *app = GetAndroidApp();
    JavaVM *jvm;
    JNIEnv *env = NULL;
    int attached = 0;

    if(env_out == NULL || jvm_out == NULL || activity_out == NULL ||
       app == NULL || app->activity == NULL || app->activity->vm == NULL ||
       app->activity->clazz == NULL)
        return -1;

    jvm = app->activity->vm;
    if((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK || env == NULL)
            return -1;
        attached = 1;
    }
    *env_out = env;
    *jvm_out = jvm;
    *activity_out = app->activity->clazz;
    return attached;
}

static void
activity_env_done(JNIEnv *env, JavaVM *jvm, int attached)
{
    if(env != NULL && (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
    if(attached && jvm != NULL)
        (*jvm)->DetachCurrentThread(jvm);
}

void
android_bridge_init(void)
{
    pthread_mutex_lock(&bridge_mutex);
    device_density = 0.0f;
    insets_system_bottom = 0;
    insets_ime_bottom = 0;
    insets_cutout_bottom = 0;
    qr_scan_result[0] = '\0';
    qr_scan_pending = 0;
    pthread_mutex_unlock(&bridge_mutex);
}

static Color
color_from_argb(jint argb)
{
    Color color;

    color.a = (unsigned char)((argb >> 24) & 0xff);
    color.r = (unsigned char)((argb >> 16) & 0xff);
    color.g = (unsigned char)((argb >> 8) & 0xff);
    color.b = (unsigned char)(argb & 0xff);
    if(color.a == 0)
        color.a = 0xff;
    return color;
}

void
android_bridge_apply_system_theme(void)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jintArray array;
    jint values[9];
    jsize len;
    int attached;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return;

    memset(values, 0, sizeof(values));
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "systemThemeColors", "()[I");
    if(method == NULL) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "systemThemeColors not found");
        goto done;
    }
    array = (jintArray)(*env)->CallObjectMethod(env, activity, method);
    if(array == NULL)
        goto done;
    len = (*env)->GetArrayLength(env, array);
    if(len < 9)
        goto done;
    (*env)->GetIntArrayRegion(env, array, 0, 9, values);
    SetSystemThemePalette("Android",
                          color_from_argb(values[1]),
                          color_from_argb(values[2]),
                          color_from_argb(values[3]),
                          color_from_argb(values[4]),
                          color_from_argb(values[5]),
                          color_from_argb(values[6]),
                          color_from_argb(values[7]),
                          color_from_argb(values[8]),
                          values[0] != 0,
                          1);

done:
    activity_env_done(env, jvm, attached);
}

void
android_bridge_set_soft_keyboard(int visible)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    int attached;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return;

    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "setSoftKeyboardVisible", "(Z)V");
    if(method == NULL) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "setSoftKeyboardVisible not found");
        goto done;
    }
    (*env)->CallVoidMethod(env, activity, method, visible ? JNI_TRUE : JNI_FALSE);

done:
    activity_env_done(env, jvm, attached);
}

static int
android_call_void_string2(const char *method_name, const char *signature,
                          const char *a, const char *b)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring ja = NULL;
    jstring jb = NULL;
    int attached;
    int ok = 0;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return 0;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, method_name, signature);
    if(method == NULL)
        goto done;
    ja = (*env)->NewStringUTF(env, a != NULL ? a : "");
    jb = (*env)->NewStringUTF(env, b != NULL ? b : "");
    if(ja == NULL || jb == NULL)
        goto done;
    (*env)->CallVoidMethod(env, activity, method, ja, jb);
    ok = !(*env)->ExceptionCheck(env);

done:
    if(jb != NULL)
        (*env)->DeleteLocalRef(env, jb);
    if(ja != NULL)
        (*env)->DeleteLocalRef(env, ja);
    activity_env_done(env, jvm, attached);
    return ok;
}

int
android_bridge_share_text(const char *text, const char *title)
{
    return android_call_void_string2("shareText",
                                     "(Ljava/lang/String;Ljava/lang/String;)V",
                                     text, title);
}

int
android_bridge_share_file(const char *path, const char *mime_type,
                          const char *title, const char *extra_text)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring jpath = NULL;
    jstring jmime = NULL;
    jstring jtitle = NULL;
    jstring jextra = NULL;
    int attached;
    int ok = 0;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return 0;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "shareFile",
                                 "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if(method == NULL)
        goto done;
    jpath = (*env)->NewStringUTF(env, path != NULL ? path : "");
    jmime = (*env)->NewStringUTF(env, mime_type != NULL ? mime_type : "application/octet-stream");
    jtitle = (*env)->NewStringUTF(env, title != NULL ? title : "");
    jextra = (*env)->NewStringUTF(env, extra_text != NULL ? extra_text : "");
    if(jpath == NULL || jmime == NULL || jtitle == NULL || jextra == NULL)
        goto done;
    (*env)->CallVoidMethod(env, activity, method, jpath, jmime, jtitle, jextra);
    ok = !(*env)->ExceptionCheck(env);

done:
    if(jextra != NULL)
        (*env)->DeleteLocalRef(env, jextra);
    if(jtitle != NULL)
        (*env)->DeleteLocalRef(env, jtitle);
    if(jmime != NULL)
        (*env)->DeleteLocalRef(env, jmime);
    if(jpath != NULL)
        (*env)->DeleteLocalRef(env, jpath);
    activity_env_done(env, jvm, attached);
    return ok;
}

int
android_bridge_scan_qr(void)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    int attached;
    int ok = 0;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return 0;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "scanQrCode", "()V");
    if(method == NULL)
        goto done;
    (*env)->CallVoidMethod(env, activity, method);
    ok = !(*env)->ExceptionCheck(env);

done:
    activity_env_done(env, jvm, attached);
    return ok;
}

int
android_bridge_take_qr_scan_result(char *out, size_t out_size)
{
    int pending;

    if(out == NULL || out_size == 0)
        return 0;
    pthread_mutex_lock(&bridge_mutex);
    pending = qr_scan_pending;
    if(pending) {
        snprintf(out, out_size, "%s", qr_scan_result);
        qr_scan_pending = 0;
        qr_scan_result[0] = '\0';
    }
    pthread_mutex_unlock(&bridge_mutex);
    return pending;
}

int
android_bridge_bottom_reserved(void)
{
    int bottom;

    pthread_mutex_lock(&bridge_mutex);
    bottom = insets_system_bottom;
    if(insets_ime_bottom > bottom)
        bottom = insets_ime_bottom;
    if(insets_cutout_bottom > bottom)
        bottom = insets_cutout_bottom;
    pthread_mutex_unlock(&bridge_mutex);
    return bottom > 0 ? bottom : 0;
}

JNIEXPORT void JNICALL
Java_xyz_waozi_uku_MainActivity_nativeSetInsets(JNIEnv *env, jobject thiz,
                                                jint system_left, jint system_top,
                                                jint system_right, jint system_bottom,
                                                jint ime_bottom,
                                                jint cutout_left, jint cutout_top,
                                                jint cutout_right, jint cutout_bottom)
{
    (void)env;
    (void)thiz;
    (void)system_left;
    (void)system_top;
    (void)system_right;
    (void)cutout_left;
    (void)cutout_top;
    (void)cutout_right;

    pthread_mutex_lock(&bridge_mutex);
    insets_system_bottom = system_bottom;
    insets_ime_bottom = ime_bottom;
    insets_cutout_bottom = cutout_bottom;
    pthread_mutex_unlock(&bridge_mutex);
}

JNIEXPORT void JNICALL
Java_xyz_waozi_uku_MainActivity_nativeSetDeviceDensity(JNIEnv *env, jobject thiz, jfloat density)
{
    (void)env;
    (void)thiz;

    if(density <= 0.0f)
        return;
    pthread_mutex_lock(&bridge_mutex);
    device_density = density;
    pthread_mutex_unlock(&bridge_mutex);
    SetUIDeviceDensity(density * 1.12f);
}

JNIEXPORT void JNICALL
Java_xyz_waozi_uku_MainActivity_nativeTextInputCommit(JNIEnv *env, jobject thiz, jint codepoint)
{
    (void)env;
    (void)thiz;
    QueueTextInputCodepoint((int)codepoint);
}

JNIEXPORT void JNICALL
Java_xyz_waozi_uku_MainActivity_nativeTextInputBackspace(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;
    QueueTextInputBackspace();
}

JNIEXPORT void JNICALL
Java_xyz_waozi_uku_MainActivity_nativeTextInputEnter(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;
    QueueTextInputEnter();
}

JNIEXPORT void JNICALL
Java_xyz_waozi_uku_MainActivity_nativeQrScanResult(JNIEnv *env, jobject thiz, jstring text)
{
    const char *value = NULL;

    (void)thiz;
    if(text != NULL)
        value = (*env)->GetStringUTFChars(env, text, NULL);
    pthread_mutex_lock(&bridge_mutex);
    snprintf(qr_scan_result, sizeof(qr_scan_result), "%s", value != NULL ? value : "");
    qr_scan_pending = 1;
    pthread_mutex_unlock(&bridge_mutex);
    if(value != NULL)
        (*env)->ReleaseStringUTFChars(env, text, value);
}

#else

void android_bridge_init(void) {}
void android_bridge_apply_system_theme(void) {}
void android_bridge_set_soft_keyboard(int visible) { (void)visible; }
int android_bridge_bottom_reserved(void) { return 0; }
int android_bridge_share_text(const char *text, const char *title) { (void)text; (void)title; return 0; }
int android_bridge_share_file(const char *path, const char *mime_type,
                              const char *title, const char *extra_text)
{
    (void)path;
    (void)mime_type;
    (void)title;
    (void)extra_text;
    return 0;
}
int android_bridge_scan_qr(void) { return 0; }
int android_bridge_take_qr_scan_result(char *out, size_t out_size)
{
    if(out != NULL && out_size > 0)
        out[0] = '\0';
    return 0;
}

#endif
