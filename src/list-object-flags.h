/* list-object-flags.h - object flags
 *
 * Changing flag order will break savefiles. There is a hard-coded limit of
 * 96 flags, due to 12 bytes of storage for item flags in the savefile. Flags
 * below start from 1 on line 11, so a flag's sequence number is its line
 * number minus 10.
 */

/* symbol       message */
OF(NONE,        "")
OF(STR,         "")
OF(INT,         "")
OF(WIS,         "")
OF(DEX,         "")
OF(CON,         "")
OF(CHR,         "")
OF(XXX1,        "")
OF(XXX2,        "")
OF(STEALTH,     "Your %s glows.")
OF(SEARCH,      "Your %s glows.")
OF(INFRA,       "")
OF(TUNNEL,      "")
OF(SPEED,       "")
OF(BLOWS,       "")
OF(SHOTS,       "")
OF(MIGHT,       "")
OF(SLAY_ANIMAL, "")
OF(SLAY_EVIL,   "")
OF(SLAY_UNDEAD, "")
OF(SLAY_DEMON,  "")
OF(SLAY_ORC,    "")
OF(SLAY_TROLL,  "")
OF(SLAY_GIANT,  "")
OF(SLAY_DRAGON, "")
OF(KILL_DRAGON, "")
OF(KILL_DEMON,  "")
OF(KILL_UNDEAD, "")
OF(BRAND_POIS,  "")
OF(BRAND_ACID,  "")
OF(BRAND_ELEC,  "")
OF(BRAND_FIRE,  "")
OF(BRAND_COLD,  "")
OF(SUST_STR,    "Your %s glows.")
OF(SUST_INT,    "Your %s glows.")
OF(SUST_WIS,    "Your %s glows.")
OF(SUST_DEX,    "Your %s glows.")
OF(SUST_CON,    "Your %s glows.")
OF(SUST_CHR,    "Your %s glows.")
OF(VULN_ACID,   "Your %s glows.")
OF(VULN_ELEC,   "Your %s glows.")
OF(VULN_FIRE,   "Your %s glows.")
OF(VULN_COLD,   "Your %s glows.")
OF(XXX3,        "Your %s glows.")
OF(XXX4,        "Your %s glows.")
OF(IM_ACID,     "Your %s glows.")
OF(IM_ELEC,     "Your %s glows.")
OF(IM_FIRE,     "Your %s glows.")
OF(IM_COLD,     "Your %s glows.")
OF(RES_ACID,    "Your %s glows.")
OF(RES_ELEC,    "Your %s glows.")
OF(RES_FIRE,    "Your %s glows.")
OF(RES_COLD,    "Your %s glows.")
OF(RES_POIS,    "Your %s glows.")
OF(RES_FEAR,    "Your %s glows.")
OF(RES_LIGHT,   "Your %s glows.")
OF(RES_DARK,    "Your %s glows.")
OF(RES_BLIND,   "Your %s glows.")
OF(RES_CONFU,   "Your %s glows.")
OF(RES_SOUND,   "Your %s glows.")
OF(RES_SHARD,   "Your %s glows.")
OF(RES_NEXUS,   "Your %s glows.")
OF(RES_NETHR,   "Your %s glows.")
OF(RES_CHAOS,   "Your %s glows.")
OF(RES_DISEN,   "Your %s glows.")
OF(SLOW_DIGEST, "You feel your %s slow your metabolism.")
OF(FEATHER,     "Your %s slows your fall.")
OF(LIGHT,       "")
OF(REGEN,       "You feel your %s speed up your recovery.")
OF(TELEPATHY,   "")
OF(SEE_INVIS,   "")
OF(FREE_ACT,    "Your %s glows.")
OF(HOLD_LIFE,   "Your %s glows.")
OF(NO_FUEL,     "")
OF(IMPAIR_HP,   "You feel your %s slow your recovery.")
OF(IMPAIR_MANA, "You feel your %s slow your mana recovery.")
OF(AFRAID,      "")
OF(IMPACT,      "Your %s causes an earthquake!")
OF(TELEPORT,    "Your %s teleports you.")
OF(AGGRAVATE,   "You feel your %s aggravate things around you.")
OF(DRAIN_EXP,   "You feel your %s drain your life.")
OF(IGNORE_ACID, "")
OF(IGNORE_ELEC, "")
OF(IGNORE_FIRE, "")
OF(IGNORE_COLD, "")
OF(XXX5,        "")
OF(XXX6,        "")
OF(BLESSED,     "")
OF(XXX7,        "")
OF(INSTA_ART,   "")
OF(EASY_KNOW,   "")
OF(HIDE_TYPE,   "")
OF(SHOW_MODS,   "")
OF(XXX8,        "")
OF(LIGHT_CURSE, "")
OF(HEAVY_CURSE, "")
OF(PERMA_CURSE, "")
