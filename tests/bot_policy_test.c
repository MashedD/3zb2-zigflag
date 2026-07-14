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
	return 0;
}
