/*	SCCS Id: @(#)zap.c	3.4	2003/08/24	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Disintegration rays have special treatment; corpses are never left.
 * But the routine which calculates the damage is separate from the routine
 * which kills the monster.  The damage routine returns this cookie to
 * indicate that the monster should be disintegrated.
 */
#define MAGIC_COOKIE 1000

#ifdef OVLB
static NEARDATA boolean obj_zapped;
static NEARDATA int poly_zapped;
static NEARDATA boolean wan_wonder;
#endif

extern boolean notonhead;	/* for long worms */

/* kludge to use mondied instead of killed */
extern boolean m_using;

STATIC_DCL void costly_cancel(struct obj *);
STATIC_DCL void polyuse(struct obj*, int, int);
STATIC_DCL void create_polymon(struct obj *, int);
STATIC_DCL boolean zap_updown(struct obj *);
STATIC_DCL int zhitm(struct monst *,int,int, struct obj **);
STATIC_DCL void zhitu(int,int,const char *,XCHAR_P,XCHAR_P);
STATIC_DCL void revive_egg(struct obj *);
STATIC_DCL boolean zap_steed(struct obj *);

#ifdef OVLB
STATIC_DCL int zap_hit(int,int);
STATIC_DCL int zap_hit_player(int,int);
#endif
#ifdef OVL0
STATIC_DCL void backfire(struct obj *);
STATIC_DCL int spell_hit_bonus(int);
#endif

/* WAC -- ZT_foo #defines moved to spell.h, since explode uses these types */

#define is_hero_spell(type)	((type) >= 10 && (type) < 20)
#define is_mega_spell(type)	(type >= ZT_MEGA(ZT_FIRST) && \
				 type <= ZT_MEGA(ZT_LAST))

#define spellid(spell)          spl_book[spell].sp_id
#define spellname(spell)	OBJ_NAME(objects[spellid(spell)])

#ifndef OVLB
STATIC_VAR const char are_blinded_by_the_flash[];
extern const char * const flash_types[];
#else
STATIC_VAR const char are_blinded_by_the_flash[] = "are blinded by the flash!";

static const char all_count[] = { ALLOW_COUNT, ALL_CLASSES, 0 };

const char * const flash_types[] = {	/* also used in buzzmu(mcastu.c) */
	"magic missile",	/* Wands must be 0-9 */
	"bolt of fire",
	"bolt of cold",
	"sleep ray",
	"death ray",
	"bolt of lightning",
	"poison ray",
	"acid ray",
	"solar beam",
	"psybeam",

	"magic missile",	/* Spell equivalents must be 10-19 */
	"fireball",
	"cone of cold",
	"sleep ray",
	"finger of death",
	"bolt of lightning",    /* New spell & used for retribution */
	"blast of poison gas",  /*WAC New spells acid + poison*/
	"stream of acid",
	"solar beam",
	"psybeam",

	"blast of missiles",	/* Dragon breath equivalents 20-29*/
	"blast of fire",
	"blast of frost",
	"blast of sleep gas",
	"blast of disintegration",
	"blast of lightning",
	"blast of poison gas",
	"blast of acid",
	"solar beam",
	"psybeam",

	"magical blast",        /* Megaspell equivalents must be 30-39 */
	"fireball",             /*Should be same as explosion names*/
	"ball of cold",
	"sleep ball",
	"death field",
	"ball lightning",
	"poison gas cloud",
	"splash of acid",
	"pure irradiation",
	"psi ball"
};

/* Yells for Megaspells*/
const char *yell_types[] = {    /*10 different beam types*/
	"With the wisdom of Merlin!",  /* Magic */ 
		/* Merlin was the wizard who advised King Arthur */ 
	"The rage of Huitzilopochtli!", /* Fire */ 
		/* Huitzilopochtli is the god of the Sun of Aztec myth */ 
	"The sorrow of Demeter!", /* Frost */
		/* Demeter - when she is without her daughter Persephone
		 * 	she caused winter
		 */
	"The rest of Hypnos!", /* Sleep */ 
		/* Hypnos - Greek god of sleep */
	"The wrath of Kali!", /* Disint */
		/* Kali is the Hindu god of dissolution and destruction */
	"From the forge of Vulcan!", /* Lightning*/ 
		/* Vulcan forged Zeus' lightning bolts [Greek] */
	"From the Fangs of Jormungand!", /* Poison gas */ 
		/* Serpent of Viking Mythology.  Poisons the skies/seas during 
		 * Ragnarok 
		 */
	"The anger of the Mad Chemist!", /* Acid */
	"Feel the power of the Sun itself!",
	"The might of Arceus will crush you!" /* Psybeam */
};


/* Routines for IMMEDIATE wands and spells. */
/* bhitm: monster mtmp was hit by the effect of wand or spell otmp */
int
bhitm(mtmp, otmp)
struct monst *mtmp;
struct obj *otmp;
{
	boolean wake = TRUE;	/* Most 'zaps' should wake monster */
	boolean reveal_invis = FALSE;
	boolean dbldam = Role_if(PM_KNIGHT) && u.uhave.questart;
	int dmg, otyp = otmp->otyp;
	int hurtlex,hurtley;
	const char *zap_type_text = "spell";
	struct obj *obj;
	boolean disguised_mimic = (mtmp->data->mlet == S_MIMIC &&
				   mtmp->m_ap_type != M_AP_NOTHING);
	int pm_index;
	int skilldmg = 0;

	if (objects[otyp].oc_class == SPBOOK_CLASS) {
	    /* Is a spell */
	    skilldmg = spell_damage_bonus(otyp);  
	    zap_type_text = "spell";
	} else zap_type_text = "wand";

	if (u.uswallow && mtmp == u.ustuck)
	    reveal_invis = FALSE;

	ranged_thorns(mtmp);

	switch(otyp) {

	case WAN_GOOD_NIGHT:
	case SPE_GOOD_NIGHT:
		dmg = d(2,12) + rnz(u.ulevel);
		if (otyp == WAN_GOOD_NIGHT) dmg += rnz(u.ulevel);
		if(dbldam) dmg *= 2;
		dmg += skilldmg;
		if (mtmp->data->maligntyp > 0) dmg *= 10;
		if (mtmp->data->maligntyp < 0) dmg /= 2;
		if (is_undead(mtmp->data)) dmg /= 2;
		if (mtmp->data->maligntyp > 0) pline("%s is hit hard!", Monnam(mtmp));
		else {
			if (is_undead(mtmp->data) && mtmp->data->maligntyp < 0) pline("%s resists a lot.", Monnam(mtmp));
			else if (mtmp->data->maligntyp < 0) pline("%s resists.", Monnam(mtmp));
			else if (is_undead(mtmp->data)) pline("%s resists.", Monnam(mtmp));
		}
		(void) resist(mtmp, otmp->oclass, dmg, NOTELL);
		break;

	case WAN_BUBBLEBEAM:
	case SPE_BUBBLEBEAM:
		if (breathless(mtmp->data) || is_swimmer(mtmp->data) || amphibious(mtmp->data)) {
			if (canseemon(mtmp)) pline("%s is unaffected!", Monnam(mtmp));
		} else {
			dmg = d(4,12) + rnz(u.ulevel);
			if(dbldam) dmg *= 2;
			dmg += skilldmg;
			if (canseemon(mtmp)) pline("%s is immersed in a water bubble!", Monnam(mtmp));
			(void) resist(mtmp, otmp->oclass, dmg, NOTELL);
		}
		break;

	case SPE_BATTERING_RAM:

		dmg = d(10, 10) + rnz(u.ulevel * 4);

		if (distu(mtmp->mx, mtmp->my) > 3) {
			wake = FALSE;
			break;
		}
		pline("%s is battered!", Monnam(mtmp));
		if (dmg > 0) (void) resist(mtmp, otmp->oclass, dmg, NOTELL);

		if (!DEADMONSTER(mtmp)) {
			int mdx, mdy;
			mdx = mtmp->mx + u.dx;
			mdy = mtmp->my + u.dy;
			if(goodpos(mdx, mdy, mtmp, 0)) {
				pline("%s is pushed back!", Monnam(mtmp));
				if (m_in_out_region(mtmp, mdx, mdy)) {
				    remove_monster(mtmp->mx, mtmp->my);
				    newsym(mtmp->mx, mtmp->my);
				    place_monster(mtmp, mdx, mdy);
				    newsym(mtmp->mx, mtmp->my);
				    set_apparxy(mtmp);
				}
			}

		}

		break;

	case SPE_BLOOD_STREAM:
		{
			int streambonus = 0;
			if (u.uhp < (u.uhpmax * 4 / 5)) streambonus = 20;
			if (u.uhp < (u.uhpmax * 3 / 5)) streambonus = 40;
			if (u.uhp < (u.uhpmax * 2 / 5)) streambonus = 50;
			if (u.uhp < (u.uhpmax * 3 / 10)) streambonus = 60;
			if (u.uhp < (u.uhpmax * 1 / 5)) streambonus = 75;
			if (u.uhp < (u.uhpmax * 1 / 10)) streambonus = 100;

			dmg = d(4,10) + rnd(u.ulevel) + (spell_damage_bonus(SPE_BLOOD_STREAM) * 3);
			dmg *= (100 + streambonus);
			dmg /= 100;
			if (flags.female) {
				dmg *= 6;
				dmg /= 5;
			}
			if (dmg > 0) {
				pline("%s is drenched with your menstruation!", Monnam(mtmp));
				(void) resist(mtmp, otmp->oclass, dmg, NOTELL);
			}

		}

		break;

	case SPE_GEYSER:

		if (is_swimmer(mtmp->data) || amphibious(mtmp->data) || dmgtype(mtmp->data, AD_WET)) {
			pline("%s is unaffected!", Monnam(mtmp));
			break;
		}

		dmg = d(4,12) + rnd(u.ulevel);
		if(dbldam) dmg *= 2;
		dmg += skilldmg;

		if (dmg > 0) pline("%s is buffeted by the water.", Monnam(mtmp));
		(void) resist(mtmp, otmp->oclass, dmg, NOTELL);

		break;

	case SPE_SHINING_WAVE:

		{
			int wavebonus = 0;
			if (ACURR(A_STR) <= 18) wavebonus = ACURR(A_STR);
			else if (ACURR(A_STR) <= STR18(10)) wavebonus = 19;
			else if (ACURR(A_STR) <= STR18(20)) wavebonus = 20;
			else if (ACURR(A_STR) <= STR18(30)) wavebonus = 21;
			else if (ACURR(A_STR) <= STR18(40)) wavebonus = 22;
			else if (ACURR(A_STR) <= STR18(50)) wavebonus = 23;
			else if (ACURR(A_STR) <= STR18(60)) wavebonus = 24;
			else if (ACURR(A_STR) <= STR18(70)) wavebonus = 25;
			else if (ACURR(A_STR) <= STR18(80)) wavebonus = 26;
			else if (ACURR(A_STR) <= STR18(90)) wavebonus = 27;
			else if (ACURR(A_STR) <= STR19(19)) wavebonus = 28;
			else if (ACURR(A_STR) <= STR19(20)) wavebonus = 29;
			else if (ACURR(A_STR) <= STR19(21)) wavebonus = 30;
			else if (ACURR(A_STR) <= STR19(22)) wavebonus = 31;
			else if (ACURR(A_STR) <= STR19(23)) wavebonus = 32;
			else if (ACURR(A_STR) <= STR19(24)) wavebonus = 33;
			else wavebonus = 34;

			dmg = d(5 + spell_damage_bonus(SPE_SHINING_WAVE), wavebonus);
			dmg *= (flags.female ? rno(2) : rnd(3)); /* gotta do enough damage to make it worth the cost */

			if (resists_magm(mtmp)) {
				if (flags.female) {
					pline("%s is unaffected!", Monnam(mtmp));
					break;
				} else dmg /= 2;
			}
			if (dmgtype(mtmp->data, AD_SOUN)) {
				if (flags.female) {
					pline("%s is unaffected!", Monnam(mtmp));
					break;
				} else dmg /= 2;
			}

			if (dmg > 0) {
				pline("%s is hit by the wave!", Monnam(mtmp));
				(void) resist(mtmp, otmp->oclass, dmg, NOTELL);
			}

		}

		break;

	case SPE_NERVE_POISON:

		dmg = 0;

		if (distu(mtmp->mx, mtmp->my) > 3) {
			wake = FALSE;
			break;
		}

		if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {

			pline("%s's nerves are poisoned!", Monnam(mtmp));
			if (!dmgtype(mtmp->data, AD_PLYS)) {
				mtmp->mcanmove = 0;
				mtmp->mfrozen = rnd(4);
				mtmp->mstrategy &= ~STRAT_WAITFORU;
			}

			if (!resists_poison(mtmp)) {
				if (!rn2(500)) {
					Your("poison was deadly...");
					dmg = (mtmp->mhp * 10);
				} else dmg = rn1(10, 6);

			} else pline("%s is unaffected by the poison.", Monnam(mtmp));

			if (dmg > 0) (void) resist(mtmp, otmp->oclass, dmg, NOTELL);

		}

		break;

	case SPE_GIANT_FOOT:

		dmg = 0;

		if (distu(mtmp->mx, mtmp->my) > 3) {
			wake = FALSE;
			break;
		}

		dmg = d(6,18) + rnd(u.ulevel * 2);
		dmg += (skilldmg * 5);
		if (amorphous(mtmp->data)) dmg /= rn1(3, 3);
		if (noncorporeal(mtmp->data)) dmg = 0;
		if (is_whirly(mtmp->data)) dmg /= rnd(3);

		pline("A giant foot falls on %s!", mon_nam(mtmp));
		if (dmg > 0) (void) resist(mtmp, otmp->oclass, dmg, NOTELL);
		else pline("%s is unaffected.", Monnam(mtmp));

		break;

	case SPE_VOLT_ROCK:

		dmg = 0;

		if (distu(mtmp->mx, mtmp->my) > 3) {
			wake = FALSE;
			break;
		}
		if (!which_armor(mtmp, W_ARMH)) {
			pline("Your rock hits %s's %s!", mon_nam(mtmp), mbodypart(mtmp, HEAD));
			dmg += (10 + rnd(u.ulevel) + skilldmg);
		}
		if (!resists_elec(mtmp)) {
			pline("%s is electrified!", Monnam(mtmp));
			dmg += (10 + rnd(u.ulevel) + skilldmg);
		}
		if (dmg > 0) (void) resist(mtmp, otmp->oclass, dmg, NOTELL);

		break;

	case SPE_BUBBLING_HOLE:

		dmg = 0;

		if (distu(mtmp->mx, mtmp->my) > 3) {
			wake = FALSE;
			break;
		}

		    if (Can_rise_up(mtmp->mx, mtmp->my, &u.uz)) {
			register int tolev = depth(&u.uz)-1;
			d_level tolevel;

			get_level(&tolevel, tolev);
			/* insurance against future changes... */
			if(on_level(&tolevel, &u.uz)) goto skipmsg;
			pline("%s rises up, through the %s!", Monnam(mtmp), ceiling(mtmp->mx, mtmp->my));
			migrate_to_level(mtmp, ledger_no(&tolevel), MIGR_RANDOM, (coord *)0);
			break;
		    } else {
skipmsg:
			pline("%s hits %s %s on the ceiling.", Monnam(mtmp), mhis(mtmp), mbodypart(mtmp, HEAD));

			dmg = d(10, 20) + rnz(u.ulevel * 5);

			if (dmg > 0) (void) resist(mtmp, otmp->oclass, dmg, NOTELL);

		    }

		break;

	case SPE_STRANGLING:

		dmg = 0;

		if (distu(mtmp->mx, mtmp->my) > 3) {
			wake = FALSE;
			break;
		}

		if (breathless(mtmp->data)) {
			pline("%s doesn't need to breathe and therefore cannot be strangled!", Monnam(mtmp));
			break;
		}

		dmg = rnd(10) + rnd(u.ulevel * rnd(2)) + skilldmg;
		if (humanoid(mtmp->data)) {
			dmg += rnd(u.ulevel);
			pline("%s is gasping for air!", Monnam(mtmp));
		} else pline("%s is being strangled.", Monnam(mtmp));

		if (dmg > 0) (void) resist(mtmp, otmp->oclass, dmg, NOTELL);

		break;

	case SPE_ARMOR_SMASH:

		dmg = 0;

		if (distu(mtmp->mx, mtmp->my) > 3) {
			wake = FALSE;
			break;
		}

		{

			/* destroy random armor piece worn by the victim; if it destroys one, stop, but if it tries to destroy
			 * an artifact (which is immune to this effect), keep rolling for other slots --Amy */

			struct obj *otmpS;

			if (resists_magm(mtmp) && rn2(20)) {
				pline("A field of force surrounds %s!", mon_nam(mtmp));
				break;
			}

			int astries = 200;
			int diceroll;
armorsmashrepeat:
			diceroll = rnd(5);

			switch (diceroll) {
				case 1:
					if (mtmp->misc_worn_check & W_ARMS) {
					    otmpS = which_armor(mtmp, W_ARMS);
					    if (otmpS->oartifact) goto armorsmashdone;
					    if (otmpS->blessed && rn2(5)) goto armorsmashdone;
					    if (otmpS->greased) {
							if (rn2(2)) otmpS->greased--;
							goto armorsmashdone;
					    }
					    if (otmpS->spe > 1 && rn2(otmpS->spe)) goto armorsmashdone;
					    pline("%s %s is disintegrated!", s_suffix(Monnam(mtmp)), distant_name(otmpS, xname));
					    m_useup(mtmp, otmpS);
					    goto armorsmashdone;
					}
					break;
				case 2:
					if (mtmp->misc_worn_check & W_ARMG) {
					    otmpS = which_armor(mtmp, W_ARMG);
					    if (otmpS->oartifact) goto armorsmashdone;
					    if (otmpS->blessed && rn2(5)) goto armorsmashdone;
					    if (otmpS->greased) {
							if (rn2(2)) otmpS->greased--;
							goto armorsmashdone;
					    }
					    if (otmpS->spe > 1 && rn2(otmpS->spe)) goto armorsmashdone;
					    pline("%s %s is disintegrated!", s_suffix(Monnam(mtmp)), distant_name(otmpS, xname));
					    m_useup(mtmp, otmpS);
					    goto armorsmashdone;
					}
					break;
				case 3:
					if (mtmp->misc_worn_check & W_ARMF) {
					    otmpS = which_armor(mtmp, W_ARMF);
					    if (otmpS->oartifact) goto armorsmashdone;
					    if (otmpS->blessed && rn2(5)) goto armorsmashdone;
					    if (otmpS->greased) {
							if (rn2(2)) otmpS->greased--;
							goto armorsmashdone;
					    }
					    if (otmpS->spe > 1 && rn2(otmpS->spe)) goto armorsmashdone;
					    pline("%s %s is disintegrated!", s_suffix(Monnam(mtmp)), distant_name(otmpS, xname));
					    m_useup(mtmp, otmpS);
					    goto armorsmashdone;
					}
					break;
				case 4:
					if (mtmp->misc_worn_check & W_ARMH) {
					    otmpS = which_armor(mtmp, W_ARMH);
					    if (otmpS->oartifact) goto armorsmashdone;
					    if (otmpS->blessed && rn2(5)) goto armorsmashdone;
					    if (otmpS->greased) {
							if (rn2(2)) otmpS->greased--;
							goto armorsmashdone;
					    }
					    if (otmpS->spe > 1 && rn2(otmpS->spe)) goto armorsmashdone;
					    pline("%s %s is disintegrated!", s_suffix(Monnam(mtmp)), distant_name(otmpS, xname));
					    m_useup(mtmp, otmpS);
					    goto armorsmashdone;
					}
					break;
				case 5:
					if (mtmp->misc_worn_check & W_ARMC) {
					    otmpS = which_armor(mtmp, W_ARMC);
					    if (otmpS->oartifact) goto armorsmashdone;
					    if (otmpS->blessed && rn2(5)) goto armorsmashdone;
					    if (otmpS->greased) {
							if (rn2(2)) otmpS->greased--;
							goto armorsmashdone;
					    }
					    if (otmpS->spe > 1 && rn2(otmpS->spe)) goto armorsmashdone;
					    pline("%s %s is disintegrated!", s_suffix(Monnam(mtmp)), distant_name(otmpS, xname));
					    m_useup(mtmp, otmpS);
					    goto armorsmashdone;
					} else if (mtmp->misc_worn_check & W_ARM) {
					    otmpS = which_armor(mtmp, W_ARM);
					    if (otmpS->oartifact) goto armorsmashdone;
					    if (otmpS->blessed && rn2(5)) goto armorsmashdone;
					    if (otmpS->greased) {
							if (rn2(2)) otmpS->greased--;
							goto armorsmashdone;
					    }
					    if (otmpS->spe > 1 && rn2(otmpS->spe)) goto armorsmashdone;
					    pline("%s %s is disintegrated!", s_suffix(Monnam(mtmp)), distant_name(otmpS, xname));
					    m_useup(mtmp, otmpS);
					    goto armorsmashdone;
					} else if (mtmp->misc_worn_check & W_ARMU) {
					    otmpS = which_armor(mtmp, W_ARMU);
					    if (otmpS->oartifact) goto armorsmashdone;
					    if (otmpS->blessed && rn2(5)) goto armorsmashdone;
					    if (otmpS->greased) {
							if (rn2(2)) otmpS->greased--;
							goto armorsmashdone;
					    }
					    if (otmpS->spe > 1 && rn2(otmpS->spe)) goto armorsmashdone;
					    pline("%s %s is disintegrated!", s_suffix(Monnam(mtmp)), distant_name(otmpS, xname));
					    m_useup(mtmp, otmpS);
					    goto armorsmashdone;
					}
					break;
			}

			if (astries > 0) {
				astries--;
				goto armorsmashrepeat;
			} else pline("%s doesn't seem to be wearing any armor that can be destroyed...", Monnam(mtmp));

		}

armorsmashdone:

		break;

	case SPE_WATER_FLAME:

		dmg = 2;
		dmg += skilldmg;
		if (resists_cold(mtmp)) {
			pline("%s is burned by the watery flame!", Monnam(mtmp));
			dmg += (rnd(u.ulevel) + skilldmg);
		}
		if (resists_fire(mtmp)) {
			pline("%s is cooled by the watery flame!", Monnam(mtmp));
			dmg += (rnd(u.ulevel) + skilldmg);
		}
		(void) resist(mtmp, otmp->oclass, dmg, NOTELL);

		break;

	case WAN_DREAM_EATER:
	case SPE_DREAM_EATER:
		if (!(mtmp->mcanmove)) {
			dmg = d(8,12) + rnz(u.ulevel * 3);
			if(dbldam) dmg *= 2;
			dmg += skilldmg;
			if (canseemon(mtmp)) pline("%s's dream is eaten!", Monnam(mtmp));
			(void) resist(mtmp, otmp->oclass, dmg, NOTELL);
		}
		break;

	case SPE_BLINDING_RAY:

		if (!resist(mtmp, otmp->oclass, 0, NOTELL) && !resists_blnd(mtmp) ) {
		    pline("%s is blinded by the flash!", Monnam(mtmp));
		    mtmp->mcansee = 0;
		    mtmp->mblinded = rnd(10);
		}

		break;

	case SPE_MANA_BOLT:
	case SPE_SNIPER_BEAM:
		dmg = d(2, 4);
		if(dbldam) dmg *= 2;
		dmg += skilldmg;
		if (canseemon(mtmp)) pline("%s is irradiated with energy!", Monnam(mtmp));
		(void) resist(mtmp, otmp->oclass, dmg, NOTELL);
		break;

	case SPE_PARTICLE_CANNON:
		dmg = d(5, 10);
		dmg += rnd(u.ulevel * rno(3));
		if(dbldam) dmg *= 2;
		dmg += (skilldmg * rnd(10));
		if (canseemon(mtmp)) pline("%s is caught in the particle beam!", Monnam(mtmp));
		(void) resist(mtmp, otmp->oclass, dmg, NOTELL);
		break;

	case SPE_BLANK_PAPER: /* placeholder for blade anger and beamsword; sadly you can't be using both at the same time */

		if (tech_inuse(T_BLADE_ANGER)) {
			dmg = bigmonst(mtmp->data) ? 6 : 8;
			dmg += otmp->spe;
			if (dmg < 1) dmg = 1; /* fail safe for cursed ones */
			if (canseemon(mtmp)) pline("%s is slit by your shuriken!", Monnam(mtmp));
			else pline("It is slit by your shuriken!");
			(void) resist(mtmp, WEAPON_CLASS, dmg, NOTELL);
		}

		if (tech_inuse(T_BEAMSWORD)) {
			dmg = rnd(20);

			/* djem so skill will most probably be pretty high, but just in case it isn't, reduce damage --Amy */
			if (!(PlayerCannotUseSkills)) {
				switch (P_SKILL(P_DJEM_SO)) {
					case P_BASIC: dmg = rnd(28); break;
					case P_SKILLED: dmg = rnd(36); break;
					case P_EXPERT: dmg = rnd(44); break;
					case P_MASTER: dmg = rnd(52); break;
					case P_GRAND_MASTER: dmg = rnd(60); break;
					case P_SUPREME_MASTER: dmg = rnd(70); break;
					default: break;
				}
			}

			if (canseemon(mtmp)) pline("%s is hit by your lightsaber beam!", Monnam(mtmp));
			else pline("It is hit by your lightsaber beam!");
			(void) resist(mtmp, WEAPON_CLASS, dmg, NOTELL);
		}

		break;

	case SPE_ENERGY_BOLT:
		dmg = d(8, 4);
		if(dbldam) dmg *= 2;
		dmg += (skilldmg * 4);
		if (canseemon(mtmp)) pline("%s is irradiated with energy!", Monnam(mtmp));
		(void) resist(mtmp, otmp->oclass, dmg, NOTELL);
		break;

	case WAN_CHLOROFORM:
	case SPE_CHLOROFORM:
		dmg = d(2, 9) + (rnz(u.ulevel) / 2);
		if (otyp == WAN_CHLOROFORM) dmg += (rnz(u.ulevel) / 2);
		if(dbldam) dmg *= 2;
		dmg += skilldmg;
		if (canseemon(mtmp)) pline("%s is enveloped in a cloud of gas!", Monnam(mtmp));
		(void) resist(mtmp, otmp->oclass, dmg, NOTELL);
		break;

	case WAN_AURORA_BEAM:
	case SPE_AURORA_BEAM:
		(void) cancel_monst(mtmp, otmp, TRUE, TRUE, FALSE);
		break;

	case WAN_NETHER_BEAM:
	case SPE_NETHER_BEAM:
		if (!resist(mtmp, otmp->oclass, 0, NOTELL) && mtmp->mhp > 1) {
			mtmp->mhp /= 2;
			if (canseemon(mtmp)) pline("%s is severely damaged by nether forces!", Monnam(mtmp));
		}
		break;

	case WAN_TOXIC:
	case SPE_TOXIC:
		if (!resists_poison(mtmp)) {
			if (canseemon(mtmp)) pline("%s is badly poisoned!", Monnam(mtmp));
			while ((!resist(mtmp, otmp->oclass, 0, NOTELL) && rn2(20) && (mtmp->mhp > 1) ) ) {
				mtmp->mhp--;
			}
		}
		break;

	case WAN_SLUDGE:
	case SPE_SLUDGE:
		if (!resist(mtmp, otmp->oclass, 0, NOTELL) && mtmp->mhp > 0) {
			if (mtmp->m_lev < 1)
				xkilled(mtmp, 1);
			else {
				mtmp->m_lev--;
				if (canseemon(mtmp))
					pline("%s suddenly seems weaker!", Monnam(mtmp));
			}
		}

		break;

	case SPE_CALL_THE_ELEMENTS:

		if (mtmp->mcanmove && !rn2(3) && !(dmgtype(mtmp->data, AD_PLYS)) ) {
			mtmp->mcanmove = 0;
			mtmp->mfrozen = rnd(2);
		    if (canseemon(mtmp)) pline("%s is paralyzed!", Monnam(mtmp));
		}

		if (!(mtmp->mstun) && !rn2(2)) {
			mtmp->mstun = TRUE;
		    if (canseemon(mtmp)) pline("%s is numbed!", Monnam(mtmp));
		}

		if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
			mon_adjust_speed(mtmp, -1, otmp);
			m_dowear(mtmp, FALSE); /* might want speed boots */
		}

		if (!(mtmp->mblinded) && mtmp->mcansee) {
		    mtmp->mblinded = rnd(8);
		    mtmp->mcansee = 0;
		    if (canseemon(mtmp)) pline("%s is blinded by the flames!", Monnam(mtmp));
		}
		if (mtmp->mhp > 1) {
			mtmp->mhp--;
			mtmp->mhpmax--;
		    if (canseemon(mtmp)) pline("%s is burned!", Monnam(mtmp));
		}

		break;

	case WAN_THUNDER:
	case SPE_THUNDER:
		if (mtmp->mcanmove && !rn2(3) && !(dmgtype(mtmp->data, AD_PLYS)) ) {
			mtmp->mcanmove = 0;
			mtmp->mfrozen = rnd(3);
		    if (canseemon(mtmp)) pline("%s is paralyzed!", Monnam(mtmp));
		}

		if (!(mtmp->mstun) && !rn2(2)) {
			mtmp->mstun = TRUE;
		    if (canseemon(mtmp)) pline("%s is numbed!", Monnam(mtmp));
		}

		break;

	case WAN_ICE_BEAM:
	case SPE_ICE_BEAM:
		if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
			mon_adjust_speed(mtmp, -1, otmp);
			m_dowear(mtmp, FALSE); /* might want speed boots */
		}
		break;

	case WAN_INFERNO:
	case SPE_INFERNO:
		if (!(mtmp->mblinded) && mtmp->mcansee) {
		    mtmp->mblinded = rnd(20);
		    mtmp->mcansee = 0;
		    if (canseemon(mtmp)) pline("%s is blinded by the flames!", Monnam(mtmp));
		}
		if (mtmp->mhp > 1) {
			mtmp->mhp--;
			mtmp->mhpmax--;
		    if (canseemon(mtmp)) pline("%s is burned!", Monnam(mtmp));
		}

		break;

	case WAN_GRAVITY_BEAM:
	case SPE_GRAVITY_BEAM:
		zap_type_text = "gravity beam";
		reveal_invis = TRUE;
		if (u.uswallow || rnd(20) < 10 + find_mac(mtmp) + rnz(u.ulevel) ) {
			dmg = d(4,12) + rnz(u.ulevel);
			if (otyp == SPE_GRAVITY_BEAM && !rn2(3)) dmg = d(4,12) + rnd(u.ulevel);
			if (otyp == SPE_GRAVITY_BEAM && !rn2(3) && dmg > 1) dmg = rnd(dmg);
			if (bigmonst(mtmp->data)) dmg += rnd(6);
			if (mtmp->data->msize >= MZ_HUGE) dmg += rnd(12);
			if (mtmp->data->msize >= MZ_GIGANTIC) dmg += rnd(18);
			if (otyp == WAN_GRAVITY_BEAM) dmg += rnz(u.ulevel * 2);
			if(dbldam) dmg *= 2;
			dmg += (skilldmg * 2);
			hit(zap_type_text, mtmp, exclam(dmg));
			(void) resist(mtmp, otmp->oclass, dmg, TELL);
		} else miss(zap_type_text, mtmp);
		makeknown(otyp);
		break;

	case WAN_STRIKING:
		zap_type_text = "wand";
		/* fall through */
	case SPE_FORCE_BOLT:
		reveal_invis = TRUE;
		if (resists_magm(mtmp)) {	/* match effect on player */
			shieldeff(mtmp->mx, mtmp->my);
			break;	/* skip makeknown */
		} else if (u.uswallow || rnd(20) < 10 + find_mac(mtmp) + rnz(u.ulevel) ) {
			dmg = d(2,12) + rnz(u.ulevel);
			if (otyp == WAN_STRIKING) dmg += rnz(u.ulevel);
			/* teh hardcore nerf by Amy - force bolt is just plain too strong */
			if (otyp == SPE_FORCE_BOLT && rn2(3)) dmg = d(2,12) + rnd(u.ulevel);
			if (otyp == SPE_FORCE_BOLT && rn2(2) && dmg > 1) dmg = rnd(dmg);
			if(dbldam) dmg *= 2;
			dmg += skilldmg;
			hit(zap_type_text, mtmp, exclam(dmg));
			(void) resist(mtmp, otmp->oclass, dmg, TELL);
		} else miss(zap_type_text, mtmp);
		makeknown(otyp);
		break;
	case WAN_CLONE_MONSTER:
		clone_mon(mtmp, 0, 0);
		makeknown(otyp);
		break;
	case SPE_CLONE_MONSTER:
		clone_mon(mtmp, 0, 0);
		break;
	case WAN_SLOW_MONSTER:
	case SPE_SLOW_MONSTER:
		if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
			mon_adjust_speed(mtmp, -1, otmp);
			m_dowear(mtmp, FALSE); /* might want speed boots */
			if (u.uswallow && (mtmp == u.ustuck) &&
			    is_whirly(mtmp->data)) {
				You("disrupt %s!", mon_nam(mtmp));
				pline("A huge hole opens up...");
				expels(mtmp, mtmp->data, TRUE);
			}
		}
		break;
	case SPE_RANDOM_SPEED:
		if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
			if (!rn2(2)) {
				mon_adjust_speed(mtmp, -1, otmp);
				m_dowear(mtmp, FALSE); /* might want speed boots */
				if (u.uswallow && (mtmp == u.ustuck) &&
				    is_whirly(mtmp->data)) {
					You("disrupt %s!", mon_nam(mtmp));
					pline("A huge hole opens up...");
					expels(mtmp, mtmp->data, TRUE);
				}
			} else {
				mon_adjust_speed(mtmp, 1, otmp);
				m_dowear(mtmp, FALSE); /* might want speed boots */
			}
		}
		break;

	case SPE_CHAOS_BOLT:
		if (dmgtype(mtmp->data, AD_HALU) ) {
			shieldeff(mtmp->mx, mtmp->my);
		    break;
		}
		dmg = d(3,10) + rnz(u.ulevel);
		dmg += skilldmg;
		hit(zap_type_text, mtmp, exclam(dmg));
		(void) resist(mtmp, otmp->oclass, dmg, TELL);

		if (mtmp->mhp < 1) break; /* no longer alive */

		if (!rn2(3) && !resist(mtmp, otmp->oclass, 0, NOTELL)) {
		    if (mtmp->cham == CHAM_ORDINARY && !rn2(25)) {
			if (canseemon(mtmp)) {
			    pline("%s shudders!", Monnam(mtmp));
			}
			xkilled(mtmp, 3);
		    } else mon_spec_poly(mtmp, (struct permonst *)0, 0L, 1, canseemon(mtmp), FALSE, TRUE);
		}

		break;

	case SPE_HELLISH_BOLT:
		if (dmgtype(mtmp->data, AD_HALU) ) {
			shieldeff(mtmp->mx, mtmp->my);
		    break;
		}
		dmg = d(6,10) + rnz(u.ulevel) + rnz(u.ulevel);
		dmg += skilldmg;
		hit(zap_type_text, mtmp, exclam(dmg));
		(void) resist(mtmp, otmp->oclass, dmg, TELL);

		if (mtmp->mhp < 1) break; /* no longer alive */

		if (!rn2(3)) {

			if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
				if (mtmp->cham == CHAM_ORDINARY && !rn2(25)) {
					if (canseemon(mtmp)) {
						pline("%s shudders!", Monnam(mtmp));
					}
				xkilled(mtmp, 3);
				} else mon_spec_poly(mtmp, (struct permonst *)0, 0L, 1, canseemon(mtmp), FALSE, TRUE);
			}

		} else if (!rn2(2)) {

			if (resists_drli(mtmp)) {
				shieldeff(mtmp->mx, mtmp->my);
				break;
			} else if (!resist(mtmp, otmp->oclass, dmg, NOTELL) && mtmp->mhp > 0) {
				mtmp->m_lev--;
				if (canseemon(mtmp))
					pline("%s suddenly seems weaker!", Monnam(mtmp));
			}

		} else if (!(dmgtype(mtmp->data, AD_PLYS))) {

			if (canseemon(mtmp) ) {
				pline("%s is frozen by the beam.", Monnam(mtmp) );
			}
			mtmp->mcanmove = 0;
			mtmp->mfrozen = rn2(3) ? rnz(5) : rnz(5 + skilldmg);
			mtmp->mstrategy &= ~STRAT_WAITFORU;

		}

		break;

	case WAN_INERTIA:
		mtmp->mcanmove = 0;
		mtmp->mfrozen = rnd(2);
		mtmp->mstrategy &= ~STRAT_WAITFORU;
		/* fall through */
	case SPE_INERTIA:
		mon_adjust_speed(mtmp, -1, otmp);
		m_dowear(mtmp, FALSE); /* might want speed boots */
		if (u.uswallow && (mtmp == u.ustuck) &&
		    is_whirly(mtmp->data)) {
			You("disrupt %s!", mon_nam(mtmp));
			pline("A huge hole opens up...");
			expels(mtmp, mtmp->data, TRUE);
		}
		break;
	case WAN_SPEED_MONSTER:
	case WAN_HASTE_MONSTER:
		if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
			mon_adjust_speed(mtmp, 1, otmp);
			m_dowear(mtmp, FALSE); /* might want speed boots */
		}
		break;
	case SPE_TURN_UNDEAD:
	case WAN_UNDEAD_TURNING:
		wake = FALSE;
		if (unturn_dead(mtmp)) wake = TRUE;
		if (is_undead(mtmp->data) || mtmp->egotype_undead) {
			reveal_invis = TRUE;
			wake = TRUE;
			dmg = rnd(8);
			if(dbldam) dmg *= 2;
			dmg += skilldmg;
			if (otyp == WAN_UNDEAD_TURNING) {
				dmg += rnz(u.ulevel);
				dmg *= (2 + rn2(2));
			}
			flags.bypasses = TRUE;	/* for make_corpse() */
			if (!resist(mtmp, otmp->oclass, dmg, NOTELL)) {
			    if (mtmp->mhp > 0) monflee(mtmp, rnd(10), FALSE, TRUE);
			}
		}
		break;
	case WAN_POLYMORPH:
	case SPE_POLYMORPH:
	case POT_POLYMORPH:
		if (resists_magm(mtmp)) {
		    /* magic resistance protects from polymorph traps, so make
		       it guard against involuntary polymorph attacks too... */
		    shieldeff(mtmp->mx, mtmp->my);
		} else if (!resist(mtmp, otmp->oclass, 0, NOTELL)) {
		    /* natural shapechangers aren't affected by system shock
		       (unless protection from shapechangers is interfering
		       with their metabolism...) */
		    if (mtmp->cham == CHAM_ORDINARY && !rn2(25)) {
			if (canseemon(mtmp)) {
			    pline("%s shudders!", Monnam(mtmp));
			    makeknown(otyp);
			}
			/* dropped inventory shouldn't be hit by this zap */
			for (obj = mtmp->minvent; obj; obj = obj->nobj)
			    bypass_obj(obj);
			/* flags.bypasses = TRUE; ## for make_corpse() */
			/* no corpse after system shock */
			xkilled(mtmp, 3);
		    } else if (mon_spec_poly(mtmp, (struct permonst *)0, 0L,
			    (otyp != POT_POLYMORPH), canseemon(mtmp), FALSE,
			    TRUE)) {
			if (!Hallucination && canspotmon(mtmp))
			    makeknown(otyp);
		    }
		}
		break;
	case WAN_MUTATION:
	case SPE_MUTATION:
		add_monster_egotype(mtmp);
		break;

	case WAN_CANCELLATION:
	case SPE_CANCELLATION:
		(void) cancel_monst(mtmp, otmp, TRUE, TRUE, FALSE);
		break;

	case SPE_VANISHING:

		if (!rn2(3)) {
			(void) cancel_monst(mtmp, otmp, TRUE, TRUE, FALSE);
		} else if (!rn2(2)) {
			reveal_invis = !u_teleport_mon(mtmp, TRUE);
		} else {
			int oldinvis = mtmp->minvis;
			char nambuf[BUFSZ];

			/* format monster's name before altering its visibility */
			strcpy(nambuf, Monnam(mtmp));
			mon_set_minvis(mtmp);
			if (!oldinvis && knowninvisible(mtmp)) {
			    pline("%s turns transparent!", nambuf);
			}
		}

		break;

	case WAN_PARALYSIS:
		if (dmgtype(mtmp->data, AD_PLYS)) break;

		if (canseemon(mtmp) ) {
			pline("%s is frozen by the beam.", Monnam(mtmp) );
		}
		mtmp->mcanmove = 0;
		mtmp->mfrozen = rnz(20);
		mtmp->mstrategy &= ~STRAT_WAITFORU;

		break;
	case SPE_PARALYSIS:
		if (dmgtype(mtmp->data, AD_PLYS)) break;

		if (canseemon(mtmp) ) {
			pline("%s is frozen by the beam.", Monnam(mtmp) );
		}
		mtmp->mcanmove = 0;
		mtmp->mfrozen = rn2(3) ? rnz(5) : rnz(5 + skilldmg);
		mtmp->mstrategy &= ~STRAT_WAITFORU;

		break;
	case WAN_DISINTEGRATION:
	case SPE_DISINTEGRATION:
		{

			struct obj *otmpS;

		    if (resists_disint(mtmp)) {

			pline("%s is unharmed.", Monnam(mtmp) );

		    } else if (mtmp->misc_worn_check & W_ARMS) {
			    /* destroy shield; other possessions are safe */
			    otmpS = which_armor(mtmp, W_ARMS);
			    pline("%s %s is disintegrated!", s_suffix(Monnam(mtmp)), distant_name(otmpS, xname));
			    m_useup(mtmp, otmpS);
			    break;
			} else if (mtmp->misc_worn_check & W_ARMC) {
			    /* destroy cloak; other possessions are safe */
			    otmpS = which_armor(mtmp, W_ARMC);
			    pline("%s %s is disintegrated!", s_suffix(Monnam(mtmp)), distant_name(otmpS, xname));
			    m_useup(mtmp, otmpS);
			    break;
			} else if (mtmp->misc_worn_check & W_ARM) {
			    /* destroy suit */
			    otmpS = which_armor(mtmp, W_ARM);
			    pline("%s %s is disintegrated!", s_suffix(Monnam(mtmp)), distant_name(otmpS, xname));
			    m_useup(mtmp, otmpS);
			    break;
			} else if (mtmp->misc_worn_check & W_ARMU) {
			    /* destroy shirt */
			    otmpS = which_armor(mtmp, W_ARMU);
			    pline("%s %s is disintegrated!", s_suffix(Monnam(mtmp)), distant_name(otmpS, xname));
			    m_useup(mtmp, otmpS);
			    break;
			}
		    else {

			struct obj *otmpX, *otmpX2, *m_amulet = mlifesaver(mtmp);

#define oresist_disintegration(obj) \
		(objects[obj->otyp].oc_oprop == DISINT_RES || \
		 obj_resists(obj, 5, 50) || is_quest_artifact(obj) || \
		 obj == m_amulet)

			pline("%s is disintegrated!", Monnam(mtmp) );

			for (otmpX = mtmp->minvent; otmpX; otmpX = otmpX2) {
			    otmpX2 = otmpX->nobj;
			    if (!oresist_disintegration(otmpX) && !stack_too_big(otmpX) ) {
				if (Has_contents(otmpX)) delete_contents(otmpX);
				obj_extract_self(otmpX);
				obfree(otmpX, (struct obj *)0);
			    }
			}

			xkilled(mtmp,2);

		    }

		}
		break;
	case WAN_STONING:
	case SPE_PETRIFY:
		if (!munstone(mtmp, FALSE))
		    minstapetrify(mtmp, FALSE);

		break;
	case WAN_TELEPORTATION:
	case SPE_TELEPORT_AWAY:
		reveal_invis = !u_teleport_mon(mtmp, TRUE);
		break;
	case WAN_BANISHMENT:
		if (u.uevent.udemigod && !u.freeplaymode) {
			reveal_invis = !u_teleport_mon(mtmp, TRUE);
			break;
		}
		reveal_invis = !u_teleport_monB(mtmp, TRUE);
		break;
	case WAN_MAKE_INVISIBLE:
	    {
		int oldinvis = mtmp->minvis;
		char nambuf[BUFSZ];

		/* format monster's name before altering its visibility */
		strcpy(nambuf, Monnam(mtmp));
		mon_set_minvis(mtmp);
		if (!oldinvis && knowninvisible(mtmp)) {
		    pline("%s turns transparent!", nambuf);
		    makeknown(otyp);
		}
		break;
	    }
	case WAN_MAKE_VISIBLE:
	    {
		int oldinvis = mtmp->minvis;
		char nambuf[BUFSZ];

		mtmp->perminvis = 0;
		mtmp->minvis = 0;
		if (mtmp->minvisreal) mtmp->perminvis = mtmp->minvis = 1;
		strcpy(nambuf, Monnam(mtmp));
		newsym(mtmp->mx, mtmp->my);		/* make it appear */
		if (oldinvis) {
		    pline("%s becomes visible!", nambuf);
		    makeknown(otyp);
		}
		break;
	    }

	case WAN_STUN_MONSTER:

		if(!resist(mtmp, otmp->oclass, 0, NOTELL))  {
			mtmp->mstun = TRUE;
			if (canseemon(mtmp)) {
				pline("%s trembles.",Monnam(mtmp));
			      makeknown(otyp);
			}
		}

		break;

	case SPE_STUN_MONSTER:

		if(!resist(mtmp, otmp->oclass, 0, NOTELL))  {
			mtmp->mstun = TRUE;
			if (canseemon(mtmp)) {
				pline("%s trembles.",Monnam(mtmp));
			}
		}

		break;

	case SPE_MAKE_VISIBLE:
	    {
		int oldinvis = mtmp->minvis;
		char nambuf[BUFSZ];

		mtmp->perminvis = 0;
		mtmp->minvis = 0;
		if (mtmp->minvisreal) mtmp->perminvis = mtmp->minvis = 1;
		strcpy(nambuf, Monnam(mtmp));
		newsym(mtmp->mx, mtmp->my);		/* make it appear */
		if (oldinvis) {
		    pline("%s becomes visible!", nambuf);
		}
		break;
	    }
	case WAN_NOTHING:
	case WAN_MISFIRE:
	case WAN_LOCKING:
	case SPE_WIZARD_LOCK:
		wake = FALSE;
		reveal_invis = TRUE;
		break;
	case SPE_WATER_BOLT:
		break;
	case SPE_HORRIFY:
	case WAN_FEAR:
		if (!is_undead(mtmp->data) && (!mtmp->egotype_undead) &&
		    !resist(mtmp, otmp->oclass, 0, NOTELL) &&
		    (!mtmp->mflee || mtmp->mfleetim)) {
		     if (canseemon(mtmp))
			 pline("%s suddenly panics!", Monnam(mtmp));
		     monflee(mtmp, rnd(10), FALSE, TRUE);
		}
		break;
	case WAN_SHARE_PAIN:	/*from Nethack TNG -- WAN_DRAINING */
		dmg = mtmp->mhpmax / 2;
		hit("wand",mtmp,exclam(dmg));
		resist(mtmp,otmp->oclass,dmg,TELL);
		/*[Sakusha] add and change message*/
		pline_The("damage reversed to you!");
		losehp(dmg,"excessive reverse damage",KILLED_BY);
		makeknown(otyp);
		break;

	case WAN_PROBING:
		reveal_invis = TRUE;
		wake = FALSE;
	case SPE_FINGER:
		probe_monster(mtmp);
		makeknown(otyp);
		break;
	case SPE_LOCK_MANIPULATION:
		if (!rn2(2)) {
			wake = FALSE;
			reveal_invis = TRUE;
			break;
		} /* else fall through */
	case WAN_OPENING:
	case SPE_KNOCK:
		wake = FALSE;	/* don't want immediate counterattack */
		if (u.uswallow && mtmp == u.ustuck) {
			if (is_animal(mtmp->data)) {
				if (Blind) You_feel("a sudden rush of air!");
				else pline("%s opens its mouth!", Monnam(mtmp));
			}
			expels(mtmp, mtmp->data, TRUE);
		} else if (!!(obj = which_armor(mtmp, W_SADDLE))) {
			mtmp->misc_worn_check &= ~obj->owornmask;
			update_mon_intrinsics(mtmp, obj, FALSE, FALSE);
			obj->owornmask = 0L;
			obj_extract_self(obj);
			place_object(obj, mtmp->mx, mtmp->my);
			/* call stackobj() if we ever drop anything that can merge */
			newsym(mtmp->mx, mtmp->my);
		}
		break;
	case WAN_HEALING:
	case WAN_EXTRA_HEALING:
	case WAN_FULL_HEALING:
	case SPE_HEALING:
	case SPE_EXTRA_HEALING:
	case SPE_FULL_HEALING:
		reveal_invis = TRUE;
	    if (mtmp->data != &mons[PM_PESTILENCE]) {
		wake = FALSE;		/* wakeup() makes the target angry */

		int healamount = 0;

		healamount +=
		  /* [ALI] FIXME: Makes no sense that cursed wands are more
		   * effective than uncursed wands. This behaviour dates
		   * right back to Slash v3 (and probably to v1).
		   */
		  (otyp == WAN_HEALING ? (d(5,2) + rnz(u.ulevel) + 5 * !!bcsign(otmp)) :
		  otyp == WAN_EXTRA_HEALING ? (d(5,4) + rnz(u.ulevel) + rnz(u.ulevel) + 10 * !!bcsign(otmp)) :
		  otyp == WAN_FULL_HEALING ? (d(5,8) + rnz(u.ulevel) + rnz(u.ulevel) + rnz(u.ulevel) + 20 * !!bcsign(otmp)) :
		  otyp == SPE_HEALING ? (rnd(10) + 4 + rnd(rnz(u.ulevel))): 
		  otyp == SPE_EXTRA_HEALING ? (rnd(20) + 6 + rnd(rnz(u.ulevel) + rnz(u.ulevel))) : 
		  (rnd(40) + 8 + rnd(rnz(u.ulevel) + rnz(u.ulevel) + rnz(u.ulevel))) ) ;

		mtmp->mhp += healamount;
		if (mtmp->bleedout && mtmp->bleedout <= healamount) {
			mtmp->bleedout = 0;
			if (canseemon(mtmp)) pline("%s's bleeding stops.", Monnam(mtmp));
		} else if (mtmp->bleedout) {
			mtmp->bleedout -= healamount;
			if (mtmp->bleedout < 0) mtmp->bleedout = 0; /* should never happen */
			if (canseemon(mtmp)) pline("%s's bleeding diminishes.", Monnam(mtmp));
		}

		if (mtmp->mhp > mtmp->mhpmax) {
		    if (otmp->oclass == WAND_CLASS)
			mtmp->mhpmax++;
		    mtmp->mhp = mtmp->mhpmax;
		}
		if (otmp->oclass == WAND_CLASS && !otmp->cursed)
		    mtmp->mcansee = 1;
		if (mtmp->mblinded) {
		    mtmp->mblinded = 0;
		    mtmp->mcansee = 1;
		}
		if (canseemon(mtmp)) {
		    if (disguised_mimic) {
			if (mtmp->m_ap_type == M_AP_OBJECT &&
			    mtmp->mappearance == STRANGE_OBJECT) {
			    /* it can do better now */
			    set_mimic_sym(mtmp);
			    newsym(mtmp->mx, mtmp->my);
			} else
			    mimic_hit_msg(mtmp, otyp);
		    } else pline("%s %s%s better.", Monnam(mtmp),
			  otmp->oclass == SPBOOK_CLASS ?
			    "looks" : "begins to look",
			  (otyp == SPE_EXTRA_HEALING ||
			   otyp == WAN_EXTRA_HEALING) ? " much" : "" );
		    makeknown(otyp);
		}
		/* Healing pets is a good thing ... */
		if (mtmp->mtame || mtmp->mpeaceful) {
		    adjalign(Role_if(PM_HEALER) ? 1 : sgn(u.ualign.type));
		}
	    } else if (otmp->oclass == SPBOOK_CLASS) {	/* Pestilence */
	        /* [ALI] FIXME: Makes no sense that Pestilence is not
		 * affected by wands of (extra) healing, but that raises
		 * whole new questions of what damage should be done.
		 * In vanilla NetHack, 3d{4,8} is half of the normal
		 * 6d{4,8} healing power of spells of (extra) healing.
		 */
		/* Pestilence will always resist; damage is half of 3d{4,8} */
		(void) resist(mtmp, otmp->oclass,
			      d(3, otyp == SPE_EXTRA_HEALING ? 8 : 4), TELL);
	    }
		break;
	case WAN_LIGHT:	/* (broken wand) */
		if (flash_hits_mon(mtmp, otmp)) {
		    makeknown(WAN_LIGHT);
		    reveal_invis = TRUE;
		}
		break;
	case WAN_SLEEP:	/* (broken wand) */
		/* [wakeup() doesn't rouse victims of temporary sleep,
		    so it's okay to leave `wake' set to TRUE here] */
		reveal_invis = TRUE;
		if (sleep_monst(mtmp, d(1 + otmp->spe, 12), WAND_CLASS))
		    slept_monst(mtmp);
		if (!Blind) makeknown(WAN_SLEEP);
		break;
	case SPE_STONE_TO_FLESH:
		if (monsndx(mtmp->data) == PM_STONE_GOLEM)
		    pm_index = PM_FLESH_GOLEM;
		else if (monsndx(mtmp->data) == PM_STATUE_GARGOYLE)
		    pm_index = PM_GARGOYLE;
		else
		    pm_index = NON_PM;
		if (pm_index != NON_PM) {
		    char *name = Monnam(mtmp);
		    /* turn into flesh equivalent */
		    if (newcham(mtmp, &mons[pm_index], FALSE, FALSE)) {
			if (canseemon(mtmp))
			    pline("%s turns to flesh!", name);
		    } else {
			if (canseemon(mtmp))
			    pline("%s looks rather fleshy for a moment.",
				  name);
		    }
		} else
		    wake = FALSE;
		break;
	case SPE_DRAIN_LIFE:
	case WAN_DRAINING:	/* KMH */
		dmg = rnd(8);
		if(dbldam) dmg *= 2;
		dmg += skilldmg;
		if (otyp ==	WAN_DRAINING) dmg *= 2;

		if (resists_drli(mtmp)) {
			shieldeff(mtmp->mx, mtmp->my);
			break;	/* skip makeknown */
		}else if (!resist(mtmp, otmp->oclass, dmg, NOTELL) &&
				mtmp->mhp > 0) {
			mtmp->mhp -= dmg;
			mtmp->mhpmax -= dmg;
			if (mtmp->mhp <= 0 || mtmp->mhpmax <= 0 || mtmp->m_lev < 1)
				xkilled(mtmp, 1);
			else {
				mtmp->m_lev--;
				if (canseemon(mtmp))
					pline("%s suddenly seems weaker!", Monnam(mtmp));
			}
		}
		makeknown(otyp);
		break;

	case WAN_DESLEXIFICATION:

		if (!is_vanillamonster(mtmp->data) ) {
			if (canseemon(mtmp))
					pline("%s is deslexified!", Monnam(mtmp));
			if (!rn2(3)) {
				cancelmonsterlite(mtmp);
			} else if (!rn2(2)) {
				mongone(mtmp);
			} else {
				struct obj *otmpXX, *otmpXX2;

				for (otmpXX = mtmp->minvent; otmpXX; otmpXX = otmpXX2) {
				    otmpXX2 = otmpXX->nobj;
				    if (Has_contents(otmpXX)) delete_contents(otmpXX);
				    obj_extract_self(otmpXX);
				    obfree(otmpXX, (struct obj *)0);
				}

				xkilled(mtmp,2);

			}

		}

		makeknown(otyp);
		break;

	case SPE_TIME:
	case WAN_TIME:
		dmg = rnd(8);
		if(dbldam) dmg *= 2;
		dmg += skilldmg;
		if (otyp ==	WAN_TIME) dmg *= 4;
		
		mtmp->mhp -= dmg;
		mtmp->mhpmax -= dmg;
		if (mtmp->mhp <= 0 || mtmp->mhpmax <= 0 || mtmp->m_lev < 1)
			xkilled(mtmp, 1);
		else {
			mtmp->m_lev--;
			if (canseemon(mtmp))
				pline("%s suddenly seems weaker!", Monnam(mtmp));
		}
		makeknown(otyp);
		break;
	case WAN_REDUCE_MAX_HITPOINTS:
		dmg = rnd(8);
		if (mtmp->mhp > 0) {
			mtmp->mhp -= dmg;
			mtmp->mhpmax -= dmg;
		}
		if (mtmp->mhp <= 0 || mtmp->mhpmax <= 0 || mtmp->m_lev < 1)
			xkilled(mtmp, 1);
		else {
			if (canseemon(mtmp))
				pline("%s suddenly seems weaker!", Monnam(mtmp));
		}
		break;
	case WAN_INCREASE_MAX_HITPOINTS:
		wake = FALSE;		/* wakeup() makes the target angry */
		dmg = rnd(8);
		if (mtmp->mhp > 0) {
			mtmp->mhp += dmg;
			mtmp->mhpmax += dmg;
		}
		if (canseemon(mtmp))
			pline("%s suddenly seems stronger!", Monnam(mtmp));
		break;
	case WAN_WIND: /* from Sporkhack */
	case SPE_WIND:
		/* Actually distance, not damage */
		dmg = rnd(2) + (bigmonst(mtmp->data) ? 0 : rnd(3));	
		hurtlex = sgn(mtmp->mx - u.ux);
		hurtley = sgn(mtmp->my - u.uy);
		if (hurtlex) { hurtley = rnd(3)-2; }
		else if (hurtley) { hurtlex = rnd(3)-2; }
		pline("%s is blown around!",Monnam(mtmp));
		mhurtle(mtmp,hurtlex,hurtley,dmg);
		makeknown(otyp);
		break;
	default:
		impossible("What an interesting effect (%d)", otyp);
		break;
	}

	if(wake) {
	    if(mtmp->mhp > 0) {
		wakeup(mtmp);
		m_respond(mtmp);
		if(mtmp->isshk && !*u.ushops) hot_pursuit(mtmp);
	    } else if(mtmp->m_ap_type)
		seemimic(mtmp); /* might unblock if mimicing a boulder/door */
	}
	/* note: bhitpos won't be set if swallowed, but that's okay since
	 * reveal_invis will be false.  We can't use mtmp->mx, my since it
	 * might be an invisible worm hit on the tail.
	 */
	if (reveal_invis) {
	    if (mtmp->mhp > 0 && cansee(bhitpos.x, bhitpos.y) &&
							!canspotmon(mtmp))
		map_invisible(bhitpos.x, bhitpos.y);
	}
	return 0;
}

void
probe_monster(mtmp)
struct monst *mtmp;
{
	struct obj *otmp;

	mstatusline(mtmp);
	if (notonhead) return;	/* don't show minvent for long worm tail */

#ifndef GOLDOBJ
	if (mtmp->minvent || mtmp->mgold) {
#else
	if (mtmp->minvent) {
#endif
	    for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
		otmp->dknown = 1;	/* treat as "seen" */
	    (void) display_minventory(mtmp, MINV_ALL, (char *)0);
	} else {
	    pline("%s is not carrying anything.", noit_Monnam(mtmp));
	}
}

#endif /*OVLB*/
#ifdef OVL1

/*
 * Return the object's physical location.  This only makes sense for
 * objects that are currently on the level (i.e. migrating objects
 * are nowhere).  By default, only things that can be seen (in hero's
 * inventory, monster's inventory, or on the ground) are reported.
 * By adding BURIED_TOO and/or CONTAINED_TOO flags, you can also get
 * the location of buried and contained objects.  Note that if an
 * object is carried by a monster, its reported position may change
 * from turn to turn.  This function returns FALSE if the position
 * is not available or subject to the constraints above.
 */
boolean
get_obj_location(obj, xp, yp, locflags)
struct obj *obj;
xchar *xp, *yp;
int locflags;
{
	switch (obj->where) {
	    case OBJ_INVENT:
		*xp = u.ux;
		*yp = u.uy;
		return TRUE;
	    case OBJ_FLOOR:
		*xp = obj->ox;
		*yp = obj->oy;
		return TRUE;
	    case OBJ_MINVENT:
		if (obj->ocarry->mx) {
		    *xp = obj->ocarry->mx;
		    *yp = obj->ocarry->my;
		    return TRUE;
		}
		break;	/* !mx => migrating monster */
	    case OBJ_BURIED:
		if (locflags & BURIED_TOO) {
		    *xp = obj->ox;
		    *yp = obj->oy;
		    return TRUE;
		}
		break;
	    case OBJ_CONTAINED:
		if (locflags & CONTAINED_TOO)
		    return get_obj_location(obj->ocontainer, xp, yp, locflags);
		break;
	}
	*xp = *yp = 0;
	return FALSE;
}

boolean
get_mon_location(mon, xp, yp, locflags)
struct monst *mon;
xchar *xp, *yp;
int locflags;	/* non-zero means get location even if monster is buried */
{
	/* somehow "mon" isn't always defined; I don't know exactly how this is caused but it segfaults the game, so...
	 * need a failsafe that makes nothing important happen if the monster isn't there --Amy */
	if (!mon) {
	    impossible("get_mon_location called with nonexistant monster");
	    *xp = *yp = 0;
	    return FALSE;
	} else if (mon == &youmonst) {
	    *xp = u.ux;
	    *yp = u.uy;
	    return TRUE;
	} else if (mon->mx > 0 && (!mon->mburied || locflags)) {
	    *xp = mon->mx;
	    *yp = mon->my;
	    return TRUE;
	} else {	/* migrating or buried */
	    *xp = *yp = 0;
	    return FALSE;
	}
}

/* used by revive() and animate_statue() */
struct monst *
montraits(obj,cc)
struct obj *obj;
coord *cc;
{
	struct monst *mtmp = (struct monst *)0;
	struct monst *mtmp2 = (struct monst *)0;

	if (obj->oxlth && (obj->oattached == OATTACHED_MONST))
		mtmp2 = get_mtraits(obj, TRUE);
	if (mtmp2) {
		/* save_mtraits() validated mtmp2->mnum */
		mtmp2->data = &mons[mtmp2->mnum];
		if (mtmp2->mhpmax <= 0 && !is_rider(mtmp2->data) && !is_deadlysin(mtmp2->data))
			return (struct monst *)0;
		mtmp = makemon(mtmp2->data,
				cc->x, cc->y, NO_MINVENT|MM_NOWAIT|MM_NOCOUNTBIRTH);
		if (!mtmp) return mtmp;

		/* heal the monster */
		/*if (mtmp->mhpmax > mtmp2->mhpmax && is_rider(mtmp2->data))
			mtmp2->mhpmax = mtmp->mhpmax;
		if (mtmp->mhpmax > mtmp2->mhpmax && is_deadlysin(mtmp2->data))
			mtmp2->mhpmax = mtmp->mhpmax;*/
		/* this interacted incorrectly with the makemon.c HP boost code... let's try something different: */
		if (is_rider(mtmp2->data) || is_deadlysin(mtmp2->data)) {
			if (mtmp2->mhpmax < 60) mtmp2->mhpmax = 60;
		}

		mtmp2->mhp = mtmp2->mhpmax;
		/* Get these ones from mtmp */
		mtmp2->minvent = mtmp->minvent; /*redundant*/
		/* monster ID is available if the monster died in the current
		   game, but should be zero if the corpse was in a bones level
		   (we cleared it when loading bones) */
		if (!mtmp2->m_id)
		    mtmp2->m_id = mtmp->m_id;
		mtmp2->mx   = mtmp->mx;
		mtmp2->my   = mtmp->my;
		mtmp2->mux  = mtmp->mux;
		mtmp2->muy  = mtmp->muy;
		mtmp2->mw   = mtmp->mw;
		mtmp2->wormno = mtmp->wormno;
		mtmp2->misc_worn_check = mtmp->misc_worn_check;
		mtmp2->weapon_check = mtmp->weapon_check;
		/*mtmp2->mtrapseen = mtmp->mtrapseen;*/
		mtmp2->mflee = mtmp->mflee;
		mtmp2->mburied = mtmp->mburied;
		mtmp2->mundetected = mtmp->mundetected;
		mtmp2->mfleetim = mtmp->mfleetim;
		mtmp2->mlstmv = mtmp->mlstmv;
		mtmp2->m_ap_type = mtmp->m_ap_type;
		mtmp2->minvis = obj->oinvis;
		mtmp2->minvisreal = obj->oinvisreal;
		/* set these ones explicitly */
		mtmp2->mavenge = 0;
		mtmp2->meating = 0;
		mtmp2->mleashed = 0;
		mtmp2->mtrapped = 0;
		mtmp2->msleeping = 0;
		mtmp2->mfrozen = 0;
		mtmp2->mcanmove = 1;
		mtmp2->masleep = 0;
		/* most cancelled monsters return to normal,
		   but some need to stay cancelled */
		/* Gypsies use mcan to enforce the rule that they only
		   ever grant one wish. */
		/* Amy edit: various nerfs made it so that this is no longer needed :D */
		mtmp2->mcan = 0;
		mtmp2->mcansee = 1;	/* set like in makemon */
		mtmp2->mblinded = 0;
		mtmp2->mstun = 0;
		mtmp2->mconf = 0;
		replmon(mtmp,mtmp2);
		newsym(mtmp2->mx, mtmp2->my);	/* Might now be invisible */
	}
	return mtmp2;
}

/*
 * get_container_location() returns the following information
 * about the outermost container:
 * loc argument gets set to: 
 *	OBJ_INVENT	if in hero's inventory; return 0.
 *	OBJ_FLOOR	if on the floor; return 0.
 *	OBJ_BURIED	if buried; return 0.
 *	OBJ_MINVENT	if in monster's inventory; return monster.
 * container_nesting is updated with the nesting depth of the containers
 * if applicable.
 */
struct monst *
get_container_location(obj, loc, container_nesting)
struct obj *obj;
int *loc;
int *container_nesting;
{
	if (!obj || !loc)
		return 0;

	if (container_nesting) *container_nesting = 0;
	while (obj && obj->where == OBJ_CONTAINED) {
		if (container_nesting) *container_nesting += 1;
		obj = obj->ocontainer;
	}
	if (obj) {
	    *loc = obj->where;	/* outermost container's location */
	    if (obj->where == OBJ_MINVENT) return obj->ocarry;
	}
	return (struct monst *)0;
}

/*
 * Attempt to revive the given corpse, return the revived monster if
 * successful.  Note: this does NOT use up the corpse if it fails.
 */
struct monst *
revive(obj)
register struct obj *obj;
{
	register struct monst *mtmp = (struct monst *)0;
	struct obj *container = (struct obj *)0;
	int container_nesting = 0;
	schar savetame = 0;
	boolean recorporealization = FALSE;
	boolean in_container = FALSE;
	if(obj->otyp == CORPSE) {
		int montype = obj->corpsenm;
		xchar x, y;

		if (montype == PM_UNFORTUNATE_VICTIM) { /* very bad! */

			pline("You get a strong feeling that the gods don't like your actions...");
			change_luck(-5);
			u.ualign.sins += 10; 
			u.alignlim -= 10;
			adjalign(-50);
			u.ugangr++; u.ugangr++; u.ugangr++;
			prayer_done();


		}

		if (obj->where == OBJ_CONTAINED) {
			/* deal with corpses in [possibly nested] containers */
			struct monst *carrier;
			int holder = 0;

			container = obj->ocontainer;
			carrier = get_container_location(container, &holder,
							&container_nesting);
			switch(holder) {
			    case OBJ_MINVENT:
				x = carrier->mx; y = carrier->my;
				in_container = TRUE;
				break;
			    case OBJ_INVENT:
				x = u.ux; y = u.uy;
				in_container = TRUE;
				break;
			    case OBJ_FLOOR:
				if (!get_obj_location(obj, &x, &y, CONTAINED_TOO))
					return (struct monst *) 0;
				in_container = TRUE;
				break;
			    default:
			    	return (struct monst *)0;
			}
		} else {
			/* only for invent, minvent, or floor */
			if (!get_obj_location(obj, &x, &y, 0))
			    return (struct monst *) 0;
		}
		if (in_container) {
			/* Rules for revival from containers:
			   - the container cannot be locked
			   - the container cannot be heavily nested (>2 is arbitrary)
			   - the container cannot be a statue or bag of holding
			     (except in very rare cases for the latter)
			*/
			if (!x || !y || container->olocked || container_nesting > 2 ||
			    container->otyp == STATUE ||
			    ( (container->otyp == BAG_OF_HOLDING || container->otyp == ICE_BOX_OF_HOLDING || container->otyp == CHEST_OF_HOLDING) && rn2(40)))
				return (struct monst *)0;
		}

		if (MON_AT(x,y)) {
		    coord new_xy;

		    if (enexto(&new_xy, x, y, &mons[montype]))
			x = new_xy.x,  y = new_xy.y;
		}

		if(cant_create(&montype, TRUE)) {
			/* make a zombie or worm instead */
			mtmp = makemon(&mons[montype], x, y,
				       NO_MINVENT|MM_NOWAIT);
			if (mtmp) {
				mtmp->mhp = mtmp->mhpmax = 100;
				mon_adjust_speed(mtmp, 2, (struct obj *)0); /* MFAST */
			}
		} else {
		    if (obj->oxlth && (obj->oattached == OATTACHED_MONST)) {
			    coord xy;
			    xy.x = x; xy.y = y;
		    	    mtmp = montraits(obj, &xy);
		    	    if (mtmp && mtmp->mtame && !mtmp->isminion)
				wary_dog(mtmp, TRUE);
		    } else
 		            mtmp = makemon(&mons[montype], x, y,
				       NO_MINVENT|MM_NOWAIT|MM_NOCOUNTBIRTH);
		    if (mtmp) {
			if (obj->oxlth && (obj->oattached == OATTACHED_M_ID)) {
			    unsigned m_id;
			    struct monst *ghost;
			    (void) memcpy((void *)&m_id,
				    (void *)obj->oextra, sizeof(m_id));
			    ghost = find_mid(m_id, FM_FMON);
		    	    if (ghost && ghost->data == &mons[PM_GHOST]) {
		    		    int x2, y2;
		    		    x2 = ghost->mx; y2 = ghost->my;
		    		    if (ghost->mtame)
		    		    	savetame = ghost->mtame;
		    		    if (canseemon(ghost))
		    		  	pline("%s is suddenly drawn into its former body!",
						Monnam(ghost));
				    mondead(ghost);
				    recorporealization = TRUE;
				    newsym(x2, y2);
			    }
			    /* don't mess with obj->oxlth here */
			    obj->oattached = OATTACHED_NOTHING;
			}
			/* Monster retains its name */
			if (obj->onamelth)
			    mtmp = christen_monst(mtmp, ONAME(obj));
			/* flag the quest leader as alive. */
			if (mtmp->data->msound == MS_LEADER || mtmp->m_id ==
				quest_status.leader_m_id)
			    quest_status.leader_is_dead = FALSE;
		    }
		}
		if (mtmp) {
			if (obj->oeaten)
				mtmp->mhp = eaten_stat(mtmp->mhp, obj);
			/* track that this monster was revived at least once */
			mtmp->mrevived = 1;

			if (recorporealization) {
				/* If mtmp is revivification of former tame ghost*/
				if (savetame) {
				    struct monst *mtmp2 = tamedog(mtmp, (struct obj *)0, FALSE);
				    if (mtmp2) {
					mtmp2->mtame = savetame;
					mtmp = mtmp2;
				    }
				}
				/* was ghost, now alive, it's all very confusing */
				mtmp->mconf = 1;
			}

			switch (obj->where) {
			    case OBJ_INVENT:
				useup(obj);
				break;
			    case OBJ_FLOOR:
				/* in case MON_AT+enexto for invisible mon */
				x = obj->ox,  y = obj->oy;
				/* not useupf(), which charges */
				if (obj->quan > 1L)
				    obj = splitobj(obj, 1);
				delobj(obj);
				newsym(x, y);
				break;
			    case OBJ_MINVENT:
				m_useup(obj->ocarry, obj);
				break;
			    case OBJ_CONTAINED:
				obj_extract_self(obj);
				obfree(obj, (struct obj *) 0);
				break;
			    default:
				panic("revive");
			}
		}
	}
	return mtmp;
}

STATIC_OVL void
revive_egg(obj)
struct obj *obj;
{
	/*
	 * Note: generic eggs with corpsenm set to NON_PM will never hatch.
	 */
	if (obj->otyp != EGG) return;
	if (obj->corpsenm != NON_PM && !dead_species(obj->corpsenm, TRUE))
	    attach_egg_hatch_timeout(obj);
}

/* try to revive all corpses and eggs carried by `mon' */
int
unturn_dead(mon)
struct monst *mon;
{
	struct obj *otmp, *otmp2;
	struct monst *mtmp2;
	char owner[BUFSZ], corpse[BUFSZ];
	boolean youseeit;
	int once = 0, res = 0;
	int x, y;

	youseeit = (mon == &youmonst) ? TRUE : canseemon(mon);
	otmp2 = (mon == &youmonst) ? invent : mon->minvent;

	while ((otmp = otmp2) != 0) {
	    otmp2 = otmp->nobj;
	    if (otmp->otyp == EGG)
		revive_egg(otmp);
	    if (otmp->otyp != CORPSE) continue;
	    /* save the name; the object is liable to go away */
	    if (youseeit) strcpy(corpse, corpse_xname(otmp, TRUE));

	    /* for a merged group, only one is revived; should this be fixed? */

	    /* Amy edit: farming via undead turning is lame, give a chance to vaporize the corpse instead */
	    if (otmp->otyp == CORPSE && !rn2(10)) {

			switch (otmp->where) {
			    case OBJ_INVENT:
				useup(otmp);
				break;
			    case OBJ_FLOOR:
				/* in case MON_AT+enexto for invisible mon */
				x = otmp->ox,  y = otmp->oy;
				/* not useupf(), which charges */
				if (otmp->quan > 1L)
				    otmp = splitobj(otmp, 1);
				delobj(otmp);
				newsym(x, y);
				break;
			    case OBJ_MINVENT:
				m_useup(otmp->ocarry, otmp);
				break;
			    case OBJ_CONTAINED:
				obj_extract_self(otmp);
				obfree(otmp, (struct obj *) 0);
				break;
			    default:
				panic("unturn_dead corpse in weird place!");
			}

	    }

	    if ((mtmp2 = revive(otmp)) != 0) {
		++res;
		if (youseeit) {
		    if (!once++) strcpy(owner,
					(mon == &youmonst) ? "Your" :
					s_suffix(Monnam(mon)));
		    pline("%s %s suddenly comes alive!", owner, corpse);
		} else if (canseemon(mtmp2))
		    pline("%s suddenly appears!", Amonnam(mtmp2));
	    }
	}
	return res;
}
#endif /*OVL1*/

#ifdef OVLB
static const char charged_objs[] = { WAND_CLASS, WEAPON_CLASS, ARMOR_CLASS,
				     SPBOOK_CLASS, 0 };

STATIC_OVL void
costly_cancel(obj)
register struct obj *obj;
{
	char objroom;
	struct monst *shkp = (struct monst *)0;

	if (obj->no_charge) return;

	switch (obj->where) {
	case OBJ_INVENT:
		if (obj->unpaid) {
		    shkp = shop_keeper(*u.ushops);
		    if (!shkp) return;
		    Norep("You cancel an unpaid object, you pay for it!");
		    bill_dummy_object(obj);
		}
		break;
	case OBJ_FLOOR:
		objroom = *in_rooms(obj->ox, obj->oy, SHOPBASE);
		shkp = shop_keeper(objroom);
		if (!shkp || !inhishop(shkp)) return;
		if (costly_spot(u.ux, u.uy) && objroom == *u.ushops) {
		    Norep("You cancel it, you pay for it!");
		    bill_dummy_object(obj);
		} else
		    (void) stolen_value(obj, obj->ox, obj->oy, FALSE, FALSE,
			    FALSE);
		break;
	}
}


/* cancel obj, possibly carried by you or a monster */
void
cancel_item(obj,weakeffect)
register struct obj *obj;
boolean weakeffect;
{
	boolean	u_ring = (obj == uleft) || (obj == uright);
	register boolean holy = (obj->otyp == POT_WATER && obj->blessed);

	int chancetoresist = 0;
	boolean willresist = FALSE;
	if (weakeffect && obj->spe < -1) chancetoresist = (obj->spe) * -1;
	if (chancetoresist && rn2(chancetoresist)) willresist = TRUE;

	if (obj->finalcancel) return;

	if (stack_too_big(obj)) return;

	if (obj->enchantment) {
		/* nasty hack - items losing their enchantment shouldn't keep giving their effects --Amy */
		if (obj->owornmask) setnotworn(obj);
		obj->enchantment = 0;
	}

	switch(obj->otyp) {
		case RIN_GAIN_STRENGTH:
			if ((obj->owornmask & W_RING) && u_ring) {
				if (obj->spe < 0 && !willresist) ABON(A_STR) += 1;
				else if (!willresist) ABON(A_STR) -= obj->spe;
				flags.botl = 1;
			}
			break;
		case RIN_GAIN_DEXTERITY:
			if ((obj->owornmask & W_RING) && u_ring) {
				if (obj->spe < 0 && !willresist) ABON(A_DEX) += 1;
				else if (!willresist) ABON(A_DEX) -= obj->spe;
				flags.botl = 1;
			}
			break;
		case RIN_GAIN_CONSTITUTION:
			if ((obj->owornmask & W_RING) && u_ring) {
				if (obj->spe < 0 && !willresist) ABON(A_CON) += 1;
				else if (!willresist) ABON(A_CON) -= obj->spe;
				flags.botl = 1;
			}
			break;
		case RIN_GAIN_INTELLIGENCE:
			if ((obj->owornmask & W_RING) && u_ring) {
				if (obj->spe < 0 && !willresist) ABON(A_INT) += 1;
				else if (!willresist) ABON(A_INT) -= obj->spe;
				flags.botl = 1;
			}
			break;
		case RIN_GAIN_WISDOM:
			if ((obj->owornmask & W_RING) && u_ring) {
				if (obj->spe < 0 && !willresist) ABON(A_WIS) += 1;
				else if (!willresist) ABON(A_WIS) -= obj->spe;
				flags.botl = 1;
			}
			break;
		case RIN_ADORNMENT:
			if ((obj->owornmask & W_RING) && u_ring) {
				if (obj->spe < 0 && !willresist) ABON(A_CHA) += 1;
				else if (!willresist) ABON(A_CHA) -= obj->spe;
				flags.botl = 1;
			}
			break;
		case RIN_INCREASE_ACCURACY:
			if ((obj->owornmask & W_RING) && u_ring) {
				if (obj->spe < 0 && !willresist) u.uhitinc += 1;
				else if (!willresist) u.uhitinc -= obj->spe;
			}
			break;
		case RIN_INCREASE_DAMAGE:
			if ((obj->owornmask & W_RING) && u_ring) {
				if (obj->spe < 0 && !willresist) u.udaminc += 1;
				else if (!willresist) u.udaminc -= obj->spe;
			}
			break;
		case RIN_HEAVY_ATTACK:
			if ((obj->owornmask & W_RING) && u_ring) {
				if (obj->spe < 0 && !willresist) {
					u.udaminc += 1;
					u.uhitinc += 1;
				}
				else if (!willresist) {
					u.udaminc -= obj->spe;
					u.uhitinc -= obj->spe;
				}
			}
			break;
		/* case RIN_PROTECTION:  not needed */
	}
	if (objects[obj->otyp].oc_magic
	    || (obj->spe && (obj->oclass == ARMOR_CLASS || obj->oclass == GEM_CLASS || obj->oclass == CHAIN_CLASS || obj->oclass == BALL_CLASS ||
			     obj->oclass == WEAPON_CLASS || obj->oclass == TOOL_CLASS || is_weptool(obj)))
	    || obj->otyp == POT_ACID || obj->otyp == POT_SICKNESS || obj->otyp == POT_POISON) {
	    if (obj->spe != ((obj->oclass == WAND_CLASS) ? -1 : 0) &&
	       obj->otyp != WAN_CANCELLATION &&
		 /* can't cancel cancellation */
		 obj->otyp != MAGIC_LAMP &&
		 obj->otyp != MAGIC_CANDLE &&
		 obj->otyp != CANDELABRUM_OF_INVOCATION) {
		costly_cancel(obj);
		if (!willresist) {
			if (obj->spe < 0 && !(obj->oclass == WAND_CLASS) ) obj->spe++;
			else obj->spe = (obj->oclass == WAND_CLASS) ? -1 : 0;
		}
	    }
	    switch (obj->oclass) {
	      case SCROLL_CLASS:
		if (obj->otyp == SCR_HEALING || obj->otyp == SCR_EXTRA_HEALING ||
#ifdef MAIL
obj->otyp == SCR_MAIL || 
#endif
obj->otyp == SCR_CURE || obj->otyp == SCR_MANA || obj->otyp == SCR_GREATER_MANA_RESTORATION || obj->otyp == SCR_STANDARD_ID || obj->otyp == SCR_PHASE_DOOR) break;
		costly_cancel(obj);
		obj->otyp = SCR_BLANK_PAPER;
		obj->spe = 0;
		break;
	      case SPBOOK_CLASS:
		if (obj->otyp != SPE_CANCELLATION &&
			obj->otyp != SPE_BOOK_OF_THE_DEAD) {
		    costly_cancel(obj);
		    obj->otyp = SPE_BLANK_PAPER;
		}
		break;
	      case POTION_CLASS:
		/* Potions of amnesia are uncancelable. */
		if (obj->otyp == POT_AMNESIA) break;

		costly_cancel(obj);
		if (obj->otyp == POT_SICKNESS || obj->otyp == POT_POISON ||
		    obj->otyp == POT_SEE_INVISIBLE) {
	    /* sickness is "biologically contaminated" fruit juice; cancel it
	     * and it just becomes fruit juice... whereas see invisible
	     * tastes like "enchanted" fruit juice, it similarly cancels.
	     */
		    obj->otyp = POT_FRUIT_JUICE;
		} else {
	            obj->otyp = POT_WATER;
		    obj->odiluted = 0; /* same as any other water */
		}
		break;
	    }
	}
	if (holy) costly_cancel(obj);
	unbless(obj);
	if (obj->oclass == WAND_CLASS || obj->spe >= 0) uncurse(obj, FALSE);
	if (obj->oinvis) obj->oinvis = 0;
	if (obj->oinvisreal) obj->oinvisreal = 0;

	if (weakeffect && !rn2(100)) obj->finalcancel = TRUE;

	return;
}

/* Remove a positive enchantment or charge from obj,
 * possibly carried by you or a monster
 */
boolean
drain_item(obj)
register struct obj *obj;
{
	boolean u_ring;

	/* Is this a charged/enchanted object? */
	if (!obj || (!objects[obj->otyp].oc_charged &&
			obj->oclass != WEAPON_CLASS &&
			obj->oclass != BALL_CLASS &&
			obj->oclass != CHAIN_CLASS &&
			obj->oclass != GEM_CLASS &&
			obj->oclass != ARMOR_CLASS && !is_weptool(obj)) ||
			obj->spe <= 0)
	    return (FALSE);
	if (obj_resists(obj, 10, 90))
	    return (FALSE);

	if (uarmf && (uarmf->oartifact == ART_ANTI_DISENCHANTER) && rn2(4) )
	    return (FALSE);

	if (stack_too_big(obj)) return (FALSE);

	/* Charge for the cost of the object */
	costly_cancel(obj);	/* The term "cancel" is okay for now */

	/* Drain the object and any implied effects */
	obj->spe--;
	u_ring = (obj == uleft) || (obj == uright);
	switch(obj->otyp) {
	case RIN_GAIN_STRENGTH:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	ABON(A_STR)--;
	    	flags.botl = 1;
	    }
	    break;
	case RIN_GAIN_CONSTITUTION:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	ABON(A_CON)--;
	    	flags.botl = 1;
	    }
	    break;
	case RIN_ADORNMENT:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	ABON(A_CHA)--;
	    	flags.botl = 1;
	    }
	    break;
	case RIN_INCREASE_ACCURACY:
	    if ((obj->owornmask & W_RING) && u_ring)
	    	u.uhitinc--;
	    break;
	case RIN_INCREASE_DAMAGE:
	    if ((obj->owornmask & W_RING) && u_ring)
	    	u.udaminc--;
	    break;
	case RIN_HEAVY_ATTACK:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	u.udaminc--;
	    	u.uhitinc--;
	    }
	    break;
	case RIN_PROTECTION:
	    flags.botl = 1;
	    break;
	}
	if (carried(obj)) update_inventory();
	return (TRUE);
}

boolean
drain_item_reverse(obj)
register struct obj *obj;
{
	boolean u_ring;
	int save_spe;

	/* Is this a charged/enchanted object? */
	if (!obj || (!objects[obj->otyp].oc_charged &&
			obj->oclass != WEAPON_CLASS &&
			obj->oclass != BALL_CLASS &&
			obj->oclass != CHAIN_CLASS &&
			obj->oclass != GEM_CLASS &&
			obj->oclass != ARMOR_CLASS && !is_weptool(obj)) ||
			obj->spe <= 0)
	    return (FALSE);
	if (obj_resists(obj, 10, 90))
	    return (FALSE);

	if (stack_too_big(obj)) return (FALSE);

	/* Charge for the cost of the object */
	costly_cancel(obj);	/* The term "cancel" is okay for now */

	/* Drain the object and any implied effects */
	save_spe = obj->spe;
	obj->spe = -(obj->spe);
	u_ring = (obj == uleft) || (obj == uright);
	switch(obj->otyp) {
	case RIN_GAIN_STRENGTH:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	ABON(A_STR) -= (save_spe * 2);
	    	flags.botl = 1;
	    }
	    break;
	case RIN_GAIN_CONSTITUTION:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	ABON(A_CON) -= (save_spe * 2);
	    	flags.botl = 1;
	    }
	    break;
	case RIN_ADORNMENT:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	ABON(A_CHA) -= (save_spe * 2);
	    	flags.botl = 1;
	    }
	    break;
	case RIN_INCREASE_ACCURACY:
	    if ((obj->owornmask & W_RING) && u_ring)
	    	u.uhitinc -= (save_spe * 2);
	    break;
	case RIN_INCREASE_DAMAGE:
	    if ((obj->owornmask & W_RING) && u_ring)
	    	u.udaminc -= (save_spe * 2);
	    break;
	case RIN_HEAVY_ATTACK:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	u.udaminc -= (save_spe * 2);
	    	u.uhitinc -= (save_spe * 2);
	    }
	    break;
	case RIN_PROTECTION:
	    flags.botl = 1;
	    break;
	}
	if (carried(obj)) update_inventory();
	return (TRUE);
}

/* AD_NGEN function: drain an item up to -20, and curse it if it gets negative --Amy */
boolean
drain_item_severely(obj)
register struct obj *obj;
{
	boolean u_ring;

	/* Is this a charged/enchanted object? */
	if (!obj || (!objects[obj->otyp].oc_charged &&
			obj->oclass != WEAPON_CLASS &&
			obj->oclass != BALL_CLASS &&
			obj->oclass != CHAIN_CLASS &&
			obj->oclass != GEM_CLASS &&
			obj->oclass != ARMOR_CLASS && !is_weptool(obj)) ||
			obj->spe <= -20)
	    return (FALSE);
	if (obj_resists(obj, 10, 90))
	    return (FALSE);

	if (uarmf && (uarmf->oartifact == ART_ANTI_DISENCHANTER) && rn2(4) )
	    return (FALSE);

	if (stack_too_big(obj)) return (FALSE);

	/* Charge for the cost of the object */
	costly_cancel(obj);	/* The term "cancel" is okay for now */

	/* Drain the object and any implied effects */
	obj->spe--;
	if (obj->spe < 0) curse(obj);
	u_ring = (obj == uleft) || (obj == uright);
	switch(obj->otyp) {
	case RIN_GAIN_STRENGTH:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	ABON(A_STR)--;
	    	flags.botl = 1;
	    }
	    break;
	case RIN_GAIN_CONSTITUTION:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	ABON(A_CON)--;
	    	flags.botl = 1;
	    }
	    break;
	case RIN_ADORNMENT:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	ABON(A_CHA)--;
	    	flags.botl = 1;
	    }
	    break;
	case RIN_INCREASE_ACCURACY:
	    if ((obj->owornmask & W_RING) && u_ring)
	    	u.uhitinc--;
	    break;
	case RIN_INCREASE_DAMAGE:
	    if ((obj->owornmask & W_RING) && u_ring)
	    	u.udaminc--;
	    break;
	case RIN_HEAVY_ATTACK:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	u.udaminc--;
	    	u.uhitinc--;
	    }
	    break;
	case RIN_PROTECTION:
	    flags.botl = 1;
	    break;
	}
	if (carried(obj)) update_inventory();
	return (TRUE);
}

boolean
drain_item_negative(obj)
register struct obj *obj;
{
	boolean u_ring;

	/* Is this a charged/enchanted object? */
	if (!obj || (!objects[obj->otyp].oc_charged &&
			obj->oclass != WEAPON_CLASS &&
			obj->oclass != BALL_CLASS &&
			obj->oclass != CHAIN_CLASS &&
			obj->oclass != GEM_CLASS &&
			obj->oclass != ARMOR_CLASS && !is_weptool(obj)) ||
			obj->spe >= 0)
	    return (FALSE);
	if (obj_resists(obj, 10, 90))
	    return (FALSE);

	if (stack_too_big(obj)) return (FALSE);

	/* Drain the object and any implied effects */
	obj->spe++;
	u_ring = (obj == uleft) || (obj == uright);
	switch(obj->otyp) {
	case RIN_GAIN_STRENGTH:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	ABON(A_STR)++;
	    	flags.botl = 1;
	    }
	    break;
	case RIN_GAIN_CONSTITUTION:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	ABON(A_CON)++;
	    	flags.botl = 1;
	    }
	    break;
	case RIN_ADORNMENT:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	ABON(A_CHA)++;
	    	flags.botl = 1;
	    }
	    break;
	case RIN_INCREASE_ACCURACY:
	    if ((obj->owornmask & W_RING) && u_ring)
	    	u.uhitinc++;
	    break;
	case RIN_INCREASE_DAMAGE:
	    if ((obj->owornmask & W_RING) && u_ring)
	    	u.udaminc++;
	    break;
	case RIN_HEAVY_ATTACK:
	    if ((obj->owornmask & W_RING) && u_ring) {
	    	u.udaminc++;
	    	u.uhitinc++;
	    }
	    break;
	case RIN_PROTECTION:
	    flags.botl = 1;
	    break;
	}
	if (carried(obj)) update_inventory();
	return (TRUE);
}

#endif /*OVLB*/
#ifdef OVL0

boolean
obj_resists(obj, ochance, achance)
struct obj *obj;
int ochance, achance;	/* percent chance for ordinary objects, artifacts */
{
	/* [ALI] obj_resists(obj, 0, 0) is used to test for objects resisting
	 * containment (see bury_an_obj() and monstone() for details).
	 */
	if (evades_destruction(obj) && (ochance || achance))
		return TRUE;

	if (hard_to_destruct(obj) && (ochance || achance) && rn2(10)) return TRUE;

	if (obj->otyp == AMULET_OF_YENDOR ||
	    obj->otyp == SPE_BOOK_OF_THE_DEAD ||
	    obj->otyp == CANDELABRUM_OF_INVOCATION ||
	    obj->otyp == BELL_OF_OPENING ||
	    (obj->otyp == CORPSE && is_rider(&mons[obj->corpsenm])) || (obj->otyp == CORPSE && is_deadlysin(&mons[obj->corpsenm]))) {
		return TRUE;
	} else {
		int chance = rn2(100);

		return((boolean)(chance < (obj->oartifact ? achance : ochance)));
	}
}

boolean
obj_shudders(obj)
struct obj *obj;
{
	int	zap_odds;

	if (obj->oclass == WAND_CLASS)
		zap_odds = 3;	/* half-life = 2 zaps */
	else if (obj->cursed)
		zap_odds = 3;	/* half-life = 2 zaps */
	else if (obj->blessed)
		zap_odds = 7;	/* half-life = 8 zaps */
	else
		zap_odds = 5;	/* half-life = 6 zaps */

	/* adjust for "large" quantities of identical things */
	if(obj->quan > 4L) zap_odds /= 2;

	/* Amy edit: polypiling is so OP that it desperately needs nerfing :P */
	if (!rn2(3)) return TRUE;
	if ((obj->oclass == SCROLL_CLASS || obj->oclass == POTION_CLASS) && rn2(2) ) return TRUE;
	if ((obj->oclass == SPBOOK_CLASS) && rn2(5) ) return TRUE;
	if ((obj->oclass == WEAPON_CLASS) && objects[obj->otyp].oc_merge && rn2(10) ) return TRUE;

	return((boolean)(! rn2(zap_odds)));
}
#endif /*OVL0*/
#ifdef OVLB

/* Use up at least minwt number of things made of material mat.
 * There's also a chance that other stuff will be used up.  Finally,
 * there's a random factor here to keep from always using the stuff
 * at the top of the pile.
 */
STATIC_OVL void
polyuse(objhdr, mat, minwt)
    struct obj *objhdr;
    int mat, minwt;
{
    register struct obj *otmp, *otmp2;

    for(otmp = objhdr; minwt > 0 && otmp; otmp = otmp2) {
	otmp2 = otmp->nexthere;
	if (otmp == uball || otmp == uchain) continue;
	if (obj_resists(otmp, 0, 0)) continue;	/* preserve unique objects */
	if (evades_destruction(otmp)) continue;
#ifdef MAIL
	if (otmp->otyp == SCR_MAIL) continue;
#endif
	if (otmp->otyp == SCR_HEALING || otmp->otyp == SCR_EXTRA_HEALING || otmp->otyp == SCR_STANDARD_ID || otmp->otyp == SCR_MANA || otmp->otyp == SCR_GREATER_MANA_RESTORATION || otmp->otyp == SCR_CURE || otmp->otyp == SCR_PHASE_DOOR) continue;

	if (((int) objects[otmp->otyp].oc_material == mat) ==
		(rn2(minwt + 1) != 0)) {
	    /* appropriately add damage to bill */
	    if (costly_spot(otmp->ox, otmp->oy)) {
		if (*u.ushops)
			addtobill(otmp, FALSE, FALSE, FALSE);
		else
			(void)stolen_value(otmp,
					   otmp->ox, otmp->oy, FALSE, FALSE,
					   TRUE);
	    }
	    if (otmp->quan < LARGEST_INT)
		minwt -= (int)otmp->quan;
	    else
		minwt = 0;
	    delobj(otmp);
	}
    }
}

/*
 * Polymorph some of the stuff in this pile into a monster, preferably
 * a golem of the kind okind.
 */
STATIC_OVL void
create_polymon(obj, okind)
    struct obj *obj;
    int okind;
{
	struct permonst *mdat = (struct permonst *)0;
	struct monst *mtmp;
	const char *material;
	int pm_index;

	/* no golems if you zap only one object -- not enough stuff */
	if(!obj || (!obj->nexthere && obj->quan == 1L)) return;

	/* some of these choices are arbitrary */
	/* Amy grepping target: "materialeffect" */
	switch(okind) {
	case MT_IRON:
	case MT_METAL:
	case MT_POURPOOR:
	    pm_index = PM_IRON_GOLEM;
	    material = "metal ";
	    break;
	case MT_MITHRIL:
	    pm_index = PM_MITHRIL_GOLEM;
	    material = "mithril ";
	    break;
	case MT_BRICK:
	    pm_index = PM_BRICK_GOLEM;
	    material = "brick ";
	    break;
	case MT_TAR:
	    pm_index = PM_TAR_GOLEM;
	    material = "tar ";
	    break;
	case MT_ETHER:
	    pm_index = PM_ETHER_GOLEM;
	    material = "ether ";
	    break;
	case MT_DRAGON_HIDE:
	    pm_index = PM_DRAGONHIDE_GOLEM;
	    material = "dragonhide ";
	    break;
	case MT_SECREE:
	    pm_index = PM_SECRETION_GOLEM;
	    material = "secree ";
	    break;
	case MT_COPPER:
		pm_index = PM_COPPER_GOLEM;
		material = "copper ";
		break;
	case MT_SILVER:
		pm_index = PM_SILVER_GOLEM;
		material = "silver ";
		break;
	case MT_SILK:
		pm_index = PM_SILK_GOLEM;
		material = "silk ";
		break;
	case MT_LEAD:
		pm_index = PM_LEAD_GOLEM;
		material = "lead ";
		break;
	case MT_CHROME:
		pm_index = PM_CHROME_GOLEM;
		material = "chrome ";
		break;
	case MT_SAND:
		pm_index = PM_SAND_GOLEM;
		material = "sand ";
		break;
	case MT_OBSIDIAN:
		pm_index = PM_OBSIDIAN_GOLEM;
		material = "obsidian ";
		break;
	case MT_CERAMIC:
		pm_index = PM_CERAMIC_GOLEM;
		material = "ceramic ";
		break;
	case MT_NANOMACHINE:
		pm_index = PM_NANO_GOLEM;
		material = "nanomachine ";
		break;
	case MT_SHADOWSTUFF:
		pm_index = PM_SHADOW_GOLEM;
		material = "shadowstuff ";
		break;
	case MT_PLATINUM:
		pm_index = PM_PLATINUM_GOLEM;
		material = "platinum ";
		break;
	case MT_GEMSTONE:
	case MT_MINERAL:
	    pm_index = rn2(2) ? PM_STONE_GOLEM : PM_CLAY_GOLEM;
	    material = "lithic ";
	    break;
	case MT_MYSTERIOUS:
	    pm_index = PM_MYSTERIOUS_GOLEM;
	    material = "mysterious ";
	    break;
	case MT_FLESH:
	case MT_VEGGY:
	    /* there is no flesh type, but all food is type 0, so we use it */
	    pm_index = PM_FLESH_GOLEM;
	    material = "organic ";
	    break;
	case MT_COMPOST:
	    pm_index = PM_COMPOST_GOLEM;
	    material = "compost ";
	    break;
	case MT_WAX:
		pm_index = PM_WAX_GOLEM;
		material = "wax ";
		break;
	case MT_ETERNIUM:
		pm_index = PM_ETERNIUM_GOLEM;
		material = "eternium ";
		break;
	case MT_VIVA:
		pm_index = PM_VIVA_GOLEM;
		material = "viva ";
		break;
	case MT_ARCANIUM:
		pm_index = PM_ARCANIUM_GOLEM;
		material = "arcanium ";
		break;
	case MT_WOOD:
	    pm_index = PM_WOOD_GOLEM;
	    material = "wood ";
	    break;
	case MT_LIQUID:
	    pm_index = PM_LIQUID_GOLEM;
	    material = "liquid ";
	    break;
	case MT_INKA:
	    pm_index = PM_INKA_GOLEM;
	    material = "inka ";
	    break;
	case MT_LEATHER:
	    pm_index = PM_LEATHER_GOLEM;
	    material = "leather ";
	    break;
	case MT_CLOTH:
	    pm_index = PM_ROPE_GOLEM;
	    material = "cloth ";
	    break;
	case MT_PLASTIC:
	    pm_index = PM_PLASTIC_GOLEM;
	    material = "plastic ";
	    break;
	case MT_BONE:
	    pm_index = PM_SKELETON;     /* nearest thing to "bone golem" */
	    material = "bony ";
	    break;
	case MT_GOLD:
	    pm_index = PM_GOLD_GOLEM;
	    material = "gold ";
	    break;
	case MT_GLASS:
	    pm_index = PM_GLASS_GOLEM;
	    material = "glassy ";
	    break;
	case MT_PAPER:
	    pm_index = PM_PAPER_GOLEM;
	    material = "paper ";
	    break;
	default:
	    /* if all else fails... */
	    pm_index = PM_STRAW_GOLEM;
	    material = "";
	    break;
	}

	if (!(mvitals[pm_index].mvflags & G_GENOD))
		mdat = &mons[pm_index];

	mtmp = makemon(mdat, obj->ox, obj->oy, NO_MM_FLAGS);
	polyuse(obj, okind, (int)mons[pm_index].cwt);

	if(mtmp && cansee(mtmp->mx, mtmp->my)) {
	    pline("Some %sobjects meld, and %s arises from the pile!",
		  material, a_monnam(mtmp));
	}
}

/* Assumes obj is on the floor. */
void
do_osshock(obj)
struct obj *obj;
{
	long i;

#ifdef MAIL
	if (obj->otyp == SCR_MAIL) return;
#endif
	if (obj->otyp == SCR_HEALING || obj->otyp == SCR_EXTRA_HEALING || obj->otyp == SCR_STANDARD_ID || obj->otyp == SCR_MANA || obj->otyp == SCR_GREATER_MANA_RESTORATION || obj->otyp == SCR_CURE || obj->otyp == SCR_PHASE_DOOR) return;

	obj_zapped = TRUE;

	if(poly_zapped < 0) {
	    /* some may metamorphosize */
	    for(i=obj->quan; i; i--)
		if (! rn2(Luck + (25 + rnd(20)))) {
		    poly_zapped = objects[obj->otyp].oc_material;
		    break;
		}
	}

	/* if quan > 1 then some will survive intact */
	if (obj->quan > 1L) {
	    if (obj->quan > LARGEST_INT)
		obj = splitobj(obj, (long)rnd(30000));
	    else
		obj = splitobj(obj, (long)rnd((int)obj->quan - 1));
	}

	/* appropriately add damage to bill */
	if (costly_spot(obj->ox, obj->oy)) {
		if (*u.ushops)
			addtobill(obj, FALSE, FALSE, FALSE);
		else
			(void)stolen_value(obj, obj->ox, obj->oy,
					   FALSE, FALSE, TRUE);
	}

	/* zap the object */
	delobj(obj);
}

/* [ALI] Deal with any special effects after "wearing" an object. */
void
puton_worn_item(obj)
struct obj *obj;
{
    if (!obj->owornmask)
	return;
    switch (obj->oclass) {
	case TOOL_CLASS:
	    if (obj == ublindf) Blindf_on(obj);
	    break;
	case AMULET_CLASS:
	    Amulet_on();
	    break;
	case IMPLANT_CLASS:
	    Implant_on();
	    break;
	case RING_CLASS:
	case FOOD_CLASS: /* meat ring */
	    Ring_on(obj);
	    break;
	case ARMOR_CLASS:
	    if (obj == uarm) (void) Armor_on();
	    else if (obj == uarmc) (void) Cloak_on();
	    else if (obj == uarmf) (void) Boots_on();
	    else if (obj == uarmg) (void) Gloves_on();
	    else if (obj == uarmh) (void) Helmet_on();
/*	    else if (obj == uarms) (void) Shield_on(); */
	    break;
    }
}

/*
 * Polymorph the object to the given object ID.  If the ID is STRANGE_OBJECT
 * then pick random object from the source's class (this is the standard
 * "polymorph" case).  If ID is set to a specific object, inhibit fusing
 * n objects into 1.  This could have been added as a flag, but currently
 * it is tied to not being the standard polymorph case. The new polymorphed
 * object replaces obj in its link chains.  Return value is a pointer to
 * the new object.
 *
 * This should be safe to call for an object anywhere.
 */
struct obj *
poly_obj(obj, id, degradation)
	struct obj *obj;
	int id;
	boolean degradation;
{
	struct obj *otmp;
	xchar ox, oy;
	boolean can_merge = (id == STRANGE_OBJECT);
	int obj_location = obj->where;
	int old_nutrit, new_nutrit;

	if (evilfriday) degradation = TRUE;

	boolean unpoly = (id == STRANGE_OBJECT);

	if (stack_too_big(obj)) return obj;

	if (obj->finalcancel) return obj;

	/* WAC Amulets of Unchanging shouldn't change */
	if (obj->otyp == AMULET_OF_UNCHANGING)
	    return obj;

	if (obj->otyp == BOULDER && In_sokoban(&u.uz))
		{change_luck(-1);
		pline("You cheater!");
		if (evilfriday) u.ugangr++;
		}
	    /* Sokoban guilt */
#ifdef WIZARD
	otmp = (struct obj *)0;
	if (id == STRANGE_OBJECT && wizard && Polymorph_control) {
	    int typ;
	    char buf[BUFSZ];
	    getlin("Polymorph into what? [type the name]", buf);
	    otmp = readobjnam(buf, (struct obj *)0, TRUE, FALSE);
	    if (otmp && otmp->oclass != obj->oclass) {
		delobj(otmp);
		otmp = (struct obj *)0;
	    }
	    else if (otmp) {
		typ = otmp->otyp;
		delobj(otmp);
		otmp = mksobj(typ, TRUE, FALSE);
	    }
	}
	if (!otmp)
#endif
	if (id == STRANGE_OBJECT) { /* preserve symbol */
	    int try_limit = 3;
	    /* Try up to 3 times to make the magic-or-not status of
	       the new item be the same as it was for the old one. */
	    otmp = (struct obj *)0;
	    do {
		if (otmp) delobj(otmp);
		otmp = mkobj(obj->oclass, FALSE);
	    } while (--try_limit > 0 &&
		  objects[obj->otyp].oc_magic != objects[otmp->otyp].oc_magic);
	    if (otmp && rn2(3) && objects[otmp->otyp].oc_magic) { /* force the item to become nonmagic --Amy */
		try_limit = 11;
	 	do {
			if (otmp) delobj(otmp);
			otmp = mkobj(obj->oclass, FALSE);
		} while (--try_limit > 0 && objects[otmp->otyp].oc_magic);
	    }
	} else {
	    /* literally replace obj with this new thing */
	    otmp = mksobj(id, FALSE, FALSE);
	/* Actually more things use corpsenm but they polymorph differently */
#define USES_CORPSENM(typ) ((typ)==CORPSE || (typ)==STATUE || (typ)==FIGURINE || (typ)==ENERGY_SAP)
	    if (USES_CORPSENM(obj->otyp) && USES_CORPSENM(id))
		otmp->corpsenm = obj->corpsenm;
#undef USES_CORPSENM
	}

	/* preserve quantity */
	otmp->quan = obj->quan;
	/* preserve the shopkeepers (lack of) interest */
	otmp->no_charge = obj->no_charge;
	/* preserve inventory letter if in inventory */
	if (obj_location == OBJ_INVENT)
	    otmp->invlet = obj->invlet;
#ifdef MAIL
	/* You can't send yourself 100 mail messages and then
	 * polymorph them into useful scrolls
	 */
	if (obj->otyp == SCR_MAIL) {
		otmp->otyp = SCR_MAIL;
		otmp->spe = 1;
		unpoly = FALSE;	/* WAC -- no change! */
	}
#endif

	if (obj->otyp == SCR_HEALING) {
		otmp->otyp = SCR_HEALING;
		unpoly = FALSE;	/* WAC -- no change! */
	}

	if (obj->otyp == SCR_EXTRA_HEALING) {
		otmp->otyp = SCR_EXTRA_HEALING;
		unpoly = FALSE;	/* WAC -- no change! */
	}

	if (obj->otyp == SCR_STANDARD_ID) {
		otmp->otyp = SCR_STANDARD_ID;
		unpoly = FALSE;	/* WAC -- no change! */
	}
	
	if (obj->otyp == SCR_MANA) {
		otmp->otyp = SCR_MANA;
		unpoly = FALSE;	/* WAC -- no change! */
	}
	
	if (obj->otyp == SCR_GREATER_MANA_RESTORATION) {
		otmp->otyp = SCR_GREATER_MANA_RESTORATION;
		unpoly = FALSE;	/* WAC -- no change! */
	}
	
	if (obj->otyp == SCR_CURE) {
		otmp->otyp = SCR_CURE;
		unpoly = FALSE;	/* WAC -- no change! */
	}
	if (obj->otyp == SCR_PHASE_DOOR) {
		otmp->otyp = SCR_PHASE_DOOR;
		unpoly = FALSE;	/* WAC -- no change! */
	}
	
	/* Amy change: make it so that occasionally the polymorph sticks, because polypiling in general was nerfed */
	/* In Soviet Russia, everything has to be exactly like it is in regular SLASH'EM, so it's weird that there was a
	 * variant of it being made in the first place if every change is considered bullshit anyway. */
	if (!issoviet && !rn2(5)) unpoly = FALSE;

	/* avoid abusing eggs laid by you */
	if (obj->otyp == EGG && obj->spe) {
		int mnum, tryct = 100;


		unpoly = FALSE;	/* WAC no unpolying eggs */
		/* first, turn into a generic egg */
		if (otmp->otyp == EGG)
		    kill_egg(otmp);
		else {
		    otmp->otyp = EGG;
		    otmp->owt = weight(otmp);
		}
		otmp->corpsenm = NON_PM;
		otmp->spe = 0;

		/* now change it into something layed by the hero */
		while (tryct--) {
		    mnum = can_be_hatched(random_monster());
		    if (mnum != NON_PM && !dead_species(mnum, TRUE)) {
			otmp->spe = 1;	/* layed by hero */
			otmp->corpsenm = mnum;
			attach_egg_hatch_timeout(otmp);
			break;
		    }
		}
	}

	/* keep special fields (including charges on wands) */
	if (index(charged_objs, otmp->oclass)) otmp->spe = obj->spe;
	otmp->recharged = obj->recharged;

	otmp->cursed = obj->cursed;
	otmp->hvycurse = obj->hvycurse;
	otmp->prmcurse = obj->prmcurse;
	otmp->morgcurse = obj->morgcurse;
	otmp->evilcurse = obj->evilcurse;
	otmp->bbrcurse = obj->bbrcurse;
	otmp->stckcurse = obj->stckcurse;
	otmp->blessed = obj->blessed;
	otmp->oeroded = obj->oeroded;
	otmp->oeroded2 = obj->oeroded2;
	/* Don't obscure the known fields if they were known for the base item, since that's just an interface screw --Amy */
	if (obj->known) otmp->known = TRUE;
	if (obj->dknown) otmp->dknown = TRUE;
	if (obj->bknown) otmp->bknown = TRUE;
	if (obj->rknown) otmp->rknown = TRUE;
	if (!is_flammable(otmp) && !is_rustprone(otmp)) otmp->oeroded = 0;
	if (!is_corrodeable(otmp) && !is_rottable(otmp)) otmp->oeroded2 = 0;
	if (is_damageable(otmp))
	    otmp->oerodeproof = obj->oerodeproof;

	/* Keep chest/box traps and poisoned ammo if we may */
	if (obj->otrapped && Is_box(otmp)) otmp->otrapped = TRUE;

	/* KMH, balance patch -- new macro */
	if (obj->opoisoned && is_poisonable(otmp))
		otmp->opoisoned = TRUE;

	if (id == STRANGE_OBJECT && obj->otyp == CORPSE) {
		unpoly = FALSE;	/* WAC - don't bother */
	/* turn crocodile corpses into shoes */
	    if (obj->corpsenm == PM_CROCODILE) {
		otmp->otyp = LOW_BOOTS;
		otmp->oclass = ARMOR_CLASS;
		otmp->spe = 0;
		otmp->oeroded = 0;
		otmp->oerodeproof = TRUE;
		otmp->quan = 1L;
		otmp->owt = weight(otmp);
		otmp->cursed = otmp->hvycurse = otmp->prmcurse = otmp->morgcurse = otmp->evilcurse = otmp->bbrcurse = otmp->stckcurse = FALSE;
	    }
	}

	/* no box contents --KAA */
	if (Has_contents(otmp)) delete_contents(otmp);

	/* 'n' merged objects may be fused into 1 object */
	if (otmp->quan > 1L && (!objects[otmp->otyp].oc_merge ||
				(can_merge && otmp->quan > (long)rn2(1000))))
	    otmp->quan = 1L;

	switch (otmp->oclass) {

	case TOOL_CLASS:
	    if(otmp->otyp == MAGIC_LAMP) {
		otmp->otyp = OIL_LAMP;
		otmp->age = 1500L;	/* "best" oil lamp possible */
		unpoly = FALSE;
	    } else if (otmp->otyp == MAGIC_MARKER) {
		otmp->recharged = 1;	/* degraded quality */
	    }
	    else if (otmp->otyp == LAND_MINE || otmp->otyp == BEARTRAP) {
		/* Avoid awkward questions about traps set using hazy objs */
		unpoly = FALSE;
	    }
	    /* don't care about the recharge count of other tools */
	    break;

	case WAND_CLASS:
	    while(otmp->otyp == WAN_WISHING || otmp->otyp == WAN_POLYMORPH || otmp->otyp == WAN_MUTATION || otmp->otyp == WAN_ACQUIREMENT)
		otmp->otyp = rnd_class(WAN_LIGHT, WAN_PSYBEAM);
	    /* altering the object tends to degrade its quality
	       (analogous to spellbook `read count' handling) */
	    if ((int)otmp->recharged < rn2(7))	/* recharge_limit */
		otmp->recharged++;
	    break;

	case POTION_CLASS:
	    while (otmp->otyp == POT_POLYMORPH || otmp->otyp == POT_MUTATION)
		otmp->otyp = rnd_class(POT_GAIN_ABILITY, POT_WATER);
	    break;

	case SCROLL_CLASS:
	    while (otmp->otyp == SCR_WISHING || otmp->otyp == SCR_RESURRECTION || otmp->otyp == SCR_ACQUIREMENT || otmp->otyp == SCR_ENTHRONIZATION || otmp->otyp == SCR_MAKE_PENTAGRAM || otmp->otyp == SCR_FOUNTAIN_BUILDING || otmp->otyp == SCR_SINKING || otmp->otyp == SCR_WC)
		otmp->otyp = rnd_class(SCR_CREATE_MONSTER, SCR_BLANK_PAPER);
	    break;

	case SPBOOK_CLASS:
	    while (otmp->otyp == SPE_POLYMORPH || otmp->otyp == SPE_MUTATION)
		otmp->otyp = rnd_class(SPE_DIG, SPE_BLANK_PAPER);
	    /* reduce spellbook abuse */
	    if ((int)otmp->recharged < rn2(7))	/* recharge_limit */
		otmp->recharged++;
	    otmp->spestudied = obj->spestudied + 1;
	    break;

	case GEM_CLASS:
	    if (otmp->quan > (long) rnd(4) &&
		    objects[obj->otyp].oc_material == MT_MINERAL &&
		    objects[otmp->otyp].oc_material != MT_MINERAL) {
		otmp->otyp = ROCK;	/* transmutation backfired */
		otmp->quan /= 2L;	/* some material has been lost */
	    }
	    break;

	case FOOD_CLASS:
	    if (otmp->otyp == SLIME_MOLD)
		otmp->spe = current_fruit;
	    /* Preserve percentage eaten (except for tins) */
	    old_nutrit = objects[obj->otyp].oc_nutrition;
	    if (obj->oeaten && otmp->otyp != TIN && old_nutrit) {
		new_nutrit = objects[otmp->otyp].oc_nutrition;
		otmp->oeaten = obj->oeaten * new_nutrit / old_nutrit;
		if (otmp->oeaten == 0)
		    otmp->oeaten++;
		if (otmp->oeaten >= new_nutrit)
		    otmp->oeaten = new_nutrit - 1;
	    }
	    break;
	}

	/* degrade the object because polypiling is too powerful --Amy */
	if (degradation) {
		if (rn2(2) || evilfriday) {
			if (otmp->spe > 2) otmp->spe /= 2;
			else if (otmp->spe > -20) otmp->spe--;
		}
		if (rn2(2) || evilfriday) {
			if (otmp->cursed) curse(otmp);
			else if (!otmp->blessed && rn2(3)) curse(otmp);
			else if (!rn2(3)) curse(otmp);
			else if (rn2(3)) unbless(otmp);
		}
	}

	/* update the weight */
	otmp->owt = weight(otmp);

	/* for now, take off worn items being polymorphed */
	/* [ALI] In Slash'EM only take off worn items if no longer compatible */
	if (obj_location == OBJ_INVENT || obj_location == OBJ_MINVENT) {
		/* This is called only for stone to flesh.  It's a lot simpler
		 * than it otherwise might be.  We don't need to check for
		 * special effects when putting them on (no meat objects have
		 * any) and only three worn masks are possible.
		 */
	    /* [ALI] Unfortunately, hazy polymorphs means that this
	     * is not true for Slash'EM, and we need to be a little more
	     * careful.
	     */
	    if (obj == uskin) rehumanize();
		otmp->owornmask = obj->owornmask;
	    /* Quietly remove worn item if no longer compatible --ALI */
	    if (otmp->owornmask & W_ARM && !is_suit(otmp))
		otmp->owornmask &= ~W_ARM;
	    if (otmp->owornmask & W_ARMC && !is_cloak(otmp))
		otmp->owornmask &= ~W_ARMC;
	    if (otmp->owornmask & W_ARMH && !is_helmet(otmp))
		otmp->owornmask &= ~W_ARMH;
	    if (otmp->owornmask & W_ARMS && !is_shield(otmp))
		otmp->owornmask &= ~W_ARMS;
	    if (otmp->owornmask & W_ARMG && !is_gloves(otmp))
		otmp->owornmask &= ~W_ARMG;
	    if (otmp->owornmask & W_ARMF && !is_boots(otmp))
		otmp->owornmask &= ~W_ARMF;
	    if (otmp->owornmask & W_ARMU && !is_shirt(otmp))
		otmp->owornmask &= ~W_ARMU;
	    if (otmp->owornmask & W_TOOL && otmp->otyp != BLINDFOLD && otmp->otyp != EYECLOSER && otmp->otyp != DRAGON_EYEPATCH && otmp->otyp != CONDOME && otmp->otyp != SOFT_CHASTITY_BELT &&
	      otmp->otyp != TOWEL && otmp->otyp != LENSES && otmp->otyp != RADIOGLASSES && otmp->otyp != BOSS_VISOR)
		otmp->owornmask &= ~W_TOOL;
	    if (obj->otyp == LEATHER_LEASH && obj->leashmon) o_unleash(obj);
	    if (obj->otyp == INKA_LEASH && obj->leashmon) o_unleash(obj);
	    if (obj_location == OBJ_INVENT) {
		remove_worn_item(obj, TRUE);
		setworn(otmp, otmp->owornmask);
		puton_worn_item(otmp);
		if (otmp->owornmask & LEFT_RING)
		    uleft = otmp;
		if (otmp->owornmask & RIGHT_RING)
		    uright = otmp;
		if (otmp->owornmask & W_WEP)
		    uwep = otmp;
		if (otmp->owornmask & W_SWAPWEP)
		    uswapwep = otmp;
		if (otmp->owornmask & W_QUIVER)
		    uquiver = otmp;
	    }
	    /* (We have to pend updating monster intrinsics until later) */
	}
	else {
	/* preserve the mask in case being used by something else */
	otmp->owornmask = obj->owornmask;
	    if (otmp->owornmask & W_SADDLE && otmp->otyp != LEATHER_SADDLE && otmp->otyp != INKA_SADDLE) {
		struct monst *mtmp = obj->ocarry;
		dismount_steed(DISMOUNT_THROWN);
		otmp->owornmask &= ~W_SADDLE;
		/* The ex-saddle slips to the floor */
		mtmp->misc_worn_check &= ~obj->owornmask;
		otmp->owornmask = obj->owornmask = 0;
		update_mon_intrinsics(mtmp, obj, FALSE, FALSE);
		obj_extract_self(obj);
		place_object(obj, mtmp->mx, mtmp->my);
		stackobj(obj);
		newsym(mtmp->mx, mtmp->my);
		obj_location = OBJ_FLOOR;
	    }
	}

	if (obj_location == OBJ_FLOOR && obj->otyp == BOULDER &&
		otmp->otyp != BOULDER)
	    unblock_point(obj->ox, obj->oy);

	/* WAC -- Attach unpoly timer if this is a standard poly */
	if (unpoly /* && !rn2(20) */) {
		set_obj_poly(otmp, obj);
		if (is_hazy(otmp) && !Blind && carried(obj))
			pline("%s seems hazy.", Yname2(otmp));
	}

	/* ** we are now done adjusting the object ** */

	/* swap otmp for obj */
	replace_object(obj, otmp);
	if (obj_location == OBJ_INVENT) {
	    /*
	     * We may need to do extra adjustments for the hero if we're
	     * messing with the hero's inventory.  The following calls are
	     * equivalent to calling freeinv on obj and addinv on otmp,
	     * while doing an in-place swap of the actual objects.
	     */
	    freeinv_core(obj);
	    addinv_core1(otmp);
	    addinv_core2(otmp);
	}
	else if (obj_location == OBJ_MINVENT) {
	    /* Pended update of monster intrinsics */
	    update_mon_intrinsics(obj->ocarry, obj, FALSE, FALSE);
	    if (otmp->owornmask)
		update_mon_intrinsics(otmp->ocarry, otmp, TRUE, FALSE);
	}

	if ((!carried(otmp) || obj->unpaid) &&
		!is_hazy(obj) &&
		get_obj_location(otmp, &ox, &oy, BURIED_TOO|CONTAINED_TOO) &&
		costly_spot(ox, oy)) {
	    char objroom = *in_rooms(ox, oy, SHOPBASE);
	    register struct monst *shkp = shop_keeper(objroom);

	    if ((!obj->no_charge ||
		 (Has_contents(obj) &&
		    (contained_cost(obj, shkp, 0L, FALSE, FALSE) != 0L)))
	       && inhishop(shkp)) {
		if(shkp->mpeaceful) {
		    if(*u.ushops && *in_rooms(u.ux, u.uy, 0) ==
			    *in_rooms(shkp->mx, shkp->my, 0) &&
			    !costly_spot(u.ux, u.uy))
			make_angry_shk(shkp, ox, oy);
		    else {
			pline("%s gets angry!", Monnam(shkp));
			hot_pursuit(shkp);
		    }
		} else Norep("%s is furious!", Monnam(shkp));
		if (!carried(otmp)) {
		    if (costly_spot(u.ux, u.uy) && objroom == *u.ushops)
			bill_dummy_object(obj);
		    else
			(void) stolen_value(obj, ox, oy, FALSE, FALSE, TRUE);
		}
	    }
	}
	delobj(obj);
	return otmp;
}

/*
 * Object obj was hit by the effect of the wand/spell otmp.  Return
 * non-zero if the wand/spell had any effect.
 */
int
bhito(obj, otmp)
struct obj *obj, *otmp;
{
	int res = 1;	/* affected object by default */
	xchar refresh_x, refresh_y;

	if (obj->bypass) {
		/* The bypass bit is currently only used as follows:
		 *
		 * POLYMORPH - When a monster being polymorphed drops something
		 *             from its inventory as a result of the change.
		 *             If the items fall to the floor, they are not
		 *             subject to direct subsequent polymorphing
		 *             themselves on that same zap. This makes it
		 *             consistent with items that remain in the
		 *             monster's inventory. They are not polymorphed
		 *             either.
		 * UNDEAD_TURNING - When an undead creature gets killed via
		 *	       undead turning, prevent its corpse from being
		 *	       immediately revived by the same effect.
		 *
		 * The bypass bit on all objects is reset each turn, whenever
		 * flags.bypasses is set.
		 *
		 * We check the obj->bypass bit above AND flags.bypasses
		 * as a safeguard against any stray occurrence left in an obj
		 * struct someplace, although that should never happen.
		 */
		if (flags.bypasses)
			return 0;
		else {
#ifdef DEBUG
			pline("%s for a moment.", Tobjnam(obj, "pulsate"));
#endif
			obj->bypass = 0;
		}
	}

	/*
	 * Some parts of this function expect the object to be on the floor
	 * obj->{ox,oy} to be valid.  The exception to this (so far) is
	 * for the STONE_TO_FLESH spell.
	 */
	if (!(obj->where == OBJ_FLOOR || otmp->otyp == SPE_STONE_TO_FLESH))
	    impossible("bhito: obj is not floor or Stone To Flesh spell");

	if (obj == uball) {
		res = 0;
	} else if (obj == uchain) {
		if (otmp->otyp == WAN_OPENING || otmp->otyp == SPE_KNOCK || (otmp->otyp == SPE_LOCK_MANIPULATION && rn2(2)) ) {
		    if (otmp->otyp != SPE_KNOCK && otmp->otyp != SPE_LOCK_MANIPULATION) unpunish();
		    if ((otmp->otyp == SPE_KNOCK || otmp->otyp == SPE_LOCK_MANIPULATION) && !rn2(5)) {
			pline_The("chain is separated from the ball.");
			if (uchain && uchain->owt > 0) {
				IncreasedGravity += uchain->owt;
				u.inertia += (uchain->owt / 10);
				You("are temporarily slowed by the weight of the chain.");
			}
			unpunish();
		    }
		    makeknown(otmp->otyp);
		} else
		    res = 0;
	} else
	switch(otmp->otyp) {
	case WAN_POLYMORPH:
	case SPE_POLYMORPH:
	case WAN_MUTATION:
	case SPE_MUTATION:
		if (obj->otyp == WAN_POLYMORPH ||
			obj->otyp == WAN_MUTATION ||
			obj->otyp == SPE_POLYMORPH ||
			obj->otyp == SPE_MUTATION ||
			obj->otyp == POT_POLYMORPH ||
			obj->otyp == POT_MUTATION ||
			obj_resists(obj, 5, 95)) {
		    res = 0;
		    break;
		}

		/* KMH, conduct */
		u.uconduct.polypiles++;
		/* any saved lock context will be dangerously obsolete */
		if (Is_box(obj)) (void) boxlock(obj, otmp);

		if (obj_shudders(obj)) {
		    if (cansee(obj->ox, obj->oy))
			makeknown(otmp->otyp);
		    do_osshock(obj);
		    break;
		} else if (rn2(2) && obj_shudders(obj)) { /* huge nerf by Amy */
		    if (cansee(obj->ox, obj->oy))
			makeknown(otmp->otyp);
		    do_osshock(obj);
		    break;
		}
		obj = poly_obj(obj, STRANGE_OBJECT, TRUE);
		newsym(obj->ox,obj->oy);
		break;
	case WAN_PROBING:
		res = !obj->dknown;
		/* target object has now been "seen (up close)" */
		obj->dknown = 1;
		if (Is_container(obj) || obj->otyp == STATUE) {
		    if (!obj->cobj)
			pline("%s empty.", Tobjnam(obj, "are"));
		    else {
			struct obj *o;
			/* view contents (not recursively) */
			for (o = obj->cobj; o; o = o->nobj)
			    o->dknown = 1;	/* "seen", even if blind */
			(void) display_cinventory(obj);
		    }
		    res = 1;
		}
		if (res) makeknown(WAN_PROBING);
		break;
	case WAN_STRIKING:
	case SPE_FORCE_BOLT:
	case WAN_GRAVITY_BEAM:
	case SPE_GRAVITY_BEAM:
		if (obj->otyp == BOULDER)
			fracture_rock(obj);
		else if (obj->otyp == STATUE)
			(void) break_statue(obj);
		else {
			if (!flags.mon_moving)
			    (void)hero_breaks(obj, obj->ox, obj->oy, FALSE);
			else
			    (void)breaks(obj, obj->ox, obj->oy);
			res = 0;
		}
		/* BUG[?]: shouldn't this depend upon you seeing it happen? */
		makeknown(otmp->otyp);
		break;
	case WAN_CANCELLATION:
		cancel_item(obj, FALSE);
#ifdef TEXTCOLOR
		newsym(obj->ox,obj->oy);	/* might change color */
#endif
		break;
	case SPE_CANCELLATION:
		cancel_item(obj, TRUE);
#ifdef TEXTCOLOR
		newsym(obj->ox,obj->oy);	/* might change color */
#endif
		break;
	case SPE_DRAIN_LIFE:
	case WAN_DRAINING:	/* KMH */
	case SPE_TIME:
	case WAN_TIME:
	case WAN_REDUCE_MAX_HITPOINTS:
		(void) drain_item(obj);
		break;
	case WAN_INCREASE_MAX_HITPOINTS:
		(void) drain_item_negative(obj);
		break;
	case WAN_TELEPORTATION:
	case WAN_BANISHMENT:
	case SPE_TELEPORT_AWAY:
		rloco(obj);
		break;
	case WAN_MAKE_INVISIBLE:
		if (!always_visible(obj) && !stack_too_big(obj) ) {
		    obj->oinvis = TRUE;
		    if (!rn2(100)) obj->oinvisreal = TRUE;
		    newsym(obj->ox,obj->oy);	/* make object disappear */
		}
		break;
	case WAN_MAKE_VISIBLE:
	case SPE_MAKE_VISIBLE:
		if (!stack_too_big(obj)) obj->oinvis = FALSE;
		if (obj->oinvisreal) obj->oinvis = TRUE;
		newsym(obj->ox,obj->oy);	/* make object appear */
		break;
	case WAN_UNDEAD_TURNING:
	case SPE_TURN_UNDEAD:
		if (obj->otyp == EGG)
			revive_egg(obj);
		else {

			/* Anti-farming measure by Amy */
		    if (obj->otyp == CORPSE && !rn2(10)) {

				int x, y;
				switch (obj->where) {
				    case OBJ_INVENT:
					useup(obj);
					break;
				    case OBJ_FLOOR:
					/* in case MON_AT+enexto for invisible mon */
					x = obj->ox,  y = obj->oy;
					/* not useupf(), which charges */
					if (obj->quan > 1L)
					    obj = splitobj(obj, 1);
					delobj(obj);
					newsym(x, y);
					break;
				    case OBJ_MINVENT:
					m_useup(obj->ocarry, obj);
					break;
				    case OBJ_CONTAINED:
					obj_extract_self(obj);
					obfree(obj, (struct obj *) 0);
					break;
				    default:
					panic("unturn_dead corpse in weird place!");
				}

		    }
			res = !!revive(obj);
		}
		break;
	case WAN_OPENING:
	case SPE_KNOCK:
	case WAN_LOCKING:
	case SPE_WIZARD_LOCK:
	case SPE_LOCK_MANIPULATION:
		if(Is_box(obj))
			res = boxlock(obj, otmp);
		else
			res = 0;
		if (res /* && otmp->oclass == WAND_CLASS */)
			makeknown(otmp->otyp);
		break;
	case SPE_WATER_BOLT:
		break;

	case WAN_SLOW_MONSTER:		/* no effect on objects */
	case SPE_SLOW_MONSTER:
	case SPE_RANDOM_SPEED:
	case WAN_INERTIA:
	case SPE_INERTIA:
	case WAN_STUN_MONSTER:
	case SPE_STUN_MONSTER:
	case WAN_SPEED_MONSTER:
	case WAN_HASTE_MONSTER:
	case WAN_NOTHING:
	case WAN_MISFIRE:
	case SPE_HEALING:
	case SPE_VANISHING:
	case SPE_EXTRA_HEALING:
	case SPE_FULL_HEALING:
	case WAN_HEALING:
	case WAN_EXTRA_HEALING:
	case WAN_CLONE_MONSTER:
	case SPE_CLONE_MONSTER:
	case WAN_FULL_HEALING:
	case SPE_FINGER:
	case WAN_FEAR:
	case SPE_HORRIFY:
	case WAN_SHARE_PAIN:
	case WAN_STONING:
	case WAN_PARALYSIS:
	case SPE_PETRIFY:
	case SPE_PARALYSIS:
	case WAN_FIREBALL:
	case WAN_DESLEXIFICATION:
	case WAN_BUBBLEBEAM:
	case SPE_BUBBLEBEAM:
	case WAN_DREAM_EATER:
	case SPE_MANA_BOLT:
	case SPE_SNIPER_BEAM:
	case SPE_BLINDING_RAY:
	case SPE_ENERGY_BOLT:
	case SPE_DREAM_EATER:
	case SPE_VOLT_ROCK:
	case SPE_GIANT_FOOT:
	case SPE_ARMOR_SMASH:
	case SPE_BLOOD_STREAM:
	case SPE_SHINING_WAVE:
	case SPE_STRANGLING:
	case SPE_PARTICLE_CANNON:
	case SPE_NERVE_POISON:
	case SPE_GEYSER:
	case SPE_BUBBLING_HOLE:
	case SPE_BATTERING_RAM:
	case SPE_WATER_FLAME:
	case WAN_GOOD_NIGHT:
	case SPE_GOOD_NIGHT:
	case WAN_INFERNO:
	case SPE_INFERNO:
	case SPE_CALL_THE_ELEMENTS:
	case WAN_THUNDER:
	case SPE_THUNDER:
	case WAN_ICE_BEAM:
	case SPE_ICE_BEAM:
	case WAN_CHLOROFORM:
	case SPE_CHLOROFORM:
	case WAN_SLUDGE:
	case SPE_SLUDGE:
	case WAN_TOXIC:
	case SPE_TOXIC:
	case WAN_NETHER_BEAM:
	case SPE_NETHER_BEAM:
	case WAN_AURORA_BEAM:
	case SPE_AURORA_BEAM:
	case SPE_CHAOS_BOLT:
	case SPE_HELLISH_BOLT:
		res = 0;
		break;
	case SPE_DISINTEGRATION:
	case WAN_DISINTEGRATION:

#define oresist_disintegrationX(obj) \
		(objects[obj->otyp].oc_oprop == DISINT_RES || \
		 obj_resists(obj, 5, 50) || is_quest_artifact(obj) )

		    if (!oresist_disintegrationX(obj) && !stack_too_big(obj) ) {
			if (Has_contents(obj)) delete_contents(obj);
			obj_extract_self(obj);
			obfree(obj, (struct obj *)0);
		    }

		break;
	case SPE_STONE_TO_FLESH:
		refresh_x = obj->ox; refresh_y = obj->oy;
		if (objects[obj->otyp].oc_material != MT_MINERAL && objects[obj->otyp].oc_material != MT_TAR &&
			objects[obj->otyp].oc_material != MT_GEMSTONE) {
		    res = 0;
		    break;
		}
		/* add more if stone objects are added.. */
		switch (objects[obj->otyp].oc_class) {
		    case ROCK_CLASS:	/* boulders and statues */
			if (obj->otyp == BOULDER) {
			    obj = poly_obj(obj, rnd(20) ? MEATBALL : HUGE_CHUNK_OF_MEAT, FALSE);
			    goto smell;
			} else if (obj->otyp == STATUE) {

				/* endless stone to flesh farming shouldn't be possible --Amy */
			    if (!rn2(10)) {
			      obj = poly_obj(obj, MEATBALL, FALSE);
			      goto smell;
			    }

			    xchar oox, ooy;

			    (void) get_obj_location(obj, &oox, &ooy, 0);
			    refresh_x = oox; refresh_y = ooy;
			    if (vegetarian(&mons[obj->corpsenm])) {
				/* Don't animate monsters that aren't flesh */
				obj = poly_obj(obj, MEATBALL, FALSE);
			    	goto smell;
			    }
			    if (!animate_statue(obj, oox, ooy,
						ANIMATE_SPELL, (int *)0)) {
				struct obj *item;
makecorpse:			if (mons[obj->corpsenm].geno &
							(G_NOCORPSE|G_UNIQ)) {
				    res = 0;
				    break;
				}
				/* Unlikely to get here since genociding
				 * monsters also sets the G_NOCORPSE flag.
				 * Drop the contents, poly_obj looses them.
				 */
				while ((item = obj->cobj) != 0) {
				    obj_extract_self(item);
				    place_object(item, oox, ooy);
				}
				obj = poly_obj(obj, CORPSE, FALSE);
				break;
			    }
			} else { /* new rock class object... */
			    /* impossible? */
			    res = 0;
			}
			break;
		    case TOOL_CLASS:	/* figurine */
		    {
			struct monst *mon;
			xchar oox, ooy;

			if (obj->otyp != FIGURINE) {
			    res = 0;
			    break;
			}
			if (vegetarian(&mons[obj->corpsenm])) {
			    /* Don't animate monsters that aren't flesh */
			    obj = poly_obj(obj, MEATBALL, FALSE);
			    goto smell;
			}
			(void) get_obj_location(obj, &oox, &ooy, 0);
			refresh_x = oox; refresh_y = ooy;
			mon = makemon(&mons[obj->corpsenm],
				      oox, ooy, NO_MM_FLAGS);
			if (mon) {
			    delobj(obj);
			    if (cansee(mon->mx, mon->my))
				pline_The("figurine animates!");
			    break;
			}
			goto makecorpse;
		    }
		    /* maybe add weird things to become? */
		    case RING_CLASS:	/* some of the rings are stone */
			obj = poly_obj(obj, MEAT_RING, FALSE);
			goto smell;
		    case WAND_CLASS:	/* marble wand */
			obj = poly_obj(obj, MEAT_STICK, FALSE);
			goto smell;
		    case GEM_CLASS:	/* rocks & gems */
			obj = poly_obj(obj, MEATBALL, FALSE);
smell:
			if (herbivorous(youmonst.data) &&
			    (!carnivorous(youmonst.data) ||
			     Role_if(PM_MONK) || !u.uconduct.unvegetarian))
			    Norep("You smell the odor of meat.");
			else
			    Norep("You smell a delicious smell.");
			break;
		    case WEAPON_CLASS:	/* crysknife */
		    	/* fall through */
		    default:
			res = 0;
			break;
		}
		newsym(refresh_x, refresh_y);
		break;
	case WAN_WIND:
	case SPE_WIND:
		refresh_x = obj->ox;
		refresh_y = obj->oy;
		scatter(obj->ox,obj->oy,4,VIS_EFFECTS|MAY_HIT|MAY_DESTROY|MAY_FRACTURE,(struct obj*)0);
		newsym(refresh_x,refresh_y);
		makeknown(otmp->otyp);
		break;
	case SPE_BLANK_PAPER: /* placeholder for T_BLADE_ANGER */
		break;
	default:
		impossible("What an interesting effect (%d)", otmp->otyp);
		break;
	}
	return res;
}

/* returns nonzero if something was hit */
int
bhitpile(obj,fhito,tx,ty)
    struct obj *obj;
    int (*fhito)(OBJ_P,OBJ_P);
    int tx, ty;
{
    int hitanything = 0;
    register struct obj *otmp, *next_obj;

    if (obj->otyp == SPE_FORCE_BOLT || obj->otyp == WAN_STRIKING || obj->otyp == SPE_GRAVITY_BEAM || obj->otyp == WAN_GRAVITY_BEAM) {
	struct trap *t = t_at(tx, ty);

	/* We can't settle for the default calling sequence of
	   bhito(otmp) -> break_statue(otmp) -> activate_statue_trap(ox,oy)
	   because that last call might end up operating on our `next_obj'
	   (below), rather than on the current object, if it happens to
	   encounter a statue which mustn't become animated. */
	if (t && (t->ttyp == STATUE_TRAP || t->ttyp == SATATUE_TRAP) &&
	    activate_statue_trap(t, tx, ty, TRUE) && obj->otyp == WAN_STRIKING)
	    makeknown(obj->otyp);
    }

    poly_zapped = -1;
    for(otmp = level.objects[tx][ty]; otmp; otmp = next_obj) {
	/* Fix for polymorph bug, Tim Wright */
	next_obj = otmp->nexthere;
	hitanything += (*fhito)(otmp, obj);
    }
    if(poly_zapped >= 0)
	create_polymon(level.objects[tx][ty], poly_zapped);

    return hitanything;
}
#endif /*OVLB*/
#ifdef OVL1

/*
 * zappable - returns 1 if zap is available, 0 otherwise.
 *	      it removes a charge from the wand if zappable.
 * added by GAN 11/03/86
 */
int
zappable(wand)
register struct obj *wand;
{
	int nochargechange = 10;
	if (!(PlayerCannotUseSkills)) {
		switch (P_SKILL(P_DEVICES)) {
			default: break;
			case P_BASIC: nochargechange = 9; break;
			case P_SKILLED: nochargechange = 8; break;
			case P_EXPERT: nochargechange = 7; break;
			case P_MASTER: nochargechange = 6; break;
			case P_GRAND_MASTER: nochargechange = 5; break;
			case P_SUPREME_MASTER: nochargechange = 4; break;
		}
		if (wand->otyp == WAN_WISHING || wand->otyp == WAN_CHARGING || wand->otyp == WAN_BAD_EQUIPMENT || wand->otyp == WAN_ACQUIREMENT || wand->otyp == WAN_GAIN_LEVEL || wand->otyp == WAN_INCREASE_MAX_HITPOINTS ) nochargechange = 10;
	}
	if(wand->spe < 0 || (wand->spe == 0 && rn2(20))) /* make wresting easier --Amy */
		return 0;
	if(wand->spe == 0) {
		You("wrest one last charge from the worn-out wand.");
		u.cnd_wandwresting++; /* wand may survive due to devices skill or being made of viva (not a bug) */
	}
	if ((!rn2(2) || !wand->oartifact) && (!rn2(2) || !(objects[(wand)->otyp].oc_material == MT_VIVA) ) && !(uarmc && uarmc->oartifact == ART_ARABELLA_S_WEAPON_STORAGE && rn2(4) && !(wand->otyp == WAN_WISHING || wand->otyp == WAN_CHARGING || wand->otyp == WAN_BAD_EQUIPMENT || wand->otyp == WAN_ACQUIREMENT || wand->otyp == WAN_GAIN_LEVEL || wand->otyp == WAN_INCREASE_MAX_HITPOINTS) ) && (nochargechange >= rnd(10) ) ) wand->spe--;

	if (DischargeBug || u.uprops[DISCHARGE_BUG].extrinsic || have_dischargestone()) wand->spe--;

	u.cnd_zapcount++;
	use_skill(P_DEVICES,1);
	if (Race_if(PM_STICKER)) {
		use_skill(P_DEVICES,1);
		use_skill(P_DEVICES,1);
		use_skill(P_DEVICES,1);
		use_skill(P_DEVICES,1);
	}
	if (Race_if(PM_FAWN)) {
		use_skill(P_DEVICES,1);
	}
	if (Race_if(PM_SATRE)) {
		use_skill(P_DEVICES,1);
		use_skill(P_DEVICES,1);
	}
	if (objects[(wand)->otyp].oc_material == MT_INKA) use_skill(P_DEVICES,1);

	if (Race_if(PM_INKA)) {
		use_skill(P_DEVICES,1);
		if (objects[(wand)->otyp].oc_material == MT_INKA) use_skill(P_DEVICES,1);
	}

	return 1;
}

/*
 * zapnodir - zaps a NODIR wand/spell.
 * added by GAN 11/03/86
 */
void
zapnodir(obj)
register struct obj *obj;
{
	register int i;
	register struct obj *otmp, *acqo;
	boolean known = FALSE;

	switch(obj->otyp) {
		case WAN_TRAP_CREATION:
		known = TRUE;
		    You_feel("endangered!!");
		{
			int rtrap;
		    int i, j, bd = 1;

		      for (i = -bd; i <= bd; i++) for(j = -bd; j <= bd; j++) {
				if (!isok(u.ux + i, u.uy + j)) continue;
				if (levl[u.ux + i][u.uy + j].typ <= DBWALL) continue;
				if (t_at(u.ux + i, u.uy + j)) continue;

			      rtrap = randomtrap();

				(void) maketrap(u.ux + i, u.uy + j, rtrap, 100);
			}
		}

		makerandomtrap();
		if (!rn2(2)) makerandomtrap();
		if (!rn2(4)) makerandomtrap();
		if (!rn2(8)) makerandomtrap();
		if (!rn2(16)) makerandomtrap();
		if (!rn2(32)) makerandomtrap();
		if (!rn2(64)) makerandomtrap();
		if (!rn2(128)) makerandomtrap();
		if (!rn2(256)) makerandomtrap();

		break;

		case WAN_SUMMON_SEXY_GIRL:
		known = TRUE;

		if (Aggravate_monster) {
			u.aggravation = 1;
			reset_rndmonst(NON_PM);
		}

	    {	coord cc;
		struct permonst *pm = 0;
		int attempts = 0;

newboss:
		do {
			pm = rndmonst();
			attempts++;

		} while ( (!pm || (pm && !(pm->msound == MS_FART_LOUD || pm->msound == MS_FART_NORMAL || pm->msound == MS_FART_QUIET ))) && attempts < 50000);

		if (!pm && rn2(50) ) {
			attempts = 0;
			goto newboss;
		}
		if (pm && !(pm->msound == MS_FART_LOUD || pm->msound == MS_FART_NORMAL || pm->msound == MS_FART_QUIET) && rn2(50) ) {
			attempts = 0;
			goto newboss;
		}

		if (pm) (void) makemon(pm, u.ux, u.uy, NO_MM_FLAGS);
	    }

		u.aggravation = 0;

		break;

		case WAN_BAD_EFFECT:

			badeffect();

		break;

		case WAN_GAIN_LEVEL:

			pluslvl(FALSE);
			known = TRUE;

		break;

		case WAN_BLEEDING:

			playerbleed(rnd(2 + (level_difficulty() * 10)));
			known = TRUE;

		break;

		case WAN_UNDRESSING:

			shank_player();
			known = TRUE;

		break;

		case WAN_CURSE_ITEMS:

			pline("A black glow surrounds you...");
			if (PlayerHearsSoundEffects) pline(issoviet ? "Vashe der'mo tol'ko chto proklinal." : "Woaaaaaa-AAAH!");
			rndcurse();
			known = TRUE;

		break;

		case WAN_AMNESIA:

			if (FunnyHallu)
			    pline("Hakuna matata!");
			else
			    You_feel("your memories dissolve.");

			forget( ALL_SPELLS | ALL_MAP);
			known = TRUE;
		    exercise(A_WIS, FALSE);

		    if (obj && obj->oartifact == ART_NOT_KNOWN_ANYMORE) {
			struct obj *otmpSC;
			pline("You may fully identify an object!");

secureidchoice:
			otmpSC = getobj(all_count, "secure identify");

			if (!otmpSC) {
				if (yn("Really exit with no object selected?") == 'y')
					pline("You just wasted the opportunity to secure identify your objects.");
				else goto secureidchoice;
				pline("A feeling of loss comes over you.");
				break;
			}
			if (otmpSC) {
				makeknown(otmpSC->otyp);
				if (otmpSC->oartifact) discover_artifact((int)otmpSC->oartifact);
				otmpSC->known = otmpSC->dknown = otmpSC->bknown = otmpSC->rknown = 1;
				if (otmpSC->otyp == EGG && otmpSC->corpsenm != NON_PM)
				learn_egg_type(otmpSC->corpsenm);
				prinv((char *)0, otmpSC, 0L);
			}

		    }

		break;

		case WAN_BAD_LUCK:

			known = TRUE;
			You_feel("very unlucky.");
			change_luck(-1);

		break;

		case WAN_DISENCHANTMENT:

			known = TRUE;
		{
			struct obj *otmpE;
		      for (otmpE = invent; otmpE; otmpE = otmpE->nobj) {
				if (otmpE && !rn2(10)) (void) drain_item_severely(otmpE);
			}
			u.cnd_disenchantamount++;
			Your("equipment seems less effective.");
			if (PlayerHearsSoundEffects) pline(issoviet ? "Vse, chto vy vladeyete budet razocharovalsya v zabveniye, kha-kha-kha!" : "Klatsch!");
		}

		break;

		case WAN_CONTAMINATION:

			contaminate(rnd(10 + level_difficulty()), TRUE);

		break;

		case WAN_TREMBLING:

			known = TRUE;
			pline("Your %s are trembling!", makeplural(body_part(HAND)));
			u.tremblingamount++;

		break;

		case WAN_REMOVE_RESISTANCE:

			attrcurse();
			while (rn2(3)) {
				attrcurse();
			}

		break;

		case WAN_CHAOS_TERRAIN:

			wandofchaosterrain();
			known = TRUE;

		break;

		case WAN_FLEECY_TERRAIN:

			wandoffleecyterrain();
			known = TRUE;

		break;

		case WAN_CORROSION:

		{

		    register struct obj *objX, *objX2;
		    for (objX = invent; objX; objX = objX2) {
		      objX2 = objX->nobj;
			if (!rn2(5)) rust_dmg(objX, xname(objX), 3, TRUE, &youmonst);
		    }
		}

		break;

		case WAN_FUMBLING:

			HFumbling = FROMOUTSIDE | rnd(5);
			incr_itimeout(&HFumbling, rnd(20));
			u.fumbleduration += rnz(1000);

		break;

		case WAN_SIN:

		{
		int dmg = 0;
		struct obj *otmpi, *otmpii;

		switch (rnd(8)) {

			case 1: /* gluttony */
				u.negativeprotection++;
				You_feel("less protected!");
				break;
			case 2: /* wrath */
				if(u.uen < 1) {
				    You_feel("less energised!");
				    u.uenmax -= rn1(10,10);
				    if(u.uenmax < 0) u.uenmax = 0;
				} else if(u.uen <= 10) {
				    You_feel("your magical energy dwindle to nothing!");
				    u.uen = 0;
				} else {
				    You_feel("your magical energy dwindling rapidly!");
				    u.uen /= 2;
				}
				break;
			case 3: /* sloth */
				You_feel("a little apathetic...");

				switch(rn2(7)) {
				    case 0: /* destroy certain things */
					lethe_damage(invent, FALSE, FALSE);
					break;
				    case 1: /* sleep */
					if (multi >= 0) {
					    if (Sleep_resistance && rn2(StrongSleep_resistance ? 20 : 5)) {pline("You yawn."); break;}
					    fall_asleep(-rnd(10), TRUE);
					    You("are put to sleep!");
					}
					break;
				    case 2: /* paralyse */
					if (multi >= 0) {
					    if (Free_action && rn2(StrongFree_action ? 100 : 20)) {
						You("momentarily stiffen.");            
					    } else {
						You("are frozen!");
						if (PlayerHearsSoundEffects) pline(issoviet ? "Teper' vy ne mozhete dvigat'sya. Nadeyus', chto-to ubivayet vas, prezhde chem vash paralich zakonchitsya." : "Klltsch-tsch-tsch-tsch-tsch!");
						nomovemsg = 0;	/* default: "you can move again" */
						nomul(-rnd(10), "paralyzed by a wand of sin", TRUE);
						exercise(A_DEX, FALSE);
					    }
					}
					break;
				    case 3: /* slow */
					if(HFast)  u_slow_down();
					else You("pause momentarily.");
					break;
				    case 4: /* drain Dex */
					adjattrib(A_DEX, -rn1(1,1), 0, TRUE);
					break;
				    case 5: /* steal teleportitis */
					if(HTeleportation & INTRINSIC) {
					      HTeleportation &= ~INTRINSIC;
					}
			 		if (HTeleportation & TIMEOUT) {
						HTeleportation &= ~TIMEOUT;
					}
					if(HTeleport_control & INTRINSIC) {
					      HTeleport_control &= ~INTRINSIC;
					}
			 		if (HTeleport_control & TIMEOUT) {
						HTeleport_control &= ~TIMEOUT;
					}
				      You("don't feel in the mood for jumping around.");
					break;
				    case 6: /* steal sleep resistance */
					if(HSleep_resistance & INTRINSIC) {
						HSleep_resistance &= ~INTRINSIC;
					} 
					if(HSleep_resistance & TIMEOUT) {
						HSleep_resistance &= ~TIMEOUT;
					} 
					You_feel("like you could use a nap.");
					break;
				}

				break;
			case 4: /* greed */
				if (u.ugold) pline("Your purse feels lighter...");
				u.ugold /= 2;
				break;
			case 5: /* lust */
				if (invent) {
					pline("Your belongings leave your body!");
				    int itemportchance = 10 + rn2(21);
				    for (otmpi = invent; otmpi; otmpi = otmpii) {

				      otmpii = otmpi->nobj;

					if (!rn2(itemportchance) && !(objects[otmpi->otyp].oc_material == MT_BONE && rn2(10)) && !stack_too_big(otmpi) ) {

						if (otmpi->owornmask & W_ARMOR) {
						    if (otmpi == uskin) {
							skinback(TRUE);		/* uarm = uskin; uskin = 0; */
						    }
						    if (otmpi == uarm) (void) Armor_off();
						    else if (otmpi == uarmc) (void) Cloak_off();
						    else if (otmpi == uarmf) (void) Boots_off();
						    else if (otmpi == uarmg) (void) Gloves_off();
						    else if (otmpi == uarmh) (void) Helmet_off();
						    else if (otmpi == uarms) (void) Shield_off();
						    else if (otmpi == uarmu) (void) Shirt_off();
						    /* catchall -- should never happen */
						    else setworn((struct obj *)0, otmpi ->owornmask & W_ARMOR);
						} else if (otmpi ->owornmask & W_AMUL) {
						    Amulet_off();
						} else if (otmpi ->owornmask & W_IMPLANT) {
						    Implant_off();
						} else if (otmpi ->owornmask & W_RING) {
						    Ring_gone(otmpi);
						} else if (otmpi ->owornmask & W_TOOL) {
						    Blindf_off(otmpi);
						} else if (otmpi ->owornmask & (W_WEP|W_SWAPWEP|W_QUIVER)) {
						    if (otmpi == uwep)
							uwepgone();
						    if (otmpi == uswapwep)
							uswapwepgone();
						    if (otmpi == uquiver)
							uqwepgone();
						}

						if (otmpi->owornmask & (W_BALL|W_CHAIN)) {
						    unpunish();
						} else if (otmpi->owornmask) {
						/* catchall */
						    setnotworn(otmpi);
						}

						dropx(otmpi);
					      if (otmpi->where == OBJ_FLOOR) rloco(otmpi);
					}

				    }
				}
				break;
			case 6: /* envy */
				if (flags.soundok) {
					You_hear("a chuckling laughter.");
					if (PlayerHearsSoundEffects) pline(issoviet ? "Kha-kha-kha-kha-kha-KDZH KDZH, tip bloka l'da smeyetsya yego tortsa, potomu chto vy teryayete vse vashi vstroyennyye funktsii!" : "Hoehoehoehoe!");
				}
			      attrcurse();
			      attrcurse();
				break;
			case 7: /* pride */
			      pline("The RNG determines to take you down a peg or two...");
				if (!rn2(3)) {
				    poisoned("air", rn2(A_MAX), "wand of sin", 30);
				}
				if (!rn2(4)) {
					You_feel("drained...");
					u.uhpmax -= rn1(10,10);
					if (u.uhpmax < 1) u.uhpmax = 1;
					if(u.uhp > u.uhpmax) u.uhp = u.uhpmax;
				}
				if (!rn2(4)) {
					You_feel("less energised!");
					u.uenmax -= rn1(10,10);
					if (u.uenmax < 0) u.uenmax = 0;
					if(u.uen > u.uenmax) u.uen = u.uenmax;
				}
				if (!rn2(4)) {
					if(!Drain_resistance || !rn2(StrongDrain_resistance ? 16 : 4) )
					    losexp("life drainage", FALSE, TRUE);
					else You_feel("woozy for an instant, but shrug it off.");
				}
				break;
			case 8: /* depression */

			    switch(rnd(20)) {
			    case 1:
				if (!Unchanging && !Antimagic) {
					You("undergo a freakish metamorphosis!");
				      polyself(FALSE);
				}
				break;
			    case 2:
				You("need reboot.");
				if (PlayerHearsSoundEffects) pline(issoviet ? "Eto poshel na khuy vverkh. No chto zhe vy ozhidali? Igra, v kotoruyu vy mozhete legko vyigrat'? Durak!" : "DUEUEDUET!");
				if (!Race_if(PM_UNGENOMOLD)) newman();
				else polyself(FALSE);
				break;
			    case 3: case 4:
				if(!rn2(4) && u.ulycn == NON_PM &&
					!Protection_from_shape_changers &&
					!is_were(youmonst.data) &&
					!defends(AD_WERE,uwep)) {
				    You_feel("feverish.");
				    exercise(A_CON, FALSE);
				    u.ulycn = PM_WERECOW;
				} else {
					if (multi >= 0) {
					    if (Sleep_resistance && rn2(StrongSleep_resistance ? 20 : 5)) break;
					    fall_asleep(-rnd(10), TRUE);
					    You("are put to sleep!");
					}
				}
				break;
			    case 5: case 6:
				pline("Suddenly, there's glue all over you!");
				u.utraptype = TT_GLUE;
				u.utrap = 25 + rnd(monster_difficulty());

				break;
			    case 7:
			    case 8:
				Your("position suddenly seems very uncertain!");
				teleX();
				break;
			    case 9:
				u_slow_down();
				break;
			    case 10:
			    case 11:
			    case 12:
				if ((!StrongSwimming || !rn2(10)) && (!StrongMagical_breathing || !rn2(10))) {
					water_damage(invent, FALSE, FALSE);
				}
				break;
			    case 13:
				if (multi >= 0) {
				    if (Free_action && rn2(StrongFree_action ? 100 : 20)) {
					You("momentarily stiffen.");            
				    } else {
					You("are frozen!");
					if (PlayerHearsSoundEffects) pline(issoviet ? "Teper' vy ne mozhete dvigat'sya. Nadeyus', chto-to ubivayet vas, prezhde chem vash paralich zakonchitsya." : "Klltsch-tsch-tsch-tsch-tsch!");
					nomovemsg = 0;	/* default: "you can move again" */
					nomul(-rnd(10), "paralyzed by a wand of sin", TRUE);
					exercise(A_DEX, FALSE);
				    }
				}
				break;
			    case 14:
				if (FunnyHallu)
					pline("What a groovy feeling!");
				else
					You(Blind ? "%s and get dizzy..." :
						 "%s and your vision blurs...",
						    stagger(youmonst.data, "stagger"));
				if (PlayerHearsSoundEffects) pline(issoviet ? "Imet' delo s effektami statusa ili sdat'sya!" : "Wrueue-ue-e-ue-e-ue-e...");
				dmg = rn1(7, 16);
				make_stunned(HStun + dmg + monster_difficulty(), FALSE);
				(void) make_hallucinated(HHallucination + dmg + monster_difficulty(),TRUE,0L);
				break;
			    case 15:
				if(!Blind)
					Your("vision bugged.");
				dmg += rn1(10, 25);
				dmg += rn1(10, 25);
				(void) make_hallucinated(HHallucination + dmg + monster_difficulty() + monster_difficulty(),TRUE,0L);
				break;
			    case 16:
				if(!Blind)
					Your("vision turns to screen saver.");
				dmg += rn1(10, 25);
				(void) make_hallucinated(HHallucination + dmg + monster_difficulty(),TRUE,0L);
				break;
			    case 17:
				{
				    struct obj *objD = some_armor(&youmonst);
	
				    if (objD && drain_item(objD)) {
					u.cnd_disenchantamount++;
					Your("%s less effective.", aobjnam(objD, "seem"));
					if (PlayerHearsSoundEffects) pline(issoviet ? "Vse, chto vy vladeyete budet razocharovalsya v zabveniye, kha-kha-kha!" : "Klatsch!");
				    }
				}
				break;
			    default:
				    if(Confusion)
					 You("are getting even more confused.");
				    else You("are getting confused.");
				    make_confused(HConfusion + monster_difficulty() + 1, FALSE);
				break;
			    }

				break;

		}

		}

		break;

		case WAN_IMMOBILITY:
			known = TRUE;
			{

			if (Aggravate_monster) {
				u.aggravation = 1;
				reset_rndmonst(NON_PM);
			}

			int monstcnt;
			monstcnt = 8 + rnd(10);
			int sessileattempts;
			int sessilemnum;

		      while(--monstcnt >= 0) {
				for (sessileattempts = 0; sessileattempts < 100; sessileattempts++) {
					sessilemnum = rndmonnum();
					if (sessilemnum != -1 && (mons[sessilemnum].mlet != S_TROVE) && is_nonmoving(&mons[sessilemnum]) ) sessileattempts = 100;
				}
				if (sessilemnum != -1) (void) makemon( &mons[sessilemnum], u.ux, u.uy, NO_MM_FLAGS);
			}

			u.aggravation = 0;

			}

		break;

		case WAN_EGOISM:
			known = TRUE;
			{

			if (Aggravate_monster) {
				u.aggravation = 1;
				reset_rndmonst(NON_PM);
			}

			int monstcnt;
			monstcnt = rnd(5);
			int sessileattempts;
			int sessilemnum;

		      while(--monstcnt >= 0) {
				for (sessileattempts = 0; sessileattempts < 10000; sessileattempts++) {
					sessilemnum = rndmonnum();
					if (sessilemnum != -1 && always_egotype(&mons[sessilemnum]) ) sessileattempts = 10000;
				}
				if (sessilemnum != -1) (void) makemon( &mons[sessilemnum], u.ux, u.uy, NO_MM_FLAGS);
			}

			u.aggravation = 0;

			}

		break;

		case WAN_INSANITY:

			increasesanity(rnd((level_difficulty() * 5) + 20));
			known = TRUE;
			break;

		case WAN_BAD_EQUIPMENT:

			bad_equipment(0);
			known = TRUE;
			break;

		case WAN_DRAIN_MANA:

			pline("You lose  Mana");
			drain_en(rnz(monster_difficulty() + 1) );

		break;

		case WAN_FINGER_BENDING:

			pline("Your %s bend themselves!", makeplural(body_part(FINGER)) );
			incr_itimeout(&Glib, rnd(15) + rnd(monster_difficulty() + 1) );

		break;

		case WAN_TIDAL_WAVE:

			pline("A sudden geyser slams into you from nowhere!");
			if (PlayerHearsSoundEffects) pline(issoviet ? "Teper' vse promokli. Vy zhe pomnite, chtoby polozhit' vodu chuvstvitel'nyy material v konteyner, ne tak li?" : "Schwatschhhhhh!");
			if ((!StrongSwimming || !rn2(10)) && (!StrongMagical_breathing || !rn2(10))) {
				water_damage(invent, FALSE, FALSE);
				if (level.flags.lethe) lethe_damage(invent, FALSE, FALSE);
			}
			if (Burned) make_burned(0L, TRUE);

		break;

		case WAN_STARVATION:

			morehungry(rnd(1000));

		break;

		case WAN_CONFUSION:
		if(!Confusion) {
		    if (FunnyHallu) {
			pline("What a trippy feeling!");
		    } else if (Role_if(PM_PIRATE) || Role_if(PM_KORSAIR) || (uwep && uwep->oartifact == ART_ARRRRRR_MATEY) )
			pline("Blimey! Ye're one sheet to the wind!");
			else 
			pline("Huh, What?  Where am I?");
		}
		make_confused(HConfusion + rn1(35, 115), FALSE);

		break;

		case WAN_SLIMING:
		if (!Slimed && !flaming(youmonst.data) && !Unchanging && !slime_on_touch(youmonst.data) ) {
		    You("don't feel very well.");
		    Slimed = Race_if(PM_EROSATOR) ? 25L : 100L;
		    flags.botl = 1;
		    killer_format = KILLED_BY_AN;
		    delayed_killer = "slimed by zapping a wand of sliming";
		}

		break;

		case WAN_LYCANTHROPY:
		if (!Race_if(PM_HUMAN_WEREWOLF) && !Race_if(PM_AK_THIEF_IS_DEAD_) && !Role_if(PM_LUNATIC)) {
			u.ulycn = PM_WEREWOLF;
			You_feel("feverish.");
		}

		break;

		case WAN_LIGHT:
		case SPE_LIGHT:
			litroom(TRUE,obj);
			if (!Blind) known = TRUE;
			break;
		case WAN_SECRET_DOOR_DETECTION:
			if(!findit()) return;
			if (!Blind) known = TRUE;
			break;
		case SPE_DETECT_UNSEEN:
			if(!finditX()) return;
			if (!Blind) known = TRUE;
			break;
		case WAN_TRAP_DISARMING:

			known = TRUE;
			You_feel("out of the danger zone.");
			{
				int rtrap;
				struct trap *ttmp;
	
			    int i, j, bd = 1;
	
			      for (i = -bd; i <= bd; i++) for(j = -bd; j <= bd; j++) {
	
					if ((ttmp = t_at(u.ux + i, u.uy + j)) != 0) {
					    if (ttmp->ttyp == MAGIC_PORTAL) continue;
						deltrap(ttmp);
					}
	
				}
			}

			(void) doredraw();
			break;
		case WAN_CREATE_MONSTER:
			known = create_critters(rn2(23) ? 1 : rn1(7,2),
					(struct permonst *)0);
			break;
		case WAN_CREATE_FAMILIAR:
			known = TRUE;
			(void) make_familiar((struct obj *)0, u.ux, u.uy, FALSE);
			break;
		case WAN_SUMMON_ELM:
			known = TRUE;

			{
			int aligntype;
			aligntype = rn2((int)A_LAWFUL+2) - 1;
			pline("A servant of %s appears!",aligns[1 - aligntype].noun);
			summon_minion(aligntype, TRUE);
			}

			break;
		case WAN_SUMMON_UNDEAD:
			known = TRUE;
			coord mm;
			mm.x = u.ux;
			mm.y = u.uy;
			pline("You summon some undead creatures!");   
			mkundeadX(&mm, FALSE, NO_MINVENT);
			break;
		case WAN_CREATE_HORDE:
			known = create_critters(rn1(7,6), (struct permonst *)0);
			break;
		case WAN_WISHING:
			known = TRUE;
			if ((Luck + rn2(5) < 0) && !RngeWishImprovement && !(uarmc && itemhasappearance(uarmc, APP_WISHFUL_CLOAK)) ) {
				makenonworkingwish();
				break;
			}
			makewish(TRUE);
			break;
		case WAN_ACQUIREMENT:
			known = TRUE;
			int acquireditem;
			acquireditem = 0;
			if ((Luck + rn2(5) < 0) && !RngeWishImprovement && !(uarmc && itemhasappearance(uarmc, APP_WISHFUL_CLOAK)) ) {
				pline("Unfortunately, nothing happens.");
				break;
			}

			pline("Pick an item type that you want to acquire. The prompt will loop until you actually make a choice.");

			while (acquireditem == 0) { /* ask the player what they want --Amy */

			/* Yeah, I know this is less elegant than DCSS. But hey, it's a wand of acquirement! */

				if (yn("Do you want to acquire a random item?")=='y') {
					    acqo = mkobj_at(RANDOM_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire a weapon?")=='y') {
					    acqo = mkobj_at(WEAPON_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire an armor?")=='y') {
					    acqo = mkobj_at(ARMOR_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire a ring?")=='y') {
					    acqo = mkobj_at(RING_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire an amulet?")=='y') {
					    acqo = mkobj_at(AMULET_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire an implant?")=='y') {
					    acqo = mkobj_at(IMPLANT_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire a tool?")=='y') {
					    acqo = mkobj_at(TOOL_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire some food?")=='y') {
					    acqo = mkobj_at(FOOD_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire a potion?")=='y') {
					    acqo = mkobj_at(POTION_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire a scroll?")=='y') {
					    acqo = mkobj_at(SCROLL_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire a spellbook?")=='y') {
					    acqo = mkobj_at(SPBOOK_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire a wand?")=='y') {
					    acqo = mkobj_at(WAND_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire some coins?")=='y') {
					    acqo = mkobj_at(COIN_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire a gem?")=='y') {
					    acqo = mkobj_at(GEM_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire a boulder or statue?")=='y') {
					    acqo = mkobj_at(ROCK_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire a heavy iron ball?")=='y') {
					    acqo = mkobj_at(BALL_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire an iron chain?")=='y') {
					    acqo = mkobj_at(CHAIN_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
				else if (yn("Do you want to acquire a splash of venom?")=='y') {
					    acqo = mkobj_at(VENOM_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
	
			}

			if (!acqo) {
				pline("Unfortunately, it failed.");
				break;
			}
	
			/* special handling to prevent wands of wishing or similarly overpowered items --Amy */
	
			if (acqo->otyp == GOLD_PIECE) acqo->quan = rnd(1000);
			if (acqo->otyp == MAGIC_LAMP || acqo->otyp == TREASURE_CHEST) { acqo->otyp = OIL_LAMP; acqo->age = 1500L; }
			if (acqo->otyp == MAGIC_MARKER) acqo->recharged = 1;
		    while(acqo->otyp == WAN_WISHING || acqo->otyp == WAN_POLYMORPH || acqo->otyp == WAN_MUTATION || acqo->otyp == WAN_ACQUIREMENT)
			acqo->otyp = rnd_class(WAN_LIGHT, WAN_PSYBEAM);
		    while (acqo->otyp == SCR_WISHING || acqo->otyp == SCR_RESURRECTION || acqo->otyp == SCR_ACQUIREMENT || acqo->otyp == SCR_ENTHRONIZATION || acqo->otyp == SCR_MAKE_PENTAGRAM || acqo->otyp == SCR_FOUNTAIN_BUILDING || acqo->otyp == SCR_SINKING || acqo->otyp == SCR_WC)
			acqo->otyp = rnd_class(SCR_CREATE_MONSTER, SCR_BLANK_PAPER);
	
			pline("Something appeared on the ground just beneath you!");

			break;
		case WAN_ENLIGHTENMENT:
			known = TRUE;
			You_feel("self-knowledgeable...");
			display_nhwindow(WIN_MESSAGE, FALSE);
			enlightenment(FALSE, TRUE);
			pline_The("feeling subsides.");
			exercise(A_WIS, TRUE);
			break;
		case WAN_DETECT_MONSTERS:
			known = TRUE;
			    int x, y;
	
			    /* after a while, repeated uses become less effective */
			    if (HDetect_monsters >= 300L)
				i = 10;
			    else
				i = rn1(20,11);
			    incr_itimeout(&HDetect_monsters, i);
			    for (x = 1; x < COLNO; x++) {
				for (y = 0; y < ROWNO; y++) {
				    if (memory_is_invisible(x, y)) {
					unmap_object(x, y);
					newsym(x,y);
				    }
				}
			    }
			    see_monsters();

			exercise(A_WIS, TRUE);
			break;
		case WAN_OBJECTION:
			known = TRUE;
			object_detect((struct obj *)0, 0);
			exercise(A_WIS, TRUE);
			break;
		case WAN_DARKNESS:
		case SPE_DARKNESS:
			if (!Blind) known = TRUE;
			litroom(FALSE,obj);
			break;
		case SPE_AMNESIA:
			You_feel("dizzy!");
			forget(1 + rn2(5));
			break;
		case WAN_MAGIC_MAPPING:
			known = TRUE;
			pline("A map coalesces in your mind!");
			do_mappingZ();
			break;
		case WAN_STINKING_CLOUD:
		      {  coord cc;

			pline("You may place a stinking cloud on the map.");
			known = TRUE;
			pline("Where do you want to center the cloud?");
			cc.x = u.ux;
			cc.y = u.uy;
			if (getpos(&cc, TRUE, "the desired position") < 0) {
			    pline("%s", Never_mind);
				if (FailureEffects || u.uprops[FAILURE_EFFECTS].extrinsic || have_failurestone()) {
					pline("Oh wait, actually I do mind...");
					badeffect();
				}
			    return;
			}
			if (!cansee(cc.x, cc.y) || distu(cc.x, cc.y) >= 32) {
			    You("smell rotten eggs.");
			    return;
			}
			(void) create_gas_cloud(cc.x, cc.y, 3, 8);
			break;
			}
		case WAN_TELE_LEVEL:
		      if (!flags.lostsoul && !flags.uberlostsoul && !(flags.wonderland && !(u.wonderlandescape)) && !(iszapem && !(u.zapemescape)) && !(u.uprops[STORM_HELM].extrinsic) && !(In_bellcaves(&u.uz)) && !(In_subquest(&u.uz)) && !(In_voiddungeon(&u.uz)) && !(In_netherrealm(&u.uz))) level_tele();
			else pline("Hmm... that level teleport wand didn't do anything.");
			known = TRUE;
			break;
		case WAN_GENOCIDE:
			do_genocide(1);	/* REALLY, see do_genocide() */
			break;
		case WAN_TIME_STOP:
			pline((Role_if(PM_SAMURAI) || Role_if(PM_NINJA)) ? "Jikan ga teishi shimashita." : "Time has stopped.");
			TimeStopped += (3 + rnd(5));
			break;

		case WAN_ENTRAPPING:
			known = TRUE;
			trap_detect((struct obj *)0);
			exercise(A_WIS, TRUE);
			break;

		case WAN_LEVITATION:
			known = TRUE;
			incr_itimeout(&HLevitation, rnd(100) );
			pline("You float up!");
			break;

		case WAN_SPELLBINDER:
			known = TRUE;
			{
				register int spellbindings = 5;

				u.spellbinder = spellbindings;
				u.spellbinder1 = -1;
				u.spellbinder2 = -1;
				u.spellbinder3 = -1;
				u.spellbinder4 = -1;
				u.spellbinder5 = -1;
				u.spellbinder6 = -1;
				u.spellbinder7 = -1;

				pline("You may cast %d more spells.", u.spellbinder);

				while (u.spellbinder) {
					docast();
					u.spellbinder--;
				}

			}

			break;

		case WAN_INERTIA_CONTROL:
			known = TRUE;

			pline("Inertia control allows you to automatically cast a spell every turn for a while. You can choose which spell you want to control.");

			if (spellid(0) == NO_SPELL)  {
				You("don't know any spells, and therefore inertia control fails.");
				break;
			}

			{
				int numspells;

				for (numspells = 0; numspells < MAXSPELL && spellid(numspells) != NO_SPELL; numspells++) {
					if (spellid(numspells) == SPE_INERTIA_CONTROL) continue;

					pline("You know the %s spell.", spellname(numspells));
					if (yn("Control the flow of this spell?") == 'y') {
						u.inertiacontrolspell = spellid(numspells);
						u.inertiacontrolspellno = numspells;

						u.inertiacontrol = 50;

						break;
					}
				}
			}

			break;

		case WAN_STERILIZE:
			known = TRUE;

			You_feel("an anti-sexual aura.");
			u.sterilized = 20 + rnd(60);

			break;

		case WAN_DEBUGGING:
			known = TRUE;
			You("need reboot.");
			if (PlayerHearsSoundEffects) pline(issoviet ? "Eto poshel na khuy vverkh. No chto zhe vy ozhidali? Igra, v kotoruyu vy mozhete legko vyigrat'? Durak!" : "DUEUEDUET!");
			if (!Race_if(PM_UNGENOMOLD)) newman();
			else polyself(FALSE);
			break;

		case WAN_IDENTIFY:
			known = TRUE;
			You_feel("insightful!");
			if (invent) {
			    /* rn2(5) agrees w/seffects() */
			    if (issoviet) pline("Sovet reshil, chto etot vopros byl slishkom silen, khotya eto ne bylo, poetomu on identifitsiruyet tol'ko odin punkt kazhdyy raz.");
			    identify_pack(issoviet ? 1 : rn2(5), 0);
			}
			exercise(A_WIS, TRUE);
			break;
		case WAN_MANA:
			known = TRUE;
			You_feel("full of mystic power!");
			if (!rn2(20)) u.uen += (400 + rnz(u.ulevel));
			else if (!rn2(5)) u.uen += (d(6,8) + rnz(u.ulevel));
			else u.uen += (d(5,6) + rnz(u.ulevel));
			if (u.uen > u.uenmax) u.uen = u.uenmax;
			break;
		case WAN_REMOVE_CURSE:
			known = TRUE;
			You_feel("like someone is helping you!");
			if (PlayerHearsSoundEffects) pline(issoviet ? "Ba, tip bloka l'da budet proklinat' svoye der'mo snova tak ili inache." : "Daedeldaedimm!");

			/* remove all contamination --Amy */
			if (u.contamination) decontaminate(u.contamination);
			if (uinsymbiosis) uncursesymbiote(FALSE);

			register struct obj *obj;

			for(obj = invent; obj ; obj = obj->nobj) {

				long wornmask;
				if (obj->oclass == COIN_CLASS) continue;
				wornmask = (obj->owornmask & ~(W_BALL|W_ART|W_ARTI));
				if (wornmask) {
				    /* handle a couple of special cases; we don't
				       allow auxiliary weapon slots to be used to
				       artificially increase number of worn items */
				    if (obj == uswapwep) {
					if (!u.twoweap) wornmask = 0L;
				    } else if (obj == uquiver) {
					if (obj->oclass == WEAPON_CLASS) {
					    /* mergeable weapon test covers ammo,
					       missiles, spears, daggers & knives */
					    if (!objects[obj->otyp].oc_merge) 
						wornmask = 0L;
					} else if (obj->oclass == GEM_CLASS) {
					    /* possibly ought to check whether
					       alternate weapon is a sling... */
					    if (!uslinging()) wornmask = 0L;
					} else {
					    /* weptools don't merge and aren't
					       reasonable quivered weapons */
					    wornmask = 0L;
					}
				    }
				}

				if ( (!rn2(5) || wornmask) && obj->cursed && !stack_too_big(obj))
					uncurse(obj, FALSE);
			}

			break;
		case WAN_PUNISHMENT:
			known = TRUE;
			You_feel("someone is punishing you for your misbehavior!");
			punishx();
			break;
		case WAN_CHARGING:
			known = TRUE;
			pline("This is a charging wand.");
chargingchoice:
			otmp = getobj(all_count, "charge");
			if (!otmp) {
				if (yn("Really exit with no object selected?") == 'y')
					pline("You just wasted the opportunity to charge your items.");
				else goto chargingchoice;
				break;
			}
			recharge(otmp, 1);
			break;
		case WAN_WONDER: /* supposed to have a random effect, may be implemented in future */

			known = TRUE;
			switch(rnd(21)) {
			case 1 : 
				litroomlite(TRUE);
				break;
			case 2 : 
				if(!findit()) return;
				break;
			case 3 : 
				create_critters(rn2(23) ? 1 : rn1(7,2), (struct permonst *)0);
				break;
			case 4 : 
				create_critters(rn1(7,6), (struct permonst *)0);
				break;
			case 5 : 
				pline("Multicolored sparks fly from the wand.");
				if (!rn2(250)) {
					if (!rn2(4)) makewish(TRUE);
					else othergreateffect();
				}
		/* since there is a 1% chance of the wand exploding, this _should_ be okay --Amy */
				break;
			case 6 : 
				pline("Nothing happens.");
				if (FailureEffects || u.uprops[FAILURE_EFFECTS].extrinsic || have_failurestone()) {
					pline("Oh wait, actually something bad happens...");
					badeffect();
				}
				break;
			case 7 : 
				You_feel("self-knowledgeable...");
				display_nhwindow(WIN_MESSAGE, FALSE);
				enlightenment(FALSE, TRUE);
				pline_The("feeling subsides.");
				exercise(A_WIS, TRUE);
				break;
			case 8 : 
			    { int x, y;
	
			    /* after a while, repeated uses become less effective */
			    if (HDetect_monsters >= 300L)
				i = 10;
			    else
				i = rn1(20,11);
			    incr_itimeout(&HDetect_monsters, i);
			    for (x = 1; x < COLNO; x++) {
				for (y = 0; y < ROWNO; y++) {
				    if (memory_is_invisible(x, y)) {
					unmap_object(x, y);
					newsym(x,y);
				    }
				}
			    }
			    see_monsters();

				exercise(A_WIS, TRUE);
				break;
				}
			case 9 : 
				object_detect((struct obj *)0, 0);
				exercise(A_WIS, TRUE);
				break;
			case 10 : 
				trap_detect((struct obj *)0);
				exercise(A_WIS, TRUE);
				break;
			case 11 : 
				You_feel("insightful!");
				if (invent) {
				    /* rn2(5) agrees w/seffects() */
				    identify_pack(rn2(5), 0);
				}
				exercise(A_WIS, TRUE);
				break;
			case 12 : 
				You_feel("like someone is helping you!");
				if (PlayerHearsSoundEffects) pline(issoviet ? "Ba, tip bloka l'da budet proklinat' svoye der'mo snova tak ili inache." : "Daedeldaedimm!");

				/* remove all contamination --Amy */
				if (u.contamination) decontaminate(u.contamination);
				if (uinsymbiosis) uncursesymbiote(FALSE);

				register struct obj *obj;
	
				for(obj = invent; obj ; obj = obj->nobj)
					if (!rn2(5) && obj->cursed && !stack_too_big(obj))	uncurse(obj, FALSE);
	
				break;
			case 13 : 
				You_feel("someone is punishing you for your misbehavior!");
				punishx();
				break;
			case 14 : 
			case 15 : 
			case 16 : 
			case 17 : 
			case 18 : 
			case 19 : 
			case 20 : 
			default:
				pline("Nothing happens.");
				if (FailureEffects || u.uprops[FAILURE_EFFECTS].extrinsic || have_failurestone()) {
					pline("Oh wait, actually something bad happens...");
					badeffect();
				}
				break;
			}
			break;
		case WAN_BUGGING:
			known = TRUE; 
			{int i;

			i = rn2(6) + 1;
			while (i--) {
				(void) makemon(&mons[rn2(3) ? PM_BUG : PM_HEISENBUG], u.ux,u.uy, NO_MM_FLAGS);
					}
			} break;
	}
	if (obj && known && !objects[obj->otyp].oc_name_known) {
		makeknown(obj->otyp);
		more_experienced(0,10);
	}
}
#endif /*OVL1*/
#ifdef OVL0

STATIC_OVL void
backfire(otmp)
struct obj *otmp;
{
	otmp->in_use = TRUE;	/* in case losehp() is fatal */
	/* KMH, balance patch -- spells (not spellbooks) explode */
	if (otmp->oclass != SPBOOK_CLASS)
	pline("%s suddenly explodes!", The(xname(otmp)));
/*        losehp(d(otmp->spe+2,6), "exploding wand", KILLED_BY_AN);
        useup(otmp);*/
	wand_explode (otmp, FALSE);
}

static NEARDATA const char zap_syms[] = { WAND_CLASS, 0 };

int
dozap()
{

	register struct obj *obj;
	int	damage;

	if (u.powerfailure || (isselfhybrid && (moves % 3 == 0 && moves % 11 != 0) )) {
		pline("Your power's down, and therefore you cannot zap anything.");
		if (flags.moreforced && !MessagesSuppressed) display_nhwindow(WIN_MESSAGE, TRUE);    /* --More-- */
		return 0;
	}

	if (isevilvariant && !freehandX()) {
		You("have no free %s and therefore cannot zap wands!", body_part(HAND));
		if (flags.moreforced && !MessagesSuppressed) display_nhwindow(WIN_MESSAGE, TRUE);    /* --More-- */
		return 0;
	}

	if(check_capacity((char *)0)) return(0);
	obj = getobj(zap_syms, "zap");
	if(!obj) return(0);

	if (CurseAsYouUse && obj && obj->otyp != CANDELABRUM_OF_INVOCATION && obj->otyp != SPE_BOOK_OF_THE_DEAD && obj->otyp != BELL_OF_OPENING) curse(obj);

	if (InterruptEffect || u.uprops[INTERRUPT_EFFECT].extrinsic || have_interruptionstone()) {
		nomul(-(rnd(5)), "zapping a wand", TRUE);
	}

	check_unpaid(obj);

	if (Race_if(PM_STICKER)) {
		if (!(uwep && uwep == obj)) {
			pline("You must wield this item first if you want to zap it!"); 
			if (flags.moreforced && !MessagesSuppressed) display_nhwindow(WIN_MESSAGE, TRUE);    /* --More-- */
			wield_tool(obj, "hold");
			return 1;
		}

	}

	/* zappable addition done by GAN 11/03/86 */
	if(!zappable(obj)) {
		pline("%s", nothing_happens);
		if (FailureEffects || u.uprops[FAILURE_EFFECTS].extrinsic || have_failurestone()) {
			pline("Oh wait, actually something bad happens...");
			badeffect();
		}
		if (flags.moreforced && !MessagesSuppressed) display_nhwindow(WIN_MESSAGE, TRUE);    /* --More-- */
	}

	else if(obj->otyp == WAN_MISFIRE) {
		backfire(obj);  /* the wand blows up in your face! */
		exercise(A_STR, FALSE);
		return(1);
	}

	else if(obj->cursed && !rn2(5)) {
		/* WAC made this rn2(5) from rn2(100)*/
		backfire(obj);  /* the wand blows up in your face! */
		exercise(A_STR, FALSE);
		return(1);
	}

	/* evil patch idea by jonadab: eroded wands have a chance of exploding */
	else if ( (obj->oeroded > 2 || (obj->oeroded2 > 2 && !(objects[(obj)->otyp].oc_material == MT_COMPOST) ) ) && !rn2(5) ) {
		backfire(obj);  /* the wand blows up in your face! */
		exercise(A_STR, FALSE);
		return(1);

	}
	else if ( (obj->oeroded > 1 || (obj->oeroded2 > 1 && !(objects[(obj)->otyp].oc_material == MT_COMPOST) ) ) && !rn2(20) ) {
		backfire(obj);  /* the wand blows up in your face! */
		exercise(A_STR, FALSE);
		return(1);

	}
	else if ( (obj->oeroded > 0 || (obj->oeroded2 > 0 && !(objects[(obj)->otyp].oc_material == MT_COMPOST) ) ) && !rn2(80) ) {
		backfire(obj);  /* the wand blows up in your face! */
		exercise(A_STR, FALSE);
		return(1);

	}

	/* evil patch idea by jondab: zapping a wand while impaired can cause it to explode */
	else if ( Stunned && !rn2(StrongStun_resist ? 2000 : Stun_resist ? 200 : 20) ) {
		backfire(obj);
		exercise(A_STR, FALSE);
		return(1);
	}
	else if ( Confusion && !rn2(StrongConf_resist ? 15000 : Conf_resist ? 1500 : 150) ) {
		backfire(obj);
		exercise(A_STR, FALSE);
		return(1);
	}
	else if ( Numbed && !rn2(500) ) {
		backfire(obj);
		exercise(A_STR, FALSE);
		return(1);
	}
	else if ( Feared && !rn2(500) ) {
		backfire(obj);
		exercise(A_STR, FALSE);
		return(1);
	}
	else if ( Frozen && !rn2(30) ) {
		backfire(obj);
		exercise(A_STR, FALSE);
		return(1);
	}
	else if ( Burned && !rn2(300) ) {
		backfire(obj);
		exercise(A_STR, FALSE);
		return(1);
	}
	else if ( Dimmed && !rn2(1000) ) {
		backfire(obj);
		exercise(A_STR, FALSE);
		return(1);
	}
	else if ( Blind && !rn2(200) ) {
		backfire(obj);
		exercise(A_STR, FALSE);
		return(1);
	}
	else if ((MagicDeviceEffect || u.uprops[MAGIC_DEVICE_BUG].extrinsic || have_magicdevicestone()) && !rn2(10)) {
		backfire(obj);
		exercise(A_STR, FALSE);
		return(1);
	}


	else if(obj->otyp == WAN_WONDER && !rn2(100)) {
		backfire(obj);  /* the wand blows up in your face! */
		exercise(A_STR, FALSE);
		return(1);
    /* WAC wands of lightning will also explode in your face*/
    } else if ((obj->otyp == WAN_LIGHTNING || obj->otyp == SPE_LIGHTNING)
                   && (Underwater) && (!obj->blessed))   {
		backfire(obj);	/* the wand blows up in your face! */
		exercise(A_STR, FALSE);
		return(1);
	} else
glowandfadechoice:
	  if(!(objects[obj->otyp].oc_dir == NODIR) && !getdir((char *)0)) {

		if (yn("Do you really want to input no direction?") == 'y') {
			if (!Blind)
			    pline("%s glows and fades.", The(xname(obj)));
		} else {
			goto glowandfadechoice;
		}
		/* make him pay for knowing !NODIR */
	} else if(!u.dx && !u.dy && !u.dz && !(objects[obj->otyp].oc_dir == NODIR)) {
	    if ((damage = zapyourself(obj, TRUE)) != 0) {
		char buf[BUFSZ];
		sprintf(buf, "zapped %sself with a wand", uhim());
		losehp(damage, buf, NO_KILLER_PREFIX);
	    }
	} else {

		/*	Are we having fun yet?
		 * weffects -> buzz(obj->otyp) -> zhitm (temple priest) ->
		 * attack -> hitum -> known_hitum -> ghod_hitsu ->
		 * buzz(AD_ELEC) -> destroy_item(WAND_CLASS) ->
		 * useup -> obfree -> dealloc_obj -> free(obj)
		 */
		current_wand = obj;
		weffects(obj);
		obj = current_wand;
		current_wand = 0;
	}
	if (obj && obj->spe < 0) {
	    pline("%s to dust.", Tobjnam(obj, "turn"));
	    useup(obj);
	}
	update_inventory();	/* maybe used a charge */
	return(1);
}


/* WAC lightning strike effect. */
void
zap_strike_fx(x,y, type)
xchar x,y;
int type;
{
        xchar row;
        boolean otherrow = FALSE;

        if (!cansee(x, y) || IS_STWALL(levl[x][y].typ))
                return;
        /* Zapdir is always downward ;B */
        tmp_at(DISP_BEAM_ALWAYS, zapdir_to_glyph(0, -1, abs(type) % 10));

        for (row = 0; row <= y; row++){
                tmp_at(x,row);
                if (otherrow) delay_output();
                otherrow = (!otherrow);
        }
        delay_output();
        tmp_at(DISP_END, 0);
}


void
throwstorm(obj, skilldmg, min, range)
register struct obj	*obj;
int min, range, skilldmg;
{
	boolean didblast = FALSE;
	int failcheck;
	int sx, sy;
	int type = (obj->otyp == SPE_FIREWORKS) ? ZT_SPELL(SPE_FIREBALL - SPE_MAGIC_MISSILE) : ZT_SPELL(obj->otyp - SPE_MAGIC_MISSILE);
	int expl_type;
	int num = 2 + rn2(3);
	if (obj->otyp == SPE_FIREWORKS) num = 2 + rn2(8);

	if (obj->otyp != SPE_FIREWORKS && tech_inuse(T_SIGIL_CONTROL)) {
	    throwspell();
	    sx = u.dx; sy = u.dy;
	} else {
	    sx = u.ux; sy = u.uy;
	}


	if (sx != u.ux || sy != u.uy) min = 0;
	if (tech_inuse(T_SIGIL_DISCHARGE)) num *= 2;

/*WAC Based off code that used to be the wizard patch mega fireball */
	if (u.uinwater) {
	    pline("You joking! In this weather?");
	    return; 
	} else if (Is_waterlevel(&u.uz)) { 
	    You("better wait for the sun to come out.");
	    return;
	}
	switch (abs(type) % 10)
	{
	case ZT_MAGIC_MISSILE:
	case ZT_DEATH:
	case ZT_SPC2:
	    expl_type = EXPL_MAGICAL;
	    break;
	case ZT_FIRE:
	case ZT_LIGHTNING:
	case ZT_LITE:
	    expl_type = EXPL_FIERY;
	    break;
	case ZT_COLD:
	    expl_type = EXPL_FROSTY;
	    break;
	case ZT_SLEEP:
	case ZT_POISON_GAS:
	case ZT_ACID:
	    expl_type = EXPL_NOXIOUS;
	    break;
	}
	while(num--) {
	    failcheck = 0;
	    do {
		confdir(); /* Random Dir */
		u.dx *= (rn2(range) + min);
		u.dx += sx;
		u.dy *= (rn2(range) + min);
		u.dy += sy;
		failcheck++;
	    } while (failcheck < 3 &&
		    (!cansee(u.dx, u.dy) || IS_STWALL(levl[u.dx][u.dy].typ)));
	    if (failcheck >= 3)
		continue;
	    if (abs(type) % 10 == ZT_LIGHTNING)
		zap_strike_fx(u.dx,u.dy,type);
	    explode(u.dx, u.dy, type, u.ulevel/4 + 1 + skilldmg, 0, expl_type);
	    delay_output();
	    didblast = TRUE;
	}
	if (!didblast) {
	    explode(u.dx, u.dy, type, u.ulevel/2 + 1 + skilldmg, 0, expl_type);
	    delay_output();
	}
	return;
}

int
zapyourself(obj, ordinary)
struct obj *obj;
boolean ordinary;
{
	int	damage = 0;
	char buf[BUFSZ];

	switch(obj->otyp) {
		case SPE_LOCK_MANIPULATION:
			break; /* do nothing */

		case WAN_STRIKING:
		    makeknown(WAN_STRIKING);
		case SPE_FORCE_BOLT:
		    if(Antimagic && rn2(StrongAntimagic ? 20 : 5)) {
			shieldeff(u.ux, u.uy);
			pline("Boing!");
		    } else {
			if (ordinary) {
			    You("bash yourself!");
			    damage = d(2,12);
			} else
			    damage = d(1 + obj->spe,6);
			exercise(A_STR, FALSE);
		    }
		    break;

		case WAN_BUBBLEBEAM:
		    makeknown(WAN_BUBBLEBEAM);
		case SPE_BUBBLEBEAM:

			if (!Swimming && !Amphibious && !Breathless) {
				pline("You're immersed in water bubbles and can't breathe!");
			      damage = d(4,12);
			} else pline("You're immersed in harmless water bubbles.");

			break;

		case SPE_BLINDING_RAY:
		      if (!resists_blnd(&youmonst)) {
				You(are_blinded_by_the_flash);
				make_blinded(Blinded + 25, FALSE);
				if (!Blind) Your("%s", vision_clears);
		      }
			break;
		case SPE_MANA_BOLT:
		case SPE_SNIPER_BEAM:
			pline("You are irradiated with energy!");
		      damage = d(2, 4);
			break;
		case SPE_ENERGY_BOLT:
			pline("You are irradiated with energy!");
		      damage = d(8, 4);
			break;

		case WAN_DREAM_EATER: /* does not self-identify because it has no effect */
		case SPE_DREAM_EATER:

			if (multi >= 0) { /* should always be the case --Amy */
				pline("You are unaffected.");
			} else {
				pline("Your dream is eaten!");
			      damage = d(10, 10);
			}

			break;

		case SPE_BATTERING_RAM:

			break;

		case SPE_GIANT_FOOT:

			pline("A giant foot falls on top of you!");
			damage = d(10, 10);

			if (amorphous(youmonst.data)) damage /= rn1(3, 3);
			if (noncorporeal(youmonst.data)) damage = 0;
			if (is_whirly(youmonst.data)) damage /= rnd(3);

			if (!damage) pline("But you're unaffected.");

			break;

		case SPE_ARMOR_SMASH:

			{

				register struct obj *asitem;
				asitem = some_armor(&youmonst);

				if (!asitem) {
					pline("Your skin itches.");
				} else if (asitem && asitem->blessed && rn2(5)) pline("Your body shakes violently!");
				else if (asitem && (asitem->spe > 1) && (rn2(asitem->spe)) ) pline("Your body shakes violently!");
				else if (asitem && asitem->oartifact && rn2(20)) pline("Your body shakes violently!");
				else if (asitem && asitem->greased) {
					pline("Your body shakes violently!");
					 if (!rn2(2) || (isfriday && !rn2(2))) {
						pline_The("grease wears off.");
						asitem->greased -= 1;
						update_inventory();
					 }
				} else if (!destroy_arm(asitem)) {
					pline("Your skin itches.");
				}
			}

			break;

		case SPE_BLOOD_STREAM:

			You("drown yourself in your own menstruation!");
			damage = d(10, 10);

			break;

		case SPE_SHINING_WAVE:

			You("have to attack with destruct wave.");
			damage = d(10, 10);

			break;

		case SPE_STRANGLING:

			You("attempt to strangle yourself.");
			damage = d(5, 5);

			break;

		case SPE_PARTICLE_CANNON:

			You("irradiate yourself with laser beams!");
			damage = d(10, 10);

			break;

		case SPE_NERVE_POISON:

			You("inject the poison into your veins.");
			nomul(-5, "paralyzed by their own nerve poison", TRUE);
			poisoned("The spell", rn2(A_MAX), "nerve poison", 30);

			break;

		case SPE_GEYSER:

			pline("A sudden geyser slams into you from nowhere!");
			if (PlayerHearsSoundEffects) pline(issoviet ? "Teper' vse promokli. Vy zhe pomnite, chtoby polozhit' vodu chuvstvitel'nyy material v konteyner, ne tak li?" : "Schwatschhhhhh!");
			damage = d(8, 6);
			if ((!StrongSwimming || !rn2(10)) && (!StrongMagical_breathing || !rn2(10))) {
				water_damage(invent, FALSE, FALSE);
				if (level.flags.lethe) lethe_damage(invent, FALSE, FALSE);
			}
			if (Burned) make_burned(0L, TRUE);

			break;

		case SPE_BUBBLING_HOLE:

			pline("Oh no, you are engulfed in a bubbling hole!");
			damage = d(10, 20);

			break;

		case SPE_VOLT_ROCK:

			damage = 0;
			if (!uarmh) {
				pline("A rock falls on your head! Ouch!");
				damage += 10;
			}
			if (!Shock_resistance) {
				pline("You are electrocuted!");
				damage += 10;	
			}
			break;

		case SPE_WATER_FLAME:
			damage = 2;
			if (Cold_resistance) {
				pline("You are burned by the watery flame!");
				damage += rn1(10,10);	
			}
			if (Fire_resistance) {
				pline("You are cooled by the watery flame!");
				damage += rn1(10,10);	
			}

			break;

		case WAN_GOOD_NIGHT:
		    makeknown(WAN_GOOD_NIGHT);
		case SPE_GOOD_NIGHT:
		    damage = d(2,12);
		    pline("Good night, %s!", playeraliasname);
		    if (u.ualign.type == A_LAWFUL) damage *= 2;
		    if (u.ualign.type == A_CHAOTIC) damage /= 2;
		    if (nonliving(youmonst.data)) damage /= 2;

			break;

		case WAN_GRAVITY_BEAM:
		    makeknown(WAN_GRAVITY_BEAM);
		case SPE_GRAVITY_BEAM:
			pline("Gravity warps around you!");
		      damage = d(6, 12);
			exercise(A_STR, FALSE);
			phase_door(0);
			pushplayer(TRUE);
		    break;

		/*WAC Add Spell Acid, Poison*/
		case SPE_POISON_BLAST:
		    poisoned("blast", A_DEX, "poisoned blast", 15);
		    break;
		case SPE_ACID_STREAM:
        	    /* KMH, balance patch -- new intrinsic */
        	    if (Acid_resistance && rn2(StrongAcid_resistance ? 20 : 5)) {
			damage = 0;
        	    } else {
			pline_The("acid burns!");
			damage = d(12,6);
			exercise(A_STR, FALSE);
        	    }

		if (Stoned) fix_petrification();

        	    if (!rn2(u.twoweap ? 3 : 6)) erode_obj(uwep, TRUE, TRUE);
        	    if (u.twoweap && !rn2(3)) erode_obj(uswapwep, TRUE, TRUE);
        	    if (!rn2(6)) erode_armor(&youmonst, TRUE);
        	    break;
		case WAN_ACID:
		    makeknown(WAN_ACID);
		case SHADOW_HORN:
		if (Stoned) fix_petrification();
		    if (Acid_resistance && rn2(StrongAcid_resistance ? 20 : 5)) {
			shieldeff(u.ux,u.uy);
			pline("Ugh!");
		    } else {
			pline("The acid burns!");
			damage = d(8,6);
		   }
		   break;

		case WAN_SLUDGE:
		    makeknown(WAN_SLUDGE);
		case SPE_SLUDGE:
		if (Stoned) fix_petrification();
		    if (Acid_resistance && rn2(StrongAcid_resistance ? 20 : 5)) {
			shieldeff(u.ux,u.uy);
			pline("Ugh!");
		    } else {
			pline("The sludge burns!");
			damage = d(16,6);
		   }

			{
			    register struct obj *objX, *objX2;
			    for (objX = invent; objX; objX = objX2) {
			      objX2 = objX->nobj;
				if (!rn2(5)) rust_dmg(objX, xname(objX), 3, TRUE, &youmonst);
			    }
			}

		   break;

		case SPE_SOLAR_BEAM:
			You("irradiate yourself!");
			damage = d(12,8);
        	    break;
		case WAN_SOLAR_BEAM:
		    makeknown(WAN_SOLAR_BEAM);
			You("irradiate yourself!");
			damage = d(8,8);
		   break;

		case WAN_AURORA_BEAM:
		    makeknown(WAN_AURORA_BEAM);
		case SPE_AURORA_BEAM:
			You("irradiate yourself!");
			damage = d(24,8);
		    (void) cancel_monst(&youmonst, obj, TRUE, FALSE, TRUE);
		   break;

		case SPE_PSYBEAM:
			if (Psi_resist && rn2(StrongPsi_resist ? 100 : 20) ) {
				pline("You blast yourself with soothing psychic energy.");
				break;
			}
			You("blast yourself with psychic energy!");
			damage = d(12,7);

			switch (rnd(10)) {

				case 1:
				case 2:
				case 3:
					make_confused(HConfusion + damage, FALSE);
					break;
				case 4:
				case 5:
				case 6:
					make_stunned(HStun + damage, FALSE);
					break;
				case 7:
					make_confused(HConfusion + damage, FALSE);
					make_stunned(HStun + damage, FALSE);
					break;
				case 8:
					make_hallucinated(HHallucination + damage, FALSE, 0L);
					break;
				case 9:
					make_feared(HFeared + damage, FALSE);
					break;
				case 10:
					make_numbed(HNumbed + damage, FALSE);
					break;
	
			}
			if (!rn2(200)) {
				forget(rnd(5));
				pline("You forget some important things...");
			}
			if (!rn2(200)) {
				losexp("psionic drain", FALSE, TRUE);
			}
			if (!rn2(200)) {
				adjattrib(A_INT, -1, 1, TRUE);
				adjattrib(A_WIS, -1, 1, TRUE);
			}
			if (!rn2(200)) {
				pline("You scream in pain!");
				wake_nearby();
			}
			if (!rn2(200)) {
				badeffect();
			}
			if (!rn2(5)) increasesanity(rnz(5));

        	    break;
		case WAN_PSYBEAM:
		    makeknown(WAN_PSYBEAM);
			if (Psi_resist && rn2(StrongPsi_resist ? 100 : 20) ) {
				pline("You blast yourself with soothing psychic energy.");
				break;
			}
			You("blast yourself with psychic energy!");
			damage = d(8,7);

			switch (rnd(10)) {

				case 1:
				case 2:
				case 3:
					make_confused(HConfusion + damage, FALSE);
					break;
				case 4:
				case 5:
				case 6:
					make_stunned(HStun + damage, FALSE);
					break;
				case 7:
					make_confused(HConfusion + damage, FALSE);
					make_stunned(HStun + damage, FALSE);
					break;
				case 8:
					make_hallucinated(HHallucination + damage, FALSE, 0L);
					break;
				case 9:
					make_feared(HFeared + damage, FALSE);
					break;
				case 10:
					make_numbed(HNumbed + damage, FALSE);
					break;
	
			}
			if (!rn2(200)) {
				forget(rnd(5));
				pline("You forget some important things...");
			}
			if (!rn2(200)) {
				losexp("psionic drain", FALSE, TRUE);
			}
			if (!rn2(200)) {
				adjattrib(A_INT, -1, 1, TRUE);
				adjattrib(A_WIS, -1, 1, TRUE);
			}
			if (!rn2(200)) {
				pline("You scream in pain!");
				wake_nearby();
			}
			if (!rn2(200)) {
				badeffect();
			}
			if (!rn2(5)) increasesanity(rnz(5));

		   break;

		case WAN_NETHER_BEAM:
		    makeknown(WAN_NETHER_BEAM);
		case SPE_NETHER_BEAM:

			if (Upolyd && u.mh > 1) u.mh /= 2;
			else if (!Upolyd && u.uhp > 1) u.uhp /= 2;
			losehp(1, "their own nether beam", KILLED_BY);

			if (Psi_resist && rn2(StrongPsi_resist ? 100 : 20) ) {
				pline("You blast yourself with nether energy.");
				break;
			}
			You("blast yourself with nether energy!");
			damage = d(16,7);

			switch (rnd(10)) {

				case 1:
				case 2:
				case 3:
					make_confused(HConfusion + damage, FALSE);
					break;
				case 4:
				case 5:
				case 6:
					make_stunned(HStun + damage, FALSE);
					break;
				case 7:
					make_confused(HConfusion + damage, FALSE);
					make_stunned(HStun + damage, FALSE);
					break;
				case 8:
					make_hallucinated(HHallucination + damage, FALSE, 0L);
					break;
				case 9:
					make_feared(HFeared + damage, FALSE);
					break;
				case 10:
					make_numbed(HNumbed + damage, FALSE);
					break;
	
			}
			if (!rn2(200)) {
				forget(rnd(5));
				pline("You forget some important things...");
			}
			if (!rn2(200)) {
				losexp("nether drain", FALSE, TRUE);
			}
			if (!rn2(200)) {
				adjattrib(A_INT, -1, 1, TRUE);
				adjattrib(A_WIS, -1, 1, TRUE);
			}
			if (!rn2(200)) {
				pline("You scream in pain!");
				wake_nearby();
			}
			if (!rn2(200)) {
				badeffect();
			}
			if (!rn2(5)) increasesanity(rnz(5));

		   break;

		case SPE_CHROMATIC_BEAM:
			You("blast yourself with magical energy!");
			damage = d(15,10);
		   break;
		case SPE_ELEMENTAL_BEAM:
			You("blast yourself with elemental energy!");
			damage = d(12,10);
		   break;
		case SPE_NATURE_BEAM:
			You("blast yourself with elemental energy!");
			damage = d(12,20);
		   break;
		case WAN_CHROMATIC_BEAM:
		    makeknown(WAN_CHROMATIC_BEAM);
			You("blast yourself with magical energy!");
			damage = d(10,10);
		   break;
		case WAN_POISON:
		case WAN_VENOM_SCATTERING:
		    makeknown(obj->otyp);
		case CHROME_HORN:
			You("poison yourself!");
			if (Poison_resistance && rn2(StrongPoison_resistance ? 20 : 5)) {
			  shieldeff(u.ux,u.uy);
			} else {
			  damage = d(7,7);
			  poisoned("blast", A_DEX, "poisoned blast", 15);
			}
		   break;

		case WAN_TOXIC:
		    makeknown(WAN_TOXIC);
		case SPE_TOXIC:
			You("badly poison yourself!");
			if (Poison_resistance && rn2(StrongPoison_resistance ? 20 : 5)) {
			  shieldeff(u.ux,u.uy);
			} else {
			  damage = d(14,7);
			  poisoned("blast", A_DEX, "toxic blast", 15);
			}
			if (!rn2( (Poison_resistance && rn2(StrongPoison_resistance ? 20 : 5) ) ? 20 : 4 )) (void) adjattrib(A_STR, -rnd(2), FALSE, TRUE);
			if (!rn2( (Poison_resistance && rn2(StrongPoison_resistance ? 20 : 5) ) ? 20 : 4 )) (void) adjattrib(A_DEX, -rnd(2), FALSE, TRUE);
			if (!rn2( (Poison_resistance && rn2(StrongPoison_resistance ? 20 : 5) ) ? 20 : 4 )) (void) adjattrib(A_CON, -rnd(2), FALSE, TRUE);
			if (!rn2( (Poison_resistance && rn2(StrongPoison_resistance ? 20 : 5) ) ? 20 : 4 )) (void) adjattrib(A_INT, -rnd(2), FALSE, TRUE);
			if (!rn2( (Poison_resistance && rn2(StrongPoison_resistance ? 20 : 5) ) ? 20 : 4 )) (void) adjattrib(A_WIS, -rnd(2), FALSE, TRUE);
			if (!rn2( (Poison_resistance && rn2(StrongPoison_resistance ? 20 : 5) ) ? 20 : 4 )) (void) adjattrib(A_CHA, -rnd(2), FALSE, TRUE);

		   break;

		case WAN_CLONE_MONSTER:
		    makeknown(WAN_CLONE_MONSTER);
			You("try to clone yourself!");
			cloneu();
		   break;
		case SPE_CLONE_MONSTER:
			You("try to clone yourself!");
			cloneu();
		   break;
		case WAN_WIND:
		case SPE_WIND:
			 /* This is not usually a bright idea. */
			 You("are caught up in the winds!");
			 scatter(u.ux,u.uy,4,VIS_EFFECTS|MAY_HIT|MAY_DESTROY|MAY_FRACTURE,(struct obj*)0);
			 hurtle(rnd(3)-2,rnd(3)-2,rnd(3)+2,FALSE);
			 exercise(A_WIS,FALSE);
			 break;
		case WAN_LIGHTNING:
		    makeknown(WAN_LIGHTNING);
		case TEMPEST_HORN:
		/*WAC Added Spell Lightning*/
		case SPE_LIGHTNING:
		    if (!Shock_resistance) {
			You("shock yourself!");
			damage = d(12,6);
			exercise(A_CON, FALSE);
		    } else {
			shieldeff(u.ux, u.uy);
			You("zap yourself, but seem unharmed.");
			ugolemeffects(AD_ELEC, d(12,6));
		    }
		    if (isevilvariant || !rn2(issoviet ? 6 : 33)) /* new calculations --Amy */	destroy_item(WAND_CLASS, AD_ELEC);
		    if (isevilvariant || !rn2(issoviet ? 6 : 33)) /* new calculations --Amy */	destroy_item(RING_CLASS, AD_ELEC);
		    if (isevilvariant || !rn2(issoviet ? 30 : 165)) /* new calculations --Amy */	destroy_item(AMULET_CLASS, AD_ELEC);
		    if (!resists_blnd(&youmonst)) {
			    You(are_blinded_by_the_flash);
			    make_blinded((long)rnd(40),FALSE);
			    if (!Blind) Your("%s", vision_clears);
		    }
		    break;

		case WAN_THUNDER:
		    makeknown(WAN_THUNDER);
		case SPE_THUNDER:
		    if (!Shock_resistance) {
			You("shock yourself powerfully!");
			damage = d(24,6);
			exercise(A_CON, FALSE);
		    } else {
			shieldeff(u.ux, u.uy);
			You("zap yourself, but seem unharmed.");
			ugolemeffects(AD_ELEC, d(24,6));
		    }
		    if (isevilvariant || !rn2(issoviet ? 6 : 33)) /* new calculations --Amy */	destroy_item(WAND_CLASS, AD_ELEC);
		    if (isevilvariant || !rn2(issoviet ? 6 : 33)) /* new calculations --Amy */	destroy_item(RING_CLASS, AD_ELEC);
		    if (isevilvariant || !rn2(issoviet ? 30 : 165)) /* new calculations --Amy */	destroy_item(AMULET_CLASS, AD_ELEC);
		    if (!resists_blnd(&youmonst)) {
			    You(are_blinded_by_the_flash);
			    make_blinded((long)rnd(40),FALSE);
			    if (!Blind) Your("%s", vision_clears);
		    }
			if (!rn2(3) && multi >= 0) nomul(-rnd(10), "paralyzed by a thunder self-zap", TRUE);
			if (!rn2(2)) make_numbed(HNumbed + rnz(150), TRUE);

		    break;

		case WAN_FIREBALL:
		    makeknown(WAN_FIREBALL);
		case SPE_FIREBALL:
		    You("explode a fireball on top of yourself!");
		    explode(u.ux, u.uy, 11, d(6,6), WAND_CLASS, EXPL_FIERY);
		    break;

		case WAN_FIRE:
		    makeknown(WAN_FIRE);
		case SPE_FIRE_BOLT:
		case FIRE_HORN:
		    if (Fire_resistance) {
			shieldeff(u.ux, u.uy);
			You_feel("rather warm.");
			ugolemeffects(AD_FIRE, d(12,6));
		    } else {
			pline("You've set yourself afire!");
			damage = d(12,6);
		    }
		    if (Slimed) {                    
			pline("The slime is burned away!");
			Slimed = 0;
		    }
		    burn_away_slime();
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 2 : 33)) (void) burnarmor(&youmonst);
		    /*destroy_item(SCROLL_CLASS, AD_FIRE);
		    destroy_item(POTION_CLASS, AD_FIRE);
		    destroy_item(SPBOOK_CLASS, AD_FIRE);*/
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 6 : 33)) /* new calculations --Amy */
		      destroy_item(POTION_CLASS, AD_FIRE);
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 6 : 33))
		      destroy_item(SCROLL_CLASS, AD_FIRE);
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 10 : 50))
		      destroy_item(SPBOOK_CLASS, AD_FIRE);
		    break;

		case WAN_INFERNO:
		    makeknown(WAN_INFERNO);
		case SPE_INFERNO:
		    if (Fire_resistance) {
			shieldeff(u.ux, u.uy);
			You_feel("extremely warm.");
			ugolemeffects(AD_FIRE, d(24,6));
		    } else {
			pline("You've set yourself afire and severely burned your own body!");
			damage = d(24,6);
		    }
		    if (Slimed) {                    
			pline("The slime is burned away!");
			Slimed = 0;
		    }
		    burn_away_slime();
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 2 : 33)) (void) burnarmor(&youmonst);
		    /*destroy_item(SCROLL_CLASS, AD_FIRE);
		    destroy_item(POTION_CLASS, AD_FIRE);
		    destroy_item(SPBOOK_CLASS, AD_FIRE);*/
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 6 : 33)) /* new calculations --Amy */
		      destroy_item(POTION_CLASS, AD_FIRE);
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 6 : 33))
		      destroy_item(SCROLL_CLASS, AD_FIRE);
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 10 : 50))
		      destroy_item(SPBOOK_CLASS, AD_FIRE);

			if (Upolyd && u.mhmax > 1) {
				u.mhmax--;
				if (u.mh > u.mhmax) u.mh = u.mhmax;
			}
			else if (!Upolyd && u.uhpmax > 1) {
				u.uhpmax--;
				if (u.uhp > u.uhpmax) u.uhp = u.uhpmax;
			}
			make_blinded(Blinded+rnz(100),FALSE);

		    break;

		case SPE_MULTIBEAM:

		    if (!Shock_resistance) {
			You("shock yourself!");
			damage = d(12,6);
			exercise(A_CON, FALSE);
		    } else {
			shieldeff(u.ux, u.uy);
			You("zap yourself, but seem unharmed.");
			ugolemeffects(AD_ELEC, d(12,6));
		    }
		    if (isevilvariant || !rn2(issoviet ? 6 : 33)) /* new calculations --Amy */	destroy_item(WAND_CLASS, AD_ELEC);
		    if (isevilvariant || !rn2(issoviet ? 6 : 33)) /* new calculations --Amy */	destroy_item(RING_CLASS, AD_ELEC);
		    if (isevilvariant || !rn2(issoviet ? 30 : 165)) /* new calculations --Amy */	destroy_item(AMULET_CLASS, AD_ELEC);
		    if (!resists_blnd(&youmonst)) {
			    You(are_blinded_by_the_flash);
			    make_blinded((long)rnd(40),FALSE);
			    if (!Blind) Your("%s", vision_clears);
		    }

		    if (Fire_resistance) {
			shieldeff(u.ux, u.uy);
			You_feel("rather warm.");
			ugolemeffects(AD_FIRE, d(12,6));
		    } else {
			pline("You've set yourself afire!");
			damage = d(12,6);
		    }
		    if (Slimed) {                    
			pline("The slime is burned away!");
			Slimed = 0;
		    }
		    burn_away_slime();
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 2 : 33)) (void) burnarmor(&youmonst);
		    /*destroy_item(SCROLL_CLASS, AD_FIRE);
		    destroy_item(POTION_CLASS, AD_FIRE);
		    destroy_item(SPBOOK_CLASS, AD_FIRE);*/
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 6 : 33)) /* new calculations --Amy */
		      destroy_item(POTION_CLASS, AD_FIRE);
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 6 : 33))
		      destroy_item(SCROLL_CLASS, AD_FIRE);
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 10 : 50))
		      destroy_item(SPBOOK_CLASS, AD_FIRE);

		    if (Cold_resistance) {
			shieldeff(u.ux, u.uy);
			You_feel("a little chill.");
			ugolemeffects(AD_COLD, d(12,6));
		    } else {
			You("imitate a popsicle!");
			damage = d(12,6);
			if (Race_if(PM_GAVIL)) damage *= 2;
			if (Race_if(PM_HYPOTHERMIC)) damage *= 3;
		    }
		    if (isevilvariant || !rn2(issoviet ? 6 : Race_if(PM_GAVIL) ? 6 : Race_if(PM_HYPOTHERMIC) ? 6 : 33)) /* new calculations --Amy */    destroy_item(POTION_CLASS, AD_COLD);

			break;

		case SPE_CALL_THE_ELEMENTS:

		    if (!Shock_resistance) {
			You("shock yourself powerfully!");
			damage = d(12,12);
			exercise(A_CON, FALSE);
		    } else {
			shieldeff(u.ux, u.uy);
			You("zap yourself, but seem unharmed.");
			ugolemeffects(AD_ELEC, d(12,12));
		    }
		    if (isevilvariant || !rn2(issoviet ? 6 : 33)) /* new calculations --Amy */	destroy_item(WAND_CLASS, AD_ELEC);
		    if (isevilvariant || !rn2(issoviet ? 6 : 33)) /* new calculations --Amy */	destroy_item(RING_CLASS, AD_ELEC);
		    if (isevilvariant || !rn2(issoviet ? 30 : 165)) /* new calculations --Amy */	destroy_item(AMULET_CLASS, AD_ELEC);
		    if (!resists_blnd(&youmonst)) {
			    You(are_blinded_by_the_flash);
			    make_blinded((long)rnd(40),FALSE);
			    if (!Blind) Your("%s", vision_clears);
		    }

			if (!rn2(3) && multi >= 0) nomul(-rnd(10), "paralyzed by a thunder self-zap", TRUE);
			if (!rn2(2)) make_numbed(HNumbed + rnz(150), TRUE);

		    if (Fire_resistance) {
			shieldeff(u.ux, u.uy);
			You_feel("extremely warm.");
			ugolemeffects(AD_FIRE, d(12,12));
		    } else {
			pline("You've set yourself afire and severely burned your own body!");
			damage = d(12,12);
		    }
		    if (Slimed) {                    
			pline("The slime is burned away!");
			Slimed = 0;
		    }
		    burn_away_slime();
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 2 : 33)) (void) burnarmor(&youmonst);
		    /*destroy_item(SCROLL_CLASS, AD_FIRE);
		    destroy_item(POTION_CLASS, AD_FIRE);
		    destroy_item(SPBOOK_CLASS, AD_FIRE);*/
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 6 : 33)) /* new calculations --Amy */
		      destroy_item(POTION_CLASS, AD_FIRE);
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 6 : 33))
		      destroy_item(SCROLL_CLASS, AD_FIRE);
		    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 10 : 50))
		      destroy_item(SPBOOK_CLASS, AD_FIRE);

			if (Upolyd && u.mhmax > 1) {
				u.mhmax--;
				if (u.mh > u.mhmax) u.mh = u.mhmax;
			}
			else if (!Upolyd && u.uhpmax > 1) {
				u.uhpmax--;
				if (u.uhp > u.uhpmax) u.uhp = u.uhpmax;
			}
			make_blinded(Blinded+rnz(100),FALSE);

		    if (Cold_resistance) {
			shieldeff(u.ux, u.uy);
			You_feel("a chill.");
			ugolemeffects(AD_COLD, d(12,12));
		    } else {
			You("imitate an ice block!");
			damage = d(12,12);
			if (Race_if(PM_GAVIL)) damage *= 2;
			if (Race_if(PM_HYPOTHERMIC)) damage *= 3;
		    }
		    if (isevilvariant || !rn2(issoviet ? 6 : Race_if(PM_GAVIL) ? 6 : Race_if(PM_HYPOTHERMIC) ? 6 : 33)) /* new calculations --Amy */    destroy_item(POTION_CLASS, AD_COLD);

			u_slow_down();

			break;

		case WAN_COLD:
		    makeknown(WAN_COLD);
		case SPE_CONE_OF_COLD:
		case FROST_HORN:
		    if (Cold_resistance) {
			shieldeff(u.ux, u.uy);
			You_feel("a little chill.");
			ugolemeffects(AD_COLD, d(12,6));
		    } else {
			You("imitate a popsicle!");
			damage = d(12,6);
			if (Race_if(PM_GAVIL)) damage *= 2;
			if (Race_if(PM_HYPOTHERMIC)) damage *= 3;
		    }
		    if (isevilvariant || !rn2(issoviet ? 6 : Race_if(PM_GAVIL) ? 6 : Race_if(PM_HYPOTHERMIC) ? 6 : 33)) /* new calculations --Amy */    destroy_item(POTION_CLASS, AD_COLD);

		    break;

		case WAN_ICE_BEAM:
		    makeknown(WAN_ICE_BEAM);
		case SPE_ICE_BEAM:
		    if (Cold_resistance) {
			shieldeff(u.ux, u.uy);
			You_feel("a chill.");
			ugolemeffects(AD_COLD, d(24,6));
		    } else {
			You("imitate an ice block!");
			damage = d(24,6);
			if (Race_if(PM_GAVIL)) damage *= 2;
			if (Race_if(PM_HYPOTHERMIC)) damage *= 3;
		    }
		    if (isevilvariant || !rn2(issoviet ? 6 : Race_if(PM_GAVIL) ? 6 : Race_if(PM_HYPOTHERMIC) ? 6 : 33)) /* new calculations --Amy */    destroy_item(POTION_CLASS, AD_COLD);

			u_slow_down();

		    break;

		case WAN_MAGIC_MISSILE:
		    makeknown(WAN_MAGIC_MISSILE);
		case ETHER_HORN:
		case SPE_MAGIC_MISSILE:
		    if(Antimagic && rn2(StrongAntimagic ? 20 : 5)) {
			shieldeff(u.ux, u.uy);
			pline_The("missiles bounce!");
		    } else {
			damage = d(4,6);
			pline(Role_if(PM_PIRATE) ? "Bilge!  Ye've shot yerself!" : Role_if(PM_KORSAIR) ? "Bilge!  Ye've shot yerself!" : (uwep && uwep->oartifact == ART_ARRRRRR_MATEY) ? "Bilge!  Ye've shot yerself!" : "Idiot!  You've shot yourself!");
		    }
		    break;

		case WAN_HYPER_BEAM:
		    makeknown(WAN_HYPER_BEAM);
		case SPE_HYPER_BEAM:
		    if(Antimagic && rn2(StrongAntimagic ? 20 : 5)) {
			shieldeff(u.ux, u.uy);
			pline_The("beam bounces!");
		    } else {
			damage = d(12,6);
			pline(Role_if(PM_PIRATE) ? "Bilge!  Ye've shot yerself!" : Role_if(PM_KORSAIR) ? "Bilge!  Ye've shot yerself!" : (uwep && uwep->oartifact == ART_ARRRRRR_MATEY) ? "Bilge!  Ye've shot yerself!" : "Idiot!  You've shot yourself!");
		    }
		    break;

		case WAN_POLYMORPH:
		    if (!Unchanging) {
		    	makeknown(WAN_POLYMORPH);

			if (obj->oartifact == ART_DIKKIN_S_DEADLIGHT) {
				HPolymorph_control += 1;
				if (yn("Do a completely random polymorph?") == 'y') {

					do {
						u.wormpolymorph = rn2(NUMMONS);
					} while(( (notake(&mons[u.wormpolymorph]) && rn2(4) ) || ((mons[u.wormpolymorph].mlet == S_BAT) && rn2(2)) || ((mons[u.wormpolymorph].mlet == S_EYE) && rn2(2) ) || ((mons[u.wormpolymorph].mmove == 1) && rn2(4) ) || ((mons[u.wormpolymorph].mmove == 2) && rn2(3) ) || ((mons[u.wormpolymorph].mmove == 3) && rn2(2) ) || ((mons[u.wormpolymorph].mmove == 4) && !rn2(3) ) || ( (mons[u.wormpolymorph].mlevel < 10) && ((mons[u.wormpolymorph].mlevel + 1) < rnd(u.ulevel)) ) || (!haseyes(&mons[u.wormpolymorph]) && rn2(2) ) || ( is_nonmoving(&mons[u.wormpolymorph]) && rn2(5) ) || ( is_eel(&mons[u.wormpolymorph]) && rn2(5) ) || ( is_nonmoving(&mons[u.wormpolymorph]) && rn2(20) ) || (is_jonadabmonster(&mons[u.wormpolymorph]) && rn2(20)) || ( uncommon2(&mons[u.wormpolymorph]) && !rn2(4) ) || ( uncommon3(&mons[u.wormpolymorph]) && !rn2(3) ) || ( uncommon5(&mons[u.wormpolymorph]) && !rn2(2) ) || ( uncommon7(&mons[u.wormpolymorph]) && rn2(3) ) || ( uncommon10(&mons[u.wormpolymorph]) && rn2(5) ) || ( is_eel(&mons[u.wormpolymorph]) && rn2(20) ) ) );

				}
			}

		    	polyself(FALSE);
		    }
		    break;
		case SPE_POLYMORPH:
		    if (!Unchanging) {

			if (uwep && uwep->oartifact == ART_DIKKIN_S_FAVORITE_SPELL) {

				if (yn("Do a completely random polymorph?") == 'y') {

					do {
						u.wormpolymorph = rn2(NUMMONS);
					} while(( (notake(&mons[u.wormpolymorph]) && rn2(4) ) || ((mons[u.wormpolymorph].mlet == S_BAT) && rn2(2)) || ((mons[u.wormpolymorph].mlet == S_EYE) && rn2(2) ) || ((mons[u.wormpolymorph].mmove == 1) && rn2(4) ) || ((mons[u.wormpolymorph].mmove == 2) && rn2(3) ) || ((mons[u.wormpolymorph].mmove == 3) && rn2(2) ) || ((mons[u.wormpolymorph].mmove == 4) && !rn2(3) ) || ( (mons[u.wormpolymorph].mlevel < 10) && ((mons[u.wormpolymorph].mlevel + 1) < rnd(u.ulevel)) ) || (!haseyes(&mons[u.wormpolymorph]) && rn2(2) ) || ( is_nonmoving(&mons[u.wormpolymorph]) && rn2(5) ) || ( is_eel(&mons[u.wormpolymorph]) && rn2(5) ) || ( is_nonmoving(&mons[u.wormpolymorph]) && rn2(20) ) || (is_jonadabmonster(&mons[u.wormpolymorph]) && rn2(20)) || ( uncommon2(&mons[u.wormpolymorph]) && !rn2(4) ) || ( uncommon3(&mons[u.wormpolymorph]) && !rn2(3) ) || ( uncommon5(&mons[u.wormpolymorph]) && !rn2(2) ) || ( uncommon7(&mons[u.wormpolymorph]) && rn2(3) ) || ( uncommon10(&mons[u.wormpolymorph]) && rn2(5) ) || ( is_eel(&mons[u.wormpolymorph]) && rn2(20) ) ) );

				}
				YellowSpells += rnz(10 * (monster_difficulty() + 1));

			}

		    	polyself(FALSE);
			if (!rn2(3)) { /* nerf by Amy */
				pline("The spell backfires!");
				badeffect();
			}
		    }
		    break;
		case WAN_MUTATION:
		    if (!Unchanging) {
		    	makeknown(WAN_MUTATION);
		    	polyself(FALSE);
		    }
		    break;
		case SPE_MUTATION:
		    if (!Unchanging) {
			if (!rn2(3)) { /* nerf by Amy */
				pline("The spell backfires!");
				badeffect();
			}
		    }
		    break;
		case WAN_CANCELLATION:
		case SPE_CANCELLATION:
		    (void) cancel_monst(&youmonst, obj, TRUE, FALSE, TRUE);
		    break;

		case SPE_CHAOS_BOLT:
			damage = d(9,9);
			pline("You are engulfed by raw chaos!");
			if (!Unchanging && !rn2(3)) {
				polyself(FALSE);
			}
		    break;

		case SPE_HELLISH_BOLT:
			damage = d(18,9);
			pline("You are engulfed by raw chaos!");

			if (!rn2(3)) {
				if (!Unchanging) {
					polyself(FALSE);
				}
			} else if (!rn2(2)) {
				if (!Drain_resistance || !rn2(StrongDrain_resistance ? 16 : 4) ) {
					losexp("life drainage", FALSE, TRUE);
				} else {
					shieldeff(u.ux, u.uy);
					pline("Boing!");
				}
			} else {
				if (!Free_action || !rn2(StrongFree_action ? 20 : 5)) {
				    pline("You are frozen in place!");
				    nomul(-rnz(20), "frozen by their own spell", TRUE);
				    nomovemsg = You_can_move_again;
				    exercise(A_DEX, FALSE);
				} else You("stiffen momentarily.");
			}
		    break;

		case SPE_VANISHING:
			if (!rn2(3)) {
			    (void) cancel_monst(&youmonst, obj, TRUE, FALSE, TRUE);
			} else if (!rn2(2)) {
			    int msg = !Invis && !Blind && !BInvis;

			    if (BInvis && uarmc->otyp == MUMMY_WRAPPING) {
				/* A mummy wrapping absorbs it and protects you */
			        You_feel("rather itchy under your %s.", xname(uarmc));
			        break;
			    }
			    if (ordinary || !rn2(10)) {	/* permanent */
				HInvis |= FROMOUTSIDE;
			    } else {			/* temporary */
			    	incr_itimeout(&HInvis, d(obj->spe, 250));
			    }
			    if (msg) {
				newsym(u.ux, u.uy);
				self_invis_message();
			    }
			} else {
			    tele();
			}
		    break;

		case WAN_PARALYSIS:
			makeknown(obj->otyp);
			if (!Free_action || !rn2(StrongFree_action ? 20 : 5)) {
			    pline("You are frozen in place!");
				if (PlayerHearsSoundEffects) pline(issoviet ? "Teper' vy ne mozhete dvigat'sya. Nadeyus', chto-to ubivayet vas, prezhde chem vash paralich zakonchitsya." : "Klltsch-tsch-tsch-tsch-tsch!");
			    nomul(-rnz(20), "frozen by their own wand", TRUE);
			    nomovemsg = You_can_move_again;
			    exercise(A_DEX, FALSE);
			} else You("stiffen momentarily.");

		    break;
		case SPE_PARALYSIS:
			if (!Free_action || !rn2(StrongFree_action ? 20 : 5)) {
			    pline("You are frozen in place!");
				if (PlayerHearsSoundEffects) pline(issoviet ? "Teper' vy ne mozhete dvigat'sya. Nadeyus', chto-to ubivayet vas, prezhde chem vash paralich zakonchitsya." : "Klltsch-tsch-tsch-tsch-tsch!");
			    nomul(-rnz(20), "frozen by their own spell", TRUE);
			    nomovemsg = You_can_move_again;
			    exercise(A_DEX, FALSE);
			} else You("stiffen momentarily.");

		    break;
		case WAN_DISINTEGRATION:
		case WAN_DISINTEGRATION_BEAM:
		case SPE_DISINTEGRATION:
		case SPE_DISINTEGRATION_BEAM:

			if (Disint_resistance && rn2(StrongDisint_resistance ? 1000 : 100) && !(evilfriday && (uarms || uarmc || uarm || uarmu))) {
			    You("are not disintegrated.");
			    break;
	            } else if (Invulnerable || (Stoned_chiller && Stoned)) {
	                pline("You are unharmed!");
	                break;
			} else if (uarms) {
			    /* destroy shield; other possessions are safe */
			    if (!(EDisint_resistance & W_ARMS)) (void) destroy_arm(uarms);
			    break;
			} else if (uarmc) {
			    /* destroy cloak; other possessions are safe */
			    if (!(EDisint_resistance & W_ARMC)) (void) destroy_arm(uarmc);
			    break;
			} else if (uarm) {
			    /* destroy suit */
			    if (!(EDisint_resistance & W_ARM)) (void) destroy_arm(uarm);
			    break;
			} else if (uarmu) {
			    /* destroy shirt */
			    if (!(EDisint_resistance & W_ARMU)) (void) destroy_arm(uarmu);
			    break;
			}
			if (u.uhpmax > 20) {
				u.uhpmax -= rnd(20);
				if (u.uhp > u.uhpmax) u.uhp = u.uhpmax;
				losehp(rnz(100 + level_difficulty()), "selfzap died", KILLED_BY);
				break;
			} else {
				u.youaredead = 1;
				killer_format = KILLED_BY;
				killer = "self-disintegration";
			      done(DIED);
				u.youaredead = 0;
				return 0;
			}

		    break;
		case WAN_STONING:
		case SPE_PETRIFY:

		    if ((!Stone_resistance || (!IntStone_resistance && !rn2(20)) ) && !(poly_when_stoned(youmonst.data) && polymon(PM_STONE_GOLEM))) {
			if (!Stoned) {
				if (Hallucination && rn2(10)) pline("Thankfully you are already stoned.");
				else {
					Stoned = Race_if(PM_EROSATOR) ? 3 : 7;
					pline("You start turning to stone!");
					sprintf(killer_buf, "their own wand of stoning");
					delayed_killer = killer_buf;
				}
			}
		
		    }

		    break;
		case WAN_DRAINING:	/* KMH */
			makeknown(obj->otyp);
		case SPE_DRAIN_LIFE:
			if (!Drain_resistance || !rn2(StrongDrain_resistance ? 16 : 4) ) {
				losexp("life drainage", FALSE, TRUE);
			} else {
				shieldeff(u.ux, u.uy);
				pline("Boing!");
			}
			damage = 0;	/* No additional damage */
			break;

		case WAN_DESLEXIFICATION:
			u.youaredead = 1;
			makeknown(obj->otyp);
			killer_format = KILLED_BY;
			killer = "self-deslexification";
			pline("Ochen' zhal'! Vy igrayete ikh slesh prodlen, kotoryy techically yeshche chast' Slex, i poetomu vy vdrug perestanet sushchestvovat', potomu chto vy byli DESLEXIFIED.");
			done(DIED);
			u.youaredead = 0;
			break;

		case WAN_TIME:
			makeknown(obj->otyp);
		case SPE_TIME:

			if (powerfulimplants() && uimplant && uimplant->oartifact == ART_TIMEAGE_OF_REALMS) {
				You_feel("not as powerful than you used to be, but the feeling passes.");
				break;
			}

		{
		int dmg;
		dmg = (rnd(10) + rnd( (monster_difficulty() * 2) + 1));
		switch (rnd(10)) {

			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
				You_feel("life has clocked back.");
				if (PlayerHearsSoundEffects) pline(issoviet ? "Zhizn' razgonyal nazad, potomu chto vy ne smotreli, i teper' vy dolzhny poluchit', chto poteryannyy uroven' nazad." : "Kloeck!");
			      losexp("time", FALSE, FALSE); /* resistance is futile :D */
				break;
			case 6:
			case 7:
			case 8:
			case 9:
				switch (rnd(A_MAX)) {
					case A_STR:
						pline("You're not as strong as you used to be...");
						ABASE(A_STR) -= 5;
						if(ABASE(A_STR) < ATTRMIN(A_STR)) {dmg *= 3; ABASE(A_STR) = ATTRMIN(A_STR);}
						break;
					case A_DEX:
						pline("You're not as agile as you used to be...");
						ABASE(A_DEX) -= 5;
						if(ABASE(A_DEX) < ATTRMIN(A_DEX)) {dmg *= 3; ABASE(A_DEX) = ATTRMIN(A_DEX);}
						break;
					case A_CON:
						pline("You're not as hardy as you used to be...");
						ABASE(A_CON) -= 5;
						if(ABASE(A_CON) < ATTRMIN(A_CON)) {dmg *= 3; ABASE(A_CON) = ATTRMIN(A_CON);}
						break;
					case A_WIS:
						pline("You're not as wise as you used to be...");
						ABASE(A_WIS) -= 5;
						if(ABASE(A_WIS) < ATTRMIN(A_WIS)) {dmg *= 3; ABASE(A_WIS) = ATTRMIN(A_WIS);}
						break;
					case A_INT:
						pline("You're not as bright as you used to be...");
						ABASE(A_INT) -= 5;
						if(ABASE(A_INT) < ATTRMIN(A_INT)) {dmg *= 3; ABASE(A_INT) = ATTRMIN(A_INT);}
						break;
					case A_CHA:
						pline("You're not as beautiful as you used to be...");
						ABASE(A_CHA) -= 5;
						if(ABASE(A_CHA) < ATTRMIN(A_CHA)) {dmg *= 3; ABASE(A_CHA) = ATTRMIN(A_CHA);}
						break;
				}
				break;
			case 10:
				pline("You're not as powerful as you used to be...");
				ABASE(A_STR)--;
				ABASE(A_DEX)--;
				ABASE(A_CON)--;
				ABASE(A_WIS)--;
				ABASE(A_INT)--;
				ABASE(A_CHA)--;
				if(ABASE(A_STR) < ATTRMIN(A_STR)) {dmg *= 2; ABASE(A_STR) = ATTRMIN(A_STR);}
				if(ABASE(A_DEX) < ATTRMIN(A_DEX)) {dmg *= 2; ABASE(A_DEX) = ATTRMIN(A_DEX);}
				if(ABASE(A_CON) < ATTRMIN(A_CON)) {dmg *= 2; ABASE(A_CON) = ATTRMIN(A_CON);}
				if(ABASE(A_WIS) < ATTRMIN(A_WIS)) {dmg *= 2; ABASE(A_WIS) = ATTRMIN(A_WIS);}
				if(ABASE(A_INT) < ATTRMIN(A_INT)) {dmg *= 2; ABASE(A_INT) = ATTRMIN(A_INT);}
				if(ABASE(A_CHA) < ATTRMIN(A_CHA)) {dmg *= 2; ABASE(A_CHA) = ATTRMIN(A_CHA);}
				break;
		}
		if (dmg) losehp(dmg, "the forces of time", KILLED_BY);
		}

			break;
		case WAN_REDUCE_MAX_HITPOINTS:
			You_feel("drained...");
				if (Upolyd) u.mhmax -= rnd(5);
				else u.uhpmax -= rnd(5);
				if (u.mhmax < 1) u.mhmax = 1;
				if (u.uhpmax < 1) u.uhpmax = 1;
				if (u.uhp > u.uhpmax) u.uhp = u.uhpmax;
				if (u.mh > u.mhmax) u.mh = u.mhmax;
			break;
		case WAN_INCREASE_MAX_HITPOINTS:
			You_feel("powered up!");
				if (Upolyd) u.mhmax += rnd(5);
				else u.uhpmax += rnd(5);
				if (uactivesymbiosis) {
					u.usymbiote.mhpmax += rnd(5);
					if (u.usymbiote.mhpmax > 500) u.usymbiote.mhpmax = 500;
				}
			break;
		case WAN_MAKE_INVISIBLE: {
		    /* have to test before changing HInvis but must change
		     * HInvis before doing newsym().
		     */

		    int msg = !Invis && !Blind && !BInvis;

		    if (evilfriday) {
			incr_itimeout(&HInvis, rn1(25,25));
			if (msg) {
				makeknown(WAN_MAKE_INVISIBLE);
				newsym(u.ux, u.uy);
				self_invis_message();
			}
			break;
		    }

		    if (BInvis && uarmc->otyp == MUMMY_WRAPPING) {
			/* A mummy wrapping absorbs it and protects you */
		        You_feel("rather itchy under your %s.", xname(uarmc));
		        break;
		    }
		    if (ordinary || !rn2(10)) {	/* permanent */
			HInvis |= FROMOUTSIDE;
		    } else {			/* temporary */
		    	incr_itimeout(&HInvis, d(obj->spe, 250));
		    }
		    if (msg) {
			makeknown(WAN_MAKE_INVISIBLE);
			newsym(u.ux, u.uy);
			self_invis_message();
		    }
		    break;
		}
		case WAN_MAKE_VISIBLE:

			if (HInvis & INTRINSIC) {
				HInvis &= ~INTRINSIC;
				pline("You become more opaque.");
				makeknown(WAN_MAKE_VISIBLE);
				newsym(u.ux, u.uy);
			}
			if (HInvis & TIMEOUT) {
				HInvis &= ~TIMEOUT;
				pline("You become more opaque.");
				makeknown(WAN_MAKE_VISIBLE);
				newsym(u.ux, u.uy);
			}
		    break;

		case WAN_STUN_MONSTER:

			make_stunned(HStun + rn1(35, 80), TRUE);
			makeknown(WAN_STUN_MONSTER);

		    break;

		case SPE_STUN_MONSTER:

			make_stunned(HStun + rn1(35, 80), TRUE);

		    break;

		case SPE_MAKE_VISIBLE:

			if (HInvis & INTRINSIC) {
				HInvis &= ~INTRINSIC;
				pline("You become more opaque.");
				newsym(u.ux, u.uy);
			}
			if (HInvis & TIMEOUT) {
				HInvis &= ~TIMEOUT;
				pline("You become more opaque.");
				newsym(u.ux, u.uy);
			}
		    break;
		case WAN_SPEED_MONSTER:
		    if (!(HFast & INTRINSIC)) {
			if (!Fast)
			    You("speed up.");
			else
			    Your("quickness feels more natural.");
			makeknown(WAN_SPEED_MONSTER);
			exercise(A_DEX, TRUE);
		    }
		    /* KMH, balance patch -- Make it permanent, like NetHack */
		    /* Note that this is _not_ very fast */
		    HFast |= FROMOUTSIDE;
		    break;
		case WAN_HASTE_MONSTER:

		makeknown(WAN_HASTE_MONSTER);

		if(Wounded_legs && !u.usteed	/* heal_legs() would heal steeds legs */
		) {
			heal_legs();
			break;
		}

		if (!Very_fast)
			You("are suddenly moving %sfaster.",
				Fast ? "" : "much ");
		else {
			Your("%s get new energy.",
				makeplural(body_part(LEG)));
		}
		exercise(A_DEX, TRUE);
		incr_itimeout(&HFast, rn1(10, 50));

		break;
		case WAN_HEALING:
		   You("begin to feel better.");
		   healup( d(5,6) + rnz(u.ulevel),0,0,0);
		   exercise(A_STR, TRUE);
		   makeknown(WAN_HEALING);
		break;
		case WAN_EXTRA_HEALING:
		   You_feel("much better.");
		   healup(d(6,8) + rnz(u.ulevel),0,0,0);
		   make_hallucinated(0L,TRUE,0L);
		   exercise(A_STR, TRUE);
		   exercise(A_CON, TRUE);
		   makeknown(WAN_EXTRA_HEALING);
		break;
		case WAN_FULL_HEALING:
		   You_feel("restored to health.");
		   healup(d(10,20) + rnz(u.ulevel),0,0,0);
		   make_hallucinated(0L,TRUE,0L);
		   exercise(A_STR, TRUE);
		   exercise(A_CON, TRUE);
		   makeknown(WAN_FULL_HEALING);
		break;

		case WAN_FEAR:
		case SPE_HORRIFY:
			You("suddenly panic!");
			make_feared(HFeared + rnd(50 + (monster_difficulty() * 5) ),TRUE);
			break;

		case WAN_SLEEP:
		    makeknown(WAN_SLEEP);
		case SPE_SLEEP:
		    if(Sleep_resistance) {
			if (!rn2(StrongSleep_resistance ? 20 : 5)) {
			You_feel("a little drowsy.");
			if (flags.moreforced && !MessagesSuppressed) display_nhwindow(WIN_MESSAGE, TRUE);    /* --More-- */
			fall_asleep(-rnd(5), TRUE);}
			else {
			shieldeff(u.ux, u.uy);
			You("don't feel sleepy!");}
		    } else {
			pline_The("sleep ray hits you!");
			if (flags.moreforced && !MessagesSuppressed) display_nhwindow(WIN_MESSAGE, TRUE);    /* --More-- */
			fall_asleep(-rnd(10), TRUE);
		    }
		    break;

		case WAN_CHLOROFORM:
		    makeknown(WAN_CHLOROFORM);
		case SPE_CHLOROFORM:
		    if(Sleep_resistance) {
			if (!rn2(StrongSleep_resistance ? 20 : 5)) {
			You_feel("drowsy.");
			if (flags.moreforced && !MessagesSuppressed) display_nhwindow(WIN_MESSAGE, TRUE);    /* --More-- */
			fall_asleep(-rnd(15), TRUE);}
			else {
			shieldeff(u.ux, u.uy);
			You("don't feel sleepy!");}
		    } else {
			pline_The("chloroform ray hits you!");
			if (flags.moreforced && !MessagesSuppressed) display_nhwindow(WIN_MESSAGE, TRUE);    /* --More-- */
			fall_asleep(-rnd(30), TRUE);
		    }
			losehp(d(1, 12), "self-chloroformation", KILLED_BY);

		    break;

		case WAN_SLOW_MONSTER:
		case SPE_SLOW_MONSTER:
		    if(HFast & (TIMEOUT | INTRINSIC)) {
			u_slow_down();
			makeknown(obj->otyp);
		    }
		    break;

		case SPE_RANDOM_SPEED:
			if (!rn2(2)) {
			    if(HFast & (TIMEOUT | INTRINSIC)) {
				u_slow_down();
				makeknown(obj->otyp);
			    }
			} else {
				if(Wounded_legs && !u.usteed	/* heal_legs() would heal steeds legs */
				) {
					heal_legs();
					break;
				}

				if (!Very_fast)
					You("are suddenly moving %sfaster.",
						Fast ? "" : "much ");
				else {
					Your("%s get new energy.",
						makeplural(body_part(LEG)));
				}
				exercise(A_DEX, TRUE);
				incr_itimeout(&HFast, rn1(10, 20));

			}
		    break;

		case WAN_INERTIA:
		case SPE_INERTIA:

			u_slow_down();
			u.uprops[DEAC_FAST].intrinsic += (( rnd(10) + rnd(monster_difficulty() + 1) ) * 10);
			pline(u.inertia ? "You feel even slower." : "You slow down to a crawl.");
			u.inertia += (rnd(10) + rnd(monster_difficulty() + 1));
			makeknown(obj->otyp);

		    break;
		case WAN_TELEPORTATION:
		case SPE_TELEPORT_AWAY:
		    tele();
			makeknown(obj->otyp);
		    break;
		case WAN_BANISHMENT:
			makeknown(obj->otyp);
			if (((u.uevent.udemigod || u.uhave.amulet) && !u.freeplaymode) || CannotTeleport || (u.usteed && mon_has_amulet(u.usteed))) { pline("You shudder for a moment."); (void) safe_teleds(FALSE); break;}
			if (flags.lostsoul || flags.uberlostsoul || (flags.wonderland && !(u.wonderlandescape)) || (iszapem && !(u.zapemescape)) || u.uprops[STORM_HELM].extrinsic || In_bellcaves(&u.uz) || In_subquest(&u.uz) || In_voiddungeon(&u.uz) || In_netherrealm(&u.uz)) {
			 pline("For some reason you resist the banishment!"); break;}

			make_stunned(HStun + 2, FALSE); /* to suppress teleport control that you might have */

		u.cnd_banishmentcount++;
		if (rn2(2)) {(void) safe_teleds(FALSE); goto_level(&medusa_level, TRUE, FALSE, FALSE); }
		else {(void) safe_teleds(FALSE); goto_level(&portal_level, TRUE, FALSE, FALSE); }

			/*(void) safe_teleds(FALSE);

			goto_level((&medusa_level), TRUE, FALSE, FALSE);*/
			register int newlev = rnd(99);
			d_level newlevel;
			get_level(&newlevel, newlev);
			goto_level(&newlevel, TRUE, FALSE, FALSE);

		    break;
		case WAN_DEATH:
		case SPE_FINGER_OF_DEATH:
		    if (nonliving(youmonst.data) || is_demon(youmonst.data) || Death_resistance) {
			pline((obj->otyp == WAN_DEATH) ?
			  "The wand shoots an apparently harmless beam at you."
			  : "You seem no deader than before.");
			break;
		    }
		    if (Invulnerable || (Stoned_chiller && Stoned)) {
			pline("You are unharmed!");
			break;
		    }

			/* Fix stOOpid interface screw. There is no sane reason why you would ever zap a death ray
			 * in . direction, other than as a way to end the game without so much as a confirmation prompt.
			 * Therefore, I'm deciding that magic resistance has to fucking work. --Amy */
			if (Antimagic && (u.uhpmax > 20)) {
				You("irradiate yourself with pure energy!");
				losehp(rnd(20), "a self-zapped death ray", KILLED_BY);
				u.uhpmax -= rnd(20);
				if (u.uhp > u.uhpmax) u.uhp = u.uhpmax;
				break;
			}

			u.youaredead = 1;
		    killer_format = NO_KILLER_PREFIX;
		    sprintf(buf, "shot %sself with a death ray", uhim());
		    killer = buf;
		    You("irradiate yourself with pure energy!");
		    You("die.");
		    makeknown(obj->otyp);
			/* They might survive with an amulet of life saving */
		    done(DIED);
			u.youaredead = 0;
		    break;

		case WAN_UNDEAD_TURNING:
		    makeknown(WAN_UNDEAD_TURNING);
		case SPE_TURN_UNDEAD:
		    (void) unturn_dead(&youmonst);
		    if (is_undead(youmonst.data)) {
			You_feel("frightened and %sstunned.",
			     Stunned ? "even more " : "");
			make_stunned(HStun + rnd(30), FALSE);
		    } else
			You("shudder in dread.");
		    break;

		case SPE_HEALING:
		case SPE_EXTRA_HEALING:
		    healup(obj->otyp == SPE_HEALING ? rnd(10) + 4 + rnd(rnz(u.ulevel)) : d(3,8)+6 + rnz(u.ulevel),
			   0, FALSE, FALSE);
		    You_feel("%sbetter.",
			obj->otyp == SPE_EXTRA_HEALING ? "much " : "");
		    break;

		case SPE_FULL_HEALING:
		    healup(d(10,10) + rnz(u.ulevel) + rnz(u.ulevel),
			   0, FALSE, FALSE);
		    You_feel("restored to health.");
		    break;
		case WAN_LIGHT:	/* (broken wand) */
		 /* assert( !ordinary ); */
		    damage = d(obj->spe, 25);
		case EXPENSIVE_CAMERA:
		    damage += rnd(25);
		    if (!resists_blnd(&youmonst)) {
			You(are_blinded_by_the_flash);
			make_blinded((long)damage, FALSE);
			makeknown(obj->otyp);
			if (!Blind) Your("%s", vision_clears);
		    }
		    damage = 0;	/* reset */
		    break;

		case WAN_OPENING:
		    if (Punished) makeknown(WAN_OPENING);
		case SPE_KNOCK:
		    if (Punished) Your("chain quivers for a moment.");
		    break;

		case WAN_SHARE_PAIN:	/*from Nethack TNG -- WAN_DRAINING */
		    /*[Sakusha] add message */
			pline(Role_if(PM_PIRATE) ? "Bilge!  Ye've shot yerself!" : Role_if(PM_KORSAIR) ? "Bilge!  Ye've shot yerself!" : (uwep && uwep->oartifact == ART_ARRRRRR_MATEY) ? "Bilge!  Ye've shot yerself!" : "Idiot!  You've shot yourself!");
		    /* theoretically we would have to take away half of */
		    /* u.uhpmax _twice_, but I don't want to be unfair ... */
		    makeknown(obj->otyp);
		    damage = u.uhpmax / 2;
		    if (damage < 1) damage = 1;
		    break;

		case WAN_DIGGING:
		case SPE_DIG:
		case SPE_DETECT_UNSEEN:
		case WAN_NOTHING:
		case WAN_MISFIRE:
		case WAN_LOCKING:
		case SPE_WIZARD_LOCK:
		case SPE_WATER_BOLT:
		    break;
		case WAN_PROBING:
		    for (obj = invent; obj; obj = obj->nobj)
			obj->dknown = 1;
		    /* note: `obj' reused; doesn't point at wand anymore */
		    makeknown(WAN_PROBING);
		case SPE_FINGER:
		    ustatusline();
		    break;
		case SPE_STONE_TO_FLESH:
		    {
		    struct obj *otemp, *onext;
		    boolean didmerge;

		    if (u.umonnum == PM_STONE_GOLEM)
			(void) polymon(PM_FLESH_GOLEM);
		    if (Stoned) fix_petrification();	/* saved! */
		    /* but at a cost.. */
		    for (otemp = invent; otemp; otemp = onext) {
			onext = otemp->nobj;
			(void) bhito(otemp, obj);
			}
		    /*
		     * It is possible that we can now merge some inventory.
		     * Do a higly paranoid merge.  Restart from the beginning
		     * until no merges.
		     */
		    do {
			didmerge = FALSE;
			for (otemp = invent; !didmerge && otemp; otemp = otemp->nobj)
			    for (onext = otemp->nobj; onext; onext = onext->nobj)
			    	if (merged(&otemp, &onext)) {
			    		didmerge = TRUE;
			    		break;
			    		}
		    } while (didmerge);
		    }
		    break;
		default: impossible("object %d used?",obj->otyp);
		    break;
	}
	return(damage);
}

/* you've zapped a wand downwards while riding
 * Return TRUE if the steed was hit by the wand.
 * Return FALSE if the steed was not hit by the wand.
 */
STATIC_OVL boolean
zap_steed(obj)
struct obj *obj;	/* wand or spell */
{
	int steedhit = FALSE;
	
	switch (obj->otyp) {

	   /*
	    * Wands that are allowed to hit the steed
	    * Carefully test the results of any that are
	    * moved here from the bottom section.
	    */
		case WAN_PROBING:
		    probe_monster(u.usteed);
		    makeknown(WAN_PROBING);
		    steedhit = TRUE;
		    break;
		case WAN_TELEPORTATION:
		case SPE_TELEPORT_AWAY:
		    /* you go together */
		    tele();
		    if(Teleport_control || !couldsee(u.ux0, u.uy0) ||
			(distu(u.ux0, u.uy0) >= 16))
				makeknown(obj->otyp);
		    steedhit = TRUE;
		    break;

		case WAN_BANISHMENT:
			makeknown(obj->otyp);
			if (((u.uevent.udemigod || u.uhave.amulet) && !u.freeplaymode) || CannotTeleport || (u.usteed && mon_has_amulet(u.usteed))) { pline("You shudder for a moment."); break;}
			if (flags.lostsoul || flags.uberlostsoul || (flags.wonderland && !(u.wonderlandescape)) || (iszapem && !(u.zapemescape)) || u.uprops[STORM_HELM].extrinsic || In_bellcaves(&u.uz) || In_subquest(&u.uz) || In_voiddungeon(&u.uz) || In_netherrealm(&u.uz)) {
			pline("For some reason you resist the banishment!"); break;}

			make_stunned(HStun + 2, FALSE); /* to suppress teleport control that you might have */

			u.cnd_banishmentcount++;
			if (rn2(2)) {(void) safe_teleds(FALSE); goto_level(&medusa_level, TRUE, FALSE, FALSE); }
			else {(void) safe_teleds(FALSE); goto_level(&portal_level, TRUE, FALSE, FALSE); }

			register int newlev = rnd(99);
			d_level newlevel;
			get_level(&newlevel, newlev);
			goto_level(&newlevel, TRUE, FALSE, FALSE);

			break;

		/* Default processing via bhitm() for these */
		case SPE_CURE_SICKNESS:
		case WAN_MAKE_INVISIBLE:
		case WAN_MAKE_VISIBLE:
		case WAN_STUN_MONSTER:
		case SPE_STUN_MONSTER:
		case SPE_MAKE_VISIBLE:
		case WAN_CANCELLATION:
		case SPE_CANCELLATION:
		case SPE_VANISHING:
		case WAN_POLYMORPH:
		case WAN_MUTATION:
		case SPE_POLYMORPH:
		case SPE_MUTATION:
		case WAN_STRIKING:
		case SPE_FORCE_BOLT:
		case SPE_CHAOS_BOLT:
		case SPE_HELLISH_BOLT:
		case WAN_SLOW_MONSTER:
		case SPE_SLOW_MONSTER:
		case SPE_RANDOM_SPEED:
		case WAN_INERTIA:
		case SPE_INERTIA:
		case WAN_SPEED_MONSTER:
		case WAN_HASTE_MONSTER:
		case SPE_HEALING:
		case SPE_EXTRA_HEALING:
		case SPE_FULL_HEALING:
		case WAN_HEALING:
		case WAN_EXTRA_HEALING:
		case WAN_FULL_HEALING:
		case WAN_DRAINING:
		case WAN_DESLEXIFICATION:
		case WAN_TIME:
		case WAN_INCREASE_MAX_HITPOINTS:
		case WAN_REDUCE_MAX_HITPOINTS:
		case SPE_DRAIN_LIFE:
		case SPE_TIME:
		case WAN_OPENING:
		case SPE_KNOCK:
		case SPE_LOCK_MANIPULATION:
		case WAN_WIND:
		case SPE_WIND:
		case WAN_STONING:
		case WAN_PARALYSIS:
		case WAN_DISINTEGRATION:
		case SPE_PETRIFY:
		case SPE_PARALYSIS:
		case SPE_DISINTEGRATION:
		case WAN_GRAVITY_BEAM:
		case SPE_GRAVITY_BEAM:
		case WAN_DREAM_EATER:
		case SPE_DREAM_EATER:
		case SPE_VOLT_ROCK:
		case SPE_GIANT_FOOT:
		case SPE_ARMOR_SMASH:
		case SPE_BLOOD_STREAM:
		case SPE_SHINING_WAVE:
		case SPE_STRANGLING:
		case SPE_PARTICLE_CANNON:
		case SPE_NERVE_POISON:
		case SPE_GEYSER:
		case SPE_BUBBLING_HOLE:
		case SPE_WATER_FLAME:
		case SPE_MANA_BOLT:
		case SPE_SNIPER_BEAM:
		case SPE_BLINDING_RAY:
		case SPE_ENERGY_BOLT:
		case WAN_BUBBLEBEAM:
		case SPE_BUBBLEBEAM:
		case WAN_GOOD_NIGHT:
		case SPE_GOOD_NIGHT:
		    (void) bhitm(u.usteed, obj);
		    steedhit = TRUE;
		    break;

		default:
		    steedhit = FALSE;
		    break;
	}
	return steedhit;
}

#endif /*OVL0*/
#ifdef OVL3

/*
 * cancel a monster (possibly the hero).  inventory is cancelled only
 * if the monster is zapping itself directly, since otherwise the
 * effect is too strong.  currently non-hero monsters do not zap
 * themselves with cancellation.
 */
boolean
cancel_monst(mdef, obj, youattack, allow_cancel_kill, self_cancel)
register struct monst	*mdef;
register struct obj	*obj;
boolean			youattack, allow_cancel_kill, self_cancel;
{
	boolean	youdefend = (mdef == &youmonst);
	static const char writing_vanishes[] =
				"Some writing vanishes from %s head!";
	static const char your[] = "your";	/* should be extern */

	if (youdefend) {
	    You(!FunnyHallu? "are covered in sparkling lights!"
			      : "are enveloped by psychedelic fireworks!");
		if (PlayerHearsSoundEffects) pline(issoviet ? "Vy ne poteryayete vse soprotivleniya i vashi detali bol'she ne zakoldovannyy ili zaryazheny, tak chto vy mozhete tochno tak zhe otkazat'sya, vy retard." : "Bimmselbimmselbimmselbimmselbimmsel!");
	}

	if (youdefend ? (!youattack && Antimagic && rn2(StrongAntimagic ? 20 : 5) ) /* no longer complete protection --Amy */
		      : resist(mdef, obj->oclass, 0, NOTELL))
		return FALSE;	/* resisted cancellation */

	/* 1st cancel inventory */
	/* Lethe allows monsters to zap you with /oCanc, which has a small
	 * chance of affecting hero's inventory; for parity, /oCanc zapped by
	 * the hero also have a small chance of affecting the monster's
	 * inventory
	 */
	if (!(youdefend ? Antimagic : resists_magm(mdef)) || !rn2(6)) {
	    struct obj *otmp;
	    boolean did_cancel = FALSE;

	    for (otmp = (youdefend ? invent : mdef->minvent);
			    otmp; otmp = otmp->nobj)
		/* extra saving throw for blessed objects --Amy */
		if (self_cancel || !rn2(issoviet ? 24 : otmp->blessed ? 100 : 24)) {
		    cancel_item(otmp, FALSE);
		    did_cancel = TRUE;
		}
	    if (youdefend && did_cancel) {
		flags.botl = 1;	/* potential AC change */
		find_ac();
	    }
	    /* Indicate to the hero that something happened */
	    if (did_cancel && !self_cancel && youdefend) {
		You_feel("a strange sense of loss.");
		if (PlayerHearsSoundEffects) pline(issoviet ? "Da! Odin iz vashikh detaley bol'she ne rabotayet! Sluzhit vam pryamo dlya igry, kak der'mo!" : "Due-l-ue-l-ue-l.");
	    }
	    if (youdefend) attrcurse(); /* remove some random intrinsic as well --Amy */
	}

	if (youdefend && evilfriday) { /* idea by K2 */
		MCReduction += 5000;
		Your("magic cancellation was cancelled!");
	}

	/* now handle special cases */
	if (youdefend) {

		if (uactivesymbiosis) {
			u.usymbiote.mhp = 1;
			u.usymbiote.mhpmax -= rnd(10);
			if (u.usymbiote.mhpmax < 1) {
				killsymbiote();
				You("no longer have a symbiote.");
			}
			else uncursesymbiote(FALSE);
			if (flags.showsymbiotehp) flags.botl = TRUE;
		}

	    if (Upolyd) {
		if ((u.umonnum == PM_CLAY_GOLEM) && !Blind)
		    pline(writing_vanishes, your);
		if (Unchanging)
		    Your("amulet grows hot for a moment, then cools.");
		else {

			/* whoops! Lorskel lost a promising character to this. Let's fix it. --Amy */
			if (Race_if(PM_UNGENOMOLD)) {
				pline("You're hit by negative resonance waves!");
				badeffect();

			} else {

			    u.uhp -= mons[u.umonnum].mlevel;
			    if (self_cancel) {
				u.uhpmax -= mons[u.umonnum].mlevel;
				if (u.uhpmax < 1) u.uhpmax = 1;
			    }
			    u.mh = 0;
			    rehumanize();
			}
		}
	    }
	} else {
	    mdef->mcan = TRUE;

		/* successfully cancelling a monster removes all egotypes --Amy */
		mdef->isegotype = mdef->egotype_thief = mdef->egotype_wallwalk = mdef->egotype_disenchant = mdef->egotype_rust = mdef->egotype_corrosion = mdef->egotype_decay = mdef->egotype_wither = mdef->egotype_grab = mdef->egotype_flying = mdef->egotype_hide = mdef->egotype_regeneration = mdef->egotype_undead = mdef->egotype_domestic = mdef->egotype_covetous = mdef->egotype_avoider = mdef->egotype_petty = mdef->egotype_pokemon = mdef->egotype_slows = mdef->egotype_vampire = mdef->egotype_teleportself = mdef->egotype_teleportyou = mdef->egotype_wrap = mdef->egotype_disease = mdef->egotype_slime = mdef->egotype_engrave = mdef->egotype_dark = mdef->egotype_luck = mdef->egotype_push = mdef->egotype_arcane = mdef->egotype_clerical = mdef->egotype_armorer = mdef->egotype_tank = mdef->egotype_speedster = mdef->egotype_racer = mdef->egotype_randomizer = mdef->egotype_blaster = mdef->egotype_multiplicator = mdef->egotype_gator = mdef->egotype_reflecting = mdef->egotype_hugger = mdef->egotype_mimic = mdef->egotype_permamimic = mdef->egotype_poisoner = mdef->egotype_elementalist = mdef->egotype_resistor = mdef->egotype_acidspiller = mdef->egotype_watcher = mdef->egotype_metallivore = mdef->egotype_lithivore = mdef->egotype_organivore = mdef->egotype_breather = mdef->egotype_beamer = mdef->egotype_troll = mdef->egotype_faker = mdef->egotype_farter = mdef->egotype_timer = mdef->egotype_thirster = mdef->egotype_watersplasher = mdef->egotype_cancellator = mdef->egotype_banisher = mdef->egotype_shredder = mdef->egotype_abductor = mdef->egotype_incrementor = mdef->egotype_mirrorimage = mdef->egotype_curser = mdef->egotype_horner = mdef->egotype_lasher = mdef->egotype_cullen = mdef->egotype_webber = mdef->egotype_itemporter = mdef->egotype_schizo = mdef->egotype_nexus = mdef->egotype_sounder = mdef->egotype_gravitator = mdef->egotype_inert = mdef->egotype_antimage = mdef->egotype_plasmon = mdef->egotype_weaponizer = mdef->egotype_engulfer = mdef->egotype_bomber = mdef->egotype_exploder = mdef->egotype_unskillor = mdef->egotype_blinker = mdef->egotype_psychic = mdef->egotype_abomination = mdef->egotype_gazer = mdef->egotype_seducer = mdef->egotype_flickerer = mdef->egotype_hitter = mdef->egotype_piercer = mdef->egotype_petshielder = mdef->egotype_displacer = mdef->egotype_lifesaver = mdef->egotype_venomizer = mdef->egotype_nastinator = mdef->egotype_baddie = mdef->egotype_dreameater = mdef->egotype_sludgepuddle = mdef->egotype_vulnerator = mdef->egotype_marysue = mdef->egotype_shader = mdef->egotype_amnesiac = mdef->egotype_trapmaster = mdef->egotype_midiplayer = mdef->egotype_rngabuser = mdef->egotype_mastercaster = mdef->egotype_aligner = mdef->egotype_sinner = mdef->egotype_minator = mdef->egotype_aggravator = mdef->egotype_contaminator = mdef->egotype_radiator = mdef->egotype_weeper = mdef->egotype_reactor = mdef->egotype_destructor = mdef->egotype_trembler = mdef->egotype_worldender = mdef->egotype_damager = mdef->egotype_antitype = mdef->egotype_painlord = mdef->egotype_empmaster = mdef->egotype_spellsucker = mdef->egotype_eviltrainer = mdef->egotype_statdamager = mdef->egotype_sanitizer = mdef->egotype_nastycurser = mdef->egotype_damagedisher = mdef->egotype_thiefguildmember = mdef->egotype_rogue = mdef->egotype_steed = mdef->egotype_champion = mdef->egotype_boss = mdef->egotype_atomizer = mdef->egotype_perfumespreader = mdef->egotype_converter = mdef->egotype_wouwouer = mdef->egotype_allivore = mdef->egotype_laserpwnzor = mdef->egotype_badowner = mdef->egotype_bleeder = mdef->egotype_shanker = mdef->egotype_terrorizer = mdef->egotype_feminizer = mdef->egotype_levitator = mdef->egotype_illusionator = mdef->egotype_stealer = mdef->egotype_stoner = mdef->egotype_maecke = 0;

	    if (is_were(mdef->data) && mdef->data->mlet != S_HUMAN)
		were_change(mdef);

	    if (mdef->data == &mons[PM_CLAY_GOLEM]) {
		if (canseemon(mdef))
		    pline(writing_vanishes, s_suffix(mon_nam(mdef)));

		if (allow_cancel_kill) {
		    if (youattack)
			killed(mdef);
		    else
			monkilled(mdef, "", AD_SPEL);
		}
	    }
	}
	return TRUE;
}

void
cancelmonsterlite(mdef)
register struct monst	*mdef;
{
	mdef->mcan = TRUE;

	/* successfully cancelling a monster removes all egotypes --Amy */
	mdef->isegotype = mdef->egotype_thief = mdef->egotype_wallwalk = mdef->egotype_disenchant = mdef->egotype_rust = mdef->egotype_corrosion = mdef->egotype_decay = mdef->egotype_wither = mdef->egotype_grab = mdef->egotype_flying = mdef->egotype_hide = mdef->egotype_regeneration = mdef->egotype_undead = mdef->egotype_domestic = mdef->egotype_covetous = mdef->egotype_avoider = mdef->egotype_petty = mdef->egotype_pokemon = mdef->egotype_slows = mdef->egotype_vampire = mdef->egotype_teleportself = mdef->egotype_teleportyou = mdef->egotype_wrap = mdef->egotype_disease = mdef->egotype_slime = mdef->egotype_engrave = mdef->egotype_dark = mdef->egotype_luck = mdef->egotype_push = mdef->egotype_arcane = mdef->egotype_clerical = mdef->egotype_armorer = mdef->egotype_tank = mdef->egotype_speedster = mdef->egotype_racer = mdef->egotype_randomizer = mdef->egotype_blaster = mdef->egotype_multiplicator = mdef->egotype_gator = mdef->egotype_reflecting = mdef->egotype_hugger = mdef->egotype_mimic = mdef->egotype_permamimic = mdef->egotype_poisoner = mdef->egotype_elementalist = mdef->egotype_resistor = mdef->egotype_acidspiller = mdef->egotype_watcher = mdef->egotype_metallivore = mdef->egotype_lithivore = mdef->egotype_organivore = mdef->egotype_breather = mdef->egotype_beamer = mdef->egotype_troll = mdef->egotype_faker = mdef->egotype_farter = mdef->egotype_timer = mdef->egotype_thirster = mdef->egotype_watersplasher = mdef->egotype_cancellator = mdef->egotype_banisher = mdef->egotype_shredder = mdef->egotype_abductor = mdef->egotype_incrementor = mdef->egotype_mirrorimage = mdef->egotype_curser = mdef->egotype_horner = mdef->egotype_lasher = mdef->egotype_cullen = mdef->egotype_webber = mdef->egotype_itemporter = mdef->egotype_schizo = mdef->egotype_nexus = mdef->egotype_sounder = mdef->egotype_gravitator = mdef->egotype_inert = mdef->egotype_antimage = mdef->egotype_plasmon = mdef->egotype_weaponizer = mdef->egotype_engulfer = mdef->egotype_bomber = mdef->egotype_exploder = mdef->egotype_unskillor = mdef->egotype_blinker = mdef->egotype_psychic = mdef->egotype_abomination = mdef->egotype_gazer = mdef->egotype_seducer = mdef->egotype_flickerer = mdef->egotype_hitter = mdef->egotype_piercer = mdef->egotype_petshielder = mdef->egotype_displacer = mdef->egotype_lifesaver = mdef->egotype_venomizer = mdef->egotype_nastinator = mdef->egotype_baddie = mdef->egotype_dreameater = mdef->egotype_sludgepuddle = mdef->egotype_vulnerator = mdef->egotype_marysue = mdef->egotype_shader = mdef->egotype_amnesiac = mdef->egotype_trapmaster = mdef->egotype_midiplayer = mdef->egotype_rngabuser = mdef->egotype_mastercaster = mdef->egotype_aligner = mdef->egotype_sinner = mdef->egotype_minator = mdef->egotype_aggravator = mdef->egotype_contaminator = mdef->egotype_radiator = mdef->egotype_weeper = mdef->egotype_reactor = mdef->egotype_destructor = mdef->egotype_trembler = mdef->egotype_worldender = mdef->egotype_damager = mdef->egotype_antitype = mdef->egotype_painlord = mdef->egotype_empmaster = mdef->egotype_spellsucker = mdef->egotype_eviltrainer = mdef->egotype_statdamager = mdef->egotype_sanitizer = mdef->egotype_nastycurser = mdef->egotype_damagedisher = mdef->egotype_thiefguildmember = mdef->egotype_rogue = mdef->egotype_steed = mdef->egotype_champion = mdef->egotype_boss = mdef->egotype_atomizer = mdef->egotype_perfumespreader = mdef->egotype_converter = mdef->egotype_wouwouer = mdef->egotype_allivore = mdef->egotype_laserpwnzor = mdef->egotype_badowner = mdef->egotype_bleeder = mdef->egotype_shanker = mdef->egotype_terrorizer = mdef->egotype_feminizer = mdef->egotype_levitator = mdef->egotype_illusionator = mdef->egotype_stealer = mdef->egotype_stoner = mdef->egotype_maecke = 0;

}

/* you've zapped an immediate type wand up or down */
STATIC_OVL boolean
zap_updown(obj)
struct obj *obj;	/* wand or spell */
{
	boolean striking = FALSE, disclose = FALSE;
	int x, y, xx, yy, ptmp;
	struct obj *otmp;
	struct engr *e;
	struct trap *ttmp;
	char buf[BUFSZ];

	/* some wands have special effects other than normal bhitpile */
	/* drawbridge might change <u.ux,u.uy> */
	x = xx = u.ux;	/* <x,y> is zap location */
	y = yy = u.uy;	/* <xx,yy> is drawbridge (portcullis) position */
	ttmp = t_at(x, y); /* trap if there is one */

	switch (obj->otyp) {
	case WAN_PROBING:
	    ptmp = 0;
	    if (u.dz < 0) {
		You("probe towards the %s.", ceiling(x,y));
	    } else {
		ptmp += bhitpile(obj, bhito, x, y);
		You("probe beneath the %s.", surface(x,y));
		ptmp += display_binventory(x, y, TRUE);
	    }
	    if (!ptmp) Your("probe reveals nothing.");
	    return TRUE;	/* we've done our own bhitpile */
	case WAN_OPENING:
	case SPE_KNOCK:
	    /* up or down, but at closed portcullis only */
	    if (is_db_wall(x,y) && find_drawbridge(&xx, &yy)) {
		open_drawbridge(xx, yy);
		disclose = TRUE;
	    } else if (u.dz > 0 && (x == xdnstair && y == ydnstair) &&
			/* can't use the stairs down to quest level 2 until
			   leader "unlocks" them; give feedback if you try */
			on_level(&u.uz, &qstart_level) && !ok_to_quest()) {
		pline_The("stairs seem to ripple momentarily.");
		disclose = TRUE;
	    }
	    break;
	case SPE_WATER_BOLT:
		break;
	case WAN_STRIKING:
	case SPE_FORCE_BOLT:
	case WAN_GRAVITY_BEAM:
	case SPE_GRAVITY_BEAM:
	case WAN_WIND:
	case SPE_WIND:
	    striking = TRUE;
	    /*FALLTHRU*/
	case WAN_LOCKING:
	case SPE_WIZARD_LOCK:
	    /* down at open bridge or up or down at open portcullis */
	    if ((levl[x][y].typ == DRAWBRIDGE_DOWN) ? (u.dz > 0) :
			(is_drawbridge_wall(x,y) && !is_db_wall(x,y)) &&
		    find_drawbridge(&xx, &yy)) {
		if (!striking)
		    close_drawbridge(xx, yy);
		else
		    destroy_drawbridge(xx, yy);
		disclose = TRUE;
	    } else if (striking && u.dz < 0 && rn2(3) &&
			!Is_airlevel(&u.uz) && !Is_waterlevel(&u.uz) &&
			!Underwater && !Is_qstart(&u.uz)) {
		/* similar to zap_dig() */
		pline("A rock is dislodged from the %s and falls on your %s.",
		      ceiling(x, y), body_part(HEAD));
		losehp(rnd((uarmh && is_metallic(uarmh) && !is_etheritem(uarmh)) ? 2 : 6),
		       "falling rock", KILLED_BY_AN);
		if ((otmp = mksobj_at(ROCK, x, y, FALSE, FALSE)) != 0) {

			if(!rn2(8)) {
				otmp->spe = rne(2);
				if (rn2(2)) otmp->blessed = rn2(2);
				 else	blessorcurse(otmp, 3);
			} else if(!rn2(10)) {
				if (rn2(10)) curse(otmp);
				 else	blessorcurse(otmp, 3);
				otmp->spe = -rne(2);
			} else	blessorcurse(otmp, 10);

		    (void)xname(otmp);	/* set dknown, maybe bknown */
		    stackobj(otmp);
		}
		newsym(x, y);
	    } else if (!striking && ttmp && ttmp->ttyp == TRAPDOOR && u.dz > 0) {
		if (!Blind) {
			if (ttmp->tseen) {
				pline("A trap door beneath you closes up then vanishes.");
				disclose = TRUE;
			} else {
				You("see a swirl of %s beneath you.",
					is_ice(x,y) ? "frost" : "dust");
			}
		} else {
			You_hear("a twang followed by a thud.");
		}
		deltrap(ttmp);
		ttmp = (struct trap *)0;
		newsym(x, y);
	    }
	    break;
	case SPE_STONE_TO_FLESH:
	    if (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz) ||
		     Underwater || (Is_qstart(&u.uz) && u.dz < 0)) {
		pline("%s", nothing_happens);
		if (FailureEffects || u.uprops[FAILURE_EFFECTS].extrinsic || have_failurestone()) {
			pline("Oh wait, actually something bad happens...");
			badeffect();
		}
	    } else if (u.dz < 0) {	/* we should do more... */
		pline("Blood drips on your %s.", body_part(FACE));
	    } else if (u.dz > 0 && !OBJ_AT(u.ux, u.uy)) {
		/*
		Print this message only if there wasn't an engraving
		affected here.  If water or ice, act like waterlevel case.
		*/
		e = engr_at(u.ux, u.uy);
		if (!(e && e->engr_type == ENGRAVE)) {
		    if (is_waterypool(u.ux, u.uy) || is_ice(u.ux, u.uy)) {
			pline("%s", nothing_happens);
			if (FailureEffects || u.uprops[FAILURE_EFFECTS].extrinsic || have_failurestone()) {
				pline("Oh wait, actually something bad happens...");
				badeffect();
			}
		    } else
			pline("Blood %ss %s your %s.",
			      is_lava(u.ux, u.uy) ? "boil" : "pool",
			      Levitation ? "beneath" : "at",
			      makeplural(body_part(FOOT)));
		}
	    }
	    break;
	default:
	    break;
	}

	if (u.dz > 0) {
	    /* zapping downward */
	    (void) bhitpile(obj, bhito, x, y);

	    /* subset of engraving effects; none sets `disclose' */
	    if ((e = engr_at(x, y)) != 0 && e->engr_type != HEADSTONE) {
		switch (obj->otyp) {
		case WAN_POLYMORPH:
		case WAN_MUTATION:
		case SPE_POLYMORPH:
		case SPE_MUTATION:
		    del_engr(e);
		    make_engr_at(x, y, random_engraving(buf), moves, (xchar)0);
		    break;
		case WAN_CANCELLATION:
		case SPE_CANCELLATION:
		case WAN_MAKE_INVISIBLE:
		case WAN_BANISHMENT:
		    del_engr(e);
		    break;
		case WAN_TELEPORTATION:
		case SPE_TELEPORT_AWAY:
		    rloc_engr(e);
		    break;
		case SPE_STONE_TO_FLESH:
		    if (e->engr_type == ENGRAVE) {
			/* only affects things in stone */
			pline_The(FunnyHallu ?
			    "floor runs like butter!" :
			    "edges on the floor get smoother.");
			wipe_engr_at(x, y, d(2,4));
			}
		    break;
		case WAN_STRIKING:
		case SPE_FORCE_BOLT:
		case WAN_GRAVITY_BEAM:
		case SPE_GRAVITY_BEAM:
		case WAN_WIND:
		case SPE_WIND:
		    wipe_engr_at(x, y, d(2,4));
		    break;
		default:
		    break;
		}
	    }
	}

	return disclose;
}

#endif /*OVL3*/
#ifdef OVLB

/* called for various wand and spell effects - M. Stephenson */
void
weffects(obj)
struct obj *obj;
{
	int otyp = obj->otyp;
	boolean disclose = FALSE, was_unkn = !objects[otyp].oc_name_known;
	int skilldmg = 0; /*WAC - Skills damage bonus*/

	if (obj->oartifact == ART_DIKKIN_S_DEADLIGHT) YellowSpells += rnz(10 * (monster_difficulty() + 1));

	if (otyp >= SPE_MAGIC_MISSILE && otyp <= SPE_PSYBEAM)
		skilldmg = spell_damage_bonus(obj->otyp);
	if (otyp == SPE_CHROMATIC_BEAM)
		skilldmg = spell_damage_bonus(obj->otyp);
	if (otyp == SPE_ELEMENTAL_BEAM)
		skilldmg = spell_damage_bonus(obj->otyp);
	if (otyp == SPE_NATURE_BEAM)
		skilldmg = spell_damage_bonus(obj->otyp);
	if (otyp == SPE_HYPER_BEAM)
		skilldmg = spell_damage_bonus(obj->otyp);
	if (otyp == SPE_FIRE_BOLT)
		skilldmg = spell_damage_bonus(obj->otyp);
	if (otyp == SPE_CALL_THE_ELEMENTS)
		skilldmg = spell_damage_bonus(obj->otyp);
	if (otyp == SPE_MULTIBEAM)
		skilldmg = spell_damage_bonus(obj->otyp);
	if (otyp >= SPE_INFERNO && otyp <= SPE_CHLOROFORM)
		skilldmg = spell_damage_bonus(obj->otyp);


	exercise(A_WIS, TRUE);
	if (u.usteed && (objects[otyp].oc_dir != NODIR) &&
	    !u.dx && !u.dy && (u.dz > 0) && zap_steed(obj)) {
		disclose = TRUE;
	} else

	/* Some wands are supposed to have both IMMEDIATE and RAY effects --Amy */
	if (obj->otyp == WAN_INFERNO || obj->otyp == SPE_INFERNO || obj->otyp == SPE_CALL_THE_ELEMENTS || obj->otyp == WAN_ICE_BEAM || obj->otyp == SPE_ICE_BEAM || obj->otyp == WAN_THUNDER || obj->otyp == SPE_THUNDER || obj->otyp == WAN_TOXIC || obj->otyp == SPE_TOXIC || obj->otyp == WAN_SLUDGE || obj->otyp == SPE_SLUDGE || obj->otyp == WAN_CHLOROFORM || obj->otyp == SPE_CHLOROFORM || obj->otyp == WAN_NETHER_BEAM || obj->otyp == SPE_NETHER_BEAM || obj->otyp == WAN_AURORA_BEAM || obj->otyp == SPE_AURORA_BEAM) {

	    if (u.uswallow) {
		(void) bhitm(u.ustuck, obj);
	    } else if (u.dz) {
		zap_updown(obj);
	    } else {
		int beamrange = (EnglandMode ? rn1(10,10) : rn1(8,6));
		if (tech_inuse(T_OVER_RAY)) {
			beamrange *= 3;
			beamrange /= 2;
		}
		if (uarmg && uarmg->oartifact == ART_LONGEST_RAY) {
			beamrange *= 3;
			beamrange /= 2;
		}
		if (tech_inuse(T_BLADE_ANGER) && obj->otyp == SPE_BLANK_PAPER) beamrange += rnd(6);
		if (tech_inuse(T_BEAMSWORD) && obj->otyp == SPE_BLANK_PAPER) beamrange += rnd(6);

		(void) bhit(u.dx,u.dy, obj->otyp == SPE_PARTICLE_CANNON ? 200 : obj->otyp == SPE_SNIPER_BEAM ? 70 : beamrange, ZAPPED_WAND, bhitm, bhito, &obj);
	    }

	}

	if (objects[otyp].oc_dir == IMMEDIATE || (tech_inuse(T_BLADE_ANGER) && obj->otyp == SPE_BLANK_PAPER) || (tech_inuse(T_BEAMSWORD) && obj->otyp == SPE_BLANK_PAPER) ) {
	    obj_zapped = FALSE;

		if (obj->otyp == WAN_WIND) {
			pline("Winds swirl around you!");
			makeknown(obj->otyp);
		}
		if (obj->otyp == SPE_WIND) {
			pline("Winds swirl around you!");
		}
	    if (u.uswallow) {
		(void) bhitm(u.ustuck, obj);
		/* [how about `bhitpile(u.ustuck->minvent)' effect?] */
	    } else if (u.dz) {
		disclose = zap_updown(obj);
	    } else {
		int beamrange = (EnglandMode ? rn1(10,10) : rn1(8,6));
		if (tech_inuse(T_OVER_RAY)) {
			beamrange *= 3;
			beamrange /= 2;
		}
		if (uarmg && uarmg->oartifact == ART_LONGEST_RAY) {
			beamrange *= 3;
			beamrange /= 2;
		}

		if (tech_inuse(T_BLADE_ANGER) && obj->otyp == SPE_BLANK_PAPER) beamrange += rnd(6);
		if (tech_inuse(T_BEAMSWORD) && obj->otyp == SPE_BLANK_PAPER) beamrange += rnd(6);

		(void) bhit(u.dx,u.dy, obj->otyp == SPE_PARTICLE_CANNON ? 200 : obj->otyp == SPE_SNIPER_BEAM ? 70 : beamrange, ZAPPED_WAND,
			    bhitm, bhito, &obj);
	    }
	    /* give a clue if obj_zapped */
	    if (obj_zapped) {
		You_feel("shuddering vibrations.");
		if (PlayerHearsSoundEffects) pline(issoviet ? "Takim obrazom, vy dumayete, po-vidimomu, vy mozhete prosto izmenit' elementy beskonechno. No tip bloka l'da udaleny nekotoryye potomu, chto vash zloupotrebleniye ne budet dopuskat'sya. Khar." : "Huddale-hualehuaaah!");
	    }

	} else if (objects[otyp].oc_dir == NODIR) {
	    zapnodir(obj);

	} else {
	    /* neither immediate nor directionless */
	    if (otyp == WAN_DIGGING)
		zap_dig(TRUE);
	    else if (otyp == SPE_DIG)
		zap_dig(FALSE);
		else if (otyp >= SPE_MAGIC_MISSILE && otyp <= SPE_PSYBEAM)
			/* WAC --
			 * Include effect for Mega Large Fireball/Cones of Cold.
			 * Include effect for Lightning cast.
			 * Include Yell...atmospheric.
			 * Added slight delay before fireworks. */

			if (((otyp >= SPE_MAGIC_MISSILE && otyp <= SPE_PSYBEAM))
		            && (tech_inuse(T_SIGIL_TEMPEST))) {
				/* sigil of tempest */                     
				throwstorm(obj, skilldmg, 2 , 2);
			} else if (((otyp >= SPE_MAGIC_MISSILE && otyp <= SPE_PSYBEAM))
					/*WAC - use sigil of discharge */
		            && (tech_inuse(T_SIGIL_DISCHARGE))) {
				You("yell \"%s\"",yell_types[otyp - SPE_MAGIC_MISSILE]);
				if (flags.moreforced && !MessagesSuppressed) display_nhwindow(WIN_MESSAGE, TRUE);    /* --More-- */
				buzz(ZT_MEGA(otyp - SPE_MAGIC_MISSILE),
						u.ulevel/2 + 1 + skilldmg,
						u.ux, u.uy, u.dx, u.dy);
			} else if (otyp == SPE_SLEEP) { /* way too uber, needs nerf badly --Amy */
				int sleepnerfamount = (u.ulevel / 2 + 1 + skilldmg);
				if (sleepnerfamount > 1) sleepnerfamount /= 2;
				if (sleepnerfamount > 1 && !rn2(2)) {
					sleepnerfamount /= 2;
					if (sleepnerfamount > 1 && !rn2(5)) sleepnerfamount /= 2;
				}
				buzz(ZT_SPELL(otyp - SPE_MAGIC_MISSILE), sleepnerfamount, u.ux, u.uy, u.dx, u.dy);
			} else { /* zap a "normal" spell */
				buzz(ZT_SPELL(otyp - SPE_MAGIC_MISSILE), u.ulevel / 2 + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);
			}

		else if (otyp >= WAN_MAGIC_MISSILE && otyp <= WAN_PSYBEAM) {
			buzz(otyp - WAN_MAGIC_MISSILE,
		     (otyp == WAN_MAGIC_MISSILE) ? 4 + (rnz(u.ulevel) / 5) + (rnz(u.ulevel) / 5) + (rnz(u.ulevel) / 5) : (otyp == WAN_SOLAR_BEAM) ? 8 + (rnz(u.ulevel) / 4) + (rnz(u.ulevel) / 4) + (rnz(u.ulevel) / 4) : (otyp == WAN_PSYBEAM) ? 7 + (rnz(u.ulevel) / 5) + (rnz(u.ulevel) / 5) + (rnz(u.ulevel) / 5) : 6 + (rnz(u.ulevel) / 5) + (rnz(u.ulevel) / 5) + (rnz(u.ulevel) / 5),
		     u.ux, u.uy, u.dx, u.dy);
	    }

	    else if (otyp == WAN_POISON) {
		buzz((int)(26), 7 + (rnz(u.ulevel) / 6) + (rnz(u.ulevel) / 6) + (rnz(u.ulevel) / 6), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == WAN_HYPER_BEAM) {
		buzz((int)(20), 6 + (rnz(u.ulevel) / 3) + (rnz(u.ulevel) / 3) + (rnz(u.ulevel) / 3), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_HYPER_BEAM) {
		buzz((int)(20), (u.ulevel * 3 / 2) + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == WAN_CHROMATIC_BEAM) {
		int damagetype = 20 + rn2(9);
		buzz((int)(damagetype), damagetype == 26 ? 7 + (rnz(u.ulevel) / 6) + (rnz(u.ulevel) / 6) + (rnz(u.ulevel) / 6) : damagetype == 20 ? 2 + (rnz(u.ulevel) / 10) + (rnz(u.ulevel) / 10) + (rnz(u.ulevel) / 10) : damagetype == 28 ? 8 + (rnz(u.ulevel) / 4) + (rnz(u.ulevel) / 4) + (rnz(u.ulevel) / 4) : 6 + (rnz(u.ulevel) / 5) + (rnz(u.ulevel) / 5) + (rnz(u.ulevel) / 5), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_CHROMATIC_BEAM) {
		int damagetype = 20 + rn2(9);
		buzz((int)(damagetype), u.ulevel / 2 + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_ELEMENTAL_BEAM) {
		int damagetype = !rn2(4) ? 21 : !rn2(3) ? 22 : !rn2(2) ? 25 : 26;
		buzz((int)(damagetype), u.ulevel / 2 + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_NATURE_BEAM) {
		int damagetype = !rn2(4) ? 21 : !rn2(3) ? 22 : !rn2(2) ? 25 : 26;
		buzz((int)(damagetype), u.ulevel + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == WAN_DISINTEGRATION_BEAM) {
		buzz((int)(24), 7 + (rnz(u.ulevel) / 6) + (rnz(u.ulevel) / 6) + (rnz(u.ulevel) / 6), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_DISINTEGRATION_BEAM) {
		buzz((int)(24), 7 + (rnz(u.ulevel) / 6), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_FIRE_BOLT) {
		buzz((int)(21), u.ulevel / 2 + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_NETHER_BEAM) {
		buzz((int)(29), u.ulevel / 2 + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == WAN_NETHER_BEAM) {
		buzz((int)(29), 7 + (rnz(u.ulevel) / 2), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_AURORA_BEAM) {
		buzz((int)(28), u.ulevel / 2 + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == WAN_AURORA_BEAM) {
		buzz((int)(28), 8 + (rnz(u.ulevel) * 3 / 2), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_INFERNO) {
		buzz((int)(21), u.ulevel + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == WAN_INFERNO) {
		buzz((int)(21), 12 + rnz(u.ulevel), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_ICE_BEAM) {
		buzz((int)(22), u.ulevel + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_MULTIBEAM) {
		buzz((int)(22), (u.ulevel / (3 + rn2(2))) + skilldmg, u.ux, u.uy, u.dx, u.dy);
		buzz((int)(21), (u.ulevel / (3 + rn2(2))) + skilldmg, u.ux, u.uy, u.dx, u.dy);
		buzz((int)(25), (u.ulevel / (3 + rn2(2))) + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_CALL_THE_ELEMENTS) {
		buzz((int)(22), (u.ulevel / 2) + skilldmg, u.ux, u.uy, u.dx, u.dy);
		buzz((int)(21), (u.ulevel / 2) + skilldmg, u.ux, u.uy, u.dx, u.dy);
		buzz((int)(25), (u.ulevel / 2) + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == WAN_ICE_BEAM) {
		buzz((int)(22), 12 + rnz(u.ulevel), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_THUNDER) {
		buzz((int)(25), u.ulevel + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == WAN_THUNDER) {
		buzz((int)(25), 12 + rnz(u.ulevel), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_SLUDGE) {
		buzz((int)(27), u.ulevel + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == WAN_SLUDGE) {
		buzz((int)(27), 12 + rnz(u.ulevel), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_TOXIC) {
		buzz((int)(26), u.ulevel + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == WAN_TOXIC) {
		buzz((int)(26), 12 + rnz(u.ulevel), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == SPE_CHLOROFORM) {
		buzz((int)(23), u.ulevel + 1 + skilldmg, u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == WAN_CHLOROFORM) {
		buzz((int)(23), 12 + rnz(u.ulevel), u.ux, u.uy, u.dx, u.dy);

	    }

	    else if (otyp == WAN_VENOM_SCATTERING) {

		buzz(36, 7 + (rnz(u.ulevel) / 3) + (rnz(u.ulevel) / 3) + (rnz(u.ulevel) / 3), u.ux, u.uy, u.dx, u.dy);

	    }

	    else
		impossible("weffects: unexpected spell or wand");
	    disclose = TRUE;
	}
	if (disclose && was_unkn) {
	    makeknown(otyp);
	    more_experienced(0,10);
	}
	return;
}
#endif /*OVLB*/
#ifdef OVL0

/*
 * LSZ/WWA The Wizard Patch July '96 -
 * Generate the to damage bonus for a spell. Based on the hero's intelligence
 */
/*  WAC now also depends on skill level.  Returns -2 to 5 */
int
spell_damage_bonus(booktype)
register int booktype;
{
	register int intell = ACURR(A_INT);
	int tmp;


	if (intell < 10) tmp = -1;      /* Punish low intell. before level else low */
	else if (GushLevel < 5) tmp = 0; /* intell. gets punished only when high level*/
	else if (intell < 14)  tmp = 1;
	else if (intell <= 18) tmp = 2;            
	else tmp = 3;                   /* Hero may have helm of brilliance on */

	if (PlayerCannotUseSkills) tmp -= 1;
	else switch (P_SKILL(spell_skilltype(booktype))) {
		case P_ISRESTRICTED:
		case P_UNSKILLED:   tmp -= 1; break;
		case P_BASIC:       break;
		case P_SKILLED:     tmp +=  1; break;
		case P_EXPERT:      tmp +=  2; break;
		case P_MASTER:      tmp +=  3; break;
		case P_GRAND_MASTER:      tmp +=  4; break;
		case P_SUPREME_MASTER:      tmp +=  5; break;
	}
	if (tmp > 0) tmp = rn2(tmp + 1); /* too high bonuses make spellcaster roles too powerful! --Amy */

    return tmp;
}

/*
 * Generate the to hit bonus for a spell.  Based on the hero's skill in
 * spell class and dexterity.
 */
STATIC_OVL int
spell_hit_bonus(skill)
int skill;
{
    int hit_bon = 0;
    int dex = ACURR(A_DEX);

	if (PlayerCannotUseSkills) hit_bon = -4;
    else switch (P_SKILL(spell_skilltype(skill))) {
	case P_ISRESTRICTED:
	case P_UNSKILLED:   hit_bon = -4; break;
	case P_BASIC:       hit_bon =  0; break;
	case P_SKILLED:     hit_bon =  2; break;
	case P_EXPERT:      hit_bon =  3; break;
	case P_MASTER:      hit_bon =  5; break;
	case P_GRAND_MASTER:      hit_bon =  7; break;
	case P_SUPREME_MASTER:      hit_bon =  10; break;
    }

    if (dex < 4)
	hit_bon -= 3;
    else if (dex < 6)
	hit_bon -= 2;
    else if (dex < 8)
	hit_bon -= 1;
    else if (dex < 14)
	hit_bon -= 0;		/* Will change when print stuff below removed */
    else
	hit_bon += dex - 14; /* Even increment for dextrous heroes (see weapon.c abon) */

    return hit_bon;
}

const char *
exclam(force)
register int force;
{
	/* force == 0 occurs e.g. with sleep ray */
	/* note that large force is usual with wands so that !! would
		require information about hand/weapon/wand */
	return (const char *)((force < 0) ? "?" : (force <= 4) ? "." : "!");
}

void
hit(str,mtmp,force)
register const char *str;
register struct monst *mtmp;
register const char *force;		/* usually either "." or "!" */
{
	if((!cansee(bhitpos.x,bhitpos.y) && !canspotmon(mtmp) &&
	     !(u.uswallow && mtmp == u.ustuck))
	   || !flags.verbose)
	    pline("%s %s it.", The(str), vtense(str, "hit"));
	else pline("%s %s %s%s", The(str), vtense(str, "hit"),
		   mon_nam(mtmp), force);
}

void
miss(str,mtmp)
register const char *str;
register struct monst *mtmp;
{
	pline("%s %s %s.", The(str), vtense(str, "miss"),
	      ((cansee(bhitpos.x,bhitpos.y) || canspotmon(mtmp))
	       && flags.verbose) ?
	      mon_nam(mtmp) : "it");
}
#endif /*OVL0*/
#ifdef OVL1

/*
 *  Called for the following distance effects:
 *	when a weapon is thrown (weapon == THROWN_WEAPON)
 *	when an object is kicked (KICKED_WEAPON)
 *	when an IMMEDIATE wand is zapped (ZAPPED_WAND)
 *	when a light beam is flashed (FLASHED_LIGHT)
 *	when a mirror is applied (INVIS_BEAM)
 *  A thrown/kicked object falls down at the end of its range or when a monster
 *  is hit.  The variable 'bhitpos' is set to the final position of the weapon
 *  thrown/zapped.  The ray of a wand may affect (by calling a provided
 *  function) several objects and monsters on its path.  The return value
 *  is the monster hit (weapon != ZAPPED_WAND), or a null monster pointer.
 *  Thrown and kicked objects (THROWN_WEAPON or KICKED_WEAPON) may be
 *  destroyed and *obj_p set to NULL to indicate this.
 *
 *  Check !u.uswallow before calling bhit().
 *  This function reveals the absence of a remembered invisible monster in
 *  necessary cases (throwing or kicking weapons).  The presence of a real
 *  one is revealed for a weapon, but if not a weapon is left up to fhitm().
 */
struct monst *
bhit(ddx,ddy,range,weapon,fhitm,fhito,obj_p)
register int ddx,ddy,range;		/* direction and range */
int weapon;				/* see values in hack.h */
int (*fhitm)(MONST_P, OBJ_P),	/* fns called when mon/obj hit */
    (*fhito)(OBJ_P, OBJ_P);
struct obj **obj_p;			/* object tossed/used */
{
	struct monst *mtmp;
	struct obj *obj = *obj_p;
	uchar typ;
	boolean shopdoor = FALSE, point_blank = TRUE;
#ifdef LIGHT_SRC_SPELL
        int lits = 0;
        boolean use_lights = FALSE;
#endif

	if (weapon == KICKED_WEAPON) {
	    /* object starts one square in front of player */
	    bhitpos.x = u.ux + ddx;
	    bhitpos.y = u.uy + ddy;
	    range--;
	} else {
	    bhitpos.x = u.ux;
	    bhitpos.y = u.uy;
	}

	if (weapon == FLASHED_LIGHT) {
#ifdef LIGHT_SRC_SPELL
	    use_lights = TRUE;
#endif
	    tmp_at(DISP_BEAM, cmap_to_glyph(S_flashbeam));
	} else if (weapon != ZAPPED_WAND && weapon != INVIS_BEAM) {
#ifdef LIGHT_SRC_SPELL
	    use_lights = obj->lamplit;
#endif
	    tmp_at(DISP_FLASH, obj_to_glyph(obj));
	}

	while(range-- > 0) {
	    int x,y;

	    bhitpos.x += ddx;
	    bhitpos.y += ddy;
	    x = bhitpos.x; y = bhitpos.y;

	    /*pline("x %d, y %d", bhitpos.x, bhitpos.y);*/

	    if(!isok(x, y)) {
		bhitpos.x -= ddx;
		bhitpos.y -= ddy;
		break;
	    }

	    if(is_pick(obj) && inside_shop(x, y) &&
					   (mtmp = shkcatch(obj, x, y))) {
		tmp_at(DISP_END, 0);
		return(mtmp);
	    }

	    typ = levl[bhitpos.x][bhitpos.y].typ;

	    /* iron bars will block anything big enough */
	    if ((weapon == THROWN_WEAPON || weapon == KICKED_WEAPON) &&
		    typ == IRONBARS &&
		    hits_bars(obj_p, x - ddx, y - ddy,
			      point_blank ? 0 : !rn2(5), 1)) {
		/* caveat: obj might now be null... */
		obj = *obj_p;
		bhitpos.x -= ddx;
		bhitpos.y -= ddy;
		break;
	    }

	    if (weapon == ZAPPED_WAND && find_drawbridge(&x,&y))
		switch (obj->otyp) {
		    case WAN_OPENING:
		    case SPE_KNOCK:
			if (is_db_wall(bhitpos.x, bhitpos.y)) {
			    if (cansee(x,y) || cansee(bhitpos.x,bhitpos.y))
				makeknown(obj->otyp);
			    open_drawbridge(x,y);
			}
			break;
		    case WAN_LOCKING:
		    case SPE_WIZARD_LOCK:
			if ((cansee(x,y) || cansee(bhitpos.x, bhitpos.y))
			    && levl[x][y].typ == DRAWBRIDGE_DOWN)
			    makeknown(obj->otyp);
			close_drawbridge(x,y);
			break;

		    case SPE_LOCK_MANIPULATION:

			if (!rn2(2)) {
				if (is_db_wall(bhitpos.x, bhitpos.y)) {
				    if (cansee(x,y) || cansee(bhitpos.x,bhitpos.y))
					makeknown(obj->otyp);
				    open_drawbridge(x,y);
				}

			} else {
				if ((cansee(x,y) || cansee(bhitpos.x, bhitpos.y))
				    && levl[x][y].typ == DRAWBRIDGE_DOWN)
				    makeknown(obj->otyp);
				close_drawbridge(x,y);

			}

			break;

		    case WAN_STRIKING:
		    case SPE_FORCE_BOLT:
		    case WAN_GRAVITY_BEAM:
		    case SPE_GRAVITY_BEAM:
			 case WAN_WIND:
			 case SPE_WIND:
			if (typ != DRAWBRIDGE_UP)
			    destroy_drawbridge(x,y);
			makeknown(obj->otyp);
			break;
		}

	    if ((mtmp = m_at(bhitpos.x, bhitpos.y)) != 0) {
		notonhead = (bhitpos.x != mtmp->mx ||
			     bhitpos.y != mtmp->my);
		if (weapon != FLASHED_LIGHT) {
			if(weapon != ZAPPED_WAND) {
			    if(weapon != INVIS_BEAM) {
				tmp_at(DISP_END, 0);
#ifdef LIGHT_SRC_SPELL
				if (use_lights) {
				    while (lits) {
					del_light_source(LS_TEMP,
						(void *) lits);
					lits--;
				    }
				    vision_full_recalc = 1;
				    vision_recalc(0);	/* clean up vision */
				}
#endif
			    }
			    if (cansee(bhitpos.x,bhitpos.y) && !canspotmon(mtmp)) {
				if (weapon != INVIS_BEAM) {
				    map_invisible(bhitpos.x, bhitpos.y);
				    return(mtmp);
				}
			    } else
				return(mtmp);
			}
			if (weapon != INVIS_BEAM) {
			    (*fhitm)(mtmp, obj);
				if (uarmg && itemhasappearance(uarmg, APP_RAYDUCTNAY_GLOVES) ) range -= 0;
			    else if (tech_inuse(T_BLADE_ANGER) && obj->otyp == SPE_BLANK_PAPER) range -= 1;
			    else if (tech_inuse(T_BEAMSWORD) && obj->otyp == SPE_BLANK_PAPER) range -= 1;
			    else range -= 3;
			}
		} else {
		    /* FLASHED_LIGHT hitting invisible monster
		       should pass through instead of stop so
		       we call flash_hits_mon() directly rather
		       than returning mtmp back to caller. That
		       allows the flash to keep on going. Note
		       that we use mtmp->minvis not canspotmon()
		       because it makes no difference whether
		       the hero can see the monster or not.*/
		    if (mtmp->minvis) {
			obj->ox = u.ux,  obj->oy = u.uy;
			(void) flash_hits_mon(mtmp, obj);
		    } else {
			tmp_at(DISP_END, 0);
		    	return(mtmp); 	/* caller will call flash_hits_mon */
		    }
		}
	    } else {
		if (weapon == ZAPPED_WAND && obj->otyp == WAN_PROBING &&
		   memory_is_invisible(bhitpos.x, bhitpos.y)) {
		    unmap_object(bhitpos.x, bhitpos.y);
		    newsym(x, y);
		}
	    }
	    if(fhito) {
		if(bhitpile(obj,fhito,bhitpos.x,bhitpos.y))
		    if (!(uarmg && itemhasappearance(uarmg, APP_RAYDUCTNAY_GLOVES) )) range--;
	    } else {
		if(weapon == KICKED_WEAPON &&
		      ((obj->oclass == COIN_CLASS &&
			 OBJ_AT(bhitpos.x, bhitpos.y)) ||
			    ship_object(obj, bhitpos.x, bhitpos.y,
					costly_spot(bhitpos.x, bhitpos.y)))) {
			tmp_at(DISP_END, 0);
			return (struct monst *)0;
		}
	    }
	    if(weapon == ZAPPED_WAND && (IS_DOOR(typ) || typ == SDOOR)) {
		switch (obj->otyp) {
		case WAN_OPENING:
		case WAN_LOCKING:
		case WAN_STRIKING:
		case WAN_GRAVITY_BEAM:
		case SPE_GRAVITY_BEAM:
		case SPE_KNOCK:
		case SPE_LOCK_MANIPULATION:
		case SPE_WIZARD_LOCK:
		case SPE_FORCE_BOLT:
		case WAN_WIND:
		case SPE_WIND:
		    if (doorlock(obj, bhitpos.x, bhitpos.y)) {
			if (cansee(bhitpos.x, bhitpos.y) ||
			    (obj->otyp == WAN_STRIKING))
			    makeknown(obj->otyp);
			if (levl[bhitpos.x][bhitpos.y].doormask == D_BROKEN
			    && *in_rooms(bhitpos.x, bhitpos.y, SHOPBASE)) {
			    shopdoor = TRUE;
			    add_damage(bhitpos.x, bhitpos.y, 400L);
			}
		    }
		    break;
		}
	    }
	    if(weapon == ZAPPED_WAND && obj->otyp == WAN_OPENING) {
		if (rn2(2) && (typ >= STONE) && (typ <= ROCKWALL) && !(*in_rooms(bhitpos.x,bhitpos.y,SHOPBASE)) && ((levl[bhitpos.x][bhitpos.y].wall_info & W_NONDIGGABLE) == 0) ) {
			levl[bhitpos.x][bhitpos.y].typ = CORR;
			unblock_point(bhitpos.x,bhitpos.y);
			if (!(levl[bhitpos.x][bhitpos.y].wall_info & W_HARDGROWTH)) levl[bhitpos.x][bhitpos.y].wall_info |= W_EASYGROWTH;
			newsym(bhitpos.x,bhitpos.y);

		}
	    }

	    if(weapon == ZAPPED_WAND && (obj->otyp == WAN_BUBBLEBEAM || obj->otyp == SPE_BUBBLEBEAM ) ) {
		if (!rn2(10) && (levl[bhitpos.x][bhitpos.y].typ == ROOM || levl[bhitpos.x][bhitpos.y].typ == CORR) ) {
			levl[bhitpos.x][bhitpos.y].typ = POOL;
		}
	    }

	    if(weapon == ZAPPED_WAND && (obj->otyp == SPE_WATER_BOLT ) ) {
		if (levl[bhitpos.x][bhitpos.y].typ == ROOM || levl[bhitpos.x][bhitpos.y].typ == CORR) {
			levl[bhitpos.x][bhitpos.y].typ = POOL;
		}
	    }

	    if(weapon == ZAPPED_WAND && (obj->otyp == WAN_GOOD_NIGHT || obj->otyp == SPE_GOOD_NIGHT ) ) {
		    levl[bhitpos.x][bhitpos.y].lit = 0;
	    }

	    if(weapon == ZAPPED_WAND && obj->otyp == WAN_LOCKING) {
		if (rn2(2) && ((levl[bhitpos.x][bhitpos.y].wall_info & W_NONDIGGABLE) == 0) && (typ == ROOM || typ == CORR) ) {

			levl[bhitpos.x][bhitpos.y].typ = STONE;
			block_point(bhitpos.x,bhitpos.y);
			if (!(levl[bhitpos.x][bhitpos.y].wall_info & W_EASYGROWTH)) levl[bhitpos.x][bhitpos.y].wall_info |= W_HARDGROWTH;
			del_engr_at(bhitpos.x,bhitpos.y);
			newsym(bhitpos.x,bhitpos.y);
		}
	    }

	    if( (!ZAP_POS(typ) && !(obj->otyp == WAN_OPENING))  || closed_door(bhitpos.x, bhitpos.y)) {
		bhitpos.x -= ddx;
		bhitpos.y -= ddy;
		break;
	    }
	    if(weapon != ZAPPED_WAND && weapon != INVIS_BEAM) {
		/* 'I' present but no monster: erase */
		/* do this before the tmp_at() */
		if (memory_is_invisible(bhitpos.x, bhitpos.y)
			&& cansee(x, y)) {
		    unmap_object(bhitpos.x, bhitpos.y);
		    newsym(x, y);
		}
#ifdef LIGHT_SRC_SPELL
		if (use_lights) {
		    lits++;
		    new_light_source(bhitpos.x, bhitpos.y, 1,
				LS_TEMP, (void *) lits);
		    vision_full_recalc = 1;
		    vision_recalc(0);
		}
#endif
		tmp_at(bhitpos.x, bhitpos.y);
		delay_output();
		/* kicked objects fall in pools */
		if((weapon == KICKED_WEAPON) &&
		   (is_waterypool(bhitpos.x, bhitpos.y) ||
		   is_lava(bhitpos.x, bhitpos.y)))
		    break;
		if(IS_SINK(typ) && weapon != FLASHED_LIGHT)
		    break;	/* physical objects fall onto sink */
		if(IS_WATERTUNNEL(typ) && weapon != FLASHED_LIGHT)
		    break;
	    }
	    /* limit range of ball so hero won't make an invalid move */
	    if (weapon == THROWN_WEAPON && range > 0 &&
		obj->otyp == HEAVY_IRON_BALL) {
		struct obj *bobj;
		struct trap *t;
		if ((bobj = sobj_at(BOULDER, x, y)) != 0) {
		    if (cansee(x,y))
			pline("%s hits %s.",
			      The(distant_name(obj, xname)), an(xname(bobj)));
		    range = 0;
		} else if (obj == uball) {
		    if (!test_move(x - ddx, y - ddy, ddx, ddy, TEST_MOVE)) {
			/* nb: it didn't hit anything directly */
			if (cansee(x,y))
			    pline("%s jerks to an abrupt halt.",
				  The(distant_name(obj, xname))); /* lame */
			range = 0;
		    } else if (In_sokoban(&u.uz) && (t = t_at(x, y)) != 0 &&
			       (t->ttyp == PIT || t->ttyp == SPIKED_PIT || t->ttyp == GIANT_CHASM || t->ttyp == SHIT_PIT || t->ttyp == SHAFT_TRAP || t->ttyp == CURRENT_SHAFT ||
				t->ttyp == HOLE || t->ttyp == TRAPDOOR)) {
			/* hero falls into the trap, so ball stops */
			range = 0;
		    }
		}
	    }

	    /* thrown/kicked missile has moved away from its starting spot */
	    point_blank = FALSE;	/* affects passing through iron bars */
	}

        if (weapon != ZAPPED_WAND && weapon != INVIS_BEAM) {
                tmp_at(DISP_END, 0);
#ifdef LIGHT_SRC_SPELL
                while (lits) {
                        del_light_source(LS_TEMP, (void *) lits);
                        lits--;
                }
                vision_full_recalc = 1;
                vision_recalc(0); /*clean up vision*/
#endif
        }

	if(shopdoor)
	    pay_for_damage("destroy", FALSE);

	return (struct monst *)0;
}

struct monst *
boomhit(dx, dy)
int dx, dy;
{
	register int i, ct;
	int boom = S_boomleft;	/* showsym[] index  */
	struct monst *mtmp;

	bhitpos.x = u.ux;
	bhitpos.y = u.uy;

	for(i=0; i<8; i++) if(xdir[i] == dx && ydir[i] == dy) break;
	tmp_at(DISP_FLASH, cmap_to_glyph(boom));
	for(ct=0; ct<10; ct++) {
		if(i == 8) i = 0;
		boom = (boom == S_boomleft) ? S_boomright : S_boomleft;
		tmp_at(DISP_CHANGE, cmap_to_glyph(boom));/* change glyph */
		dx = xdir[i];
		dy = ydir[i];
		bhitpos.x += dx;
		bhitpos.y += dy;
		if(MON_AT(bhitpos.x, bhitpos.y)) {
			mtmp = m_at(bhitpos.x,bhitpos.y);
			m_respond(mtmp);
			tmp_at(DISP_END, 0);
			return(mtmp);
		}
		if(!ZAP_POS(levl[bhitpos.x][bhitpos.y].typ) ||
		   closed_door(bhitpos.x, bhitpos.y)) {
			bhitpos.x -= dx;
			bhitpos.y -= dy;
			break;
		}
		if(bhitpos.x == u.ux && bhitpos.y == u.uy) { /* ct == 9 */
			if(Fumbling || rn2(20) >= ACURR(A_DEX)) {
				/* we hit ourselves */
				(void) thitu(10, rnd(10), (struct obj *)0,
					"boomerang");
				break;
			} else {	/* we catch it */
				tmp_at(DISP_END, 0);
				You("skillfully catch the boomerang.");
				return(&youmonst);
			}
		}
		tmp_at(bhitpos.x, bhitpos.y);
		delay_output();
		if(ct % 5 != 0) i++;
		if(IS_SINK(levl[bhitpos.x][bhitpos.y].typ))
			break;	/* boomerang falls on sink */
		if(IS_WATERTUNNEL(levl[bhitpos.x][bhitpos.y].typ))
			break;
	}
	tmp_at(DISP_END, 0);	/* do not leave last symbol */
	return (struct monst *)0;
}

STATIC_OVL int
zhitm(mon, type, nd, ootmp)			/* returns damage to mon */
register struct monst *mon;
register int type, nd;
struct obj **ootmp;	/* to return worn armor for caller to disintegrate */
{
	register int tmp = 0;
	register int abstype = abs(type) % 10;
	boolean sho_shieldeff = FALSE;
	boolean spellcaster = (is_hero_spell(type) || is_mega_spell(type)); 
				/* maybe get a bonus! */
    	int skilldmg = 0;

	*ootmp = (struct obj *)0;
	switch(abstype) {
	case ZT_MAGIC_MISSILE:
		if (resists_magm(mon)) {
		    sho_shieldeff = TRUE;
		    break;
		}
		tmp = d(nd,6);
		if (spellcaster)
		    skilldmg = spell_damage_bonus(SPE_MAGIC_MISSILE);
		break;
	case ZT_FIRE:
		if (resists_fire(mon)) {
		    sho_shieldeff = TRUE;
		    break;
		}
		tmp = d(nd,6);
		if (resists_cold(mon)) tmp += 7;
		if (spellcaster)
		    skilldmg = spell_damage_bonus(SPE_FIREBALL);
		if (!rn2(33)) (burnarmor(mon));
	    if (!rn2(33)) (void)destroy_mitem(mon, POTION_CLASS, AD_FIRE);
	    if (!rn2(33)) (void)destroy_mitem(mon, SCROLL_CLASS, AD_FIRE);
	    if (!rn2(50)) (void)destroy_mitem(mon, SPBOOK_CLASS, AD_FIRE);
		break;
	case ZT_COLD:
		if (resists_cold(mon)) {
		    sho_shieldeff = TRUE;
		    break;
		}
		tmp = d(nd,6);
		if (resists_fire(mon)) tmp += d(nd, 3);
		if (spellcaster)
		    skilldmg = spell_damage_bonus(SPE_CONE_OF_COLD);
		if (!rn2(75)) (void)destroy_mitem(mon, POTION_CLASS, AD_COLD);
		break;
	case ZT_SLEEP:
		tmp = 0;
		(void)sleep_monst(mon, d(nd, 6),
				type == ZT_WAND(ZT_SLEEP) ? WAND_CLASS : '\0');
		break;
	case ZT_DEATH:		/* death/disintegration */
		if(abs(type) != ZT_BREATH(ZT_DEATH)) {	/* death */
		    if(mon->data == &mons[PM_DEATH]) {
			mon->mhpmax += mon->mhpmax/2;
			if (mon->mhpmax >= /*MAGIC_COOKIE*/50000)
			    mon->mhpmax = /*MAGIC_COOKIE - 1*/49999;
			mon->mhp = mon->mhpmax;
			break;
		    }
		    if (nonliving(mon->data) || is_demon(mon->data) ||
			    resists_death(mon) || mon->data->msound == MS_NEMESIS ||
			    resists_magm(mon)) {	/* similar to player */
			sho_shieldeff = TRUE;
			break;
		    }
		    type = -1; /* so they don't get saving throws */
		} else {
		    struct obj *otmp2;

		    if (resists_disint(mon)) {
			sho_shieldeff = TRUE;
		    } else if (mon->misc_worn_check & W_ARMS) {
			/* destroy shield; victim survives */
			*ootmp = which_armor(mon, W_ARMS);
		    } else if (mon->misc_worn_check & W_ARMC) {
			/* destroy shield; victim survives */
			*ootmp = which_armor(mon, W_ARMC);
		    } else if (mon->misc_worn_check & W_ARM) {
			/* destroy shield; victim survives */
			*ootmp = which_armor(mon, W_ARM);
		    } else if (mon->misc_worn_check & W_ARMU) {
			/* destroy shield; victim survives */
			*ootmp = which_armor(mon, W_ARMU);
		    } else {
			/* no body armor, victim dies; destroy cloak
			   and shirt now in case target gets life-saved */
			tmp = MAGIC_COOKIE;
			if ((otmp2 = which_armor(mon, W_ARMC)) != 0)
			    m_useup(mon, otmp2);
			if ((otmp2 = which_armor(mon, W_ARMU)) != 0)
			    m_useup(mon, otmp2);
		    }
		    type = -1;	/* no saving throw wanted */
		    break;	/* not ordinary damage */
		}
		tmp = mon->mhp+1;
		break;
	case ZT_LIGHTNING:
		if (resists_elec(mon)) {
		    sho_shieldeff = TRUE;
		    tmp = 0;
		    /* can still blind the monster */
		} else
		    tmp = d(nd,6);
		if (spellcaster && !resists_elec(mon))
		    skilldmg = spell_damage_bonus(SPE_LIGHTNING);
		if (!resists_blnd(mon) && !rn2(5) && !(type > 0 && u.uswallow && mon == u.ustuck)) {
			register unsigned rnd_tmp = rnd(10);
			mon->mcansee = 0;
			if((mon->mblinded + rnd_tmp) > 127)
				mon->mblinded = 127;
			else mon->mblinded += rnd_tmp;
		}

		if (resists_elec(mon)) break;

		if (!rn2(100)) (void)destroy_mitem(mon, WAND_CLASS, AD_ELEC);
		/* not actually possible yet */
		if (!rn2(100)) (void)destroy_mitem(mon, RING_CLASS, AD_ELEC);
		break;
	case ZT_POISON_GAS:
		if (resists_poison(mon)) {
		    sho_shieldeff = TRUE;
		    break;
		}
		tmp = d(nd,6);
		break;
	case ZT_ACID:
		if (resists_acid(mon)) {
		    sho_shieldeff = TRUE;
		    break;
		}
		tmp = d(nd,6);
		if (!rn2(6)) erode_obj(MON_WEP(mon), TRUE, TRUE);
		if (!rn2(6)) erode_armor(mon, TRUE);
		break;
	case ZT_LITE:
		if (dmgtype(mon->data, AD_LITE) ) {
		    sho_shieldeff = TRUE;
		    break;
		}
		if (mon->data->mlet == S_LIGHT || emits_light(mon->data) ) { /* suggested by maxlunar IIRC? */
		    sho_shieldeff = TRUE;
		    break;
		}

		{
			boolean literes = FALSE;
			tmp = d(nd,8);

			if (tmp > 1 && dmgtype(mon->data, AD_DARK)) {
				tmp /= 10;
				if (tmp < 1) tmp = 1;
				literes = TRUE;
			}
			if (tmp > 1 && !haseyes(mon->data)) {
				tmp /= 10;
				if (tmp < 1) tmp = 1;
				literes = TRUE;
			}
			if (tmp > 1 && mon->mblinded) {
				tmp /= 10;
				if (tmp < 1) tmp = 1;
				literes = TRUE;
			}
			if (literes) pline("%s resists a lot.", Monnam(mon));

			if (is_vampire(mon->data)) {
			tmp *= 2; /* vampires take more damage from sunlight --Amy */
			pline(literes ? "The light ray slightly burns %s's pale skin." : "The light ray sears %s's pale skin!",mon_nam(mon));
			}

		}
		break;
	case ZT_SPC2:
		if (dmgtype(mon->data, AD_SPC2) || dmgtype(mon->data, AD_INSA) || dmgtype(mon->data, AD_SANI) || mindless(mon->data) ) {
		    sho_shieldeff = TRUE;
		    break;
		}
		tmp = d(nd,7);
		pline("The beam confuses %s!",mon_nam(mon));
		mon->mconf = TRUE;
		if (Role_if(PM_PSION)) mon->mstun = TRUE;
		break;
	}
	if (sho_shieldeff) shieldeff(mon->mx, mon->my);
	if (spellcaster && (Role_if(PM_KNIGHT) && u.uhave.questart))
	    tmp *= 2;

#ifdef WIZ_PATCH_DEBUG
	if (spellcaster)
	    pline("Damage = %d + %d", tmp, skilldmg);
#endif
	tmp += skilldmg;
	    
	if (tmp > 0 && type >= 0 &&
		resist(mon, type < ZT_SPELL(0) ? WAND_CLASS : '\0', 0, NOTELL))
	    tmp /= 2;

	/* magic missile spell, which is too strong relative to all others --Amy */
	if (tmp > 1 && !issoviet && type == 10 && resist(mon, '\0', 0, NOTELL) ) {
	/* In Soviet Russia, people have no consideration for game balance. They insist that the magic missile spell,
	 * despite being a lower level than all other attack spells that shoot damaging beams, absolutely has to do the same
	 * amount of damage, even though it's already better for another reason and that is the fact that few monsters have
	 * magic resistance while the elemental resistances (fire, cold etc.) are ubiquitous! --Amy */
		tmp /= 2;
	}

	/* but it's still too strong, so we'll nerf it some more --Amy */
	if (tmp > 1 && !rn2(2) && !issoviet && type == 10) {

		tmp /= 2;
		if (tmp > 1 && !rn2(5)) tmp /= 2;

	}

	if (tmp < 0) tmp = 0;		/* don't allow negative damage */
#ifdef WIZ_PATCH_DEBUG
	pline("zapped monster hp = %d (= %d - %d)", mon->mhp-tmp,mon->mhp,tmp);
#endif

	mon->mhp -= tmp;

#ifdef SHOW_DMG
	if (mon->mhp > 0) showdmg(tmp);
#endif

	return(tmp);
}

STATIC_OVL void
zhitu(type, nd, fltxt, sx, sy)
int type, nd;
const char *fltxt;
xchar sx, sy;
{
	int dam = 0;

	/* KMH -- Don't bother checking if the ray hit our steed.
	 * 1.  Assume monsters aim to hit you.
	 * 2.  If hit by own beam, let the hero suffer.
	 * 3.  Falling off of the steed could relocate the hero
	 *     so he is hit twice, which is unrealistic.
	 */

	switch (abs(type) % 10) {
	case ZT_MAGIC_MISSILE:
	    if (Antimagic && rn2(StrongAntimagic ? 20 : 5)) {
		shieldeff(sx, sy);
		pline_The("missiles bounce off!");
	    } else {
		dam = d(nd,6);
		exercise(A_STR, FALSE);
	    }
	    break;
	case ZT_FIRE:
	    if (Fire_resistance && rn2(StrongFire_resistance ? 20 : 5)) {
		shieldeff(sx, sy);
		You("don't feel hot!");
		ugolemeffects(AD_FIRE, d(nd, 6));
	    } else {
		dam = d(nd, 6);
	    }
	    if (Slimed) {            
		pline("The slime is burned away!");
		Slimed = 0;
	    }
	    burn_away_slime();
	    if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 2 : 33)) (burnarmor(&youmonst));	/* "body hit" */
		if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 6 : 33)) destroy_item(POTION_CLASS, AD_FIRE);
		if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 6 : 33)) destroy_item(SCROLL_CLASS, AD_FIRE);
		if (isevilvariant || !rn2(Race_if(PM_SEA_ELF) ? 1 : issoviet ? 10 : 50)) destroy_item(SPBOOK_CLASS, AD_FIRE);
	    break;
	case ZT_COLD:
	    if (Cold_resistance && rn2(20)) {
		shieldeff(sx, sy);
		You("don't feel cold.");
		ugolemeffects(AD_COLD, d(nd, 6));
	    } else {
		dam = d(nd, 6);
		if (Race_if(PM_GAVIL)) dam *= 2;
		if (Race_if(PM_HYPOTHERMIC)) dam *= 3;
	    }
	    if (isevilvariant || !rn2(issoviet ? 15 : Race_if(PM_GAVIL) ? 15 : Race_if(PM_HYPOTHERMIC) ? 15 : 75)) destroy_item(POTION_CLASS, AD_COLD);
	    break;
	case ZT_SLEEP:
	    if (Sleep_resistance && rn2(StrongSleep_resistance ? 20 : 5)) {
		shieldeff(u.ux, u.uy);
		You("don't feel sleepy.");
	    } else {
		if (flags.moreforced && !MessagesSuppressed) display_nhwindow(WIN_MESSAGE, TRUE);    /* --More-- */
		fall_asleep(-rnd(5+nd), TRUE); /* sleep ray */
	    }
	    break;
	case ZT_DEATH:
	    if (abs(type) == ZT_BREATH(ZT_DEATH)) {

		dam = d(nd, StrongDisint_resistance ? 2 : Disint_resistance ? 3 : 6);
		losehp(dam, "a disintegration beam", KILLED_BY);

		if (Disint_resistance && rn2(StrongDisint_resistance ? 1000 : 100) && !(evilfriday && (uarms || uarmc || uarm || uarmu))) {
		    You("are not disintegrated.");
		    break;
            } else if (Invulnerable || (Stoned_chiller && Stoned)) {
                pline("You are unharmed!");
                break;
		} else if (uarms) {
		    /* destroy shield; other possessions are safe */
		    if (!(EDisint_resistance & W_ARMS)) (void) destroy_arm(uarms);
		    break;
		} else if (uarmc) {
		    /* destroy cloak; other possessions are safe */
		    if (!(EDisint_resistance & W_ARMC)) (void) destroy_arm(uarmc);
		    break;
		} else if (uarm) {
		    /* destroy suit */
		    if (!(EDisint_resistance & W_ARM)) (void) destroy_arm(uarm);
		    break;
		} else if (uarmu) {
		    /* destroy shirt */
		    if (!(EDisint_resistance & W_ARMU)) (void) destroy_arm(uarmu);
		    break;
		} else if (u.uhpmax > 20) {
			u.uhpmax -= rnd(20);
			if (u.uhp > u.uhpmax) u.uhp = u.uhpmax;
			losehp(rnz(100 + level_difficulty()), "a termination beam", KILLED_BY);
			break;
		}

	    } else if (nonliving(youmonst.data) || is_demon(youmonst.data) || Death_resistance) {
		shieldeff(sx, sy);
		You("seem unaffected.");
		break;
	    } else if (Antimagic) { /* Sorry people, but being magic resistant no longer makes you immune. --Amy */
	            dam = d(2,4) + rno(level_difficulty() + 1);
			if (StrongAntimagic && dam > 1) dam /= 2;
			u.uhpmax -= dam/2;
			if (u.uhp > u.uhpmax) u.uhp = u.uhpmax;
	            pline("You resist the attack, but it hurts!");
            break;
            } else if (Invulnerable || (Stoned_chiller && Stoned)) {
                dam = 0;
                pline("You are unharmed!");
                break;
	    }

		if (!rn2(20) || abs(type) == ZT_BREATH(ZT_DEATH) ) {
		u.youaredead = 1;
	    killer_format = KILLED_BY_AN;
	    killer = fltxt;
	    /* when killed by disintegration breath, don't leave corpse */
	    u.ugrave_arise = (type == -ZT_BREATH(ZT_DEATH)) ? -3 : NON_PM;
	    done(DIED);
		u.youaredead = 0;
	    return; /* lifesaved */
		}
		else
                dam = d(4,6) + rnd(level_difficulty() + 1);
			u.uhpmax -= dam/2;
			if (u.uhp > u.uhpmax) u.uhp = u.uhpmax;
                pline("This hurts a lot!");
		break;
	case ZT_LIGHTNING:
	    if (Shock_resistance && rn2(StrongShock_resistance ? 20 : 5)) {
		shieldeff(sx, sy);
		You("aren't affected.");
		ugolemeffects(AD_ELEC, d(nd, 6));
	    } else {
		dam = d(nd, 6);
		exercise(A_CON, FALSE);
	    }
	    if (isevilvariant || !rn2(issoviet ? 20 : 100)) destroy_item(WAND_CLASS, AD_ELEC);
	    if (isevilvariant || !rn2(issoviet ? 20 : 100)) destroy_item(RING_CLASS, AD_ELEC);
	    if (isevilvariant || !rn2(issoviet ? 100 : 500)) destroy_item(AMULET_CLASS, AD_ELEC);
	    break;
	case ZT_POISON_GAS:
	    poisoned("blast", A_DEX, "poisoned blast", 15);
	    break;
	case ZT_ACID:

		if (Stoned) fix_petrification();

		/* KMH, balance patch -- new intrinsic */
	    if (Acid_resistance && rn2(StrongAcid_resistance ? 20 : 5)) {
		dam = 0;
	    } else {
		pline_The("acid burns!");
		dam = d(nd,6);
		exercise(A_STR, FALSE);
	    }
	    /* using two weapons at once makes both of them more vulnerable */
	    if (!rn2(u.twoweap ? 3 : 6)) erode_obj(uwep, TRUE, TRUE);
	    if (u.twoweap && !rn2(3)) erode_obj(uswapwep, TRUE, TRUE);
	    if (!rn2(issoviet ? 2 : 6)) erode_armor(&youmonst, TRUE);
	    break;
	case ZT_LITE:

		if (uarmh && uarmh->oartifact == ART_SECURE_BATHMASTER && rn2(20) ) {
			dam = 0;
		} else {
			pline_The("irradiation hurts like hell!");
			dam = d(nd,8);
			if (maybe_polyd(is_vampire(youmonst.data), Race_if(PM_VAMPIRE)) || Role_if(PM_GOFF) ) {
				dam *= 2; /* vampires are susceptible to sunlight --Amy */
				pline("Your pale skin is seared by the light ray!");
			}
		}

	    break;
	case ZT_SPC2:

		if (Psi_resist && rn2(StrongPsi_resist ? 100 : 20)) {
			shieldeff(sx, sy);
			pline("You aren't affected.");
			dam = 0;
		} else {

		dam = d(nd,7);
		pline("Your mind is hit by psionic waves!");
		switch (rnd(10)) {

			case 1:
			case 2:
			case 3:
				make_confused(HConfusion + dam, FALSE);
				break;
			case 4:
			case 5:
			case 6:
				make_stunned(HStun + dam, FALSE);
				break;
			case 7:
				make_confused(HConfusion + dam, FALSE);
				make_stunned(HStun + dam, FALSE);
				break;
			case 8:
				make_hallucinated(HHallucination + dam, FALSE, 0L);
				break;
			case 9:
				make_feared(HFeared + dam, FALSE);
				break;
			case 10:
				make_numbed(HNumbed + dam, FALSE);
				break;

		}
		if (!rn2(200)) {
			forget(rnd(5));
			pline("You forget some important things...");
		}
		if (!rn2(200)) {
			losexp("psionic drain", FALSE, TRUE);
		}
		if (!rn2(200)) {
			adjattrib(A_INT, -1, 1, TRUE);
			adjattrib(A_WIS, -1, 1, TRUE);
		}
		if (!rn2(200)) {
			pline("You scream in pain!");
			wake_nearby();
		}
		if (!rn2(200)) {
			badeffect();
		}
		if (!rn2(5)) increasesanity(rnz(5));

		}

	    break;
	}

	if (type >= 0) {
		if (!issoviet) {
			if (rn2(2)) dam = (dam + 1) / 2;
			else dam = (dam + 2) / 3;
		} else pline("Vy ochen' glupyy chelovek, kotoryy prosto zastrelilsya, i poetomu teper' vy budete stradat'.");
	}

	if (Half_spell_damage && rn2(2) && dam &&
	   type < 0 && (type > -20 || type < -29)) /* !Breath */
	    dam = (dam + 1) / 2;
	if (StrongHalf_spell_damage && rn2(2) && dam &&
	   type < 0 && (type > -20 || type < -29)) /* !Breath */
	    dam = (dam + 1) / 2;

	if (rn2(5) && dam && /* Enemies with wands are deadly enough already. Let's nerf them a bit. --Amy */
	   type < 0 && (type > -20 || type < -29)) /* !Breath */
	    dam = (dam + 1) / 2;
	if (!rn2(5) && dam && 
	   type < 0 && (type > -20 || type < -29)) /* !Breath */
	    dam = (dam + 2) / 3;

	/* when killed by disintegration breath, don't leave corpse */
	u.ugrave_arise = (type == -ZT_BREATH(ZT_DEATH)) ? -3 : -1;
	losehp(dam, fltxt, KILLED_BY_AN);
	return;
}

#endif /*OVL1*/
#ifdef OVLB

/*
 * burn scrolls and spell books on floor at position x,y
 * return the number of scrolls and spell books burned
 */
int
burn_floor_paper(x, y, give_feedback, u_caused)
int x, y;
boolean give_feedback;	/* caller needs to decide about visibility checks */
boolean u_caused;
{
	struct obj *obj, *obj2;
	long i, scrquan, delquan;
	char buf1[BUFSZ], buf2[BUFSZ];
	int cnt = 0;

	for (obj = level.objects[x][y]; obj; obj = obj2) {
	    obj2 = obj->nexthere;
	    if (obj->oclass == SCROLL_CLASS || obj->oclass == SPBOOK_CLASS) {
		if (obj->otyp == SCR_FIRE || obj->otyp == SPE_FIREBALL ||
			obj_resists(obj, 2, 100))
		    continue;
		scrquan = obj->quan;	/* number present */
		delquan = 0;		/* number to destroy */
		for (i = scrquan; i > 0; i--)
		    if (!rn2(issoviet ? 1 : 33)) delquan++;
		if (delquan) {
		    /* save name before potential delobj() */
		    if (give_feedback) {
			obj->quan = 1;
			strcpy(buf1, (x == u.ux && y == u.uy) ?
				xname(obj) : distant_name(obj, xname));
			obj->quan = 2;
		    	strcpy(buf2, (x == u.ux && y == u.uy) ?
				xname(obj) : distant_name(obj, xname));
			obj->quan = scrquan;
		    }
		    /* useupf(), which charges, only if hero caused damage */
		    if (u_caused) useupf(obj, delquan);
		    else if (delquan < scrquan) obj->quan -= delquan;
		    else delobj(obj);
		    cnt += delquan;
		    if (give_feedback) {
			if (delquan > 1)
			    pline("%ld %s burn.", delquan, buf2);
			else
			    pline("%s burns.", An(buf1));
		    }
		}
	    }
	}
	return cnt;
}

/* will zap/spell/breath attack score a hit against armor class `ac'? */
STATIC_OVL int
zap_hit(ac, type)
int ac;
int type;
{
    int chance = rn2(20);
    int spell_bonus = type ? spell_hit_bonus(type) : 0;
    if (!issoviet && !rn2(2)) spell_bonus += rnd(GushLevel); /* otherwise, monsters with good AC are just way too hard to hit --Amy */
    if (!issoviet && !rn2(2)) spell_bonus += rnd(ACURR(A_DEX));
	/* In Soviet Russia, nobody needs a spell bonus of any kind. It's cool if your death rays miss Rodney 90% of the
	 * time! And it's also cool if your wands of magic missile are absolute trash in the late game! --Amy */

    /* small chance for naked target to avoid being hit */
    if (!chance) return rnd(10) < ac+spell_bonus;

    /* very high armor protection does not achieve invulnerability */
    ac = AC_VALUE(ac);

    return (3 - chance) < ac+spell_bonus;
}

STATIC_OVL int
zap_hit_player(ac, type)
int ac;
int type;
{
    int chance = rn2(40);
    if (!rn2(5)) chance += rn2(100);
    int spell_bonus = type ? spell_hit_bonus(type) : 0;

    int yourarmorclass;

    /* small chance for naked target to avoid being hit */
    if (!chance) return rnd(10) < ac+spell_bonus;

    /* very high armor protection does not achieve invulnerability */

	if (u.uac >= 0) yourarmorclass = u.uac;
	else if (u.uac > -40) yourarmorclass = -rnd(-(u.uac));
	else if (u.uac > -80) {
		yourarmorclass = -u.uac;
		yourarmorclass -= rn2(-(u.uac) - 38);
		yourarmorclass = -rnd(yourarmorclass);
	}
	else if (u.uac > -120) {
		yourarmorclass = -u.uac;
		yourarmorclass -= rn3(-(u.uac) - 78);
		yourarmorclass -= rn2(-(u.uac) - 38);
		yourarmorclass = -rnd(yourarmorclass);
	}
	else { /* AC of -120 or better */
		yourarmorclass = -u.uac;
		yourarmorclass -= rn3(-(u.uac) - 118);
		yourarmorclass -= rn3(-(u.uac) - 78);
		yourarmorclass -= rn2(-(u.uac) - 38);
		yourarmorclass = -rnd(yourarmorclass);
	}

	ac = yourarmorclass;

    /* do you have the force field technique active? if yes, 3 out of 4 zaps miss you unconditionally --Amy */
    if (tech_inuse(T_FORCE_FIELD) && rn2(4)) {
	return FALSE;
    }
    if (Race_if(PM_PLAYER_ATLANTEAN) && rn2(2)) {
	return FALSE;
    }

    return (3 - chance) < ac+spell_bonus;
}

/* #endif */
/* type ==   0 to   9 : you shooting a wand */
/* type ==  10 to  19 : you casting a spell */
/* type ==  20 to  29 : you breathing as a monster */
/* type ==  30 to  39 : you casting Super spell -- WAC */
/*                      currently only fireball, cone of cold*/
/* type == -10 to -19 : monster casting spell */
/* type == -20 to -29 : monster breathing at you */
/* type == -30 to -39 : monster shooting a wand */
/* called with dx = dy = 0 with vertical bolts */
void
buzz(type,nd,sx,sy,dx,dy)
register int type, nd;
register xchar sx,sy;
register int dx,dy;
{
    int range, abstype;
    struct rm *lev;
    register xchar lsx, lsy;
    struct monst *mon;
    coord save_bhitpos;
    boolean shopdamage = FALSE;
    register const char *fltxt;
    struct obj *otmp;
#ifdef LIGHT_SRC_SPELL
    int lits = 0;
#endif
/*for mega crude hack to keep from blowing up in face --WAC*/
    int away=0;
    struct monst *mblamed = m_at(sx, sy);	/* Apparent aggressor */
    int spell_type = 0;


      /* LSZ/WWA The Wizard Patch July 96
       * If its a Hero Spell then get its SPE_TYPE
       */
    /* horrible kludge for wands of fireball... */    
    if (type == ZT_WAND(ZT_LIGHTNING+1)) type = ZT_SPELL(ZT_FIRE);
    /*WAC kludge for monsters zapping wands of fireball*/
    if  ((type <= ZT_MONWAND(ZT_FIRST) && type >=ZT_MONWAND(ZT_LAST)) &&
        ( (abs(type) % 10) == ZT_WAND(ZT_LIGHTNING+1))) type = - ZT_SPELL(ZT_FIRE);

    /*WAC bugfix - should show right color stream now (for wands of fireball) */
    abstype = abs(type) % 10;

    if (is_hero_spell(type)  || is_mega_spell(type))
        spell_type = abstype + SPE_MAGIC_MISSILE;

    /*  WAC Whoops...this just catches monster wands ;B */
    fltxt = flash_types[(type <= ZT_MONWAND(ZT_FIRST)) ? abstype : abs(type)];
    if(u.uswallow) {
	register int tmp;

	if(type < 0) return;
	tmp = zhitm(u.ustuck, type, nd, &otmp);
	if(!u.ustuck)	u.uswallow = 0;
	else	pline("%s rips into %s%s",
		      The(fltxt), mon_nam(u.ustuck), exclam(tmp));
	/* Using disintegration from the inside only makes a hole... */
	if (tmp == MAGIC_COOKIE)
	    u.ustuck->mhp = 0;
	if (u.ustuck->mhp < 1)
	    killed(u.ustuck);
	return;
    }
    if(type < 0) newsym(u.ux,u.uy);
    range = EnglandMode ? rn1(10,10) : rn1(7,7);
    if (tech_inuse(T_OVER_RAY)) {
	range *= 3;
	range /= 2;
    }
    if (uarmg && uarmg->oartifact == ART_LONGEST_RAY) {
	range *= 3;
	range /= 2;
    }

    if(dx == 0 && dy == 0) range = 1;
    save_bhitpos = bhitpos;

    if (!is_mega_spell(type)) {
	tmp_at(DISP_BEAM, zapdir_to_glyph(dx, dy, abstype));
    }

    while(range-- > 0) {
        /*hack to keep mega spells from blowing up in your face WAC*/
        away++;

	/* Control sigil */
	if ((away > 4 && !rn2(4)) && tech_inuse(T_SIGIL_CONTROL)) {
sigilcontroldirection:
		if (!getdir((char *)0)) {
			if (yn("Do you really want to input no direction?") == 'y')
				pline("Ah well. You could have controlled the direction of the beam but if you don't wanna...");
			else {
				goto sigilcontroldirection;
			}
		}
		if(u.dx || u.dy) {
			/* Change dir! */
			dx = u.dx; dy = u.dy;
		}
	}

	lsx = sx; sx += dx;
	lsy = sy; sy += dy;
        lev = &levl[sx][sy];
/*new zap code*/
/*        if(isok(sx,sy) && (lev = &levl[sx][sy])->typ) {*/

        if(isok(sx,sy) && (ZAP_POS(lev->typ))) {

#ifdef LIGHT_SRC_SPELL
        /*WAC added light sourcing for the zap...*/
            if (((abstype == ZT_FIRE) || (abstype == ZT_LIGHTNING))
              && (!(type >= ZT_MEGA(ZT_FIRST) && type <= ZT_MEGA(ZT_LAST)))) {
                lits++;
                new_light_source(sx, sy, 1, LS_TEMP, (void *) lits);
                vision_full_recalc = 1;
                vision_recalc(0);
            }
#endif
	    /*Actual Megablast:right now only mag missile to cone of cold WAC*/
	    if (is_mega_spell(type) && away != 1) {
		/*explode takes care of vision stuff*/
		int expl_type;
		expl_type = abstype == ZT_FIRE ? EXPL_FIERY :
			abstype == ZT_COLD ? EXPL_FROSTY : EXPL_MAGICAL;
		explode(sx, sy, type, nd, 0, expl_type);
		delay_output(); /* wait a little */
	    }
	    mon = m_at(sx, sy);
	    /* Normal Zap */
	    if (!is_mega_spell(type) && cansee(sx,sy)) {
		/* reveal/unreveal invisible monsters before tmp_at() */
		if (mon && !canspotmon(mon))
		    map_invisible(sx, sy);
		else if (!mon && memory_is_invisible(sx, sy)) {
		    unmap_object(sx, sy);
		    newsym(sx, sy);
		}
		if(ZAP_POS(lev->typ) || cansee(lsx,lsy)) {
		    tmp_at(sx,sy);
		    delay_output(); /* wait a little */
		}
            }

            /*Clear blast for Megablasts + Fireball*/
            /*Clean for fireball*/
#ifdef LIGHT_SRC_SPELL
	    if (abs(type) == ZT_SPELL(ZT_FIRE)) {
		del_light_source(LS_TEMP, (void *) lits);
		lits--;
	    }
#endif
	} else
	    goto make_bounce;

	/* hit() and miss() need bhitpos to match the target */
	bhitpos.x = sx,  bhitpos.y = sy;

	/* Fireballs only damage when they explode */
        if (abs(type) != ZT_SPELL(ZT_FIRE))
	    range += zap_over_floor(sx, sy, type, &shopdamage);

	if (mon) {
        /* WAC Player/Monster Fireball */
            if (abs(type) == ZT_SPELL(ZT_FIRE)) break;
	    if (type >= 0) mon->mstrategy &= ~STRAT_WAITMASK;
	    buzzmonst:
	    if (zap_hit(find_mac(mon), spell_type)) {
		if (mon_reflects(mon, (char *)0)) {
		    if(cansee(mon->mx,mon->my)) {
			hit(fltxt, mon, exclam(0));
			shieldeff(mon->mx, mon->my);
			(void) mon_reflects(mon, "But it reflects from %s %s!");
		    }
		    dx = -dx;
		    dy = -dy;
		    /* WAC clear the beam so you can see it bounce back ;B */
		    if (!is_mega_spell(type)) {
                    	tmp_at(DISP_END,0);
                    	tmp_at(DISP_BEAM, zapdir_to_glyph(dx, dy, abstype));
		    }
                    delay_output();
		} else {
		    if (type >= 0) ranged_thorns(mon);
		    boolean mon_could_move = mon->mcanmove;
		    int tmp = zhitm(mon, type, nd, &otmp);

/*WAC Since Death is a rider, check for death before disint*/
                    if (mon->data == &mons[PM_DEATH] && abstype == ZT_DEATH && tmp != MAGIC_COOKIE) {
			if (canseemon(mon)) {
			    hit(fltxt, mon, ".");
			    pline("%s absorbs the deadly %s!", Monnam(mon),
/*make this abs(type) rather than just type to catch those monsters*/
/*actually,  breath should always be disintegration....*/
                                  abs(type) == ZT_BREATH(ZT_DEATH) ?
					"blast" : "ray");
			    pline("It seems even stronger than before.");
/*It heals to max hitpoints.  Max hp is raised in zhitm */
			}
			mon->mhp = mon->mhpmax;
			break; /* Out of while loop */
		    }

/*                    if ( (is_rider(mon->data) || is_deadlysin(mon->data)) && abs(type) && type == ZT_BREATH(ZT_DEATH)) {*/
/*WAC rider and disintegration check*/

                    if ( (is_rider(mon->data) || is_deadlysin(mon->data)) && abstype == ZT_DEATH && tmp == MAGIC_COOKIE) {
			if (canseemon(mon)) {
			    hit(fltxt, mon, ".");
			    pline("%s disintegrates.", Monnam(mon));
			    pline("%s body reintegrates before your %s!",
				  s_suffix(Monnam(mon)),
				  (eyecount(youmonst.data) == 1) ?
				  	body_part(EYE) : makeplural(body_part(EYE)));
			    pline("%s resurrects!", Monnam(mon));
			}
			mon->mhp = mon->mhpmax;
			break; /* Out of while loop */
                    } else if (tmp == MAGIC_COOKIE) { /* disintegration */
			struct obj *otmp2, *m_amulet = mlifesaver(mon);

			if (canseemon(mon)) {
			    if (!m_amulet)
				pline("%s is disintegrated!", Monnam(mon));
			    else
				hit(fltxt, mon, "!");
			}
#ifndef GOLDOBJ
			mon->mgold = 0L;
#endif

/* note: worn amulet of life saving must be preserved in order to operate */
#define oresist_disintegration(obj) \
		(objects[obj->otyp].oc_oprop == DISINT_RES || \
		 obj_resists(obj, 5, 50) || is_quest_artifact(obj) || \
		 obj == m_amulet)

			for (otmp = mon->minvent; otmp; otmp = otmp2) {
			    otmp2 = otmp->nobj;
			    if (!oresist_disintegration(otmp) && !stack_too_big(otmp) ) {
				if (Has_contents(otmp)) delete_contents(otmp);
				obj_extract_self(otmp);
				obfree(otmp, (struct obj *)0);
			    }
			}

			if (type < 0)
			    monkilled(mon, (char *)0, -AD_RBRE);
			else
			    xkilled(mon, 2);
		    } else if(mon->mhp < 1) {
			if(type < 0)
			    monkilled(mon, fltxt, AD_RBRE);
			else
			    killed(mon);
		    } else {
			if (!otmp) {
			    /* normal non-fatal hit */
			    hit(fltxt, mon, exclam(tmp));
				wounds_message(mon);
			    if (mblamed && mblamed != mon &&
				    !DEADMONSTER(mblamed) &&
				    mon->movement >= NORMAL_SPEED && rn2(4)) {
				/* retaliate */
				mon->movement -= NORMAL_SPEED;
				mattackm(mon, mblamed);
			    }
			} else {
			    /* some armor was destroyed; no damage done */
			    if (canseemon(mon))
				pline("%s %s is disintegrated!",
				      s_suffix(Monnam(mon)),
				      distant_name(otmp, xname));
			    m_useup(mon, otmp);
			}
			if (mon_could_move && !mon->mcanmove)	/* ZT_SLEEP */
			    slept_monst(mon);
		    }
		}
		if (!(uarmg && itemhasappearance(uarmg, APP_RAYDUCTNAY_GLOVES) ) ) range -= 2;
	    } else {
		miss(fltxt,mon);
	    }
	} else if (sx == u.ux && sy == u.uy && range >= 0) {
	    nomul(0, 0, FALSE);
	    if (u.usteed && will_hit_steed() && !mon_reflects(u.usteed, (char *)0)) {
		    mon = u.usteed;
		    goto buzzmonst;
	    } else
	    if ((zap_hit_player((int) u.uac, 0)) || (Conflict && (zap_hit_player((int) u.uac, 0)) ) || (Race_if(PM_SPARD) && (zap_hit_player((int) u.uac, 0)) ) || (StrongConflict && (zap_hit_player((int) u.uac, 0)) ) ) {

		if (!(uarmg && itemhasappearance(uarmg, APP_RAYDUCTNAY_GLOVES))) range -= 2;
		pline("%s hits you!", The(fltxt));
		if (Reflecting && ((abstype == ZT_DEATH && rn2(StrongReflecting ? 100 : 20)) || (abstype != ZT_DEATH && rn2(StrongReflecting ? 20 : 5)) ) && abs(type) != ZT_SPELL(ZT_FIRE)) {
		    if (!Blind) {
		    	(void) ureflects("But %s reflects from your %s!", "it");
		    } else
			pline("For some reason you are not affected.");

	/* special reflection types adapted from FHS. It would be too much of a pain to code correctly,
	 * so I just decide that special reflection amulets "overwrite" standard reflection. --Amy */

			if (uamul && uamul->otyp == AMULET_OF_PRISM) {

			    if (dx && dy) {

				if (rn2(2)) {
					dx = -dx;
				} else {
					dy = -dy;
				}

			    } else if (dx) {
				dx = 0;
				dy = rn2(2) ? -1 : 1;
			    } else {
				dx = rn2(2) ? -1 : 1;
				dy = 0;
			    }

			} else if (uamul && uamul->oartifact == ART_GUARDIAN_ANGLE) {

			    if (dx && dy) {

				if (rn2(2)) {
					dx = -dx;
				} else {
					dy = -dy;
				}

			    } else if (dx) {
				dx = 0;
				dy = rn2(2) ? -1 : 1;
			    } else {
				dx = rn2(2) ? -1 : 1;
				dy = 0;
			    }

			} else if (uarmc && uarmc->oartifact == ART_ALLCOLOR_PRISM) {

			    if (dx && dy) {

				if (rn2(2)) {
					dx = -dx;
				} else {
					dy = -dy;
				}

			    } else if (dx) {
				dx = 0;
				dy = rn2(2) ? -1 : 1;
			    } else {
				dx = rn2(2) ? -1 : 1;
				dy = 0;
			    }

			} else if (uarm && uarm->oartifact == ART_TERRY_PRATCHETT_S_INGENUIT) {

			    if (dx && dy) {

				if (rn2(2)) {
					dx = -dx;
				} else {
					dy = -dy;
				}

			    } else if (dx) {
				dx = 0;
				dy = rn2(2) ? -1 : 1;
			    } else {
				dx = rn2(2) ? -1 : 1;
				dy = 0;
			    }

			} else if (uamul && uamul->oartifact == ART_TYRANITAR_S_OWN_GAME) {

			    if (dx && dy) {

				if (rn2(2)) {
					dx = -dx;
				} else {
					dy = -dy;
				}

			    } else if (dx) {
				dx = 0;
				dy = rn2(2) ? -1 : 1;
			    } else {
				dx = rn2(2) ? -1 : 1;
				dy = 0;
			    }

			} else if (uarmf && uarmf->oartifact == ART_CINDERELLA_S_SLIPPERS) {

			    if (dx && dy) {

				if (rn2(2)) {
					dx = -dx;
				} else {
					dy = -dy;
				}

			    } else if (dx) {
				dx = 0;
				dy = rn2(2) ? -1 : 1;
			    } else {
				dx = rn2(2) ? -1 : 1;
				dy = 0;
			    }

			} else if (uamul && uamul->otyp == AMULET_OF_WARP_DIMENSION) {

			    dx = rn1(3, -1);	/*-1, 0, 1*/
			    dy = rn1(3, -1);	/*-1, 0, 1*/

			} else if (uamul && uamul->oartifact == ART_PRECIOUS_UNOBTAINABLE_PROP) {

			    dx = rn1(3, -1);	/*-1, 0, 1*/
			    dy = rn1(3, -1);	/*-1, 0, 1*/

			} else if (uamul && uamul->oartifact == ART_ONE_MOMENT_IN_TIME) {

			    dx = rn1(3, -1);	/*-1, 0, 1*/
			    dy = rn1(3, -1);	/*-1, 0, 1*/

			} else {

			    dx = -dx;
			    dy = -dy;

			}

		    shieldeff(sx, sy);
		    /* WAC clear the beam so you can see it bounce back ;B */
		    if (!is_mega_spell(type)) {
			tmp_at(DISP_END,0);
			tmp_at(DISP_BEAM, zapdir_to_glyph(dx, dy, abstype));
		    }
		} else {
		    if (abs(type) != ZT_SPELL(ZT_FIRE))
		    zhitu(type, nd, fltxt, sx, sy);
		    else
			range = 0;
		}
	    } else {
		pline("%s whizzes by you!", The(fltxt));
	    }
	    if (abstype == ZT_LIGHTNING && !resists_blnd(&youmonst) && (issoviet || !rn2(3)) ) {
		You(are_blinded_by_the_flash);
		if (!issoviet) make_blinded((long)d(nd,2),FALSE);
		else make_blinded((long)d(nd,5),FALSE);
		if (!Blind) Your("%s", vision_clears);
	    }
	    stop_occupation();
	    nomul(0, 0, FALSE);
	}

	if(!ZAP_POS(lev->typ) || (closed_door(sx, sy) && (range >= 0))) {
	    int bounce;
	    uchar rmn;

 make_bounce:
        /* WAC Player/Monster Fireball */
            if (abs(type) == ZT_SPELL(ZT_FIRE)) {
		sx = lsx;
		sy = lsy;
		break; /* fireballs explode before the wall */
	    }
	    bounce = 0;
/*            range--;*/   
	    if(range && isok(lsx, lsy) && cansee(lsx,lsy))
		pline("%s bounces!", The(fltxt));
	    if(!dx || !dy || !rn2(20)) {

		if (tech_inuse(T_OVER_RAY) && !rn2(2) && (!dx || !dy)) range--;
		if (uarmg && uarmg->oartifact == ART_LONGEST_RAY && !rn2(2) && (!dx || !dy)) range--;

		dx = -dx;
		dy = -dy;
		/* WAC clear the beam so you can see it bounce back ;B */
		if (!is_mega_spell(type)) {
		    tmp_at(DISP_END,0);
		    tmp_at(DISP_BEAM, zapdir_to_glyph(dx, dy, abstype));
                }
                delay_output();
	    } else {

		if (tech_inuse(T_OVER_RAY) && !rn2(2)) {
			range++;
			if (!rn2(10)) range++;
			if (!rn2(50)) range += 5;
			if (!rn2(500)) range += 25;
		}
		if (uarmg && uarmg->oartifact == ART_LONGEST_RAY && !rn2(2)) {
			range++;
			if (!rn2(10)) range++;
			if (!rn2(50)) range += 5;
			if (!rn2(500)) range += 25;
		}

		if(isok(sx,lsy) && ZAP_POS(rmn = levl[sx][lsy].typ) &&
		   !closed_door(sx,lsy) &&
		   (IS_ROOM(rmn) || (isok(sx+dx,lsy) &&
				     ZAP_POS(levl[sx+dx][lsy].typ))))
		    bounce = 1;
		if(isok(lsx,sy) && ZAP_POS(rmn = levl[lsx][sy].typ) &&
		   !closed_door(lsx,sy) &&
		   (IS_ROOM(rmn) || (isok(lsx,sy+dy) &&
				     ZAP_POS(levl[lsx][sy+dy].typ))))
		    if(!bounce || rn2(2))
			bounce = 2;

		switch(bounce) {
                case 0: dx = -dx;
                        dy = -dy;
			/* WAC clear the beam so you can see it bounce back ;B */
			if (!is_mega_spell(type)) {
			    tmp_at(DISP_END,0);
			    tmp_at(DISP_BEAM, zapdir_to_glyph(dx, dy, abstype));
			}
                        delay_output();
			break;
                case 1: dy = -dy;
                        sx = lsx; break;
		case 2: dx = -dx;
                        sy = lsy; break;
		}
                if (!is_mega_spell(type))
		tmp_at(DISP_CHANGE, zapdir_to_glyph(dx,dy,abstype));
	    }
	}
    }

    /* Let you enjoy the beam */
    display_nhwindow(WIN_MESSAGE, TRUE);    /* --More-- */

/*WAC clear the light source*/
#ifdef LIGHT_SRC_SPELL
    if (((abstype == ZT_FIRE) || (abstype == ZT_LIGHTNING))
        && (!(type >= ZT_MEGA(ZT_FIRST) && type <= ZT_MEGA(ZT_LAST))))
        {
        while (lits) {
                del_light_source(LS_TEMP, (void *) lits);
                lits--;
                }
        vision_full_recalc = 1;
        vision_recalc(0); /*clean up vision*/
        }
#endif

    if (!is_mega_spell(type))
    tmp_at(DISP_END,0);

    /*WAC Player/Monster fireball*/
    if (abs(type) == ZT_SPELL(ZT_FIRE))
	explode(sx, sy, type, d(/*12*/nd,6), 0, EXPL_FIERY);
    if (shopdamage)
	pay_for_damage(abstype == ZT_FIRE ?  "burn away" :
		       abstype == ZT_COLD ?  "shatter" :
		       abstype == ZT_DEATH ? "disintegrate" : "destroy", FALSE);
    bhitpos = save_bhitpos;
}
#endif /*OVLB*/
#ifdef OVL0

void
melt_ice(x, y)
xchar x, y;
{
	struct rm *lev = &levl[x][y];
	struct obj *otmp;

	if (lev->typ == DRAWBRIDGE_UP)
	    lev->drawbridgemask &= ~DB_ICE;	/* revert to DB_MOAT */
	else {	/* lev->typ == ICE */
#ifdef STUPID
	    if (lev->icedpool == ICED_POOL) lev->typ = POOL;
	    else lev->typ = MOAT;
#else
	    lev->typ = (lev->icedpool == ICED_POOL ? POOL : MOAT);
#endif
	    lev->icedpool = 0;
	}
	obj_ice_effects(x, y, FALSE);
	unearth_objs(x, y);
	if (Underwater) vision_recalc(1);
	newsym(x,y);
	if (cansee(x,y)) Norep("The ice crackles and melts.");
	if ((otmp = sobj_at(BOULDER, x, y)) != 0) {
	    if (cansee(x,y)) pline("%s settles...", An(xname(otmp)));
	    do {
		obj_extract_self(otmp);	/* boulder isn't being pushed */
		if (!boulder_hits_pool(otmp, x, y, FALSE))
		    impossible("melt_ice: no pool?");
		/* try again if there's another boulder and pool didn't fill */
	    } while (is_pool(x,y) && (otmp = sobj_at(BOULDER, x, y)) != 0);
	    newsym(x,y);
	}
	if (x == u.ux && y == u.uy)
		spoteffects(TRUE);	/* possibly drown, notice objects */
}

/* Burn floor scrolls, evaporate pools, etc...  in a single square.  Used
 * both for normal bolts of fire, cold, etc... and for fireballs.
 * Sets shopdamage to TRUE if a shop door is destroyed, and returns the
 * amount by which range is reduced (the latter is just ignored by fireballs)
 */
int
zap_over_floor(x, y, type, shopdamage)
xchar x, y;
int type;
boolean *shopdamage;
{
	struct monst *mon;
	int abstype = abs(type) % 10;
	struct rm *lev = &levl[x][y];
	int rangemod = 0;

	if(abstype == ZT_FIRE) {
	    struct trap *t = t_at(x, y);

	    if (t && t->ttyp == WEB) {
		/* a burning web is too flimsy to notice if you can't see it */
		if (cansee(x,y)) Norep("A web bursts into flames!");
		(void) delfloortrap(t);
		if (cansee(x,y)) newsym(x,y);
	    }

		if (is_ash(x,y)) {
		    const char *msgtxt = "You hear a flaming sound.";
		    rangemod -= 3;
		    lev->typ = LAVAPOOL;
		    if (cansee(x,y)) msgtxt = "The ash floor melts and turns into lava.";
		    if (cansee(x,y)) newsym(x,y);
		    Norep("%s", msgtxt);
	    } else if (is_farmland(x,y)) {
		    const char *msgtxt = "You hear a burning sound.";
		    rangemod -= 3;
		    lev->typ = rn2(10) ? CORR : ASH;
		    if (cansee(x,y)) msgtxt = "The farmland burns up!";
		    /* yeah I know, this doesn't check whether YOU burned the farmland... but allowing monsters with fire
			* to burn the farmland is also a chaotic act, so there! :P --Amy */
		    if (u.ualign.type == A_CHAOTIC) adjalign(1);
		    if (cansee(x,y)) newsym(x,y);
		    Norep("%s", msgtxt);
	    } else if (is_raincloud(x,y) && !rn2(5)) {
		    const char *msgtxt = "You hear a sizzling sound.";
		    rangemod -= 3;
		    lev->typ = CLOUD;
		    if (cansee(x,y)) msgtxt = "The rain cloud boils!";
		    if (cansee(x,y)) newsym(x,y);
		    Norep("%s", msgtxt);
	    } else if (is_grassland(x,y)) {
		    const char *msgtxt = "You hear a burning sound.";
		    rangemod -= 3;
		    lev->typ = rn2(25) ? CORR : ASH;
		    if (u.ualign.type == A_CHAOTIC && !rn2(5)) adjalign(1);
		    if (cansee(x,y)) msgtxt = "The grass burns up!";
		    if (cansee(x,y)) newsym(x,y);
		    Norep("%s", msgtxt);
	    } else if (is_snow(x,y)) {
		    rangemod -= 3;
		    lev->typ = CORR;
		    if (cansee(x,y)) Norep("The snow melts away.");
		    if (cansee(x,y)) newsym(x,y);
	    } else if (is_mattress(x,y) && !rn2(5)) {
		    rangemod -= 3;
		    lev->typ = CORR;
		    if (u.ualign.type == A_CHAOTIC) adjalign(1);
		    if (cansee(x,y)) Norep("The mattress burns up.");
		    if (cansee(x,y)) newsym(x,y);
	    } else if (is_table(x,y) && !rn2(5)) {
		    rangemod -= 3;
		    lev->typ = CORR;
		    if (u.ualign.type == A_CHAOTIC) adjalign(1);
		    if (cansee(x,y)) Norep("The table burns up.");
		    if (cansee(x,y)) newsym(x,y);
	    } else if (is_carvedbed(x,y) && !rn2(5)) {
		    rangemod -= 3;
		    lev->typ = CORR;
		    if (u.ualign.type == A_CHAOTIC) adjalign(1);
		    if (cansee(x,y)) Norep("The bed burns up.");
		    if (cansee(x,y)) newsym(x,y);
	    } else if (is_wagon(x,y)) {
		    const char *msgtxt = "You hear a flaming sound.";
		    rangemod -= 3;
		    lev->typ = BURNINGWAGON;
		    if (cansee(x,y)) msgtxt = "The wagon bursts into flames!";
		    if (cansee(x,y)) newsym(x,y);
		    Norep("%s", msgtxt);
	    } else if(is_ice(x, y)) {
		melt_ice(x, y);
	    } else if(is_pool(x,y)) {
		const char *msgtxt = "You hear hissing gas.";
		if(lev->typ != POOL) {	/* MOAT or DRAWBRIDGE_UP */
		    if (cansee(x,y)) msgtxt = "Some water evaporates.";
		} else {
		    register struct trap *ttmp;

		    rangemod -= 3;
		    lev->typ = ROOM;
		    ttmp = maketrap(x, y, PIT, 0);
		    if (ttmp) ttmp->tseen = 1;
		    if (cansee(x,y)) msgtxt = "The water evaporates.";
		}
		Norep("%s", msgtxt);
		if (lev->typ == ROOM) newsym(x,y);
	    } else if(IS_FOUNTAIN(lev->typ)) {
		    if (u.ualign.type == A_CHAOTIC) adjalign(1);
		    if (cansee(x,y))
			pline("Steam billows from the fountain.");
		    rangemod -= 1;
		    dryup(x, y, type > 0);
	    }
	}

	else if(abstype == ZT_ACID && IS_IRONBAR(levl[x][y].typ)) {
	    if (tech_inuse(T_MELTEE) && !rn2(5)) {
		    const char *msgtxt = "You hear a sizzling sound.";
		    lev->typ = CORR;
		    if (cansee(x,y)) msgtxt = "The iron bars are dissolved!";
		    if (cansee(x,y)) newsym(x,y);
		    Norep("%s", msgtxt);
	    }
	}
	else if(abstype == ZT_LITE && is_burningwagon(x,y)) {
		rangemod -= 3;
		lev->typ = ROOM;
		if (cansee(x,y)) pline("The light ray completely destroys the burning wagon.");
		if (cansee(x,y)) newsym(x,y);
		/* The effects of solar beams and psybeams on burning wagons make no sense at all,
		 * yet they are intentional (isn't it annoying how I always have to say this?)
		 * After all, burning wagons should be possible to remove somehow, but it should be hard to do --Amy */
	}
	else if(abstype == ZT_SPC2 && is_burningwagon(x,y)) {
		rangemod -= 3;
		lev->typ = RAINCLOUD;
		if (cansee(x,y)) pline("As the psybeam hits the burning wagon, it mysteriously transforms into a rain cloud!");
		if (cansee(x,y)) newsym(x,y);

	}
	else if(abstype == ZT_COLD && (is_pool(x,y) || is_crystalwater(x,y) || is_watertunnel(x,y) || is_lava(x,y))) {
		boolean lava = is_lava(x,y);
		boolean moat = (!lava && (lev->typ != POOL) &&
				(lev->typ != WATER) &&
				!Is_medusa_level(&u.uz) &&
				!Is_waterlevel(&u.uz));

		if (lev->typ == WATER) {
		    /* For now, don't let WATER freeze. */
		    if (cansee(x,y))
			pline_The("water freezes for a moment.");
		    else
			You_hear("a soft crackling.");
		    rangemod -= 1000;	/* stop */
		} else if (lev->typ == CRYSTALWATER) {
		    rangemod -= 3;
		    if (cansee(x,y))
			pline_The("crystal water solidifies.");
		    else
			You_hear("icy crackling sounds.");
		    lev->typ = STALACTITE;
		} else if (lev->typ == WATERTUNNEL) {
		    rangemod -= 3;
		    if (cansee(x,y))
			pline_The("water solidifies.");
		    lev->typ = ROCKWALL;

		} else {
		    rangemod -= 3;
		    if (lev->typ == DRAWBRIDGE_UP) {
			lev->drawbridgemask &= ~DB_UNDER;  /* clear lava */
			lev->drawbridgemask |= (lava ? DB_FLOOR : DB_ICE);
		    } else {
			if (!lava)
			    lev->icedpool =
				    (lev->typ == POOL ? ICED_POOL : ICED_MOAT);
			lev->typ = (lava ? ASH : ICE);
		    }
		    bury_objs(x,y);
		    if(cansee(x,y)) {
			if(moat)
				Norep("The moat is bridged with ice!");
			else if(lava)
				Norep("The lava cools and solidifies.");
			else
				Norep("The water freezes.");
			newsym(x,y);
		    } else if(flags.soundok && !lava)
			You_hear("a crackling sound.");

		    if (x == u.ux && y == u.uy) {
			if (u.uinwater) {   /* not just `if (Underwater)' */
			    /* leave the no longer existent water */
			    u.uinwater = 0;
			    u.uundetected = 0;
			    docrt();
			    vision_full_recalc = 1;
			} else if (u.utrap && u.utraptype == TT_LAVA) {
			    if (Passes_walls) {
				You("pass through the now-solid rock.");
			    } else {
				u.utrap = rn1(50,20);
				u.utraptype = TT_INFLOOR;
				You("are firmly stuck in the cooling rock.");
			    }
			}
		    } else if ((mon = m_at(x,y)) != 0) {
			/* probably ought to do some hefty damage to any
			   non-ice creature caught in freezing water;
			   at a minimum, eels are forced out of hiding */
			if (is_swimmer(mon->data) && mon->mundetected) {
			    mon->mundetected = 0;
			    newsym(x,y);
			}
		    }
		}
		obj_ice_effects(x,y,TRUE);
	}
	if(closed_door(x, y)) {
		int new_doormask = -1;
		const char *see_txt = 0, *sense_txt = 0, *hear_txt = 0;
		rangemod = -1000;
		/* ALI - Artifact doors */
		if (artifact_door(x, y))
		    goto def_case;
		switch(abstype) {
		case ZT_FIRE:
		    new_doormask = D_NODOOR;
		    see_txt = "The door is consumed in flames!";
		    sense_txt = "smell smoke.";
		    break;
		case ZT_COLD:
		    new_doormask = D_NODOOR;
		    see_txt = "The door freezes and shatters!";
		    sense_txt = "feel cold.";
		    break;
		case ZT_DEATH:
		    /* death spells/wands don't disintegrate */
		    if(abs(type) != ZT_BREATH(ZT_DEATH))
			goto def_case;
		    new_doormask = D_NODOOR;
		    see_txt = "The door disintegrates!";
		    hear_txt = "crashing wood.";
		    break;
		case ZT_LIGHTNING:
		    new_doormask = D_BROKEN;
		    see_txt = "The door splinters!";
		    hear_txt = "crackling.";
		    break;
                case ZT_ACID:
                    new_doormask = D_NODOOR;
                    see_txt = "The door melts!";
                    hear_txt = "sizzling.";
		    break;
		default:
		def_case:
		    if(cansee(x,y)) {
			pline_The("door absorbs %s %s!",
			      (type < 0) ? "the" : "your",
			      abs(type) < ZT_SPELL(0) ? "bolt" :
			      abs(type) < ZT_BREATH(0) ? "spell" :
			      "blast");
		    } else You_feel("vibrations.");
		    break;
		}
		if (new_doormask >= 0) {	/* door gets broken */
		    if (*in_rooms(x, y, SHOPBASE)) {
			if (type >= 0) {
			    add_damage(x, y, 400L);
			    *shopdamage = TRUE;
			} else	/* caused by monster */
			    add_damage(x, y, 0L);
		    }
		    lev->doormask = new_doormask;
		    unblock_point(x, y);	/* vision */
		    if (cansee(x, y)) {
			pline("%s", see_txt);
			newsym(x, y);
		    } else if (sense_txt) {
			You("%s", sense_txt);
		    } else if (hear_txt) {
			if (flags.soundok) You_hear("%s", hear_txt);
		    }
		    if (picking_at(x, y)) {
			stop_occupation();
			reset_pick();
		    }
		}
	}

	if(OBJ_AT(x, y) && abstype == ZT_FIRE)
		if (burn_floor_paper(x, y, FALSE, type > 0) && couldsee(x, y)) {
		    newsym(x,y);
		    You("%s of smoke.",
			!Blind ? "see a puff" : "smell a whiff");
		}
	if ((mon = m_at(x,y)) != 0) {
		/* Cannot use wakeup() which also angers the monster */
		mon->msleeping = 0;
		if(mon->m_ap_type) seemimic(mon);
		if(type >= 0) {
		    setmangry(mon);
		    if(mon->ispriest && *in_rooms(mon->mx, mon->my, TEMPLE))
			ghod_hitsu(mon);

		    if(mon->isshk && !*u.ushops)
			hot_pursuit(mon);
		}
	}
	return rangemod;
}

#endif /*OVL0*/
#ifdef OVL3

void
fracture_rock(obj)	/* fractured by pick-axe or wand of striking */
register struct obj *obj;		   /* no texts here! */
{
	/* A little Sokoban guilt... */
	if (obj->otyp == BOULDER && In_sokoban(&u.uz) && !flags.mon_moving)
		{change_luck(-1);
		pline("You cheater!");
		if (evilfriday) u.ugangr++;
		}

	obj->otyp = ROCK;
	obj->quan = (long) rn1(60, 7);
	obj->owt = weight(obj);
	obj->oclass = GEM_CLASS;
	obj->known = FALSE;
	obj->onamelth = 0;		/* no names */
	obj->oxlth = 0;			/* no extra data */
	obj->oattached = OATTACHED_NOTHING;

	if (is_hazy(obj)) { /* fix the Banach-Tarski paradox, thanks blargdag --Amy */
		stop_timer(UNPOLY_OBJ, (void *) obj);
		obj->oldtyp = STRANGE_OBJECT;
	}

	if(!rn2(8)) {
		obj->spe = rne(2);
		if (rn2(2)) obj->blessed = rn2(2);
		 else	blessorcurse(obj, 3);
	} else if(!rn2(10)) {
		if (rn2(10)) curse(obj);
		 else	blessorcurse(obj, 3);
		obj->spe = -rne(2);
	} else	blessorcurse(obj, 10);

	if (obj->where == OBJ_FLOOR) {
		obj_extract_self(obj);		/* move rocks back on top */
		place_object(obj, obj->ox, obj->oy);
		if(!does_block(obj->ox,obj->oy,&levl[obj->ox][obj->oy]))
	    		unblock_point(obj->ox,obj->oy);
		if(cansee(obj->ox,obj->oy))
		    newsym(obj->ox,obj->oy);
	}
}

/* handle statue hit by striking/force bolt/pick-axe */
boolean
break_statue(obj)
register struct obj *obj;
{
	/* [obj is assumed to be on floor, so no get_obj_location() needed] */
	struct trap *trap = t_at(obj->ox, obj->oy);
	struct obj *item;

	if (trap && (trap->ttyp == STATUE_TRAP || trap->ttyp == SATATUE_TRAP) &&
		activate_statue_trap(trap, obj->ox, obj->oy, TRUE))
	    return FALSE;
	/* drop any objects contained inside the statue */
	while ((item = obj->cobj) != 0) {
	    obj_extract_self(item);
	    place_object(item, obj->ox, obj->oy);
	}
	if (Role_if(PM_ARCHEOLOGIST) && !flags.mon_moving && (obj->spe & STATUE_HISTORIC)) {
	    You_feel("guilty about damaging such a historic statue.");
	    adjalign(-10);
	}

	if (Role_if(PM_ARTIST) && !flags.mon_moving) { /* Breaking statues really hurts an Artist's alignment */
	    You_feel("guilty about damaging such a beautiful statue.");
	    adjalign(-10);
	}

	obj->spe = 0;
	fracture_rock(obj);
	return TRUE;
}

const char * const destroy_strings[] = {	/* also used in trap.c */
	"freezes and shatters", "freeze and shatter", "shattered potion",
	"boils and explodes", "boil and explode", "boiling potion",
	"catches fire and burns", "catch fire and burn", "burning scroll",
	"catches fire and burns", "catch fire and burn", "burning book",
	"turns to dust and vanishes", "turn to dust and vanish", "vaporized ring",
	"breaks apart and explodes", "break apart and explode", "exploding wand",
	"disintegrates", "disintegrate", "disintegrated amulet",
	"is poisoned and turns useless", "are poisoned and turn useless", "poisoned potion",
	"is poisoned and dissolves", "are poisoned and dissolve", "poisoned food",
};

void
destroy_item(osym, dmgtyp)
register int osym, dmgtyp;
{
	register struct obj *obj;
	register int dmg, xresist, skip;
	register long i, cnt, quan;
	register int dindx;
	const char *mult;
	/*
	 * [ALI] Because destroy_item() can call wand_explode() which can
	 * call explode() which can call destroy_item() again, we need to
	 * be able to deal with the possibility of not only the current
	 * object we are looking at no longer being in the inventory, but
	 * also the next object (and the one after that, etc.).
	 * We do this by taking advantage of the fact that objects in the
	 * inventory destroyed as a result of calling wand_explode() will
	 * always be destroyed by this same function. This allows us to
	 * maintain a list of "next" pointers and to adjust these pointers
	 * as required when we destroy an object that they might be pointing
	 * at.
	 */
	struct destroy_item_frame {
	    struct obj *next_obj;
	    struct destroy_item_frame *next_frame;
	} frame;
	static struct destroy_item_frame *destroy_item_stack;

	/* this is to deal with gas spores -- they don't really
	   destroy objects, but this routine is called a lot in
	   explode.c for them... hmph... */
	if (dmgtyp == AD_PHYS) return;

	frame.next_frame = destroy_item_stack;
	destroy_item_stack = &frame;

	for(obj = invent; obj; obj = frame.next_obj) {
	    frame.next_obj = obj->nobj;
	    if(obj->oclass != osym) continue; /* test only objs of type osym */
	    if(obj->oartifact) continue; /* don't destroy artifacts */
	    if(obj->in_use && obj->quan == 1) continue; /* not available */
	    if (obj->oerodeproof) continue; /* this item is immune --Amy */
	    xresist = skip = 0;
#ifdef GCC_WARN
	    dmg = dindx = 0;
	    quan = 0L;
#endif
	    switch(dmgtyp) {
		case AD_COLD:

		    if (uarmf && itemhasappearance(uarmf, APP_FLEECY_BOOTS) ) {
				skip++;
				break;
			}

		    if (uwep && uwep->oartifact == ART_GLACIERDALE ) {
				skip++;
				break;
			}

		    if (powerfulimplants() && uimplant && uimplant->oartifact == ART_WHITE_WHALE_HATH_COME) {
				skip++;
				break;
			}

		    if (powerfulimplants() && uimplant && uimplant->oartifact == ART_DUBAI_TOWER_BREAK) {
				skip++;
				break;
			}

		    if (uarmf && uarmf->oartifact == ART_VERA_S_FREEZER) {
				skip++;
				break;
			}

		    if (uarmf && uarmf->oartifact == ART_CORINA_S_SNOWY_TREAD) {
				skip++;
				break;
			}

		    if (uarmf && uarmf->oartifact == ART_KATIE_MELUA_S_FLEECINESS) {
				skip++;
				break;
			}

		    if(osym == POTION_CLASS && obj->otyp != POT_OIL && obj->otyp != POT_ICE) {
			quan = obj->quan;
			dindx = 0;
			dmg = rnd(4);
			if (!skip) u.cnd_colddestroy += quan;
		    } else skip++;
		    break;
		case AD_VENO:

		    if(osym == POTION_CLASS) {

			if (obj->otyp == POT_SICKNESS || obj->otyp == POT_POISON || obj->otyp == POT_CYANIDE) {
				skip++; /* idea by Andrio */
				break;
			}

			quan = obj->quan;
			dindx = 7;
			dmg = 0;
			if (!skip) u.cnd_poisondestroy += quan;
		    } else if(osym == FOOD_CLASS) {
			quan = obj->quan;
			dindx = 8;
			dmg = 0;
			if (!skip) u.cnd_poisondestroy += quan;
		    } else skip++;
		    break;
		case AD_FIRE:
		    xresist = (Fire_resistance && obj->oclass != POTION_CLASS);

			if (uarmc && uarmc->oartifact == ART_BOWSER_S_FUN_ARENA) {
				skip++;
				break;
			}
			if (Race_if(PM_HYPOTHERMIC)) {
				skip++;
				break;
			}

			if (osym==SCROLL_CLASS && obj->oartifact)
			skip++;
		    if (obj->otyp == SCR_FIRE || obj->otyp == POT_FIRE || obj->otyp == SPE_FIREBALL || obj->otyp == SPE_INFERNO || obj->otyp == SPE_FIRE_BOLT || obj->otyp == SPE_FIRE_GOLEM)
			skip++;
		    if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
			skip++;
			if (!Blind)
			    pline("%s glows a strange %s, but remains intact.",
				The(xname(obj)), hcolor("dark red"));
		    }
		    quan = obj->quan;
		    switch(osym) {
			case POTION_CLASS:
			    dindx = 1;
			    dmg = rnd(6);
			    if (!skip) u.cnd_firedestroy += quan;
			    break;
			case SCROLL_CLASS:
			    dindx = 2;
			    dmg = 1;
			    if (!skip) u.cnd_firedestroy += quan;
			    break;
			case SPBOOK_CLASS:
			    dindx = 3;
			    dmg = 1;
			    if (!skip) u.cnd_firedestroy += quan;
			    break;
			default:
			    skip++;
			    break;
		    }
		    break;
		case AD_ELEC:
		    xresist = (Shock_resistance && obj->oclass != RING_CLASS);

			if (Race_if(PM_PLAYER_DYNAMO)) {
				skip++;
				break;
			}

		    quan = obj->quan;
		    switch(osym) {
			case RING_CLASS:
			    if(obj->otyp == RIN_SHOCK_RESISTANCE)
				    { skip++; break; }
			    dindx = 4;
			    dmg = 0;
			    if (!skip) u.cnd_shockdestroy += quan;
			    break;
			case WAND_CLASS:
			    if(obj->otyp == WAN_LIGHTNING) { skip++; break; }
#if 0
			    if (obj == current_wand) { skip++; break; }
#endif
			    dindx = 5;
			    dmg = rnd(10);
			    if (!skip) u.cnd_shockdestroy += quan;
			    break;
			case AMULET_CLASS:
			    dindx = 6;
			    dmg = 0;
			    if (!skip) u.cnd_shockdestroy += quan;
			    break;
			default:
			    skip++;
			    break;
		    }
		    break;
		default:
		    skip++;
		    break;
	    }
	    if(!skip) {
		if (obj->in_use) --quan; /* one will be used up elsewhere */
		for(i = cnt = 0L; i < quan; i++)
		    if(!rn2(10) && (rnd(20 + Luck) < 15) && (rnd(20 + Luck) < 15) ) cnt++;

		if(!cnt) continue;
		if(cnt == quan)	mult = "Your";
		else	mult = (cnt == 1L) ? "One of your" : "Some of your";
		pline("%s %s %s!", mult, xname(obj),
			(cnt > 1L) ? destroy_strings[dindx*3 + 1]
				  : destroy_strings[dindx*3]);
		if(osym == POTION_CLASS && dmgtyp != AD_COLD && dmgtyp != AD_VENO) {
		    if (!breathless(youmonst.data) || haseyes(youmonst.data))
		    	potionbreathe(obj);
		}
		if (obj->owornmask) {
		    if (obj->owornmask & W_RING) /* ring being worn */
			Ring_gone(obj);
		    else
			setnotworn(obj);
		}
		if (obj == current_wand) current_wand = 0;	/* destroyed */
		if (cnt == obj->quan && frame.next_frame) {
		    /* Before removing an object from the inventory, adjust
		     * any "next" pointers that would otherwise become invalid.
		     */
		    struct destroy_item_frame *fp;
		    for(fp = frame.next_frame; fp; fp = fp->next_frame) {
			if (fp->next_obj == obj)
			    fp->next_obj = frame.next_obj;
		    }
		}
		if(osym == WAND_CLASS && dmgtyp == AD_ELEC) {
		    /* MAR use a continue since damage and stuff is taken care of
		     *  in wand_explode */
                    wand_explode(obj, FALSE);
                    continue;
		}
		/* Use up the object */
		for (i = 0; i < cnt; i++)
		    useup(obj);
		/* Do the damage if not resisted */
		if(dmg) {
		    if(xresist)	You("aren't hurt!");
		    else {
			const char *how = destroy_strings[dindx * 3 + 2];
			boolean one = (cnt == 1L);

			losehp(dmg, one ? how : (const char *)makeplural(how),
			       one ? KILLED_BY_AN : KILLED_BY);
			exercise(A_STR, FALSE);
		    }
		}
	    }
	}
	destroy_item_stack = frame.next_frame;
	return;
}

int
destroy_mitem(mtmp, osym, dmgtyp)
struct monst *mtmp;
int osym, dmgtyp;
{
	struct obj *obj, *obj2;
	int skip, tmp = 0;
	long i, cnt, quan;
	int dindx;
	boolean vis;

	if (mtmp == &youmonst) {	/* this simplifies artifact_hit() */
	    destroy_item(osym, dmgtyp);
	    return 0;	/* arbitrary; value doesn't matter to artifact_hit() */
	}

	vis = canseemon(mtmp);
	for(obj = mtmp->minvent; obj; obj = obj2) {
	    obj2 = obj->nobj;
	    if(obj->oclass != osym) continue; /* test only objs of type osym */
	    skip = 0;
	    quan = 0L;
	    dindx = 0;

	    switch(dmgtyp) {
		case AD_COLD:
		    if(osym == POTION_CLASS && obj->otyp != POT_OIL) {
			quan = obj->quan;
			dindx = 0;
			tmp++;
		    } else skip++;
		    break;
		case AD_FIRE:
			if (osym==SCROLL_CLASS && obj->oartifact)
				skip++;
		    if (obj->otyp == SCR_FIRE || obj->otyp == SPE_FIREBALL)
			skip++;
		    if (obj->otyp == SPE_BOOK_OF_THE_DEAD) {
			skip++;
			if (vis)
			    pline("%s glows a strange %s, but remains intact.",
				The(distant_name(obj, xname)),
				hcolor("dark red"));
		    }
		    quan = obj->quan;
		    switch(osym) {
			case POTION_CLASS:
			    dindx = 1;
			    tmp++;
			    break;
			case SCROLL_CLASS:
			    dindx = 2;
			    tmp++;
			    break;
			case SPBOOK_CLASS:
			    dindx = 3;
			    tmp++;
			    break;
			default:
			    skip++;
			    break;
		    }
		    break;
		case AD_ELEC:
		    quan = obj->quan;
		    switch(osym) {
			case RING_CLASS:
			    if(obj->otyp == RIN_SHOCK_RESISTANCE)
				    { skip++; break; }
			    dindx = 4;
			    break;
			case WAND_CLASS:
			    if(obj->otyp == WAN_LIGHTNING) { skip++; break; }
			    dindx = 5;
			    tmp++;
			    break;
			default:
			    skip++;
			    break;
		    }
		    break;
		default:
		    skip++;
		    break;
	    }
	    if(!skip) {
		for(i = cnt = 0L; i < quan; i++)
		    if(!rn2(10) && (rnd(20 + Luck) < 15) && (rnd(20 + Luck) < 15) ) cnt++;

		if(!cnt) continue;
		if (vis) pline("%s %s %s!",
			s_suffix(Monnam(mtmp)), xname(obj),
			(cnt > 1L) ? destroy_strings[dindx*3 + 1]
				  : destroy_strings[dindx*3]);
		for(i = 0; i < cnt; i++) m_useup(mtmp, obj);
	    }
	}
	return(tmp);
}

#endif /*OVL3*/
#ifdef OVL2

int
resist(mtmp, oclass, damage, tell)
struct monst *mtmp;
char oclass;
int damage, tell;
{
	int resisted;
	int alev, dlev;

	/* attack level */
	switch (oclass) {
	    case WAND_CLASS:	alev = 12 + (Role_if(PM_WANDKEEPER) ? GushLevel : Role_if(PM_EMPATH) ? (GushLevel * 3 / 2) : (GushLevel / 2));	 break;
	    case TOOL_CLASS:	alev = 10 + ((Role_if(PM_GRADUATE) || Role_if(PM_CRACKER)) ? (GushLevel * 2) : (Role_if(PM_GEEK) || Role_if(PM_SOFTWARE_ENGINEER) || Role_if(PM_ASTRONAUT) || Role_if(PM_ARCHEOLOGIST)) ? GushLevel : (GushLevel / 2));	 break;	/* instrument */
	    case WEAPON_CLASS:	alev = 10 + (Role_if(PM_GOFF) ? GushLevel : (Role_if(PM_ARTIST) || Role_if(PM_QUARTERBACK) || Role_if(PM_SPACE_MARINE)) ? (GushLevel / 2) : (GushLevel / 3));	 break;	/* artifact */
	    case SCROLL_CLASS:	alev =  9 + (Role_if(PM_INTEL_SCRIBE) ? GushLevel : Role_if(PM_LIBRARIAN) ? (GushLevel / 2) : (GushLevel / 3));	 break;
	    case POTION_CLASS:	alev =  6 + (Role_if(PM_DRUNK) ? GushLevel : (Role_if(PM_SCIENTIST) || Race_if(PM_ALCHEMIST)) ? (GushLevel / 2) : (GushLevel / 4));	 break;
	    case RING_CLASS:	alev =  5 + (Role_if(PM_LADIESMAN) ? (GushLevel / 2) : (Role_if(PM_DOLL_MISTRESS) || Role_if(PM_DISSIDENT)) ? (GushLevel / 3) : (GushLevel / 5));	 break;
	    default:		alev = rnd(Role_if(PM_MASTERMIND) ? (GushLevel * 3 / 2) : GushLevel); break;	/* spell */
	}

	if (u.uprops[LOW_EFFECTS].extrinsic || LowEffects || (uamul && uamul->oartifact == ART_MYSTERIOUS_MAGIC) || have_loweffectstone() ) alev = 1;

	/* defense level */
	dlev = (int)mtmp->m_lev;
	if (dlev > 50) dlev = 50;
	else if (dlev < 1) dlev = is_mplayer(mtmp->data) ? u.ulevel : 1;

	resisted = rn2(100 + alev - dlev) < mtmp->data->mr;
	if (resisted) {
	    if (tell) {
		shieldeff(mtmp->mx, mtmp->my);
		pline("%s resists!", Monnam(mtmp));
	    }
	    damage = (damage + 1) / 2;
	}

	if (damage) {
	    mtmp->mhp -= damage;
	    if (mtmp->mhp < 1) {
		if(m_using) monkilled(mtmp, "", AD_RBRE);
		else killed(mtmp);
	    }
	    else wounds_message(mtmp);
	}
	return(resisted);
}

void
makewish(magicalwish)
boolean magicalwish; /* for the evil variant, because of Unnethack's nonmagical pseudo"wishes" --Amy */
{
	char buf[BUFSZ];
#ifdef LIVELOGFILE
	char rawbuf[BUFSZ]; /* for exact livelog reporting */
#endif
	struct obj *otmp, nothing;
	int tries = 0;
	boolean magical_object;

	nothing = zeroobj;  /* lint suppression; only its address matters */
	if (flags.verbose) { (Role_if(PM_PIRATE) || Role_if(PM_KORSAIR) || (uwep && uwep->oartifact == ART_ARRRRRR_MATEY) ) ? pline("Shiver me timbers! Ye may wish for an object!") : You("may wish for an object."); }

	if (!magicalwish) pline("This is the evil variant though, you can only wish for nonmagical crap.");
	/* In Unnethack you'd even be disallowed to wish for an object class that has potentially magical ones...
	 * But I decided that it's fair game, after all wishing for a random scroll may well give you a scroll of amnesia
	 * or other useless one. If you decide to gamble like that, oh well, I'm allowing it. --Amy */

retry:
	getlin("For what do you wish?", buf);
#ifdef LIVELOGFILE
	strcpy(rawbuf, buf);
#endif
	if(buf[0] == '\033') {

		if (!wizard) {
			if (yn("You escaped out of the prompt! Really throw the wish away and possibly get a random useless object?") == 'y')
				pline("Man, there would have so many good items you could have wished for...");
			else goto retry;
		}

		buf[0] = 0;
	}
	/*
	 *  Note: if they wished for and got a non-object successfully,
	 *  otmp == &zeroobj.  That includes gold, or an artifact that
	 *  has been denied.  Wishing for "nothing" requires a separate
	 *  value to remain distinct.
	 */
	otmp = readobjnam(buf, &nothing, TRUE, TRUE);
	if (!otmp) {
	    pline("Nothing fitting that description exists in the game.");
	    if (++tries < 5) goto retry;
	    pline("%s", thats_enough_tries);
	    /*otmp = readobjnam((char *)0, (struct obj *)0, TRUE, TRUE);
	    if (!otmp)*/ return;	/* for safety; should never happen */
	} else if (otmp == &nothing) {
	    /* explicitly wished for "nothing", presumeably attempting
	       to retain wishless conduct */
#ifdef LIVELOGFILE
	    livelog_wish(buf);
#endif
	    return;
	}

	/* check if wishing for magical objects is allowed; code stolen from unnethack */
	magical_object = (otmp && (otmp->oartifact || objects[otmp->otyp].oc_magic));
	if (!magicalwish && magical_object) {
		verbalize("Har har har, this object is magical and therefore you cannot wish for it, bitch.");
		if (otmp->oartifact) artifact_exists(otmp, ONAME(otmp), FALSE);
		obfree(otmp, (struct obj *) 0);
		otmp = &zeroobj;
		goto retry;
	}

	/* KMH, conduct */
	/* Amy edit: nonmagical pseudo"wishes" aren't really wishes and therefore don't break the conduct. */
	if (magicalwish) u.uconduct.wishes++;

	/* Livelog patch */
#ifdef LIVELOGFILE
	livelog_wish(rawbuf);
#endif

	if (otmp != &zeroobj) {
	    /* The(aobjnam()) is safe since otmp is unidentified -dlc */
	    (void) hold_another_object(otmp, u.uswallow ?
				       "Oops!  %s out of your reach!" :
				       (Is_airlevel(&u.uz) ||
					Is_waterlevel(&u.uz) ||
					levl[u.ux][u.uy].typ < IRONBARS ||
					levl[u.ux][u.uy].typ >= ICE) ?
				       "Oops!  %s away from you!" :
				       "Oops!  %s to the floor!",
				       The(aobjnam(otmp,
					     Is_airlevel(&u.uz) || u.uinwater ?
						   "slip" : "drop")),
				       (const char *)0);
	    u.ublesscnt += rn1(100,50);  /* the gods take notice */

		/* evil patch idea by jonadab: wishing can set the player's luck to -7 */
	    if (!rn2(500) && (u.uluck > -7) ) {
			u.uluck = -7;
			if (rn2(3)) pline("You seem to have used up all your good luck for that wish...");
	    }
		/* another evil patch idea by jonadab: getting a wish causes amnesia as a side effect. */
	    if (!rn2(100)) {
		pline("Whoops... you wish you hadn't forgotten to think about Maud from all the thinking about what to wish for.");		
		if (FunnyHallu) pline("You also wish you were able to remember that you suffer from amnesia.");
		forget(1 + rn2(5));
	    }
	}
}

/* Evil Patch idea by Amy: If your wish fails, e.g. because your luck is too low, you may get an evil artifact instead,
 * possibly after the game gave you an apparently working wish prompt, except that it doesn't matter at all what you enter.
 * It can, however, still happen that you simply get the "nothing happens" message instead. */
void
makenonworkingwish()
{
	char buf[BUFSZ];

	/* No matter which result we pick, this does not break wishless conduct --Amy */

	if (!rn2(3)) {
		pline("Unfortunately, nothing happens.");
	}
	else if (rn2(2)) {
		if (flags.verbose) { (Role_if(PM_PIRATE) || Role_if(PM_KORSAIR) || (uwep && uwep->oartifact == ART_ARRRRRR_MATEY) ) ? pline("Shiver me timbers! Ye may wish for an object!") : You("may wish for an object."); }
retry:
		getlin("For what do you wish?", buf);
		bad_artifact();
	}
	else {
		bad_artifact();
	}
}

/* Some wish sources are less likely to give a wish; make them do other nice things if they don't --Amy */
void
othergreateffect()
{
	register struct obj *acqo;

	int typeofeffect = rnd(3);

	/* case 1: great item from a predetermined list - can also be an artifact if you're lucky */

	if (typeofeffect == 1) {

		acqo = mksobj(makegreatitem(), TRUE, TRUE);
		if (acqo) {
			dropy(acqo);
			pline("A high-quality item appeared on the ground!");
		}

	}

	/* case 2: make a random artifact */

	if (typeofeffect == 2) {

		boolean havegifts = u.ugifts;

		if (!havegifts) u.ugifts++;

		acqo = mk_artifact((struct obj *)0, !rn2(3) ? A_CHAOTIC : rn2(2) ? A_NEUTRAL : A_LAWFUL, TRUE);
		if (acqo) {
		    dropy(acqo);
			if (P_MAX_SKILL(get_obj_skill(acqo, TRUE)) == P_ISRESTRICTED) {
			    unrestrict_weapon_skill(get_obj_skill(acqo, TRUE));
			} else if (P_MAX_SKILL(get_obj_skill(acqo, TRUE)) == P_UNSKILLED) {
				unrestrict_weapon_skill(get_obj_skill(acqo, TRUE));
				P_MAX_SKILL(get_obj_skill(acqo, TRUE)) = P_BASIC;
			} else if (rn2(2) && P_MAX_SKILL(get_obj_skill(acqo, TRUE)) == P_BASIC) {
				P_MAX_SKILL(get_obj_skill(acqo, TRUE)) = P_SKILLED;
			} else if (!rn2(4) && P_MAX_SKILL(get_obj_skill(acqo, TRUE)) == P_SKILLED) {
				P_MAX_SKILL(get_obj_skill(acqo, TRUE)) = P_EXPERT;
			} else if (!rn2(10) && P_MAX_SKILL(get_obj_skill(acqo, TRUE)) == P_EXPERT) {
				P_MAX_SKILL(get_obj_skill(acqo, TRUE)) = P_MASTER;
			} else if (!rn2(100) && P_MAX_SKILL(get_obj_skill(acqo, TRUE)) == P_MASTER) {
				P_MAX_SKILL(get_obj_skill(acqo, TRUE)) = P_GRAND_MASTER;
			} else if (!rn2(200) && P_MAX_SKILL(get_obj_skill(acqo, TRUE)) == P_GRAND_MASTER) {
				P_MAX_SKILL(get_obj_skill(acqo, TRUE)) = P_SUPREME_MASTER;
			}

		    discover_artifact(acqo->oartifact);

			if (!havegifts) u.ugifts--;
			pline("An artifact appeared beneath you!");

		}	

		else pline("Opportunity knocked, but nobody was home.  Bummer.");

	}

	/* case 3: acquirement */

	if (typeofeffect == 3) {

		int acquireditem;
		acquireditem = 0;
		pline("You may acquire an item!");

		pline("Pick an item type that you want to acquire. The prompt will loop until you actually make a choice.");

		while (acquireditem == 0) { /* ask the player what they want --Amy */

			if (yn("Do you want to acquire a random item?")=='y') {
				    acqo = mkobj_at(RANDOM_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire a weapon?")=='y') {
				    acqo = mkobj_at(WEAPON_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire an armor?")=='y') {
				    acqo = mkobj_at(ARMOR_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire a ring?")=='y') {
				    acqo = mkobj_at(RING_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire an amulet?")=='y') {
				    acqo = mkobj_at(AMULET_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire an implant?")=='y') {
				    acqo = mkobj_at(IMPLANT_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire a tool?")=='y') {
				    acqo = mkobj_at(TOOL_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire some food?")=='y') {
				    acqo = mkobj_at(FOOD_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire a potion?")=='y') {
				    acqo = mkobj_at(POTION_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire a scroll?")=='y') {
				    acqo = mkobj_at(SCROLL_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire a spellbook?")=='y') {
				    acqo = mkobj_at(SPBOOK_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire a wand?")=='y') {
				    acqo = mkobj_at(WAND_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire some coins?")=='y') {
				    acqo = mkobj_at(COIN_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire a gem?")=='y') {
				    acqo = mkobj_at(GEM_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire a boulder or statue?")=='y') {
				    acqo = mkobj_at(ROCK_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire a heavy iron ball?")=='y') {
				    acqo = mkobj_at(BALL_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire an iron chain?")=='y') {
				    acqo = mkobj_at(CHAIN_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
			else if (yn("Do you want to acquire a splash of venom?")=='y') {
				    acqo = mkobj_at(VENOM_CLASS, u.ux, u.uy, FALSE);	acquireditem = 1; }
	
		}
		if (!acqo) {
			pline("Unfortunately it failed.");
			return;
		}

		/* special handling to prevent wands of wishing or similarly overpowered items --Amy */

		if (acqo->otyp == GOLD_PIECE) acqo->quan = rnd(1000);
		if (acqo->otyp == MAGIC_LAMP || acqo->otyp == TREASURE_CHEST) { acqo->otyp = OIL_LAMP; acqo->age = 1500L; }
		if (acqo->otyp == MAGIC_MARKER) acqo->recharged = 1;
	    while(acqo->otyp == WAN_WISHING || acqo->otyp == WAN_POLYMORPH || acqo->otyp == WAN_MUTATION || acqo->otyp == WAN_ACQUIREMENT)
		acqo->otyp = rnd_class(WAN_LIGHT, WAN_PSYBEAM);
	    while (acqo->otyp == SCR_WISHING || acqo->otyp == SCR_RESURRECTION || acqo->otyp == SCR_ACQUIREMENT || acqo->otyp == SCR_ENTHRONIZATION || acqo->otyp == SCR_MAKE_PENTAGRAM || acqo->otyp == SCR_FOUNTAIN_BUILDING || acqo->otyp == SCR_SINKING || acqo->otyp == SCR_WC)
		acqo->otyp = rnd_class(SCR_CREATE_MONSTER, SCR_BLANK_PAPER);

		pline("Something appeared on the ground just beneath you!");
	}

}

/* LSZ/WWA Wizard Patch June '96 Choose location where spell takes effect.*/
/* WAC made into a void */
void
throwspell()
{
	coord cc;


/*	if (u.uinwater) {
		pline("You joking! In this weather?"); return 0; 
	} else if (Is_waterlevel(&u.uz)) { 
		You("better wait for the sun to come out."); return 0; 
	} */
	pline("Where do you want to cast the spell?");
	cc.x = u.ux;
	cc.y = u.uy;
	while (1) {
		getpos(&cc, TRUE, "the desired position");
		if(cc.x == -10) {
			u.dx = u.ux; u.dy = u.uy;
			return; /* user pressed esc */
		}
		/* The number of moves from hero to where the spell drops.*/
		if (distmin(u.ux, u.uy, cc.x, cc.y) > 10) {
			You("can't reach that far with your mind!");
		} else if (u.uswallow) {
			pline("The spell is cut short!");
			u.dx = u.ux;
			u.dy = u.uy;
			return;
		} else if (!cansee(cc.x, cc.y) || IS_STWALL(levl[cc.x][cc.y].typ)) {
			Your("mind fails to lock onto that location!");
		} else {
			/* Bingo! */
			u.dx=cc.x;
			u.dy=cc.y;
			return;
		}
	}
}


#endif /*OVL2*/

/*zap.c*/
