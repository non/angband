/* level/town.h - town generation */

#ifndef TOWN_H
#define TOWN_H

void build_store(struct cave *c, int n, int yy, int xx);
void town_gen_hack(struct cave *c, struct player *p);
bool town_gen(struct cave *c, struct player *p);

#endif /* TOWN_H */
