#pragma once

#include "statehandler_onwater.h"

namespace engine::lara
{
class StateHandler_47 final : public StateHandler_OnWater
{
public:
  explicit StateHandler_47(objects::LaraObject& lara)
      : StateHandler_OnWater{lara, LaraStateId::OnWaterBackward}
  {
  }

  void handleInput(CollisionInfo& /*collisionInfo*/) override
  {
    if(getLara().isDead())
    {
      setGoalAnimState(LaraStateId::WaterDeath);
      return;
    }

    setSwimToDiveKeypressDuration(0_frame);

    const auto& inputHandler = getWorld().getPresenter().getInputHandler();
    if(inputHandler.getInputState().xMovement == hid::AxisMovement::Left)
    {
      getLara().m_state.rotation.Y -= core::OnWaterMovementTurnSpeed * 1_frame;
    }
    else if(inputHandler.getInputState().xMovement == hid::AxisMovement::Right)
    {
      getLara().m_state.rotation.Y += core::OnWaterMovementTurnSpeed * 1_frame;
    }

    if(inputHandler.getInputState().zMovement != hid::AxisMovement::Backward)
    {
      setGoalAnimState(LaraStateId::OnWaterStop);
    }

    getLara().m_state.fallspeed
      = std::min(core::OnWaterMaxSpeed, getLara().m_state.fallspeed + core::OnWaterAcceleration * 1_frame);
  }

  void postprocessFrame(CollisionInfo& collisionInfo) override
  {
    setMovementAngle(getLara().m_state.rotation.Y + 180_deg);
    commonOnWaterHandling(collisionInfo);
  }
};
} // namespace engine::lara
