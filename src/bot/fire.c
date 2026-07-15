#include "../header/bot.h"
#include "../header/shared.h"
#include "../header/player.h"
#include "policy.h"

static bool Bot_TeamCombat (void)
{
	return tdm->value || ctf->value ||
	       ((int)dmflags->value & (DF_MODELTEAMS | DF_SKINTEAMS));
}

static bot_policy_weapon_kind_t Bot_PolicyWeaponKind (int weapon)
{
	switch (weapon) {
	case WEAP_BLASTER: return BOT_WEAPON_BLASTER;
	case WEAP_SHOTGUN: return BOT_WEAPON_SHOTGUN;
	case WEAP_SUPERSHOTGUN: return BOT_WEAPON_SUPERSHOTGUN;
	case WEAP_MACHINEGUN: return BOT_WEAPON_MACHINEGUN;
	case WEAP_CHAINGUN: return BOT_WEAPON_CHAINGUN;
	case WEAP_GRENADES: return BOT_WEAPON_GRENADES;
	case WEAP_GRENADELAUNCHER: return BOT_WEAPON_GRENADELAUNCHER;
	case WEAP_ROCKETLAUNCHER: return BOT_WEAPON_ROCKETLAUNCHER;
	case WEAP_HYPERBLASTER: return BOT_WEAPON_HYPERBLASTER;
	case WEAP_RAILGUN: return BOT_WEAPON_RAILGUN;
	case WEAP_BFG: return BOT_WEAPON_BFG;
	default: return BOT_WEAPON_OTHER;
	}
}

static float Bot_PreferredWeaponScore (edict_t *ent, int weapon, float distance,
	int nearby_enemies, int target_health, int rank)
{
	bot_policy_weapon_t policy = {0};

	policy.kind = Bot_PolicyWeaponKind(weapon);
	policy.distance = distance;
	policy.available_shots = 1; /* CanUsewep performs the authoritative inventory check. */
	policy.preference = 2 - rank;
	policy.nearby_enemies = nearby_enemies;
	policy.target_health = target_health;
	policy.current_weapon = Get_KindWeapon(ent->client->pers.weapon) == weapon;
	return BotPolicy_WeaponScore(&policy);
}

/* Check the actual firing corridor, not only player-to-player visibility. */
static bool Bot_ClearShot (edict_t *ent, edict_t *target, float splash_radius)
{
	vec3_t start, end, forward, delta;
	trace_t tr;
	int i;
	float self_radius = splash_radius > 165.0f ? 120.0f : splash_radius;

	AngleVectors(ent->s.angles, forward, NULL, NULL);
	VectorCopy(ent->s.origin, start);
	start[2] += ent->viewheight - 8;
	VectorMA(start, 8, forward, start);
	if (splash_radius > 0) {
		VectorCopy(target->s.origin, end);
		end[2] += target->viewheight * 0.5f;
	} else {
		VectorMA(start, 8192, forward, end);
	}

	tr = gi.trace(start, NULL, NULL, end, ent, MASK_SHOT);
	if (tr.ent && tr.ent->client && Bot_TeamCombat() && OnSameTeam(ent, tr.ent))
		return false;
	if (splash_radius <= 0)
		return true;

	/* Refuse splash shots that detonate beside the bot or a teammate. */
	if (tr.fraction < 0.18f)
		return false;
	if (tr.ent && tr.ent != target && tr.ent->client)
		return false;
	VectorSubtract(tr.endpos, ent->s.origin, delta);
	if (VectorLength(delta) < self_radius + 24.0f)
		return false;
	if (Bot_TeamCombat()) {
		for (i = 1; i <= (int)maxclients->value; i++) {
			edict_t *mate = &g_edicts[i];
			if (!mate->inuse || !mate->client || mate == ent || mate->deadflag ||
			    !OnSameTeam(ent, mate))
				continue;
			VectorSubtract(tr.endpos, mate->s.origin, delta);
			if (VectorLength(delta) < splash_radius + 24.0f)
				return false;
		}
	}
	return true;
}

/* Apply safety after all legacy battle states have made their decision. */
void Bot_ValidateAttack (edict_t *ent)
{
	edict_t *target;
	int weapon;
	float splash_radius = 0;

	if (!ent || !ent->client || !(ent->client->buttons & BUTTON_ATTACK))
		return;
	weapon = Get_KindWeapon(ent->client->pers.weapon);
	if (weapon == WEAP_GRAPPLE)
		return;
	target = ent->client->zc.first_target;
	if (!target || !target->inuse || !target->client)
		return;
	switch (weapon) {
	case WEAP_ROCKETLAUNCHER: splash_radius = 120.0f; break;
	case WEAP_GRENADELAUNCHER: splash_radius = 160.0f; break;
	case WEAP_GRENADES: splash_radius = 165.0f; break;
	case WEAP_BFG: splash_radius = 1000.0f; break;
	case WEAP_PHALANX: splash_radius = 120.0f; break;
	default: break;
	}
	if (!Bot_ClearShot(ent, target, splash_radius)) {
		ent->client->buttons &= ~BUTTON_ATTACK;
		ent->client->zc.weapon_action = BOT_WEAPON_BLOCKED;
	}
}

//======================================================================
//aim決定
//ent	entity
//aim	aimスキル
//yaw	dist
//wep	weapon
void Get_AimAngle (edict_t *ent, float aim, float dist, int weapon)
{
	edict_t *target;
	vec3_t targaim, v;
	trace_t rs_trace;

	target = ent->client->zc.first_target;
	if ((ent->client->zc.battlemode & FIRE_ESTIMATE) &&
	    ent->client->zc.target_seen_time > 0) {
		VectorSubtract(ent->client->zc.vtemp, ent->s.origin, targaim);
		ent->s.angles[YAW] = Get_yaw(targaim);
		ent->s.angles[PITCH] = Get_pitch(targaim);
		return;
	}

	/* Instagib is hitscan: track the body with skill-scaled, persistent error.
	 * Do not lead velocity as if the rail projectile had travel time. */
	if (instagib && instagib->value && weapon == WEAP_RAILGUN) {
		int aim_skill = Bot[ent->client->zc.botindex].param[BOP_AIM];
		float miss_skill;
		float yaw_error;
		float pitch_error;
		float phase = level.time * 2.1f + ent->client->zc.botindex * 0.73f;

		if (aim_skill > 9)
			aim_skill = 9;
		miss_skill = 9.0f - aim_skill;
		/* Angular error matters at every range.  At skill 5 this is large
		 * enough to miss a player regularly instead of remaining inside the
		 * bounding box as the old world-unit offset did. */
		yaw_error = 0.12f + miss_skill * 0.55f;
		pitch_error = 0.08f + miss_skill * 0.38f;

		VectorCopy(target->s.origin, targaim);
		targaim[2] += target->viewheight * 0.45f;
		VectorSubtract(targaim, ent->s.origin, targaim);
		ent->s.angles[YAW] = Get_yaw(targaim);
		ent->s.angles[PITCH] = Get_pitch(targaim);
		ent->s.angles[YAW] += sinf(phase) * yaw_error;
		ent->s.angles[PITCH] += cosf(phase * 0.79f) * pitch_error;
		return;
	}
	if (weapon == WEAP_ROCKETLAUNCHER || weapon == WEAP_HYPERBLASTER ||
	    weapon == WEAP_PHALANX || weapon == WEAP_BOOMER) {
		float speed = weapon == WEAP_ROCKETLAUNCHER ? 650.0f :
		              weapon == WEAP_HYPERBLASTER ? 1000.0f :
		              weapon == WEAP_PHALANX ? 725.0f : 500.0f;
		float lead = BotPolicy_ProjectileLead(dist, speed,
			Bot[ent->client->zc.botindex].param[BOP_AIM]);
		float error = aim * 0.35f;
		VectorMA(target->s.origin, lead, target->velocity, targaim);
		targaim[2] += target->viewheight * 0.35f;
		VectorSubtract(targaim, ent->s.origin, targaim);
		ent->s.angles[YAW] = Get_yaw(targaim) + (random() - 0.5f) * error;
		ent->s.angles[PITCH] = Get_pitch(targaim) + (random() - 0.5f) * error;
		return;
	}

	switch (weapon) {
		//即判定
		case WEAP_SHOTGUN:
		case WEAP_SUPERSHOTGUN:
		case WEAP_RAILGUN:
			if (target != ent->client->zc.last_target) {
				if (target->svflags & SVF_MONSTER) {
					VectorSubtract(target->s.old_origin, target->s.origin, targaim);
				} else {
					VectorCopy(target->velocity, targaim);
					VectorInverse(targaim);
				}
				VectorNormalize(targaim);
				VectorMA(target->s.origin, random() * aim * AIMING_POSGAP * random(), targaim, targaim);
			} else {
				VectorSubtract(ent->client->zc.last_pos, target->s.origin, targaim);
				//				VectorScale (targaim, vec_t scale, vec3_t out)
				VectorMA(target->s.origin, aim * /*VectorLength(targaim)**/ random(), targaim, targaim);
			}
			VectorSubtract(targaim, ent->s.origin, targaim);

			ent->s.angles[YAW] = Get_yaw(targaim);
			ent->s.angles[PITCH] = Get_pitch(targaim);

			ent->s.angles[YAW] += aim * AIMING_ANGLEGAP_S * (random() - 0.5) * 2;
			if (ent->s.angles[YAW] > 180)
				ent->s.angles[YAW] -= 360;
			else if (ent->s.angles[YAW] < -180)
				ent->s.angles[YAW] += 360;

			ent->s.angles[PITCH] += (aim * AIMING_ANGLEGAP_S * (random() - 0.5) * 2);
			if (ent->s.angles[PITCH] > 90)
				ent->s.angles[PITCH] = 90;
			else if (ent->s.angles[PITCH] < -90)
				ent->s.angles[PITCH] = -90;
			break;

		case WEAP_MACHINEGUN:
		case WEAP_CHAINGUN:
			if (target != ent->client->zc.last_target) {
				if (target->svflags & SVF_MONSTER) {
					VectorSubtract(target->s.old_origin, target->s.origin, targaim);
				} else {
					VectorCopy(target->velocity, targaim);
					VectorInverse(targaim);
				}
				VectorNormalize(targaim);
				VectorMA(target->s.origin, random() * aim * AIMING_POSGAP, targaim, targaim);
			} else {
				VectorSubtract(ent->client->zc.last_pos, target->s.origin, targaim);
				VectorMA(target->s.origin, random() * aim /** VectorLength(targaim)*/, targaim, targaim);
			}
			VectorSubtract(targaim, ent->s.origin, targaim);

			ent->s.angles[YAW] = Get_yaw(targaim);
			ent->s.angles[PITCH] = Get_pitch(targaim);

			ent->s.angles[YAW] += aim * AIMING_ANGLEGAP_M * (random() - 0.5) * 2;
			if (ent->s.angles[YAW] > 180)
				ent->s.angles[YAW] -= 360;
			else if (ent->s.angles[YAW] < -180)
				ent->s.angles[YAW] += 360;

			ent->s.angles[PITCH] += (aim * AIMING_ANGLEGAP_M * (random() - 0.5) * 2);
			if (ent->s.angles[PITCH] > 90)
				ent->s.angles[PITCH] = 90;
			else if (ent->s.angles[PITCH] < -90)
				ent->s.angles[PITCH] = -90;
			break;

		case WEAP_BLASTER:
		case WEAP_GRENADES:
		case WEAP_GRENADELAUNCHER:
		case WEAP_ROCKETLAUNCHER:
		case WEAP_PHALANX:
		case WEAP_BOOMER:
			if (target != ent->client->zc.last_target) {
				if (target->svflags & SVF_MONSTER) {
					VectorSubtract(target->s.origin, target->s.old_origin, targaim);
				} else {
					VectorCopy(target->velocity, targaim);
					targaim[0] *= 32;
					targaim[1] *= 32;
					targaim[2] *= 32;
				}
				VectorNormalize(targaim);
				VectorMA(target->s.origin, (11 - aim) * dist / 25, targaim, targaim);
			} else {
				VectorSubtract(target->s.origin, ent->client->zc.last_pos, targaim);
				targaim[2] /= 2;
				VectorMA(target->s.origin, -aim * random() + dist / 75, targaim, targaim);
			}
			rs_trace = gi.trace(target->s.origin, NULL, NULL, targaim, target, MASK_SHOT);
			VectorCopy(rs_trace.endpos, targaim);

			if (weapon == WEAP_GRENADELAUNCHER || weapon == WEAP_ROCKETLAUNCHER || weapon == WEAP_PHALANX) {
				if (targaim[2] < (ent->s.origin[2] + JumpMax)) {
					targaim[2] -= 24;

					VectorCopy(ent->s.origin, v);
					v[2] += ent->viewheight - 8;
					rs_trace = gi.trace(v, NULL, NULL, targaim, ent, MASK_SHOT);
					if (rs_trace.fraction != 1.0)
						targaim[2] += 24;
				} else if (targaim[2] > (ent->s.origin[2] + JumpMax))
					targaim[2] += 5;
			}

			VectorSubtract(targaim, ent->s.origin, targaim);

			ent->s.angles[YAW] = Get_yaw(targaim);
			ent->s.angles[PITCH] = Get_pitch(targaim);

			ent->s.angles[YAW] += aim * AIMING_ANGLEGAP_M * (random() - 0.5) * 2;
			if (ent->s.angles[YAW] > 180)
				ent->s.angles[YAW] -= 360;
			else if (ent->s.angles[YAW] < -180)
				ent->s.angles[YAW] += 360;

			ent->s.angles[PITCH] += (aim * AIMING_ANGLEGAP_M * (random() - 0.5) * 2);
			if (ent->s.angles[PITCH] > 90)
				ent->s.angles[PITCH] = 90;
			else if (ent->s.angles[PITCH] < -90)
				ent->s.angles[PITCH] = -90;
			break;

		case WEAP_HYPERBLASTER:
			if (target != ent->client->zc.last_target) {
				if (target->svflags & SVF_MONSTER) {
					VectorSubtract(target->s.origin, target->s.old_origin, targaim);
				} else {
					VectorCopy(target->velocity, targaim);
					targaim[0] *= 32;
					targaim[1] *= 32;
					targaim[2] *= 32;
				}
				VectorNormalize(targaim);
				VectorMA(target->s.origin, (11 - aim) * dist / 100, targaim, targaim);
			} else {
				VectorSubtract(target->s.origin, ent->client->zc.last_pos, targaim);
				targaim[2] /= 2;
				VectorMA(target->s.origin, -aim + dist / 115, targaim, targaim);
			}
			rs_trace = gi.trace(target->s.origin, NULL, NULL, targaim, target, MASK_SHOT);
			VectorCopy(rs_trace.endpos, targaim);

			VectorSubtract(targaim, ent->s.origin, targaim);

			ent->s.angles[YAW] = Get_yaw(targaim);
			ent->s.angles[PITCH] = Get_pitch(targaim);

			ent->s.angles[YAW] += aim * AIMING_ANGLEGAP_M * (random() - 0.5) * 2;
			if (ent->s.angles[YAW] > 180)
				ent->s.angles[YAW] -= 360;
			else if (ent->s.angles[YAW] < -180)
				ent->s.angles[YAW] += 360;

			ent->s.angles[PITCH] += (aim * AIMING_ANGLEGAP_M * (random() - 0.5) * 2);
			if (ent->s.angles[PITCH] > 90)
				ent->s.angles[PITCH] = 90;
			else if (ent->s.angles[PITCH] < -90)
				ent->s.angles[PITCH] = -90;
			break;

		case WEAP_BFG:
			VectorCopy(ent->client->zc.vtemp, targaim);
			VectorSubtract(targaim, ent->s.origin, targaim);

			ent->s.angles[YAW] = Get_yaw(targaim);
			ent->s.angles[PITCH] = Get_pitch(targaim);
			break;
		default:
			break;
	}
}


//======================================================================
//武器使用可能？
int CanUsewep (edict_t *ent, int weapon)
{
	gitem_t *item;
	gclient_t *client;
	int mywep, ammoindex;

	client = ent->client;

	mywep = Get_KindWeapon(client->pers.weapon);

	switch (weapon) {
		case WEAP_BLASTER:
			item = Fdi_BLASTER; //FindItem("Blaster");
			if (client->pers.inventory[ITEM_INDEX(item)]) {
				if (mywep == WEAP_BLASTER || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_SHOTGUN:
			item = Fdi_SHOTGUN; //FindItem("Shotgun");
			ammoindex = ITEM_INDEX(Fdi_SHELLS /*FindItem("Shells")*/);
			if (client->pers.inventory[ITEM_INDEX(item)] && client->pers.inventory[ammoindex] > 0) {
				if (mywep == WEAP_SHOTGUN || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_SUPERSHOTGUN:
			item = Fdi_SUPERSHOTGUN; //FindItem("Super Shotgun");
			ammoindex = ITEM_INDEX(Fdi_SHELLS /*FindItem("Shells")*/);

			if (client->pers.inventory[ITEM_INDEX(item)] && client->pers.inventory[ammoindex] > 1) {
				if (mywep == WEAP_SUPERSHOTGUN || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_MACHINEGUN:
			item = Fdi_MACHINEGUN; //FindItem("Machinegun");
			ammoindex = ITEM_INDEX(Fdi_BULLETS /*FindItem("Bullets")*/);

			if (client->pers.inventory[ITEM_INDEX(item)] && client->pers.inventory[ammoindex] > 0) {
				if (client->pers.weapon != item)
					item->use(ent, item);

				if (mywep == WEAP_MACHINEGUN || client->weaponstate == WEAPON_READY || client->weaponstate == WEAPON_FIRING) {
					//					if(client->pers.weapon == item) return true;
					//					else {item->use(ent,item); return 2;}
					if (client->weaponstate == WEAPON_READY || client->weaponstate == WEAPON_FIRING)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_CHAINGUN:
			item = FindItem("Chaingun");
			ammoindex = ITEM_INDEX(Fdi_BULLETS /*FindItem("Bullets")*/);

			if (client->pers.inventory[ITEM_INDEX(item)] && client->pers.inventory[ammoindex] > 0) {
				if (mywep == WEAP_CHAINGUN || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY || client->weaponstate == WEAPON_FIRING)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_GRENADES:
			item = Fdi_GRENADES; //FindItem("Grenades");
			ammoindex = ITEM_INDEX(Fdi_GRENADES /*FindItem("Grenades")*/);

			if (client->pers.inventory[ammoindex] > 0) {
				if (mywep == WEAP_GRENADES || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_TRAP:
			item = Fdi_TRAP; //FindItem("Trap");
			ammoindex = ITEM_INDEX(Fdi_TRAP /*FindItem("Trap")*/);

			if (client->pers.inventory[ammoindex] > 0) {
				if (mywep == WEAP_GRENADES || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_GRENADELAUNCHER:
			item = Fdi_GRENADELAUNCHER; //FindItem("Grenade Launcher");
			ammoindex = ITEM_INDEX(Fdi_GRENADES /*FindItem("Grenades")*/);

			if (client->pers.inventory[ITEM_INDEX(item)] && client->pers.inventory[ammoindex] > 0) {
				if (mywep == WEAP_GRENADELAUNCHER || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_ROCKETLAUNCHER:
			item = Fdi_ROCKETLAUNCHER; //FindItem("Rocket Launcher");
			ammoindex = ITEM_INDEX(Fdi_ROCKETS /*FindItem("Rockets")*/);

			if (client->pers.inventory[ITEM_INDEX(item)] && client->pers.inventory[ammoindex] > 0) {
				if (mywep == WEAP_ROCKETLAUNCHER || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_HYPERBLASTER:
			item = Fdi_HYPERBLASTER; //FindItem("HyperBlaster");
			ammoindex = ITEM_INDEX(Fdi_CELLS /*FindItem("Cells")*/);

			if (client->pers.inventory[ITEM_INDEX(item)] && client->pers.inventory[ammoindex] > 0) {
				if (mywep == WEAP_HYPERBLASTER || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY || client->weaponstate == WEAPON_FIRING)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_BOOMER:
			item = Fdi_BOOMER; //FindItem("Ionripper");
			ammoindex = ITEM_INDEX(Fdi_CELLS /*FindItem("Cells")*/);

			if (client->pers.inventory[ITEM_INDEX(item)] && client->pers.inventory[ammoindex] > 0) {
				if (mywep == WEAP_BOOMER || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY || client->weaponstate == WEAPON_FIRING)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_RAILGUN:
			item = Fdi_RAILGUN; //FindItem("Railgun");
			ammoindex = ITEM_INDEX(Fdi_SLUGS /*FindItem("Slugs")*/);

			if (client->pers.inventory[ITEM_INDEX(item)] && client->pers.inventory[ammoindex] > 0) {
				if (mywep == WEAP_RAILGUN || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_PHALANX:
			item = Fdi_PHALANX; //FindItem("Phalanx");
			ammoindex = ITEM_INDEX(Fdi_MAGSLUGS /*FindItem("Mag Slug")*/);

			if (client->pers.inventory[ITEM_INDEX(item)] && client->pers.inventory[ammoindex] > 0) {
				if (mywep == WEAP_PHALANX || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY)
						return true;
					else
						return 2;
				}
			}
			break;

		case WEAP_BFG:
			item = Fdi_BFG; //FindItem("BFG10K");
			ammoindex = ITEM_INDEX(Fdi_CELLS /*FindItem("Cells")*/);

			if (client->pers.inventory[ITEM_INDEX(item)] && client->pers.inventory[ammoindex] >= 50) {
				if (mywep == WEAP_BFG || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY)
						return true;
					else
						return 2;
				}
			}
			break;
		default:		    //case WEAP_BLASTER:
			item = Fdi_BLASTER; //FindItem("Blaster");
			if (client->pers.inventory[ITEM_INDEX(item)]) {
				if (mywep == WEAP_BLASTER || client->weaponstate == WEAPON_READY) {
					item->use(ent, item);
					if (client->weaponstate == WEAPON_READY)
						return true;
					else
						return 2;
				}
			}
			break;
	}
	return false;
}

//------------------------------------------------------------

//	Use BFG

//------------------------------------------------------------
bool B_UseBfg (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int k, mywep;
	zgcl_t *zc;
	gclient_t *client;

	client = ent->client;
	zc = &client->zc;

	if (CanUsewep(ent, WEAP_BFG)) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		if ((k = Bot_traceS(ent, target)))
			VectorCopy(target->s.origin, zc->vtemp);

		if (FFlg[skill] & FIRE_STAYFIRE) {
			if (k /*&& random() < 0.8*/) {
				client->buttons |= BUTTON_ATTACK;
				zc->battlemode |= FIRE_STAYFIRE; //モード遷移
				zc->battlecount = 8 + (int)(10 * random());
				trace_priority = TRP_ALLKEEP;
				return true;
			}
		}
		//爆発回避
		else if ((FFlg[skill] & FIRE_EXPAVOID) && distance < 300 /*&& random() < 0.5 */
			 && Bot_traceS(ent, target)) {
			if (ent->groundentity || zc->waterstate) {
				zc->battlemode |= FIRE_EXPAVOID;
				zc->battlecount = 6 + (int)(6 * random());
				trace_priority = TRP_ALLKEEP;
				return true;
			}
		}
		//普通
		else if (!(FFlg[skill] & (FIRE_STAYFIRE | FIRE_EXPAVOID))) {
			if (k /*&& random() < 0.8*/) {
				zc->battlemode |= FIRE_BFG;
				zc->battlecount = 6 + (int)(6 * random());
				trace_priority = TRP_ANGLEKEEP;
				return true;
			}
		} else if ((FFlg[skill] & FIRE_EXPAVOID) && Bot_traceS(ent, target)) {
			if (k /*&& random() < 0.8*/) {
				zc->battlemode |= FIRE_BFG;
				zc->battlecount = 6 + (int)(6 * random());
				trace_priority = TRP_ANGLEKEEP;
				return true;
			}
		}
	}
	return false;
}

//------------------------------------------------------------

//	Use Hyper Blaster

//------------------------------------------------------------
bool B_UseHyperBlaster (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int mywep;
	gclient_t *client;

	client = ent->client;

	if (CanUsewep(ent, WEAP_HYPERBLASTER)) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		client->buttons |= BUTTON_ATTACK;
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		return true;
	}
	return false;
}

//------------------------------------------------------------

//	Use Phalanx

//------------------------------------------------------------
bool B_UsePhalanx (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int mywep;
	zgcl_t *zc;
	gclient_t *client;

	client = ent->client;
	zc = &client->zc;

	if (CanUsewep(ent, WEAP_PHALANX)) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		if ((FFlg[skill] & FIRE_PRESTAYFIRE) && ((distance > 500 && random() < 0.1) || fabs(ent->s.angles[PITCH]) > 45) && Bot_traceS(ent, target) && (enewep <= WEAP_MACHINEGUN || enewep == WEAP_GRENADES)) {
			if (ent->groundentity || zc->waterstate) {
				zc->battlemode |= FIRE_PRESTAYFIRE;
				zc->battlecount = 2 + (int)(6 * random());
				trace_priority = TRP_ALLKEEP;
				return true;
			}
		}
		if ((FFlg[skill] & FIRE_JUMPROC) && random() < 0.3 && (target->s.origin[2] - ent->s.origin[2]) < JumpMax && !(client->ps.pmove.pm_flags && PMF_DUCKED)) {
			if (ent->groundentity && !(ent->waterlevel <= 1)) {
				if (zc->route_trace) {
					if (Bot_Fall(ent, ent->s.origin, 0)) {
						trace_priority = TRP_ALLKEEP;
						if (Bot_traceS(ent, target))
							client->buttons |= BUTTON_ATTACK;
						return true;
					}
				} else {
					ent->moveinfo.speed = 0;
					ent->velocity[2] += VEL_BOT_JUMP;
					gi.sound(ent, CHAN_VOICE, gi.soundindex("*jump1.wav"), 1, ATTN_NORM, 0);
					PlayerNoise(ent, ent->s.origin, PNOISE_SELF); //pon
					Set_BotAnim(ent, ANIM_JUMP, FRAME_jump1 - 1, FRAME_jump6);
					trace_priority = TRP_ALLKEEP;
					if (Bot_traceS(ent, target))
						client->buttons |= BUTTON_ATTACK;
					return true;
				}
			}
		} else if ((FFlg[skill] & FIRE_EXPAVOID) && distance < 300 && random() < 0.5 && Bot_traceS(ent, target)) {
			if (ent->groundentity || zc->waterstate) {
				zc->battlemode |= FIRE_EXPAVOID;
				zc->battlecount = 4 + (int)(6 * random());
				trace_priority = TRP_ALLKEEP;
				return true;
			}
		}
		if (Bot_traceS(ent, target) && Bot_ClearShot(ent, target, 120.0f))
			client->buttons |= BUTTON_ATTACK;
		return true;
	}
	return false;
}


//------------------------------------------------------------

//	Use Rocket

//------------------------------------------------------------
bool B_UseRocket (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int mywep;
	zgcl_t *zc;
	gclient_t *client;

	client = ent->client;
	zc = &client->zc;

	if (CanUsewep(ent, WEAP_ROCKETLAUNCHER)) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		if ((FFlg[skill] & FIRE_PRESTAYFIRE) && ((distance > 500 && random() < 0.1) || fabs(ent->s.angles[PITCH]) > 45) && Bot_traceS(ent, target) && (enewep <= WEAP_MACHINEGUN || enewep == WEAP_GRENADES)) {
			if (ent->groundentity || zc->waterstate) {
				zc->battlemode |= FIRE_PRESTAYFIRE;
				zc->battlecount = 2 + (int)(6 * random());
				trace_priority = TRP_ALLKEEP;
				return true;
			}
		}
		if ((FFlg[skill] & FIRE_JUMPROC) && random() < 0.3 && (target->s.origin[2] - ent->s.origin[2]) < JumpMax && !(client->ps.pmove.pm_flags && PMF_DUCKED)) {
			if (ent->groundentity && !(ent->waterlevel <= 1)) {
				if (zc->route_trace) {
					if (Bot_Fall(ent, ent->s.origin, 0)) {
						trace_priority = TRP_ALLKEEP;
						if (Bot_traceS(ent, target))
							client->buttons |= BUTTON_ATTACK;
						return true;
					}
				} else {
					ent->moveinfo.speed = 0;

					ent->velocity[2] += VEL_BOT_JUMP;
					gi.sound(ent, CHAN_VOICE, gi.soundindex("*jump1.wav"), 1, ATTN_NORM, 0);
					PlayerNoise(ent, ent->s.origin, PNOISE_SELF); //pon
					Set_BotAnim(ent, ANIM_JUMP, FRAME_jump1 - 1, FRAME_jump6);
					trace_priority = TRP_ALLKEEP;
					if (Bot_traceS(ent, target))
						client->buttons |= BUTTON_ATTACK;
					return true;
				}
			}
		} else if ((FFlg[skill] & FIRE_EXPAVOID) && distance < 300 && random() < 0.5 && Bot_traceS(ent, target)) {
			if (ent->groundentity || zc->waterstate) {
				zc->battlemode |= FIRE_EXPAVOID;
				zc->battlecount = 4 + (int)(6 * random());
				trace_priority = TRP_ALLKEEP;
				return true;
			}
		}
		if (Bot_traceS(ent, target) && Bot_ClearShot(ent, target, 120.0f))
			client->buttons |= BUTTON_ATTACK;
		return true;
	}
	return false;
}


//------------------------------------------------------------

//	Use Boomer

//------------------------------------------------------------
bool B_UseBoomer (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int mywep;
	gclient_t *client;

	client = ent->client;

	if (CanUsewep(ent, WEAP_BOOMER)) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		client->buttons |= BUTTON_ATTACK;
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		return true;
	}
	return false;
}

//------------------------------------------------------------

//	Use Railgun

//------------------------------------------------------------
bool B_UseRailgun (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int mywep;
	gclient_t *client;

	client = ent->client;

	if (CanUsewep(ent, WEAP_RAILGUN)) {
		int aim_skill = Bot[client->zc.botindex].param[BOP_AIM];
		int reaction_skill = Bot[client->zc.botindex].param[BOP_REACTION];
		int combat_skill = Bot[client->zc.botindex].param[BOP_COMBATSKILL];

		if (aim_skill > 9)
			aim_skill = 9;
		if (reaction_skill > 9)
			reaction_skill = 9;
		if (combat_skill > 9)
			combat_skill = 9;
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		if (Bot_ClearShot(ent, target, 0)) {
			if (!(instagib && instagib->value) || level.time >= client->zc.next_fire_time) {
				client->buttons |= BUTTON_ATTACK;
				if (instagib && instagib->value) {
					float delay = 0.62f - reaction_skill * 0.025f - combat_skill * 0.015f;
					float hesitation = (9 - aim_skill) * 0.018f;
					if (delay < 0.24f)
						delay = 0.24f;
					client->zc.next_fire_time = level.time + delay + random() * (0.08f + hesitation);
				}
			}
		}
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		return true;
	}
	return false;
}

//------------------------------------------------------------

//	Use Grenade Launcher

//------------------------------------------------------------
bool B_UseGrenadeLauncher (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int mywep;
	zgcl_t *zc;
	gclient_t *client;

	client = ent->client;
	zc = &client->zc;

	if (CanUsewep(ent, WEAP_GRENADELAUNCHER)) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		if ((FFlg[skill] & FIRE_STAYFIRE) && random() < 0.3 && target->s.origin[2] < ent->s.origin[2]) {
			if (ent->groundentity || zc->waterstate) {
				if (Bot_traceS(ent, target)) {
					zc->battlemode |= FIRE_STAYFIRE;
					zc->battlecount = 5 + (int)(10 * random());
					trace_priority = TRP_ALLKEEP;
					client->buttons |= BUTTON_ATTACK;
					return true;
				}
			}
		} else if ((FFlg[skill] & FIRE_EXPAVOID) && distance < 300 && random() < 0.5 && Bot_traceS(ent, target)) {
			if (ent->groundentity || zc->waterstate) {
				zc->battlemode |= FIRE_EXPAVOID;
				zc->battlecount = 2 + (int)(6 * random());
				trace_priority = TRP_ALLKEEP;
				return true;
			}
		}
		if (distance >= 140 && Bot_ClearShot(ent, target, 160.0f))
			client->buttons |= BUTTON_ATTACK;
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		return true;
	}
	return false;
}

//------------------------------------------------------------

//	Use Chain Gun

//------------------------------------------------------------
bool B_UseChainGun (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int mywep;
	gclient_t *client;

	client = ent->client;

	if (CanUsewep(ent, WEAP_CHAINGUN)) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		client->buttons |= BUTTON_ATTACK;
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		return true;
	}
	return false;
}


//------------------------------------------------------------

//	Use Machine Gun

//------------------------------------------------------------
bool B_UseMachineGun (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int k, mywep;
	gclient_t *client;

	client = ent->client;

	if ((k = CanUsewep(ent, WEAP_MACHINEGUN))) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		if (k == true)
			client->buttons |= BUTTON_ATTACK;
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		return true;
	}
	return false;
}

//------------------------------------------------------------

//	Use S-Shotgun

//------------------------------------------------------------
bool B_UseSuperShotgun (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int mywep;
	gclient_t *client;

	client = ent->client;

	if (CanUsewep(ent, WEAP_SUPERSHOTGUN)) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		client->buttons |= BUTTON_ATTACK;
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		return true;
	}
	return false;
}

//------------------------------------------------------------

//	Use Shotgun

//------------------------------------------------------------
bool B_UseShotgun (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int mywep;
	gclient_t *client;

	client = ent->client;

	if (CanUsewep(ent, WEAP_SHOTGUN)) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		client->buttons |= BUTTON_ATTACK;
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		return true;
	}
	return false;
}

//------------------------------------------------------------

//	Use Hand Grenade

//------------------------------------------------------------
bool B_UseHandGrenade (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int mywep;
	gclient_t *client;

	client = ent->client;

	if (CanUsewep(ent, WEAP_GRENADES)) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		if (ent->client->weaponstate == WEAPON_READY)
			client->buttons |= BUTTON_ATTACK;
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		return true;
	}
	return false;
}

//------------------------------------------------------------

//	Use Trap

//------------------------------------------------------------
bool B_UseTrap (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int mywep;
	gclient_t *client;

	client = ent->client;

	if (CanUsewep(ent, WEAP_TRAP)) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		if (ent->client->weaponstate == WEAPON_READY)
			client->buttons |= BUTTON_ATTACK;
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		return true;
	}
	return false;
}


//------------------------------------------------------------

//	Use Blaster

//------------------------------------------------------------
bool B_UseBlaster (edict_t *ent, edict_t *target, int enewep, float aim, float distance, int skill)
{
	int mywep;
	gclient_t *client;

	client = ent->client;

	if (CanUsewep(ent, WEAP_BLASTER)) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		client->buttons |= BUTTON_ATTACK;
		if (trace_priority < TRP_ANGLEKEEP)
			trace_priority = TRP_ANGLEKEEP;
		return true;
		;
	}
	return false;
}

static gitem_t *Bot_WeaponItem (int weapon)
{
	switch (weapon) {
	case WEAP_BLASTER: return Fdi_BLASTER;
	case WEAP_SHOTGUN: return Fdi_SHOTGUN;
	case WEAP_SUPERSHOTGUN: return Fdi_SUPERSHOTGUN;
	case WEAP_MACHINEGUN: return Fdi_MACHINEGUN;
	case WEAP_CHAINGUN: return Fdi_CHAINGUN;
	case WEAP_GRENADES: return Fdi_GRENADES;
	case WEAP_GRENADELAUNCHER: return Fdi_GRENADELAUNCHER;
	case WEAP_ROCKETLAUNCHER: return Fdi_ROCKETLAUNCHER;
	case WEAP_HYPERBLASTER: return Fdi_HYPERBLASTER;
	case WEAP_RAILGUN: return Fdi_RAILGUN;
	case WEAP_BFG: return Fdi_BFG;
	case WEAP_PHALANX: return Fdi_PHALANX;
	case WEAP_BOOMER: return Fdi_BOOMER;
	case WEAP_TRAP: return Fdi_TRAP;
	default: return NULL;
	}
}

static int Bot_WeaponShots (edict_t *ent, int weapon)
{
	gitem_t *item = Bot_WeaponItem(weapon);
	gitem_t *ammo;
	int quantity;

	if (!item)
		return 0;
	if (weapon == WEAP_BLASTER)
		return 999;
	if (item->flags & IT_AMMO)
		return ent->client->pers.inventory[ITEM_INDEX(item)];
	if (!ent->client->pers.inventory[ITEM_INDEX(item)])
		return 0;
	if (!item->ammo)
		return 999;
	ammo = FindItem(item->ammo);
	if (!ammo)
		return 0;
	quantity = item->quantity > 0 ? item->quantity : 1;
	return ent->client->pers.inventory[ITEM_INDEX(ammo)] / quantity;
}

static float Bot_WeaponSplash (int weapon)
{
	switch (weapon) {
	case WEAP_ROCKETLAUNCHER: return 120.0f;
	case WEAP_GRENADELAUNCHER: return 160.0f;
	case WEAP_GRENADES: return 165.0f;
	case WEAP_BFG: return 1000.0f;
	case WEAP_PHALANX: return 120.0f;
	default: return 0;
	}
}

static bool Bot_DirectCorridorSafe (edict_t *ent, edict_t *target)
{
	vec3_t start, end;
	trace_t tr;

	VectorCopy(ent->s.origin, start);
	start[2] += ent->viewheight - 8;
	VectorCopy(target->s.origin, end);
	end[2] += target->viewheight * 0.5f;
	tr = gi.trace(start, NULL, NULL, end, ent, MASK_SHOT);
	if (tr.ent && tr.ent->client && Bot_TeamCombat() && OnSameTeam(ent, tr.ent))
		return false;
	return tr.ent == target || tr.fraction == 1.0f;
}

static bot_weapon_action_t Bot_FireUtilityWeapon (edict_t *ent, edict_t *target,
	int weapon, int enewep, float aim, float distance, int skill)
{
	bool handled = false;
	float splash = Bot_WeaponSplash(weapon);

	if (Bot_WeaponShots(ent, weapon) <= 0)
		return BOT_WEAPON_UNAVAILABLE;
	if (splash > 0 && !Bot_ClearShot(ent, target, splash))
		return BOT_WEAPON_BLOCKED;
	if (splash <= 0 && !Bot_DirectCorridorSafe(ent, target))
		return BOT_WEAPON_BLOCKED;
	switch (weapon) {
	case WEAP_BFG: handled = B_UseBfg(ent, target, enewep, aim, distance, skill); break;
	case WEAP_HYPERBLASTER: handled = B_UseHyperBlaster(ent, target, enewep, aim, distance, skill); break;
	case WEAP_PHALANX: handled = B_UsePhalanx(ent, target, enewep, aim, distance, skill); break;
	case WEAP_ROCKETLAUNCHER: handled = B_UseRocket(ent, target, enewep, aim, distance, skill); break;
	case WEAP_BOOMER: handled = B_UseBoomer(ent, target, enewep, aim, distance, skill); break;
	case WEAP_RAILGUN: handled = B_UseRailgun(ent, target, enewep, aim, distance, skill); break;
	case WEAP_GRENADELAUNCHER: handled = B_UseGrenadeLauncher(ent, target, enewep, aim, distance, skill); break;
	case WEAP_CHAINGUN: handled = B_UseChainGun(ent, target, enewep, aim, distance, skill); break;
	case WEAP_MACHINEGUN: handled = B_UseMachineGun(ent, target, enewep, aim, distance, skill); break;
	case WEAP_SUPERSHOTGUN: handled = B_UseSuperShotgun(ent, target, enewep, aim, distance, skill); break;
	case WEAP_SHOTGUN: handled = B_UseShotgun(ent, target, enewep, aim, distance, skill); break;
	case WEAP_GRENADES: handled = B_UseHandGrenade(ent, target, enewep, aim, distance, skill); break;
	case WEAP_TRAP: handled = B_UseTrap(ent, target, enewep, aim, distance, skill); break;
	case WEAP_BLASTER: handled = B_UseBlaster(ent, target, enewep, aim, distance, skill); break;
	default: break;
	}
	if (!handled)
		return BOT_WEAPON_UNAVAILABLE;
	if (Get_KindWeapon(ent->client->pers.weapon) != weapon || ent->client->newweapon)
		return BOT_WEAPON_SWITCHING;
	return (ent->client->buttons & BUTTON_ATTACK) ? BOT_WEAPON_FIRED : BOT_WEAPON_BLOCKED;
}

static int Bot_RankWeapons (edict_t *ent, edict_t *target, float distance,
	int *ordered, float *scores, int capacity)
{
	static const int weapons[] = {
		WEAP_BFG, WEAP_RAILGUN, WEAP_ROCKETLAUNCHER, WEAP_HYPERBLASTER,
		WEAP_CHAINGUN, WEAP_GRENADELAUNCHER, WEAP_SUPERSHOTGUN,
		WEAP_MACHINEGUN, WEAP_SHOTGUN, WEAP_PHALANX, WEAP_BOOMER,
		WEAP_GRENADES, WEAP_TRAP, WEAP_BLASTER
	};
	int count = 0, i, j;

	for (i = 0; i < (int)(sizeof(weapons) / sizeof(weapons[0])) && count < capacity; i++) {
		bot_policy_weapon_t policy = {0};
		int weapon = weapons[i];
		policy.kind = Bot_PolicyWeaponKind(weapon);
		policy.distance = distance;
		policy.available_shots = Bot_WeaponShots(ent, weapon);
		policy.preference = weapon == Bot[ent->client->zc.botindex].param[BOP_PRIWEP] ? 3 :
		                    weapon == Bot[ent->client->zc.botindex].param[BOP_SECWEP] ? 2 : 0;
		policy.nearby_enemies = ent->client->zc.nearby_enemies;
		policy.target_health = ent->client->zc.target_visible ? target->health :
		                       ent->client->zc.target_last_seen_health;
		policy.current_weapon = Get_KindWeapon(ent->client->pers.weapon) == weapon;
		policy.splash_unsafe = Bot_WeaponSplash(weapon) > 0 &&
		                       !Bot_ClearShot(ent, target, Bot_WeaponSplash(weapon));
		policy.corridor_blocked = Bot_WeaponSplash(weapon) <= 0 &&
		                            !Bot_DirectCorridorSafe(ent, target);
		scores[count] = BotPolicy_WeaponScore(&policy);
		ordered[count++] = weapon;
	}
	for (i = 1; i < count; i++) {
		int weapon = ordered[i];
		float score = scores[i];
		for (j = i; j > 0 && scores[j - 1] < score; j--) {
			ordered[j] = ordered[j - 1];
			scores[j] = scores[j - 1];
		}
		ordered[j] = weapon;
		scores[j] = score;
	}
	return count;
}

static void Bot_SelectCombatMove (edict_t *ent, edict_t *target, int skill)
{
	static const bot_policy_move_kind_t kinds[] = {
		BOT_MOVE_HOLD, BOT_MOVE_FORWARD, BOT_MOVE_BACK, BOT_MOVE_LEFT, BOT_MOVE_RIGHT
	};
	static const float offsets[] = {0, 0, 180, -90, 90};
	zgcl_t *zc = &ent->client->zc;
	vec3_t to_target, route_point;
	float base_yaw, ideal = 420.0f, best_score = BOT_POLICY_REJECTED;
	int weapon = zc->selected_weapon ? zc->selected_weapon : Get_KindWeapon(ent->client->pers.weapon);
	int best = BOT_MOVE_HOLD, i;

	if (!ent->groundentity || ent->waterlevel >= 2 || level.time < zc->combat_move_time)
		return;
	if (weapon == WEAP_SHOTGUN || weapon == WEAP_SUPERSHOTGUN)
		ideal = 190.0f;
	else if (weapon == WEAP_RAILGUN)
		ideal = 800.0f;
	else if (weapon == WEAP_ROCKETLAUNCHER || weapon == WEAP_GRENADELAUNCHER)
		ideal = 500.0f;
	VectorSubtract(target->s.origin, ent->s.origin, to_target);
	base_yaw = Get_yaw(to_target);
	VectorClear(route_point);
	if (zc->route_trace && zc->routeindex + 1 < CurrentIndex)
		Get_RouteOrigin(zc->routeindex + 1, route_point);

	for (i = 0; i < 5; i++) {
		bot_policy_move_t move = {0};
		vec3_t angles = {0, base_yaw + offsets[i], 0};
		vec3_t forward, destination, down, delta;
		trace_t travel, floor, sight;
		float score, old_route_distance = 0, new_route_distance = 0;
		int client_index;

		move.kind = kinds[i];
		if (kinds[i] == BOT_MOVE_HOLD)
			VectorCopy(ent->s.origin, destination);
		else {
			AngleVectors(angles, forward, NULL, NULL);
			VectorMA(ent->s.origin, 72.0f, forward, destination);
		}
		travel = gi.trace(ent->s.origin, ent->mins, ent->maxs, destination, ent, MASK_BOTSOLID);
		VectorCopy(travel.endpos, destination);
		VectorCopy(destination, down);
		down[2] -= 72.0f;
		floor = gi.trace(destination, ent->mins, ent->maxs, down, ent, MASK_BOTGROUND);
		move.hazard = kinds[i] != BOT_MOVE_HOLD &&
			(travel.fraction < 0.65f || floor.fraction == 1.0f ||
			 (gi.pointcontents(floor.endpos) & (CONTENTS_LAVA | CONTENTS_SLIME)));
		VectorSubtract(target->s.origin, destination, delta);
		move.range_error = fabsf(VectorLength(delta) - ideal);
		sight = gi.trace(destination, NULL, NULL, target->s.origin, ent, MASK_SHOT);
		move.cover = sight.fraction < 1.0f && sight.ent != target;
		move.exposed = !move.cover && target->client->weaponstate == WEAPON_FIRING;
		for (client_index = 1; client_index <= (int)maxclients->value; client_index++) {
			edict_t *mate = &g_edicts[client_index];
			if (!mate->inuse || !mate->client || mate == ent || !Bot_TeamCombat() ||
			    !OnSameTeam(ent, mate))
				continue;
			VectorSubtract(mate->s.origin, destination, delta);
			if (VectorLength(delta) < 72.0f) {
				move.teammate_crowded = true;
				break;
			}
		}
		if (zc->route_trace && zc->routeindex + 1 < CurrentIndex) {
			VectorSubtract(route_point, ent->s.origin, delta);
			old_route_distance = VectorLength(delta);
			VectorSubtract(route_point, destination, delta);
			new_route_distance = VectorLength(delta);
			move.objective_progress = old_route_distance - new_route_distance;
		}
		score = BotPolicy_MoveScore(&move) + (random() - 0.5f) * (9 - skill) * 18.0f;
		if (score > best_score) {
			best_score = score;
			best = kinds[i];
		}
	}

	zc->combat_move_choice = best;
	zc->combat_move_time = level.time + 0.18f + (9 - skill) * 0.045f + random() * 0.18f;
	if (best == BOT_MOVE_HOLD) {
		ent->moveinfo.speed = 0;
		trace_priority = TRP_MOVEKEEP;
		return;
	}
	zc->moveyaw = base_yaw + (best == BOT_MOVE_BACK ? 180.0f :
	              best == BOT_MOVE_LEFT ? -90.0f : best == BOT_MOVE_RIGHT ? 90.0f : 0);
	if (zc->moveyaw > 180) zc->moveyaw -= 360;
	if (zc->moveyaw < -180) zc->moveyaw += 360;
	trace_priority = TRP_MOVEKEEP;
}

//return weapon
void Combat_LevelX (edict_t *ent, int foundedenemy, int enewep, float aim, float distance, int skill)
{
	gclient_t *client;
	zgcl_t *zc;
	edict_t *target;
	int mywep, k;
	vec3_t v;

	client = ent->client;
	zc = &client->zc;
	target = zc->first_target;

	//-----------------------------------------------------------------------
	//ステータスを反映
	//-----------------------------------------------------------------------
	k = false;
	//予測========================
	if (zc->battlemode & FIRE_ESTIMATE) {
		mywep = Get_KindWeapon(client->pers.weapon);
		//Phalanx
		if (distance > 100 || mywep == WEAP_PHALANX) {
			if (B_UsePhalanx(ent, target, enewep, aim, distance, skill))
				k = true;
		}

		//Rocket
		if (distance > 100 || mywep == WEAP_ROCKETLAUNCHER) {
			if (B_UseRocket(ent, target, enewep, aim, distance, skill))
				k = true;
		}

		//Boomer
		if (distance < 1200) {
			if (B_UseBoomer(ent, target, enewep, aim, distance, skill))
				k = true;
		}
		//Grenade Launcher
		if (distance > 100 && distance < 400 && (target->s.origin[2] - ent->s.origin[2]) < 200) {
			if (B_UseGrenadeLauncher(ent, target, enewep, aim, distance, skill))
				k = true;
		}
		//Hand Grenade
		if (distance < 1200) {
			if (B_UseHandGrenade(ent, target, enewep, aim, distance, skill))
				k = true;
		}
		VectorSubtract(zc->vtemp, ent->s.origin, v);
		ent->s.angles[YAW] = Get_yaw(v);
		ent->s.angles[PITCH] = Get_pitch(v);
		if (k)
			trace_priority = TRP_ALLKEEP;
		else
			trace_priority = TRP_ANGLEKEEP;
		return;
	}
	VectorSubtract(target->s.origin, ent->s.origin, v);
	ent->s.angles[YAW] = Get_yaw(v);
	ent->s.angles[PITCH] = Get_pitch(v);
	trace_priority = TRP_ANGLEKEEP;
}

//return weapon
void Combat_Level0 (edict_t *ent, int foundedenemy, int enewep, float aim, float distance, int skill)
{
	float f;
	gclient_t *client;
	zgcl_t *zc;

	edict_t *target;
	int mywep, i, j, k;
	int utility_order[2] = {0, 1};
	int ranked_weapons[16];
	float ranked_scores[16];
	vec3_t v, vv, v1, v2;

	trace_t rs_trace;

	client = ent->client;
	zc = &client->zc;
	target = zc->first_target;


	//-----------------------------------------------------------------------
	//ステータスを反映
	//-----------------------------------------------------------------------
	//チキンは狙いがキツイ==============
	if (zc->battlemode == FIRE_CHIKEN)
		aim *= 0.7;
	//左右に回避========================
	if (zc->battlemode & FIRE_SHIFT) {
		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);

		if (--zc->battlesubcnt > 0) {
			if (ent->groundentity) {
				if (zc->battlemode & FIRE_SHIFT_R) {
					zc->moveyaw = ent->s.angles[YAW] + 90;
					if (zc->moveyaw > 180)
						zc->moveyaw -= 360;
				} else {
					zc->moveyaw = ent->s.angles[YAW] - 90;
					if (zc->moveyaw < -180)
						zc->moveyaw += 360;
				}
				trace_priority = TRP_MOVEKEEP; //後退処理
			}
		} else {
			zc->battlemode &= ~FIRE_SHIFT;
		}
	}

	//dodge=============================
	if (Bot[ent->client->zc.botindex].param[BOP_DODGE] && ent->groundentity && !ent->waterlevel) {
		AngleVectors(target->client->v_angle, v, NULL, NULL);
		VectorScale(v, 300, v);

		VectorSet(vv, 0, 0, target->viewheight - 8);
		VectorAdd(target->s.origin, vv, vv);
		VectorAdd(vv, v, v);

		VectorSet(v1, -4, -4, -4);
		VectorSet(v2, 4, 4, 4);
		rs_trace = gi.trace(vv, v1, v2, v, target, MASK_SHOT);

		if (rs_trace.ent == ent) {
			bool enemyFiring = (target->client->weaponstate == WEAPON_FIRING);
			float dodgeChance = Bot[ent->client->zc.botindex].param[BOP_DODGE] / 10.0;

			if (enemyFiring)
				dodgeChance += 0.3;

			if (random() < dodgeChance) {
				vec3_t toTarget, strafeDir;
				float randDodge = random();

				VectorSubtract(target->s.origin, ent->s.origin, toTarget);
				toTarget[2] = 0;
				VectorNormalize(toTarget);

				CrossProduct(toTarget, vec3_origin, strafeDir);
				if (strafeDir[0] == 0 && strafeDir[1] == 0) {
					strafeDir[0] = -toTarget[1];
					strafeDir[1] = toTarget[0];
				}

				if (randDodge < 0.25 && rs_trace.endpos[2] > (ent->s.origin[2] + 4)) {
					client->ps.pmove.pm_flags |= PMF_DUCKED;
					zc->battleduckcnt = 2 + 6 * random();
				} else if (randDodge < 0.5 && rs_trace.endpos[2] < (ent->s.origin[2] + JumpMax - 24)) {
					if (zc->route_trace) {
						if (Bot_Fall(ent, ent->s.origin, 0))
							trace_priority = TRP_MOVEKEEP;
					} else {
						ent->moveinfo.speed = 0.5;
						ent->velocity[2] += VEL_BOT_JUMP;
						gi.sound(ent, CHAN_VOICE, gi.soundindex("*jump1.wav"), 1, ATTN_NORM, 0);
						PlayerNoise(ent, ent->s.origin, PNOISE_SELF);
						Set_BotAnim(ent, ANIM_JUMP, FRAME_jump1 - 1, FRAME_jump6);
					}
				} else if (randDodge < 0.75) {
					float strafeYaw;
					if (random() < 0.5) {
						strafeYaw = ent->s.angles[YAW] + 90;
					} else {
						strafeYaw = ent->s.angles[YAW] - 90;
					}
					if (strafeYaw > 180)
						strafeYaw -= 360;
					else if (strafeYaw < -180)
						strafeYaw += 360;
					zc->moveyaw = strafeYaw;
					zc->battlemode |= FIRE_SHIFT;
					zc->battlesubcnt = 4 + (int)(8 * random());
					trace_priority = TRP_MOVEKEEP;
				} else {
					zc->moveyaw = ent->s.angles[YAW] + 180;
					if (zc->moveyaw > 180)
						zc->moveyaw -= 360;
					else if (zc->moveyaw < -180)
						zc->moveyaw += 360;
					trace_priority = TRP_MOVEKEEP;
				}
			}
		}
	}
	//dodge (water) =============================
	else if (Bot[ent->client->zc.botindex].param[BOP_DODGE] && ent->waterlevel >= 2) // waist-deep or fully submerged only
	{
		// Same shot trace as land dodge: project enemy's aim 300 units forward
		AngleVectors(target->client->v_angle, v, NULL, NULL);
		VectorScale(v, 300, v);

		VectorSet(vv, 0, 0, target->viewheight - 8);
		VectorAdd(target->s.origin, vv, vv);
		VectorAdd(vv, v, v);

		VectorSet(v1, -4, -4, -4);
		VectorSet(v2, 4, 4, 4);
		rs_trace = gi.trace(vv, v1, v2, v, target, MASK_SHOT);

		if (rs_trace.ent == ent) {
			// --- Horizontal component: strafe perpendicular to current facing ---
			zc->moveyaw = ent->s.angles[YAW] + (random() < 0.5f ? 90.0f : -90.0f);
			if (zc->moveyaw > 180.0f)
				zc->moveyaw -= 360.0f;
			else if (zc->moveyaw < -180.0f)
				zc->moveyaw += 360.0f;

			// --- Vertical component: swim away from the shot's impact height ---
			if (rs_trace.endpos[2] > (ent->s.origin[2] + 4)) {
				// Shot hits upper body: swim down
				if (ent->velocity[2] > -100)
					ent->velocity[2] -= 150;
			} else if (zc->waterstate == WAS_IN) {
				// Shot hits lower body and bot is fully submerged: swim up
				if (ent->velocity[2] < 0)
					ent->velocity[2] = 0;
				if (ent->velocity[2] < 100)
					ent->velocity[2] += 150;
			}

			trace_priority = TRP_MOVEKEEP;
		}
	}
	//無視して走る========================
	if (zc->battlemode & FIRE_IGNORE) {
		if (--zc->battlecount > 0) {
			if (zc->first_target != zc->last_target) {
				zc->battlemode = 0;
			} else
				return;
		}
		zc->battlemode = 0;
	}

	//立ち止まって撃つ準備========================
	if (zc->battlemode & FIRE_PRESTAYFIRE) {
		if (--zc->battlecount > 0) {
			mywep = Get_KindWeapon(client->pers.weapon);
			Get_AimAngle(ent, aim, distance, mywep);
			if (target->client->weaponstate == WEAPON_FIRING && ent->groundentity)
				ent->client->ps.pmove.pm_flags |= PMF_DUCKED;
			trace_priority = TRP_ALLKEEP; //動かない
			return;
		}
		if (!(zc->battlemode & FIRE_SHIFT))
			zc->battlemode = FIRE_STAYFIRE; //モード遷移
		zc->battlecount = 5 + (int)(20 * random());
	}

	//立ち止まって撃つ========================
	if (zc->battlemode & FIRE_STAYFIRE) {
		if (--zc->battlecount > 0) {
			mywep = Get_KindWeapon(client->pers.weapon);
			if (1 /*mywep == WEAP_BFG*/)
				CanUsewep(ent, WEAP_BFG);
			aim *= 0.95;
			Get_AimAngle(ent, aim, distance, mywep);
			if (target->client->weaponstate == WEAPON_FIRING && ent->groundentity) {
				if (mywep == WEAP_BFG) {
					if (target->s.origin[2] > ent->s.origin[2])
						client->ps.pmove.pm_flags |= PMF_DUCKED;
				} else
					client->ps.pmove.pm_flags |= PMF_DUCKED;
			}
			if (!(zc->battlemode & FIRE_SHIFT))
				trace_priority = TRP_ALLKEEP; //動かない
			if (Bot_traceS(ent, target) || mywep == WEAP_BFG || mywep == WEAP_GRENADELAUNCHER)
				client->buttons |= BUTTON_ATTACK;
			return;
		}
		zc->battlemode = 0;
	}

	//FIRE_RUSH	つっこむ========================
	if (zc->battlemode & FIRE_RUSH) {
		if (--zc->battlecount > 0) {
			mywep = Get_KindWeapon(client->pers.weapon);
			if (1 /*mywep == WEAP_BFG*/)
				CanUsewep(ent, WEAP_BFG);
			aim *= 0.95;
			Get_AimAngle(ent, aim, distance, mywep);
			if (target->client->weaponstate == WEAPON_FIRING && ent->groundentity) {
				if (mywep == WEAP_BFG) {
					if (target->s.origin[2] > ent->s.origin[2])
						client->ps.pmove.pm_flags |= PMF_DUCKED;
				} else
					client->ps.pmove.pm_flags |= PMF_DUCKED;
			}
			trace_priority = TRP_MOVEKEEP; //後退処理
			zc->moveyaw = ent->s.angles[YAW];

			if (Bot_traceS(ent, target))
				client->buttons |= BUTTON_ATTACK;
			return;
		}
		zc->battlemode = 0;
	}

	//後退ファイア(爆発回避)========================
	if (zc->battlemode & FIRE_EXPAVOID) {
		if (--zc->battlecount > 0) {
			mywep = Get_KindWeapon(client->pers.weapon);
			if (1 /*mywep == WEAP_BFG*/)
				CanUsewep(ent, WEAP_BFG);
			aim *= 0.95;
			Get_AimAngle(ent, aim, distance, mywep);
			if (target->client->weaponstate == WEAPON_FIRING && ent->groundentity) {
				if (mywep == WEAP_BFG) {
					if (target->s.origin[2] > ent->s.origin[2])
						client->ps.pmove.pm_flags |= PMF_DUCKED;
				} else
					client->ps.pmove.pm_flags |= PMF_DUCKED;
			}
			trace_priority = TRP_MOVEKEEP; //後退処理
			zc->moveyaw = ent->s.angles[YAW] + 180;
			if (zc->moveyaw > 180)
				zc->moveyaw -= 360;

			if (Bot_traceS(ent, target) || mywep == WEAP_BFG || mywep == WEAP_GRENADELAUNCHER)
				client->buttons |= BUTTON_ATTACK;
			return;
		}
		zc->battlemode = 0;
	}
	//ＢＦＧファイア(爆発回避)========================
	if (zc->battlemode & FIRE_BFG) {
		if (--zc->battlecount > 0) {
			mywep = Get_KindWeapon(client->pers.weapon);
			if (1 /*mywep == WEAP_BFG*/)
				CanUsewep(ent, WEAP_BFG);
			aim *= 0.95;
			Get_AimAngle(ent, aim, distance, mywep);
			if (target->client->weaponstate == WEAPON_FIRING && ent->groundentity) {
				if (1 /*mywep == WEAP_BFG*/) {
					if (target->s.origin[2] > ent->s.origin[2])
						client->ps.pmove.pm_flags |= PMF_DUCKED;
				} else
					client->ps.pmove.pm_flags |= PMF_DUCKED;
			}
			trace_priority = TRP_ANGLEKEEP; //後退処理

			if (Bot_traceS(ent, target) || mywep == WEAP_BFG || mywep == WEAP_GRENADELAUNCHER)
				client->buttons |= BUTTON_ATTACK;
			return;
		}
		zc->battlemode = 0;
	}

	//撃って避難========================
	if (zc->battlemode & FIRE_REFUGE) {
		if (--zc->battlecount > 0) {
			mywep = Get_KindWeapon(client->pers.weapon);
			//CanUsewep(ent,WEAP_BFG);
			aim *= 0.95;
			Get_AimAngle(ent, aim, distance, mywep);
			if (target->client->weaponstate == WEAPON_FIRING && ent->groundentity) {
				if (mywep == WEAP_BFG) {
					if (target->s.origin[2] > ent->s.origin[2])
						client->ps.pmove.pm_flags |= PMF_DUCKED;
				} else
					client->ps.pmove.pm_flags |= PMF_DUCKED;
			}
			trace_priority = TRP_ANGLEKEEP; //動かない
							//			trace_priority = TRP_ALLKEEP;	//動かない
			if (Bot_traceS(ent, target) || mywep == WEAP_BFG || mywep == WEAP_GRENADELAUNCHER)
				client->buttons |= BUTTON_ATTACK;
			return;
		}
		zc->battlemode = 0;
		zc->routeindex -= 2;
	}

	if (!(client->zc.zccmbstt & CTS_ENEM_NSEE) && (zc->zcstate & STS_WAITSMASK2) && (target->s.origin[2] - ent->s.origin[2]) < -300) {
		if (CanUsewep(ent, WEAP_GRENADELAUNCHER)) {
			mywep = Get_KindWeapon(client->pers.weapon);
			Get_AimAngle(ent, aim, distance, mywep);
			if ((target->client->weaponstate == WEAPON_FIRING && ent->groundentity) || (zc->zcstate & STS_WAITSMASK2))
				ent->client->ps.pmove.pm_flags |= PMF_DUCKED;
			client->buttons |= BUTTON_ATTACK;
			trace_priority = TRP_ANGLEKEEP;
			return;
		}
		if (CanUsewep(ent, WEAP_GRENADES)) {
			mywep = Get_KindWeapon(client->pers.weapon);
			Get_AimAngle(ent, aim, distance, mywep);
			if (target->client->weaponstate == WEAPON_FIRING && ent->groundentity)
				ent->client->ps.pmove.pm_flags |= PMF_DUCKED;
			if (ent->client->weaponstate == WEAPON_READY)
				client->buttons |= BUTTON_ATTACK;
			trace_priority = TRP_ANGLEKEEP;
			return;
		}
	}

	//-----------------------------------------------------------------------
	//特殊ファイアリング
	//-----------------------------------------------------------------------
	mywep = Get_KindWeapon(client->pers.weapon);

	//左右回避セット========================
	if (!(zc->battlemode & FIRE_SHIFT) && skill > (random() * skill) && (30 * random()) < Bot[zc->botindex].param[BOP_OFFENCE]) {
		k = false;
		if (zc->route_trace && enewep != WEAP_RAILGUN) {
			for (i = zc->routeindex; i < (zc->routeindex + 10); i++) {
				if (i >= CurrentIndex)
					break;
				if (Route[i].state == GRS_ITEMS) {
					if (Route[i].ent->solid == SOLID_TRIGGER) {
						k = true;
						break;
					}
				}
			}
		}
		if (!k) {
			Get_AimAngle(ent, aim, distance, mywep);
			f = target->s.angles[YAW] - ent->s.angles[YAW];

			if (f > 180) {
				f = -(360 - f);
			}
			if (f < -180) {
				f = -(f + 360);
			}

			bool enemyFiring = (target->client->weaponstate == WEAPON_FIRING);
			float triggerAngle = 150;
			float dodgeChance = Bot[zc->botindex].param[BOP_DODGE] / 10.0;

			if (enemyFiring) {
				triggerAngle = 130;
				dodgeChance += 0.2;
			}

			if (skill >= 7)
				triggerAngle -= 15;
			if (skill >= 5)
				triggerAngle -= 10;

			if (random() < dodgeChance) {
				if (f <= -triggerAngle) {
					zc->battlemode |= FIRE_SHIFT_L;
					zc->battlesubcnt = 4 + (int)(12 * random());
				} else if (f >= triggerAngle) {
					zc->battlemode |= FIRE_SHIFT_R;
					zc->battlesubcnt = 4 + (int)(12 * random());
				}
			}
		}
	}

	//敵がペンタをとっている========================
	if ((FFlg[skill] & FIRE_AVOIDINV) && target->client->invincible_framenum > level.framenum) {
		//		mywep = Get_KindWeapon(client->pers.weapon);
		Get_AimAngle(ent, aim, distance, mywep);
		trace_priority = TRP_MOVEKEEP; //後退処理
		zc->moveyaw = ent->s.angles[YAW] + 180;
		if (zc->moveyaw > 180)
			zc->moveyaw -= 360;
		return;
	}
	//Quad時の処理=================================
	if ((FFlg[skill] & FIRE_QUADUSE) && (ent->client->quad_framenum > level.framenum) && distance < 300) {
		j = false;
		if (enewep < WEAP_MACHINEGUN || enewep == WEAP_GRENADES)
			j = true;

		//Hyper Blaster
		if (CanUsewep(ent, WEAP_HYPERBLASTER)) {
			mywep = Get_KindWeapon(client->pers.weapon);
			Get_AimAngle(ent, aim, distance, mywep);
			client->buttons |= BUTTON_ATTACK;
			trace_priority = TRP_ANGLEKEEP;
			if (j) {
				zc->battlemode |= FIRE_RUSH;
				zc->battlecount = 8 + (int)(10 * random());
			}
			return;
		}
		//Chain Gun
		if (CanUsewep(ent, WEAP_CHAINGUN)) {
			mywep = Get_KindWeapon(client->pers.weapon);
			Get_AimAngle(ent, aim, distance, mywep);
			client->buttons |= BUTTON_ATTACK;
			trace_priority = TRP_ANGLEKEEP;
			if (j) {
				zc->battlemode |= FIRE_RUSH;
				zc->battlecount = 8 + (int)(10 * random());
			}
			return;
		}
		//Machine Gun
		if (CanUsewep(ent, WEAP_MACHINEGUN)) {
			mywep = Get_KindWeapon(client->pers.weapon);
			Get_AimAngle(ent, aim, distance, mywep);
			client->buttons |= BUTTON_ATTACK;
			trace_priority = TRP_ANGLEKEEP;
			if (j) {
				zc->battlemode |= FIRE_RUSH;
				zc->battlecount = 8 + (int)(10 * random());
			}
			return;
		}
		//S-Shotgun
		if (CanUsewep(ent, WEAP_SUPERSHOTGUN)) {
			mywep = Get_KindWeapon(client->pers.weapon);
			Get_AimAngle(ent, aim, distance, mywep);
			client->buttons |= BUTTON_ATTACK;
			trace_priority = TRP_ANGLEKEEP;
			if (j) {
				zc->battlemode |= FIRE_RUSH;
				zc->battlecount = 8 + (int)(10 * random());
			}
			return;
		}
	}
	//撃って逃げる処理=================================
	if ((FFlg[skill] & FIRE_REFUGE) && zc->battlemode == 0 && zc->route_trace && zc->routeindex > 1) {
		j = false;
		if (enewep >= WEAP_CHAINGUN && enewep != WEAP_GRENADES)
			j = true;


		Get_RouteOrigin(zc->routeindex - 2, v);

		if (fabs(v[2] - ent->s.origin[2]) < JumpMax && j) {
			mywep = Get_KindWeapon(client->pers.weapon);
			if (mywep == WEAP_GRENADELAUNCHER || mywep == WEAP_ROCKETLAUNCHER || mywep == WEAP_PHALANX) {
				zc->battlemode |= FIRE_REFUGE; //モード遷移
				zc->battlecount = 8 + (int)(10 * random());
				trace_priority = TRP_ALLKEEP;
				return;
			}
		}
	}
	//トレース中以外のときにグルグルを防ぐ=================================
	if (!zc->route_trace && distance < 100) {
		zc->battlemode |= FIRE_EXPAVOID; //モード遷移
		zc->battlecount = 4 + (int)(8 * random());
		trace_priority = TRP_ALLKEEP;
	}


	//-----------------------------------------------------------------------
	//プライオリティ
	//-----------------------------------------------------------------------

	if (zc->threat_level > 0.7f && ent->health > 50) {
		if (distance > 200 && distance < 600) {
			if (B_UseRocket(ent, target, enewep, aim * 0.8, distance, skill))
				goto FIRED;
		}
	}

	if (zc->nearby_enemies >= 3 && distance < 500) {
		if (B_UseBfg(ent, target, enewep, aim * 0.7, distance, skill))
			goto FIRED;
		if (distance < 400 && (target->s.origin[2] - ent->s.origin[2]) < 100) {
			if (B_UseGrenadeLauncher(ent, target, enewep, aim * 0.8, distance, skill))
				goto FIRED;
		}
	}

	if (ent->health < 40 && distance < 300) {
		if (B_UseSuperShotgun(ent, target, enewep, aim * 0.7, distance, skill))
			goto FIRED;
		if (B_UseShotgun(ent, target, enewep, aim * 0.7, distance, skill))
			goto FIRED;
	}

	if (target->s.origin[2] > ent->s.origin[2] + 100 && distance < 800) {
		if (B_UseRocket(ent, target, enewep, aim * 0.85, distance, skill))
			goto FIRED;
	}

	/* Reserve the BFG for durable targets or groups instead of allowing its
	 * position in the legacy weapon list to dominate every mid-range fight. */
	if (distance > 300 && (zc->nearby_enemies >= 2 || target->health > 120)) {
		if (B_UseBfg(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}
	if (Bot_PreferredWeaponScore(ent, Bot[zc->botindex].param[BOP_PRIWEP + 1], distance,
	                             zc->nearby_enemies, target->health, 1) >
	    Bot_PreferredWeaponScore(ent, Bot[zc->botindex].param[BOP_PRIWEP], distance,
	                             zc->nearby_enemies, target->health, 0)) {
		utility_order[0] = 1;
		utility_order[1] = 0;
	}

	for (i = 0; i < 3; i++) {
		mywep = Get_KindWeapon(client->pers.weapon);

		if (i == 0 && zc->secwep_selected)
			continue;

		//try to select secondary weapon
		if (i == 0 && zc->secwep_selected)
			i = 1;
		else if (i == 0 && foundedenemy < 3 && target->health < 50 && !zc->secwep_selected && ent->health >= 50) {
			if ((9 * random()) < Bot[zc->botindex].param[BOP_COMBATSKILL]) {
				zc->secwep_selected = 2;
				i = 1;
			}
		}

		if (i == 2) {
			if (zc->secwep_selected) {
				zc->secwep_selected = 0;
				j = 0;
			} else
				break;
		} else
			j = utility_order[i];

		if (distance > 300 && (zc->nearby_enemies >= 2 || target->health > 120) &&
		    (mywep == WEAP_BFG || random() < 0.5)) {
			if (B_UseBfg(ent, target, enewep, aim, distance, skill))
				goto FIRED;
		}

		switch (Bot[zc->botindex].param[BOP_PRIWEP + j]) {
			case WEAP_BFG:
				if (distance > 100) {
					if (B_UseBfg(ent, target, enewep, aim, distance, skill))
						goto FIRED;
				}
				break;
			case WEAP_HYPERBLASTER:
				if (distance < 1200) {
					if (B_UseHyperBlaster(ent, target, enewep, aim, distance, skill))
						goto FIRED;
				}
				break;
			case WEAP_PHALANX:
				if (distance > 100 && distance < 1200 /*|| mywep == WEAP_PHALANX*/) {
					if (B_UsePhalanx(ent, target, enewep, aim, distance, skill))
						goto FIRED;
				}
				break;
			case WEAP_ROCKETLAUNCHER:
				if (distance > 100 && distance < 1200 /*|| mywep == WEAP_ROCKETLAUNCHER*/) {
					if (B_UseRocket(ent, target, enewep, aim, distance, skill))
						goto FIRED;
				}
				break;
			case WEAP_BOOMER:
				if (distance < 1200) {
					if (B_UseBoomer(ent, target, enewep, aim, distance, skill))
						goto FIRED;
				}
				break;
			case WEAP_RAILGUN:
				if (distance < 1200) {
					if (B_UseRailgun(ent, target, enewep, aim, distance, skill))
						goto FIRED;
				}
				break;
			case WEAP_GRENADELAUNCHER:
				if (distance > 100 && distance < 400 && (target->s.origin[2] - ent->s.origin[2]) < 200) {
					if (B_UseGrenadeLauncher(ent, target, enewep, aim, distance, skill))
						goto FIRED;
				}
				break;
			case WEAP_CHAINGUN:
			case WEAP_MACHINEGUN:
				if (distance < 1200) {
					if (B_UseChainGun(ent, target, enewep, aim, distance, skill))
						goto FIRED;
				}
				if (distance < 1200) {
					if (B_UseMachineGun(ent, target, enewep, aim, distance, skill))
						goto FIRED;
				}
				break;
			case WEAP_SUPERSHOTGUN:
			case WEAP_SHOTGUN:
				if (distance < 1200) {
					if (B_UseSuperShotgun(ent, target, enewep, aim, distance, skill))
						goto FIRED;
				}
				if (distance < 1200) {
					if (B_UseShotgun(ent, target, enewep, aim, distance, skill))
						goto FIRED;
				}
			case WEAP_GRENADES:
				if (distance < 1200) {
					if (B_UseHandGrenade(ent, target, enewep, aim, distance, skill))
						goto FIRED;
				}
				break;
			default:
				break;
		}
	}


	//-----------------------------------------------------------------------
	//通常ファイアリング
	//-----------------------------------------------------------------------
	 zc->secwep_selected = 0;
	{
		int ranked_count = Bot_RankWeapons(ent, target, distance, ranked_weapons,
		                                   ranked_scores, 16);
		for (i = 0; i < ranked_count; i++) {
			bot_weapon_action_t action;
			if (ranked_scores[i] <= BOT_POLICY_REJECTED)
				continue;
			zc->selected_weapon = ranked_weapons[i];
			zc->selected_weapon_score = ranked_scores[i];
			action = Bot_FireUtilityWeapon(ent, target, ranked_weapons[i], enewep,
			                               aim, distance, skill);
			zc->weapon_action = action;
			if (action == BOT_WEAPON_FIRED)
				goto FIRED;
			if (action == BOT_WEAPON_SWITCHING)
				return;
		}
	}

	/* Range-aware fallback order. Per-bot primary/secondary preferences above
	 * still win when suitable, while this path avoids selecting by weapon enum. */
	if (distance >= 600) {
		if (B_UseRailgun(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}
	if (distance < 180) {
		if (B_UseSuperShotgun(ent, target, enewep, aim, distance, skill))
			goto FIRED;
		if (B_UseChainGun(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}
	//BFG
	if (distance > 300 && (zc->nearby_enemies >= 2 || target->health > 120)) {
		if (B_UseBfg(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}

	//Hyper Blaster
	if (distance < 1200) {
		if (B_UseHyperBlaster(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}

	//Phalanx
	if ((distance > 100 && distance < 1200) /*|| mywep == WEAP_PHALANX*/) {
		if (B_UsePhalanx(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}

	//Rocket
	if ((distance > 100 && distance < 1200) /*|| mywep == WEAP_ROCKETLAUNCHER*/) {
		if (B_UseRocket(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}

	//Boomer
	if (distance < 1200) {
		if (B_UseBoomer(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}

	//Railgun
	if (distance < 1200) {
		if (B_UseRailgun(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}

	//Grenade Launcher
	if (distance > 100 && distance < 400 && (target->s.origin[2] - ent->s.origin[2]) < 200) {
		if (B_UseGrenadeLauncher(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}
	//Chain Gun
	if (distance < 1200) {
		if (B_UseChainGun(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}
	//Machine Gun
	if (distance < 1200) {
		if (B_UseMachineGun(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}
	//S-Shotgun
	if (distance < 1200) {
		if (B_UseSuperShotgun(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}

	if ((FFlg[skill] & FIRE_IGNORE) && distance > 400 && ent->groundentity && !(zc->zcstate & STS_WAITSMASK)) {
		zc->battlemode = FIRE_IGNORE;
		zc->battlecount = 5 + (int)(10 * random());
	}

	//Shotgun
	if (distance < 1200) {
		if (B_UseShotgun(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}
	//Hand Grenade
	if (distance < 400) {
		if (B_UseHandGrenade(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}
	//Trap
	if (distance < 400) {
		if (B_UseTrap(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}
	//Blaster
	if (distance < 1200) {
		if (B_UseBlaster(ent, target, enewep, aim, distance, skill))
			goto FIRED;
	}

	VectorSubtract(zc->vtemp, ent->s.origin, v);
	ent->s.angles[YAW] = Get_yaw(v);
	ent->s.angles[PITCH] = Get_pitch(v);
	trace_priority = TRP_ANGLEKEEP;
	return;

FIRED:
	if (zc->secwep_selected == 2)
		zc->secwep_selected = 1;
	Bot_SelectCombatMove(ent, target, skill);

	if (instagib && instagib->value && Bot[zc->botindex].param[BOP_DODGE] &&
	    ent->groundentity && !ent->waterlevel && trace_priority < TRP_MOVEKEEP) {
		if (level.time >= zc->combat_move_time) {
			float hold = 0.22f + random() * 0.38f;

			zc->combat_move_time = level.time + hold;
			if (random() < 0.12f + (9 - skill) * 0.015f)
				zc->combat_move_dir = 0;
			else if (!zc->combat_move_dir || random() < 0.65f)
				zc->combat_move_dir = (random() < 0.5f) ? -1 : 1;
			else
				zc->combat_move_dir = -zc->combat_move_dir;
		}

		if (zc->combat_move_dir) {
			zc->moveyaw = ent->s.angles[YAW] + 90.0f * zc->combat_move_dir;
			if (zc->moveyaw > 180.0f)
				zc->moveyaw -= 360.0f;
			else if (zc->moveyaw < -180.0f)
				zc->moveyaw += 360.0f;
			trace_priority = TRP_MOVEKEEP;

			if (skill >= 6 && random() < 0.025f && !(client->ps.pmove.pm_flags & PMF_DUCKED))
				ent->velocity[2] += VEL_BOT_JUMP;
		} else {
			ent->moveinfo.speed = 0;
		}
	} else if (Bot[zc->botindex].param[BOP_DODGE] && ent->groundentity && !ent->waterlevel && trace_priority < TRP_MOVEKEEP && skill >= 5) {
		// fbattlecount encodes direction + next-switch time:
		//   > 0  → strafe right, switch when level.time >= fbattlecount
		//   < 0  → strafe left,  switch when level.time >= -fbattlecount
		//   == 0 → uninitialized
		if (zc->fbattlecount == 0.0f || level.time >= fabsf(zc->fbattlecount)) {
			//float interval = 0.2f + random() * 0.4f;  // 0.2–0.6s random interval
			//float interval = 0.25f + random() * 0.20f;  // 0.25–0.45s random interval
			float interval = 0.2f + random() * 0.3f;
			float next = level.time + interval;

			if (zc->fbattlecount == 0.0f)
				zc->fbattlecount = (random() < 0.5f) ? next : -next; // random initial dir
			else
				zc->fbattlecount = (zc->fbattlecount > 0) ? -next : next; // flip direction
		}

		float yaw_offset = (zc->fbattlecount > 0) ? 90.0f : -90.0f;
		zc->moveyaw = ent->s.angles[YAW] + yaw_offset;

		if (zc->moveyaw > 180.0f)
			zc->moveyaw -= 360.0f;
		else if (zc->moveyaw < -180.0f)
			zc->moveyaw += 360.0f;

		// "Plant and shoot" window: ~10% of time, per-bot offset.
		{
			float still_phase = level.time * 0.25f + zc->botindex * 0.11f;
			if ((still_phase - (int)still_phase) < 0.1f)
				ent->moveinfo.speed = 0.5f + random() * 0.5f; //moveinfo.speed is always implicitly full speed. Setting it to 0.5f + random() * 0.5f at each direction switch would make the bot's movement less uniform
		}

		trace_priority = TRP_MOVEKEEP;
	} else {
		zc->fbattlecount = 0.0f; // reset so next combat entry reinitializes cleanly
	}

	//チキンやろう========================
	if (zc->battlemode == FIRE_CHIKEN) {
		if (--zc->battlesubcnt > 0 && ent->groundentity && ent->waterlevel < 2) {
			f = target->s.angles[YAW] - ent->s.angles[YAW];

			if (f > 180) {
				f = -(360 - f);
			}
			if (f < -180) {
				f = -(f + 360);
			}
			if (fabs(f) >= 150) {
				zc->battlemode = 0;
			} else {
				if (client->weaponstate != WEAPON_READY && target->s.origin[2] < ent->s.origin[2]) {
					if (mywep == WEAP_ROCKETLAUNCHER || mywep == WEAP_PHALANX || mywep == WEAP_GRENADELAUNCHER || mywep == WEAP_RAILGUN)
						client->ps.pmove.pm_flags |= PMF_DUCKED;
					else if (Bot[zc->botindex].param[BOP_COMBATSKILL] >= 7) {
						if (mywep == WEAP_SHOTGUN || mywep == WEAP_SUPERSHOTGUN || mywep == WEAP_BLASTER)
							client->ps.pmove.pm_flags |= PMF_DUCKED;
					}
				}
				trace_priority = TRP_ALLKEEP;
			}
			return;
		} else
			zc->battlemode = 0;
	} else if (zc->battlemode == 0 && distance > 200 && ent->groundentity && ent->waterlevel < 2 && (9 * random()) > Bot[zc->botindex].param[BOP_OFFENCE]) {
		mywep = Get_KindWeapon(client->pers.weapon);
		if (mywep > WEAP_BLASTER && target->client->zc.first_target != ent) {
			f = target->s.angles[YAW] - ent->s.angles[YAW];

			if (f > 180) {
				f = -(360 - f);
			}
			if (f < -180) {
				f = -(f + 360);
			}
			if (fabs(f) < 150) {
				zc->battlemode = FIRE_CHIKEN;
				zc->battlesubcnt = 5 + (int)(random() * 8);
				trace_priority = TRP_ALLKEEP;
			}
		}
	}
}


void UsePrimaryWeapon (edict_t *ent)
{
	if (CanUsewep(ent, WEAP_BFG))
		return;

	CanUsewep(ent, Bot[ent->client->zc.botindex].param[BOP_PRIWEP]);
}


/*------------------------------------------------------------------------------*/

void UpdateExplIndex (edict_t *ent)
{
	int i;
	bool mod = false;

	for (i = 0; i < MAX_EXPLINDEX; i++) {
		if (ExplIndex[i] != NULL) {
			if (ExplIndex[i]->inuse == false)
				ExplIndex[i] = NULL;
		}
		if (!mod && ExplIndex[i] == NULL) {
			ExplIndex[i] = ent;
			mod = true;
		}
	}
}
