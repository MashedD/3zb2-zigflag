#include <stdbool.h>
#include <stdio.h>
#include "../src/bot/policy.h"

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
		return 1; \
	} \
} while (0)

int main(void)
{
	bot_policy_target_t ordinary = {.distance = 100.0f, .health = 100};
	bot_policy_target_t carrier = {.distance = 800.0f, .health = 100, .objective_carrier = true};
	bot_policy_item_t health = {.kind = BOT_ITEM_HEALTH, .distance = 100.0f, .pickup_skill = 5,
		.need = 0.8f, .effective_gain = 50.0f};
	bot_policy_item_t ammo = {.kind = BOT_ITEM_AMMO, .distance = 20.0f, .pickup_skill = 9};
	bot_policy_weapon_t close_rocket = {.kind = BOT_WEAPON_ROCKETLAUNCHER, .distance = 80.0f,
		.available_shots = 5};
	bot_policy_weapon_t far_rail = {.kind = BOT_WEAPON_RAILGUN, .distance = 900.0f,
		.available_shots = 5};
	bot_policy_weapon_t far_shotgun = {.kind = BOT_WEAPON_SHOTGUN, .distance = 900.0f,
		.available_shots = 5};
	bot_policy_route_t objective = {.distance = 300.0f, .forward = true, .objective_match = true};
	bot_policy_route_t near_backtrack = {.distance = 30.0f, .backtrack = true};
	bot_policy_ctf_quota_t quota;
	bot_policy_move_t cover = {.kind = BOT_MOVE_LEFT, .range_error = 20, .cover = true};
	bot_policy_move_t hazard = {.kind = BOT_MOVE_RIGHT, .hazard = true};
	bot_policy_goal_t committed = {.base_score = 500, .travel_cost = 100,
		.confidence = 1, .committed = true};
	bot_policy_goal_t uncertain = {.base_score = 700, .travel_cost = 100, .confidence = 0.4f};
	bot_policy_role_t defender = {.offence = 2, .teamwork = 7, .defender = true};
	bot_policy_role_t attacker = {.offence = 8, .teamwork = 4, .defender = true};

	CHECK(BotPolicy_TargetScore(&carrier) > BotPolicy_TargetScore(&ordinary));
	CHECK(!BotPolicy_ShouldSwitchTarget(1000.0f, 1200.0f, true, false));
	CHECK(BotPolicy_ShouldSwitchTarget(1000.0f, 1001.0f, true, true));
	CHECK(BotPolicy_ItemScore(&health) > 400.0f);
	CHECK(BotPolicy_ItemScore(&ammo) == BOT_POLICY_REJECTED);
	CHECK(BotPolicy_WeaponScore(&far_rail) > BotPolicy_WeaponScore(&far_shotgun));
	CHECK(BotPolicy_WeaponScore(&close_rocket) < 0.0f);
	close_rocket.splash_unsafe = true;
	CHECK(BotPolicy_WeaponScore(&close_rocket) == BOT_POLICY_REJECTED);
	CHECK(BotPolicy_RouteScore(&objective) > BotPolicy_RouteScore(&near_backtrack));
	quota = BotPolicy_CTFQuota(6, true);
	CHECK(quota.defenders == 2 && quota.supporters == 2);
	quota = BotPolicy_CTFQuota(2, false);
	CHECK(quota.defenders == 1 && quota.supporters == 0);
	CHECK(BotPolicy_MemoryConfidence(0.2f, 8, false) >
	      BotPolicy_MemoryConfidence(0.8f, 8, false));
	CHECK(BotPolicy_MemoryConfidence(2.0f, 9, false) == 0.0f);
	CHECK(BotPolicy_MoveScore(&cover) > 300.0f);
	CHECK(BotPolicy_MoveScore(&hazard) == BOT_POLICY_REJECTED);
	CHECK(BotPolicy_GoalScore(&committed) > BotPolicy_GoalScore(&uncertain));
	CHECK(BotPolicy_RoleScore(&defender) > BotPolicy_RoleScore(&attacker));
	CHECK(BotPolicy_ProjectileLead(1000, 1000, 9) > BotPolicy_ProjectileLead(1000, 1000, 1));
	CHECK(BotPolicy_ProjectileLead(5000, 100, 9) == 1.2f);
	return 0;
}
