#pragma once
#include "goal.h"
#include <list>
#include <memory>
#include <functional>

class GoalManager
{
    typedef std::unique_ptr<Goal> GoalPtr;
    
    GoalPtr m_current;
    
public:
}
