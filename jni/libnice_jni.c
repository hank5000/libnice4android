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

#define MAX_STREAM 2
#define MAX_COMPONENT 20
static JavaVM *gJavaVM;
typedef struct CallbackObserverCtx {
  int used;
  jobject   jObj;
  jmethodID jmid;
  //struct cbObserverCtx* next;
}CallbackCtx;

typedef struct AgentManagetCtx {
  NiceAgent* agent;
  int hasStateObserver;
  jobject stateObserverObj;
  jmethodID cbCandidateGatheringDoneId;
  jmethodID cbComponentStateChangedId;
  CallbackCtx recvCallbackCtx[MAX_STREAM][MAX_COMPONENT];
  gboolean exit_thread;
  gboolean candidate_gathering_done;
  GMutex gather_mutex;
  GCond gather_cond;
}AgentCtx;

CallbackCtx recvCallbackCtx[MAX_STREAM][MAX_COMPONENT];
int currentCtxIdx = 0;


NiceAgent *agent = NULL;
int stunPort = 3478;
static GMainLoop *gloop;


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

JNIEnv* getEnv()
{
  JNIEnv* env;
  (*gJavaVM)->GetEnv(gJavaVM, (void**)&env, JNI_VERSION_1_6);
  return env;
}

JNIEXPORT jint JNICALL CAST_JNI(restartStreamNative,jlong agentCtxLong,jint stream_id) {
  AgentCtx* agentCtx = (AgentCtx*) agentCtxLong;
  NiceAgent* tmp_agent = agentCtx->agent;

  return nice_agent_restart_stream(tmp_agent,stream_id);
}

int bInit = 0;

JNIEXPORT jint JNICALL CAST_JNI_NON(initNative)
{
  // Libnice init
  if(!bInit) {
    g_networking_init();
    gloop = g_main_loop_new(NULL, FALSE);
    bInit = 1;
    (*env)->GetJavaVM(env,&gJavaVM);
  }
	return 1;
}

int isMainLoopStart = 0;

JNIEXPORT jint JNICALL CAST_JNI_NON(isMainLoopStarted)
{
  return isMainLoopStart;
}

JNIEXPORT jint JNICALL CAST_JNI_NON(mainLoopStart)
{
  if(isMainLoopStart)
  {
    return 1;
  }

  isMainLoopStart = 1;
  g_main_loop_run(gloop);
  return 0;
}

JNIEXPORT jint JNICALL CAST_JNI_NON(mainLoopStop)
{
  g_main_loop_quit(gloop);
  isMainLoopStart = 0;
}

JNIEXPORT jlong JNICALL CAST_JNI(createAgentNative,jint useReliable)
{
  NiceAgent* tmp_agent = NULL;
  AgentCtx* agentCtx = (AgentCtx*) malloc(sizeof(AgentCtx));
  
  agentCtx->hasStateObserver = 0;
  agentCtx->stateObserverObj = 0;
  agentCtx->cbCandidateGatheringDoneId=0;
  agentCtx->cbComponentStateChangedId=0;
  for(int i=0;i<MAX_COMPONENT;i++) {
    for(int j=0;j<MAX_STREAM;j++) {
      agentCtx->recvCallbackCtx[j][i].used = 0;
      agentCtx->recvCallbackCtx[j][i].jObj = 0;
      agentCtx->recvCallbackCtx[j][i].jmid = 0;
    }
  }

  agentCtx->exit_thread = FALSE;
  agentCtx->candidate_gathering_done = FALSE;
    // Create the nice agent
    if(useReliable==1) {
      tmp_agent = nice_agent_new_reliable(g_main_loop_get_context (gloop), NICE_COMPATIBILITY_RFC5245);
    } else {
      tmp_agent = nice_agent_new(g_main_loop_get_context (gloop), NICE_COMPATIBILITY_RFC5245);
    }

    agentCtx->agent = tmp_agent;




    if (tmp_agent == NULL) 
    {
        LOGD("Failed to create agent");
        return 0;
    }

    // Connect to the signals
    g_signal_connect(tmp_agent, "candidate-gathering-done",
      G_CALLBACK(cb_candidate_gathering_done), agentCtx);
    g_signal_connect(tmp_agent, "component-state-changed",
      G_CALLBACK(cb_component_state_changed), agentCtx);
    g_signal_connect(tmp_agent, "new-selected-pair",
      G_CALLBACK(cb_new_selected_pair), agentCtx);

    return (long)agentCtx;
}

JNIEXPORT jint JNICALL CAST_JNI(setStunAddressNative,jlong agentCtxLong,jstring jstun_ip,jint jstun_port)
{
  AgentCtx* agentCtx = (AgentCtx*) agentCtxLong;

  NiceAgent* tmp_agent = agentCtx->agent;
    const gchar *stun_ip = (gchar*) (*env)->GetStringUTFChars(env, jstun_ip, 0);
    int stun_port = stunPort;
    int ret = 0;

    if(jstun_port>0)
       stun_port = jstun_port;

    LOGD("set Stun address %s:%d",stun_ip,stun_port);
    if(stun_ip) {
        if(tmp_agent!=NULL) { 
          g_object_set(tmp_agent, "stun-server", stun_ip, NULL);
          g_object_set(tmp_agent, "stun-server-port", stun_port, NULL);
          ret = 1;
      }
    }
    return ret;
}

JNIEXPORT jint JNICALL CAST_JNI(setControllingModeNative,jlong agentCtxLong,jint controllingMode)
{
    AgentCtx* agentCtx = (AgentCtx*) agentCtxLong;
    NiceAgent* tmp_agent = agentCtx->agent;

    int ret = 0;
    gboolean controlling;
    if(controllingMode==1) {
      controlling = TRUE;
    } else if(controllingMode==0) {
      controlling = FALSE;
    } else {
      LOGD("controlling mode should be 0 or 1");
      return 0;
    }

    if(tmp_agent!=NULL) { 
      g_object_set(tmp_agent, "controlling-mode", controlling, NULL);
      ret = 1;
    }

    return ret;
}

JNIEXPORT jint JNICALL CAST_JNI(addStreamNative,jlong agentCtxLong,jstring jstreamName,jint numberOfComponent)
{
  AgentCtx* agentCtx = (AgentCtx*) agentCtxLong;
  NiceAgent* tmp_agent = agentCtx->agent;

    jboolean isCopy;
    const gchar *name = (gchar*) (*env)->GetStringUTFChars(env, jstreamName, &isCopy);

    // Create a new stream with one component
    int stream_id = nice_agent_add_stream(tmp_agent, numberOfComponent);
    if (stream_id == 0) {
        LOGD("Failed to add stream");
        return 0;
    }
    nice_agent_set_stream_name (tmp_agent, stream_id, name);

    if(isCopy == JNI_TRUE) {
      (*env)->ReleaseStringUTFChars(env,jstreamName,name);
    }

    totalComponentNumber = numberOfComponent;
    for(int i=0;i<totalComponentNumber;i++) {
      // Attach to the component to receive the data
      // Without this call, candidates cannot be gathered
        nice_agent_attach_recv(tmp_agent, stream_id, i+1,
            g_main_loop_get_context (gloop), cb_nice_recv1, agentCtx);

    }

    return stream_id;
}



JNIEXPORT jstring JNICALL CAST_JNI(getLocalSdpNative,jlong agentCtxLong,jint stream_id)
{
    jstring ret;
  AgentCtx* agentCtx = (AgentCtx*) agentCtxLong;
  NiceAgent* tmp_agent = agentCtx->agent;
      // Start gathering local candidates
    if (!nice_agent_gather_candidates(tmp_agent, stream_id))
        LOGD("Failed to start candidate gathering");

    LOGD("waiting for candidate-gathering-done signal...");

    g_mutex_lock(&(agentCtx->gather_mutex));
    while (!agentCtx->exit_thread && !(agentCtx->candidate_gathering_done))
        g_cond_wait(&(agentCtx->gather_cond), &(agentCtx->gather_mutex));
    g_mutex_unlock(&(agentCtx->gather_mutex));
    if (agentCtx->exit_thread) {
        LOGD("something wrong, exit_thread = 1");
        return 0;
    }
    gchar *sdp, *sdp64;

    // Candidate gathering is done. Send our local candidates on stdout
    sdp = nice_agent_generate_local_sdp (tmp_agent);
    LOGD("Generated SDP from agent :\n%s\n\n", sdp);
    LOGD("Copy the following line to remote client:\n");
    sdp64 = g_base64_encode ((const guchar *)sdp, strlen (sdp));

    ret = (*env)->NewStringUTF(env,sdp64);
    
    LOGD("\n %s\n",sdp64);

    g_free (sdp);
    g_free (sdp64);

    return ret;
}



JNIEXPORT jint JNICALL CAST_JNI(setRemoteSdpNative,jlong agentCtxLong,jstring jremoteSdp,jlong size)
{
  AgentCtx* agentCtx = (AgentCtx*) agentCtxLong;
  NiceAgent* tmp_agent = agentCtx->agent;
  gchar* sdp_decode;
  const char *remoteSdp = (*env)->GetStringUTFChars(env, jremoteSdp, 0);
  gsize sdp_len = size;
  LOGD("set Remote SDP : %s\n",remoteSdp);
  sdp_decode = (gchar *) g_base64_decode (remoteSdp, &sdp_len);
  if (sdp_decode && nice_agent_parse_remote_sdp (tmp_agent, sdp_decode) > 0) {
    g_free (sdp_decode);
    LOGD("waiting for state READY or FAILED signal...");
  } else {
    // TODO: Add Callback Function.
    LOGD("please try it again");
    //cbMsgToJava("please retry it");
  }
   // use your string
  (*env)->ReleaseStringUTFChars(env, jremoteSdp, remoteSdp);
}

JNIEXPORT jint JNICALL CAST_JNI(sendMsgNative,jlong agentCtxLong,jstring data,jint stream_id,jint component_id)
{
  AgentCtx* agentCtx = (AgentCtx*) agentCtxLong;
  NiceAgent* tmp_agent = agentCtx->agent;

  if(component_id>totalComponentNumber || component_id<=0) {
    LOGD("fail to send to component id %d",component_id);
    return 0;
  }

  const gchar *line = (gchar*) (*env)->GetStringUTFChars(env, data, 0);
  return nice_agent_send(tmp_agent, stream_id, component_id, strlen(line), line);
}


JNIEXPORT jint JNICALL CAST_JNI(sendDataNative,jlong agentCtxLong,jbyteArray data,jint len,jint stream_id,jint component_id)
{
  AgentCtx* agentCtx = (AgentCtx*) agentCtxLong;
  NiceAgent* tmp_agent = agentCtx->agent;
  if(component_id>totalComponentNumber || component_id<=0) {
    LOGD("fail to send to component id %d",component_id);
    return 0;
  }
    //LOGD("fsend to component id %d",component_id);

  jboolean isCopy;
  int ret;
  //const gchar *line = (gchar*) (*env)->GetStringUTFChars(env, data, 0);
  const jbyte* _data = (jbyte*) (*env)->GetByteArrayElements(env,data,0);
  ret = nice_agent_send(tmp_agent, stream_id, component_id, len, _data);

  // if(isCopy) {
  //   (*env)->ReleaseByteArrayElements(env,data,_data,0);
  // }


  return ret;
}

jbyte* directBuffers[16];

JNIEXPORT jint JNICALL CAST_JNI(setDirectBufferIndexNative,jobject data,jint index)
{

  int ret=1;
  directBuffers[index] = (jbyte*) (*env)->GetDirectBufferAddress(env,data);

  return ret;
}


JNIEXPORT jint JNICALL CAST_JNI(sendDataDirectByIndexNative,jlong agentCtxLong,jobject data,jint len,jint index,jint stream_id,jint component_id)
{
  AgentCtx* agentCtx = (AgentCtx*) agentCtxLong;
  NiceAgent* tmp_agent = agentCtx->agent;

  if(component_id>totalComponentNumber || component_id<=0) {
    LOGD("fail to send to component id %d",component_id);
    return 0;
  }

  int ret;

  ret = nice_agent_send(tmp_agent, stream_id, component_id, len, directBuffers[index]);
  return ret;
}


JNIEXPORT jint JNICALL CAST_JNI(sendDataDirectNative,jlong agentCtxLong,jobject data,jint len,jint stream_id,jint component_id)
{
  AgentCtx* agentCtx = (AgentCtx*) agentCtxLong;
  NiceAgent* tmp_agent = agentCtx->agent;
  if(component_id>totalComponentNumber || component_id<=0) {
    LOGD("fail to send to component id %d",component_id);
    return 0;
  }

  int ret;
  const jbyte* _data = (jbyte*) (*env)->GetDirectBufferAddress(env,data);

  ret = nice_agent_send(tmp_agent, stream_id, component_id, len, _data);
  //LOGD("Send %d size",ret);
  return ret;
}

void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
    gpointer data)
{

  AgentCtx* agentCtx = (AgentCtx*) data;

  LOGD("SIGNAL candidate gathering done\n");

  g_mutex_lock(&(agentCtx->gather_mutex));
  agentCtx->candidate_gathering_done = TRUE;
  g_cond_signal(&(agentCtx->gather_cond));
  g_mutex_unlock(&(agentCtx->gather_mutex));

  JNIEnv *env;

  if(agentCtx->hasStateObserver) {
    if((*gJavaVM)->AttachCurrentThread(gJavaVM, &env, NULL) != JNI_OK)
    {
      LOGD("%s: AttachCurrentThread() failed", __FUNCTION__);

    } 
    else
    {
        (*env)->CallVoidMethod(env,agentCtx->stateObserverObj,agentCtx->cbCandidateGatheringDoneId,stream_id);
    }
  }
}

void cb_component_state_changed(NiceAgent *agent, guint stream_id,
    guint component_id, guint state,
    gpointer data)
{
  LOGD("SIGNAL: state changed %d %d %s[%d]\n",
      stream_id, component_id, state_name[state], state);
  AgentCtx* agentCtx = (AgentCtx*) data;

  JNIEnv* env;
  if(agentCtx->hasStateObserver) {
    if((*gJavaVM)->AttachCurrentThread(gJavaVM, &env, NULL) != JNI_OK)
    {
      LOGD("%s: AttachCurrentThread() failed", __FUNCTION__);
    } 
    else
    {
        (*env)->CallVoidMethod(env,agentCtx->stateObserverObj,agentCtx->cbComponentStateChangedId,stream_id,component_id,state);
    }
  }
}

void
cb_new_selected_pair(NiceAgent *agent, guint _stream_id,
    guint component_id, gchar *lfoundation,
    gchar *rfoundation, gpointer data)
{
  AgentCtx* agentCtx = (AgentCtx*) data;
  //g_debug("SIGNAL: selected pair %s %s", lfoundation, rfoundation);
  LOGD("SIGNAL: selected pair %s %s", lfoundation, rfoundation);
}

jobject callback_obj;
jmethodID callback_mid;

JNIEnv *jenv = NULL;

void cb_nice_recv1(NiceAgent *agent, guint stream_id, guint component_id,
    guint len, gchar *buf, gpointer data)
{
  AgentCtx* agentCtx = (AgentCtx*) data;
  static int j = 0;
  if (len == 1 && buf[0] == '\0')
    g_main_loop_quit (gloop);

  JNIEnv *env;
  jclass cls;
  jmethodID mid;


  if(agentCtx->recvCallbackCtx[stream_id][component_id].used!=1) {
    LOGD("Please Register callback function for stream[%d],component[%d]: receive size : %d",stream_id,component_id,len);
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
      jbyteArray tmp_arr = (jbyteArray) (*env)->NewGlobalRef(env, arr);
      (*env)->CallVoidMethod(env, agentCtx->recvCallbackCtx[stream_id][component_id].jObj, agentCtx->recvCallbackCtx[stream_id][component_id].jmid, tmp_arr);
      (*env)->DeleteGlobalRef(env,tmp_arr);
      (*env)->DeleteLocalRef(env, arr);
  }
}


JNIEXPORT void CAST_JNI(registerReceiveCallbackNative,jlong agentCtxLong, jobject cb_obj,jint stream_id,jint component_id) {
    AgentCtx* agentCtx = (AgentCtx*) agentCtxLong;

    agentCtx->recvCallbackCtx[stream_id][component_id].jObj = (*env)->NewGlobalRef(env,cb_obj);
    jclass clz = (*env)->GetObjectClass(env,agentCtx->recvCallbackCtx[stream_id][component_id].jObj);
    agentCtx->recvCallbackCtx[stream_id][component_id].jmid = (*env)->GetMethodID(env,clz,"onMessage","([B)V");
    agentCtx->recvCallbackCtx[stream_id][component_id].used = 1;
}

JNIEXPORT void CAST_JNI(registerStateObserverNative,jlong agentCtxLong,jobject cb_obj) {
    AgentCtx* agentCtx = (AgentCtx*) agentCtxLong;


    agentCtx->stateObserverObj = (*env)->NewGlobalRef(env,cb_obj);
    jclass clz = (*env)->GetObjectClass(env,agentCtx->stateObserverObj);
    if(clz == NULL) {
        LOGD("Failed to find class\n");
    }
    agentCtx->cbCandidateGatheringDoneId = (*env)->GetMethodID(env,clz,"cbCandidateGatheringDone","(I)V");
    agentCtx->cbComponentStateChangedId  = (*env)->GetMethodID(env,clz,"cbComponentStateChanged","(III)V");
    agentCtx->hasStateObserver = 1;
}

// JNIEXPORT jint JNICALL CAST_JNI(createReceiveProcessNative,jobject cb_obj,jint sid,jint cid) {
//     jobject cb_object = (*env)->NewGlobalRef(env,cb_obj);
//     jclass clz = (*env)->GetObjectClass(env,cb_object);
//     if(clz == NULL) {
//         LOGD("Failed to find class\n");
//     }
//     jmethodID cb_mid = (*env)->GetMethodID(env,clz,"onMessage","([BI)V");
      
//     gsize buf_len = 1024*1024;
//     guint8 *buf = (guint8*) malloc(buf_len);
//     jbyteArray arr = (*env)->NewByteArray(env,buf_len);
//     gsize recv_size = 0;
//     exit_thread = FALSE;
//     while(!exit_thread)
//     {
//       recv_size = nice_agent_recv(agent, sid,cid,buf,buf_len,NULL,NULL);
//       LOGD("nice_agent_recv s[%d]c[%d] recv size : %d",sid,cid,recv_size);
//       if(recv_size>0) {
//           (*env)->SetByteArrayRegion(env,arr,0,recv_size, (jbyte*)buf);
//           (*env)->CallVoidMethod(env,cb_object,cb_mid,arr);
//       }
//     }
//     LOGD("thread out");
// } 