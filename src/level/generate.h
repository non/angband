/* level/generate.h - dungeon generation interface */

#ifndef GENERATE_H
#define GENERATE_H

#include "level/gen-util.h"

//void ensure_connectedness(struct cave *c);

void place_object(struct cave *c, int y, int x, int level, bool good,
	bool great, byte origin);
void place_gold(struct cave *c, int y, int x, int level, byte origin);
void place_secret_door(struct cave *c, int y, int x);
void place_closed_door(struct cave *c, int y, int x);
void place_random_door(struct cave *c, int y, int x);

extern struct vault *random_vault(int typ);

struct tunnel_profile {
	const char *name;
    int rnd; /* % chance of choosing random direction */
    int chg; /* % chance of changing direction */
    int con; /* % chance of extra tunneling */
    int pen; /* % chance of placing doors at room entrances */
    int jct; /* % chance of doors at tunnel junctions */
};

struct streamer_profile {
	const char *name;
    int den; /* Density of streamers */    
    int rng; /* Width of streamers */
    int mag; /* Number of magma streamers */
    int mc; /* 1/chance of treasure per magma */
    int qua; /* Number of quartz streamers */
    int qc; /* 1/chance of treasure per quartz */
};

/*
* cave_builder is a function pointer which builds a level.
*/
typedef bool (*cave_builder) (struct cave *c, struct player *p);


struct cave_profile {
	const char *name;
	cave_builder builder; /* Function used to build the level */
    int dun_rooms; /* Number of rooms to attempt */
    int dun_unusual; /* Level/chance of unusual room */
    int max_rarity; /* Max number of rarity levels used in room generation */
    int n_room_profiles; /* Number of room profiles */
	struct tunnel_profile tun; /* Used to build tunnels */
	struct streamer_profile str; /* Used to build mineral streamers*/
    const struct room_profile *room_profiles; /* Used to build rooms */
	int cutoff; /* Used to see if we should try this dungeon */
};


/**
 * room_builder is a function pointer which builds rooms in the cave given
 * anchor coordinates.
 */
typedef bool (*room_builder) (struct cave *c, int y0, int x0);


/**
 * This tracks information needed to generate the room, including the room's
 * name and the function used to build it.
 */
struct room_profile {
	const char *name;
	room_builder builder; /* Function used to build the room */
	int height, width; /* Space required in blocks */
	int level; /* Minimum dungeon level */
	bool crowded; /* Whether this room is crowded or not */
	int rarity; /* How unusual this room is */
	int cutoff; /* Upper limit of 1-100 random roll for room generation */
};
	

struct pit_color_profile {
	struct pit_color_profile *next;

	byte color;
};

struct pit_forbidden_monster {
	struct pit_forbidden_monster *next;
	
	int r_idx;
};

typedef struct pit_profile {
	struct pit_profile *next;

	int pit_idx; /* Index in pit_info */
	const char* name;
	int room_type; /* Is this a pit or a nest? */
	int ave; /* Level where this pit is most common */
	int rarity; /* How unusual this pit is */
	int obj_rarity; /* How rare objects are in this pit */
	bitflag flags[RF_SIZE];         /* Required flags */
	bitflag forbidden_flags[RF_SIZE];
	bitflag spell_flags[RSF_SIZE];  /* Required spell flags */
	bitflag forbidden_spell_flags[RSF_SIZE]; 
	int n_bases; 
	struct monster_base *base[MAX_RVALS];
	struct pit_color_profile *colors;
	struct pit_forbidden_monster *forbidden_monsters;
} pit_profile;

/**
 * Structure to hold all "dungeon generation" data
 */
struct dun_data {
	/* The profile used to generate the level */
	const struct cave_profile *profile;

	/* Array of centers of rooms */
	int cent_n;
	struct loc cent[CENT_MAX];

	/* Array of possible door locations */
	int door_n;
	struct loc door[DOOR_MAX];

	/* Array of wall piercing locations */
	int wall_n;
	struct loc wall[WALL_MAX];

	/* Array of tunnel grids */
	int tunn_n;
	struct loc tunn[TUNN_MAX];

	/* Number of blocks along each axis */
	int row_rooms;
	int col_rooms;

	/* Array of which blocks are used */
	bool room_map[MAX_ROOMS_ROW][MAX_ROOMS_COL];

	/* Hack -- there is a pit/nest on this level */
	bool crowded;
};

struct dun_data *get_dun(void);

#endif /* !GENERATE_H */
