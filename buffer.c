#include <lua.h>
#include <lauxlib.h>
#include <string.h>

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

/*
Returns a new user data from the specified size, string or user data.
*/
static int buffer_new(lua_State *l) {
  size_t nbytes = 0;
  unsigned char *buffer = NULL;
  const char *src = NULL;
  if (lua_isinteger(l, 1)) {
    nbytes = luaL_checkinteger(l, 1);
  } else {
    src = toBuffer(l, 1, &nbytes);
  }
  if (nbytes > 0) {
    buffer = (unsigned char *)lua_newuserdata(l, nbytes);
    if (src != NULL) {
      memcpy(buffer, src, nbytes);
    }
  } else {
    lua_pushnil(l);
  }
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

/*
Returns a pointer string representing the specified user data.
*/
static int buffer_topointer(lua_State *l) {
  const void *p = lua_touserdata(l, 1);
   lua_pushlstring(l, (const char *) &p, sizeof(void *));
  return 1;
}

/*
Returns a light user data corresponding to the specified pointer string.
*/
static int buffer_frompointer(lua_State *l) {
  size_t len = 0;
  const char *p = luaL_optlstring(l, 1, NULL, &len);
  if (len == sizeof(void *)) {
    lua_pushlightuserdata(l, *((void **) p));
    return 1;
  }
  return 0;
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
    memcpy(buffer + k - 1, src + i - 1, j - i + 1);
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

LUALIB_API int luaopen_buffer(lua_State *l) {
  luaL_Reg reg[] = {
    { "new", buffer_new },
    { "len", buffer_len },
    { "lighten", buffer_lighten },
    { "topointer", buffer_topointer },
    { "frompointer", buffer_frompointer },
    { "byte", buffer_byte },
    { "byteset", buffer_byteset },
    { "memset", buffer_memset },
    { "memcpy", buffer_memcpy },
    { "sub", buffer_sub },
    { NULL, NULL }
  };
  lua_newtable(l);
  luaL_setfuncs(l, reg, 0);
  lua_pushliteral(l, "Lua buffer");
  lua_setfield(l, -2, "_NAME");
  lua_pushliteral(l, "0.3");
  lua_setfield(l, -2, "_VERSION");
  return 1;
}
