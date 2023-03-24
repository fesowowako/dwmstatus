#include <X11/Xlib.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  const char *prefix;
  const char *command;
  const uint64_t interval;
} Module;

#include "config.h"

#define MODULESLEN (sizeof(modules) / sizeof(modules[0]))

#ifndef MAX_OUTPUT
#define MAX_CMD_OUTPUT 150
#endif
#ifndef MAX_STATUS
#define MAX_STATUS 1024
#endif

char cached_strs[MODULESLEN][MAX_CMD_OUTPUT];
size_t prefix_lens[MODULESLEN];
pthread_mutex_t update_cond_mutex;
pthread_mutex_t module_mutexes[MODULESLEN];
pthread_mutex_t quit_mutex;

pthread_cond_t update_cond;

volatile bool update_refresh = false;

static Window root;
static Display *dpy;
pthread_mutex_t quit_mutex;
static volatile sig_atomic_t quit = false;

/*
 *NOTE: error checking not included here because it would make it less
 *efficient and would very rarely matter
 */
static void *updatemodule(void *data) {
  const size_t index = (size_t)data;
  const uint64_t interval_ns = modules[index].interval * 1000;
  const uint32_t secs = interval_ns / (1000 * 1000);
  const uint32_t ns = interval_ns % (1000 * 1000);
  const struct timespec time = {.tv_nsec =
                                    ns, // modules[index].interval * 1000000L,
                                .tv_sec = secs};

  const char *command = modules[index].command;

  do {
    pthread_mutex_unlock(&quit_mutex);
    FILE *p = popen(command, "r");
    pthread_mutex_lock(&module_mutexes[index]);
    fgets(cached_strs[index], MAX_CMD_OUTPUT, p);
    pthread_mutex_unlock(&module_mutexes[index]);
    pclose(p);
    update_refresh = true;
    pthread_cond_signal(&update_cond);
    nanosleep(&time, NULL);
    pthread_mutex_lock(&quit_mutex);
  } while (interval_ns != 0 && !quit);
  pthread_mutex_unlock(&quit_mutex);
  pthread_exit(NULL);
}

static void setstatus(char *status) {
  if (XStoreName(dpy, root, status) < 0) {
    fprintf(stderr, "failed to set status");
    exit(1);
  }
  XFlush(dpy);
}

static void *updatebar(void *ignore) {
  (void)ignore;
  char status[MAX_STATUS];
  pthread_mutex_lock(&quit_mutex);
  while (!quit) {
    {
      pthread_mutex_unlock(&quit_mutex);
      pthread_mutex_lock(&update_cond_mutex);
      while (!update_refresh) {
        pthread_cond_wait(&update_cond, &update_cond_mutex);
      }
      update_refresh = false;
      pthread_mutex_unlock(&update_cond_mutex);
    }

    size_t index = 0;
    for (size_t i = 0; i < MODULESLEN; i++) {
      pthread_mutex_lock(&module_mutexes[i]);
      const char *str = cached_strs[i];
      const size_t len = strlen(str);
      if (len == 0) {
        pthread_mutex_unlock(&module_mutexes[i]);
        continue;
      }
      if (prefix_lens[i]) {
        memcpy(status + index, modules[i].prefix, prefix_lens[i]);
        index += prefix_lens[i];
      }
      memcpy(status + index, str, len);
      pthread_mutex_unlock(&module_mutexes[i]);
      index += len - 1; // ignore the newline at the end
      if (MODULESLEN - i > 1) {
        memcpy(status + index, seperator, sizeof(seperator));
        // ignore the null terminator in 'seperator'
        index += sizeof(seperator) - 1;
      }
    }
    status[index] = '\0';
    setstatus(status);
    pthread_mutex_lock(&quit_mutex);
  }
  fprintf(stderr, "asdasd\n");
  sleep(1);
  pthread_exit(NULL);
}

static void clean_exit(const int ignore) {
  (void)ignore;
  pthread_mutex_lock(&quit_mutex);
  quit = true;
  pthread_mutex_unlock(&quit_mutex);
}

static void setup(void) {
  /*
// cleanly exit via signal hanlder
struct sigaction act = {0};
act.sa_handler = clean_exit;
// act.sa_flags |= SA_RESTART;
sigaction(SIGINT, &act, NULL);
sigaction(SIGTERM, &act, NULL);
  */
  for (size_t i = 0; i < MODULESLEN; i++) {
    pthread_t thread_handle;
    pthread_create(&thread_handle, NULL, updatemodule, (void *)i);

    pthread_mutex_init(&module_mutexes[i], NULL);
  }
  pthread_mutex_init(&quit_mutex, NULL);

  pthread_mutex_init(&update_cond_mutex, NULL);
  pthread_mutex_init(&quit_mutex, NULL);
  pthread_cond_init(&update_cond, NULL);
  pthread_t thread_handle;
  pthread_create(&thread_handle, NULL, updatebar, NULL);
  pthread_join(thread_handle, NULL);
}

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "test") == 0) {
      for (size_t i = 0; i < MODULESLEN; i++) {
        fprintf(stderr, "running command `%s`...\n", modules[i].command);
        if (system(modules[i].command) != 0) {
          fprintf(stderr, "test failed on module %zu!", i + 1);
          return 1;
        }
      }
      return 0;
    }
  }
  // might be able to do this on compile time
  // but it only matters on start up so this is okay
  for (size_t i = 0; i < MODULESLEN; i++) {
    if (modules[i].prefix)
      prefix_lens[i] = strlen(modules[i].prefix);
    else
      prefix_lens[i] = 0;
  }

  dpy = XOpenDisplay(NULL);
  root = XDefaultRootWindow(dpy);
  if (dpy == NULL) {
    fprintf(stderr, "failed to open display\n");
    return 1;
  }
  setup();
  XCloseDisplay(dpy);
  return 0;
}
