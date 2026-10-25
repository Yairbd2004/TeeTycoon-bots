#ifndef GAME_SERVER_BOT_AI_NAVIGATION_H
#define GAME_SERVER_BOT_AI_NAVIGATION_H

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <vector>

namespace bot_ai
{
// Sparse, resumable graph search. Coordinates are map tile indices, not an
// allocation sized to the whole map. The adapter supplies legal movement edges.
class CNavigationSearch
{
public:
	struct SStep { int m_Cell; bool m_Teleport; };
	enum class EState { IDLE, SEARCHING, FOUND, UNREACHABLE, LIMIT };
	struct SNode
	{
		float m_Cost = std::numeric_limits<float>::infinity();
		int m_Parent = -1;
		bool m_Closed = false;
		bool m_Teleport = false;
	};
	struct SOpen
	{
		float m_Estimate;
		float m_Cost;
		int m_Cell;
		bool operator<(const SOpen &Other) const
		{
			return m_Estimate != Other.m_Estimate ? m_Estimate > Other.m_Estimate : m_Cell > Other.m_Cell;
		}
	};
	EState m_State = EState::IDLE;
	int m_Start = -1;
	int m_Goal = -1;
	int m_Expanded = 0;
	std::vector<SStep> m_vRoute;
	std::unordered_map<int, unsigned char> m_CellFlags;
	std::unordered_map<int, int> m_PortalLandings;

	void Reset()
	{
		m_State = EState::IDLE;
		m_Nodes.clear();
		m_Open = {};
		m_CellFlags.clear();
		m_PortalLandings.clear();
		m_vRoute.clear();
		m_Expanded = 0;
	}
	void Start(int StartCell, int GoalCell)
	{
		Reset();
		m_Start = StartCell;
		m_Goal = GoalCell;
		m_Nodes[StartCell].m_Cost = 0;
		m_Open.push({0, 0, StartCell});
		m_State = EState::SEARCHING;
	}
	// A slice has a fixed work allowance; exhaustion preserves the frontier.
	// Never publish a partial route as though it reached the requested goal.
	template<typename TNeighbors, typename THeuristic>
	void Step(int Budget, TNeighbors Neighbors, THeuristic Heuristic)
	{
		while(m_State == EState::SEARCHING && Budget-- > 0)
		{
			if(m_Open.empty()) { m_State = EState::UNREACHABLE; break; }
			const SOpen Entry = m_Open.top();
			m_Open.pop();
			SNode &Node = m_Nodes.at(Entry.m_Cell);
			if(Node.m_Closed || Entry.m_Cost != Node.m_Cost)
				continue;
			Node.m_Closed = true;
			++m_Expanded;
			if(Entry.m_Cell == m_Goal)
			{
				for(int Cell = m_Goal; Cell != -1; Cell = m_Nodes.at(Cell).m_Parent)
					m_vRoute.push_back({Cell, m_Nodes.at(Cell).m_Teleport});
				std::reverse(m_vRoute.begin(), m_vRoute.end());
				m_State = EState::FOUND;
				break;
			}
			Neighbors(Entry.m_Cell, [&](int Next, float Cost, bool Teleport) {
				if(m_Nodes.size() >= 32768 || m_Open.size() >= 65536)
				{ m_State = EState::LIMIT; return; }
				const float NewCost = Entry.m_Cost + std::max(0.01f, Cost);
				SNode &NextNode = m_Nodes[Next];
				if(NewCost >= NextNode.m_Cost)
					return;
				NextNode.m_Cost = NewCost;
				NextNode.m_Parent = Entry.m_Cell;
				NextNode.m_Teleport = Teleport;
				NextNode.m_Closed = false;
				m_Open.push({NewCost + Heuristic(Next), NewCost, Next});
			});
		}
	}

private:
	std::unordered_map<int, SNode> m_Nodes;
	std::priority_queue<SOpen> m_Open;
};
}
#endif
