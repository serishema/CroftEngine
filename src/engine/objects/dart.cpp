#include "dart.h"

#include "engine/particle.h"
#include "laraobject.h"

namespace engine::objects
{
void Dart::collide(CollisionInfo& info)
{
  if(!isNear(getEngine().getLara(), info.collisionRadius))
    return;

  if(!testBoneCollision(getEngine().getLara()))
    return;

  if(!info.policyFlags.is_set(CollisionInfo::PolicyFlags::EnableBaddiePush))
    return;

  enemyPush(info, false, true);
}

void Dart::update()
{
  if(m_state.touch_bits != 0)
  {
    getEngine().getLara().m_state.health -= 50_hp;
    getEngine().getLara().m_state.is_hit = true;

    auto fx = createBloodSplat(getEngine(), m_state.position, m_state.speed, m_state.rotation.Y);
    getEngine().getParticles().emplace_back(fx);
  }

  ModelObject::update();

  auto room = m_state.position.room;
  const auto sector = findRealFloorSector(m_state.position.position, &room);
  if(room != m_state.position.room)
    setCurrentRoom(room);

  const HeightInfo h = HeightInfo::fromFloor(sector, m_state.position.position, getEngine().getObjects());
  m_state.floor = h.y;

  if(m_state.position.position.Y < m_state.floor)
    return;

  kill();

  const auto particle = std::make_shared<RicochetParticle>(m_state.position, getEngine());
  setParent(particle, m_state.position.room->node);
  particle->angle = m_state.rotation;
  particle->timePerSpriteFrame = 6;
  getEngine().getParticles().emplace_back(particle);
}
} // namespace engine