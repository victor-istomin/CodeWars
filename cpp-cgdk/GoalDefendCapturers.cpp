#include <limits>

#include "GoalDefendCapturers.h"
#include "GoalCaptureNearFacility.h"
#include "goalUtils.h"

using namespace goals;
using namespace model;

DefendCapturers::DefendCapturers(State& worldState, GoalManager& goalManager)
    : Goal(worldState, goalManager)
{
    pushBackStep([this]() { return shouldAbort(); }, 
                 [this]() { return hasActionPoints(); },
                 [this]() { return moveHelicopters(); }, "start defense", StepType::ALLOW_MULTITASK);
}

DefendCapturers::~DefendCapturers()
{
}

bool DefendCapturers::isCompatibleWith(const Goal* interrupted)
{
    return nullptr != dynamic_cast<const CaptureNearFacility*>(interrupted);
}

bool DefendCapturers::shouldAbort()
{
    return helicopterGroup().m_units.empty()
        || (tankGroup().m_units.empty() && arrvGroup().m_units.empty() && ifvGroup().m_units.empty());
}

bool DefendCapturers::moveHelicopters()
{
    ProtectionTarget targetInfo = getProtectionTarget();
    if (targetInfo.m_teammate == nullptr)
        return false;    // nothing to protect, aborting

    const VehicleGroup& helicopters = helicopterGroup();
    
    Point target     = getProtectionPoint(targetInfo);
    Vec2d moveVector = target - helicopters.m_center;

    static const double k_far = 2 * std::pow(state().game()->getHelicopterVisionRange(), 2);

    const double k_minStep = targetInfo.m_squareDistance > k_far ? 100 : 1;

    if (moveVector.length() > k_minStep)
    {
        state().setSelectAction(helicopters);

        pushBackStep([this]() { return shouldAbort(); },
                     [this]() { return hasActionPoints(); },
                     [this, moveVector]() { state().setMoveAction(moveVector);  return true; }, "do defense move");
    }

    const int ticksToWait = std::max(10, static_cast<int>(moveVector.length() / state().game()->getHelicopterSpeed() / 2));

    pushBackStep([this]() { return shouldAbort(); }, WaitSomeTicks(state(), ticksToWait), DoNothing(), "wait next move", StepType::ALLOW_MULTITASK);

    pushBackStep([this]() { return shouldAbort(); },
                 [this]() { return hasActionPoints(); },
                 [this]() { return moveHelicopters(); }, "continue defense", StepType::ALLOW_MULTITASK);

    return true;
}

DefendCapturers::ProtectionTarget DefendCapturers::getProtectionTarget() const
{
    std::vector<ProtectionTarget> alternatives;
    alternatives.reserve(state().teammates().size() * state().alliens().size());

    for (const auto& idTeammatePair : state().teammates())
    {
        const auto& id = idTeammatePair.first;
        if (id == VehicleType::FIGHTER || id == VehicleType::HELICOPTER || idTeammatePair.second.m_units.empty())
            continue;

        for (const auto& idAllienPair : state().alliens())
        {
            if (idAllienPair.second.m_units.empty())
                continue;

            alternatives.emplace_back(idTeammatePair.second, idAllienPair.second);
        }
    }

    std::sort(alternatives.begin(), alternatives.end(), [](const ProtectionTarget& a, const ProtectionTarget& b) { return a.m_squareDistance < b.m_squareDistance; });

    return alternatives.empty() ? ProtectionTarget() : alternatives.front();
}

DefendCapturers::ProtectionTarget::ProtectionTarget(const VehicleGroup& teammate, const VehicleGroup& alliens) 
    : m_teammate(&teammate)
    , m_alliens(&alliens)
    , m_squareDistance(std::numeric_limits<double>::max())
{
    const Point& myCenter = teammate.m_center;

    m_squareDistance = std::min({
        m_squareDistance,
        myCenter.getSquareDistance(alliens.m_center),
        myCenter.getSquareDistance(alliens.m_rect.m_topLeft),
        myCenter.getSquareDistance(alliens.m_rect.m_bottomRight),
        myCenter.getSquareDistance(alliens.m_rect.bottomLeft()),
        myCenter.getSquareDistance(alliens.m_rect.topRight())
    });
}

Point DefendCapturers::getProtectionPoint(const ProtectionTarget& protectionInfo) const
{
    static const double k_far = 2 * std::pow(state().game()->getHelicopterVisionRange(), 2);

    const VehicleGroup* teammate    = protectionInfo.m_teammate;
    const VehicleGroup* alliens     = protectionInfo.m_alliens;
    const VehicleGroup& helicopters = helicopterGroup();
    const VehicleGroup& fighters    = fighterGroup();

    Point moveTarget = teammate->m_center;
    bool isDangerousForDefender = !alliens->m_units.empty() && alliens->m_units.front().lock()->getType() == VehicleType::IFV;
    if (protectionInfo.m_squareDistance > k_far || isDangerousForDefender)
        return moveTarget;

    Point teammatePoints[] = {
        teammate->m_center,
        teammate->m_rect.m_topLeft,     teammate->m_rect.topRight(),
        teammate->m_rect.m_bottomRight, teammate->m_rect.bottomLeft()    // TODO: add more point on perimeter?
    };

    Point allienPoints[] = {
        alliens->m_center,
        alliens->m_rect.m_topLeft,     alliens->m_rect.topRight(),
        alliens->m_rect.m_bottomRight, alliens->m_rect.bottomLeft()
    };

    struct Nearest
    {
        Point  m_point            = Point();
        double m_squareDistance   = std::numeric_limits<double>::max();

        void apply(const Point& teammate, const Point& allien, const VehicleGroup& helicopters, const VehicleGroup& fighters)
        {
            double sqDistance = teammate.getSquareDistance(allien);

            Vec2d displacement = teammate - helicopters.m_center;
            VehicleGroupGhost helicopterGhost{ helicopters, displacement };

            if (helicopterGhost.m_rect.overlaps(fighters.m_rect))
                sqDistance *= 10;   // huge penalty. TODO: better mid-air collision avoidance

            if (sqDistance < m_squareDistance)
            {
                m_point = teammate;
                m_squareDistance = sqDistance;
            }
        }
    };

    // TODO: avoid aircraft collisions

    Nearest nearest;
    for (const Point& myPoint : teammatePoints)
        for (const Point& enemyPoint : allienPoints)
            nearest.apply(myPoint, enemyPoint, helicopters, fighters);

    return nearest.m_point;
}
