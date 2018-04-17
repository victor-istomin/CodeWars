#include "goalManager.h"
#include "goalDefendHelicoptersFromRush.h"
#include "GoalDefendTank.h"
#include "GoalDefendIfv.h"
#include "GoalMixTanksAndHealers.h"
#include "GoalRushWithAircraft.h"
#include "GoalCaptureNearFacility.h"
#include "GoalDefendCapturers.h"
#include "GoalProduceVehicles.h"
#include "state.h"

void GoalManager::fillCurrentGoals()
{
    // use sorted list because std::multimap looks like overengieneering

    if (m_state.world()->getTickIndex() == 0)
    {
        int priority = 0;
        int backPriority = 256;

        m_currentGoals.emplace_back(priority++, std::make_unique<goals::MixTanksAndHealers>(m_state, *this));
        m_currentGoals.emplace_back(priority++, std::make_unique<goals::DefendHelicoptersFromRush>(m_state, *this));
        m_currentGoals.emplace_back(priority++, std::make_unique<goals::GoalDefendTank>(m_state, *this));
        m_currentGoals.emplace_back(priority++, std::make_unique<goals::GoalDefendIfv>(m_state, *this));

        if (m_state.areFacilitiesEnabled())
        {
            m_currentGoals.emplace_back(priority++, std::make_unique<goals::ProduceVehicles>(m_state, *this));
            m_currentGoals.emplace_back(priority++, std::make_unique<goals::CaptureNearFacility>(m_state, *this));
            m_currentGoals.emplace_back(priority++, std::make_unique<goals::DefendCapturers>(m_state, *this));
        }
        
        m_currentGoals.emplace_back(priority++, std::make_unique<goals::RushWithAircraft>(m_state, *this));

        m_currentGoals.sort();
    }

    if (!m_waitingInsetrion.empty())
    {
        // can't reorder goals if there is any goal which can't be paused right now
        auto isGoalBusy = [](const GoalHolder& holder) { return !holder.m_goal->canPause(); };
        bool isManagerBusy = std::find_if(m_currentGoals.begin(), m_currentGoals.end(), isGoalBusy) != m_currentGoals.end();

        if (!isManagerBusy)
        {
            m_currentGoals.splice(m_currentGoals.end(), m_waitingInsetrion);
        }

        m_currentGoals.sort();
    }

    assert(std::is_sorted(m_currentGoals.begin(), m_currentGoals.end()) && "please keep goals sorted by-priority");
}

GoalManager::GoalManager(State& state) 
    : m_state(state)
    , m_forcedGoal(nullptr)
{

}

struct NukeGoal : public Goal
{
    NukeGoal(State& s, GoalManager& goalManager)
        : Goal(s, goalManager)
    {
        pushNextStep([]() {return false; }, []() {return true; }, []() {return true; }, "ensure nuke may be launched");
    }
};

void GoalManager::tick()
{
    fillCurrentGoals();

    if (m_forcedGoal)
    {
        m_forcedGoal->performStep(*this, true);

        if (m_forcedGoal->isFinished())
        {
            auto forcedIt = std::find_if(m_currentGoals.begin(), m_currentGoals.end(), 
                [this](const GoalHolder& holder) { return holder.m_goal.get() == m_forcedGoal; });
            assert(forcedIt != m_currentGoals.end());

            m_forcedGoal = nullptr;           // done, pause and remove
            m_currentGoals.erase(forcedIt);
        }

        if (m_forcedGoal && m_forcedGoal->canPause())
            m_forcedGoal = nullptr;           // just pause
    }

    if (!m_state.isMoveCommitted())
    {
        if (!m_currentGoals.empty())
        {
            const GoalPtr& mostPriority = m_currentGoals.front().m_goal;
            mostPriority->performStep(*this, false);
            if (mostPriority->isFinished())
            {
                if (m_forcedGoal == m_currentGoals.front().m_goal.get())
                    m_forcedGoal = nullptr;

                m_currentGoals.pop_front();
            }
        }
        else
        {
            NukeGoal dummy(m_state, *this);
            dummy.performStep(*this, false);
        }
    }
}

void GoalManager::doMultitasking(const Goal* interruptedGoal)
{
    Goal* executedGoal = nullptr;

    for (const GoalHolder& goalHolder : m_currentGoals)
    {
        const GoalPtr& goal = goalHolder.m_goal;
        if (!goal->isEligibleForBackgroundMode(interruptedGoal))
            continue;

        goal->performStep(*this, true);
        if (m_state.isMoveCommitted())
        {
            executedGoal = goal.get();
            break;   // one tick - one move
        }
    }

    if (executedGoal && !executedGoal->isFinished() && !executedGoal->canPause())
        m_forcedGoal = executedGoal;

    // purge finished goals
    m_currentGoals.remove_if([](const GoalHolder& holder) { return holder.m_goal->isFinished(); });
}

