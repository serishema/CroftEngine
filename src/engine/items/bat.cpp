#include "bat.h"

#include "engine/heightinfo.h"
#include "level/level.h"
#include "engine/laranode.h"
#include "engine/ai/ai.h"

#include <boost/range/adaptors.hpp>


namespace engine
{
    namespace items
    {
        void Bat::updateImpl(const std::chrono::microseconds& /*deltaTime*/, const boost::optional<FrameChangeType>& frameChangeType)
        {
            if( !frameChangeType.is_initialized() )
                return;

            if( m_triggerState == TriggerState::Locked )
            {
                m_triggerState = TriggerState::Enabled;
            }

            static constexpr const uint16_t StartingToFly = 1;
            static constexpr const uint16_t FlyingStraight = 2;
            static constexpr const uint16_t Biting = 3;
            static constexpr const uint16_t Circling = 4;
            static constexpr const uint16_t Dying = 5;

            core::Angle rotationToMoveTarget = 0_deg;
            if( getHealth() > 0 )
            {
                ai::LookAhead lookAhead(*this, 0);

                getBrain().route.updateMood(getBrain(), lookAhead, *this, false, 1024);
                rotationToMoveTarget = rotateTowardsMoveTarget(getBrain(), 20_deg);
                switch( getCurrentState() )
                {
                    case StartingToFly:
                        setTargetState(FlyingStraight);
                        break;
                    case FlyingStraight:
                        if( false /** @fixme touch_bits != 0 */ )
                            setTargetState(Biting);
                        break;
                    case Biting:
                        if( false /** @fixme touch_bits != 0 */ )
                        {
                            //! @fixme Show blood splatter FX
                            getLevel().m_lara->m_flags2_10_isHit = true;
                            getLevel().m_lara->setHealth(getLevel().m_lara->getHealth() - 2);
                        }
                        else
                        {
                            setTargetState(FlyingStraight);
                            getBrain().mood = ai::Mood::Bored;
                        }
                        break;
                    default:
                        break;
                }
            }
            else if( getPosition().Y >= getFloorHeight() )
            {
                setTargetState(Dying);
                setY(getFloorHeight());
                setFalling(false);
            }
            else
            {
                setTargetState(Circling);
                setHorizontalSpeed(core::makeInterpolatedValue(0.0f));
                setFalling(true);
            }
            animateCreature(rotationToMoveTarget, 0_deg);
        }
    }
}