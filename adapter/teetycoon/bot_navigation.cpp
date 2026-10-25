#include "bot.h"
#include "bot_ai/brain.h"
#include "entities/character.h"
#include "player.h"

#include <engine/shared/config.h>
#include <game/collision.h>
#include <game/mapitems.h>

void CBot::ResetNavigation()
{
	m_NavigationSearch.Reset();
	m_vNavigationRoute.clear();
	m_NavigationIndex = 0;
	m_NavigationPlanTick = -1;
	m_NavigationControlTick = -1;
	m_NavigationProgressTick = -1;
	m_NavigationProgressIndex = -1;
	m_NavigationBestDistance = 1e30f;
	m_NavigationActive = false;
	m_NavigationHook = false;
}

bool CBot::NavigationClear(vec2 From, vec2 To, bool CheckHazards)
{
	// Test the swept square used by MoveBox, including its corners. Testing only
	// a center ray (or a cross) lets a tee clip the inside of a route corner.
	const vec2 Corners[] = {vec2(-14, -14), vec2(14, -14), vec2(-14, 14), vec2(14, 14)};
	for(vec2 Corner : Corners)
		if(Collision()->FastIntersectLine(From + Corner, To + Corner, nullptr, nullptr)) return false;
	const int Steps = static_cast<int>(std::ceil(distance(From, To) / 16.0f));
	for(int Step = 0; Step <= Steps; ++Step)
	{
		const vec2 Pos = Steps ? mix(From, To, static_cast<float>(Step) / Steps) : From;
		if(Collision()->TestBox(Pos, CCharacterCore::PhysicalSizeVec2()))
			return false;
		if(CheckHazards && (IsDangerous(Pos) || IsDangerous(Pos + vec2(13, 13)) ||
			IsDangerous(Pos + vec2(-13, 13)) || IsDangerous(Pos + vec2(13, -13)) ||
			IsDangerous(Pos + vec2(-13, -13))))
			return false;
	}
	return true;
}

unsigned char CBot::NavigationCellFlags(bot_ai::CNavigationSearch &Search, int Cell)
{
	auto It = Search.m_CellFlags.find(Cell);
	if(It != Search.m_CellFlags.end())
		return It->second;
	const int Width = Collision()->GetWidth();
	const vec2 Pos((Cell % Width) * 32 + 16.0f, (Cell / Width) * 32 + 16.0f);
	const unsigned char Flags = NavigationClear(Pos, Pos) ? 1 : 0;
	Search.m_CellFlags.emplace(Cell, Flags);
	return Flags;
}

void CBot::SimulateNavigationTick(CCharacterCore &Core, int &FreezeTicks)
{
	const int Index = Collision()->GetPureMapIndex(Core.m_Pos);
	Core.m_Tuning = *m_pPlayer->GetCharacter()->GetTuning(Collision()->IsTune(Index));
	// The rollout world has no other characters. Skip its empty character
	// collision/hook loops; this changes no terrain physics and avoids scanning
	// all client slots at every simulated pixel of every candidate movement.
	Core.m_Tuning.m_PlayerCollision = 0;
	Core.m_Tuning.m_PlayerHooking = 0;
	if(IsInFreezeFootprint(Core.m_Pos))
		FreezeTicks = g_Config.m_SvFreezeDelay * GameServer()->Server()->TickSpeed();
	if(Collision()->GetTileIndex(Index) == TILE_UNFREEZE || Collision()->GetFrontTileIndex(Index) == TILE_UNFREEZE)
		FreezeTicks = 0;
	if(FreezeTicks > 0)
	{
		--FreezeTicks;
		Core.m_Input.m_Direction = 0;
		Core.m_Input.m_Jump = 0;
		Core.m_Input.m_Hook = 0;
	}
	Core.Tick(true, false);
	if(Collision()->IsSpeedup(Index))
	{
		vec2 Direction;
		int Force, MaxSpeed, Type;
		Collision()->GetSpeedup(Index, &Direction, &Force, &MaxSpeed, &Type);
		if(Type == TILE_SPEED_BOOST_OLD && Force == 255 && MaxSpeed)
			Core.m_Vel = Direction * (MaxSpeed / 5);
		else
		{
			float Delta = static_cast<float>(Force);
			if(Type == TILE_SPEED_BOOST_OLD && MaxSpeed > 0)
				Delta = std::clamp(std::max(5, MaxSpeed) / 5.0f - dot(Direction, Core.m_Vel), -Delta, Delta);
			else if(Type == TILE_SPEED_BOOST)
			{
				if(MaxSpeed == 0)
					MaxSpeed = std::max(Core.m_Tuning.m_VelrampRange / (50 * std::log(std::max(static_cast<float>(Core.m_Tuning.m_VelrampCurvature), 1.01f))), Core.m_Tuning.m_VelrampStart / 50) * 5;
				Delta = std::min(Delta, MaxSpeed / 5.0f - dot(Direction, Core.m_Vel));
			}
			Core.m_Vel = ClampVel(Collision()->GetMoveRestrictions(Core.m_Pos), Core.m_Vel + Direction * Delta);
		}
	}
	Core.Move();
	Core.Quantize();
}

int CBot::NavigationPortalLanding(bot_ai::CNavigationSearch &Search, vec2 Exit)
{
	const int ExitCell = Collision()->GetPureMapIndex(Exit);
	if(NavigationCellFlags(Search, ExitCell) & 1)
		return ExitCell;
	const auto Found = Search.m_PortalLandings.find(ExitCell);
	if(Found != Search.m_PortalLandings.end())
		return Found->second;
	int Landing = -1;
	// Some maps deliberately place tele-outs in freeze chutes. Predict their
	// passive fall through the actual tuning and speedups, including the freeze
	// timer and unfreeze tile. A solid/dead end must never become a portal edge.
	if(IsInFreezeFootprint(Exit) && !Collision()->TestBox(Exit, CCharacterCore::PhysicalSizeVec2()))
	{
		CWorldCore World;
		CCharacterCore Core = *m_pPlayer->GetCharacter()->Core();
		Core.Init(&World, Collision());
		Core.m_Pos = Exit;
		Core.m_Vel = vec2(0, 0);
		Core.m_HookState = HOOK_IDLE;
		Core.SetHookedPlayer(-1);
		Core.m_Input = {};
		Core.m_Input.m_TargetY = -1;
		int FreezeTicks = 0;
		for(int Step = 0; Step < 300; ++Step)
		{
			SimulateNavigationTick(Core, FreezeTicks);
			if(IsDeathAt(Core.m_Pos) || IsDeathAt(Core.m_Pos + vec2(0, 14))) break;
			if(FreezeTicks == 0 && NavigationClear(Core.m_Pos, Core.m_Pos))
			{
				const int Cell = Collision()->GetPureMapIndex(Core.m_Pos);
				if(NavigationCellFlags(Search, Cell) & 1) { Landing = Cell; break; }
			}
		}
	}
	Search.m_PortalLandings.emplace(ExitCell, Landing);
	return Landing;
}

void CBot::StepNavigationSearch(bot_ai::CNavigationSearch &Search, int Budget, bool Teleports)
{
	const int Width = Collision()->GetWidth();
	const int Height = Collision()->GetHeight();
	const auto Position = [Width](int Cell) { return vec2(Cell % Width * 32 + 16.0f, Cell / Width * 32 + 16.0f); };
	const float Gravity = std::max(0.1f, static_cast<float>(Tuning()->m_Gravity));
	const float JumpHeight = (static_cast<float>(Tuning()->m_GroundJumpImpulse) * Tuning()->m_GroundJumpImpulse +
		static_cast<float>(Tuning()->m_AirJumpImpulse) * Tuning()->m_AirJumpImpulse) / (2 * Gravity);
	const auto CanRise = [&](int Cell) {
		unsigned char Flags = NavigationCellFlags(Search, Cell);
		if(Flags & 4)
			return (Flags & 2) != 0;
		const vec2 Pos = Position(Cell);
		bool Rise = false;
		// This is a conservative geometric corridor test. The controller below
		// separately simulates the actual jumps/hooks before applying an input.
		for(float Y = 16; Y <= std::min(320.0f, JumpHeight); Y += 16)
		{
			if(IsDangerous(Pos + vec2(0, Y)))
				break;
			if(Collision()->CheckPoint(Pos + vec2(0, Y))) { Rise = true; break; }
		}
		if(!Rise && g_Config.m_SvBotAllowHook)
		{
			for(int Ray = 0; Ray < 9 && !Rise; ++Ray)
			{
				const vec2 Aim = direction(-pi + (Ray + 1) * pi / 10.0f);
				vec2 Hit;
				const int Tile = Collision()->FastIntersectLine(Pos, Pos + Aim * Tuning()->m_HookLength, &Hit, nullptr);
				Rise = Tile && Tile != TILE_NOHOOK && Hit.y < Pos.y - 24;
			}
		}
		Search.m_CellFlags[Cell] = Flags | 4 | (Rise ? 2 : 0);
		return Rise;
	};
	Search.Step(Budget, [&](int Cell, auto Add) {
		const vec2 Pos = Position(Cell);
		const int X = Cell % Width, Y = Cell / Width;
		int Tele = Teleports ? Collision()->IsTeleport(Cell) : 0;
		if(Teleports && !Tele)
			Tele = Collision()->IsEvilTeleport(Cell);
		if(Tele > 0 && !Collision()->TeleOuts(Tele - 1).empty())
		{
			for(vec2 Exit : Collision()->TeleOuts(Tele - 1))
			{
				const int Next = NavigationPortalLanding(Search, Exit);
				if(Next >= 0)
					Add(Next, 32.0f, true);
			}
			return; // Stepping onto an inlet triggers it; walking through is not an edge.
		}
		for(int DY = -1; DY <= 1; ++DY)
			for(int DX = -1; DX <= 1; ++DX)
			{
				if((!DX && !DY) || X + DX < 0 || X + DX >= Width || Y + DY < 0 || Y + DY >= Height)
					continue;
				const int Next = Cell + DY * Width + DX;
				if(!(NavigationCellFlags(Search, Next) & 1))
					continue;
				if(DX && DY && (!(NavigationCellFlags(Search, Cell + DX) & 1) ||
					!(NavigationCellFlags(Search, Cell + DY * Width) & 1)))
					continue;
				const int Restrictions = Collision()->GetMoveRestrictions(Pos);
				if((DX < 0 && (Restrictions & CANTMOVE_LEFT)) || (DX > 0 && (Restrictions & CANTMOVE_RIGHT)) ||
					(DY < 0 && (Restrictions & CANTMOVE_UP)) || (DY > 0 && (Restrictions & CANTMOVE_DOWN)))
					continue;
				if(DY < 0 && !CanRise(Cell))
					continue;
				const float Risk = std::min(8, BotEngine()->NavigationDeathPenalty(Position(Next))) * 8.0f;
				Add(Next, (DX && DY ? 45.255f : 32.0f) + (DY < 0 ? 4.0f : 0.0f) + Risk, false);
			}
	}, [&](int Cell) {
		// A teleporter can invalidate a Euclidean lower bound. Dijkstra is exact
		// for the graph including portals; its frontier is resumed across ticks.
		return Teleports ? 0.0f : distance(Position(Cell), Position(Search.m_Goal));
	});
}

bool CBot::UpdateNavigation(vec2 Goal, int TargetId, bool Force)
{
	using EState = bot_ai::CNavigationSearch::EState;
	const vec2 Pos = m_pPlayer->GetCharacter()->GetPos();
	const int Now = GameServer()->Server()->Tick();
	const bool Direct = NavigationClear(Pos, Goal, false);
	// Combat is allowed only once the opponent is actually reachable. A hidden
	// target must not overwrite a route waypoint with its raw x/y position.
	if(!Force && distance(Pos, Goal) < 280.0f && Goal.y > Pos.y - 120.0f && Direct)
	{
		if(m_NavigationSearch.m_State != EState::IDLE)
			ResetNavigation();
		return false;
	}
	m_NavigationActive = true;
	m_UsingLocalRoute = true;
	m_pPath->m_Size = 0;
	m_ComputeTarget.m_Type = CTarget::TARGET_AIR;
	m_ComputeTarget.m_PlayerCID = TargetId;
	m_ComputeTarget.m_NeedUpdate = false;
	if(distance(Pos, Goal) < 28.0f && Direct)
	{
		m_vNavigationRoute.clear();
		m_ComputeTarget.m_Pos = Pos;
		if(m_NavigationTrace && m_NavigationProgressIndex != -2)
			dbg_msg("botnav", "ARRIVED cid=%d tick=%d pos=%.2f,%.2f", m_pPlayer->GetCid(), Now, Pos.x, Pos.y);
		m_NavigationProgressIndex = -2;
		return true;
	}
	bool Replan = m_NavigationSearch.m_State == EState::IDLE ||
		(m_NavigationSearch.m_State == EState::FOUND && m_vNavigationRoute.empty()) || m_NavigationTargetId != TargetId ||
		(distance_squared(m_NavigationGoal, Goal) > 96.0f * 96.0f && Now - m_NavigationPlanTick >= 25);
	if(!m_vNavigationRoute.empty())
	{
		// A teleport edge is a discontinuity, never a straight travel segment.
		if(m_NavigationIndex + 1 < static_cast<int>(m_vNavigationRoute.size()) &&
			m_vNavigationRoute[m_NavigationIndex + 1].m_Teleport &&
			distance(Pos, m_vNavigationRoute[m_NavigationIndex + 1].m_Pos) < 96)
			m_NavigationIndex += 2;
		if(m_NavigationIndex >= static_cast<int>(m_vNavigationRoute.size()))
			Replan = true;
		else
		{
			// Look ahead only through body-clear segments. Proximity alone must
			// never skip a corner while the tee is still below the solid ledge.
			for(int i = m_NavigationIndex + 1; i < static_cast<int>(m_vNavigationRoute.size()); ++i)
			{
				if(m_vNavigationRoute[i].m_Teleport)
					break;
				if(distance(Pos, m_vNavigationRoute[i].m_Pos) <= 480 && NavigationClear(Pos, m_vNavigationRoute[i].m_Pos))
					m_NavigationIndex = i;
			}
			const float Dist = distance(Pos, m_vNavigationRoute[m_NavigationIndex].m_Pos);
			if(m_NavigationProgressIndex != m_NavigationIndex || Dist < m_NavigationBestDistance - 12)
			{
				m_NavigationProgressIndex = m_NavigationIndex;
				m_NavigationBestDistance = Dist;
				m_NavigationProgressTick = Now;
			}
			if(!NavigationClear(Pos, m_vNavigationRoute[m_NavigationIndex].m_Pos) ||
				Now - m_NavigationProgressTick > 150)
				Replan = true;
		}
	}
	if(m_NavigationSearch.m_State == EState::UNREACHABLE || m_NavigationSearch.m_State == EState::LIMIT)
		Replan = Replan || Now - m_NavigationPlanTick > 100;
	if(Replan)
	{
		if(m_NavigationTrace)
			dbg_msg("botnav", "PLAN cid=%d tick=%d start=%.2f,%.2f goal=%.2f,%.2f", m_pPlayer->GetCid(), Now, Pos.x, Pos.y, Goal.x, Goal.y);
		m_NavigationGoal = Goal;
		m_NavigationSearchStart = Pos;
		m_NavigationTargetId = TargetId;
		m_NavigationPlanTick = Now;
		m_NavigationControlTick = -1;
		m_NavigationProgressTick = Now;
		m_NavigationProgressIndex = -1;
		m_vNavigationRoute.clear();
		m_NavigationIndex = 0;
		m_NavigationHasTeleports = !BotEngine()->m_vTeleEntrances.empty();
		m_NavigationSearch.Start(Collision()->GetPureMapIndex(Pos), Collision()->GetPureMapIndex(Goal));
	}
	if(m_NavigationSearch.m_State == EState::SEARCHING)
	{
		const int Budget = BotEngine()->NavigationBudget(m_pPlayer->GetCid(), Now);
		if(Budget)
			StepNavigationSearch(m_NavigationSearch, Budget, m_NavigationHasTeleports);
		if(m_NavigationTrace && (m_NavigationSearch.m_State == EState::UNREACHABLE || m_NavigationSearch.m_State == EState::LIMIT))
			dbg_msg("botnav", "NO_ROUTE cid=%d status=%d expanded=%d", m_pPlayer->GetCid(), static_cast<int>(m_NavigationSearch.m_State), m_NavigationSearch.m_Expanded);
		if(m_NavigationSearch.m_State == EState::FOUND)
		{
			const auto &Route = m_NavigationSearch.m_vRoute;
			const int Width = Collision()->GetWidth();
			vec2 Anchor = Pos;
			for(int i = 1; i < static_cast<int>(Route.size());)
			{
				const auto Position = [&](int Index) {
					return Index == static_cast<int>(Route.size()) - 1 ? m_NavigationGoal :
						vec2(Route[Index].m_Cell % Width * 32 + 16.0f, Route[Index].m_Cell / Width * 32 + 16.0f);
				};
				int Best = i;
				if(!Route[i].m_Teleport)
				{
					for(int j = i; j < std::min(i + 12, static_cast<int>(Route.size())); ++j)
					{
						if(Route[j].m_Teleport) break;
						if(NavigationClear(Anchor, Position(j))) Best = j;
					}
					if(!NavigationClear(Anchor, Position(Best)))
					{ m_vNavigationRoute.clear(); m_NavigationSearch.Reset(); break; }
				}
				Anchor = Position(Best);
				m_vNavigationRoute.push_back({Anchor, Route[i].m_Teleport});
				i = Best + 1;
			}
			if(Route.size() == 1)
				m_vNavigationRoute.push_back({Goal, false});
			if(m_NavigationTrace)
			{
				dbg_msg("botnav", "ROUTE cid=%d expanded=%d points=%d ticks=%d", m_pPlayer->GetCid(),
					m_NavigationSearch.m_Expanded, static_cast<int>(m_vNavigationRoute.size()), Now - m_NavigationPlanTick);
				for(const auto &Point : m_vNavigationRoute)
					dbg_msg("botnav", "POINT %.2f %.2f portal=%d", Point.m_Pos.x, Point.m_Pos.y, Point.m_Teleport);
			}
		}
	}
	m_ComputeTarget.m_Pos = m_vNavigationRoute.empty() ? Pos : m_vNavigationRoute[m_NavigationIndex].m_Pos;
	return true;
}

void CBot::DriveNavigation()
{
	const CCharacterCore *pCore = m_pPlayer->GetCharacter()->Core();
	const vec2 Pos = pCore->m_Pos;
	const int Now = GameServer()->Server()->Tick();
	mem_zero(&m_InputData, sizeof(m_InputData));
	if(g_Config.m_SvBotDrawTarget)
		for(int &Id : m_aNavigationSnapIds)
			if(Id < 0) Id = GameServer()->Server()->SnapNewId().value_or(-1);
	m_InputData.m_TargetY = -1;
	m_pPlayer->to_fire = false;
	if(stay || !g_Config.m_SvBotAllowMove || m_vNavigationRoute.empty() ||
		m_NavigationIndex >= static_cast<int>(m_vNavigationRoute.size()))
		return;
	const vec2 Goal = m_vNavigationRoute[m_NavigationIndex].m_Pos;
	m_Target = Goal - Pos;
	m_RealTarget = Goal;
	const auto Input = [&](const CCharacterCore &Core, int Dir, bool Jump, bool Hook, vec2 Anchor) {
		CNetObj_PlayerInput Result{};
		const float DX = Goal.x - Core.m_Pos.x;
		Result.m_Direction = Dir == 2 ? (DX > 10 ? 1 : DX < -10 ? -1 :
			Core.m_Vel.x > 1 ? -1 : Core.m_Vel.x < -1 ? 1 : 0) : Dir;
		Result.m_Jump = bot_ai::JumpPressAllowed(Jump, Core.m_Jumped, Core.m_JumpedTotal, Core.m_Vel.y);
		Result.m_Hook = Hook && g_Config.m_SvBotAllowHook;
		const vec2 Aim = Hook ? Anchor - Core.m_Pos : Goal - Core.m_Pos;
		Result.m_TargetX = static_cast<int>(Aim.x);
		Result.m_TargetY = static_cast<int>(Aim.y);
		if(Result.m_TargetX == 0 && Result.m_TargetY == 0) Result.m_TargetY = -1;
		return Result;
	};
	if((m_NavigationControlTick < 0 || Now - m_NavigationControlTick >= 16 - Skill(PET_SKILL_RACE)) &&
		BotEngine()->NavigationMovementBudget(m_pPlayer->GetCid(), Now))
	{
		m_NavigationControlTick = Now;
		std::vector<float> Suffix(m_vNavigationRoute.size(), 0);
		for(int i = static_cast<int>(Suffix.size()) - 2; i >= 0; --i)
			Suffix[i] = Suffix[i + 1] + (m_vNavigationRoute[i + 1].m_Teleport ? 32.0f :
				distance(m_vNavigationRoute[i].m_Pos, m_vNavigationRoute[i + 1].m_Pos));
		float BestScore = std::numeric_limits<float>::infinity();
		const auto Evaluate = [&](int Dir, bool Jump, bool Hook, vec2 Anchor) {
			CWorldCore World;
			CCharacterCore Predicted = *pCore;
			Predicted.Init(&World, Collision());
			float Penalty = 0;
			int FreezeTicks = 0;
			const int Horizon = 16 + 2 * Skill(PET_SKILL_RACE);
			for(int Step = 0; Step < Horizon; ++Step)
			{
				Predicted.m_Input = Input(Predicted, Dir, Jump, Hook, Anchor);
				SimulateNavigationTick(Predicted, FreezeTicks);
				if(!NavigationClear(Predicted.m_Pos, Predicted.m_Pos))
				{ Penalty += 100000 + (Horizon - Step) * 100; break; }
			}
			float Remaining = distance(Predicted.m_Pos, Goal) + Suffix[m_NavigationIndex];
			for(int i = m_NavigationIndex + 1; i < std::min(m_NavigationIndex + 4, static_cast<int>(Suffix.size())); ++i)
			{
				if(m_vNavigationRoute[i].m_Teleport) break;
				if(NavigationClear(Predicted.m_Pos, m_vNavigationRoute[i].m_Pos))
					Remaining = std::min(Remaining, distance(Predicted.m_Pos, m_vNavigationRoute[i].m_Pos) + Suffix[i]);
			}
			const float Score = Remaining + Penalty + (Jump ? 1.5f : 0) + (Hook ? 2.0f : 0);
			if(Score < BestScore)
			{
				BestScore = Score;
				m_NavigationDirection = Dir;
				m_NavigationJump = Jump;
				m_NavigationHook = Hook;
				m_NavigationAnchor = Anchor;
			}
		};
		for(int Dir : {2, -1, 0, 1})
			for(bool Jump : {false, true})
				Evaluate(Dir, Jump, false, Pos);
		if(g_Config.m_SvBotAllowHook)
		{
			if(pCore->m_HookState == HOOK_GRABBED || pCore->m_HookState == HOOK_FLYING)
				for(bool Jump : {false, true}) Evaluate(2, Jump, true, m_NavigationAnchor);
			else if(pCore->m_HookState == HOOK_IDLE)
				for(int Ray = 0; Ray < 16; ++Ray)
				{
					const vec2 Aim = direction(Ray * 2 * pi / 16.0f);
					vec2 Hit;
					const int Tile = Collision()->FastIntersectLine(Pos, Pos + Aim * pCore->m_Tuning.m_HookLength, &Hit, nullptr);
					if(!Tile || Tile == TILE_NOHOOK || distance(Pos, Hit) < 40) continue;
					for(bool Jump : {false, true}) Evaluate(2, Jump, true, Hit + Aim * 2.0f);
				}
		}
	}
	if(pCore->m_HookState == HOOK_RETRACTED)
		m_NavigationHook = false;
	m_InputData = Input(*pCore, m_NavigationDirection, m_NavigationJump, m_NavigationHook, m_NavigationAnchor);
	if(m_NavigationTrace && Now % 10 == m_pPlayer->GetCid() % 10)
		dbg_msg("botnav", "MOVE cid=%d tick=%d pos=%.2f,%.2f vel=%.2f,%.2f waypoint=%.2f,%.2f index=%d dir=%d jump=%d hook=%d state=%d",
			m_pPlayer->GetCid(), Now, Pos.x, Pos.y, pCore->m_Vel.x, pCore->m_Vel.y, Goal.x, Goal.y,
			m_NavigationIndex, m_InputData.m_Direction, m_InputData.m_Jump, m_InputData.m_Hook, pCore->m_HookState);
}
