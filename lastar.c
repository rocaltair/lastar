#include <lua.h>
#include <lauxlib.h>
#include <math.h>
#include <strings.h>
#include <stdint.h>
#include "AStar.h"

#define ASTAR_MAP "astar{map}"
#define IS_SET(bits, i) (bits[i/sizeof(uint32_t)] & (1 << (i%sizeof(uint32_t))))
#define SET_SET(bits, i) bits[i/sizeof(uint32_t)] |= (1 << (i%sizeof(uint32_t)))
#define check_map(L, i) (*(map_t **)luaL_checkudata(L, i, ASTAR_MAP))

#if LUA_VERSION_NUM < 502
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
#endif

typedef struct map_s {
	int w;
	int h;
	uint32_t bits[0];
} map_t;

typedef struct {
	int x;
	int y;
} pathnode_t;

static int world_at(map_t * map, int x, int y)
{
	int i;
	if (x >= 0 && x < map->w && y >= 0 && y < map->h) {
		i = y * map->h + x;
		return IS_SET(map->bits, i);
	}
	return -1;
}

static void PathNodeNeighbors(ASNeighborList neighbors, void *node, void *context)
{
	int i,j;
	float dis;
	pathnode_t *pathNode = (pathnode_t *)node;
	map_t *map = (map_t *)context;
	for (i=-1; i<=1; i++) {
		for (j=-1; j<=1; j++) {
			if (i==0 && j == 0) {
				continue;
			}
			if (world_at(map, pathNode->x+i, pathNode->y+j) == 0) {
				pathnode_t add = {pathNode->x+i, pathNode->y+j};
				dis = (i * j != 0) ? 1.4 : 1;
				ASNeighborListAdd(neighbors, &add, dis);
			}
		}
	}
}

static int lua__new_map(lua_State *L)
{
	int w,h;
	int sz;
	map_t *map;
	map_t **pmap;
	if (!lua_istable(L, 1)) {
		return luaL_error(L, "#1 table required in %s", __FUNCTION__);
	}
	w = luaL_checkinteger(L, 2);
	h = luaL_checkinteger(L, 3);
	sz = (w * h) / sizeof(uint32_t) + 1;
	map = malloc(sizeof(map_t) + sz * sizeof(uint32_t));
	if (map == NULL) {
		luaL_error(L, "create map failed, no memory");
		return 0;
	}
	map->w = w;
	map->h = h;
	bzero(map->bits, sz);
	

	lua_pushnil(L);  /* first key */
	while (lua_next(L, 1) != 0) {
		int i = lua_tointeger(L, -2);
		uint32_t v = (uint32_t)lua_tointeger(L, -1);
		map->bits[i - 1] = v;
		lua_pop(L, 1);
	}

	pmap = lua_newuserdata(L, sizeof(void *));
	*pmap = map;
	luaL_getmetatable(L, ASTAR_MAP);
	lua_setmetatable(L, -2);
	return 1;
}

static float PathNodeHeuristic(void *fromNode, void *toNode, void *context)
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
	ASPathNodeSource PathNodeSource = {
		sizeof(pathnode_t),
		&PathNodeNeighbors,
		&PathNodeHeuristic,
		NULL,
		NULL
	};
	ASPath path;
	pathnode_t from;
	pathnode_t to;
	int i;
	int sz;

	map_t *map = check_map(L, 1);
	from.x = luaL_checkinteger(L, 2) - 1;
	from.y = luaL_checkinteger(L, 3) - 1;
	to.x = luaL_checkinteger(L, 4) - 1;
	to.y = luaL_checkinteger(L, 5) - 1;
	if (from.x < 0 || from.x >= map->w || from.y < 0 || from.y >= map->h) {
		return luaL_error(L, "from point error!");
	}
	if (to.x < 0 || to.x >= map->w || to.y < 0 || to.y >= map->h) {
		return luaL_error(L, "to point error!");
	}

	path = ASPathCreate(&PathNodeSource, map, &from, &to);

	sz = ASPathGetCount(path);
	lua_newtable(L);
	for (i=0; i<sz; i++) {
		pathnode_t *pathNode = ASPathGetNode(path, i);
		lua_newtable(L);
		lua_pushinteger(L, pathNode->x + 1);
		lua_setfield(L, -2, "x");
		lua_pushinteger(L, pathNode->y + 1);
		lua_setfield(L, -2, "y");
		lua_rawseti(L, -2, i + 1);
	}
	lua_pushnumber(L, ASPathGetCost(path));
	ASPathDestroy(path);
	return 2;
}

static int lua__gc(lua_State *L)
{
	map_t *map = check_map(L, 1);
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

