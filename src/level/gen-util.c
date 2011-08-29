/*
 * File: level/gen-util.c
 * Purpose: Dungeon generation utility functions.
 *
 * Copyright (c) 2011 Erik Osheim
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cave.h"
#include "math.h"
#include "files.h"
#include "level/gen-util.h"
#include "level/generate.h"
#include "monster/mon-make.h"
#include "monster/mon-spell.h"
#include "object/tvalsval.h"
#include "trap.h"
#include "z-queue.h"
#include "z-type.h"


/**
 * Convenient macros for debugging level generation.
 */
#if  __STDC_VERSION__ < 199901L
# define ROOM_DEBUG if (0) msg;
# define ROOM_LOG  if (OPT(cheat_room)) msg
#else
# define ROOM_DEBUG(...) if (0) msg(__VA_ARGS__);
# define ROOM_LOG(...) if (OPT(cheat_room)) msg(__VA_ARGS__);
#endif


/**
 * This is a global array of positions in the cave we're currently
 * generating. It's used to quickly randomize all the current cave positions.
 */
static int *cave_squares = NULL;

//static void alloc_objects(struct cave *c, int set, int typ, int num, int depth, byte origin);
//static bool alloc_object(struct cave *c, int set, int typ, int depth, byte origin);

/**
 * Shuffle an array using Knuth's shuffle.
 */
void shuffle(int *arr, int n)
{
	  	int i, j, k;
	  	for (i = 0; i < n; i++) {
	  		  	j = randint0(n - i) + i;
	  		  	k = arr[j];
	  		  	arr[j] = arr[i];
	  		  	arr[i] = k;
	  	}
}


/**
 * Locate a square in y1 <= y < y2, x1 <= x < x2 which satisfies the given
 * predicate.
 */
static bool _find_in_range(struct cave *c, int *y, int y1, int y2, int *x,
	  	int x1, int x2, int *squares, cave_predicate pred)
{
	  	int yd = y2 - y1;
	  	int xd = x2 - x1;
	  	int i, n = yd * xd;
	  	bool found = FALSE;

	  	/* Test each square in (random) order for openness */
	  	for (i = 0; i < n && !found; i++) {
	  		  	int j = randint0(n - i) + i;
	  		  	int k = squares[j];
	  		  	squares[j] = squares[i];
	  		  	squares[i] = k;

	  		  	*y = (k / xd) + y1;
	  		  	*x = (k % xd) + x1;
	  		  	if (pred(c, *y, *x)) found = TRUE;
	  	}

	  	/* Return whether we found an empty square or not. */
	  	return found;
}


/**
 * Locate a square in the dungeon which satisfies the given predicate.
 */
bool cave_find(struct cave *c, int *y, int *x, cave_predicate pred)
{
	  	int h = c->height;
	  	int w = c->width;
	  	return _find_in_range(c, y, 0, h, x, 0, w, cave_squares, pred);
}


/**
 * Locate a square in y1 <= y < y2, x1 <= x < x2 which satisfies the given
 * predicate.
 */
bool cave_find_in_range(struct cave *c, int *y, int y1, int y2,
	  	int *x, int x1, int x2, cave_predicate pred)
{
	  	int yd = y2 - y1;
	  	int xd = x2 - x1;
	  	int n = yd * xd;
	  	int i, found;

	  	/* Allocate the squares, and randomize their order */
	  	int *squares = C_ZNEW(n, int);
	  	for (i = 0; i < n; i++) squares[i] = i;

	  	/* Do the actual search */
	  	found = _find_in_range(c, y, y1, y2, x, x1, x2, squares, pred);

	  	/* Deallocate memory */
	  	FREE(squares);

	  	/* Return whether or not we found an empty square */
	  	return found;
}


/**
 * Locate an empty square for 0 <= y < ymax, 0 <= x < xmax.
 */
bool find_empty(struct cave *c, int *y, int *x)
{
	  	return cave_find(c, y, x, cave_isempty);
}


/**
 * Locate an empty square for y1 <= y < y2, x1 <= x < x2.
 */
bool find_empty_range(struct cave *c, int *y, int y1, int y2, int *x, int x1, int x2)
{
	  	return cave_find_in_range(c, y, y1, y2, x, x1, x2, cave_isempty);
}


/**
 * Locate a grid nearby (y0, x0) within +/- yd, xd.
 */
bool find_nearby_grid(struct cave *c, int *y, int y0, int yd, int *x, int x0, int xd)
{
	  	int y1 = y0 - yd;
	  	int x1 = x0 - xd;
	  	int y2 = y0 + yd + 1;
	  	int x2 = x0 + xd + 1;
	  	return cave_find_in_range(c, y, y1, y2, x, x1, x2, cave_in_bounds);
}


/**
 * Given two points, pick a valid cardinal direction from one to the other.
 */
void correct_dir(int *rdir, int *cdir, int y1, int x1, int y2, int x2)
{
	  	/* Extract vertical and horizontal directions */
	  	*rdir = CMP(y2, y1);
	  	*cdir = CMP(x2, x1);

	  	/* If we only have one direction to go, then we're done */
	  	if (!*rdir || !*cdir) return;

	  	/* If we need to go diagonally, then choose a random direction */
	  	if (randint0(100) < 50)
	  		  	*rdir = 0;
	  	else
	  		  	*cdir = 0;
}


/**
 * Pick a random cardinal direction.
 */
void rand_dir(int *rdir, int *cdir)
{
	  	/* Pick a random direction and extract the dy/dx components */
	  	int i = randint0(4);
	  	*rdir = ddy_ddd[i];
	  	*cdir = ddx_ddd[i];
}


/**
 * Determine whether the given coordinate is a valid starting location.
 */
bool cave_isstart(struct cave *c, int y, int x)
{
	  	if (!cave_isempty(c, y, x)) return FALSE;
	  	if (cave_isvault(c, y, x)) return FALSE;
	  	return TRUE;
}


/**
 * Place the player at a random starting location.
 */
void new_player_spot(struct cave *c, struct player *p)
{
	  	int y, x;

	  	/* Try to find a good place to put the player */
	  	cave_find_in_range(c, &y, 0, c->height, &x, 0, c->width, cave_isstart);

	  	/* Create stairs the player came down if allowed and necessary */
	  	if (OPT(birth_no_stairs)) {
	  	} else if (p->create_down_stair) {
	  		  	cave_set_feat(c, y, x, FEAT_MORE);
	  		  	p->create_down_stair = FALSE;
	  	} else if (p->create_up_stair) {
	  		  	cave_set_feat(c, y, x, FEAT_LESS);
	  		  	p->create_up_stair = FALSE;
	  	}

	  	player_place(c, p, y, x);
}


/**
 * Return how many cardinal directions around (x, y) contain walls.
 */
int next_to_walls(struct cave *c, int y, int x)
{
	  	int k = 0;
	  	assert(cave_in_bounds(c, y, x));

	  	if (cave_iswall(c, y + 1, x)) k++;
	  	if (cave_iswall(c, y - 1, x)) k++;
	  	if (cave_iswall(c, y, x + 1)) k++;
	  	if (cave_iswall(c, y, x - 1)) k++;

	  	return k;
}


/**
 * Place rubble at (x, y).
 */
void place_rubble(struct cave *c, int y, int x)
{
	  	cave_set_feat(c, y, x, FEAT_RUBBLE);
}


/**
 * Place stairs (of the requested type 'feat' if allowed) at (x, y).
 *
 * All stairs from town go down. All stairs on an unfinished quest level go up.
 */
void place_stairs(struct cave *c, int y, int x, int feat)
{
	  	if (!c->depth)
	  		  	cave_set_feat(c, y, x, FEAT_MORE);
	  	else if (is_quest(c->depth) || c->depth >= MAX_DEPTH - 1)
	  		  	cave_set_feat(c, y, x, FEAT_LESS);
	  	else
	  		  	cave_set_feat(c, y, x, feat);
}


/**
 * Place random stairs at (x, y).
 */
void place_random_stairs(struct cave *c, int y, int x)
{
	  	int feat = randint0(100) < 50 ? FEAT_LESS : FEAT_MORE;
	  	if (cave_canputitem(c, y, x)) place_stairs(c, y, x, feat);
}


/**
 * Place a random object at (x, y).
 */
void place_object(struct cave *c, int y, int x, int level, bool good, bool great, byte origin)
{
	  	s32b rating = 0;
	  	object_type otype;

	  	assert(cave_in_bounds(c, y, x));

	  	if (!cave_canputitem(c, y, x)) return;

	  	object_wipe(&otype);
	  	if (!make_object(c, &otype, level, good, great, &rating)) return;

	  	otype.origin = origin;
	  	otype.origin_depth = c->depth;

	  	/* Give it to the floor */
	  	/* XXX Should this be done in floor_carry? */
	  	if (!floor_carry(c, y, x, &otype)) {
	  		  	if (otype.artifact)
	  		  		  	otype.artifact->created = FALSE;
	  		  	return;
	  	} else {
 	  		  		  	if (otype.artifact)
	  		  		  	c->good_item = TRUE;
	  		  	c->obj_rating += rating;
	  	}
}


/**
 * Place a random amount of gold at (x, y).
 */
void place_gold(struct cave *c, int y, int x, int level, byte origin)
{
	  	object_type *i_ptr;
	  	object_type object_type_body;

	  	assert(cave_in_bounds(c, y, x));

	  	if (!cave_canputitem(c, y, x)) return;

	  	i_ptr = &object_type_body;
	  	object_wipe(i_ptr);
	  	make_gold(i_ptr, level, SV_GOLD_ANY);

	  	i_ptr->origin = origin;
	  	i_ptr->origin_depth = level;

	  	floor_carry(c, y, x, i_ptr);
}


/**
 * Place a secret door at (x, y).
 */
void place_secret_door(struct cave *c, int y, int x)
{
	  	cave_set_feat(c, y, x, FEAT_SECRET);
}


/**
 * Place a closed door at (x, y).
 */
void place_closed_door(struct cave *c, int y, int x)
{
	  	int tmp = randint0(400);

	  	if (tmp < 300)
	  		  	cave_set_feat(c, y, x, FEAT_DOOR_HEAD + 0x00);
	  	else if (tmp < 399)
	  		  	cave_set_feat(c, y, x, FEAT_DOOR_HEAD + randint1(7));
	  	else
	  		  	cave_set_feat(c, y, x, FEAT_DOOR_HEAD + 0x08 + randint0(8));
}


/**
 * Place a random door at (x, y).
 *
 * The door generated could be closed, open, broken, or secret.
 */
void place_random_door(struct cave *c, int y, int x)
{
	  	int tmp = randint0(100);

	  	if (tmp < 30)
	  		  	cave_set_feat(c, y, x, FEAT_OPEN);
	  	else if (tmp < 40)
	  		  	cave_set_feat(c, y, x, FEAT_BROKEN);
	  	else if (tmp < 60)
	  		  	cave_set_feat(c, y, x, FEAT_SECRET);
	  	else
	  		  	place_closed_door(c, y, x);
}

/**
 * Allocates 'num' random objects in the dungeon.
 *
 * See alloc_object() for more information.
 */
void alloc_objects(struct cave *c, int set, int typ, int num, int depth, byte origin)
{
	  	int k, l = 0;
	  	for (k = 0; k < num; k++) {
	  		  	bool ok = alloc_object(c, set, typ, depth, origin);
	  		  	if (!ok) l++;
	  	}
}


/**
 * Allocates a single random object in the dungeon.
 *
 * 'set' controls where the object is placed (corridor, room, either).
 * 'typ' conrols the kind of object (rubble, trap, gold, item).
 */
bool alloc_object(struct cave *c, int set, int typ, int depth, byte origin)
{
	  	int x, y;
	  	int tries = 0;
	  	bool room;

	  	/* Pick a "legal" spot */
	  	while (tries < 2000) {
	  		  	tries++;

	  		  	find_empty(c, &y, &x);

	  		  	/* See if our spot is in a room or not */
	  		  	room = (c->info[y][x] & CAVE_ROOM) ? TRUE : FALSE;

	  		  	/* If we are ok with a corridor and we're in one, we're done */
	  		  	if (set & SET_CORR && !room) break;

	  		  	/* If we are ok with a room and we're in one, we're done */
	  		  	if (set & SET_ROOM && room) break;
	  	}

	  	if (tries == 2000) return FALSE;

	  	/* Place something */
	  	switch (typ) {
	  		  	case TYP_RUBBLE: place_rubble(c, y, x); break;
	  		  	case TYP_TRAP: place_trap(c, y, x); break;
	  		  	case TYP_GOLD: place_gold(c, y, x, depth, origin); break;
	  		  	case TYP_OBJECT: place_object(c, y, x, depth, FALSE, FALSE, origin); break;
	  		  	case TYP_GOOD: place_object(c, y, x, depth, TRUE, FALSE, origin); break;
	  		  	case TYP_GREAT: place_object(c, y, x, depth, TRUE, TRUE, origin); break;
	  	}
	  	return TRUE;
}


void alloc_cave_squares(int n)
{
    int i;
	  	cave_squares = C_ZNEW(n, int);
	  	for (i = 0; i < n; i++) cave_squares[i] = i;
}

void free_cave_squares(void)
{
	  	if (cave_squares != NULL) FREE(cave_squares);
	  	cave_squares = NULL;
}

/**
 * 
 */
void set_cave_dimensions(struct cave *c, int h, int w)
{
	  	c->height = h;
	  	c->width = w;
    free_cave_squares();
    alloc_cave_squares(h * w);
}



/**
 * Fill a rectangle with a feature.
 *
 * The boundaries (y1, x1, y2, x2) are inclusive.
 */
void fill_rectangle(struct cave *c, int y1, int x1, int y2, int x2, int feat)
{
	  	int y, x;
	  	for (y = y1; y <= y2; y++)
	  		  	for (x = x1; x <= x2; x++)
	  		  		  	cave_set_feat(c, y, x, feat);
}


/**
 * Fill the edges of a rectangle with a feature.
 *
 * The boundaries (y1, x1, y2, x2) are inclusive.
 */
void draw_rectangle(struct cave *c, int y1, int x1, int y2, int x2, int feat)
{
	  	int y, x;

	  	for (y = y1; y <= y2; y++) {
	  		  	cave_set_feat(c, y, x1, feat);
	  		  	cave_set_feat(c, y, x2, feat);
	  	}

	  	for (x = x1; x <= x2; x++) {
	  		  	cave_set_feat(c, y1, x, feat);
	  		  	cave_set_feat(c, y2, x, feat);
	  	}
}


/**
 * Fill a horizontal range with the given feature/info.
 */
void fill_xrange(struct cave *c, int y, int x1, int x2, int feat, int info)
{
	  	int x;
	  	for (x = x1; x <= x2; x++) {
	  		  	cave_set_feat(c, y, x, feat);
	  		  	c->info[y][x] |= info;
	  	}
}


/**
 * Fill a vertical range with the given feature/info.
 */
void fill_yrange(struct cave *c, int x, int y1, int y2, int feat, int info)
{
	  	int y;
	  	for (y = y1; y <= y2; y++) {
	  		  	cave_set_feat(c, y, x, feat);
	  		  	c->info[y][x] |= info;
	  	}
}


/**
 * Fill a circle with the given feature/info.
 */
void fill_circle(struct cave *c, int y0, int x0, int radius, int border, int feat, int info)
{
	  	int i, last = 0;
	  	int r2 = radius * radius;
	  	for(i = 0; i <= radius; i++) {
	  		  	double j = sqrt(r2 - (i * i));
	  		  	int k = (int)(j + 0.5);

	  		  	int b = border;
	  		  	if (border && last > k) b++;
	  		  	
	  		  	fill_xrange(c, y0 - i, x0 - k - b, x0 + k + b, feat, info);
	  		  	fill_xrange(c, y0 + i, x0 - k - b, x0 + k + b, feat, info);
	  		  	fill_yrange(c, x0 - i, y0 - k - b, y0 + k + b, feat, info);
	  		  	fill_yrange(c, x0 + i, y0 - k - b, y0 + k + b, feat, info);
	  		  	last = k;
	  	}
}


/**
 * Place some staircases near walls.
 */
void alloc_stairs(struct cave *c, int feat, int num, int walls)
{
	  	int y, x, i, j, done;

	  	/* Place "num" stairs */
	  	for (i = 0; i < num; i++) {
	  		  	/* Place some stairs */
	  		  	for (done = FALSE; !done; ) {
	  		  		  	/* Try several times, then decrease "walls" */
	  		  		  	for (j = 0; !done && j <= 1000; j++) {
	  		  		  		  	find_empty(c, &y, &x);

	  		  		  		  	if (next_to_walls(c, y, x) < walls) continue;

	  		  		  		  	place_stairs(c, y, x, feat);
	  		  		  		  	done = TRUE;
	  		  		  	}

	  		  		  	/* Require fewer walls */
	  		  		  	if (walls) walls--;
	  		  	}
	  	}
}


/* Global variable storing which type of pit we are building */
struct pit_profile *pit_type = NULL;


/**
 * Hook for picking monsters appropriate to a nest/pit.
 *
 * Requires pit_type to be set.
 */
static bool mon_pit_hook(int r_idx)
{
	  	monster_race *r_ptr = &r_info[r_idx];

	  	/* pit_type needs to be set */
	  	assert(pit_type);

	  	/* Uniques are not allowed in pits */
	  	if (rf_has(r_ptr->flags, RF_UNIQUE)) return FALSE;

	  	/* Monster must have all required flags */
	  	if (!rf_is_subset(r_ptr->flags, pit_type->flags)) return FALSE;

	  	/* Monster must not have any forbidden flags */
	  	if (rf_is_inter(r_ptr->flags, pit_type->forbidden_flags)) return FALSE;

	  	/* Monster have all required spells */
	  	if (!rsf_is_subset(r_ptr->spell_flags, pit_type->spell_flags)) return FALSE;

	  	/* Monster must not have any forbidden spells */
	  	if (rsf_is_inter(r_ptr->spell_flags, pit_type->forbidden_spell_flags)) return FALSE;

	  	/* Monster must not be explicitly forbidden */
	  	if (pit_type->forbidden_monsters) {
	  		  	struct pit_forbidden_monster *m;
	  		  	for (m = pit_type->forbidden_monsters; m; m = m->next)
	  		  		  	if (r_idx == m->r_idx) return FALSE;
	  	}

	  	/* If pit specifies monster types, this monster must be one of them */
	  	if (pit_type->n_bases > 0) {
	  		  	int i;
	  		  	bool match_base = FALSE;
	  		  	for (i = 0; i < pit_type->n_bases; i++) {
	  		  		  	if (r_ptr->base == pit_type->base[i]) {
	  		  		  		  	match_base = TRUE;
	  		  		  		  	break;
	  		  		  	}
	  		  	}
	  		  	if (!match_base) return FALSE;
	  	}
	  	
	  	/* If pit specifies colors, this monster must be one of those */
	  	if (pit_type->colors) {
	  		  	struct pit_color_profile *colors;
	  		  	bool match_color = FALSE;
	  		  	for (colors = pit_type->colors; colors; colors = colors->next) {
	  		  		  	if (r_ptr->d_attr == colors->color) {
	  		  		  		  	match_color = TRUE;
	  		  		  		  	break;
	  		  		  	}
	  		  	}
	  		  	if (!match_color) return FALSE;
	  	}

	  	return TRUE;
}

/**
 * Pick a type of monster pit, based on the level.
 *
 * We scan through all pits, and for each one generate a random depth
 * using a normal distribution, with the mean given in pit.txt, and a
 * standard deviation of 10. Then we pick the pit that gave us a depth that
 * is closest to the player's actual depth.
 *
 * Sets pit_type, which is required for mon_pit_hook.
 * Returns the index of the chosen pit.
 */
int set_pit_type(int depth, int type)
{
	  	int i;
	  	int pit_idx = 0;
	  	
	  	/* Hack -- set initial distance large */
	  	int pit_dist = 999;
	  	
	  	for (i = 0; i < z_info->pit_max; i++) {
	  		  	int offset, dist;
	  		  	struct pit_profile *pit = &pit_info[i];
	  		  	
	  		  	/* Skip empty pits or pits of the wrong room type */
	  		  	if (!pit->name || pit->room_type != type) continue;
	  		  	
	  		  	offset = Rand_normal(pit->ave, 10);
	  		  	dist = ABS(offset - depth);
	  		  	
	  		  	if (dist < pit_dist && one_in_(pit->rarity)) {
	  		  		  	/* This pit is the closest so far */
	  		  		  	pit_idx = i;
	  		  		  	pit_dist = dist;
	  		  	}
	  	}

	  	pit_type = &pit_info[pit_idx];
	  	get_mon_num_hook = mon_pit_hook;
        
	  	return pit_idx;
}
