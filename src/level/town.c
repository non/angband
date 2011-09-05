/*
 * File: town.c
 * Purpose: Town generation.
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "level/town.h"
#include "monster/mon-make.h"
#include "monster/mon-spell.h"
#include "object/tvalsval.h"
#include "trap.h"
#include "z-queue.h"
#include "z-type.h"


/**
 * Builds a store at a given pseudo-location
 *
 * Currently, there is a main street horizontally through the middle of town,
 * and all the shops face it (e.g. the shops on the north side face south).
 */
void build_store(struct cave *c, int n, int yy, int xx)
{
	/* Find the "center" of the store */
	int y0 = yy * 9 + 6;
	int x0 = xx * 14 + 12;

	/* Determine the store boundaries */
	int y1 = y0 - randint1((yy == 0) ? 3 : 2);
	int y2 = y0 + randint1((yy == 1) ? 3 : 2);
	int x1 = x0 - randint1(5);
	int x2 = x0 + randint1(5);

	/* Determine door location, based on which side of the street we're on */
	int dy = (yy == 0) ? y2 : y1;
	int dx = rand_range(x1, x2);

	/* Build an invulnerable rectangular building */
	fill_rectangle(c, y1, x1, y2, x2, FEAT_PERM_EXTRA);

	/* Clear previous contents, add a store door */
	cave_set_feat(c, dy, dx, FEAT_SHOP_HEAD + n);
}


/**
 * Generate the "consistent" town features, and place the player
 *
 * HACK: We seed the simple RNG, so we always get the same town layout,
 * including the size and shape of the buildings, the locations of the
 * doorways, and the location of the stairs. This means that if any of the
 * functions used to build the town change the way they use the RNG, the
 * town layout will be generated differently.
 *
 * XXX: Remove this gross hack when this piece of code is fully reentrant -
 * i.e., when all we need to do is swing a pointer to change caves, we just
 * need to generate the town once (we will also need to save/load the town).
 */
void town_gen_hack(struct cave *c, struct player *p)
{
	int y, x, n, k;
	int rooms[MAX_STORES];

	int n_rows = 2;
	int n_cols = (MAX_STORES + 1) / n_rows;

	/* Switch to the "simple" RNG and use our original town seed */
	Rand_quick = TRUE;
	Rand_value = seed_town;

	/* Prepare an Array of "remaining stores", and count them */
	for (n = 0; n < MAX_STORES; n++) rooms[n] = n;

	/* Place rows of stores */
	for (y = 0; y < n_rows; y++) {
		for (x = 0; x < n_cols; x++) {
			if (n < 1) break;

			/* Pick a remaining store */
			k = randint0(n);

			/* Build that store at the proper location */
			build_store(c, rooms[k], y, x);

			/* Shift the stores down, remove one store */
			rooms[k] = rooms[--n];
		}
	}

	/* Place the stairs */
	find_empty_range(c, &y, 3, TOWN_HGT - 3, &x, 3, TOWN_WID - 3);

	/* Clear previous contents, add down stairs */
	cave_set_feat(c, y, x, FEAT_MORE);

	/* Place the player */
	player_place(c, p, y, x);

	/* go back to using the "complex" RNG */
	Rand_quick = FALSE;
}


/**
 * Town logic flow for generation of new town.
 *
 * We start with a fully wiped cave of normal floors. This function does NOT do
 * anything about the owners of the stores, nor the contents thereof. It only
 * handles the physical layout.
 */
bool town_gen(struct cave *c, struct player *p)
{
	int i;
	bool daytime = turn % (10 * TOWN_DAWN) < (10 * TOWN_DUSK);
	int residents = daytime ? MIN_M_ALLOC_TD : MIN_M_ALLOC_TN;

	assert(c);

	set_cave_dimensions(c, TOWN_HGT, TOWN_WID);

	/* NOTE: We can't use c->height and c->width here because then there'll be
	 * a bunch of empty space in the level that monsters might spawn in (or
	 * teleport might take you to, or whatever).
	 *
	 * TODO: fix this to use c->height and c->width when all the 'choose
	 * random location' things honor them.
	 */

	/* Start with solid walls, and then create some floor in the middle */
	fill_rectangle(c, 0, 0, DUNGEON_HGT - 1, DUNGEON_WID - 1, FEAT_PERM_SOLID);
	fill_rectangle(c, 1, 1, c->height -2, c->width - 2, FEAT_FLOOR);

	/* Build stuff */
	town_gen_hack(c, p);

	/* Apply illumination */
	cave_illuminate(c, daytime);

	/* Make some residents */
	for (i = 0; i < residents; i++)
		pick_and_place_distant_monster(c, loc(p->px, p->py), 3, TRUE, c->depth);

	return TRUE;
}
