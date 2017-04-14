#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"
#include "engine/inputstate.h"
#include "level/level.h"

namespace engine
{
    namespace lara
    {
        class StateHandler_0 final : public AbstractStateHandler
        {
        public:

            explicit StateHandler_0(LaraNode& lara)
                    : AbstractStateHandler(lara, LaraStateId::WalkForward)
            {
            }


            void handleInputImpl(CollisionInfo& /*collisionInfo*/, const std::chrono::microseconds& deltaTime) override
            {
                if( getHealth() <= 0 )
                {
                    setTargetState(LaraStateId::Stop);
                    return;
                }

                if( getLevel().m_inputHandler->getInputState().zMovement == AxisMovement::Forward )
                {
                    if( getLevel().m_inputHandler->getInputState().moveSlow )
                        setTargetState(LaraStateId::WalkForward);
                    else
                        setTargetState(LaraStateId::RunForward);
                }
                else
                {
                    setTargetState(LaraStateId::Stop);
                }
            }

            void animateImpl(CollisionInfo& /*collisionInfo*/, const std::chrono::microseconds& deltaTime) override
            {
                if( getLevel().m_inputHandler->getInputState().xMovement == AxisMovement::Left )
                    subYRotationSpeed(deltaTime, 2.25_deg, -4_deg);
                else if( getLevel().m_inputHandler->getInputState().xMovement == AxisMovement::Right )
                    addYRotationSpeed(deltaTime, 2.25_deg, 4_deg);
            }


            void postprocessFrame(CollisionInfo& collisionInfo, const std::chrono::microseconds& deltaTime) override
            {
                setFallSpeed(core::makeInterpolatedValue(0.0f));
                setFalling(false);
                collisionInfo.yAngle = getRotation().Y;
                setMovementAngle(collisionInfo.yAngle);
                collisionInfo.passableFloorDistanceBottom = core::ClimbLimit2ClickMin;
                collisionInfo.passableFloorDistanceTop = -core::ClimbLimit2ClickMin;
                collisionInfo.neededCeilingDistance = 0;
                collisionInfo.policyFlags |= CollisionInfo::SlopesAreWalls | CollisionInfo::SlopesArePits | CollisionInfo::LavaIsPit;
                collisionInfo.initHeightInfo(getPosition(), getLevel(), core::ScalpHeight);

                if(stopIfCeilingBlocked(collisionInfo))
                    return;

                if(tryClimb(collisionInfo))
                    return;

                if(checkWallCollision(collisionInfo))
                {
                    const auto fr = getCurrentTime();
                    if( fr >= 29_frame && fr < 48_frame)
                    {
                        setAnimIdGlobal(loader::AnimationId::END_WALK_LEFT, 74);
                    }
                    else if( (fr >= 22_frame && fr < 29_frame) || (fr >= 48_frame && fr < 58_frame) )
                    {
                        setAnimIdGlobal(loader::AnimationId::END_WALK_RIGHT, 58);
                    }
                    else
                    {
                        setAnimIdGlobal(loader::AnimationId::STAY_SOLID, 185);
                    }
                }

                if( collisionInfo.current.floor.distance > core::ClimbLimit2ClickMin )
                {
                    setAnimIdGlobal(loader::AnimationId::FREE_FALL_FORWARD, 492);
                    setTargetState(LaraStateId::JumpForward);
                    setFallSpeed(core::makeInterpolatedValue(0.0f));
                    setFalling(true);
                }

                if( collisionInfo.current.floor.distance > core::SteppableHeight )
                {
                    const auto fr = getCurrentTime();
                    if( fr < 28_frame || fr >= 46_frame)
                    {
                        setAnimIdGlobal(loader::AnimationId::WALK_DOWN_RIGHT, 887);
                    }
                    else
                    {
                        setAnimIdGlobal(loader::AnimationId::WALK_DOWN_LEFT, 874);
                    }
                }

                if( collisionInfo.current.floor.distance >= -core::ClimbLimit2ClickMin && collisionInfo.current.floor.distance < -core::SteppableHeight )
                {
                    const auto fr = getCurrentTime();
                    if( fr < 27_frame || fr >= 45_frame)
                    {
                        setAnimIdGlobal(loader::AnimationId::WALK_UP_STEP_RIGHT, 844);
                    }
                    else
                    {
                        setAnimIdGlobal(loader::AnimationId::WALK_UP_STEP_LEFT, 858);
                    }
                }

                if( !tryStartSlide(collisionInfo) )
                {
                    placeOnFloor(collisionInfo);
                }
            }
        };
    }
}