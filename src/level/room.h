/* level/room.h - room generation */

#ifndef ROOM_H
#define ROOM_H

bool build_simple(struct cave *c, int y0, int x0);
bool build_circular(struct cave *c, int y0, int x0);
bool build_overlap(struct cave *c, int y0, int x0);
bool build_crossed(struct cave *c, int y0, int x0);
bool build_large(struct cave *c, int y0, int x0);
bool build_nest(struct cave *c, int y0, int x0);
bool build_pit(struct cave *c, int y0, int x0);
bool build_template(struct cave *c, int y0, int x0);
bool build_lesser_vault(struct cave *c, int y0, int x0);
bool build_medium_vault(struct cave *c, int y0, int x0);
bool build_greater_vault(struct cave *c, int y0, int x0);

#endif /* ROOM_H */
