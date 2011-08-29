/* level/gen-util.h - dungeon generation utility functions */

#ifndef GEN_UTIL_H
#define GEN_UTIL_H

/*
 * Dungeon allocation places and types, used with alloc_object().
 */
#define SET_CORR 1 /* Hallway */
#define SET_ROOM 2 /* Room */
#define SET_BOTH 3 /* Anywhere */

#define TYP_RUBBLE 1 /* Rubble */
#define TYP_TRAP 3 /* Trap */
#define TYP_GOLD 4 /* Gold */
#define TYP_OBJECT 5 /* Object */
#define TYP_GOOD 6 /* Great object */
#define TYP_GREAT 7 /* Great object */

void alloc_objects(struct cave *c, int set, int typ, int num, int depth, byte origin);
bool alloc_object(struct cave *c, int set, int typ, int depth, byte origin);

void shuffle(int *arr, int n);

bool cave_find(struct cave *c, int *y, int *x, cave_predicate pred);
bool cave_find_in_range(struct cave *c, int *y, int y1, int y2, int *x, int x1, int x2, cave_predicate pred);
bool find_empty(struct cave *c, int *y, int *x);
bool find_empty_range(struct cave *c, int *y, int y1, int y2, int *x, int x1, int x2);
bool find_nearby_grid(struct cave *c, int *y, int y0, int yd, int *x, int x0, int xd);
void correct_dir(int *rdir, int *cdir, int y1, int x1, int y2, int x2);
void rand_dir(int *rdir, int *cdir);

bool cave_isstart(struct cave *c, int y, int x);
void new_player_spot(struct cave *c, struct player *p);

int next_to_walls(struct cave *c, int y, int x);

void place_rubble(struct cave *c, int y, int x);
void place_stairs(struct cave *c, int y, int x, int feat);
void place_random_stairs(struct cave *c, int y, int x);
void place_object(struct cave *c, int y, int x, int level, bool good, bool great, byte origin);
void place_gold(struct cave *c, int y, int x, int level, byte origin);
void place_secret_door(struct cave *c, int y, int x);
void place_closed_door(struct cave *c, int y, int x);
void place_random_door(struct cave *c, int y, int x);

void alloc_cave_squares(int n);
void free_cave_squares(void);
void set_cave_dimensions(struct cave *c, int h, int w);

void fill_rectangle(struct cave *c, int y1, int x1, int y2, int x2, int feat);
void draw_rectangle(struct cave *c, int y1, int x1, int y2, int x2, int feat);
void fill_xrange(struct cave *c, int y, int x1, int x2, int feat, int info);
void fill_yrange(struct cave *c, int x, int y1, int y2, int feat, int info);
void fill_circle(struct cave *c, int y0, int x0, int radius, int border, int feat, int info);

void alloc_stairs(struct cave *c, int feat, int num, int walls);

int set_pit_type(int depth, int type);

#endif /* GENERATE_H */

