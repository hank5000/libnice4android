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


#define MAX_COMPONENT 32
static JavaVM *gJavaVM;
JNIEnv* niceJavaEnv = 0;
jobject niceJavaObj= 0;
jclass niceClazz;
jmethodID cbJavaMsgId;
jmethodID cbJavaIntId;
jmethodID cbJavaMsgStaticId;
typedef struct CallbackObserverCtx {
  int used;
  jobject   jObj;
  jmethodID jmid;
  //struct cbObserverCtx* next;
}ObCtx;

ObCtx cbObserverCtx[MAX_COMPONENT];
int currentCtxIdx = 0;

// state observer interface
int     hasStateObserver = 0;
jobject stateObserverObj;
jmethodID cbCandidateGatheringDoneId;
jmethodID cbComponentStateChangedId;



NiceAgent *agent = NULL;
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

static int totalComponentNumber = 0;

void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
    gpointer data);
void cb_component_state_changed(NiceAgent *agent, guint stream_id,
    guint component_id, guint state,
    gpointer data);

void cb_nice_recv1(NiceAgent *agent, guint stream_id, guint component_id,
    guint len, gchar *buf, gpointer data);

void cb_new_selected_pair(NiceAgent *agent, guint _stream_id,
    guint component_id, gchar *lfoundation,
    gchar *rfoundation, gpointer data);

typedef struct _RecvInfo {
  jobject   jobj;
  jmethodID jmid;
  int stream_id;
  int component_id;
} RecvInfo;

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


JNIEXPORT jint JNICALL CAST_JNI_NON(initNative)
{
  hasStateObserver = 0;
  // Libnice init
  g_networking_init();
  exit_thread = FALSE;

  gloop = g_main_loop_new(NULL, FALSE);

  // JNI init
  (*env)->GetJavaVM(env,&gJavaVM);
  niceJavaEnv = env;
  niceJavaObj = (*env)->NewGlobalRef(env,obj);
  niceClazz = (*env)->FindClass(env, "com/via/libnice");

  for(int i=0;i<MAX_COMPONENT;i++) {
    cbObserverCtx[i].used = 0;
    cbObserverCtx[i].jObj = 0;
    cbObserverCtx[i].jmid = 0;
  }

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

JNIEXPORT jint JNICALL CAST_JNI_NON(mainLoopStop)
{
  g_main_loop_quit(gloop);
}

JNIEXPORT jint JNICALL CAST_JNI(createAgentNative,jint useReliable)
{

    // Create the nice agent
    if(useReliable==1) {
      agent = nice_agent_new_reliable(g_main_loop_get_context (gloop), NICE_COMPATIBILITY_RFC5245);
    } else {
      agent = nice_agent_new(g_main_loop_get_context (gloop), NICE_COMPATIBILITY_RFC5245);
    }


    if (agent == NULL) 
    {
        LOGD("Failed to create agent");
        return 0;
    }

    // Connect to the signals
    g_signal_connect(agent, "candidate-gathering-done",
      G_CALLBACK(cb_candidate_gathering_done), NULL);
    g_signal_connect(agent, "component-state-changed",
      G_CALLBACK(cb_component_state_changed), NULL);
    g_signal_connect(agent, "new-selected-pair",
      G_CALLBACK(cb_new_selected_pair), NULL);

    return 1;

}

JNIEXPORT jint JNICALL CAST_JNI(setStunAddressNative,jstring jstun_ip,jint jstun_port)
{
    const gchar *stun_ip = (gchar*) (*env)->GetStringUTFChars(env, jstun_ip, 0);
    int stun_port = stunPort;
    int ret = 0;

    if(jstun_port>0)
       stun_port = jstun_port;

    LOGD("set Stun address %s:%d",stun_ip,stun_port);
    if(stun_ip) {
        if(agent!=NULL) { 
          g_object_set(agent, "stun-server", stun_ip, NULL);
          g_object_set(agent, "stun-server-port", stun_port, NULL);
          ret = 1;
      }
    }
    return ret;
}

JNIEXPORT jint JNICALL CAST_JNI(setControllingModeNative,jint controllingMode)
{
    int ret = 0;

    if(controllingMode==1) {
      controlling = TRUE;
    } else if(controllingMode==0) {
      controlling = FALSE;
    } else {
      LOGD("controlling mode should be 0 or 1");
      return 0;
    }

    if(agent!=NULL) { 
      g_object_set(agent, "controlling-mode", controlling, NULL);
      ret = 1;
    }

    return ret;
}

JNIEXPORT jint JNICALL CAST_JNI(addStreamNative,jstring jstreamName,jint numberOfComponent)
{
    jboolean isCopy;
    const gchar *name = (gchar*) (*env)->GetStringUTFChars(env, jstreamName, &isCopy);

    // Create a new stream with one component
    stream_id = nice_agent_add_stream(agent, numberOfComponent);
    if (stream_id == 0) {
        LOGD("Failed to add stream");
        return 0;
    }
    nice_agent_set_stream_name (agent, stream_id, name);

    if(isCopy == JNI_TRUE) {
      (*env)->ReleaseStringUTFChars(env,jstreamName,name);
    }

    totalComponentNumber = numberOfComponent;
    for(int i=0;i<totalComponentNumber;i++) {
      // Attach to the component to receive the data
      // Without this call, candidates cannot be gathered
      nice_agent_attach_recv(agent, stream_id, i+1,
          g_main_loop_get_context (gloop), cb_nice_recv1, NULL);
      
    }

    return 1;
}

JNIEXPORT jstring JNICALL CAST_JNI_NON(getLocalSdpNative)
{
      jstring ret;

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
    gchar *sdp, *sdp64;

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



JNIEXPORT jint JNICALL CAST_JNI(setRemoteSdpNative,jstring jremoteSdp,jlong size)
{
  gchar* sdp_decode;
  const char *remoteSdp = (*env)->GetStringUTFChars(env, jremoteSdp, 0);
  gsize sdp_len = size;
  sdp_decode = (gchar *) g_base64_decode (remoteSdp, &sdp_len);
  if (sdp_decode && nice_agent_parse_remote_sdp (agent, sdp_decode) > 0) {
    g_free (sdp_decode);
    LOGD("waiting for state READY or FAILED signal...");
  } else {
    cbMsgToJava("please retry it");
  }
   // use your string
  (*env)->ReleaseStringUTFChars(env, jremoteSdp, remoteSdp);

}

JNIEXPORT jint JNICALL CAST_JNI(sendMsgNative,jstring data,jint component_id)
{
  if(component_id>totalComponentNumber || component_id<=0) {
    LOGD("fail to send to component id %d",component_id);
    return 0;
  }

  const gchar *line = (gchar*) (*env)->GetStringUTFChars(env, data, 0);
  return nice_agent_send(agent, stream_id, component_id, strlen(line), line);
}


JNIEXPORT jint JNICALL CAST_JNI(sendDataNative,jbyteArray data,jint len,jint component_id)
{
  if(component_id>totalComponentNumber || component_id<=0) {
    LOGD("fail to send to component id %d",component_id);
    return 0;
  }
    //LOGD("fsend to component id %d",component_id);

  jboolean isCopy;
  int ret;
  //const gchar *line = (gchar*) (*env)->GetStringUTFChars(env, data, 0);
  const jbyte* _data = (jbyte*) (*env)->GetByteArrayElements(env,data,0);
  ret = nice_agent_send(agent, stream_id, component_id, len, _data);

  // if(isCopy) {
  //   (*env)->ReleaseByteArrayElements(env,data,_data,0);
  // }


  return ret;
}

JNIEXPORT jint JNICALL CAST_JNI(sendDataDirectNative,jobject data,jint len,jint component_id)
{
  if(component_id>totalComponentNumber || component_id<=0) {
    LOGD("fail to send to component id %d",component_id);
    return 0;
  }

  int ret;
  const jbyte* _data = (jbyte*) (*env)->GetDirectBufferAddress(env,data);

  ret = nice_agent_send(agent, stream_id, component_id, len, _data);
  LOGD("Send %d size",ret);
  return ret;
}


JNIEXPORT jint JNICALL CAST_JNI(sendVideoDataDirectNative,jobject data,jint len,jint component_id)
{
  if(component_id>totalComponentNumber || component_id<=0) {
    LOGD("fail to send to component id %d",component_id);
    return 0;
  }
  int ret;

  ret = nice_agent_send(agent, stream_id, component_id, sizeof(len), (const gchar*)&len);

  const jbyte* _data = (jbyte*) (*env)->GetDirectBufferAddress(env,data);

  ret = nice_agent_send(agent, stream_id, component_id, len, _data);
  LOGD("Send %d size",ret);
  return ret;
}

void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
    gpointer data)
{
  LOGD("SIGNAL candidate gathering done\n");

  g_mutex_lock(&gather_mutex);
  candidate_gathering_done = TRUE;
  g_cond_signal(&gather_cond);
  g_mutex_unlock(&gather_mutex);

  JNIEnv *env;

  if(hasStateObserver) {
    if((*gJavaVM)->AttachCurrentThread(gJavaVM, &env, NULL) != JNI_OK)
    {
      LOGD("%s: AttachCurrentThread() failed", __FUNCTION__);

    } 
    else
    {
        (*env)->CallVoidMethod(env,stateObserverObj,cbCandidateGatheringDoneId,stream_id);
    }
  }
}

void cb_component_state_changed(NiceAgent *agent, guint stream_id,
    guint component_id, guint state,
    gpointer data)
{
  LOGD("SIGNAL: state changed %d %d %s[%d]\n",
      stream_id, component_id, state_name[state], state);

  JNIEnv* env;
  if(hasStateObserver) {
    if((*gJavaVM)->AttachCurrentThread(gJavaVM, &env, NULL) != JNI_OK)
    {
      LOGD("%s: AttachCurrentThread() failed", __FUNCTION__);
    } 
    else
    {
        (*env)->CallVoidMethod(env,stateObserverObj,cbComponentStateChangedId,stream_id,component_id,state);
    }
  }
}

void
cb_new_selected_pair(NiceAgent *agent, guint _stream_id,
    guint component_id, gchar *lfoundation,
    gchar *rfoundation, gpointer data)
{
  //g_debug("SIGNAL: selected pair %s %s", lfoundation, rfoundation);
  LOGD("SIGNAL: selected pair %s %s", lfoundation, rfoundation);
}

jobject callback_obj;
jmethodID callback_mid;

void cb_nice_recv1(NiceAgent *agent, guint stream_id, guint component_id,
    guint len, gchar *buf, gpointer data)
{
  static int j = 0;
  j++;
  if (len == 1 && buf[0] == '\0')
    g_main_loop_quit (gloop);




  JNIEnv *env;
  jclass cls;
  jmethodID mid;


  if(cbObserverCtx[component_id].used!=1) {
    LOGD("Please Register callback function for comp[%d]",component_id);
    return;
  }

  // using interface which is registered
  if((*gJavaVM)->AttachCurrentThread(gJavaVM, &env, NULL) != JNI_OK)
  {
    LOGD("%s: AttachCurrentThread() failed", __FUNCTION__);

  } 
  else
  {
      jbyteArray arr = (*env)->NewByteArray(env,len);
      (*env)->SetByteArrayRegion(env,arr,0,len, (jbyte*)buf);
      (*env)->CallVoidMethod(env, cbObserverCtx[component_id].jObj, cbObserverCtx[component_id].jmid, arr);
      (*env)->DeleteLocalRef(env, arr);
  }

}


JNIEXPORT void CAST_JNI(registerReceiveObserverNative, jobject cb_obj, jint component_id) {

    cbObserverCtx[component_id].jObj = (*env)->NewGlobalRef(env,cb_obj);
    jclass clz = (*env)->GetObjectClass(env,cbObserverCtx[component_id].jObj);
    cbObserverCtx[component_id].jmid = (*env)->GetMethodID(env,clz,"obCallback","([B)V");
    cbObserverCtx[component_id].used = 1;
}




JNIEXPORT void CAST_JNI(registerStateObserverNative,jobject cb_obj) {

    stateObserverObj = (*env)->NewGlobalRef(env,cb_obj);
    jclass clz = (*env)->GetObjectClass(env,stateObserverObj);
    if(clz == NULL) {
        LOGD("Failed to find class\n");
    }
    cbCandidateGatheringDoneId = (*env)->GetMethodID(env,clz,"cbCandidateGatheringDone","(I)V");
    cbComponentStateChangedId  = (*env)->GetMethodID(env,clz,"cbComponentStateChanged","(III)V");
    hasStateObserver = 1;
}





JNIEXPORT jint JNICALL CAST_JNI(createReceiveProcessNative,jobject cb_obj,jint sid,jint cid) {
    jobject cb_object = (*env)->NewGlobalRef(env,cb_obj);
    jclass clz = (*env)->GetObjectClass(env,cb_object);
    if(clz == NULL) {
        LOGD("Failed to find class\n");
    }
    jmethodID cb_mid = (*env)->GetMethodID(env,clz,"onMessage","([BI)V");
      
    gsize buf_len = 1024*1024;
    guint8 *buf = (guint8*) malloc(buf_len);
    jbyteArray arr = (*env)->NewByteArray(env,buf_len);
    gsize recv_size = 0;
    exit_thread = FALSE;
    while(!exit_thread)
    {
      recv_size = nice_agent_recv(agent, sid,cid,buf,buf_len,NULL,NULL);
      LOGD("nice_agent_recv s[%d]c[%d] recv size : %d",sid,cid,recv_size);
      if(recv_size>0) {
          (*env)->SetByteArrayRegion(env,arr,0,recv_size, (jbyte*)buf);
          (*env)->CallVoidMethod(env,cb_object,cb_mid,arr);
      }
    }
    LOGD("thread out");
} 