/* monster/attack */

#include "unit-test.h"
#include "unit-test-data.h"

#include "monster/monster.h"

int setup_tests(void **state) {
	struct monster_race *r = &test_r_human;
	struct monster *m = mem_zalloc(sizeof *m);
	m->race = r;
	m->r_idx = r->ridx;
	r_info = r;
	*state = m;

	p_ptr = NULL;
	return 0;
}

NOTEARDOWN

static int test_attack(void *state) {
	struct monster *m = state;
	struct player *p = &test_player;
	int old, new;
	int i;

	rand_fix(100);
	flags_set(m->race->flags, RF_SIZE, RF_NEVER_BLOW, FLAG_END);
	old = p->chp;
	for (i = 0; i < 100; i++)
		testfn_make_attack_normal(m, p);
	new = p->chp;
	eq(old, new);
	flags_clear(m->race->flags, RF_SIZE, RF_NEVER_BLOW, FLAG_END);

	old = p->chp;
	testfn_make_attack_normal(m, p);
	new = p->chp;
	eq(old - m->race->blow[0].d_dice, new);

	ok;
}

const char *suite_name = "monster/attack";
const struct test tests[] = {
	{ "attack", test_attack },
	{ NULL, NULL },
};