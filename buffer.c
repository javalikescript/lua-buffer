#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*
* Lua 5.1 and 5.2 compatibility
*/
#if LUA_VERSION_NUM < 502
// From Lua 5.3 lauxlib.c
LUALIB_API void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    int i;
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -nup);
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_setfield(L, -(nup + 2), l->name);
  }
  lua_pop(L, nup);  /* remove upvalues */
}
#define lua_rawlen lua_objlen
#endif

#if LUA_VERSION_NUM < 503
#define lua_isinteger lua_isnumber
#endif

// from lstrlib.c
// clip result to [1, inf)
static size_t posrelatI (lua_Integer pos, size_t len) {
  if (pos > 0)
    return (size_t)pos;
  else if (pos == 0)
    return 1;
  else if (pos < -(lua_Integer)len)  /* inverted comparison */
    return 1;  /* clip to 1 */
  else return len + (size_t)pos + 1;
}

static size_t unsafeposrelatI(lua_Integer pos, size_t len) {
  if (len == 0) {
    if (pos > 0) {
      return pos;
    }
    return 0;
  }
  return posrelatI(pos, len);
}

static size_t getunsafeendpos(lua_State *L, int arg, lua_Integer def, size_t len) {
  // from lstrlib.c
  lua_Integer pos = luaL_optinteger(L, arg, def);
  if (len == 0) {
    if (pos > 0) {
      return pos;
    }
    return 0;
  }
  // clip result to [0, len]
  if (pos > (lua_Integer)len)
    return len;
  else if (pos >= 0)
    return (size_t)pos;
  else if (pos < -(lua_Integer)len)
    return 0;
  else return len + (size_t)pos + 1;
}

/*
Provides functions to deal with userdata as plain byte buffer.
The indices are corrected following the same rules of function string.sub.
*/

/*
Writes a variable length unsigned integer.
*/
static unsigned char *write_vlui(unsigned char *pb, unsigned int i) {
  unsigned char c;
  do {
    c = i & 127;
    i >>= 7;
    if (i != 0) {
      c |= 128;
    }
    *pb = c;
    pb++;
  } while (i != 0);
  return pb;
}

/*
Reads a variable length unsigned integer.
*/
static unsigned char *read_vlui(unsigned char *pb, unsigned int *pi) {
  unsigned int i = 0;
  unsigned int s = 0;
  unsigned char uc;
  do {
    if (s >= 64) {
      return NULL;
    }
    uc = *pb;
    pb++;
    i |= (uc & 127) << s;
    s += 7;
  } while (uc & 128);
  if (pi != NULL) {
    *pi = i;
  }
  return pb;
}

static const unsigned char crc_lookup[] = {
  0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
  0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
  0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
  0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
  0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
  0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
  0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
  0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
  0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
  0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
  0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
  0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
  0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
  0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
  0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
  0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3,
};

/*
Returns a CRC.
*/
static unsigned char compute_crc(unsigned char *pb, int l) {
  unsigned char crc = 0;
  unsigned char data = 0;
  for (; l-- > 0; pb++) {
    data = (*pb) ^ crc;
    crc = crc_lookup[data];
  }
  //printf("crc %02x\n", crc);
  return crc;
}

/*
Returns a pointer and its associated size or NULL.
*/
static const char *toBuffer(lua_State *l, int i, size_t *len) {
  if (lua_isstring(l, i)) {
    return lua_tolstring(l, i, len);
  } else if (lua_isuserdata(l, i) && !lua_islightuserdata(l, i)) {
    *len = lua_rawlen(l, i);
    return (const char *)lua_touserdata(l, i);
  }
  return NULL;
}

typedef void * (*buffer_Alloc) (lua_State *l, size_t s);

static void *buffer_alloc_ud(lua_State *l, size_t s) {
#if LUA_VERSION_NUM < 504
  return lua_newuserdata(l, s);
#else
  return lua_newuserdatauv(l, s, 0);
#endif
}

static void *buffer_alloc_malloc(lua_State *l, size_t s) {
  void *p = malloc(s);
  lua_pushlightuserdata(l, p);
  return p;
}

static int buffer_create(lua_State *l, buffer_Alloc alloc) {
  size_t nbytes = 0;
  unsigned char *buffer = NULL;
  const char *src = NULL;
  if (lua_isinteger(l, 1)) {
    nbytes = luaL_checkinteger(l, 1);
  } else {
    src = toBuffer(l, 1, &nbytes);
  }
  if (nbytes > 0) {
    buffer = (unsigned char *)alloc(l, nbytes);
    if (src != NULL) {
      memcpy(buffer, src, nbytes);
    }
  } else {
    lua_pushnil(l);
  }
  return 1;
}

/*
Returns a new user data from the specified size, string or user data.
*/
static int buffer_new(lua_State *l) {
  return buffer_create(l, buffer_alloc_ud);
}

/*
Returns a new light user data from the specified size, string or user data.
*/
static int buffer_malloc(lua_State *l) {
  return buffer_create(l, buffer_alloc_malloc);
}

/*
Frees the specified light user data.
*/
static int buffer_free(lua_State *l) {
  void *p = lua_touserdata(l, 1);
  if (p != NULL && lua_islightuserdata(l, 1)) {
    free(p);
  }
  return 0;
}

/*
Re-allocate the specified light user data.
*/
static int buffer_realloc(lua_State *l) {
  void *p = lua_touserdata(l, 1);
  lua_Integer s = lua_tointeger(l, 2);
  if (!lua_islightuserdata(l, 1)) {
    luaL_error(l, "light user data expected");
  }
  if (s < 0) {
    luaL_error(l, "invalid negative size");
  }
  p = realloc(p, (size_t)s);
  lua_pushlightuserdata(l, p);
  return 1;
}

/*
Returns the length of the specified user data or 0.
*/
static int buffer_len(lua_State *l) {
  size_t blen = lua_rawlen(l, 1);
  lua_pushinteger(l, blen);
  return 1;
}

/*
Returns the specified user data as a light user data.
*/
static int buffer_lighten(lua_State *l) {
  void *p = lua_touserdata(l, 1);
  lua_pushlightuserdata(l, p);
  return 1;
}

#define NAME_MAX_SIZE 127
#define POINTER_MAX_SIZE (sizeof(void *) + sizeof(size_t) + sizeof(pid_t) + 1 + NAME_MAX_SIZE)

/*
Returns a reference string representing the specified user data.
*/
static int buffer_toreference(lua_State *l) {
  unsigned char b[POINTER_MAX_SIZE];
  unsigned char *pb = (unsigned char *) b;
  size_t bs;
  size_t len = lua_rawlen(l, 1);
  const void *p = lua_touserdata(l, 1);
  lua_Integer s = luaL_optinteger(l, 2, len);
  const char *n = luaL_optstring(l, 3, NULL);
  if (s < 0) {
    luaL_error(l, "invalid negative size");
  }
  if (len > 0 && s > (lua_Integer) len) {
    luaL_error(l, "size out of range");
  }
  if (n == NULL && len > 0 && lua_getmetatable(l, 1)) {
    lua_pushstring(l, "__name");
    lua_rawget(l, -2);
    n = lua_tostring(l, -1);
    lua_pop(l, 2);
  }
  bs = sizeof(void *);
  memcpy(pb, &p, bs);
  pb += bs;
  pb = write_vlui(pb, (unsigned int) s);
  pb = write_vlui(pb, (unsigned int) getpid());
  *pb = compute_crc(b, pb - b);
  pb++;
  // TODO Add a one byte CRC
  if (n != NULL) {
    bs = strlen(n);
    if (bs > 0) {
      if (bs > NAME_MAX_SIZE) {
        luaL_error(l, "name too long");
      }
      memcpy(pb, n, bs);
      pb += bs;
    }
  }
  lua_pushlstring(l, (char *) b, pb - b);
  return 1;
}

/*
Returns a light user data corresponding to the specified reference string.
*/
static int buffer_fromreference(lua_State *l) {
  size_t len = 0;
  const char *p = luaL_optlstring(l, 1, NULL, &len);
  char *pb = (char *) p;
  lua_Integer s = luaL_optinteger(l, 2, 0);
  const char *n = luaL_optstring(l, 3, NULL);
  unsigned int i = 0;
  unsigned int rp = 0;
  size_t sl = 0;
  pid_t pid = getpid();
  unsigned char sc;
  unsigned char crc;
  if (len <= sizeof(void *)) {
    luaL_error(l, "invalid reference length");
  }
  pb += sizeof(void *);
  pb = (char *) read_vlui((unsigned char *) pb, &i);
  if (pb == NULL) {
    luaL_error(l, "invalid reference size");
  }
  pb = (char *) read_vlui((unsigned char *) pb, &rp);
  if (pb == NULL) {
    luaL_error(l, "invalid reference process");
  }
  crc = compute_crc((unsigned char *) p, pb - p);
  sc = *pb;
  pb++;
  if (crc != sc) {
    luaL_error(l, "bad CRC");
  }
  if (s > 0 && s != i) {
    luaL_error(l, "size does not match");
  }
  if (pid != (pid_t) rp) {
    luaL_error(l, "invalid process id");
  }
  sl = len - (pb - p);
  if (n != NULL && memcmp(n, pb, sl) != 0) {
    luaL_error(l, "name does not match");
  }
  lua_pushlightuserdata(l, *((void **) p));
  lua_pushinteger(l, i);
  lua_pushlstring(l, pb, sl);
  return 3;
}

/*
Returns the bytes.
*/
static int buffer_byte(lua_State *l) {
  size_t blen = lua_rawlen(l, 1); // 0 when light
  unsigned char *buffer = (unsigned char *)lua_touserdata(l, 1);
  size_t i = posrelatI(luaL_optinteger(l, 2, 1), blen);
  size_t j = getunsafeendpos(l, 3, i, blen);
  size_t ii;
  int n = j - i + 1;
  if (j <= 0 || n <= 0) {
    return 0;
  }
  if (n >= INT_MAX) {
    return luaL_error(l, "bytes interval too long");
  }
  luaL_checkstack(l, n, "bytes interval too long");
  for (ii = i - 1; ii < j; ii++) {
    lua_pushinteger(l, buffer[ii]);
  }
  return n;
}

/*
Sets the bytes in the buffer.
*/
static int buffer_byteset(lua_State *l) {
  int narg = lua_gettop(l);
  size_t blen = lua_rawlen(l, 1);
  unsigned char *buffer = (unsigned char *)lua_touserdata(l, 1);
  size_t i = unsafeposrelatI(luaL_checkinteger(l, 2), blen);
  int arg, offset;
  if (i <= 0 || (blen > 0 && (i + narg - 3 > blen))) {
    return 0;
  }
  offset = i - 1;
  for (arg = 3; arg <= narg; arg++) {
    lua_Integer c = luaL_checkinteger(l, arg);
    unsigned char b = c;
    luaL_argcheck(l, b == c, arg, "value out of range");
    buffer[offset] = b;
    offset++;
  }
  return 0;
}

/*
Fills the buffer with a specified byte value.
*/
static int buffer_memset(lua_State *l) {
  size_t blen = lua_rawlen(l, 1);
  unsigned char *buffer = (unsigned char *)lua_touserdata(l, 1);
  lua_Integer value = luaL_optinteger(l, 2, 0);
  size_t i = posrelatI(luaL_optinteger(l, 3, 1), blen);
  size_t j = getunsafeendpos(l, 4, -1, blen);
  unsigned char byteValue = value;
  luaL_argcheck(l, byteValue == value, 2, "value out of range");
  if (j > 0 && i <= j) {
    memset(buffer + i - 1, byteValue, j - i + 1);
  }
  return 0;
}

/*
Copies the string to the buffer.
*/
static int buffer_memcpy(lua_State *l) {
  size_t blen = lua_rawlen(l, 1);
  unsigned char *buffer = (unsigned char *)lua_touserdata(l, 1);
  size_t slen = 0;
  const char *src = toBuffer(l, 2, &slen);
  size_t k = unsafeposrelatI(luaL_optinteger(l, 3, 1), blen);
  size_t i = posrelatI(luaL_optinteger(l, 4, 1), slen);
  size_t j = getunsafeendpos(l, 5, -1, slen);
  if (src != NULL && slen > 0 && k > 0 && j > 0 && i <= j) {
    size_t clen = j - i + 1;
    if (blen > 0 && k - 1 + clen > blen) {
      clen = blen - k + 1;
    }
    memcpy(buffer + k - 1, src + i - 1, clen);
  }
  return 0;
}

/*
Returns the specified user data as a string.
*/
static int buffer_sub(lua_State *l) {
  size_t blen = lua_rawlen(l, 1);
  char *buffer = (char *)lua_touserdata(l, 1);
  size_t i = posrelatI(luaL_optinteger(l, 2, 1), blen);
  size_t j = getunsafeendpos(l, 3, -1, blen);
  if (j > 0 && i <= j) {
    lua_pushlstring(l, buffer + i - 1, j - i + 1);
  } else {
    lua_pushstring(l, "");
  }
  return 1;
}

#ifndef NO_MUTEX
#include "mutex.c"
#endif

LUALIB_API int luaopen_buffer(lua_State *l) {
  luaL_Reg reg[] = {
    { "new", buffer_new },
    { "malloc", buffer_malloc },
    { "realloc", buffer_realloc },
    { "free", buffer_free },
    { "len", buffer_len },
    { "lighten", buffer_lighten },
    { "toreference", buffer_toreference },
    { "fromreference", buffer_fromreference },
    { "byte", buffer_byte },
    { "byteset", buffer_byteset },
    { "memset", buffer_memset },
    { "memcpy", buffer_memcpy },
    { "sub", buffer_sub },
#ifndef NO_MUTEX
    { "newmutex", mutex_new },
    { "initmutex", mutex_init },
    { "destroymutex", mutex_destroy },
    { "lock", mutex_lock },
    { "unlock", mutex_unlock },
    { "trylock", mutex_trylock },
#endif
    { NULL, NULL }
  };
  lua_newtable(l);
  luaL_setfuncs(l, reg, 0);
  lua_pushliteral(l, "Lua buffer");
  lua_setfield(l, -2, "_NAME");
  lua_pushliteral(l, "0.3");
  lua_setfield(l, -2, "_VERSION");
#ifndef NO_MUTEX
  lua_pushinteger(l, sizeof(mutex_t));
  lua_setfield(l, -2, "MUTEX_SIZE");
#endif
  return 1;
}
