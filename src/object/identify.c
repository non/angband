/*
 * File: identify.c
 * Purpose: Object identification and knowledge routines
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2009 Brian Bull
 * Copyright (c) 2011 Edward F. Grove
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

/*
 * Some standard naming conventions for all functions below
 *
 * o_ptr	is the object being identified
 * o_flags 	are the actual flags of the object
 * k_flags	are the flags of a kind, including flags inherited from base tval
 * o_known	are the flags known by the char to be in the object
 * obvious_mask	is the set of flags that would be obvious on the object
 * curse_mask	is the set of flags for curses
 * pval_mask	is the set of flags with pvals
 */


#include "angband.h"
#include "cave.h"
#include "game-event.h"
#include "history.h"
#include "object/slays.h"
#include "object/tvalsval.h"
#include "object/pval.h"
#include "spells.h"
#include "squelch.h"

/** Time last item was wielded */
/* EFG evil global, was externed, but not used elsewhere and not put into savefile
 *     save/reload to learn faster, this needs to be excised soon */
static s32b object_last_wield = 0;

/*** Knowledge accessor functions ***/


/**
 * \returns whether an object counts as "known" due to EASY_KNOW status
 */
bool easy_know(const object_type *o_ptr)
{
	if (o_ptr->kind->aware && of_has(o_ptr->kind->flags, OF_EASY_KNOW))
		return TRUE;
	else
		return FALSE;
}

/**
 * \returns whether an object should be treated as fully known (e.g. ID'd)
 */
bool object_is_known(const object_type *o_ptr)
{
	return (o_ptr->ident & IDENT_KNOWN) || easy_know(o_ptr) ||
			(o_ptr->ident & IDENT_STORE);
}

/**
 * \returns whether the object is known to be an artifact
 */
bool object_is_known_artifact(const object_type *o_ptr)
{
	return (o_ptr->artifact && (o_ptr->ident & IDENT_NOTICE_ART));
}

/**
 * \returns whether the object is known to be cursed
 */
bool object_is_known_cursed(const object_type *o_ptr)
{
	bitflag o_known[OF_SIZE], curse_mask[OF_SIZE];

	object_flags_known(o_ptr, o_known);

	of_curse_mask(curse_mask);

	return of_is_inter(o_known, curse_mask);
}

/**
 * \returns whether the object is known to be cursed
 */
bool object_is_known_not_cursed(const object_type *o_ptr)
{
	bitflag o_flags[OF_SIZE], curse_mask[OF_SIZE];

	/* Gather whatever curse flags there are to know */
	of_curse_mask(curse_mask);

	/* if you don't know a curse flag, might be cursed */
	if (!of_is_subset(o_ptr->known_flags, curse_mask))
		return FALSE;

	object_flags(o_ptr, o_flags);

	return of_is_inter(o_flags, curse_mask) ? FALSE : TRUE;
}

/**
 * \returns whether the object is known to be blessed
 */
bool object_is_known_blessed(const object_type *o_ptr)
{
	bitflag o_known[OF_SIZE];

	object_flags_known(o_ptr, o_known);

	return of_has(o_known, OF_BLESSED);
}

/**
 * \returns whether the object is known to not be an artifact
 */
bool object_is_known_not_artifact(const object_type *o_ptr)
{
	return (o_ptr->ident & IDENT_NOTICE_ART) && !o_ptr->artifact;
}

 /**
 * \returns whether the effect exists and is known 
 */
bool object_has_known_effect(const object_type *o_ptr)
{
	assert(o_ptr->kind);
	return o_ptr->kind->effect && object_effect_is_known(o_ptr) ? TRUE : FALSE;
}

/**
 * \returns whether the object is neither ego nor artifact
 */
bool object_is_not_excellent(const object_type *o_ptr)
{
	return !o_ptr->artifact && !o_ptr->ego ? TRUE : FALSE;
}

/**
 * \returns whether the object is known to be neither ego nor artifact
 */
bool object_is_known_not_excellent(const object_type *o_ptr)
{
	return object_name_is_visible(o_ptr) && object_is_not_excellent(o_ptr) ? TRUE : FALSE;
}


/**
 * \returns whether the object is known to be bad
 *
 * currently only checking numeric values
 *
 * this calls amulet of inertia bad -- should it be?
 * negative pval stat rings are bad even though they have a sustain
 */
bool object_is_known_bad(const object_type *o_ptr)
{
	int i;
	bool something_bad = FALSE;
	bool something_good = FALSE;

	for (i = 0; i < o_ptr->num_pvals; i++) {
		if (object_this_pval_is_visible(o_ptr, i)) {
			if (o_ptr->pval[i] > 0)
				something_good = TRUE;
			else if (o_ptr->pval[i] < 0)
				something_bad = TRUE;
		}
	}

	if (object_attack_plusses_are_visible(o_ptr)) {
		if (o_ptr->to_h > 0)
			something_good = TRUE;
		else if (o_ptr->to_h < 0)
			something_bad = TRUE;
		if (o_ptr->to_d > 0)
			something_good = TRUE;
		else if (o_ptr->to_d < 0)
			something_bad = TRUE;
	}

	if (object_defence_plusses_are_visible(o_ptr)) {
		if (o_ptr->to_a > 0)
			something_good = TRUE;
		else if (o_ptr->to_a < 0)
			something_bad = TRUE;
	}

	return something_bad && !something_good ? TRUE : FALSE;
}


/**
 * \returns whether the object has been worn/wielded
 */
bool object_was_worn(const object_type *o_ptr)
{
	return o_ptr->ident & IDENT_WORN ? TRUE : FALSE;
}

/**
 * \returns whether the object has been fired/thrown
 */
bool object_was_fired(const object_type *o_ptr)
{
	return o_ptr->ident & IDENT_FIRED ? TRUE : FALSE;
}

/**
 * \returns whether the object has been sensed with pseudo-ID
 */
bool object_was_sensed(const object_type *o_ptr)
{
	return o_ptr->ident & IDENT_SENSE ? TRUE : FALSE;
}

/**
 * \returns whether the player is aware of the object's flavour
 */
bool object_flavor_is_aware(const object_type *o_ptr)
{
	assert(o_ptr->kind);
	return o_ptr->kind->aware;
}

/**
 * \returns whether the player has tried to use other objects of the same kind
 */
bool object_flavor_was_tried(const object_type *o_ptr)
{
	assert(o_ptr->kind);
	return o_ptr->kind->tried;
}

/**
 * \returns whether the player is aware of the object's effect when used
 */
bool object_effect_is_known(const object_type *o_ptr)
{
	assert(o_ptr->kind);
	return (easy_know(o_ptr) || (o_ptr->ident & IDENT_EFFECT)
		|| (object_flavor_is_aware(o_ptr) && o_ptr->kind->effect)
		|| (o_ptr->ident & IDENT_STORE)) ? TRUE : FALSE;
}

/**
 * \returns whether any ego or artifact name is available to the player
 *
 * This can be TRUE for a boring object that is known to be not excellent.
 */
bool object_name_is_visible(const object_type *o_ptr)
{
	return o_ptr->ident & IDENT_NAME ? TRUE : FALSE;
}

/**
 * \returns whether both the object is an ego and the player knows it is
 */
bool object_ego_is_visible(const object_type *o_ptr)
{
	if (!o_ptr->ego)
		return FALSE;

	/* lights cannot be sensed, so ego is obvious */
	/* EFG this seems broken -- jewelry cannot be sensed but is still learned
	 * Should lights require learning through check_for_ident? */
	if (o_ptr->tval == TV_LIGHT)
		return TRUE;

	if ((o_ptr->ident & IDENT_NAME) || (o_ptr->ident & IDENT_STORE))
		return TRUE;
	else
		return FALSE;
}


/**
 * \returns whether the object's attack plusses are known
 */
bool object_attack_plusses_are_visible(const object_type *o_ptr)
{
	/* bare hands e.g. have visible attack plusses */
	if (!o_ptr->kind) return TRUE;

	/* Bonuses have been revealed or for sale */
	if ((o_ptr->ident & IDENT_ATTACK) || (o_ptr->ident & IDENT_STORE))
		return TRUE;

	/* Aware jewelry with non-variable bonuses */
	if (object_is_jewelry(o_ptr) && object_flavor_is_aware(o_ptr)) {
		if (!randcalc_varies(o_ptr->kind->to_h) && !randcalc_varies(o_ptr->kind->to_d))
			return TRUE;
	}

	/* Defensive items such as shields have fixed attack values unless ego or artifact */
	else if (object_base_only_defensive(o_ptr)) {
		if (object_is_known_not_excellent(o_ptr))
			return TRUE;
		if (o_ptr->ego && object_ego_is_visible(o_ptr)
		    && !randcalc_varies(o_ptr->ego->to_h) && !randcalc_varies(o_ptr->ego->to_d))
			return TRUE;
	}

	return FALSE;
}

/**
 * \returns whether the object's defence bonuses are known
 */
bool object_defence_plusses_are_visible(const object_type *o_ptr)
{
	if (!o_ptr->kind) return TRUE;

	/* Bonuses have been revealed or for sale */
	if ((o_ptr->ident & IDENT_DEFENCE) || (o_ptr->ident & IDENT_STORE))
		return TRUE;

	/* Aware jewelry with non-variable bonuses */
	if (object_is_jewelry(o_ptr) && object_flavor_is_aware(o_ptr))
	{
		if (!randcalc_varies(o_ptr->kind->to_a))
			return TRUE;
	}

	/* Offensive items such as daggers have fixed defense values unless ego or artifact */
	else if (object_base_only_offensive(o_ptr)) {
		if (object_is_known_not_excellent(o_ptr))
			return TRUE;
		if (o_ptr->ego && object_ego_is_visible(o_ptr) && !randcalc_varies(o_ptr->ego->to_a))
			return TRUE;
	}

	return FALSE;
}


/**
 * \returns whether the player knows whether an object has a given flag
 */
bool object_flag_is_known(const object_type *o_ptr, flag_type flag)
{
	if (easy_know(o_ptr) ||
	    (o_ptr->ident & IDENT_STORE) ||
	    of_has(o_ptr->known_flags, flag))
		return TRUE;

	return FALSE;
}


/**
 * \returns whether it is possible an object has a high resist given the
 *          player's current knowledge
 */
bool object_high_resist_is_possible(const object_type *o_ptr)
{
	bitflag possible[OF_SIZE], high_resists[OF_SIZE];

	/* Actual object flags */
	object_flags(o_ptr, possible);

	/* Add player's uncertainty */
	of_comp_union(possible, o_ptr->known_flags);

	/* Check for possible high resist */
	create_mask(high_resists, FALSE, OFT_HRES, OFT_MAX);
	if (of_is_inter(possible, high_resists))
		return TRUE;
	else
		return FALSE;
}


/**
 * \returns number of object flags in the list which are learnable
 */
static int num_learnable_flags(const bitflag flags[OF_SIZE])
{
	bitflag unlearnable[OF_SIZE];
	flag_type i;
	int num = 0;

	of_unlearnable_mask(unlearnable);

	for_each_object_flag(i) {
		if (of_has(flags, i) && !of_has(unlearnable, i))
			num++;
	}

	return num;
}


/**
 * \returns number of learnable flags in the object that are not known yet
 */
int object_num_unlearned_flags(const object_type *o_ptr)
{
	bitflag o_flags[OF_SIZE];
	bitflag o_known[OF_SIZE];
	bitflag unlearnable[OF_SIZE];
	int unknown = 0;
	flag_type i;

	object_flags(o_ptr, o_flags);
	object_flags_known(o_ptr, o_known);
	of_unlearnable_mask(unlearnable);

	for_each_object_flag(i)
		if (of_has(o_flags, i) && !of_has(o_known, i) && !of_has(unlearnable, i)) {
			unknown++;
		}

	return unknown;
}


/**
 * \returns number of learnable flags in the object
 */
int object_num_learnable_flags(const object_type *o_ptr)
{
	bitflag o_flags[OF_SIZE];
	object_flags(o_ptr, o_flags);

	return num_learnable_flags(o_flags);
}


/**
 * Create a list of flags that are obvious on a particular kind.
 *
 * This should be allowed to depend upon race and class as well.
 * If you want SI not to be obvious to high-elves, put that code here.
 */
void kind_obvious_mask(const object_kind *kind, bitflag flags[OF_SIZE])
{
	if (base_is_ammo(kind->base)) {
		of_wipe(flags);
		return;
	}

	create_mask(flags, TRUE, OFID_WIELD, OFT_MAX);

	/* special case FA, needed at least for mages wielding gloves */
	if (player_has(PF_CUMBER_GLOVE) && base_wield_slot(kind->base) == INVEN_HANDS)
		of_on(flags, OF_FREE_ACT);

}

/**
 * \returns whether the pval of the flag is visible
 */
bool object_flag_pval_is_visible(const object_type *o_ptr, flag_type flag)
{
	/* currently either all or no pvals are visible, depending upon worn status */
	return object_was_worn(o_ptr);
}

/**
 * \returns whether the player knows the object is splendid
 */
bool object_is_known_splendid(const object_type *o_ptr)
{
	bitflag o_known[OF_SIZE], k_flags[OF_SIZE], obvious_mask[OF_SIZE], pval_mask[OF_SIZE];
	flag_type i;

	assert(o_ptr->kind);

	/* possibly cursed items cannot be known to be splendid */
	if (!object_is_known_not_cursed(o_ptr))
		return FALSE;

	object_flags_known(o_ptr, o_known);
	object_kind_flags(o_ptr->kind, k_flags);
	kind_obvious_mask(o_ptr->kind, obvious_mask);
	of_pval_mask(pval_mask);

	/* check if any known obvious flags */
	for_each_object_flag(i) {
		if (of_has(o_known, i) && of_has(obvious_mask, i)) {
			/* a pval flag in the kind, such as digging on a pick, is obvious only if the pval is visible */
			if (of_has(pval_mask, i)) {
				if (!of_has(k_flags, i))
					return TRUE;
				else if (object_flag_pval_is_visible(o_ptr, i) && o_ptr->ego)
					return TRUE;
			}
			else return TRUE;
		}
	}

	return FALSE;
}

/**
 * \returns whether the player knows the object is not splendid
 */
bool object_is_known_unsplendid(const object_type *o_ptr)
{
	bitflag known_not_in_kind[OF_SIZE], obvious_mask[OF_SIZE];
	bitflag pval_mask[OF_SIZE], kind_only_pvals[OF_SIZE];

	kind_obvious_mask(o_ptr->kind, obvious_mask);
	object_flags_known(o_ptr, known_not_in_kind);

	if (o_ptr->ego) {
		/* collect flags with pvals due to kind not in the ego */
		of_pval_mask(pval_mask);
		object_kind_flags(o_ptr->kind, kind_only_pvals);
		of_inter(kind_only_pvals, pval_mask);
		of_diff(kind_only_pvals, o_ptr->ego->flags);

		/* removed kind only pval flags before testing for obvious flags */
		of_diff(known_not_in_kind, kind_only_pvals);
	}

	/* check if anything known obvious */
	if (of_is_inter(known_not_in_kind, obvious_mask))
		return FALSE;

	/* make sure all possible obvious flags are accounted for */
	return of_is_subset(o_ptr->known_flags, obvious_mask);
}


/**
 * Sets some IDENT_ flags on an object.
 *
 * \param o_ptr is the object to check
 * \param flags are the ident flags to be added
 *
 * \returns whether o_ptr->ident changed
 */
static bool object_add_ident_flags(object_type *o_ptr, u32b flags)
{
	if ((o_ptr->ident & flags) != flags)
	{
		o_ptr->ident |= flags;
		return TRUE;
	}

	return FALSE;
}



/*
 * Set the second param to be those flags known to be in the object with visible pvals
 *
 * Currently all pvals or none are visible, depending upon worn status.
 */
void object_flags_with_visible_pvals(const object_type *o_ptr, bitflag o_pvals_known[OF_SIZE])
{
	if (object_was_worn(o_ptr)) {
		bitflag pval_mask[OF_SIZE];

		object_flags_known(o_ptr, o_pvals_known);
		of_pval_mask(pval_mask);
		
		of_inter(o_pvals_known, pval_mask);
	}
	else
		of_wipe(o_pvals_known);
}

/**
 * Counts the number of egos that are consistent with flags known by player.
 * Count is currently not guaranteed to be exact because of imperfect checks
 * for random powers.
 *
 * \returns an upper bound on the number of egos consistent with knowledge of the object
 */
static int num_matching_egos(const object_type *o_ptr)
{
	int num = 0;
	bitflag known_true[OF_SIZE];	/* flags known to be on object */
	bitflag known_false[OF_SIZE];	/* flags known to be missing from object */
	bitflag ego_pval_flags_known[OF_SIZE];	/* flags known to be required in the ego */
	ego_item_type *e_ptr;

	bitflag pval_mask[OF_SIZE];
	of_pval_mask(pval_mask);

	object_flags_known(o_ptr, known_true);
	of_copy(known_false, o_ptr->known_flags);
	of_diff(known_false, known_true);

	if (o_ptr->ego) {
		of_copy(ego_pval_flags_known, o_ptr->ego->flags);
		of_inter(ego_pval_flags_known, pval_mask);
		of_inter(ego_pval_flags_known, o_ptr->known_flags);
	}
	else of_wipe(ego_pval_flags_known);

	/* We check each ego to see whether it is a possible match for what is
	 * known about the flags on the object, both positive and negative */
	for_each_ego(e_ptr)
	{
		bitflag possible[OF_SIZE];
		bitflag required[OF_SIZE];
		bitflag xtra_flags[OF_SIZE];

		/* restrict to egos that match the object */
		if (ego_applies(e_ptr, o_ptr->tval, o_ptr->sval))
		{
			/* If a base object has a flag like slay undead you cannot differentiate the ego based upon it.
			 * However, for flags with pvals things are clear.  */
			if (o_ptr->ego) {
				bitflag o_pvals_known[OF_SIZE];	/* flags in the object for which the pval is known */
				bitflag e_pval_flags[OF_SIZE];	/* flags with pvals in e_ptr that match flags known on the object */
				bitflag ego_pvals_visible[OF_SIZE];	/* corresponding flags with visible pvals in actual ego */

				/* collect the set of flags in question */
				object_flags_with_visible_pvals(o_ptr, o_pvals_known);

				/* find matching flags in e_ptr */
				of_copy(e_pval_flags, e_ptr->flags);
				of_inter(e_pval_flags, o_pvals_known);

				/* find matching flags in real ego */
				of_copy(ego_pvals_visible, o_ptr->ego->flags);
				of_inter(ego_pvals_visible, o_pvals_known);

				/* check that e_ptr matches the actual ego for flags with known pvals */
				if (!of_is_equal(e_pval_flags, ego_pvals_visible))
					continue;
			}
			/* This ends checking about specific pval values. */

			/* flags either from the object's kind or possible ego are required */
			object_kind_flags(o_ptr->kind, required);
			of_union(required, e_ptr->flags);

			/* the possible flags are mainly the required flags */
			of_copy(possible, required);

			/* egos with an xtra flag increase the range of possible flags */
			of_wipe(xtra_flags);
			switch (e_ptr->xtra)
			{
				case OBJECT_XTRA_TYPE_NONE:
					break;
				case OBJECT_XTRA_TYPE_SUSTAIN:
					create_mask(xtra_flags, FALSE, OFT_SUST, OFT_MAX);
					break;
				case OBJECT_XTRA_TYPE_RESIST:
					create_mask(xtra_flags, FALSE, OFT_HRES, OFT_MAX);
					break;
				case OBJECT_XTRA_TYPE_POWER:
					create_mask(xtra_flags, FALSE, OFT_MISC, OFT_PROT, OFT_MAX);
					break;
				default:
					assert(0);
			}
			of_union(possible, xtra_flags);

			/* check consistency of object knowledge with flags possible and required */
			if (!of_is_subset(possible, known_true)) continue;
			if (of_is_inter(known_false, required)) continue;

			if (SENSING_REVEALS_FLAG_COUNT && object_was_sensed(o_ptr)) {
				/*
				 * Given the flag count, we can limit to egos producing the same number of flags as the object has.
				 * This section of code is only about the true number of flags matching the ego.
				 * There is no need for any reference to what is known.
				 * However, sensing is only about learnable flags.  Stuff like HIDE_TYPE should not be counted.
				 */

				int num_eflags;	/* number of flags present in possible ego, including inherited from kind */
				int num_oflags; /* ditto for object */
				bitflag e_flags[OF_SIZE];	/* flags in ego, including inherited from kind */
				bitflag o_flags[OF_SIZE];	/* ditto for object */

				/* get the number of learnable flags in the object */
				object_flags(o_ptr, o_flags);
				num_oflags = num_learnable_flags(o_flags);

				/* get the number of learnable flags for ego in question applied to same kind */
				object_kind_flags(o_ptr->kind, e_flags);
				of_union(e_flags, e_ptr->flags);
				num_eflags = num_learnable_flags(e_flags);
				if (e_ptr->xtra)
					/* Currently all xtra powers are a single flag guaranteed to be different from given ego and kind properties */
					num_eflags++;

				if (num_oflags != num_eflags) continue;
			}
			num++;
		}
	}
	if ((num == 0) && (o_ptr->ego))
		/*
		 * We could assert that some ego must match, but in case of an old savefile
		 * that does not quite match, the ident code just reveals the ego in such cases.
		 * This message is so that perhaps someone will make a bug report.
		 */
		msgt(MSG_GENERIC, "Bug: object's ego seems impossible.");

	return num;
}


/**
 * \returns the number of flavors that match current knowledge of object
 */
static int num_matching_unaware_flavors(const object_type *o_ptr)
{
	int num = 0;
	bitflag known_true[OF_SIZE];	/* flags known to be in the object */
	bitflag known_false[OF_SIZE];	/* flags known not to be in the object */
	object_kind *k_ptr;

	object_flags_known(o_ptr, known_true);
	of_copy(known_false, o_ptr->known_flags);
	of_diff(known_false, known_true);

	/* simply check each kind to see if it allocatable and matches knowledge */
	for_each_kind(k_ptr) {
		bitflag k_flags[OF_SIZE];
		
		if (k_ptr->aware) continue;
		if (k_ptr->tval != o_ptr->tval) continue;
		if (!k_ptr->alloc_prob) continue;

		object_kind_flags(k_ptr, k_flags);
		if (!of_is_subset(k_flags, known_true)) continue;

		if (of_is_inter(known_false, k_flags)) continue;
		if (o_ptr->kind->effect && !k_ptr->effect) continue;
		if (!o_ptr->kind->effect && k_ptr->effect) continue;
	
		num++;
	}
	if ((num == 0) && (!o_ptr->artifact))
		msgt(MSG_GENERIC, "Bug: object's flavor seems impossible.");
	return num;
}

/**
 * \returns whether all plusses are known on an object
 *
 * plusses are values not associated to flags, such as to_h and to_d and to_a, 
 */
bool object_all_plusses_are_visible(const object_type *o_ptr)
{
	return object_attack_plusses_are_visible(o_ptr) && object_defence_plusses_are_visible(o_ptr) ? TRUE : FALSE;
}


/*
 * Checks for additional knowledge implied by what the player already knows.
 *
 * \param o_ptr is the object to check
 *
 * returns whether it calls object_notice_everyting
 */
bool object_check_for_ident(object_type *o_ptr)
{
	bitflag o_flags[OF_SIZE], k_flags[OF_SIZE], known_flags[OF_SIZE], unlearnable[OF_SIZE];
	bitflag o_flags_learnable[OF_SIZE], known_flags_learnable[OF_SIZE];
	int i;

	bool something_sensed;	/* whether you have learned any information about flags being on the object */

	/* 
	 * This next variable describes whether any flag was learned to be present on the object.
	 * This does not inlcude flags known to be present in the kind, such as OF_TUNNEL on a digger
	 * It does include flags on unaware jewelry.
	 */	
	bool some_flag_known;

	assert(o_ptr->kind);
	assert(o_ptr->kind->base);

	if (object_is_known(o_ptr))
		return FALSE;

	/*
	 * Objects such as wands could be checked for flags such as IGNORE_ELEC,
	 * but for now this function is only about learning about wieldable items and ammo
	 */
	if (!obj_can_wear(o_ptr) && !object_can_be_sensed(o_ptr))
		return FALSE;

	if (SENSING_REVEALS_FLAG_COUNT && object_was_sensed(o_ptr)) {
		/* if you know there are no flags left to learn, know all flags */
		if (object_num_unlearned_flags(o_ptr) == 0)
			of_setall(o_ptr->known_flags);
	}

	object_flags(o_ptr, o_flags);
	object_flags_known(o_ptr, known_flags);
	object_kind_flags(o_ptr->kind, k_flags);

	/* first determine if we know anything about the object's flags */
	if (object_is_jewelry(o_ptr))
		/* jewelry learning is about learning kind flags */
		some_flag_known = object_has_known_effect(o_ptr) || !of_is_subset(o_ptr->kind->base->flags, known_flags);
	else
		/* non-jewelry kinds are known, learning is about further flags */
		some_flag_known = object_has_known_effect(o_ptr) || !of_is_subset(k_flags, known_flags);

	something_sensed = some_flag_known || object_was_sensed(o_ptr);

	/*
	 * If nothing is known about flags, give up, unless the item might have magical plusses only.
	 * An object with no possible ego or artifact can continue.
	 * E.g. a wielded digger with no digging bonus is known neither ego nor artifact, so might still ID.
	 */
	if (!something_sensed && !object_is_jewelry(o_ptr) &&
	    ((num_matching_egos(o_ptr) != 0) || !object_is_known_not_artifact(o_ptr)))
		return FALSE;

	/*
	 * See if there is a unique match, flavor or ego, to the object in question.
	 * Then the player can be informed.
	 *
	 * The following tests for <= 1 rather than == 1 give some hope to recover if
	 * there are bugs or an object comes from an earlier version and the
	 * rarity has been changed to 0.
	 */

	/* unique jewelry flavor */
	if (!o_ptr->artifact && object_is_jewelry(o_ptr) &&
	    !object_flavor_is_aware(o_ptr) &&
	    (num_matching_unaware_flavors(o_ptr) <= 1)) {
		object_flavor_aware(o_ptr);

		/* noticing the flavor may mean more flags known */
		object_flags_known(o_ptr, known_flags);
	}


	/* unique ego, or no possible ego */
	else if (!object_is_jewelry(o_ptr) && !o_ptr->artifact && !object_ego_is_visible(o_ptr)) {
		int num = num_matching_egos(o_ptr);
		if (num == 0) {
			if (o_ptr->ego) {
				/* requires a bug to get here, but instead of assert failure,
				 * might as well let the player keep playing */
				num = 1;
			} else {
				/* the char knows that no ego could match */
				object_notice_ego(o_ptr);
				if (object_all_plusses_are_visible(o_ptr)) {
					object_notice_everything(o_ptr);
					return TRUE;
				}
				else
					return FALSE;
			}
		}
		if (num == 1) { /* not an else-if, see num=1 above */
			/* it is possible for num of 1 with no ego e.g. boring gloves match ego of slaying */
			if (o_ptr->ego) {
				object_notice_ego(o_ptr);
				/* object_notice_ego recursively calls back to object_check_for_ident
				 * so the recursion already did all ident work possible.  */
				return o_ptr->ident & IDENT_KNOWN ? TRUE : FALSE;

				/* If the recursion is ever removed from object_notice_ego, uncomment the next line */
				/* noticing the ego may mean more flags known 
				object_flags_known(o_ptr, known_flags);
				*/
			}
		}

	}

	/* ID is not finished if you do not know the activation */
	if (object_effect(o_ptr) && !object_effect_is_known(o_ptr))
		return FALSE;

	of_copy(o_flags_learnable, o_flags);
	of_copy(known_flags_learnable, known_flags);

	/* only interested in what's over and above flags inherited from kind */
	if (object_flavor_is_aware(o_ptr)) {
 		bitflag k_flags[OF_SIZE];
		object_kind_flags(o_ptr->kind, k_flags);

		of_diff(o_flags_learnable, k_flags);
		of_diff(known_flags_learnable, k_flags);
	}

	/* We need to deal with flags that cannot be learned */
	of_unlearnable_mask(unlearnable);
	of_diff(o_flags_learnable, unlearnable);
	of_diff(known_flags_learnable, unlearnable);


	/* require full knowledge, positive and negative, of all learnable flags */
	if (!of_is_equal(o_flags_learnable, known_flags_learnable)) return FALSE;

	/* 
	 * In order to get this far, all knowable flags in the object must be known.
	 * To give IDENT_KNOWN status, we also require knowing there are no other flags,
	 * and also require knowledge of all pvals and non-flag plusses.
	 */

	/* In addition to knowing the pval flags, it is necessary to know the pvals */
	/* EFG this should test knowledge of pvals of individual flags */
	for (i = 0; i < o_ptr->num_pvals; i++) if (!object_this_pval_is_visible(o_ptr, i)) return FALSE;

	/* require sensing to know if there are unknown flags, except if cannot sense */
	if (object_was_sensed(o_ptr) || !object_can_be_sensed(o_ptr)) {
		if (object_all_plusses_are_visible(o_ptr)) {
			object_notice_everything(o_ptr);
			return TRUE;
		}
	}
 
	return FALSE;
}


/**
 * Mark an object's flavour as as one the player is aware of.
 *
 * \param o_ptr is the object whose flavour should be marked as aware
 */
void object_flavor_aware(object_type *o_ptr)
{
	int i;

	if (o_ptr->kind->aware) return;
	o_ptr->kind->aware = TRUE;

	/* Fix squelch/autoinscribe */
	p_ptr->notice |= PN_SQUELCH;
	apply_autoinscription(o_ptr);

	for (i = 1; i < o_max; i++)
	{
		const object_type *floor_o_ptr = object_byid(i);

		/* Some objects change tile on awareness */
		/* So update display for all floor objects of this kind */
		if (!floor_o_ptr->held_m_idx &&
				floor_o_ptr->kind == o_ptr->kind)
			cave_light_spot(cave, floor_o_ptr->iy, floor_o_ptr->ix);
	}
}


/**
 * Mark an object's flavour as tried.
 *
 * \param o_ptr is the object whose flavour should be marked
 */
void object_flavor_tried(object_type *o_ptr)
{
	assert(o_ptr);
	assert(o_ptr->kind);

	o_ptr->kind->tried = TRUE;
}

/**
 * Make the player aware of all of an object's flags.
 *
 * \param o_ptr is the object to mark
 */
void object_know_all_flags(object_type *o_ptr)
{
	of_setall(o_ptr->known_flags);
}


#define IDENTS_SET_BY_IDENTIFY ( IDENT_KNOWN | IDENT_ATTACK | IDENT_DEFENCE | IDENT_SENSE | IDENT_EFFECT | IDENT_WORN | IDENT_FIRED | IDENT_NAME )

/**
 * \returns whether an object has IDENT_KNOWN but should not
 */
bool object_is_not_known_consistently(const object_type *o_ptr)
{
	if (easy_know(o_ptr))
		return FALSE;
	if (!(o_ptr->ident & IDENT_KNOWN))
		return TRUE;
	if ((o_ptr->ident & IDENTS_SET_BY_IDENTIFY) != IDENTS_SET_BY_IDENTIFY)
		return TRUE;
	if (o_ptr->ident & IDENT_EMPTY)
		return TRUE;
	else if (o_ptr->artifact &&
			!(o_ptr->artifact->seen || o_ptr->artifact->everseen))
		return TRUE;

	if (!of_is_full(o_ptr->known_flags))
		return TRUE;

	return FALSE;
}



/**
 * Mark as object as fully known, a.k.a identified. 
 *
 * \param o_ptr is the object to mark as identified
 */
void object_notice_everything(object_type *o_ptr)
{
	/* The object is "empty" */
	o_ptr->ident &= ~(IDENT_EMPTY);

	/* Mark as known */
	object_flavor_aware(o_ptr);
	object_notice_artifact(o_ptr);
	object_notice_ego(o_ptr);

	/* IDENT_NAME is in the next set, but it should be set only by
	 * object_notice_artifact or object_notice_ego for everseen purposes,
	 * among other reasons.  What should be changed?
	 */
	object_add_ident_flags(o_ptr, IDENTS_SET_BY_IDENTIFY);

	/* Know all flags there are to be known */
	object_know_all_flags(o_ptr);
}



#if 0
/* EFG the flag IDENT_INDESTRUCT should be eliminated */
/**
 * Notice that an object is indestructible.
 */
void object_notice_indestructible(object_type *o_ptr)
{
	if (object_add_ident_flags(o_ptr, IDENT_INDESTRUCT))
		object_check_for_ident(o_ptr);
	object_notice_artifact(o_ptr);
}
#endif


/**
 * Notice the ego on an ego item, or that there is no ego
 */
void object_notice_ego(object_type *o_ptr)
{
	bitflag learned_flags[OF_SIZE];
	bitflag xtra_flags[OF_SIZE];

	/* Things are a bit confused because lights can be egos, but can also be jewelry e.g. The Phial */
	/* This function might be called on a light, which might be jewelry, leading to bugs elsewhere. */
	if (object_is_jewelry(o_ptr)) return;

	if (!object_add_ident_flags(o_ptr, IDENT_NAME)) return;

	/* all flags are known when an object is known neither ego nor artifact */
	if (!o_ptr->ego && object_is_known_not_artifact(o_ptr)) {
		of_setall(o_ptr->known_flags);
		return;
	}

	/* Learn all flags except random abilities */
	of_setall(learned_flags);
	switch (o_ptr->ego->xtra)
	{
		case OBJECT_XTRA_TYPE_NONE:
			break;
		case OBJECT_XTRA_TYPE_SUSTAIN:
			create_mask(xtra_flags, FALSE, OFT_SUST, OFT_MAX);
			of_diff(learned_flags, xtra_flags);
			break;
		case OBJECT_XTRA_TYPE_RESIST:
			create_mask(xtra_flags, FALSE, OFT_HRES, OFT_MAX);
			of_diff(learned_flags, xtra_flags);
			break;
		case OBJECT_XTRA_TYPE_POWER:
			create_mask(xtra_flags, FALSE, OFT_MISC, OFT_PROT, OFT_MAX);
			of_diff(learned_flags, xtra_flags);
			break;
		default:
			assert(0);
	}
	of_union(o_ptr->known_flags, learned_flags);

	/*
	 * Learn ego flags, which is not redundant in the case where an ego is 
	 * allowed with a guaranteed flag that matches the xtra possibilities.
	 * E.g. gondolin ego has guaranteed flags matching xtra_flags.
	 */
	of_union(o_ptr->known_flags, o_ptr->ego->flags);

	object_check_for_ident(o_ptr);
}


/**
 * Mark an object as sensed.
 *
 * This now means that the char knows the number of flags on the item,
 * so it is premature to notice sensing just because you know the item
 * is splendid or an artifact.
 */
void object_notice_sensing(object_type *o_ptr)
{
	if (!object_add_ident_flags(o_ptr, IDENT_SENSE)) return;

	object_notice_artifact(o_ptr);

	object_notice_curses(o_ptr);

	/* note lack of name for boring objects */
	if (object_is_not_excellent(o_ptr))
		object_notice_ego(o_ptr);

	object_check_for_ident(o_ptr);
}


/**
 * Notice whether object is an artifact
 */
void object_notice_artifact(object_type *o_ptr)
{
	if (!object_add_ident_flags(o_ptr, IDENT_NOTICE_ART)) return;

	if (o_ptr->artifact) {
		/* Show the artifact name so you get the correct description. */
		object_add_ident_flags(o_ptr, IDENT_NAME);
		/* no need to show flavor when name is known */
		object_flavor_aware(o_ptr);

		if (!(o_ptr->ident & IDENT_FAKE)) {
			/* mark seen status */
			o_ptr->artifact->seen = TRUE;
			o_ptr->artifact->everseen = TRUE;

			/* Note artifacts when found */
			history_add_artifact(o_ptr->artifact, object_is_known(o_ptr), TRUE);
		}
	}
}


/**
 * Notice the "effect" from activating an object.
 *
 * \param o_ptr is the object to become aware of
 */
void object_notice_effect(object_type *o_ptr)
{
	if (!object_add_ident_flags(o_ptr, IDENT_EFFECT)) return;

	/* noticing an effect gains awareness */
	if (!object_flavor_is_aware(o_ptr))
		object_flavor_aware(o_ptr);

	object_check_for_ident(o_ptr);
}


static void object_notice_defence_plusses(struct player *p, object_type *o_ptr)
{
	assert(o_ptr && o_ptr->kind);

	if (!object_add_ident_flags(o_ptr, IDENT_DEFENCE)) return;

	object_check_for_ident(o_ptr);

	if (o_ptr->ac || o_ptr->to_a)
	{
		char o_name[80];

		object_desc(o_name, sizeof(o_name), o_ptr, ODESC_BASE);
		msgt(MSG_PSEUDOID,
				"You know more about the %s you are wearing.",
				o_name);
	}

	p->update |= (PU_BONUS);
	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);
}


void object_notice_attack_plusses(object_type *o_ptr)
{
	assert(o_ptr && o_ptr->kind);

	if (!object_add_ident_flags(o_ptr, IDENT_ATTACK)) return;

	object_check_for_ident(o_ptr);


	if (wield_slot(o_ptr) == INVEN_WIELD)
	{
		char o_name[80];

		object_desc(o_name, sizeof(o_name), o_ptr, ODESC_BASE);
		msgt(MSG_PSEUDOID,
				"You know more about the %s you are using.",
				o_name);
	}
	else if ((o_ptr->to_d || o_ptr->to_h) &&
			!((o_ptr->tval == TV_HARD_ARMOR || o_ptr->tval == TV_SOFT_ARMOR) && (o_ptr->to_h < 0)))
	{
		char o_name[80];

		object_desc(o_name, sizeof(o_name), o_ptr, ODESC_BASE);
		msgt(MSG_PSEUDOID, "Your %s glows.", o_name);
	}

	p_ptr->update |= (PU_BONUS);
	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);
}


/**
 * Notice a single flag
 *
 * \returns whether anything new was learned
 */
bool object_notice_flag(object_type *o_ptr, flag_type flag)
{
	if (!of_has(o_ptr->known_flags, flag))
	{
		/* message for noticing presence of flag */
		bitflag o_flags[OF_SIZE];
		object_flags(o_ptr, o_flags);
		if (of_has(o_flags, flag)) {
			char o_name[80];
			object_desc(o_name, sizeof(o_name), o_ptr, ODESC_BASE);
			flag_message(flag, o_name);
		}

		of_on(o_ptr->known_flags, flag);
		object_check_for_ident(o_ptr);
		event_signal(EVENT_INVENTORY);
		event_signal(EVENT_EQUIPMENT);

		return TRUE;
	}

	return FALSE;
}


/**
 * Notice a set of flags
 *
 * \returns whether anything new was learned
 */
bool object_notice_flags(object_type *o_ptr, bitflag flags[OF_SIZE])
{
	if (!of_is_subset(o_ptr->known_flags, flags))
	{
		of_union(o_ptr->known_flags, flags);
		object_check_for_ident(o_ptr);
		event_signal(EVENT_INVENTORY);
		event_signal(EVENT_EQUIPMENT);

		return TRUE;
	}

	return FALSE;
}


/**
 * Notice curses on an object.
 *
 * \param o_ptr is the object to notice curses on
 *
 * \returns whether the object is cursed
 */
bool object_notice_curses(object_type *o_ptr)
{
	bitflag o_flags[OF_SIZE], curse_mask[OF_SIZE];

	object_flags(o_ptr, o_flags);
	of_curse_mask(curse_mask);

	/* give knowledge of which curses are present */
	object_notice_flags(o_ptr, curse_mask);

	object_check_for_ident(o_ptr);

	p_ptr->notice |= PN_SQUELCH;

	return of_is_inter(o_flags, curse_mask);
}


/**
 * Notice things which happen on defending.
 */
void object_notice_on_defend(struct player *p)
{
	int i;

	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
		if (p->inventory[i].kind)
			object_notice_defence_plusses(p, &p->inventory[i]);

	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);
}


/**
 * Notice stuff when firing or throwing objects.
 *
 */
void object_notice_on_firing(object_type *o_ptr)
{
	if (object_add_ident_flags(o_ptr, IDENT_FIRED))
		object_check_for_ident(o_ptr);
}



/**
 * Notice flags that are apparent when the object is wielded
 */
void object_notice_on_wield(object_type *o_ptr)
{
	bitflag o_flags[OF_SIZE], obvious_mask[OF_SIZE];
	bool obvious = FALSE;

	/* Save time of wield for later */
	object_last_wield = turn;

	if (!object_add_ident_flags(o_ptr, IDENT_WORN)) {
		/* In case of bugs, it is nice to run through the 
		 * routine even with IDENT_WORN already set
		return*/;
	}

	/* Only deal with un-ID'd items */
	if (object_is_known(o_ptr)) return;

	/* Automatically sense artifacts upon wield */
	object_notice_artifact(o_ptr);

	/* Wear it */
	object_flavor_tried(o_ptr);

	if (object_flavor_is_aware(o_ptr) && easy_know(o_ptr))
	{
		object_notice_everything(o_ptr);
		return;
	}

	object_check_for_ident(o_ptr);

	/* Extract the flags */
	object_flags(o_ptr, o_flags);

	kind_obvious_mask(o_ptr->kind, obvious_mask);

	/* ability to be activated is obvious, but there is no flag for it any more */
	if (of_is_inter(o_flags, obvious_mask) || object_effect(o_ptr))
		obvious = TRUE;

	/* Notice any obvious brands or slays */
	object_notice_slays(o_ptr, obvious_mask);

	/* Learn about obvious flags */
	of_union(o_ptr->known_flags, obvious_mask);

	object_check_for_ident(o_ptr);

	if (!obvious) return;

	/* EFG need to add stealth here, also need to assert/double-check everything is covered */
	/* CC: also need to add FA! */
	if (of_has(o_flags, OF_STR))
		msg("You feel %s!", o_ptr->pval[which_pval(o_ptr,
			OF_STR)] > 0 ? "stronger" : "weaker");
	if (of_has(o_flags, OF_INT))
		msg("You feel %s!", o_ptr->pval[which_pval(o_ptr,
			OF_INT)] > 0 ? "smarter" : "more stupid");
	if (of_has(o_flags, OF_WIS))
		msg("You feel %s!", o_ptr->pval[which_pval(o_ptr,
			OF_WIS)] > 0 ? "wiser" : "more naive");
	if (of_has(o_flags, OF_DEX))
		msg("You feel %s!", o_ptr->pval[which_pval(o_ptr,
			OF_DEX)] > 0 ? "more dextrous" : "clumsier");
	if (of_has(o_flags, OF_CON))
		msg("You feel %s!", o_ptr->pval[which_pval(o_ptr,
			OF_CON)] > 0 ? "healthier" : "sicklier");
	if (of_has(o_flags, OF_CHR))
		msg("You feel %s!", o_ptr->pval[which_pval(o_ptr,
			OF_CHR)] > 0 ? "cuter" : "uglier");
	if (of_has(o_flags, OF_SPEED))
		msg("You feel strangely %s.", o_ptr->pval[which_pval(o_ptr,
			OF_SPEED)] > 0 ? "quick" : "sluggish");
	if (of_has(o_flags, OF_BLOWS))
		msg("Your weapon %s in your hands.",
			o_ptr->pval[which_pval(o_ptr, OF_BLOWS)] > 0 ?
				"tingles" : "aches");
	if (of_has(o_flags, OF_SHOTS))
		msg("Your bow %s in your hands.",
			o_ptr->pval[which_pval(o_ptr, OF_SHOTS)] > 0 ?
				"tingles" : "aches");
	if (of_has(o_flags, OF_INFRA))
		msg("Your eyes tingle.");
	if (of_has(o_flags, OF_LIGHT))
		msg("It glows!");
	if (of_has(o_flags, OF_TELEPATHY))
		msg("Your mind feels strangely sharper!");

	/* this used to be a flag, still counts as the same idea */
	if (object_effect(o_ptr) && !object_effect_is_known(o_ptr))
		msg("You have something to activate.");

}


/**
 * Notice things about an object that would be noticed in time.
 */
static void object_notice_after_time(void)
{
	int i;
	flag_type flag;

	object_type *o_ptr;

	bitflag timed_mask[OF_SIZE];

	create_mask(timed_mask, TRUE, OFID_TIMED, OFT_MAX);

	/* Check every item the player is wearing */
	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		o_ptr = &p_ptr->inventory[i];

		if (!o_ptr->kind || object_is_known(o_ptr)) continue;

		for (flag = of_next(timed_mask, FLAG_START); flag != FLAG_END; flag = of_next(timed_mask, flag + 1))
		{
			/* Learn the flag */
			object_notice_flag(o_ptr, flag);
		}

		object_check_for_ident(o_ptr);
	}
}


/**
 * Notice a given special flag on wielded items.
 *
 * \param flag is the flag to notice
 */
void wieldeds_notice_flag(struct player *p, flag_type flag)
{
	int i;

	/* Sanity check */
	if (!flag) return;

	for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
	{
		object_type *o_ptr = &p->inventory[i];
		if (!o_ptr->kind) continue;
		object_notice_flag(o_ptr, flag);
	}

	return;
}


/**
 * Notice attack plusses from offweapon slots.
 * Does not apply to melee weapon or bow.
 */
void wieldeds_notice_offweapon_attack_plusses(void)
{
	int i;

	for (i = INVEN_WIELD + 2; i < INVEN_TOTAL; i++)
		if (p_ptr->inventory[i].kind)
			object_notice_attack_plusses(&p_ptr->inventory[i]);

	return;
}


/*
 * Given an object, return a short identifier which gives some idea of what
 * the item is.
 */
obj_pseudo_t object_pseudo(const object_type *o_ptr)
{
	if (object_is_known_artifact(o_ptr))
		return INSCRIP_SPECIAL;

	/* jewelry does not pseudo */
	if (object_is_jewelry(o_ptr))
		return INSCRIP_NULL;

	if (object_is_known_splendid(o_ptr))
		return INSCRIP_SPLENDID;

	if (!object_is_known(o_ptr) && !object_was_sensed(o_ptr))
		return INSCRIP_NULL;

	if (o_ptr->ego)
	{
		bitflag curse_mask[OF_SIZE];
		of_curse_mask(curse_mask);

		/* uncursed bad egos are not excellent */
		if (of_is_inter(o_ptr->ego->flags, curse_mask))
			return INSCRIP_STRANGE;
		else
			return INSCRIP_EXCELLENT;
	}

	if (o_ptr->to_a == randcalc(o_ptr->kind->to_a, 0, MINIMISE) &&
	    o_ptr->to_h == randcalc(o_ptr->kind->to_h, 0, MINIMISE) &&
		 o_ptr->to_d == randcalc(o_ptr->kind->to_d, 0, MINIMISE))
		return INSCRIP_AVERAGE;

	if (o_ptr->to_a >= randcalc(o_ptr->kind->to_a, 0, MINIMISE) &&
	    o_ptr->to_h >= randcalc(o_ptr->kind->to_h, 0, MINIMISE) &&
	    o_ptr->to_d >= randcalc(o_ptr->kind->to_d, 0, MINIMISE))
		return INSCRIP_MAGICAL;

	if (o_ptr->to_a <= randcalc(o_ptr->kind->to_a, 0, MINIMISE) &&
	    o_ptr->to_h <= randcalc(o_ptr->kind->to_h, 0, MINIMISE) &&
	    o_ptr->to_d <= randcalc(o_ptr->kind->to_d, 0, MINIMISE))
		return INSCRIP_MAGICAL;

	return INSCRIP_STRANGE;
}


/*
 * Sense the inventory
 */
void sense_inventory(void)
{
	int i;
	
	char o_name[80];
	
	unsigned int rate;
	
	
	/* No ID when confused in a bad state */
	if (p_ptr->timed[TMD_CONFUSED]) return;


	/* Notice some things after a while */
	if (turn >= (object_last_wield + 3000))
	{
		object_notice_after_time();
		object_last_wield = 0;
	}


	/* Get improvement rate */
	if (player_has(PF_PSEUDO_ID_IMPROV))
		rate = p_ptr->class->sense_base / (p_ptr->lev * p_ptr->lev + p_ptr->class->sense_div);
	else
		rate = p_ptr->class->sense_base / (p_ptr->lev + p_ptr->class->sense_div);

	if (!one_in_(rate)) return;


	/* Check everything */
	for (i = 0; i < ALL_INVEN_TOTAL; i++)
	{
		const char *text = NULL;

		object_type *o_ptr = &p_ptr->inventory[i];
		obj_pseudo_t feel;
		bool cursed;

		bool okay = FALSE;

		/* Skip empty slots */
		if (!o_ptr->kind) continue;

		/* Valid "tval" codes */
		switch (o_ptr->tval)
		{
			case TV_SHOT:
			case TV_ARROW:
			case TV_BOLT:
			case TV_BOW:
			case TV_DIGGING:
			case TV_HAFTED:
			case TV_POLEARM:
			case TV_SWORD:
			case TV_BOOTS:
			case TV_GLOVES:
			case TV_HELM:
			case TV_CROWN:
			case TV_SHIELD:
			case TV_CLOAK:
			case TV_SOFT_ARMOR:
			case TV_HARD_ARMOR:
			case TV_DRAG_ARMOR:
			{
				okay = TRUE;
				break;
			}
		}
		
		/* Skip non-sense machines */
		if (!okay) continue;
		
		/* It is known, no information needed */
		if (object_is_known(o_ptr)) continue;
		
		
		/* It has already been sensed, do not sense it again */
		if (object_was_sensed(o_ptr))
		{
			/* Small chance of wielded, sensed items getting complete ID */
			if (!o_ptr->artifact && (i >= INVEN_WIELD) && one_in_(1000))
				do_ident_item(i, o_ptr);

			continue;
		}

		/* Occasional failure on inventory items */
		if ((i < INVEN_WIELD) && one_in_(5)) continue;


		/* Sense the object */
		object_notice_sensing(o_ptr);
		cursed = object_notice_curses(o_ptr);

		/* Get the feeling */
		feel = object_pseudo(o_ptr);

		/* Stop everything */
		disturb(p_ptr, 0, 0);

		if (cursed)
			text = "cursed";
		else
			text = inscrip_text[feel];

		object_desc(o_name, sizeof(o_name), o_ptr, ODESC_BASE);

		/* Average pseudo-ID means full ID */
		if (feel == INSCRIP_AVERAGE)
		{
			object_notice_everything(o_ptr);

			msgt(MSG_PSEUDOID,
					"You feel the %s (%c) %s %s average...",
					o_name, index_to_label(i),((i >=
					INVEN_WIELD) ? "you are using" : "in your pack"),
					((o_ptr->number == 1) ? "is" : "are"));
		}
		else
		{
			if (i >= INVEN_WIELD)
			{
				msgt(MSG_PSEUDOID, "You feel the %s (%c) you are %s %s %s...",
							   o_name, index_to_label(i), describe_use(i),
							   ((o_ptr->number == 1) ? "is" : "are"),
				                           text);
			}
			else
			{
				msgt(MSG_PSEUDOID, "You feel the %s (%c) in your pack %s %s...",
							   o_name, index_to_label(i),
							   ((o_ptr->number == 1) ? "is" : "are"),
				                           text);
			}
		}


		/* Set squelch flag as appropriate */
		if (i < INVEN_WIELD)
			p_ptr->notice |= PN_SQUELCH;
		
		
		/* Combine / Reorder the pack (later) */
		p_ptr->notice |= (PN_COMBINE | PN_REORDER | PN_SORT_QUIVER);
		
		/* Redraw stuff */
		p_ptr->redraw |= (PR_INVEN | PR_EQUIP);
	}
}


