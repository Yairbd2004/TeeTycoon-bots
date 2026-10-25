#ifndef GAME_SERVER_BOT_H
#define GAME_SERVER_BOT_H

#include <base/vmath.h>

#include "gamecontext.h"
#include "botengine.h"
#include "bot_ai/skills.h"
#include "bot_ai/navigation.h"

#include "ai/genetics.h"
#include "ai/strategy.h"

#include <string>
#include <unordered_set>
#include <vector>

const int g_aBotPriority[MAX_CLIENTS][8] = {
	{0,0,0,0,0,0,0,1},
	{0,0,0,0,0,0,0,1},
	{0,0,0,0,0,0,0,1},
	{0,0,0,0,0,0,0,1},
	{0,8,7,7,6,0,0,1},
	{0,8,7,7,6,0,0,1},
	{6,2,7,7,0,8,0,1},
	{6,2,7,7,0,8,0,1},
	{6,2,7,7,8,0,0,1},
	{6,2,7,7,8,0,0,1},
	{6,2,7,7,0,0,8,1},
	{6,2,7,7,0,0,8,1},
	{3,5,6,6,4,4,4,0},
	{3,5,6,6,4,4,4,0},
	{8,1,5,5,4,4,4,0},
	{8,1,5,5,4,4,4,0}
};

#define	BOT_HOOK_DIRS	32

#define BOT_CHECK_TIME (20*60*1000000)

class CBot
{
	class CBotEngine *m_pBotEngine;
	class CPlayer *m_pPlayer;
	class CGameContext *m_pGameServer;

protected:

	class CBotEngine *BotEngine() { return m_pBotEngine; }
	class CGameContext *GameServer() { return BotEngine()->GameServer(); }

	class CCollision *Collision() { return GameServer()->Collision(); }
	class CTuningParams *Tuning();

	CBotEngine::CPath *m_pPath;

	int m_SnapID;

	enum {
		BFLAG_LOST	= 0,
		BFLAG_LEFT	= 1,
		BFLAG_RIGHT	= 2,
		BFLAG_JUMP	= 4,
		BFLAG_HOOK	= 8,
		BFLAG_FIRE	= 16
	};

	int m_Flags;

	vec2 m_RealTarget;
	struct CTarget {
		vec2 m_Pos;
		enum {
			TARGET_EMPTY=-1,
			TARGET_PLAYER=0,
			TARGET_FLAG,
			TARGET_ARMOR,
			TARGET_HEALTH,
			TARGET_WEAPON_SHOTGUN,
			TARGET_WEAPON_GRENADE,
			//TARGET_POWERUP_NINJA,
			TARGET_WEAPON_LASER,
			TARGET_AIR,
			NUM_TARGETS
		};
		int m_Type;
		int m_PlayerCID;
		bool m_NeedUpdate;
		int m_StartTick;
	} m_ComputeTarget;
	vec2 m_LastPathTargetPos = vec2(0, 0);
	bool m_HasPathTarget = false;

	class CGenetics m_Genetics;
	int m_aTargetOrder[CTarget::NUM_TARGETS];

	CStrategyPosition* m_pStrategyPosition;

	void UpdateTargetOrder();

	CNetObj_PlayerInput m_InputData;
	CNetObj_PlayerInput m_LastData;

	int m_Move;
	vec2 m_Direction;
	int m_Jump;
	int m_Attack;
	int m_Hook;

	int GetTarget();
	void UpdateTarget();
	int GetTeam(int ClientID);
	int IsFalling();
	bool IsGrounded();

	bool NeedPickup(int Type);
	bool FindPickup(int Type, vec2 *pPos, float Radius = 1000);

	void HandleWeapon(bool SeeTarget);
	void HandleHook(bool SeeTarget);
	void UpdateEdge();
	void MakeChoice(bool UseTarget);
	int Skill(EPetSkill SkillId) const;
	bool IsDangerous(vec2 Pos, int Team = -1);
	void ApplyMovementSkills();
	bool DefendAgainstUpwardThrow(bool CombatHookAim);
	bool FindBounceAim(vec2 Target, vec2 *pAim);

	int GetTile(int x, int y) { return BotEngine()->GetTile(x/32,y/32);}

	vec2 ClosestCharacter();

public:
	CBot(class CBotEngine *m_pBotEngine, CPlayer *pPlayer, int ownerid);
	virtual ~CBot();

	vec2 m_Target;
	bool m_destruct;
	bool ownerAttacked;
	int enemyID;
	int enemyTime;
	bool isClose;
	int m_GenomeTick;
	int emoteTick;
	int stuckTick;
	int owner;
	bool stay;
	bool stuck;
	bool m_Rescuing = false;
	bool m_Fighting = false;
	int m_ThreatUntilTick = 0;
	int m_SelfAggressorId = -1;
	int m_SelfAggressorSpawnTick = -1;
	int m_SelfAggressorUntilTick = 0;
	std::unordered_set<std::string> m_HelpNames;
	std::unordered_set<std::string> m_BlockNames;
	int m_FreezeRespawnSeconds = 10;
	int m_NinjaActiveUntilTick = 0;
	int m_NinjaCooldownUntilTick = 0;
	int m_NinjaNextDashTick = 0;
	int m_MobilityShotgunTick = 0;
	int m_MobilityGrenadeTick = 0;
	int m_FrozenSinceTick = -1;
	bool m_RespawnFromFreeze = false;
	bool m_RaceToFrozenOwner = false;
	bool m_UsingLocalRoute = false;
	struct SNavigationPoint { vec2 m_Pos; bool m_Teleport; };
	bot_ai::CNavigationSearch m_NavigationSearch;
	std::vector<SNavigationPoint> m_vNavigationRoute;
	std::array<int, 16> m_aNavigationSnapIds;
	int m_NavigationIndex = 0;
	int m_NavigationTargetId = -2;
	vec2 m_NavigationGoal = vec2(0, 0);
	vec2 m_NavigationSearchStart = vec2(0, 0);
	bool m_NavigationActive = false;
	bool m_NavigationTest = false;
	bool m_NavigationTrace = false;
	bool m_NavigationHasTeleports = false;
	vec2 m_NavigationTestGoal = vec2(0, 0);
	int m_NavigationPlanTick = -1;
	int m_NavigationControlTick = -1;
	int m_NavigationProgressTick = -1;
	int m_NavigationProgressIndex = -1;
	float m_NavigationBestDistance = 1e30f;
	int m_NavigationDirection = 0;
	bool m_NavigationJump = false;
	bool m_NavigationHook = false;
	vec2 m_NavigationAnchor = vec2(0, 0);
	void ResetNavigation();
	bool NavigationClear(vec2 From, vec2 To, bool CheckHazards = true);
	unsigned char NavigationCellFlags(bot_ai::CNavigationSearch &Search, int Cell);
	void StepNavigationSearch(bot_ai::CNavigationSearch &Search, int Budget, bool Teleports);
	void SimulateNavigationTick(CCharacterCore &Core, int &FreezeTicks);
	int NavigationPortalLanding(bot_ai::CNavigationSearch &Search, vec2 Exit);
	bool UpdateNavigation(vec2 Goal, int TargetId, bool Force = false);
	void DriveNavigation();
	int m_LastRescuePlanTick = -1;
	int m_RescueTargetId = -1;
	vec2 m_RescueWaypoint = vec2(0, 0);
	vec2 m_RescueVantage = vec2(0, 0);
	vec2 m_LastRescueTargetPos = vec2(0, 0);
	bool m_HasRescueRoute = false;
	int m_RescueCandidateOffset = 0;
	vec2 m_RescueProgressPos = vec2(0, 0);
	int m_RescueProgressTick = -1;
	bool m_LocalRouteSearchDeferred = false;
	bool m_NoSafeRoute = false;
	int m_LastFreezeCrossPlanTick = -1;
	int m_FreezeCrossUntilTick = -1;
	int m_FreezeCrossDirection = 0;
	int m_FreezeCrossJump = 0;
	int m_FreezeCrossHook = 0;
	vec2 m_FreezeCrossAim = vec2(0, -1);
	vec2 m_FreezeCrossGoal = vec2(0, 0);
	int m_BlockFreezeTargetId = -1;
	int m_LastBlockPlanTick = -1;
	vec2 m_BlockFreezeGoal = vec2(0, 0);
	vec2 m_BlockStance = vec2(0, 0);
	vec2 m_BlockWaypoint = vec2(0, 0);
	vec2 m_LastBlockTargetPos = vec2(0, 0);
	bool m_LastBlockTargetOnFreeze = false;
	int m_LastBlockStancePlanTick = -1;
	int m_LastBlockGrenadePlanTick = -1;
	int m_LastThinkTick = -1;
	int m_BlockGrenadeTargetId = -1;
	int m_BlockGrenadeCooldownUntilTick = 0;
	vec2 m_BlockGrenadeTargetPos = vec2(0, 0);
	vec2 m_BlockGrenadeGoal = vec2(0, 0);
	vec2 m_BlockGrenadeAim = vec2(0, 0);
	bool m_HasBlockFreezeGoal = false;
	bool m_HasBlockStance = false;
	bool m_HasBlockGrenadeAim = false;
	int m_LastBlockJumpCheckTick = -1;
	int m_BlockJumpCooldownUntilTick = -1;
	bool m_BlockJumpArcSafe = false;
	int m_PlayerHookTargetId = -1;
	int m_PlayerHookHoldUntilTick = -1;
	mutable int m_BlockerAllyCacheTick = -1;
	mutable int m_BlockerAllyHumanId = -1;
	bool m_BlockerSpawnTelePending = false;
	bool m_BlockerSpawnTeleUsed = false;
	vec2 m_BlockerSpawnTelePos = vec2(0, 0);
	vec2 m_BlockerPatrolPos = vec2(0, 0);
	int m_LastBlockerPatrolTick = -1;
	vec2 m_LastProgressPos = vec2(0, 0);
	int m_LastProgressTick = -1;
	vec2 m_LastGoalProgressTarget = vec2(0, 0);
	float m_LastGoalProgressDistance = 1e30f;
	int m_LastGoalProgressTick = -1;
	int m_LastRouteRefreshTick = -1;
	bool m_TriedWallJump = false;
	int m_LastWallHookPlanTick = -1;
	int m_DefenseHookUntilTick = -1;
	vec2 m_DefenseHookAim = vec2(0, 1);
	bool CanUseWeapon(int Weapon) const;
	void ApplyPurchasedWeapons();
	void UpdatePetNinja();
	bool TryMobilityWeapon();
	bool CanHammerHit(CCharacter *pTarget);
	bool FindLocalRoute(vec2 Start, vec2 Goal, vec2 *pWaypoint, std::vector<vec2> *pWaypoints = nullptr);
	bool HasReachableClimbHook(vec2 Position);
	bool FindRescueRoute(int TargetId, vec2 TargetPos, vec2 *pWaypoint);
	bool SafeTravelSegment(vec2 Start, vec2 End);
	bool HasFreezeBelow(vec2 Pos, float MaxDistance);
	bool IsFreezeAt(vec2 Pos, int Team = -1);
	bool IsInFreezeFootprint(vec2 Pos, int Team = -1);
	bool IsDeathAt(vec2 Pos, int Team = -1);
	float FreezeClearanceAbove(vec2 Pos, float MaxDistance);
	bool FindSafeFreezeCrossing(vec2 Goal, int *pDirection, int *pJump, int *pHook, vec2 *pAim);
	bool FindBlockFreezeGoal(vec2 TargetPos, int RemainingFreezeTicks, vec2 *pGoal, bool *pSupported);
	bool FindBlockGrenadeAim(CCharacter *pTarget, vec2 FreezeGoal, vec2 *pAim);
	bool FindBlockStance(vec2 MyPos, vec2 TargetPos, vec2 FreezeGoal, vec2 *pStance, vec2 *pWaypoint);
	bool ShouldHoldEnemyHook(CCharacter *pTarget, vec2 FreezeGoal);
	void OnSkillUpgrade();
	bool IsHelpTarget(const CPlayer *pTarget, int TargetId) const;
	bool IsNearbyBlockerAlly(const CPlayer *pTarget) const;
	bool IsBlockTarget(const CPlayer *pTarget, int TargetId) const;
	void NotifyProtectedPlayerHurt(int VictimId, int EnemyId, bool DealtDamage);
	void NotifyAttackedBy(int EnemyId);
	void checkStuck(bool inSight);
	void emote();
	int GetID() { return m_SnapID; }
	void Snap(int SnappingClient);
	void Tick();

	virtual void OnReset();

	CNetObj_PlayerInput GetInputData() { Tick(); return m_InputData; };
	CNetObj_PlayerInput GetLastInputData() { return m_LastData; }
};

#endif
