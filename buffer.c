#include <lua.h>
#include <lauxlib.h>
#include <string.h>

/*
Provides functions to deal with userdata as plain byte buffer.
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

static int adjustOffset(lua_Integer offset, size_t length) {
	if (offset < 0) {
		offset = length + 1 + offset;
		if (offset < 1) {
			return 0;
		}
	}
	return offset;
}


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

static int buffer_len(lua_State *l) {
	size_t blen = lua_rawlen(l, 1);
	lua_pushinteger(l, blen);
	return 1;
}

static int buffer_tostring(lua_State *l) {
	if (lua_isuserdata(l, 1) && !lua_islightuserdata(l, 1)) {
		size_t blen = lua_rawlen(l, 1);
		char *buffer = (char *)lua_touserdata(l, 1);
		lua_pushlstring(l, buffer, blen);
	} else {
		lua_pushnil(l);
	}
    return 1;
}

static int buffer_byte(lua_State *l) {
	size_t blen = lua_rawlen(l, 1);
	unsigned char *buffer = (unsigned char *)lua_touserdata(l, 1);
	lua_Integer i = adjustOffset(luaL_optinteger(l, 2, 1), blen);
	lua_Integer j = adjustOffset(luaL_optinteger(l, 3, i), blen);
	int n, ii;
	if (i < 1) {
		i = 1;
	}
	if (j > (lua_Integer)blen) {
		j = blen;
	}
	if (i > j) {
		return 0;
	}
	if (j - i >= INT_MAX) {
		return luaL_error(l, "bytes interval too long");
	}
	n = (int)(j -  i) + 1;
	luaL_checkstack(l, n, "bytes interval too long");
	for (ii = i - 1; ii < j; ii++) {
		lua_pushinteger(l, buffer[ii]);
	}
	return n;
}

static int buffer_byteset(lua_State *l) {
	int n = lua_gettop(l);
	size_t blen = lua_rawlen(l, 1);
	unsigned char *buffer = (unsigned char *)lua_touserdata(l, 1);
	lua_Integer offset = adjustOffset(luaL_optinteger(l, 2, 1), blen);
	int i;
	for (i = 3; i <= n; i++) {
		lua_Integer c = luaL_checkinteger(l, i);
		unsigned char b = c;
		luaL_argcheck(l, b == c, i, "value out of range");
		buffer[offset + i - 4] = b;
	}
	return 0;
}

static int buffer_memset(lua_State *l) {
	size_t blen = lua_rawlen(l, 1);
	unsigned char *buffer = (unsigned char *)lua_touserdata(l, 1);
	lua_Integer value = luaL_checkinteger(l, 2);
	unsigned char byteValue = value;
	luaL_argcheck(l, byteValue == value, 2, "value out of range");
	memset(buffer, byteValue, blen);
	return 0;
}

static int buffer_memcpy(lua_State *l) {
	size_t blen = lua_rawlen(l, 1);
	unsigned char *buffer = (unsigned char *)lua_touserdata(l, 1);
	lua_Integer offset = adjustOffset(luaL_optinteger(l, 2, 1), blen);
	size_t slen = 0;
	const char *src = toBuffer(l, 3, &slen);
	lua_Integer i = adjustOffset(luaL_optinteger(l, 4, 1), slen);
	lua_Integer j = adjustOffset(luaL_optinteger(l, 5, slen), slen);
	if (i < 1) {
		i = 1;
	}
	if (j > (lua_Integer)blen) {
		j = blen;
	}
	int ii = i - 1;
	int n = j - ii;
	if ((src != NULL) && (offset >= 1) && (offset <= blen) && (i <= j) && (offset + n <= blen)) {
		memcpy(buffer + offset - 1, src + ii, n);
	}
	return 0;
}

static int buffer_sub(lua_State *l) {
	size_t blen = lua_rawlen(l, 1);
	char *buffer = (char *)lua_touserdata(l, 1);
	lua_Integer i = adjustOffset(luaL_optinteger(l, 2, 1), blen);
	lua_Integer j = adjustOffset(luaL_optinteger(l, 3, blen), blen);
	if (i < 1) {
		i = 1;
	}
	if (j > (lua_Integer)blen) {
		j = blen;
	}
	if (i > j) {
		lua_pushstring(l, "");
		return 1;
	}
	int ii = i - 1;
	lua_pushlstring(l, buffer + ii, (size_t)(j - ii));
    return 1;
}

LUALIB_API int luaopen_buffer(lua_State *l) {
	luaL_Reg reg[] = {
		{ "new", buffer_new },
		{ "len", buffer_len },
		{ "tostring", buffer_tostring },
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
	lua_pushliteral(l, "0.1");
	lua_setfield(l, -2, "_VERSION");
	return 1;
}
