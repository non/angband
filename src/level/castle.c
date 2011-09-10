/*
 * File: castle.c
 * Purpose: Castle generation.
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
 * Draw a parapet located at (x,y) with the given radius.
 */
void draw_parapet(struct cave *c, int y, int x, int r)
{
	int y1 = y - r, y2 = y + r, x1 = x - r, x2 = x + r;

	/* TODO: in some cases we should round off the edges */
	draw_rectangle(c, y1, x1, y2, x2, FEAT_WALL_SOLID);
	fill_rectangle(c, y1 + 1, x1 + 1, y2 - 1, x2 - 1, FEAT_FLOOR);
}


/**
 *
 */
void draw_vertical_rampart(struct cave *c, int y1, int y2, int x, int r, int off)
{
	int x1 = x - r, x2 = x + r;
	draw_rectangle(c, y1, x1, y2, x2, FEAT_WALL_SOLID);
	fill_rectangle(c, y1, x1 + 1, y2, x2 - 1, FEAT_FLOOR);
}


/**
 *
 */
void draw_horizontal_rampart(struct cave *c, int x1, int x2, int y, int r, int off)
{
	int y1 = y - r, y2 = y + r;
	draw_rectangle(c, y1, x1, y2, x2, FEAT_WALL_SOLID);
	fill_rectangle(c, y1 + 1, x1, y2 - 1, x2, FEAT_FLOOR);
}


/**
 *
 */
void draw_entrance(struct cave *c, int y, int x, int r, int num, int width)
{
	int i;
	for (i = 0; i < width; i++) {
		if (i < num) {
			place_closed_door(c, y - r, x - i);
			place_closed_door(c, y + r, x - i);
			place_closed_door(c, y - r, x + i);
			place_closed_door(c, y + r, x + i);
		} else {
			cave_set_feat(c , y - r, x - i, FEAT_WALL_SOLID);
			cave_set_feat(c , y + r, x - i, FEAT_WALL_SOLID);
			cave_set_feat(c , y - r, x + i, FEAT_WALL_SOLID);
			cave_set_feat(c , y + r, x + i, FEAT_WALL_SOLID);
		}
	}
	place_closed_door(c, y, x - num - 1);
	place_closed_door(c, y, x + num + 1);
}


/**
 * Divide 'amount' evenly between intervals in a (mostly) symmetrical way.
 *
 * This is used to try to evenly space things along walls and other places
 * where the number of things to be spaced and the amount of space can both
 * be variable.
 */
void fit_intervals(int *intervals, int n, int amount)
{
	int i;
	int middle = (n + 1) / 2;
	int quot = amount / n;
	int rem = amount % n;

	for (i = 0; i < intervals; i++) intervals[i] = quot;

	if (rem % 2) {
		intervals[middle]++;
		rem--;
	}

	for (i = 0; rem > 0 && i < middle; i++) {
		intervals[i] = intervals[n - i - 1] = quot + 1;
		rem -= 2;
	}
}


/**
 *
 */
void draw_outer_wall(struct cave *c, int y1, int x1, int y2, int x2)
{
	int r = 2;
	int h = y2 - y1, w = x2 - x1;
	int h2 = h / 2, w2 = w / 2;

	draw_parapet(c, y1, x1, r);
	draw_parapet(c, y2, x1, r);
	draw_parapet(c, y1, x2, r);
	draw_parapet(c, y2, x2, r);

	draw_parapet(c, y2, x1 + (w / 2) - r - 3, r);
	draw_parapet(c, y2, x1 + (w / 2) + r + 3, r);

	draw_entrance(c, y2, x1 + w2, r - 1, 2, 3);

	draw_vertical_rampart(c, y1 + r, y2 - r, x1, r - 1, -1);
	draw_vertical_rampart(c, y1 + r, y2 - r, x2, r - 1, 1);
	draw_horizontal_rampart(c, x1 + r, x2 - r, y1, r - 1, -1);

	draw_horizontal_rampart(c, x1 + r, x1 + w2 - r * 3 - 1, y2, r - 1, 1);
	draw_horizontal_rampart(c, x1 + w2 + r * 3 + 1, x2 - r, y2, r - 1, 1);
}

bool castle_gen(struct cave *c, struct player *p)
{
    int i, k, h, w, oy, ox;

	int ch = 21;
	int cy1 = 10;
	int cy2 = cy1 + ch;

	int cw = 44;
	int cx1 = 10;
	int cx2 = cx1 + cw;

    i = randint1(4) + 6;

	h = rand_range(DUNGEON_HGT * i / 10, DUNGEON_HGT);
	w = rand_range(DUNGEON_WID * i / 12, DUNGEON_WID);

    oy = (DUNGEON_HGT - h) / 2;
    ox = (DUNGEON_WID - w) / 2;

	set_cave_dimensions(c, h, w);

	/* Fill the edges with perma-rock, and rest open (for now) */
	draw_rectangle(c, 0, 0, DUNGEON_HGT - 1, DUNGEON_WID - 1, FEAT_PERM_SOLID);
	fill_rectangle(c, 1, 1, DUNGEON_HGT - 2, DUNGEON_WID - 2, FEAT_FLOOR);

	draw_outer_wall(c, cy1, cx1, cy2, cx2);

    ///* Place 2-3 down stairs near some walls */
    //alloc_stairs(c, FEAT_MORE, rand_range(1, 3), 3);
	//  	
    ///* Place 1-2 up stairs near some walls */
    //alloc_stairs(c, FEAT_LESS, rand_range(1, 2), 3);
    //
    ///* General some rubble, traps and monsters */
    //k = MAX(MIN(c->depth / 3, 10), 2);
	//  	
    ///* Scale number of monsters items by cavern size */
    //k = (2 * k * (h *  w)) / (DUNGEON_HGT * DUNGEON_WID);
	//  	
    ///* Put some rubble in corridors */
    //alloc_objects(c, SET_BOTH, TYP_RUBBLE, randint1(k), c->depth, 0);
	//  	
    ///* Place some traps in the dungeon */
    //alloc_objects(c, SET_BOTH, TYP_TRAP, randint1(k), c->depth, 0);
	  	
    /* Determine the character location */
    //new_player_spot(c, p);
	player_place(c, p, (cy1 + cy2) / 2, (cx1 + cx2) / 2);
	  	
    ///* Put some monsters in the dungeon */
    //for (i = MIN_M_ALLOC_LEVEL + randint1(8) + k; i > 0; i--)
    //    pick_and_place_distant_monster(c, loc(p->px, p->py), 0, TRUE, c->depth);
	//  	
    ///* Put some objects/gold in the dungeon */
    //alloc_objects(c, SET_BOTH, TYP_OBJECT, Rand_normal(6, 3), c->depth, ORIGIN_CAVERN);
    //alloc_objects(c, SET_BOTH, TYP_GOLD, Rand_normal(6, 3), c->depth, ORIGIN_CAVERN);
    //alloc_objects(c, SET_BOTH, TYP_GOOD, randint0(2), c->depth, ORIGIN_CAVERN);

    return TRUE;
}
