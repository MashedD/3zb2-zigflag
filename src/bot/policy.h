#ifndef BOT_POLICY_H
#define BOT_POLICY_H

#define BOT_POLICY_REJECTED (-1000000.0f)

typedef unsigned char bot_policy_bool_t;

typedef enum {
	BOT_ITEM_HEALTH,
	BOT_ITEM_ARMOR,
	BOT_ITEM_WEAPON,
	BOT_ITEM_AMMO,
	BOT_ITEM_POWERUP,
	BOT_ITEM_OBJECTIVE,
	BOT_ITEM_ROAM
} bot_item_kind_t;

typedef enum {
	BOT_WEAPON_BLASTER,
	BOT_WEAPON_SHOTGUN,
	BOT_WEAPON_SUPERSHOTGUN,
	BOT_WEAPON_MACHINEGUN,
	BOT_WEAPON_CHAINGUN,
	BOT_WEAPON_GRENADES,
	BOT_WEAPON_GRENADELAUNCHER,
	BOT_WEAPON_ROCKETLAUNCHER,
	BOT_WEAPON_HYPERBLASTER,
	BOT_WEAPON_RAILGUN,
	BOT_WEAPON_BFG,
	BOT_WEAPON_OTHER
} bot_policy_weapon_kind_t;

typedef enum {
	BOT_WEAPON_UNAVAILABLE,
	BOT_WEAPON_SWITCHING,
	BOT_WEAPON_BLOCKED,
	BOT_WEAPON_FIRED
} bot_weapon_action_t;

typedef enum {
	BOT_MOVE_HOLD,
	BOT_MOVE_FORWARD,
	BOT_MOVE_BACK,
	BOT_MOVE_LEFT,
	BOT_MOVE_RIGHT
} bot_policy_move_kind_t;

typedef struct {
	float distance;
	int health;
	bot_policy_bool_t attacks_me;
	bot_policy_bool_t objective_carrier;
	bot_policy_bool_t assists_teammate;
	bot_policy_bool_t firing;
	bot_policy_bool_t weapon_advantage;
	bot_policy_bool_t close_while_vulnerable;
} bot_policy_target_t;

typedef struct {
	bot_item_kind_t kind;
	float distance;
	int pickup_skill;
	float need;
	float effective_gain;
	bot_policy_bool_t weapon_upgrade;
	bot_policy_bool_t preferred_weapon;
	bot_policy_bool_t dropped;
	bot_policy_bool_t dangerous;
} bot_policy_item_t;

typedef struct {
	bot_policy_weapon_kind_t kind;
	float distance;
	int available_shots;
	int preference;
	int nearby_enemies;
	int target_health;
	bot_policy_bool_t current_weapon;
	bot_policy_bool_t splash_unsafe;
	bot_policy_bool_t corridor_blocked;
} bot_policy_weapon_t;

typedef struct {
	float distance;
	bot_policy_bool_t forward;
	bot_policy_bool_t linked;
	bot_policy_bool_t objective_match;
	bot_policy_bool_t backtrack;
} bot_policy_route_t;

typedef struct {
	bot_policy_move_kind_t kind;
	float range_error;
	float objective_progress;
	bot_policy_bool_t cover;
	bot_policy_bool_t exposed;
	bot_policy_bool_t hazard;
	bot_policy_bool_t teammate_crowded;
} bot_policy_move_t;

typedef struct {
	float base_score;
	float travel_cost;
	float confidence;
	bot_policy_bool_t urgent;
	bot_policy_bool_t committed;
} bot_policy_goal_t;

typedef struct {
	int offence;
	int teamwork;
	float carrier_distance;
	bot_policy_bool_t defender;
	bot_policy_bool_t supporter;
} bot_policy_role_t;

typedef struct {
	int defenders;
	int supporters;
} bot_policy_ctf_quota_t;

float BotPolicy_TargetScore(const bot_policy_target_t *target);
bot_policy_bool_t BotPolicy_ShouldSwitchTarget(float current_score, float candidate_score,
	bot_policy_bool_t target_lock_active, bot_policy_bool_t urgent);
float BotPolicy_ItemScore(const bot_policy_item_t *item);
float BotPolicy_WeaponScore(const bot_policy_weapon_t *weapon);
float BotPolicy_RouteScore(const bot_policy_route_t *route);
bot_policy_ctf_quota_t BotPolicy_CTFQuota(int team_size, bot_policy_bool_t has_carrier);
float BotPolicy_MemoryConfidence(float age, int estimate_skill, bot_policy_bool_t visible);
float BotPolicy_MoveScore(const bot_policy_move_t *move);
float BotPolicy_GoalScore(const bot_policy_goal_t *goal);
float BotPolicy_RoleScore(const bot_policy_role_t *role);
float BotPolicy_ProjectileLead(float distance, float projectile_speed, int aim_skill);

#endif
