#include "MyStrategy.h"

#define PI 3.14159265358979323846
#define _USE_MATH_DEFINES

#include <cmath>
#include <cstdlib>

using namespace model;
using namespace std;

void MyStrategy::move(const Player& me, const World& world, const Game& game, Move& move) 
{
    m_state.update(world, me, game, move);

    m_goalManager.tick();
}

MyStrategy::MyStrategy()
    : m_state()
    , m_goalManager(m_state)
{ 
}
