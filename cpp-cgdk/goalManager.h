#pragma once
#include <map>
#include <memory>

#include "model/Player.h"
#include "model/World.h"
#include "model/Game.h"
#include "model/Move.h"

#include "goal.h"
#include "forwardDeclarations.h"

class GoalManager
{
    typedef std::unique_ptr<Goal> GoalPtr;

    State&             m_state;
    std::list<GoalPtr> m_currentGoals;

    void fillCurrentGoals();

public:
    explicit GoalManager(State& state);

    void tick();
};


