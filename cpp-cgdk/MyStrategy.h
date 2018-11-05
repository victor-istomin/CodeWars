#pragma once

#ifndef _MY_STRATEGY_H_
#define _MY_STRATEGY_H_

#include "Strategy.h"
#include "state.h"
#include "goalManager.h"
#include "DebugOut.h"

class MyStrategy : public Strategy {
public:
    MyStrategy();

    void move(const model::Player& me, const model::World& world, const model::Game& game, model::Move& move) override;

private:
    State       m_state;
    GoalManager m_goalManager;
    DebugOut    m_debug;
};

#endif
