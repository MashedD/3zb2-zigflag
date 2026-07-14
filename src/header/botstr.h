#ifndef BOTSTRUCT
#define BOTSTRUCT

//Zigock client info
#define ALEAT_MAX 10

typedef enum
{
	BOT_GOAL_NONE,
	BOT_GOAL_ENEMY,
	BOT_GOAL_ITEM,
	BOT_GOAL_OBJECTIVE,
	BOT_GOAL_FOLLOW,
	BOT_GOAL_ROUTE_RECOVERY
} bot_goal_t;

typedef enum
{
	BOT_REASON_NONE,
	BOT_REASON_NEAREST_THREAT,
	BOT_REASON_DIRECT_ATTACKER,
	BOT_REASON_OBJECTIVE_CARRIER,
	BOT_REASON_TEAM_ASSIST,
	BOT_REASON_WEAK_ENEMY,
	BOT_REASON_ITEM_NEED,
	BOT_REASON_ITEM_UPGRADE,
	BOT_REASON_ROUTE_STUCK
} bot_reason_t;

typedef struct zgcl_s
{
	int zclass; //class no.

	int botindex; //botlist's index NO.

	// true client用 zoom フラグ
	int aiming;	   //0-not 1-aiming  2-firing zoomingflag
	float distance;	   //zoom中のFOV値
	float olddistance; //旧zooming FOV値
	bool autozoom; //autozoom
	bool lockon;   //lockon flag false-not true-locking

	// bot用
	int zcstate;  //status
	int zccmbstt; //combat status

	//duck
	float n_duckedtime; //non ducked time

	//targets
	edict_t *first_target;	//enemy		uses LockOntarget(for client)
	float targetlock;	//target locking time
	short firstinterval;	//enemy search count
	edict_t *second_target; //kindof items
	short secondinterval;	//item pickup call count

	//waiting
	vec3_t movtarget_pt; //moving target waiting point
	edict_t *waitin_obj; //for waiting sequence complete

	//basical moving
	float moveyaw; //true moving yaw
	float oldyaw;  //previous frame yaw (for rate limiting)

	//combat
	int total_bomb;	 //total put bomb
	float gren_time; //grenade time

	//contents
	//	int			front_contents;
	int ground_contents;
	float ground_slope;

	//count (inc only)
	int tmpcount;

	//moving hist
	float nextcheck;    //checking time
	vec3_t pold_origin; //old origin
	vec3_t pold_angles; //old angles
	float push_time;    //level.time when trigger_push affected bot
	vec3_t push_norm;   //velocity normal imparted by last trigger_push

	//target object shot
	bool objshot;


	edict_t *sighten; //sighting enemy to me info from entity sight
	edict_t *locked;  //locking enemy to me info from lockon missile

	//waterstate
	int waterstate;

	//route
	bool route_trace;
	int routeindex; //routing index
	float rt_locktime;
	float rt_releasetime;
	bool havetarget; //target on/off
	int targetindex;

	//battle
	edict_t *last_target; //old enemy
	vec3_t last_pos;      //old origin
	int battlemode;	      //mode
	int battlecount;      //temporary count
	int battlesubcnt;     //subcount
	int battleduckcnt;    //duck
	float fbattlecount;   //float temoporary count
	float next_fire_time; //skill-scaled cadence for precision weapons
	float combat_move_time; //next tactical movement decision
	short combat_move_dir; //-1 left, 0 pause, 1 right
	short ai_goal;        //bot_goal_t, for stable decisions and diagnostics
	short ai_reason;      //bot_reason_t explaining the selected goal
	float ai_goal_score;
	float target_seen_time;
	float target_switch_time;
	float target_acquire_time;
	vec3_t vtemp;	      //temporary vec
	int foundedenemy;     //foundedenemy
	char secwep_selected; //secondweapon selected

	vec3_t aimedpos;  //shottenpoint
	bool trapped; //trapflag

	//threat assessment
	float threat_level;	 //0.0-1.0: how dangerous current situation is
	float last_threat_check; //level.time of last threat assessment
	int nearby_enemies;	 //count of enemies in vicinity

	//team
	short tmplstate;     //teamplay state
	short ctfstate;	     //ctf state
	float ctf_role_time; //earliest time a non-carrier role may be reassigned
	edict_t *followmate; //follow
	float matelock;	     //team mate locking time
	float donation_time; //next time a team donation may be made

	// route progress and local recovery
	vec3_t route_progress_origin;
	int route_progress_index;
	float route_progress_time;
	float route_recovery_time;
	short route_recovery_attempts;
	float ai_debug_time;
} zgcl_t;

#endif
