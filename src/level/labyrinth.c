/*
 * File: generate.c
 * Purpose: Dungeon generation.
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
#include "level/labyrinth.h"
#include "monster/mon-make.h"
#include "monster/mon-spell.h"
#include "object/tvalsval.h"
#include "trap.h"
#include "z-queue.h"
#include "z-type.h"

/**
 * Build a labyrinth level.
 *
 * Note that if the function returns FALSE, a level wasn't generated.
 * Labyrinths use the dungeon level's number to determine whether to generate
 * themselves (which means certain level numbers are more likely to generate
 * labyrinths than others).
 */
bool labyrinth_gen(struct cave *c, struct player *p)
{
	int i, j, k, y, x;

	/* Size of the actual labyrinth part must be odd. */
	/* NOTE: these are not the actual dungeon size, but rather the size of the
  	  * area we're genearting a labyrinth in (which doesn't count theh enclosing
  	  * outer walls. */
	int h = 15 + randint0(c->depth / 10) * 2;
	int w = 51 + randint0(c->depth / 10) * 2;

	/* This is the number of squares in the labyrinth */
	int n = h * w;

	/* NOTE: 'sets' and 'walls' are too large... we only need to use about
  	  * 1/4 as much memory. However, in that case, the addressing math becomes
  	  * a lot more complicated, so let's just stick with this because it's
  	  * easier to read. */

  	 /* 'sets' tracks connectedness; if sets[i] == sets[j] then cells i and j
  	  * are connected to each other in the maze. */
	int *sets;

  	 /* 'walls' is a list of wall coordinates which we will randomize */
  	 int *walls;

  	 /* Most labyrinths are lit */
	bool lit = randint0(c->depth) < 25 || randint0(2) < 1;

	/* Many labyrinths are known */
	bool known = lit && randint0(c->depth) < 25;
	
	/* Most labyrinths have soft (diggable) walls */
	bool soft = randint0(c->depth) < 35 || randint0(3) < 2;
	
	/* There's a base 1 in 100 to accept the labyrinth */
	int chance = 1;

	/* If we're too shallow then don't do it */
	if (c->depth < 13) return FALSE;

	/* Don't try this on quest levels, kids... */
	if (is_quest(c->depth)) return FALSE;

	/* Certain numbers increase the chance of having a labyrinth */
	if (c->depth % 3 == 0) chance += 1;
	if (c->depth % 5 == 0) chance += 1;
	if (c->depth % 7 == 0) chance += 1;
	if (c->depth % 11 == 0) chance += 1;
	if (c->depth % 13 == 0) chance += 1;

	/* Only generate the level if we pass a check */
	/* NOTE: This test gets performed after we pass the test to use the
  	  * labyrinth cave profile. */
	if (randint0(100) >= chance) return FALSE;

	/* allocate our arrays */
	sets = C_ZNEW(n, int);
	walls = C_ZNEW(n, int);
	
	/* This is the dungeon size, which does include the enclosing walls */
	set_cave_dimensions(c, h + 2, w + 2);
	
	/* Fill whole level with perma-rock */
	fill_rectangle(c, 0, 0, DUNGEON_HGT - 1, DUNGEON_WID - 1, FEAT_PERM_SOLID);
	
	/* Fill the labyrinth area with rock */
	fill_rectangle(c, 1, 1, h, w, soft ? FEAT_WALL_SOLID : FEAT_PERM_SOLID);
	
	/* Initialize each wall. */
	for (i = 0; i < n; i++) {
		walls[i] = i;
		sets[i] = -1;
	}

	/* Cut out a grid of 1x1 rooms which we will call "cells" */
	for (y = 0; y < h; y += 2) {
		for (x = 0; x < w; x += 2) {
			int k = lab_toi(y, x, w);
			sets[k] = k;
			cave_set_feat(c, y + 1, x + 1, FEAT_FLOOR);
			if (lit) c->info[y + 1][x + 1] |= CAVE_GLOW;
		}
	}
	
	/* Shuffle the walls, using Knuth's shuffle. */
	shuffle(walls, n);
	
	/* For each adjoining wall, look at the cells it divides. If they aren't
	 * in the same set, remove the wall and join their sets.
	 *
	 * This is a randomized version of Kruskal's algorithm. */
	for (i = 0; i < n; i++) {
		int a, b, x, y;

		j = walls[i];

		/* If this cell isn't an adjoining wall, skip it */
		lab_toyx(j, w, &y, &x);
		if ((x < 1 && y < 1) || (x > w - 2 && y > h - 2)) continue;
		if (x % 2 == y % 2) continue;
		
		/* Figure out which cells are separated by this wall */
		lab_get_adjoin(j, w, &a, &b);
		
		/* If the cells aren't connected, kill the wall and join the sets */
		if (sets[a] != sets[b]) {
			int sa = sets[a];
			int sb = sets[b];
			cave_set_feat(c, y + 1, x + 1, FEAT_FLOOR);
			if (lit) c->info[y + 1][x + 1] |= CAVE_GLOW;
			
			for (k = 0; k < n; k++) {
				if (sets[k] == sb) sets[k] = sa;
			}
		}
	}
	
	/* Determine the character location */
	new_player_spot(c, p);
	
	/* The level should have exactly one down and one up staircase */
	if (OPT(birth_no_stairs)) {
		/* new_player_spot() won't have created stairs, so make both*/
		alloc_stairs(c, FEAT_MORE, 1, 3);
		alloc_stairs(c, FEAT_LESS, 1, 3);
	} else if (p->create_down_stair) {
		/* new_player_spot() will have created down, so only create up */
		alloc_stairs(c, FEAT_LESS, 1, 3);
	} else {
		/* new_player_spot() will have created up, so only create down */
		alloc_stairs(c, FEAT_MORE, 1, 3);
	}
	
	/* Generate a door for every 100 squares in the labyrinth */
	for (i = n / 100; i > 0; i--) {
		/* Try 10 times to find a useful place for a door, then place it */
		for (j = 0; j < 10; j++) {
			find_empty(c, &y, &x);
			if (lab_is_tunnel(c, y, x)) break;
		}

		place_closed_door(c, y, x);
	}

	/* General some rubble, traps and monsters */
	k = MAX(MIN(c->depth / 3, 10), 2);

	/* Scale number of monsters items by labyrinth size */
	k = (3 * k * (h * w)) / (DUNGEON_HGT * DUNGEON_WID);
	
	/* Put some rubble in corridors */
	alloc_objects(c, SET_BOTH, TYP_RUBBLE, randint1(k), c->depth, 0);
	
	/* Place some traps in the dungeon */
	alloc_objects(c, SET_BOTH, TYP_TRAP, randint1(k), c->depth, 0);
	
	/* Put some monsters in the dungeon */
	for (i = MIN_M_ALLOC_LEVEL + randint1(8) + k; i > 0; i--)
		pick_and_place_distant_monster(c, loc(p->px, p->py), 0, TRUE, c->depth);
	
	/* Put some objects/gold in the dungeon */
	alloc_objects(c, SET_BOTH, TYP_OBJECT, Rand_normal(6, 3), c->depth, ORIGIN_LABYRINTH);
	alloc_objects(c, SET_BOTH, TYP_GOLD, Rand_normal(6, 3), c->depth, ORIGIN_LABYRINTH);
	alloc_objects(c, SET_BOTH, TYP_GOOD, randint0(2), c->depth, ORIGIN_LABYRINTH);
	
	/* Unlit labyrinths will have some good items */
	if (!lit)
		alloc_objects(c, SET_BOTH, TYP_GOOD, Rand_normal(3, 2), c->depth, ORIGIN_LABYRINTH);

	/* Hard (non-diggable) labyrinths will have some great items */
	if (!soft)
		alloc_objects(c, SET_BOTH, TYP_GREAT, Rand_normal(2, 1), c->depth, ORIGIN_LABYRINTH);

	/* If we want the players to see the maze layout, do that now */
	if (known) wiz_light();

	/* Deallocate our lists */
	FREE(sets);
	FREE(walls);

	return TRUE;
}
