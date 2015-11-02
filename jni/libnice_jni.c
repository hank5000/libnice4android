#include <jni.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <agent.h>
#include <android/log.h>

// has input CAST
#define CAST_JNI_FN(fn) Java_com_via_libnice_##fn
#define CAST_JNI_IN(fn, arg...) CAST_JNI_FN(fn) (JNIEnv* env, jobject obj, arg)
#define CAST_JNI(fn, arg...) CAST_JNI_IN(fn, arg)

// has no input CAST
#define CAST_JNI_NON(fn) CAST_JNI_FN(fn) (JNIEnv* env, jobject obj)


#define JNI_TAG "libnice-jni"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, JNI_TAG, __VA_ARGS__)

#if GLIB_CHECK_VERSION(2, 36, 0)
#include <gio/gnetworking.h>
#endif

static JavaVM *gJavaVM;
JNIEnv* niceJavaEnv = 0;
jobject niceJavaObj= 0;
jclass niceClazz;
jmethodID cbJavaMsgId;
jmethodID cbJavaIntId;
jmethodID cbJavaMsgStaticId;


NiceAgent *agent;
guint stream_id;


int stunPort = 3478;


static GMainLoop *gloop;
static gchar *stun_addr = NULL;
static guint stun_port;
static gboolean controlling;
static gboolean exit_thread, candidate_gathering_done, negotiation_done;
static GMutex gather_mutex, negotiate_mutex;
static GCond gather_cond, negotiate_cond;



static const gchar *state_name[] = {"disconnected", "gathering", "connecting",
                                    "connected", "ready", "failed"};

static void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
    gpointer data);
static void cb_component_state_changed(NiceAgent *agent, guint stream_id,
    guint component_id, guint state,
    gpointer data);
static void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
    guint len, gchar *buf, gpointer data);

static void * example_thread(void *data);

void cbIntToJava(int i);
void cbMsgToJava(char* c);
void cvMsgToJavaStatic(char* c);


void cbIntToJava(int i)
{
  jint tmp = i;
  (*niceJavaEnv)->CallVoidMethod(niceJavaEnv,niceJavaObj,cbJavaIntId,tmp);
}

void cbMsgToJava(char* c)
{
  jstring tmp = (*niceJavaEnv)->NewStringUTF(niceJavaEnv,c);
  (*niceJavaEnv)->CallVoidMethod(niceJavaEnv,niceJavaObj,cbJavaMsgId,tmp);
  (*niceJavaEnv)->DeleteLocalRef(niceJavaEnv,tmp);
}

void cbMsgToJavaStatic(char* c)
{
  jstring tmp = (*niceJavaEnv)->NewStringUTF(niceJavaEnv,c);
  (*niceJavaEnv)->CallStaticVoidMethod(niceJavaEnv,niceJavaObj,cbJavaMsgStaticId,tmp);
  (*niceJavaEnv)->DeleteLocalRef(niceJavaEnv,tmp);
}

JNIEnv* getEnv()
{
    JNIEnv* env;
    (*gJavaVM)->GetEnv(gJavaVM, (void**)&env, JNI_VERSION_1_6);
    return env;
}

JNIEXPORT jint JNICALL CAST_JNI_NON(Init)
{
    // Libnice init
    g_networking_init();
    exit_thread = FALSE;

    gloop = g_main_loop_new(NULL, FALSE);

    // JNI init
    (*env)->GetJavaVM(env,&gJavaVM);
    niceJavaEnv = env;
    niceJavaObj = (*env)->NewGlobalRef(env,obj);
    niceClazz = (*env)->FindClass(env, "com/via/libnice");

    // Init Callback function method id
    cbJavaMsgId = (*env)->GetMethodID(env,niceClazz,"jniCallBackMsg","(Ljava/lang/String;)V");
    //cbJavaMsgStaticId = (*env)->GetStaticMethodID(env,niceClazz,"jniCallBackMsgStatic","(Ljava/lang/String;)V");
    cbJavaIntId = (*env)->GetMethodID(env,niceClazz,"jniCallBackInt","(I)V");

	return 1;
}

JNIEXPORT jint JNICALL CAST_JNI_NON(mainLoopStart)
{
  g_main_loop_run(gloop);
}

JNIEXPORT jint JNICALL CAST_JNI(sendData,jstring data)
{
  const gchar *line = (gchar*) (*env)->GetStringUTFChars(env, data, 0);
  nice_agent_send(agent, stream_id, 1, strlen(line), line);
}

JNIEXPORT jstring JNICALL CAST_JNI(createNiceAgentAndGetSdp,jstring jstun_ip, jint jstun_port)
{
    jstring ret;

    gchar *sdp, *sdp64;
    const gchar *stun_ip = (gchar*) (*env)->GetStringUTFChars(env, jstun_ip, 0);
    int stun_port = jstun_port;

    // Create the nice agent
    agent = nice_agent_new(g_main_loop_get_context (gloop), NICE_COMPATIBILITY_RFC5245);
    if (agent == NULL) 
    {
        LOGD("Failed to create agent");
        return 0;
    }

    LOGD("createNiceAgent access, Get Stun address %s:%d",stun_ip,stun_port);
    // Set the STUN settings and controlling mode
    if(stun_ip) {
        g_object_set(agent, "stun-server", stun_ip, NULL);
        g_object_set(agent, "stun-server-port", stun_port, NULL);
    }

    // TODO: using 0 first, check it how to use.
    controlling = 0;
    g_object_set(agent, "controlling-mode", controlling, NULL);
    
    // Connect to the signals
    g_signal_connect(agent, "candidate-gathering-done",
      G_CALLBACK(cb_candidate_gathering_done), NULL);
    g_signal_connect(agent, "component-state-changed",
      G_CALLBACK(cb_component_state_changed), NULL);

    // Create a new stream with one component
    stream_id = nice_agent_add_stream(agent, 1);
    if (stream_id == 0)
        LOGD("Failed to add stream");
    nice_agent_set_stream_name (agent, stream_id, "HankWuStream");

    // Attach to the component to receive the data
    // Without this call, candidates cannot be gathered
    nice_agent_attach_recv(agent, stream_id, 1,
        g_main_loop_get_context (gloop), cb_nice_recv, NULL);

    // Start gathering local candidates
    if (!nice_agent_gather_candidates(agent, stream_id))
        LOGD("Failed to start candidate gathering");

    LOGD("waiting for candidate-gathering-done signal...");

    g_mutex_lock(&gather_mutex);
    while (!exit_thread && !candidate_gathering_done)
        g_cond_wait(&gather_cond, &gather_mutex);
    g_mutex_unlock(&gather_mutex);
    if (exit_thread) {
        LOGD("something wrong, exit_thread = 1");
        return 0;
    }

    // Candidate gathering is done. Send our local candidates on stdout
    sdp = nice_agent_generate_local_sdp (agent);
    LOGD("Generated SDP from agent :\n%s\n\n", sdp);
    LOGD("Copy the following line to remote client:\n");
    sdp64 = g_base64_encode ((const guchar *)sdp, strlen (sdp));

    ret = (*env)->NewStringUTF(env,sdp64);
    (*env)->CallVoidMethod(env,obj,cbJavaMsgId,ret);
    

    LOGD("\n %s\n",sdp64);

    g_free (sdp);
    g_free (sdp64);

    return ret;
}

JNIEXPORT jint JNICALL CAST_JNI(setRemoteSdp,jstring jremoteSdp,jlong size)
{
  gchar* sdp_decode;
  const char *remoteSdp = (*env)->GetStringUTFChars(env, jremoteSdp, 0);
  gsize sdp_len = size;
  sdp_decode = (gchar *) g_base64_decode (remoteSdp, &sdp_len);
  if (sdp_decode && nice_agent_parse_remote_sdp (agent, sdp_decode) > 0) {
    g_free (sdp_decode);
    LOGD("waiting for state READY or FAILED signal...");
    g_mutex_lock(&negotiate_mutex);
    while (!negotiation_done)
      g_cond_wait(&negotiate_cond, &negotiate_mutex);
    g_mutex_unlock(&negotiate_mutex);
  } else {
    //cbMsgToJava("please retry it");
  }
   // use your string
  (*env)->ReleaseStringUTFChars(env, jremoteSdp, remoteSdp);

}


static void
cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
    gpointer data)
{
  LOGD("SIGNAL candidate gathering done\n");

  g_mutex_lock(&gather_mutex);
  candidate_gathering_done = TRUE;
  g_cond_signal(&gather_cond);
  g_mutex_unlock(&gather_mutex);
}

static void
cb_component_state_changed(NiceAgent *agent, guint stream_id,
    guint component_id, guint state,
    gpointer data)
{
  LOGD("SIGNAL: state changed %d %d %s[%d]\n",
      stream_id, component_id, state_name[state], state);

  if (state == NICE_COMPONENT_STATE_READY) {
    g_mutex_lock(&negotiate_mutex);
    negotiation_done = TRUE;
    g_cond_signal(&negotiate_cond);
    g_mutex_unlock(&negotiate_mutex);
  } else if (state == NICE_COMPONENT_STATE_FAILED) {
    g_main_loop_quit (gloop);
  }
}


jobject callback_obj;
jmethodID callback_mid;

void
cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
    guint len, gchar *buf, gpointer data)
{
  if (len == 1 && buf[0] == '\0')
    g_main_loop_quit (gloop);

  LOGD("Recv data : %.*s", len, buf);
  LOGD("Recv data size : %d",len);

  JNIEnv *env;
  jclass cls;
  jmethodID mid;


  #if 0
  // using static 

  if((*gJavaVM)->AttachCurrentThread(gJavaVM, &env, NULL) != JNI_OK)
  {
      LOGD("%s: AttachCurrentThread() failed", __FUNCTION__);

  } 
  else
  {
    jbyteArray arr = (*env)->NewByteArray(env,len);
    (*env)->SetByteArrayRegion(env,arr,0,len, (jbyte*)buf);

    //jstring tmp = (*env)->NewStringUTF(env,buf);
    cls = (*env)->GetObjectClass(env,niceJavaObj);
    if(cls == NULL)
    {
      LOGD("FindClass() Error.....");
      //goto error; 
    }
    else
    {
      mid = (*env)->GetStaticMethodID(env, cls, "jniCallBackMsgStatic","([B)V");
      if (mid == NULL) 
      {
        LOGD("GetMethodID() Error.....");
        //goto error;  
      }
      else
      {
        (*env)->CallStaticVoidMethod(env,cls,mid,arr);

      }
    }
  }
  #else
  // using interface which is registered
    if((*gJavaVM)->AttachCurrentThread(gJavaVM, &env, NULL) != JNI_OK)
    {
      LOGD("%s: AttachCurrentThread() failed", __FUNCTION__);

    } 
    else
    {
        jbyteArray arr = (*env)->NewByteArray(env,len);
        (*env)->SetByteArrayRegion(env,arr,0,len, (jbyte*)buf);
        (*env)->CallVoidMethod(env,callback_obj,callback_mid,arr);
    }


  #endif
}


JNIEXPORT void CAST_JNI(registerObserverNative,jobject cb_obj) {
    //bool returnValue = true;
    callback_obj = (*env)->NewGlobalRef(env,cb_obj);
    jclass clz = (*env)->GetObjectClass(env,callback_obj);
    if(clz == NULL) {
        LOGD("Failed to find class\n");
        //return false;
    }
    callback_mid = (*env)->GetMethodID(env,clz,"obCallback","([B)V");
}


