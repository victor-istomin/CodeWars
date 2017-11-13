#include "goalManager.h"
#include "goalDefendHelicopter.h"
#include "GoalDefendTank.h"
#include "GoalDefendIfv.h"
#include "state.h"

GoalManager::GoalManager(State& state) : m_state(state)
{

}

struct NukeGoal : public Goal
{
    NukeGoal(State& s)
        : Goal(s)
    {
        pushNextStep([]() {return false; }, []() {return true; }, []() {return true; }, "ensure nuke may be launched");
    }
};

void GoalManager::tick()
{
    if (m_state.world()->getTickIndex() == 0)
    {
        m_currentGoals.emplace_back(std::make_unique<goals::DefendHelicopters>(m_state));
        m_currentGoals.emplace_back(std::make_unique<goals::GoalDefendTank>(m_state));
        m_currentGoals.emplace_back(std::make_unique<goals::GoalDefendIfv>(m_state));
    }

    if (!m_currentGoals.empty())
    {
        m_currentGoals.front()->performStep();
        if (!m_currentGoals.front()->inProgress())
            m_currentGoals.pop_front();
    }
    else
    {
        NukeGoal dummy(m_state);
        dummy.performStep();
    }
}

