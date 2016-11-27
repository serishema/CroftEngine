#pragma once

#include "statehandler_onwater.h"

namespace engine
{
    namespace lara
    {
        class StateHandler_33 final : public StateHandler_OnWater
        {
        public:
            explicit StateHandler_33(LaraNode& lara)
                    : StateHandler_OnWater(lara)
            {
            }

            std::unique_ptr<AbstractStateHandler> handleInputImpl(CollisionInfo& /*collisionInfo*/) override
            {
                if( getHealth() <= 0 )
                {
                    setTargetState(LaraStateId::WaterDeath);
                    return nullptr;
                }

                if( getLevel().m_inputHandler->getInputState().zMovement == AxisMovement::Forward )
                    setTargetState(LaraStateId::OnWaterForward);
                else if( getLevel().m_inputHandler->getInputState().zMovement == AxisMovement::Backward )
                    setTargetState(LaraStateId::OnWaterBackward);

                if( getLevel().m_inputHandler->getInputState().stepMovement == AxisMovement::Left )
                    setTargetState(LaraStateId::OnWaterLeft);
                else if( getLevel().m_inputHandler->getInputState().stepMovement == AxisMovement::Right )
                    setTargetState(LaraStateId::OnWaterRight);

                if( !getLevel().m_inputHandler->getInputState().jump )
                {
                    setSwimToDiveKeypressDuration(std::chrono::microseconds::zero());
                    return nullptr;
                }

                if(!getSwimToDiveKeypressDuration())
                    return nullptr; // not allowed to dive at all

                if(*getSwimToDiveKeypressDuration() < 10_frame)
                    return nullptr; // not yet allowed to dive

                setTargetState(LaraStateId::UnderwaterForward);
                playAnimation(loader::AnimationId::FREE_FALL_TO_UNDERWATER_ALTERNATE, 2041);
                setXRotation(-45_deg);
                setFallSpeed(core::makeInterpolatedValue(80.0f));
                setUnderwaterState(UnderwaterState::Diving);
                return createWithRetainedAnimation(LaraStateId::UnderwaterDiving);
            }

            void animateImpl(CollisionInfo& /*collisionInfo*/, const std::chrono::microseconds& deltaTimeMs) override
            {
                if(getLevel().m_inputHandler->getInputState().freeLook)
                {
                    getLevel().m_cameraController->setCamOverrideType(2);
                    getLevel().m_cameraController->addHeadRotationXY(
                            -FreeLookMouseMovementScale * (getLevel().m_inputHandler->getInputState().mouseMovement.y/2000),
                            FreeLookMouseMovementScale * (getLevel().m_inputHandler->getInputState().mouseMovement.x/2000)
                    );

                    getLevel().m_cameraController->setTorsoRotation(getLevel().m_cameraController->getHeadRotation());
                }
                else if(getLevel().m_cameraController->getCamOverrideType() == 2)
                {
                    getLevel().m_cameraController->setCamOverrideType(0);
                }

                setFallSpeed(std::max(core::makeInterpolatedValue(0.0f), getFallSpeed() - core::makeInterpolatedValue(4.0f).getScaled(deltaTimeMs)));

                if( getLevel().m_inputHandler->getInputState().xMovement == AxisMovement::Left )
                    m_yRotationSpeed = -4_deg;
                else if( getLevel().m_inputHandler->getInputState().xMovement == AxisMovement::Right )
                    m_yRotationSpeed = 4_deg;
                else
                    m_yRotationSpeed = 0_deg;

                addSwimToDiveKeypressDuration(deltaTimeMs);
            }

            std::unique_ptr<AbstractStateHandler> postprocessFrame(CollisionInfo& collisionInfo) override
            {
                setMovementAngle(getRotation().Y);
                return commonOnWaterHandling(collisionInfo);
            }

            loader::LaraStateId getId() const noexcept override
            {
                return LaraStateId::OnWaterStop;
            }
        };
    }
}
