#include <jni.h>
#include <dlfcn.h>
#include <stdlib.h>

#define CAST_JNI_FN(fn) Java_com_via_libnice_##fn
#define CAST_JNI_IN(fn, arg...) CAST_JNI_FN(fn) (JNIEnv* env, jobject obj, arg)
#define CAST_JNI(fn, arg...) CAST_JNI_IN(fn, arg)

#define JNI_TAG "libnice-jni"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, JNI_TAG, __VA_ARGS__)

JNIEXPORT jint JNICALL CAST_JNI(libniceInit,jint input)
{

	return 0;
}