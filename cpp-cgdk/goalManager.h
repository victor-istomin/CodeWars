#pragma once
#include <list>
#include <memory>

#include "model/Player.h"
#include "model/World.h"
#include "model/Game.h"
#include "model/Move.h"

#include "forwardDeclarations.h"
#include "goal.h"

class GoalManager
{
public:
    typedef std::unique_ptr<Goal> GoalPtr;

    struct GoalHolder
    {
        const int m_priority;
        GoalPtr   m_goal;

        GoalHolder(int priority, GoalPtr&& goal) : m_priority(priority), m_goal(std::move(goal)) {}

        bool operator<(const GoalHolder& right)
        {
            return m_priority < right.m_priority
                || (m_priority == right.m_priority && m_goal.get() < right.m_goal.get());
        }
    };

    typedef std::list<GoalHolder> Goals;

private:

    State& m_state;
    Goals  m_currentGoals;
	Goal*  m_forcedGoal;

    void fillCurrentGoals();

public:
    explicit GoalManager(State& state);

    void tick();
    void doMultitasking(const Goal* interruptedGoal);

    const Goals& currentGoals() const { return m_currentGoals; }
};


