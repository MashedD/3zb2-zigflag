#ifndef BOTSTRUCT
#define BOTSTRUCT

//Zigock client info
#define ALEAT_MAX 10

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
	edict_t *followmate; //follow
	float matelock;	     //team mate locking time
} zgcl_t;

#endif
