#include "goalManager.h"
#include "goalDefendHelicopter.h"
#include "GoalDefendTank.h"
#include "GoalDefendIfv.h"
#include "GoalMixTanksAndHealers.h"
#include "state.h"

void GoalManager::fillCurrentGoals()
{
    // use sorted list because std::multimap looks like overengieneering

    if (m_state.world()->getTickIndex() == 0)
    {
        m_currentGoals.emplace_back(0, std::make_unique<goals::MixTanksAndHealers>(m_state));
        m_currentGoals.emplace_back(1, std::make_unique<goals::DefendHelicopters>(m_state));
        m_currentGoals.emplace_back(2, std::make_unique<goals::GoalDefendTank>(m_state));
        m_currentGoals.emplace_back(3, std::make_unique<goals::GoalDefendIfv>(m_state));

        m_currentGoals.sort();
    }

    assert(std::is_sorted(m_currentGoals.begin(), m_currentGoals.end()) && "please keep goals sorted by-priority");
}

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
    fillCurrentGoals();

    if (!m_currentGoals.empty())
    {
        const GoalPtr& mostPriority = m_currentGoals.front().m_goal;
        mostPriority->performStep(*this, false);
        if (!mostPriority->inProgress())
            m_currentGoals.pop_front();
    }
    else
    {
        NukeGoal dummy(m_state);
        dummy.performStep(*this, false);
    }
}

void GoalManager::doMultitasking(const Goal* interruptedGoal)
{
    for (const GoalHolder& goalHolder : m_currentGoals)
    {
        if (!goalHolder.m_goal->isEligibleForBackgroundMode(interruptedGoal))
            continue;

        goalHolder.m_goal->performStep(*this, true);
        if (m_state.isMoveCommitted())
            break;   // one tick - one move
    }
}

