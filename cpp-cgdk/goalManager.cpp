#include "goalManager.h"
#include "goalDefendHelicopter.h"
#include "state.h"

void GoalManager::tick()
{
    if (m_state.world()->getTickIndex() == 0)
    {
        m_currentGoals.emplace_back(std::make_unique<goals::DefendHelicopters>(m_state));
    }

    if (!m_currentGoals.empty())
    {
        m_currentGoals.front()->performStep();
        if (!m_currentGoals.front()->inProgress())
            m_currentGoals.pop_front();
    }
}

