#include <lua.h>
#include <lauxlib.h>
#include <math.h>
#include <stdint.h>
#include "AStar.h"

#define ASTAR_MAP "astar{map}"
#define check_map(L, i) (*(map_t **)luaL_checkudata(L, i, ASTAR_MAP))

#if LUA_VERSION_NUM < 502
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
#endif

#define REF_MAP "astar{ref}"

typedef struct map_s {
	int w;
	int h;
	int ref;
	void *udata;
} map_t;

typedef struct {
	int x;
	int y;
} pathnode_t;

static int getfuncbyref(lua_State *L, const char * module, int ref)
{
	lua_pushstring(L, module);
	lua_rawget(L, LUA_REGISTRYINDEX);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return -1;
	}
	lua_rawgeti(L, -1, ref);
	lua_insert(L, -2);
	lua_pop(L, 1);
	return 0;
}

static int unreffunc(lua_State *L, const char * module, int ref)
{
	lua_pushstring(L, module);
	lua_rawget(L, LUA_REGISTRYINDEX);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return -1;
	}
	lua_pushnil(L);
	lua_rawseti(L, -2, ref);
	lua_pop(L, 1);
	return 0;
}

static int pushfunc(lua_State *L, const char * module, int funcidx)
{
	int ref;
	if (funcidx < 0) {
		funcidx = lua_gettop(L) + 1 + funcidx;
	}
	lua_pushstring(L, module);
	lua_rawget(L, LUA_REGISTRYINDEX);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1); 
		lua_pushstring(L, module);
		lua_newtable(L);
		lua_rawset(L, LUA_REGISTRYINDEX);
		lua_pushstring(L, module);
		lua_rawget(L, LUA_REGISTRYINDEX);
	}
	lua_pushvalue(L, funcidx);
	ref = luaL_ref(L, -2);
	lua_pop(L, 1);
	return ref;
}

static int get_neighbors(lua_State *L, int ref,
			 int w, int h,
			 int sx, int sy,
			 int ex, int ey,
			 float *dis)
{
	int ret;
	int lret;
	int rtop;
	int top = lua_gettop(L);
	if (getfuncbyref(L, REF_MAP, ref) != 0) {
		return -1;
	}
	lua_pushinteger(L, w);
	lua_pushinteger(L, h);
	lua_pushinteger(L, sx);
	lua_pushinteger(L, sy);
	lua_pushinteger(L, ex);
	lua_pushinteger(L, ey);
	ret = lua_pcall(L, 6, LUA_MULTRET, 0);
	if (ret != 0) {
		lua_settop(L, top);
		return -1;
	}
	rtop = lua_gettop(L);
	if (rtop <= top) {
		return -1;
	}
	lret = lua_toboolean(L, top+1);
	if (rtop - top >= 2) {
		*dis = (float)lua_tonumber(L, top+2);
	}
	lua_settop(L, top);
	return !lret;
}

static void path_neighbors(ASNeighborList neighbors, void *node, void *context)
{
	int i,j;
	int sx,sy;
	int w, h;
	float dis;
	pathnode_t *pathNode = (pathnode_t *)node;
	map_t *map = (map_t *)context;
	lua_State *L = (lua_State *)map->udata;
	
	w = map->w;
	h = map->h;

	sx = pathNode->x;
	sy = pathNode->y;

	for (i=-1; i<=1; i++) {
		for (j=-1; j<=1; j++) {
			int tx = sx + i;
			int ty = sy + j;
			if ( ( i == 0 && j == 0) || tx > w || tx <= 0 || ty > h || ty <= 0) {
				continue;
			}
			if (get_neighbors(L, map->ref, w, h, sx, sy, tx, ty, &dis) == 0) {
				pathnode_t add = {tx, ty};
				ASNeighborListAdd(neighbors, &add, dis);
			}
		}
	}
}

static int lua__new_map(lua_State *L)
{
	int w,h;
	map_t *map;
	map_t **pmap;
	w = luaL_checkinteger(L, 1);
	h = luaL_checkinteger(L, 2);

	if (!lua_isfunction(L, 3)) {
		return luaL_typerror(L, 3, "function");
	}
	
	map = malloc(sizeof(*map));
	if (map == NULL) {
		luaL_error(L, "create map failed, no memory");
		return 0;
	}
	map->w = w;
	map->h = h;

	pmap = lua_newuserdata(L, sizeof(void *));
	*pmap = map;
	luaL_getmetatable(L, ASTAR_MAP);
	lua_setmetatable(L, -2);

	map->ref = pushfunc(L, REF_MAP, 3);

	return 1;
}

static float path_heuristic(void *fromNode, void *toNode, void *context)
{
	pathnode_t *from = (pathnode_t *)fromNode;
	pathnode_t *to = (pathnode_t *)toNode;
	float absX = fabs(from->x - to->x);
	float absY = fabs(from->y - to->y);
	// max + sqrt(min * min) - min
	if (absX < absY) {
		return absY + 0.4 * absX;
	}
	return absX + 0.4 * absY;
}

static int lua__path(lua_State *L)
{
	ASPathNodeSource pathnode_source = {
		sizeof(pathnode_t),
		&path_neighbors,
		&path_heuristic,
		NULL,
		NULL
	};
	ASPath path;
	pathnode_t from;
	pathnode_t to;
	int i;
	int sz;

	map_t *map = check_map(L, 1);
	map->udata = L;
	from.x = luaL_checkinteger(L, 2);
	from.y = luaL_checkinteger(L, 3);
	to.x = luaL_checkinteger(L, 4);
	to.y = luaL_checkinteger(L, 5);
	if (from.x <= 0 || from.x > map->w || from.y <= 0 || from.y > map->h) {
		return luaL_error(L, "from point error!");
	}
	if (to.x <= 0 || to.x > map->w || to.y <= 0 || to.y > map->h) {
		return luaL_error(L, "to point error!");
	}

	path = ASPathCreate(&pathnode_source, map, &from, &to);

	sz = ASPathGetCount(path);
	lua_newtable(L);
	for (i=0; i<sz; i++) {
		pathnode_t *pathNode = ASPathGetNode(path, i);
		lua_newtable(L);

		lua_pushinteger(L, pathNode->x);
		lua_rawseti(L, -2, 1);
		lua_pushinteger(L, pathNode->y);
		lua_rawseti(L, -2, 2);

		lua_rawseti(L, -2, i + 1);
	}
	lua_pushnumber(L, ASPathGetCost(path));
	ASPathDestroy(path);
	return 2;
}

static int lua__gc(lua_State *L)
{
	map_t *map = check_map(L, 1);
	unreffunc(L, REF_MAP, map->ref);
	free(map);
	return 0;
}


static int opencls__map(lua_State *L)
{
	luaL_Reg lmethods[] = {
		{"path", lua__path},
		{NULL, NULL},
	};
	luaL_newmetatable(L, ASTAR_MAP);
	lua_newtable(L);
	luaL_register(L, NULL, lmethods);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction (L, lua__gc);
	lua_setfield (L, -2, "__gc");
	return 1;
}

int luaopen_lastar(lua_State* L)
{
	luaL_Reg lfuncs[] = {
		{"new", lua__new_map},
		{NULL, NULL},
	};
	opencls__map(L);
	luaL_newlib(L, lfuncs);
	return 1;
}

