// JNI and Android assets only. All application UI and behavior are Ziran.
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern struct android_app *GetAndroidApp(void);
static pthread_mutex_t input_lock = PTHREAD_MUTEX_INITIALIZER;
static int input[256];
static unsigned input_read;
static unsigned input_write;
static int keyboard_visible;

const char *uku_data_directory(void)
{
    struct android_app *app = GetAndroidApp();
    return app && app->activity ? app->activity->internalDataPath : NULL;
}

void uku_initialize(void)
{
    struct android_app *app = GetAndroidApp();
    const char *directory = uku_data_directory();
    if (!app || !directory || chdir(directory) != 0)
        return;
    AAsset *asset = AAssetManager_open(app->activity->assetManager,
        "LiberationSans-Regular.ttf", AASSET_MODE_STREAMING);
    if (!asset)
        return;
    FILE *file = fopen("LiberationSans-Regular.ttf", "wb");
    if (file) {
        char bytes[8192];
        int length;
        while ((length = AAsset_read(asset, bytes, sizeof(bytes))) > 0) {
            if (fwrite(bytes, 1, (size_t)length, file) != (size_t)length)
                break;
        }
        fclose(file);
        setenv("KRYON_FONT_PATH", "LiberationSans-Regular.ttf", 1);
    }
    AAsset_close(asset);
}

static int take_input(int keys)
{
    int result = 0;
    pthread_mutex_lock(&input_lock);
    if (input_read != input_write) {
        int value = input[input_read];
        if ((value < 0) == keys) {
            result = keys ? -value : value;
            input_read = (input_read + 1) % 256;
        }
    }
    pthread_mutex_unlock(&input_lock);
    return result;
}

int uku_typed_codepoint(void) { return take_input(0); }
int uku_pressed_key(void) { return take_input(1); }

JNIEXPORT void JNICALL
Java_xyz_waozi_uku_MainActivity_nativeInput(JNIEnv *env, jobject activity, jint value)
{
    (void)env;
    (void)activity;
    pthread_mutex_lock(&input_lock);
    unsigned next = (input_write + 1) % 256;
    if (next != input_read) {
        input[input_write] = value;
        input_write = next;
    }
    pthread_mutex_unlock(&input_lock);
}

void uku_keyboard(int visible)
{
    if (visible == keyboard_visible)
        return;
    struct android_app *app = GetAndroidApp();
    if (!app || !app->activity)
        return;
    JavaVM *vm = app->activity->vm;
    JNIEnv *env = NULL;
    int attached = (*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK;
    if (attached && (*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK)
        return;
    jobject activity = app->activity->clazz;
    jclass type = (*env)->GetObjectClass(env, activity);
    jmethodID method = type ? (*env)->GetMethodID(env, type, "setKeyboardVisible", "(Z)V") : NULL;
    if (method) {
        (*env)->CallVoidMethod(env, activity, method, visible ? JNI_TRUE : JNI_FALSE);
        if (!(*env)->ExceptionCheck(env))
            keyboard_visible = visible;
    }
    if ((*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);
    if (type)
        (*env)->DeleteLocalRef(env, type);
    if (attached)
        (*vm)->DetachCurrentThread(vm);
}
