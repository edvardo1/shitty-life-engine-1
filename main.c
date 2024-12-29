#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdint.h>
#include <time.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_hints.h>

typedef unsigned int uint;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

void *
my_malloc(size_t size) {
	void *ret = malloc(size);
	assert(ret != NULL);
	return ret;
}
#define malloc my_malloc

SDL_Window *win = NULL;
SDL_Renderer *ren = NULL;
SDL_Event ev;

void
sdl_set_color(u32 color) {
	SDL_SetRenderDrawColor(ren,
			       (color >> 24) & 0xff,
			       (color >> 16) & 0xff,
			       (color >>  8) & 0xff,
			       (color >>  0) & 0xff);
}

typedef enum {
	None,
	Producer,
	Mover,
	Eater,
	CellTypesN
} CellType;

typedef enum {
	Up,
	Down,
	Left,
	Right,
	DirectionN
} Direction;

typedef enum {
	Empty,
	Wall,
	Food,
	TileTypesN
} TileType;

#define ALIVEGUY_CELLS_W 16
#define ALIVEGUY_CELLS_H 16
typedef struct {
	int x;
	int y;

	int lifetime;
	int hp;
	int food_consumed;

	Direction moving_direction;
	int moving_frames_left;

	CellType cells[ALIVEGUY_CELLS_W * ALIVEGUY_CELLS_H];
} AliveGuy;

typedef struct {
	TileType *tiles;
	int w;
	int h;
} TileMap;

#define RECT_WITH_GUYS_W 16
#define RECT_WITH_GUYS_H 16
typedef struct {
	int amount;
	int indices[64];
} RectWithGuys;

#define GUYS_N 2048
typedef struct {
	int alives;
	AliveGuy guys[GUYS_N];

	RectWithGuys *rects_with_guys;
	TileMap *map;

	int mutation_chance_percent;
} Game;

int game_get_rects_with_guys_w(Game *game);
int game_get_rects_with_guys_h(Game *game);
void aliveguy_init(AliveGuy *guy);
CellType aliveguy_get_cell(AliveGuy *guy, int x, int y);
void aliveguy_set_cell(AliveGuy *guy, int x, int y, CellType cell);
u32 get_cell_color(CellType t);
void aliveguy_render(AliveGuy *guy);
int aliveguy_cells_amount(AliveGuy *guy);
int aliveguy_starting_x(AliveGuy *guy);
int aliveguy_starting_y(AliveGuy *guy);
int aliveguy_ending_x(AliveGuy *guy);
int aliveguy_ending_y(AliveGuy *guy);
void cell_update(CellType cell, int x, int y, AliveGuy *guy, int index, Game *game);
void aliveguy_calculate_new_lifetime(AliveGuy *guy);
int aliveguy_food_needed_to_reproduce(AliveGuy *guy);
int aliveguy_occupies_point(AliveGuy *guy, int x, int y);
int game_is_point_vacant(Game *game, int x, int y);
int aliveguy_is_spot_vacant(AliveGuy *guy, int x, int y, Game *game);
void game_aliveguy_register_birth(Game *game, AliveGuy *aliveguy);
void aliveguy_guy_mutate(AliveGuy *guy, Game *game);
void aliveguy_birth(AliveGuy *guy, int x, int y, Game *game);
int aliveguy_try_reproduce(AliveGuy *guy, int index, Game *game);
void aliveguy_tostring(AliveGuy *guy);
void aliveguy_update(AliveGuy *guy, int index, Game *game);
TileMap * make_tilemap(int w, int h);
TileType * tilemap_get_tile_ptr(TileMap *map, int x, int y);
TileType tilemap_get_tile(TileMap *map, int x, int y);
void tilemap_set_tile(TileMap *map, int x, int y, TileType t);
u32 get_tile_color(TileType t);
void tilemap_render(TileMap *map);
void game_init(Game *game);
void game_render(Game *game);
AliveGuy * game_new_aliveguy(Game *game);
void game_update(Game *game);

int
game_get_rects_with_guys_w(Game *game) {
	return game->map->w / RECT_WITH_GUYS_W;
}

int
game_get_rects_with_guys_h(Game *game) {
	return game->map->h / RECT_WITH_GUYS_H;
}

void
aliveguy_init(AliveGuy *guy) {
	guy->x = 0;
	guy->y = 0;
	guy->lifetime = 0;
	guy->hp = 0;
	guy->food_consumed = 0;
	guy->moving_direction = Left;
	guy->moving_frames_left = 0;

	for (int i = 0; i < ALIVEGUY_CELLS_W * ALIVEGUY_CELLS_H; i++) {
		guy->cells[i] = None;
	}
}

CellType
aliveguy_get_cell(AliveGuy *guy, int x, int y) {
	assert(x < ALIVEGUY_CELLS_W && y < ALIVEGUY_CELLS_H);
	assert(0 <= x && 0 <= y);
	return guy->cells[y * ALIVEGUY_CELLS_W + x];
}

void
aliveguy_set_cell(AliveGuy *guy, int x, int y, CellType cell) {
	assert(x < ALIVEGUY_CELLS_W && y < ALIVEGUY_CELLS_H);
	assert(0 <= x && 0 <= y);
	guy->cells[y * ALIVEGUY_CELLS_W + x] = cell;
}

u32
get_cell_color(CellType t) {
	switch (t) {
	case None : assert(0);
	case Producer : return 0x55cc55ff;
	case Eater    : return 0xdead00ff;
	case Mover    : return 0x00deadff;
	default : assert(0);
	}
	assert(0);
}

void
aliveguy_render(AliveGuy *guy) {
	for (int y = 0; y < ALIVEGUY_CELLS_H; y++) {
		for (int x = 0; x < ALIVEGUY_CELLS_W; x++) {
			CellType cell = aliveguy_get_cell(guy, x, y);

			if (cell == None) {
				continue;
			}

			u32 color = get_cell_color(cell);
			sdl_set_color(color);
			SDL_FRect rect = {
				(guy->x + x) * 10,
				(guy->y + y) * 10,
				10 - 1,
				10 - 1
			};
			SDL_RenderFillRect(ren, &rect);
		}
	}
}

int
aliveguy_cells_amount(AliveGuy *guy) {
	int ret = 0;

	for (int y = 0; y < ALIVEGUY_CELLS_H; y++) {
		for (int x = 0; x < ALIVEGUY_CELLS_W; x++) {
			CellType cell = aliveguy_get_cell(guy, x, y);

			if (cell != None) {
				ret += 1;
			}
		}
	}

	return ret;
}

int
aliveguy_starting_x(AliveGuy *guy) {
	for (int y = 0; y < ALIVEGUY_CELLS_H; y++) {
		for (int x = 0; x < ALIVEGUY_CELLS_W; x++) {
			CellType cell = aliveguy_get_cell(guy, x, y);

			if (cell != None) {
				return x;
			}
		}
	}

	assert(0);
}

int
aliveguy_starting_y(AliveGuy *guy) {
	for (int y = 0; y < ALIVEGUY_CELLS_H; y++) {
		for (int x = 0; x < ALIVEGUY_CELLS_W; x++) {
			CellType cell = aliveguy_get_cell(guy, x, y);

			if (cell != None) {
				return y;
			}
		}
	}

	assert(0);
}

int
aliveguy_ending_x(AliveGuy *guy) {
	for (int y = ALIVEGUY_CELLS_H - 1; y >= 0 ; y--) {
		for (int x = ALIVEGUY_CELLS_W - 1; x >= 0 ; x--) {
			CellType cell = aliveguy_get_cell(guy, x, y);

			if (cell != None) {
				return x;
			}
		}
	}

	assert(0);
}

int
aliveguy_ending_y(AliveGuy *guy) {
	for (int y = ALIVEGUY_CELLS_H - 1; y >= 0 ; y--) {
		for (int x = ALIVEGUY_CELLS_W - 1; x >= 0 ; x--) {
			CellType cell = aliveguy_get_cell(guy, x, y);

			if (cell != None) {
				return y;
			}
		}
	}

	assert(0);
}

void
cell_update(CellType cell, int x, int y, AliveGuy *guy, int index, Game *game) {
	TileMap *t = game->map;

	struct pt { int x; int y; };
	struct pt arr[4] = {
		{guy->x + x - 1, guy->y + y    },
		{guy->x + x + 1, guy->y + y    },
		{guy->x + x    , guy->y + y - 1},
		{guy->x + x    , guy->y + y + 1}
	};

	switch (cell) {
	case None : assert(0);
	case Producer : {
		if ((rand() % 2) < 1) {
			return;
		}

		TileType *ptr;
		for(int i = 0; i < 4; i++) {
			ptr = tilemap_get_tile_ptr(t, arr[i].x, arr[i].y);
			if (ptr != NULL && *ptr == Empty && rand() % 2 == 0) {
				*ptr = Food;
			}
		}
	} break;
	case Eater : {
		TileType *ptr;
		for(int i = 0; i < 4; i++) {
			ptr = tilemap_get_tile_ptr(t, arr[i].x, arr[i].y);
			if (ptr != NULL && *ptr == Food) {
				guy->food_consumed += 1;
				*ptr = Empty;
			}
		}
	} break;
	default : break;
	}
}

void
aliveguy_calculate_new_lifetime(AliveGuy *guy) {
	guy->lifetime = aliveguy_cells_amount(guy) * 80;
}

int
aliveguy_food_needed_to_reproduce(AliveGuy *guy) {
	return aliveguy_cells_amount(guy) * 15;
}

int
aliveguy_occupies_point(AliveGuy *guy, int x, int y) {
	// @todo implement boxes

	// @todo remove if never gets done

	if(x < guy->x) {
		return 0;
	}

	if(y < guy->y) {
		return 0;
	}

	if(x > guy->x + ALIVEGUY_CELLS_W) {
		return 0;
	}

	if(y > guy->y + ALIVEGUY_CELLS_H) {
		return 0;
	}

	for (int gy = 0; gy < ALIVEGUY_CELLS_H; gy++) {
		for (int gx = 0; gx < ALIVEGUY_CELLS_W; gx++) {
			CellType cell = aliveguy_get_cell(guy, gx, gy);
			if (cell == None) {
				continue;
			}

			if (guy->x + gx == x &&
			    guy->y + gy == y) {
				return 1;
			}
		}
	}

	return 0;
}

int
game_is_point_vacant(Game *game, int x, int y) {
	TileMap *tm = game->map;
	if (!(0 < x && x < tm->w &&
	      0 < y && y < tm->h)) {
		return 0;
	}

	if (tilemap_get_tile(tm, x, y) != Empty) {
		return 0;
	}

	// @todo implement boxes
	for (int i = 0; i < GUYS_N; i++) {
		AliveGuy *guy = &game->guys[i];
		if(guy->hp > 0) {
			if (aliveguy_occupies_point(guy, x, y)) {
				return 0;
			}
		}
	}

	return 1;
}

int
aliveguy_is_spot_vacant(AliveGuy *guy, int x, int y, Game *game) {
	TileMap *tm = game->map;
	for (int gy = 0; gy < ALIVEGUY_CELLS_H; gy++) {
		for (int gx = 0; gx < ALIVEGUY_CELLS_W; gx++) {
			CellType cell = aliveguy_get_cell(guy, gx, gy);
			if (cell == None) {
				continue;
			}

			int nx = x + gx;
			int ny = y + gy;

			if (!(0 < nx && nx < tm->w &&
			      0 < ny && ny < tm->h)) {
				return 0;
			}

			if (tilemap_get_tile(tm, nx, ny) != Empty) {
				return 0;
			}

			// @todo implement boxes
			if (!game_is_point_vacant(game, nx, ny)) {
				return 0;
			}
		}
	}

	return 1;
}

void
game_aliveguy_register_birth(Game *game, AliveGuy *aliveguy) {
	// @todo do this

	//int top_lef_x = aliveguy->x;
	//int top_lef_y = aliveguy->y;

	//int top_rig_x = aliveguy->x + ALIVEGUY_CELLS_W;
	//int top_rig_y = aliveguy->y;

	//int bot_lef_x = aliveguy->x;
	//int bot_lef_y = aliveguy->y + ALIVEGUY_CELLS_H;

	//int bot_rig_x = aliveguy->x + ALIVEGUY_CELLS_W;
	//int bot_rig_y = aliveguy->y + ALIVEGUY_CELLS_H;
	
}

// game is passed in order to check if the added cell is occupied
void
aliveguy_guy_mutate(AliveGuy *guy, Game *game) {
	enum {
		AddCell,
		ChangeCell,
		RemoveCell,
		Choices
	} choice;
	choice = rand() % Choices;

	struct pt { int x; int y; };
	#define NEI_CELLS_N ALIVEGUY_CELLS_W * ALIVEGUY_CELLS_H
	struct pt neighboring_cells[NEI_CELLS_N];
	int nc_amount = 0;

	TileMap *tm = game->map;

	// get neighboring cells
	for (int y = 0; y < ALIVEGUY_CELLS_H; y++) {
		for (int x = 0; x < ALIVEGUY_CELLS_W; x++) {
			CellType cell;
			cell = aliveguy_get_cell(guy, x, y);
			if (cell == None) {
				continue;
			}

			int out;

			// @todo refactor this into function
#define ADD_IF_NOT_THERE(bx, by)					\
			if (!(0 < guy->x + bx && guy->x + bx < tm->w && \
			      0 < guy->y + by && guy->y + by < tm->h)) { \
				out = 1;				\
			}						\
			if (!(0 < bx && bx < tm->w &&			\
			      0 < by && by < tm->h)) {			\
				out = 1;				\
			}						\
			if (!out) {					\
				int cond1 = game_is_point_vacant(	\
					game, guy->x + bx, guy->y + by); \
				int cond2 =				\
					aliveguy_get_cell(guy, bx, by) == None; \
				if (cond1 && cond2) {			\
					neighboring_cells[nc_amount].x = bx; \
					neighboring_cells[nc_amount].y = by; \
					nc_amount += 1;			\
					assert(nc_amount < NEI_CELLS_N); \
				}					\
			}						\
			out = 0;					\

			ADD_IF_NOT_THERE((x - 1), (y - 1));
			ADD_IF_NOT_THERE((x    ), (y - 1));
			ADD_IF_NOT_THERE((x + 1), (y - 1));
			ADD_IF_NOT_THERE((x - 1), (y    ));
			ADD_IF_NOT_THERE((x + 1), (y    ));
			ADD_IF_NOT_THERE((x - 1), (y + 1));
			ADD_IF_NOT_THERE((x    ), (y + 1));
			ADD_IF_NOT_THERE((x + 1), (y + 1));

			#undef ADD_IF_NOT_THERE
		}
	}

	if (nc_amount == 0) {
		goto END_OF_CHANGES;
	}

	if (choice == AddCell) {
		int index = rand() % nc_amount;
		struct pt randcellpos = neighboring_cells[index];
		CellType ct;
		ct = rand() % CellTypesN;
		aliveguy_set_cell(guy, randcellpos.x, randcellpos.y, ct);
	}

	if (choice == RemoveCell) {
		int amount = aliveguy_cells_amount(guy);
		if (amount == 1) {
			goto OUT_OF_REMOVE_CELL;
		}

		int chosen_cell = rand() % amount;
		int acc = 0;

		for (int y = 0; y < ALIVEGUY_CELLS_H; y++) {
			for (int x = 0; x < ALIVEGUY_CELLS_W; x++) {
				CellType cell;
				cell = aliveguy_get_cell(guy, x, y);
				if (cell == None) {
					continue;
				}

				if (chosen_cell == acc) {
					aliveguy_set_cell(guy, x, y, Empty);
					goto OUT_OF_REMOVE_CELL;
				}
				acc++;
			}
		}
	}
OUT_OF_REMOVE_CELL:
	if (choice == ChangeCell) {
		int amount = aliveguy_cells_amount(guy);
		int chosen_cell = rand() % amount;
		int acc = 0;

		CellType ct;
		ct = rand() % CellTypesN;

		if (ct == None) {
			goto OUT_OF_CHANGE_CELL;
		}

		for (int y = 0; y < ALIVEGUY_CELLS_H; y++) {
			for (int x = 0; x < ALIVEGUY_CELLS_W; x++) {
				CellType cell;
				cell = aliveguy_get_cell(guy, x, y);
				if (cell == None) {
					continue;
				}

				if (chosen_cell == acc) {
					aliveguy_set_cell(guy, x, y, ct);
					goto OUT_OF_CHANGE_CELL;
				}

				acc++;
			}
		}
	}
OUT_OF_CHANGE_CELL:
END_OF_CHANGES:

	if(aliveguy_cells_amount(guy) < 1) {
		aliveguy_tostring(guy);
		printf("ZERO CELLS IN MUTATION \\o/\n");
		abort();
	}

	return;
}

void
aliveguy_birth(AliveGuy *guy, int x, int y, Game *game) {
	AliveGuy *child = game_new_aliveguy(game);
	
	if (child == NULL) {
		// couldnt give birth :(
		return;
	}

	aliveguy_init(child);

	child->hp = 50;
	child->food_consumed = 0;

	child->x = x;
	child->y = y;

	assert(aliveguy_cells_amount(guy) > 0);
	for (int y = 0; y < ALIVEGUY_CELLS_H; y++) {
		for (int x = 0; x < ALIVEGUY_CELLS_W; x++) {
			CellType cell = aliveguy_get_cell(guy, x, y);
			aliveguy_set_cell(child, x, y, cell);
		}
	}

	if(rand() % 100 < game->mutation_chance_percent) {
		aliveguy_guy_mutate(guy, game);
	}
	assert(aliveguy_cells_amount(child) > 0);

	aliveguy_calculate_new_lifetime(child);

	game_aliveguy_register_birth(game, guy);
}

int
aliveguy_try_reproduce(AliveGuy *guy, int index, Game *game) {
	// find vacant_spot
	assert(aliveguy_cells_amount(guy) > 0);

	int sx = aliveguy_starting_x(guy);
	int sy = aliveguy_starting_y(guy);
	int ex = aliveguy_ending_x(guy);
	int ey = aliveguy_ending_y(guy);

	int offset_x = ex - sx + (4 /*+ (rand() % 5)*/);
	int offset_y = ey - sy + (4 /*+ (rand() % 5)*/);

	struct pt { int x; int y; };
	struct pt arr[4] = {
		{guy->x - offset_x, guy->y           },
		{guy->x + offset_x, guy->y           },
		{guy->x           , guy->y - offset_y},
		{guy->x           , guy->y + offset_y}
	};

	// randomize the array
	for(int i = 0; i < 4; i++) {
		int rn = rand() % 4;
		if (rn == i) { continue; }
		struct pt tmp = arr[i];
		arr[i] = arr[rn];
		arr[rn] = tmp;
	}

	for(int i = 0; i < 4; i++) {
		int x = arr[i].x, y = arr[i].y;
		if(aliveguy_is_spot_vacant(guy, x, y, game)) {
			aliveguy_birth(guy, x, y, game);
			return 1;
		}
	}

	return 0;
}

void
aliveguy_tostring(AliveGuy *guy) {
	printf("x %d:\n", guy->x);
	printf("y %d:\n", guy->y);
	printf("lifetime %d:\n", guy->lifetime);
	printf("hp %d:\n", guy->hp);
	printf("foodconsumed %d:\n", guy->food_consumed);
	printf("cells:\n");
	for (int y = 0; y < ALIVEGUY_CELLS_H; y++) {
		for (int x = 0; x < ALIVEGUY_CELLS_W; x++) {
			switch(aliveguy_get_cell(guy, x, y)) {
			case None     : printf("."); break;
			case Producer : printf("P"); break;
			case Mover    : printf("M"); break;
			case Eater    : printf("E"); break;
			default : assert(0);
			}
		}
		printf("\n");
	}

	printf("\n");
}

void
aliveguy_update(AliveGuy *guy, int index, Game *game) {
	if (guy->hp <= 0) {
		return;
	}

	if(aliveguy_cells_amount(guy) < 1) {
		aliveguy_tostring(guy);
		printf("wtf\n");
		abort();
	}

	guy->lifetime -= 1;

	if (guy->lifetime == 0) {
		printf("organism %d died of old age.\n", index);
		guy->hp = 0;

		for (int y = 0; y < ALIVEGUY_CELLS_H; y++) {
			for (int x = 0; x < ALIVEGUY_CELLS_W; x++) {
				CellType cell = aliveguy_get_cell(guy, x, y);
				if (cell == None) {
					continue;
				}

				int nx = x + guy->x;
				int ny = y + guy->y;

				tilemap_set_tile(game->map, nx, ny, Food);
			}
		}
	}

	if (guy->hp <= 0) {
		return;
	}

	int food_needed = aliveguy_food_needed_to_reproduce(guy);
	
	if (guy->food_consumed > food_needed) {
		int success = aliveguy_try_reproduce(guy, index, game);
		(void) success;
		//if (success) {
		guy->food_consumed -= food_needed;
		//}
	}


	struct pt { int x; int y; };
	struct pt arr[4] = {
		{guy->x - 1, guy->y    },
		{guy->x + 1, guy->y    },
		{guy->x    , guy->y - 1},
		{guy->x    , guy->y + 1}
	};
	struct pt direction;

	if (guy->moving_frames_left <= 0) {
		guy->moving_frames_left = 1 + (rand() % 6);
		guy->moving_direction = rand() % 4;
	}
	direction = arr[guy->moving_direction];

	int has_moved = 0;
	int has_mover = 0;
	for (int y = 0; y < ALIVEGUY_CELLS_H; y++) {
		for (int x = 0; x < ALIVEGUY_CELLS_W; x++) {
			CellType cell = aliveguy_get_cell(guy, x, y);
			if (cell == Mover) {
				has_mover = 1;
				goto OUT_OF_MOVER_LOOP;
			}
		}
	}
OUT_OF_MOVER_LOOP:

	for (int y = 0; y < ALIVEGUY_CELLS_H; y++) {
		for (int x = 0; x < ALIVEGUY_CELLS_W; x++) {
			CellType cell = aliveguy_get_cell(guy, x, y);
			if (cell == None) {
				continue;
			}
			if (cell == Producer && has_mover) {
				continue;
			}
			if (cell == Mover && !has_moved) {
				has_moved = 1;

				// the hp is set to 0 in order to
				// not count its own spots as non-vacant
				// in the function aliveguy_is_spot_vacant
				int old_hp = guy->hp;
				guy->hp = 0;
				int vacant = aliveguy_is_spot_vacant(
					guy, direction.x, direction.y, game);
				if (vacant) {
					guy->moving_frames_left -= 1;
					guy->x = direction.x;
					guy->y = direction.y;
				} else {
					guy->moving_frames_left = 0;
				}
				guy->hp = old_hp;
			}
			cell_update(cell, x, y, guy, index, game);
		}
	}
}

TileMap *
make_tilemap(int w, int h) {
	TileMap *ret = malloc(sizeof(TileMap));
	ret->tiles = malloc(sizeof(TileType) * w * h);
	ret->w = w;
	ret->h = h;
	for (int i = 0; i < w * h; i++) {
		ret->tiles[i] = Empty;
	}

	return ret;
}

TileType *
tilemap_get_tile_ptr(TileMap *map, int x, int y) {
	if (!(0 <= x && 0 <= y &&
	      x < map->w && y < map->h)) {
		return NULL;
	}

	return &map->tiles[y * map->w + x];
}

TileType
tilemap_get_tile(TileMap *map, int x, int y) {
	assert(0 <= x && 0 <= y);
	assert(x < map->w && y < map->h);
	return map->tiles[y * map->w + x];
}

void
tilemap_set_tile(TileMap *map, int x, int y, TileType t) {
	assert(0 <= x && 0 <= y);
	assert(x < map->w && y < map->h);
	map->tiles[y * map->w + x] = t;
}

u32
get_tile_color(TileType t) {
	switch (t) {
	case Empty : return 0x484848ff;
	case Wall  : return 0x989898ff;
	case Food  : return 0x28a8b8ff;
	default : assert(0);
	}
	assert(0);
}

void
tilemap_render(TileMap *map) {
	for (int y = 0; y < map->h; y++) {
		for (int x = 0; x < map->w; x++) {
			TileType tile = tilemap_get_tile(map, x, y);
			u32 color = get_tile_color(tile);

			if (y % (RECT_WITH_GUYS_H * 2) < RECT_WITH_GUYS_H) {
				if (x % (RECT_WITH_GUYS_W * 2) < RECT_WITH_GUYS_W) {
					color += 0x0a0a0a00;
				}
			} else {
				if (x % (RECT_WITH_GUYS_W * 2) >= RECT_WITH_GUYS_W) {
					color += 0x0a0a0a00;
				}
			}

			sdl_set_color(color);

			SDL_FRect rect = {
				x * 10,
				y * 10,
				10 - 1,
				10 - 1
			};
			SDL_RenderFillRect(ren, &rect);
		}
	}
}

void
game_init(Game *game) {
	game->alives = 0;
	game->mutation_chance_percent = 20;
	for (int i = 0; i < GUYS_N; i++) {
		aliveguy_init(&game->guys[i]);
	}

	game->map = make_tilemap(RECT_WITH_GUYS_W * 10, RECT_WITH_GUYS_H * 10);
	game->rects_with_guys = malloc(sizeof(RectWithGuys) * 10 * 10);

	int rwgw = game_get_rects_with_guys_w(game);
	int rwgh = game_get_rects_with_guys_h(game);
	for(int i = 0; i < rwgw * rwgh; i++) {
		RectWithGuys *rect = &game->rects_with_guys[i];
		rect->amount = 0;
	}

	AliveGuy *g = &game->guys[0];
	g->x = 50;
	g->y = 0;
	g->cells[5 * ALIVEGUY_CELLS_W + 5] = Producer;
	g->cells[6 * ALIVEGUY_CELLS_H + 6] = Eater;
	g->hp = 50;
	game->alives = 1;
	aliveguy_calculate_new_lifetime(g);
	game_aliveguy_register_birth(game, g);
}

void
game_render(Game *game) {
	tilemap_render(game->map);
	for (int i = 0; i < GUYS_N; i++) {
		AliveGuy *guy = &game->guys[i];
		if(guy->hp > 0) {
			aliveguy_render(guy);
		}
	}
}

AliveGuy *
game_new_aliveguy(Game *game) {
	for (int i = 0; i < GUYS_N; i++) {
		AliveGuy *guy = &game->guys[i];
		if(guy->hp <= 0) {
			return guy;
		}
	}
	return NULL;
}

void
game_update(Game *game) {
	for (int i = 0; i < GUYS_N; i++) {
		AliveGuy *guy = &game->guys[i];
		if(guy->hp > 0) {
			aliveguy_update(guy, i, game);
		}
	}
}

int main() {
	srand(time(NULL));
	SDL_Init(SDL_INIT_VIDEO);
	win = SDL_CreateWindow(
		"title", 800, 600,
		0
		//| SDL_WINDOW_RESIZABLE
		);
	ren = SDL_CreateRenderer(win, NULL);
	SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

	Game game;
	game_init(&game);

	long tick = 0;
	long last_tick = 0;
	long current_tick = 0;

	bool running = true;

	while (running) {
		current_tick = SDL_GetTicks();
		if (current_tick - last_tick > (1000/60.0)) {
			tick++;
			last_tick = current_tick;

			while (SDL_PollEvent(&ev)) {
				switch(ev.type) {
				case SDL_EVENT_QUIT : {
					running = 0;
				} break;
				}
			}
			game_update(&game);

			SDL_SetRenderDrawColor(ren, 0x18, 0x18, 0x18, 0xff);
			SDL_RenderClear(ren);

			game_render(&game);

			SDL_RenderPresent(ren);
		} else {
			SDL_Delay(1000/60.0 - (current_tick - last_tick));
		}
	}

	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();

	return 0;
}
