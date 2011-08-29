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
#include "level/labyrinth.h"
#include "level/town.h"
#include "level/room.h"
#include "monster/mon-make.h"
#include "monster/mon-spell.h"
#include "object/tvalsval.h"
#include "trap.h"
#include "z-queue.h"
#include "z-type.h"

static bool default_gen(struct cave *c, struct player *p);


/**
 * Note that Level generation is *not* an important bottleneck, though it can
 * be annoyingly slow on older machines...  Thus we emphasize "simplicity" and
 * "correctness" over "speed".
 *
 * See the "vault.txt" file for more on vault generation.
 *
 * In this file, we use the "special" granite and perma-wall sub-types, where
 * "basic" is normal, "inner" is inside a room, "outer" is the outer wall of a
 * room, and "solid" is the outer wall of the dungeon or any walls that may not
 * be pierced by corridors.  Thus the only wall type that may be pierced by a
 * corridor is the "outer granite" type. The "basic granite" type yields the
 * "actual" corridors.
 *
 * We use the special "solid" granite wall type to prevent multiple corridors
 * from piercing a wall in two adjacent locations, which would be messy, and we
 * use the special "outer" granite wall to indicate which walls "surround"
 * rooms, and may thus be "pierced" by corridors entering or leaving the room.
 *
 * Note that a tunnel which attempts to leave a room near the "edge" of the
 * dungeon in a direction toward that edge will cause "silly" wall piercings,
 * but will have no permanently incorrect effects, as long as the tunnel can
 * eventually exit from another side. And note that the wall may not come back
 * into the room by the hole it left through, so it must bend to the left or
 * right and then optionally re-enter the room (at least 2 grids away). This is
 * not a problem since every room that is large enough to block the passage of
 * tunnels is also large enough to allow the tunnel to pierce the room itself
 * several times.
 *
 * Note that no two corridors may enter a room through adjacent grids, they
 * must either share an entryway or else use entryways at least two grids
 * apart. This prevents "large" (or "silly") doorways.
 *
 * To create rooms in the dungeon, we first divide the dungeon up into "blocks"
 * of 11x11 grids each, and require that all rooms occupy a rectangular group
 * of blocks.  As long as each room type reserves a sufficient number of
 * blocks, the room building routines will not need to check bounds. Note that
 * most of the normal rooms actually only use 23x11 grids, and so reserve 33x11
 * grids.
 *
 * Note that the use of 11x11 blocks (instead of the 33x11 panels) allows more
 * variability in the horizontal placement of rooms, and at the same time has
 * the disadvantage that some rooms (two thirds of the normal rooms) may be
 * "split" by panel boundaries.  This can induce a situation where a player is
 * in a room and part of the room is off the screen.  This can be so annoying
 * that the player must set a special option to enable "non-aligned" room
 * generation.
 *
 * The 64 new "dungeon features" will also be used for "visual display"
 * but we must be careful not to allow, for example, the user to display
 * hidden traps in a different way from floors, or secret doors in a way
 * different from granite walls.
 */


/**
 * This is the global structure representing dungeon generation info.
 */
static struct dun_data *dun;

struct dun_data *get_dun(void)
{
	return dun;
}

/**
 * Profile used for generating the town level.
 */
static struct cave_profile town_profile = {
	/* name builder dun_rooms dun_unusual max_rarity n_room_profiles */
	"town-default", town_gen, 50, 200, 2, 0,

	/* name rnd chg con pen jct */
	{"tunnel-default", 10, 30, 15, 25, 90},

	/* name den rng mag mc qua qc */
	{"streamer-default", 5, 2, 3, 90, 2, 40},

	/* room_profiles -- not applicable */
	NULL,

	/* cutoff -- not applicable */
	0
};


/* name function width height min-depth crowded? rarity %cutoff */
static struct room_profile default_rooms[] = {
	/* greater vaults only have rarity 1 but they have other checks */
	{"greater vault", build_greater_vault, 4, 6, 10, FALSE, 1, 100},

	/* very rare rooms (rarity=2) */
	{"medium vault", build_medium_vault, 2, 3, 5, FALSE, 2, 10},
	{"lesser vault", build_lesser_vault, 2, 3, 5, FALSE, 2, 25},
	{"monster pit", build_pit, 1, 3, 5, TRUE, 2, 40},
	{"monster nest", build_nest, 1, 3, 5, TRUE, 2, 50},

	/* unusual rooms (rarity=1) */
	{"large room", build_large, 1, 3, 3, FALSE, 1, 25},
	{"crossed room", build_crossed, 1, 3, 3, FALSE, 1, 50},
	{"circular room", build_circular, 2, 2, 1, FALSE, 1, 60},
	{"overlap room", build_overlap, 1, 3, 1, FALSE, 1, 100},

	/* normal rooms */
	{"simple room", build_simple, 1, 3, 1, FALSE, 0, 100}
};


#define NUM_CAVE_PROFILES 3

/**
 * Profiles used for generating dungeon levels.
 */
static struct cave_profile cave_profiles[NUM_CAVE_PROFILES] = {
	{
		"labyrinth", labyrinth_gen, 0, 200, 0, 0,

		/* tunnels -- not applicable */
		{"tunnel-default", 10, 30, 15, 25, 90},

		/* streamers -- not applicable */
		{"streamer-default", 5, 2, 3, 90, 2, 40},

		/* room_profiles -- not applicable */
		NULL,

		/* cutoff -- unused because of internal checks in labyrinth_gen  */
		100
	},
	{
		"cavern", cavern_gen, 0, 200, 0, 0,

		/* tunnels -- not applicable */
		{"tunnel-default", 10, 30, 15, 25, 90},

		/* streamers -- not applicable */
		{"streamer-default", 5, 2, 3, 90, 2, 40},

		/* room_profiles -- not applicable */
		NULL,

		/* cutoff -- debug  */
		10
	},
	{
		/* name builder dun_rooms dun_unusual max_rarity n_room_profiles */
		"default", default_gen, 50, 200, 2, N_ELEMENTS(default_rooms),

		/* name rnd chg con pen jct */
		{"tunnel-default", 10, 30, 15, 25, 90},

		/* name den rng mag mc qua qc */
		{"streamer-default", 5, 2, 3, 90, 2, 40},

		/* room_profiles */
		default_rooms,

		/* cutoff */
		100
	}
};


/**
 * Places a streamer of rock through dungeon.
 *
 * Note that their are actually six different terrain features used to
 * represent streamers. Three each of magma and quartz, one for basic vein, one
 * with hidden gold, and one with known gold. The hidden gold types are
 * currently unused.
 */
static void build_streamer(struct cave *c, int feat, int chance)
{
	int i, tx, ty;
	int y, x, dir;

	/* Hack -- Choose starting point */
	y = rand_spread(DUNGEON_HGT / 2, 10);
	x = rand_spread(DUNGEON_WID / 2, 15);

	/* Choose a random direction */
	dir = ddd[randint0(8)];

	/* Place streamer into dungeon */
	while (TRUE) {
		/* One grid per density */
		for (i = 0; i < dun->profile->str.den; i++) {
			int d = dun->profile->str.rng;

			/* Pick a nearby grid */
			find_nearby_grid(c, &ty, y, d, &tx, x, d);

			/* Only convert walls */
			if (cave_isrock(c, ty, tx)) {
				/* Turn the rock into the vein type */
				cave_set_feat(c, ty, tx, feat);

				/* Sometimes add known treasure */
				if (one_in_(chance)) upgrade_mineral(c, ty, tx);
			}
		}

		/* Advance the streamer */
		y += ddy[dir];
		x += ddx[dir];

		/* Stop at dungeon edge */
		if (!cave_in_bounds(c, y, x)) break;
	}
}



/**
 * Constructs a tunnel between two points
 *
 * This function must be called BEFORE any streamers are created, since we use
 * the special "granite wall" sub-types to keep track of legal places for
 * corridors to pierce rooms.
 *
 * We queue the tunnel grids to prevent door creation along a corridor which
 * intersects itself.
 *
 * We queue the wall piercing grids to prevent a corridor from leaving
 * a room and then coming back in through the same entrance.
 *
 * We pierce grids which are outer walls of rooms, and when we do so, we change
 * all adjacent outer walls of rooms into solid walls so that no two corridors
 * may use adjacent grids for exits.
 *
 * The solid wall check prevents corridors from chopping the corners of rooms
 * off, as well as silly door placement, and excessively wide room entrances.
 */
static void build_tunnel(struct cave *c, int row1, int col1, int row2, int col2)
{
	int i, y, x;
	int tmp_row, tmp_col;
	int row_dir, col_dir;
	int start_row, start_col;
	int main_loop_count = 0;

	/* Used to prevent excessive door creation along overlapping corridors. */
	bool door_flag = FALSE;
	
	/* Reset the arrays */
	dun->tunn_n = 0;
	dun->wall_n = 0;
	
	/* Save the starting location */
	start_row = row1;
	start_col = col1;

	/* Start out in the correct direction */
	correct_dir(&row_dir, &col_dir, row1, col1, row2, col2);

	/* Keep going until done (or bored) */
	while ((row1 != row2) || (col1 != col2)) {
		/* Mega-Hack -- Paranoia -- prevent infinite loops */
		if (main_loop_count++ > 2000) break;

		/* Allow bends in the tunnel */
		if (randint0(100) < dun->profile->tun.chg) {
			/* Get the correct direction */
			correct_dir(&row_dir, &col_dir, row1, col1, row2, col2);

			/* Random direction */
			if (randint0(100) < dun->profile->tun.rnd)
				rand_dir(&row_dir, &col_dir);
		}

		/* Get the next location */
		tmp_row = row1 + row_dir;
		tmp_col = col1 + col_dir;

		while (!cave_in_bounds(c, tmp_row, tmp_col)) {
			/* Get the correct direction */
			correct_dir(&row_dir, &col_dir, row1, col1, row2, col2);

			/* Random direction */
			if (randint0(100) < dun->profile->tun.rnd)
				rand_dir(&row_dir, &col_dir);

			/* Get the next location */
			tmp_row = row1 + row_dir;
			tmp_col = col1 + col_dir;
		}


		/* Avoid the edge of the dungeon */
		if (cave_isperm(c, tmp_row, tmp_col)) continue;

		/* Avoid "solid" granite walls */
		if (c->feat[tmp_row][tmp_col] == FEAT_WALL_SOLID) continue;

		/* Pierce "outer" walls of rooms */
		if (c->feat[tmp_row][tmp_col] == FEAT_WALL_OUTER) {
			/* Get the "next" location */
			y = tmp_row + row_dir;
			x = tmp_col + col_dir;

			/* Hack -- Avoid outer/solid permanent walls */
			if (c->feat[y][x] == FEAT_PERM_SOLID) continue;
			if (c->feat[y][x] == FEAT_PERM_OUTER) continue;

			/* Hack -- Avoid outer/solid granite walls */
			if (c->feat[y][x] == FEAT_WALL_OUTER) continue;
			if (c->feat[y][x] == FEAT_WALL_SOLID) continue;

			/* Accept this location */
			row1 = tmp_row;
			col1 = tmp_col;

			/* Save the wall location */
			if (dun->wall_n < WALL_MAX) {
				dun->wall[dun->wall_n].y = row1;
				dun->wall[dun->wall_n].x = col1;
				dun->wall_n++;
			}

			/* Forbid re-entry near this piercing */
			for (y = row1 - 1; y <= row1 + 1; y++)
				for (x = col1 - 1; x <= col1 + 1; x++)
					if (c->feat[y][x] == FEAT_WALL_OUTER)
						cave_set_feat(c, y, x, FEAT_WALL_SOLID);

		} else if (c->info[tmp_row][tmp_col] & (CAVE_ROOM)) {
			/* Travel quickly through rooms */
			/* Accept the location */
			row1 = tmp_row;
			col1 = tmp_col;

		} else if (c->feat[tmp_row][tmp_col] >= FEAT_WALL_EXTRA) {
			/* Tunnel through all other walls */
			/* Accept this location */
			row1 = tmp_row;
			col1 = tmp_col;

			/* Save the tunnel location */
			if (dun->tunn_n < TUNN_MAX) {
				dun->tunn[dun->tunn_n].y = row1;
				dun->tunn[dun->tunn_n].x = col1;
				dun->tunn_n++;
			}

			/* Allow door in next grid */
			door_flag = FALSE;

		} else {
			/* Handle corridor intersections or overlaps */
			/* Accept the location */
			row1 = tmp_row;
			col1 = tmp_col;

			/* Collect legal door locations */
			if (!door_flag) {
				/* Save the door location */
				if (dun->door_n < DOOR_MAX) {
					dun->door[dun->door_n].y = row1;
					dun->door[dun->door_n].x = col1;
					dun->door_n++;
				}

				/* No door in next grid */
				door_flag = TRUE;
			}

			/* Hack -- allow pre-emptive tunnel termination */
			if (randint0(100) >= dun->profile->tun.con) {
				/* Distance between row1 and start_row */
				tmp_row = row1 - start_row;
				if (tmp_row < 0) tmp_row = (-tmp_row);

				/* Distance between col1 and start_col */
				tmp_col = col1 - start_col;
				if (tmp_col < 0) tmp_col = (-tmp_col);

				/* Terminate the tunnel */
				if ((tmp_row > 10) || (tmp_col > 10)) break;
			}
		}
	}


	/* Turn the tunnel into corridor */
	for (i = 0; i < dun->tunn_n; i++) {
		/* Get the grid */
		y = dun->tunn[i].y;
		x = dun->tunn[i].x;

		/* Clear previous contents, add a floor */
		cave_set_feat(c, y, x, FEAT_FLOOR);
	}


	/* Apply the piercings that we found */
	for (i = 0; i < dun->wall_n; i++) {
		/* Get the grid */
		y = dun->wall[i].y;
		x = dun->wall[i].x;

		/* Convert to floor grid */
		cave_set_feat(c, y, x, FEAT_FLOOR);

		/* Place a random door */
		if (randint0(100) < dun->profile->tun.pen)
			place_random_door(c, y, x);
	}
}

/**
 * Count the number of corridor grids adjacent to the given grid.
 *
 * This routine currently only counts actual "empty floor" grids which are not
 * in rooms.
 *
 * TODO: count stairs, open doors, closed doors?
 */
static int next_to_corr(struct cave *c, int y1, int x1)
{
	int i, k = 0;
	assert(cave_in_bounds(c, y1, x1));

	/* Scan adjacent grids */
	for (i = 0; i < 4; i++) {
		/* Extract the location */
		int y = y1 + ddy_ddd[i];
		int x = x1 + ddx_ddd[i];

		/* Count only floors which aren't part of rooms */
		if (cave_isfloor(c, y, x) && !cave_isroom(c, y, x)) k++;
	}

	/* Return the number of corridors */
	return k;
}

/**
 * Returns whether a doorway can be built in a space.
 *
 * To have a doorway, a space must be adjacent to at least two corridors and be
 * between two walls.
 */
static bool possible_doorway(struct cave *c, int y, int x)
{
	assert(cave_in_bounds(c, y, x));
	if (next_to_corr(c, y, x) < 2)
		return FALSE;
	else if (cave_isstrongwall(c, y - 1, x) && cave_isstrongwall(c, y + 1, x))
		return TRUE;
	else if (cave_isstrongwall(c, y, x - 1) && cave_isstrongwall(c, y, x + 1))
		return TRUE;
	else
		return FALSE;
}


/**
 * Places door at y, x position if at least 2 walls found
 */
static void try_door(struct cave *c, int y, int x)
{
	assert(cave_in_bounds(c, y, x));

	if (cave_isstrongwall(c, y, x)) return;
	if (cave_isroom(c, y, x)) return;

	if (randint0(100) < dun->profile->tun.jct && possible_doorway(c, y, x))
		place_random_door(c, y, x);
}


/**
 * Attempt to build a room of the given type at the given block
 *
 * Note that we restrict the number of "crowded" rooms to reduce
 * the chance of overflowing the monster list during level creation.
 */
static bool room_build(struct cave *c, int by0, int bx0, struct room_profile profile)
{
	/* Extract blocks */
	int by1 = by0;
	int bx1 = bx0;
	int by2 = by0 + profile.height;
	int bx2 = bx0 + profile.width;

	int allocated;
	int y, x;
	int by, bx;

	/* Enforce the room profile's minimum depth */
	if (c->depth < profile.level) return FALSE;

	/* Only allow one crowded room per level */
	if (dun->crowded && profile.crowded) return FALSE;

	/* Never run off the screen */
	if (by1 < 0 || by2 >= dun->row_rooms) return FALSE;
	if (bx1 < 0 || bx2 >= dun->col_rooms) return FALSE;

	/* Verify open space */
	for (by = by1; by <= by2; by++) {
		for (bx = bx1; bx <= bx2; bx++) {
			if (1) {
				/* previous rooms prevent new ones */
				if (dun->room_map[by][bx]) return FALSE;
			} else {
				return FALSE; /* XYZ */
			}
		}
	}

	/* Get the location of the room */
	y = ((by1 + by2 + 1) * BLOCK_HGT) / 2;
	x = ((bx1 + bx2 + 1) * BLOCK_WID) / 2;

	/* Try to build a room */
	if (!profile.builder(c, y, x)) return FALSE;

	/* Save the room location */
	if (dun->cent_n < CENT_MAX) {
		dun->cent[dun->cent_n].y = y;
		dun->cent[dun->cent_n].x = x;
		dun->cent_n++;
	}

	/* Reserve some blocks */
	allocated = 0;
	for (by = by1; by < by2; by++) {
		for (bx = bx1; bx < bx2; bx++) {
			dun->room_map[by][bx] = TRUE;
			allocated++;
		}
	}

	/* Count "crowded" rooms */
	if (profile.crowded) dun->crowded = TRUE;

	/* Success */
	return TRUE;
}


/**
 * Generate a new dungeon level.
 */
#define DUN_AMT_ROOM 7 /* Number of objects for rooms */
#define DUN_AMT_ITEM 2 /* Number of objects for rooms/corridors */
#define DUN_AMT_GOLD 3 /* Amount of treasure for rooms/corridors */
static bool default_gen(struct cave *c, struct player *p)
{
	int i, j, k, y, x, y1, x1;
	int by, bx = 0, tby, tbx, key, rarity, built;
	int num_rooms, size_percent;
	int dun_unusual = dun->profile->dun_unusual;

	bool blocks_tried[MAX_ROOMS_ROW][MAX_ROOMS_COL];
	for (by = 0; by < MAX_ROOMS_ROW; by++) {
		for (bx = 0; bx < MAX_ROOMS_COL; bx++) {
			blocks_tried[by][bx] = FALSE;
		}
	}

	/* Possibly generate fewer rooms in a smaller area via a scaling factor.
	 * Since we scale row_rooms and col_rooms by the same amount, DUN_ROOMS
	 * gives the same "room density" no matter what size the level turns out
	 * to be. TODO: vary room density slightly? */
	i = randint1(10) + c->depth / 24;
	if (is_quest(c->depth)) size_percent = 100;
	else if (i < 2) size_percent = 75;
	else if (i < 3) size_percent = 80;
	else if (i < 4) size_percent = 85;
	else if (i < 5) size_percent = 90;
	else if (i < 6) size_percent = 95;
	else size_percent = 100;

	/* scale the various generation variables */
	num_rooms = (dun->profile->dun_rooms * size_percent) / 100;
	set_cave_dimensions(c, DUNGEON_HGT, DUNGEON_WID);
	//ROOM_LOG("height=%d  width=%d  nrooms=%d", c->height, c->width, num_rooms);

	/* Initially fill with basic granite */
	fill_rectangle(c, 0, 0, DUNGEON_HGT - 1, DUNGEON_WID - 1, FEAT_WALL_EXTRA);

	/* Actual maximum number of rooms on this level */
	dun->row_rooms = c->height / BLOCK_HGT;
	dun->col_rooms = c->width / BLOCK_WID;

	/* Initialize the room table */
	for (by = 0; by < dun->row_rooms; by++)
		for (bx = 0; bx < dun->col_rooms; bx++)
			dun->room_map[by][bx] = blocks_tried[by][bx]  = FALSE;

	/* No rooms yet, crowded or otherwise. */
	dun->crowded = FALSE;
	dun->cent_n = 0;

	/* Build some rooms */
	built = 0;
	while(built < num_rooms) {

		/* Count the room blocks we haven't tried yet. */
		j = 0;
		tby = 0;
		tbx = 0;
		for(by = 0; by < dun->row_rooms; by++) {
			for(bx = 0; bx < dun->col_rooms; bx++) {
				if (blocks_tried[by][bx]) continue;
				j++;
				if (one_in_(j)) {
					tby = by;
					tbx = bx;
				}
			} 
		}
		bx = tbx;
		by = tby;

		/* If we've tried all blocks we're done. */
		if (j == 0) break;

		if (blocks_tried[by][bx]) quit_fmt("generation: inconsistent blocks");

		/* Mark that we are trying this block. */
		blocks_tried[by][bx] = TRUE;

		/* Roll for random key (to be compared against a profile's cutoff) */
		key = randint0(100);

		/* We generate a rarity number to figure out how exotic to make the
		 * room. This number has a depth/DUN_UNUSUAL chance of being > 0,
		 * a depth^2/DUN_UNUSUAL^2 chance of being > 1, up to MAX_RARITY. */
		i = 0;
		rarity = 0;
		while (i == rarity && i < dun->profile->max_rarity) {
			if (randint0(dun_unusual) < c->depth) rarity++;
			i++;
		}

		/* Once we have a key and a rarity, we iterate through out list of
		 * room profiles looking for a match (whose cutoff > key and whose
		 * rarity > this rarity). We try building the room, and if it works
		 * then we are done with this iteration. We keep going until we find
		 * a room that we can build successfully or we exhaust the profiles. */
		i = 0;
		for (i = 0; i < dun->profile->n_room_profiles; i++) {
			struct room_profile profile = dun->profile->room_profiles[i];
			if (profile.rarity > rarity) continue;
			if (profile.cutoff <= key) continue;
			
			if (room_build(c, by, bx, profile)) {
				built++;
				break;
			}
		}
	}

	/* Generate permanent walls around the edge of the dungeon */
	draw_rectangle(c, 0, 0, DUNGEON_HGT - 1, DUNGEON_WID - 1, FEAT_PERM_SOLID);

	/* Hack -- Scramble the room order */
	for (i = 0; i < dun->cent_n; i++) {
		int pick1 = randint0(dun->cent_n);
		int pick2 = randint0(dun->cent_n);
		y1 = dun->cent[pick1].y;
		x1 = dun->cent[pick1].x;
		dun->cent[pick1].y = dun->cent[pick2].y;
		dun->cent[pick1].x = dun->cent[pick2].x;
		dun->cent[pick2].y = y1;
		dun->cent[pick2].x = x1;
	}

	/* Start with no tunnel doors */
	dun->door_n = 0;

	/* Hack -- connect the first room to the last room */
	y = dun->cent[dun->cent_n-1].y;
	x = dun->cent[dun->cent_n-1].x;

	/* Connect all the rooms together */
	for (i = 0; i < dun->cent_n; i++) {
		/* Connect the room to the previous room */
		build_tunnel(c, dun->cent[i].y, dun->cent[i].x, y, x);

		/* Remember the "previous" room */
		y = dun->cent[i].y;
		x = dun->cent[i].x;
	}

	/* Place intersection doors */
	for (i = 0; i < dun->door_n; i++) {
		/* Extract junction location */
		y = dun->door[i].y;
		x = dun->door[i].x;

		/* Try placing doors */
		try_door(c, y, x - 1);
		try_door(c, y, x + 1);
		try_door(c, y - 1, x);
		try_door(c, y + 1, x);
	}

	ensure_connectedness(c);

	/* Add some magma streamers */
	for (i = 0; i < dun->profile->str.mag; i++)
		build_streamer(c, FEAT_MAGMA, dun->profile->str.mc);

	/* Add some quartz streamers */
	for (i = 0; i < dun->profile->str.qua; i++)
		build_streamer(c, FEAT_QUARTZ, dun->profile->str.qc);

	/* Place 3 or 4 down stairs near some walls */
	alloc_stairs(c, FEAT_MORE, rand_range(3, 4), 3);

	/* Place 1 or 2 up stairs near some walls */
	alloc_stairs(c, FEAT_LESS, rand_range(1, 2), 3);

	/* General amount of rubble, traps and monsters */
	k = MAX(MIN(c->depth / 3, 10), 2);

	/* Put some rubble in corridors */
	alloc_objects(c, SET_CORR, TYP_RUBBLE, randint1(k), c->depth, 0);

	/* Place some traps in the dungeon */
	alloc_objects(c, SET_BOTH, TYP_TRAP, randint1(k), c->depth, 0);

	/* Determine the character location */
	new_player_spot(c, p);

	/* Pick a base number of monsters */
	i = MIN_M_ALLOC_LEVEL + randint1(8) + k;

	/* Put some monsters in the dungeon */
	for (; i > 0; i--)
		pick_and_place_distant_monster(c, loc(p->px, p->py), 0, TRUE, c->depth);

	/* Put some objects in rooms */
	alloc_objects(c, SET_ROOM, TYP_OBJECT, Rand_normal(DUN_AMT_ROOM, 3),
		c->depth, ORIGIN_FLOOR);

	/* Put some objects/gold in the dungeon */
	alloc_objects(c, SET_BOTH, TYP_OBJECT, Rand_normal(DUN_AMT_ITEM, 3),
		c->depth, ORIGIN_FLOOR);
	alloc_objects(c, SET_BOTH, TYP_GOLD, Rand_normal(DUN_AMT_GOLD, 3),
		c->depth, ORIGIN_FLOOR);

	return TRUE;
}




/**
 * Clear the dungeon, ready for generation to begin.
 */
static void cave_clear(struct cave *c, struct player *p)
{
	int x, y;

	wipe_o_list(c);
	wipe_mon_list(c, p);

	/* Clear flags and flow information. */
	for (y = 0; y < DUNGEON_HGT; y++) {
		for (x = 0; x < DUNGEON_WID; x++) {
			/* Erase features */
			c->feat[y][x] = 0;

			/* Erase flags */
			c->info[y][x] = 0;
			c->info2[y][x] = 0;

			/* Erase flow */
			c->cost[y][x] = 0;
			c->when[y][x] = 0;

			/* Erase monsters/player */
			c->m_idx[y][x] = 0;

			/* Erase items */
			c->o_idx[y][x] = 0;
		}
	}

	/* Unset the player's coordinates */
	p->px = p->py = 0;

	/* Nothing special here yet */
	c->good_item = FALSE;

	/* Nothing good here yet */
	c->mon_rating = 0;
	c->obj_rating = 0;
}

/**
 * Place hidden squares that will be used to generate feeling
 */
static void place_feeling(struct cave *c)
{
	int y,x,i,j;
	int tries = 500;
	
	for (i = 0; i < FEELING_TOTAL; i++){
		for(j = 0; j < tries; j++){
			
			/* Pick a random dungeon coordinate */
			y = randint0(DUNGEON_HGT);
			x = randint0(DUNGEON_WID);
			
			/* Check to see if it is not a wall */
			if (cave_iswall(c,y,x))
				continue;
				
			/* Check to see if it is already marked */
			if (cave_isfeel(c,y,x))
				continue;
				
			/* Set the cave square appropriately */
			c->info2[y][x] |= CAVE2_FEEL;
			
			break;
		
		}
	}

	/* Reset number of feeling squares */
	c->feeling_squares = 0;
	
}


/**
 * Calculate the level feeling for objects.
 */
static int calc_obj_feeling(struct cave *c)
{
	u32b x;

	/* Town gets no feeling */
	if (c->depth == 0) return 0;

	/* Artifacts trigger a special feeling when preserve=no */
	if (c->good_item && OPT(birth_no_preserve)) return 10;

	/* Check the loot adjusted for depth */
	x = c->obj_rating / c->depth;

	if (x > 6000) return 20;
	if (x > 3500) return 30;
	if (x > 2000) return 40;
	if (x > 1000) return 50;
	if (x > 500) return 60;
	if (x > 300) return 70;
	if (x > 200) return 80;
	if (x > 100) return 90;
	return 100;
}

/**
 * Calculate the level feeling for monsters.
 */
static int calc_mon_feeling(struct cave *c)
{
	u32b x;

	/* Town gets no feeling */
	if (c->depth == 0) return 0;

	/* Check the monster power adjusted for depth */
	x = c->mon_rating / (c->depth * c->depth);

	if (x > 7000) return 1;
	if (x > 4500) return 2;
	if (x > 2500) return 3;
	if (x > 1500) return 4;
	if (x > 800) return 5;
	if (x > 400) return 6;
	if (x > 150) return 7;
	if (x > 50) return 8;
	return 9;
}


/**
 * Reset the current dungeon's generation data.
 */
static void clear_dun_data(struct dun_data *d) {
	int bx, by;
	for (by = 0; by < MAX_ROOMS_ROW; by++) {
		for (bx = 0; bx < MAX_ROOMS_COL; bx++) {
			d->room_map[by][bx] = FALSE;
		}
	}
}

/**
 * Generate a random level.
 *
 * Confusingly, this function also generate the town level (level 0).
 */
void cave_generate(struct cave *c, struct player *p) {
	const char *error = "no generation";
	int tries = 0;

	assert(c);

	c->depth = p->depth;

	/* Generate */
	for (tries = 0; tries < 100 && error; tries++) {
		struct dun_data dun_body;

		error = NULL;
		cave_clear(c, p);

		/* Mark the dungeon as being unready (to avoid artifact loss, etc) */
		character_dungeon = FALSE;

		/* Allocate global data (will be freed when we leave the loop) */
		dun = &dun_body;
		clear_dun_data(dun);

		if (p->depth == 0) {
			dun->profile = &town_profile;
			dun->profile->builder(c, p);
		} else {
			int perc = randint0(100);
			int last = NUM_CAVE_PROFILES - 1;
			int i;
			for (i = 0; i < NUM_CAVE_PROFILES; i++) {
				bool ok;
				const struct cave_profile *profile;

				profile = dun->profile = &cave_profiles[i];
				if (i < last && profile->cutoff < perc) continue;

				ok = dun->profile->builder(c, p);
				if (ok) break;
			}
		}

		/* Ensure quest monsters */
		if (is_quest(c->depth)) {
			int i;
			for (i = 1; i < z_info->r_max; i++) {
				monster_race *r_ptr = &r_info[i];
				int y, x;
				
				/* The monster must be an unseen quest monster of this depth. */
				if (r_ptr->cur_num > 0) continue;
				if (!rf_has(r_ptr->flags, RF_QUESTOR)) continue;
				if (r_ptr->level != c->depth) continue;
	
				/* Pick a location and place the monster */
				find_empty(c, &y, &x);
				place_new_monster(c, y, x, i, TRUE, TRUE, ORIGIN_DROP);
			}
		}

		/* Place dungeon squares to trigger feeling */
		place_feeling(c);
		
		c->feeling = calc_obj_feeling(c) + calc_mon_feeling(c);

		/* Regenerate levels that overflow their maxima */
		if (o_max >= z_info->o_max) 
			error = "too many objects";
		if (cave_monster_max(cave) >= z_info->m_max)
			error = "too many monsters";

		if (error) ROOM_LOG("Generation restarted: %s.", error);
	}

	free_cave_squares();

	if (error) quit_fmt("cave_generate() failed 100 times!");

	/* The dungeon is ready */
	character_dungeon = TRUE;

	c->created_at = turn;
}
