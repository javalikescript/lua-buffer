#ifdef USE_SOFT_MUTEX

#include <unistd.h>

#define MUTEX_MAX_ID 2
#define MUTEX_GET_ID(_PM, _LS) (_PM->owner == _LS ? 0 : 1)

typedef struct mutex_t {
  lua_State  *owner;
  int choosing[2];
  int ticket[2];
} mutex_t;

static void mutex_soft_init(mutex_t *pm, lua_State *l) {
  pm->owner = l;
  for(int i = 0; i < MUTEX_MAX_ID; i++) {
    pm->choosing[i] = 0;
    pm->ticket[i] = 0;
  }
}

// From Lamport's bakery algorithm
static int mutex_soft_lock(mutex_t *pm, int id, int try) {
  int max = 0;
  int cur, tic;
  pm->choosing[id] = 1;
  for(int i = 0; i < MUTEX_MAX_ID; i++) {
    cur = pm->ticket[i];
    if (cur > max)
      max = cur;
  }
  tic = 1 + max;
  pm->ticket[id] = tic;
  pm->choosing[id] = 0;
  printf("mutex_soft_init(%d, %d) max: %d\n", id, try, max);
  if (try && max != 0) {
    pm->ticket[id] = 0;
    return 0;
  }
  for(int i = 0; i < MUTEX_MAX_ID; i++) {
    if (i != id) {
      while (pm->choosing[i])
        sleep(0);
      for (;;) {
        cur = pm->ticket[i];
        if (cur != 0 && (cur < tic || (cur == tic && i < id))) {
          sleep(0);
        } else {
          break;
        }
      }
    }
  }
  return 1;
}

#elif WIN32

#include <synchapi.h>

typedef CRITICAL_SECTION mutex_t;

#else // PTHREAD

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

typedef pthread_mutex_t mutex_t;

#endif

static int mutex_new(lua_State *l) {
  lua_newuserdata(l, sizeof(mutex_t));
  return 1;
}

static int mutex_init(lua_State *l) {
  mutex_t *pm = lua_touserdata(l, 1);
#ifdef USE_SOFT_MUTEX
  mutex_soft_init(pm, l);
#elif WIN32
  InitializeCriticalSection(pm);
#else
  pthread_mutexattr_t attr;
  int init_err;
  if (pthread_mutexattr_init(&attr))
    goto err;
  if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE))
    goto err;
  init_err = pthread_mutex_init(pm, &attr);
  if (pthread_mutexattr_destroy(&attr))
    goto err;
  if (init_err) {
    err:
    luaL_error(l, "fail to initialize mutex");
  }
#endif
  return 0;
}

static int mutex_destroy(lua_State *l) {
  mutex_t *pm = lua_touserdata(l, 1);
#ifdef USE_SOFT_MUTEX
  mutex_soft_init(pm, NULL);
#elif WIN32
  DeleteCriticalSection(pm);
#else
  if (pthread_mutex_destroy(pm))
    luaL_error(l, "fail to destroy mutex");
#endif
  return 0;
}

static int mutex_lock(lua_State *l) {
  mutex_t *pm = lua_touserdata(l, 1);
#ifdef USE_SOFT_MUTEX
  int id = MUTEX_GET_ID(pm, l);
  mutex_soft_lock(pm, id, 0);
#elif WIN32
  EnterCriticalSection(pm);
#else
  if (pthread_mutex_lock(pm))
    luaL_error(l, "fail to lock mutex");
#endif
  return 0;
}

static int mutex_unlock(lua_State *l) {
  mutex_t *pm = lua_touserdata(l, 1);
#ifdef USE_SOFT_MUTEX
  int id = MUTEX_GET_ID(pm, l);
  pm->ticket[id] = 0;
#elif WIN32
  LeaveCriticalSection(pm);
#else
  if (pthread_mutex_unlock(pm))
    luaL_error(l, "fail to unlock mutex");
#endif
  return 0;
}

static int mutex_trylock(lua_State *l) {
  mutex_t *pm = lua_touserdata(l, 1);
  int success = 0;
#ifdef USE_SOFT_MUTEX
  int id = MUTEX_GET_ID(pm, l);
  success = mutex_soft_lock(pm, id, 1);
#elif WIN32
  success = TryEnterCriticalSection(pm);
#else
  int err = pthread_mutex_trylock(pm);
  if (err) {
    if (err != EBUSY && err != EAGAIN)
      luaL_error(l, "fail to try lock mutex");
  } else {
    success = 1;
  }
#endif
  lua_pushboolean(l, success);
  return 1;
}
