#include "animationcontroller.h"

#include "laracontroller.h"


namespace engine
{
    MeshAnimationController::MeshAnimationController(gsl::not_null<const level::Level*> level, const loader::AnimatedModel& model, gsl::not_null<gameplay::Node*> node, gsl::not_null<gameplay::AnimationController*> ctrl, const std::string& name)
        : AnimationController(level, name)
        , m_model(model)
        , m_currentAnimationId(model.animationIndex)
        , m_node(node)
        , m_animController{ctrl}
    {
        auto it = model.frameMapping.find(m_currentAnimationId);
        if( it == model.frameMapping.end() )
        {
            BOOST_LOG_TRIVIAL(error) << "No initial animation for " << name;
            return;
        }

        startAnimLoop(it->second.firstFrame);
        m_targetState = getCurrentAnimState();
    }


    void MeshAnimationController::startAnimLoop(uint32_t localFrame)
    {
        auto it = m_model.frameMapping.find(m_currentAnimationId);
        BOOST_ASSERT(it != m_model.frameMapping.end());
        it->second.apply(*m_animController, localFrame);
    }


    void MeshAnimationController::advanceFrame()
    {
        if( m_animController->getRunningClips().empty() )
            return;

        gameplay::AnimationClip* clip = m_animController->getRunningClips().front();
        Expects(clip != nullptr);

        BOOST_LOG_TRIVIAL(debug) << "Advance frame: current=" << clip->getElapsedTime() << ", end=" << clip->getEndTime();
        if( clip->getElapsedTime() + 1000.0 / core::FrameRate >= clip->getEndTime() )
        {
            handleAnimationEnd();
        }
        else
        {
            clip->setElapsedTime(clip->getElapsedTime() + 1000.0 / core::FrameRate);
        }

        handleTRTransitions();
    }


    uint32_t MeshAnimationController::getCurrentFrame() const
    {
        auto it = m_model.frameMapping.find(m_currentAnimationId);
        BOOST_ASSERT(it != m_model.frameMapping.end());

        gameplay::AnimationClip* clip = m_animController->getRunningClips().front();
        Expects(clip != nullptr);

        return std::lround(clip->getElapsedTime() * core::FrameRate / 1000 - it->second.offset + it->second.firstFrame);
    }


    uint32_t MeshAnimationController::getAnimEndFrame() const
    {
        auto it = m_model.frameMapping.find(m_currentAnimationId);
        BOOST_ASSERT(it != m_model.frameMapping.end());

        return it->second.lastFrame;
    }


    gameplay::BoundingBox MeshAnimationController::getBoundingBox() const
    {
        auto it = m_model.frameMapping.find(m_currentAnimationId);
        BOOST_ASSERT(it != m_model.frameMapping.end());

        return it->second.getBoundingBox(getCurrentFrame());
    }


    uint32_t MeshAnimationController::getCurrentRelativeFrame() const
    {
        auto it = m_model.frameMapping.find(m_currentAnimationId);
        BOOST_ASSERT(it != m_model.frameMapping.end());

        gameplay::AnimationClip* clip = m_animController->getRunningClips().front();
        Expects(clip != nullptr);

        return std::lround(clip->getElapsedTime() * core::FrameRate / 1000 - it->second.offset);
    }


    uint16_t MeshAnimationController::getCurrentAnimState() const
    {
        BOOST_ASSERT(m_currentAnimationId < getLevel()->m_animations.size());
        const loader::Animation& currentAnim = getLevel()->m_animations[m_currentAnimationId];
        return currentAnim.state_id;
    }


    void MeshAnimationController::playGlobalAnimation(uint16_t anim, const boost::optional<uint32_t>& firstFrame)
    {
        auto it = m_model.frameMapping.find(anim);
        if( it == m_model.frameMapping.end() )
        {
            BOOST_LOG_TRIVIAL(error) << "No animation " << anim << " for " << getName();
            return;
        }

        m_currentAnimationId = anim;
        it->second.apply(*m_animController, firstFrame.get_value_or(it->second.firstFrame));
        //m_targetState = getCurrentState();

        BOOST_LOG_TRIVIAL(debug) << "Playing animation " << anim << ", state " << getCurrentAnimState();
    }


    bool MeshAnimationController::handleTRTransitions()
    {
        if( getCurrentAnimState() == m_targetState )
            return false;

        BOOST_ASSERT(m_currentAnimationId < getLevel()->m_animations.size());
        const loader::Animation& currentAnim = getLevel()->m_animations[m_currentAnimationId];
        const auto currentFrame = getCurrentFrame();

        for( size_t i = 0; i < currentAnim.transitionsCount; ++i )
        {
            auto tIdx = currentAnim.transitionsIndex + i;
            BOOST_ASSERT(tIdx < getLevel()->m_transitions.size());
            const loader::Transitions& tr = getLevel()->m_transitions[tIdx];
            if( tr.stateId != m_targetState )
                continue;

            for( auto j = tr.firstTransitionCase; j < tr.firstTransitionCase + tr.transitionCaseCount; ++j )
            {
                BOOST_ASSERT(j < getLevel()->m_transitionCases.size());
                const loader::TransitionCase& trc = getLevel()->m_transitionCases[j];

                if( currentFrame >= trc.firstFrame && currentFrame <= trc.lastFrame )
                {
                    m_currentAnimationId = trc.targetAnimation;
                    startAnimLoop(trc.targetFrame);
                    BOOST_LOG_TRIVIAL(debug) << getName() << " -- found transition to state " << m_targetState << ", new animation " << m_currentAnimationId << "/frame " << trc.targetFrame;
                    return true;
                }
            }
        }

        return false;
    }


    void MeshAnimationController::handleAnimationEnd()
    {
        BOOST_ASSERT(m_currentAnimationId < getLevel()->m_animations.size());
        const loader::Animation& currentAnim = getLevel()->m_animations[m_currentAnimationId];

        m_currentAnimationId = currentAnim.nextAnimation;
        startAnimLoop(currentAnim.nextFrame);

        setTargetState(getCurrentAnimState());
    }
}
