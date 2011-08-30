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
#include "level/generate.h"
#include "level/cavern.h"
#include "monster/mon-make.h"
#include "monster/mon-spell.h"
#include "object/tvalsval.h"
#include "trap.h"
#include "z-queue.h"
#include "z-type.h"

bool castle_gen(struct cave *c, struct player *p)
{
    int i, k, h, w, oy, ox;

    i = randint1(4) + 6;

	  	h = rand_range(DUNGEON_HGT * i / 10, DUNGEON_HGT);
	  	w = rand_range(DUNGEON_WID * i / 12, DUNGEON_WID);

    oy = (DUNGEON_HGT - h) / 2;
    ox = (DUNGEON_WID - w) / 2;

	  	set_cave_dimensions(c, h, w);

	  	/* Fill the edges with perma-rock, and rest with rock */
	  	draw_rectangle(c, 0, 0, DUNGEON_HGT - 1, DUNGEON_WID - 1, FEAT_PERM_SOLID);
	  	fill_rectangle(c, 1, 1, DUNGEON_HGT - 2, DUNGEON_WID - 2, FEAT_WALL_SOLID);
	  	fill_rectangle(c, oy, ox, h - 2, w - 2, FEAT_FLOOR);

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
    alloc_objects(c, SET_BOTH, TYP_OBJECT, Rand_normal(6, 3), c->depth, ORIGIN_CAVERN);
    alloc_objects(c, SET_BOTH, TYP_GOLD, Rand_normal(6, 3), c->depth, ORIGIN_CAVERN);
    alloc_objects(c, SET_BOTH, TYP_GOOD, randint0(2), c->depth, ORIGIN_CAVERN);

    return TRUE;
}
