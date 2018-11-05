#include "MyStrategy.h"

#define PI 3.14159265358979323846
#define _USE_MATH_DEFINES

#include <cmath>
#include <cstdlib>

using namespace model;
using namespace std;

void MyStrategy::move(const Player& me, const World& world, const Game& game, Move& move) 
{
    m_state.updateBeforeMove(world, me, game, move);

    m_goalManager.tick();

    m_state.updateAfterMove(world, me, game, move);
    m_debug.drawVehicles(m_state.getAllVehicles(), me);
    m_debug.commitFrame();
}

MyStrategy::MyStrategy()
    : m_state()
    , m_goalManager(m_state)
{ 
}
