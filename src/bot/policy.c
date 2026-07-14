#include "policy.h"

float BotPolicy_TargetScore(const bot_policy_target_t *target)
{
	float score = 1200.0f - target->distance;

	if (target->attacks_me)
		score += 900.0f;
	if (target->objective_carrier)
		score += 2400.0f;
	if (target->assists_teammate)
		score += 450.0f;
	if (target->health > 0 && target->health < 50)
		score += 200.0f;
	if (target->firing)
		score += 160.0f;
	if (target->weapon_advantage)
		score += 90.0f;
	if (target->close_while_vulnerable)
		score += 120.0f;
	return score;
}

bot_policy_bool_t BotPolicy_ShouldSwitchTarget(float current_score, float candidate_score,
	bot_policy_bool_t target_lock_active, bot_policy_bool_t urgent)
{
	float margin = target_lock_active ? 360.0f : 160.0f;

	return urgent || candidate_score > current_score + margin;
}

float BotPolicy_ItemScore(const bot_policy_item_t *item)
{
	float score = 120.0f;
	int pickup = item->pickup_skill;

	if (pickup < 0)
		pickup = 0;
	if (pickup > 9)
		pickup = 9;
	score -= item->distance * (0.38f - pickup * 0.025f);
	if (item->dangerous)
		score -= 500.0f;

	switch (item->kind) {
	case BOT_ITEM_HEALTH:
		score += item->need * 500.0f + item->effective_gain * 3.0f;
		break;
	case BOT_ITEM_ARMOR:
		score += item->need * 420.0f + item->effective_gain * 2.0f;
		break;
	case BOT_ITEM_WEAPON:
		score += 420.0f;
		if (item->weapon_upgrade)
			score += 260.0f;
		if (item->preferred_weapon)
			score += 180.0f;
		if (item->dropped)
			score += 100.0f;
		break;
	case BOT_ITEM_AMMO:
		if (item->need <= 0.0f)
			return BOT_POLICY_REJECTED;
		score += item->need * 420.0f + item->effective_gain;
		break;
	case BOT_ITEM_POWERUP:
		score += 700.0f;
		break;
	case BOT_ITEM_OBJECTIVE:
		score += 1400.0f;
		break;
	case BOT_ITEM_ROAM:
		score += 40.0f;
		break;
	}
	return score;
}

float BotPolicy_WeaponScore(const bot_policy_weapon_t *weapon)
{
	float score = weapon->preference * 25.0f;

	if (weapon->available_shots <= 0 || weapon->corridor_blocked ||
	    weapon->splash_unsafe)
		return BOT_POLICY_REJECTED;
	if (weapon->current_weapon)
		score += 35.0f;
	if (weapon->available_shots > 5)
		score += 25.0f;

	switch (weapon->kind) {
	case BOT_WEAPON_SHOTGUN:
	case BOT_WEAPON_SUPERSHOTGUN:
		score += weapon->distance < 450.0f ? 260.0f : -180.0f;
		break;
	case BOT_WEAPON_MACHINEGUN:
	case BOT_WEAPON_CHAINGUN:
	case BOT_WEAPON_HYPERBLASTER:
		score += 170.0f;
		break;
	case BOT_WEAPON_GRENADES:
	case BOT_WEAPON_GRENADELAUNCHER:
		score += weapon->distance > 180.0f && weapon->distance < 700.0f ? 230.0f : -260.0f;
		break;
	case BOT_WEAPON_ROCKETLAUNCHER:
		score += weapon->distance > 150.0f && weapon->distance < 1000.0f ? 300.0f : -300.0f;
		break;
	case BOT_WEAPON_RAILGUN:
		score += weapon->distance > 450.0f ? 330.0f : 80.0f;
		break;
	case BOT_WEAPON_BFG:
		score += weapon->distance > 500.0f && weapon->nearby_enemies > 1 ? 400.0f : -120.0f;
		break;
	case BOT_WEAPON_BLASTER:
	case BOT_WEAPON_OTHER:
		score += 20.0f;
		break;
	}
	if (weapon->target_health > 0 && weapon->target_health < 35)
		score += 40.0f;
	return score;
}

float BotPolicy_RouteScore(const bot_policy_route_t *route)
{
	float score = 800.0f - route->distance;

	if (route->forward)
		score += 260.0f;
	if (route->linked)
		score += 360.0f;
	if (route->objective_match)
		score += 500.0f;
	if (route->backtrack)
		score -= 300.0f;
	return score;
}

bot_policy_ctf_quota_t BotPolicy_CTFQuota(int team_size, bot_policy_bool_t has_carrier)
{
	bot_policy_ctf_quota_t quota = {0, 0};
	int available;

	if (team_size <= 0)
		return quota;
	if (team_size >= 2) {
		quota.defenders = team_size / 3;
		if (quota.defenders < 1)
			quota.defenders = 1;
	}
	if (has_carrier && team_size >= 3)
		quota.supporters = team_size >= 6 ? 2 : 1;

	available = team_size - (has_carrier ? 1 : 0);
	while (quota.defenders + quota.supporters > available - (team_size >= 3 ? 1 : 0)) {
		if (quota.supporters > 0)
			quota.supporters--;
		else if (quota.defenders > 0)
			quota.defenders--;
		else
			break;
	}
	return quota;
}
