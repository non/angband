/*
 * File: cavern.c
 * Purpose: Cavern generation.
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
#include "level/cavern.h"
#include "monster/mon-make.h"
#include "monster/mon-spell.h"
#include "object/tvalsval.h"
#include "trap.h"
#include "z-queue.h"
#include "z-type.h"

/**
 * Initialize the dungeon array, with a random percentage of squares open.
 */
static void init_cavern(struct cave *c, struct player *p, int density)
{
	int h = c->height;
	int w = c->width;
	int size = h * w;
	
	int count = (size * density) / 100;
	
	/* Fill the edges with perma-rock, and rest with rock */
	draw_rectangle(c, 0, 0, DUNGEON_HGT - 1, DUNGEON_WID - 1, FEAT_PERM_SOLID);
	fill_rectangle(c, 1, 1, DUNGEON_HGT - 2, DUNGEON_WID - 2, FEAT_WALL_SOLID);
	
	while (count > 0) {
		int y = randint1(h - 2);
		int x = randint1(w - 2);
		if (cave_isrock(c, y, x)) {
			cave_set_feat(c, y, x, FEAT_FLOOR);
			count--;
		}
	}
}


/**
 * Return the number of walls (0-8) adjacent to this square.
 */
static int count_adj_walls(struct cave *c, int y, int x)
{
	int yd, xd;
	int count = 0;

	for (yd = -1; yd <= 1; yd++) {
		for (xd = -1; xd <= 1; xd++) {
			if (yd == 0 && xd == 0) continue;
			if (cave_isfloor(c, y + yd, x + xd)) continue;
			count++;
		}
	}

	return count;
}


/**
 * Count the number of open cells in the dungeon.
 */
static int count_open_squares(struct cave *c)
{
	int x, y;
	int h = c->height;
	int w = c->width;
	int num = 0;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++)
			if (cave_ispassable(c, y, x)) num++;
	return num;
}


/**
 * Run a single pass of the cellular automata rules (4,5) on the dungeon.
 */
static void mutate_cavern(struct cave *c)
{
	int y, x;
	int h = c->height;
	int w = c->width;

	int *temp = C_ZNEW(h * w, int);

	for (y = 1; y < h - 1; y++) {
		for (x = 1; x < w - 1; x++) {
			int count = count_adj_walls(c, y, x);
			if (count > 5)
				temp[y * w + x] = FEAT_WALL_SOLID;
			else if (count < 4)
				temp[y * w + x] = FEAT_FLOOR;
			else
				temp[y * w + x] = cave->feat[y][x];
		}
	}

	for (y = 1; y < h - 1; y++) {
		for (x = 1; x < w - 1; x++) {
			cave_set_feat(c, y, x, temp[y * w + x]);
		}
	}

	FREE(temp);
}


/**
 * Find and delete all small (<9 square) open regions.
 */
static void clear_small_regions(struct cave *c, int colors[], int counts[])
{
	int i, y, x;
	int h = c->height;
	int w = c->width;
	int size = h * w;

	int *deleted = C_ZNEW(size, int);
	array_filler(deleted, 0, size);

	for (i = 0; i < size; i++) {
		if (counts[i] < 9) {
			deleted[i] = 1;
			counts[i] = 0;
		}
	}

	for (y = 1; y < c->height - 1; y++) {
		for (x = 1; x < c->width - 1; x++) {
			i = lab_toi(y, x, w);

			if (!deleted[colors[i]]) continue;

			colors[i] = 0;
			cave_set_feat(c, y, x, FEAT_WALL_SOLID);
		}
	}
	FREE(deleted);
}


#define MAX_CAVERN_TRIES 10
/**
 * The generator's main function.
 */
bool cavern_gen(struct cave *c, struct player *p)
{
	int i, k, openc;

	int h = rand_range(DUNGEON_HGT / 2, (DUNGEON_HGT * 3) / 4);
	int w = rand_range(DUNGEON_WID / 2, (DUNGEON_WID * 3) / 4);
	int size = h * w;
	int limit = size / 13;

	int density = rand_range(25, 30);
	int times = rand_range(3, 6);

	int *colors = C_ZNEW(size, int);
	int *counts = C_ZNEW(size, int);

	int tries = 0;

	bool ok = TRUE;

	set_cave_dimensions(c, h, w);
	ROOM_LOG("cavern h=%d w=%d size=%d density=%d times=%d", h, w, size, density, times);

	if (c->depth < 15) {
		/* If we're too shallow then don't do it */
		ok = FALSE;

	} else {
		/* Start trying to build caverns */
		array_filler(colors, 0, size);
		array_filler(counts, 0, size);
	
		for (tries = 0; tries < MAX_CAVERN_TRIES; tries++) {
			/* Build a random cavern and mutate it a number of times */
			init_cavern(c, p, density);
			for (i = 0; i < times; i++) mutate_cavern(c);
	
			/* If there are enough open squares then we're done */
			openc = count_open_squares(c);
			if (openc >= limit) {
				ROOM_LOG("cavern ok (%d vs %d)", openc, limit);
				break;
			}
			ROOM_LOG("cavern failed--try again (%d vs %d)", openc, limit);
		}

		/* If we couldn't make a big enough cavern then fail */
		if (tries == MAX_CAVERN_TRIES) ok = FALSE;
	}

	if (ok) {
		build_colors(c, colors, counts, FALSE);
		clear_small_regions(c, colors, counts);
		join_regions(c, colors, counts);
	
		/* Place 2-3 down stairs near some walls */
		alloc_stairs(c, FEAT_MORE, rand_range(1, 3), 3);
	
		/* Place 1-2 up stairs near some walls */
		alloc_stairs(c, FEAT_LESS, rand_range(1, 2), 3);
	
		/* General some rubble, traps and monsters */
		k = MAX(MIN(c->depth / 3, 10), 2);
	
		/* Scale number of monsters items by cavern size */
		k = (2 * k * (h *  w)) / (DUNGEON_HGT * DUNGEON_WID);
	
		/* Put some rubble in corridors */
		alloc_objects(c, SET_BOTH, TYP_RUBBLE, randint1(k), c->depth, 0);
	
		/* Place some traps in the dungeon */
		alloc_objects(c, SET_BOTH, TYP_TRAP, randint1(k), c->depth, 0);
	
		/* Determine the character location */
		new_player_spot(c, p);
	
		/* Put some monsters in the dungeon */
		for (i = MIN_M_ALLOC_LEVEL + randint1(8) + k; i > 0; i--)
			pick_and_place_distant_monster(c, loc(p->px, p->py), 0, TRUE, c->depth);
	
		/* Put some objects/gold in the dungeon */
		alloc_objects(c, SET_BOTH, TYP_OBJECT, Rand_normal(6, 3), c->depth,
			ORIGIN_CAVERN);
		alloc_objects(c, SET_BOTH, TYP_GOLD, Rand_normal(6, 3), c->depth,
			ORIGIN_CAVERN);
		alloc_objects(c, SET_BOTH, TYP_GOOD, randint0(2), c->depth,
			ORIGIN_CAVERN);
	}

	FREE(colors);
	FREE(counts);

	return ok;
}

