#include <jni.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <agent.h>
#include <android/log.h>

// has input
#define CAST_JNI_FN(fn) Java_com_via_libnice_##fn
#define CAST_JNI_IN(fn, arg...) CAST_JNI_FN(fn) (JNIEnv* env, jobject obj, arg)
#define CAST_JNI(fn, arg...) CAST_JNI_IN(fn, arg)

// has no input
#define CAST_JNI_NON(fn) CAST_JNI_FN(fn) (JNIEnv* env, jobject obj)






#define JNI_TAG "libnice-jni"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, JNI_TAG, __VA_ARGS__)

#if GLIB_CHECK_VERSION(2, 36, 0)
#include <gio/gnetworking.h>
#endif

JNIEnv* niceJavaEnv = 0;
jobject niceJavaObj= 0;
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

jclass libniceJava;
jmethodID method1;

JNIEXPORT jint JNICALL CAST_JNI_NON(Init)
{
    // Libnice init
    g_networking_init();
    exit_thread = FALSE;

    gloop = g_main_loop_new(NULL, FALSE);
    // JNI init
    niceJavaEnv = env;
    niceJavaObj = obj;

    //libniceJava = (*env)->GetObjectClass(env, obj);
    libniceJava = (*env)->FindClass(env, "com/via/libnice");
    //libniceJava = (*env)->GetObjectClass(env,obj);
    method1 = (*env)->GetMethodID(env,libniceJava,"cb_message","(Ljava/lang/String;)V");

	return 1;
}

JNIEXPORT jint JNICALL CAST_JNI_NON(mainLoopStart) 
{
    if(gloop) {
       g_main_loop_run (gloop);
    }
}
//(*env)->ReleaseStringUTFChars(env, jstun_ip, stunIp);

NiceAgent *agent;
guint stream_id;

JNIEXPORT jstring JNICALL CAST_JNI(createNiceAgent,jstring jstun_ip, jint jstun_port)
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
    //printf("\n  %s\n", sdp64);
    ret = (*env)->NewStringUTF(env,sdp64);

    //(*env)->CallVoidMethod(env,obj,method1,ret);

    LOGD("\n %s\n",sdp64);
    g_free (sdp);
    g_free (sdp64);


    return ret;
}



JNIEXPORT jint JNICALL CAST_JNI_NON(mainTest)
{
  GThread *gexamplethread;

  // Parse arguments  
#if GLIB_CHECK_VERSION(2, 36, 0)
  g_networking_init();
#else
  g_type_init();
#endif

  gloop = g_main_loop_new(NULL, FALSE);

  LOGD("111");
  // Run the mainloop and the example thread
  exit_thread = FALSE;
  gexamplethread = g_thread_new("example thread", &example_thread, NULL);
  g_main_loop_run (gloop);
  exit_thread = TRUE;
  LOGD("222");

  g_thread_join (gexamplethread);
  g_main_loop_unref(gloop);

  return EXIT_SUCCESS;
}



static void *
example_thread(void *data)
{
  NiceAgent *agent;
  GIOChannel* io_stdin;
  guint stream_id;
  gchar *line = NULL;
  gchar *sdp, *sdp64;
LOGD("example thread");
#ifdef G_OS_WIN32
  io_stdin = g_io_channel_win32_new_fd(_fileno(stdin));
#else
  io_stdin = g_io_channel_unix_new(fileno(stdin));
#endif
  g_io_channel_set_flags(io_stdin, G_IO_FLAG_NONBLOCK, NULL);

LOGD("example thread 1 ");

  // Create the nice agent
  agent = nice_agent_new(g_main_loop_get_context (gloop),
      NICE_COMPATIBILITY_RFC5245);
  if (agent == NULL)
    LOGD("Failed to create agent");

LOGD("example thread 2 ");

  // Set the STUN settings and controlling mode
  if (1) {
    g_object_set(agent, "stun-server", "74.125.204.127", NULL);
    g_object_set(agent, "stun-server-port", 19302, NULL);
  }
  g_object_set(agent, "controlling-mode", controlling, NULL);

  // Connect to the signals
  g_signal_connect(agent, "candidate-gathering-done",
      G_CALLBACK(cb_candidate_gathering_done), NULL);
  g_signal_connect(agent, "component-state-changed",
      G_CALLBACK(cb_component_state_changed), NULL);
LOGD("example thread 3 ");

  // Create a new stream with one component
  stream_id = nice_agent_add_stream(agent, 1);
  if (stream_id == 0)
    LOGD("Failed to add stream");
  nice_agent_set_stream_name (agent, stream_id, "text");

LOGD("example thread 4 ");

  // Attach to the component to receive the data
  // Without this call, candidates cannot be gathered
  nice_agent_attach_recv(agent, stream_id, 1,
      g_main_loop_get_context (gloop), cb_nice_recv, NULL);

LOGD("example thread 5 ");

  // Start gathering local candidates
  if (!nice_agent_gather_candidates(agent, stream_id))
    g_error("Failed to start candidate gathering");

  g_debug("waiting for candidate-gathering-done signal...");
 LOGD("example thread 6 ");

  g_mutex_lock(&gather_mutex);
  while (!exit_thread && !candidate_gathering_done)
    g_cond_wait(&gather_cond, &gather_mutex);
  g_mutex_unlock(&gather_mutex);
  if (exit_thread)
    goto end;

LOGD("example thread 7 ");

  // Candidate gathering is done. Send our local candidates on stdout
  sdp = nice_agent_generate_local_sdp (agent);
  printf("Generated SDP from agent :\n%s\n\n", sdp);
  printf("Copy the following line to remote client:\n");
  sdp64 = g_base64_encode ((const guchar *)sdp, strlen (sdp));
  LOGD("\n  %s\n", sdp64);
  g_free (sdp);
  g_free (sdp64);
  LOGD("example thread 8 ");

  // Listen on stdin for the remote candidate list
  printf("Enter remote data (single line, no wrapping):\n");
  printf("> ");
  fflush (stdout);
  while (!exit_thread) {
    GIOStatus s = g_io_channel_read_line (io_stdin, &line, NULL, NULL, NULL);
    if (s == G_IO_STATUS_NORMAL) {
      gsize sdp_len;

      sdp = (gchar *) g_base64_decode (line, &sdp_len);
      // Parse remote candidate list and set it on the agent
      if (sdp && nice_agent_parse_remote_sdp (agent, sdp) > 0) {
        g_free (sdp);
        g_free (line);
        break;
      } else {
        fprintf(stderr, "ERROR: failed to parse remote data\n");
        printf("Enter remote data (single line, no wrapping):\n");
        printf("> ");
        fflush (stdout);
      }
      g_free (sdp);
      g_free (line);
    } else if (s == G_IO_STATUS_AGAIN) {
      g_usleep (100000);
    }
  }

  g_debug("waiting for state READY or FAILED signal...");
  g_mutex_lock(&negotiate_mutex);
  while (!exit_thread && !negotiation_done)
    g_cond_wait(&negotiate_cond, &negotiate_mutex);
  g_mutex_unlock(&negotiate_mutex);
  if (exit_thread)
    goto end;

  // Listen to stdin and send data written to it
  printf("\nSend lines to remote (Ctrl-D to quit):\n");
  printf("> ");
  fflush (stdout);
  while (!exit_thread) {
    GIOStatus s = g_io_channel_read_line (io_stdin, &line, NULL, NULL, NULL);

    if (s == G_IO_STATUS_NORMAL) {
      nice_agent_send(agent, stream_id, 1, strlen(line), line);
      g_free (line);
      printf("> ");
      fflush (stdout);
    } else if (s == G_IO_STATUS_AGAIN) {
      g_usleep (100000);
    } else {
      // Ctrl-D was pressed.
      nice_agent_send(agent, stream_id, 1, 1, "\0");
      break;
    }
  }

end:
  g_object_unref(agent);
  g_io_channel_unref (io_stdin);
  g_main_loop_quit (gloop);

  return NULL;
}

static void
cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
    gpointer data)
{
  g_debug("SIGNAL candidate gathering done\n");

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
  g_debug("SIGNAL: state changed %d %d %s[%d]\n",
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

static void
cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
    guint len, gchar *buf, gpointer data)
{
  if (len == 1 && buf[0] == '\0')
    g_main_loop_quit (gloop);

  printf("%.*s", len, buf);
  fflush(stdout);
}