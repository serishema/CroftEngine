#include "aiagent.h"
#include "engine/heightinfo.h"
#include "level/level.h"

#include <boost/range/adaptors.hpp>

namespace engine
{
namespace items
{
core::Angle AIAgent::rotateTowardsTarget(core::Angle maxRotationSpeed)
{
    if( m_state.speed == 0 || maxRotationSpeed == 0_au )
    {
        return 0_au;
    }

    const auto dx = m_state.creatureInfo->target.X - m_state.position.position.X;
    const auto dz = m_state.creatureInfo->target.Z - m_state.position.position.Z;
    auto turnAngle = core::Angle::fromAtan( dx, dz ) - m_state.rotation.Y;
    if( turnAngle < -90_deg || turnAngle > 90_deg )
    {
        // the target is behind the current item, so we need a U-turn
        auto relativeSpeed = m_state.speed * (+90_deg).toAU() / maxRotationSpeed.toAU();
        if( util::square( dx ) + util::square( dz ) < util::square( relativeSpeed ) )
        {
            maxRotationSpeed /= 2;
        }
    }

    turnAngle = util::clamp(turnAngle, -maxRotationSpeed, maxRotationSpeed);

    m_state.rotation.Y += turnAngle;
    return turnAngle;
}

bool AIAgent::isPositionOutOfReach(const core::TRCoordinates& testPosition,
                                   int currentBoxFloor,
                                   int nextBoxFloor,
                                   const ai::LotInfo& lotInfo) const
{
    const auto sectorBoxIdx = getLevel().findRealFloorSector( testPosition, m_state.position.room )->boxIndex;
    if( sectorBoxIdx < 0 )
        return true;

    const auto& sectorBox = getLevel().m_boxes[sectorBoxIdx];
    if( lotInfo.block_mask & sectorBox.overlap_index )
        return true;

    const auto stepHeight = currentBoxFloor - sectorBox.floor;

    if( stepHeight > lotInfo.step || stepHeight < lotInfo.drop )
        return true;

    if( stepHeight < -lotInfo.step && sectorBox.floor > nextBoxFloor )
        return true;

    return lotInfo.fly != 0 && testPosition.Y > lotInfo.fly + sectorBox.floor;

}

bool AIAgent::anyMovingEnabledItemInReach() const
{
    for( const std::shared_ptr<ItemNode>& item : getLevel().m_itemNodes | boost::adaptors::map_values )
    {
        if( !item->m_isActive || item.get() == this )
            continue;

        if( item->m_state.triggerState == items::TriggerState::Enabled
            && item->m_state.speed != 0
            && item->m_state.position.position.distanceTo( m_state.position.position ) < m_collisionRadius )
        {
            return true;
        }
    }
    return false;
}

bool AIAgent::animateCreature(core::Angle angle, core::Angle tilt)
{
    const auto creatureInfo = m_state.creatureInfo;
    if ( creatureInfo == nullptr )
    {
        return false;
    }
    const auto& lotInfo = creatureInfo->lot;
    const auto oldPosition = m_state.position.position;
    const auto boxFloor = getLevel().m_boxes[m_state.box_number].floor;
    const loader::ZoneData* zoneData = nullptr;
    if ( creatureInfo->lot.fly != 0 )
    {
        zoneData = getLevel().roomsAreSwapped ? &getLevel().m_alternateZones.flyZone : &getLevel().m_baseZones.flyZone;
    }
    else if ( creatureInfo->lot.step == loader::QuarterSectorSize )
    {
        zoneData = getLevel().roomsAreSwapped ? &getLevel().m_alternateZones.groundZone1 : &getLevel().m_baseZones.groundZone1;
    }
    else
    {
        zoneData = getLevel().roomsAreSwapped ? &getLevel().m_alternateZones.groundZone2 : &getLevel().m_baseZones.groundZone2;
    }
    ModelItemNode::update();
    if ( m_state.triggerState == TriggerState::Activated )
    {
        m_state.health = -16384;
        m_state.collidable = false;
        m_state.creatureInfo.reset();
        deactivate();
        return false;
    }

    auto bbox = getSkeleton()->getBoundingBox(m_state);
    const auto bboxMinY = m_state.position.position.Y + bbox.minY;

    auto room = m_state.position.room;
    auto sector = getLevel().findRealFloorSector(m_state.position.position, &room);
    auto currentFloor = getLevel().m_boxes[sector->boxIndex].floor;

    int nextFloor;
    if ( lotInfo.nodes[sector->boxIndex].exit_box < 0 )
    {
        nextFloor = currentFloor;
    }
    else
    {
        nextFloor = getLevel().m_boxes[lotInfo.nodes[sector->boxIndex].exit_box].floor;
    }

    if ( sector->boxIndex < 0
         || (*zoneData)[m_state.box_number] != (*zoneData)[sector->boxIndex]
         || boxFloor - currentFloor > lotInfo.step
         || boxFloor - currentFloor < lotInfo.drop )
    {
        const auto oldSectorX = oldPosition.X / loader::SectorSize;
        const auto newSectorX = m_state.position.position.X / loader::SectorSize;
        const auto oldSectorZ = oldPosition.Z / loader::SectorSize;
        const auto newSectorZ = m_state.position.position.Z / loader::SectorSize;

        if( newSectorX < oldSectorX )
            m_state.position.position.X = oldPosition.X & ~0x3ff;
        else if( newSectorX > oldSectorX )
            m_state.position.position.X = oldPosition.X | 0x3ff;

        if( newSectorZ < oldSectorZ )
            m_state.position.position.Z = oldPosition.Z & ~0x3ff;
        else if( newSectorZ > oldSectorZ )
            m_state.position.position.Z = oldPosition.Z | 0x3ff;

        sector = getLevel().findRealFloorSector(
                core::TRCoordinates{m_state.position.position.X, bboxMinY, m_state.position.position.Z},
                &room );
        currentFloor = getLevel().m_boxes[sector->boxIndex].floor;
        if( lotInfo.nodes[sector->boxIndex].exit_box < 0 )
        {
            nextFloor = getLevel().m_boxes[sector->boxIndex].floor;
        }
        else
        {
            nextFloor = getLevel().m_boxes[lotInfo.nodes[sector->boxIndex].exit_box].floor;
        }
    }

    const auto basePosX = m_state.position.position.X;
    const auto basePosZ = m_state.position.position.Z;
    const auto inSectorX = m_state.position.position.X % loader::SectorSize;
    const auto inSectorZ = m_state.position.position.Z % loader::SectorSize;

    sol::table objectInfo = getLevel().m_scriptEngine["getObjectInfo"].call( m_state.object_number );
    const int radius = objectInfo["radius"];

    int moveZ = 0;
    int moveX = 0;
    if ( radius > inSectorZ )
    {
        if ( isPositionOutOfReach(core::TRCoordinates{basePosX, bboxMinY, basePosZ - radius}, currentFloor, nextFloor, lotInfo) )
        {
            moveZ = radius - inSectorZ;
        }

        if ( radius > inSectorX )
        {
            if ( isPositionOutOfReach(core::TRCoordinates{basePosX - radius, bboxMinY, basePosZ}, currentFloor, nextFloor, lotInfo) )
            {
                moveX = radius - inSectorX;
            }
            else if ( moveZ == 0 && isPositionOutOfReach(core::TRCoordinates{basePosX - radius, bboxMinY, basePosZ - radius}, currentFloor, nextFloor, lotInfo) )
            {
                if ( m_state.rotation.Y > -135_deg && m_state.rotation.Y < 45_deg )
                {
                    moveZ = radius - inSectorZ;
                }
                else
                {
                    moveX = radius - inSectorX;
                }
            }
        }
        else if( loader::SectorSize - radius < inSectorX )
        {
            if( isPositionOutOfReach( core::TRCoordinates{radius + basePosX, bboxMinY, basePosZ}, currentFloor,
                                      nextFloor, lotInfo ) )
            {
                moveX = loader::SectorSize - radius - inSectorX;
            }
            else if( moveZ == 0 && isPositionOutOfReach( core::TRCoordinates{radius + basePosX, bboxMinY, basePosZ - radius}, currentFloor, nextFloor, lotInfo ) )
            {
                if( m_state.rotation.Y > -45_deg && m_state.rotation.Y < 135_deg )
                {
                    moveZ = radius - inSectorZ;
                }
                else
                {
                    moveX = loader::SectorSize - radius - inSectorX;
                }
            }
        }
    }
    else if ( loader::SectorSize - radius < inSectorZ )
    {
        if ( isPositionOutOfReach(core::TRCoordinates{basePosX, bboxMinY, radius + basePosZ}, currentFloor, nextFloor, lotInfo) )
        {
            moveZ = loader::SectorSize - radius - inSectorZ;
        }

        if( inSectorX < radius )
        {
            if( isPositionOutOfReach( core::TRCoordinates{basePosX - radius, bboxMinY, basePosZ}, currentFloor,
                                      nextFloor, lotInfo ) )
            {
                moveX = radius - inSectorX;
            }
            else if( isPositionOutOfReach( core::TRCoordinates{basePosX - radius, bboxMinY, radius + basePosZ},
                                           currentFloor, nextFloor, lotInfo ) )
            {
                if( moveZ == 0 && m_state.rotation.Y > -45_deg && m_state.rotation.Y < 135_deg )
                {
                    moveX = radius - inSectorX;
                }
            }
        }
        else if( inSectorX > loader::SectorSize - radius )
        {
            if( isPositionOutOfReach( core::TRCoordinates{radius + basePosX, bboxMinY, basePosZ}, currentFloor,
                                      nextFloor, lotInfo ) )
            {
                moveX = loader::SectorSize - radius - inSectorX;
            }
            else if( isPositionOutOfReach( core::TRCoordinates{radius + basePosX, bboxMinY, radius + basePosZ},
                                           currentFloor, nextFloor, lotInfo ) )
            {
                if( moveZ == 0 && m_state.rotation.Y > -135_deg && m_state.rotation.Y < 45_deg )
                {
                    moveX = loader::SectorSize - radius - inSectorX;
                }
            }
        }
        else
        {
            moveZ = loader::SectorSize - radius - inSectorZ;
        }
    }
    else if ( radius > inSectorX && isPositionOutOfReach( core::TRCoordinates{basePosX - radius, bboxMinY, basePosZ}, currentFloor, nextFloor, lotInfo ) )
    {
        moveX = radius - inSectorX;
    }
    else if ( inSectorX > loader::SectorSize - radius && isPositionOutOfReach(core::TRCoordinates{radius + basePosX, bboxMinY, basePosZ}, currentFloor, nextFloor, lotInfo) )
    {
        moveX = loader::SectorSize - radius - inSectorX;
    }

    m_state.position.position.X += moveX;
    m_state.position.position.Z += moveZ;
    if ( moveX != 0 || moveZ != 0 )
    {
        sector = getLevel().findRealFloorSector(core::TRCoordinates{m_state.position.position.X, bboxMinY, m_state.position.position.Z}, &room);
        m_state.rotation.Y += angle;
        m_state.rotation.Z += util::clamp(core::Angle{gsl::narrow<int16_t>(8 * tilt.toAU())} - m_state.rotation.Z, -3_deg, +3_deg);
    }
    if ( !anyMovingEnabledItemInReach() )
    {
        if ( lotInfo.fly != 0 )
        {
            auto moveY = util::clamp<int>(creatureInfo->target.Y - m_state.position.position.Y, -lotInfo.fly, lotInfo.fly);
            const auto floor = HeightInfo::fromFloor(sector, core::TRCoordinates{m_state.position.position.X, bboxMinY, m_state.position.position.Z}, getLevel().m_cameraController).distance;
            if ( moveY + m_state.position.position.Y <= floor )
            {
                if ( m_state.object_number == 11 )
                {
                    bbox.minY = 0;
                }

                if ( m_state.position.position.Y + bbox.minY + moveY < floor )
                {
                    if ( m_state.position.position.Y + bbox.minY >= floor )
                    {
                        moveY = 0;
                    }
                    else
                    {
                        m_state.position.position.X = oldPosition.X;
                        m_state.position.position.Z = oldPosition.Z;
                        moveY = lotInfo.fly;
                    }
                }
            }
            else if ( floor >= m_state.position.position.Y )
            {
                moveY = 0;
                m_state.position.position.Y = floor;
            }
            else
            {
                m_state.position.position.X = oldPosition.X;
                m_state.position.position.Z = oldPosition.Z;
                moveY = -lotInfo.fly;
            }
            m_state.position.position.Y += moveY;
            sector = getLevel().findRealFloorSector(core::TRCoordinates{m_state.position.position.X, bboxMinY, m_state.position.position.Z}, &room);
            m_state.floor = HeightInfo::fromFloor(sector, core::TRCoordinates{m_state.position.position.X, bboxMinY, m_state.position.position.Z}, getLevel().m_cameraController).distance;
            core::Angle yaw{0};
            if ( m_state.speed != 0 )
                yaw = core::Angle::fromAtan(-moveY, m_state.speed);

            if( yaw < m_state.rotation.X - 1_deg )
                m_state.rotation.X -= 1_deg;
            else if( yaw > m_state.rotation.X + 1_deg )
                m_state.rotation.X += 1_deg;
            else
                m_state.rotation.X = yaw;

            setCurrentRoom(room);
            return true;
        }

        if( m_state.position.position.Y > m_state.floor )
        {
            m_state.position.position.Y = m_state.floor;
        }
        else if( m_state.floor - m_state.position.position.Y > 64 )
        {
            m_state.position.position.Y += 64;
        }
        else if( m_state.position.position.Y < m_state.floor )
        {
            m_state.position.position.Y = m_state.floor;
        }

        m_state.rotation.X = 0_au;
        sector = getLevel().findRealFloorSector(m_state.position.position, &room);
        m_state.floor = HeightInfo::fromFloor(
                sector,
                m_state.position.position, getLevel().m_cameraController).distance;
        setCurrentRoom(room);
        return true;
    }
    m_state.position.position = oldPosition;
    return true;
}

AIAgent::AIAgent(const gsl::not_null<level::Level*>& level,
                 const std::string& name,
                 const gsl::not_null<const loader::Room*>& room,
                 const loader::Item& item,
                 ItemNode::Characteristics characteristics,
                 const loader::SkeletalModelType& animatedModel,
                 int collisionRadius)
        : ModelItemNode( level, name, room, item, true, characteristics, animatedModel )
        , m_collisionRadius{collisionRadius}
{
    m_state.collidable = true;
    const core::Angle v = core::Angle( std::rand() % 65536 );
    m_state.rotation.Y += v;
}
}
}
