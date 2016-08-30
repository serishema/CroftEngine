#include "Base.h"
#include "AnimationClip.h"
#include "Animation.h"
#include "AnimationTarget.h"
#include "Game.h"
#include "Quaternion.h"


namespace gameplay
{
    AnimationClip::AnimationClip(const std::string& id, Animation* animation, const std::chrono::microseconds& startTime, const std::chrono::microseconds& endTime)
        : _id(id)
        , _animation(animation)
        , _startTime(startTime)
        , _endTime(endTime)
        , _duration(_endTime - _startTime)
        , _stateBits(0x00)
        , _repeatCount(1.0f)
        , _loopBlendTime(std::chrono::microseconds::zero())
        , _activeDuration(std::chrono::duration_cast<std::chrono::microseconds>( _duration * _repeatCount ))
        , _speed(1.0f)
        , _timeStarted(std::chrono::microseconds::zero())
        , _elapsedTime(std::chrono::microseconds::zero())
        , _crossFadeToClip(nullptr)
        , _crossFadeOutElapsed(std::chrono::microseconds::zero())
        , _crossFadeOutDuration(std::chrono::microseconds::zero())
        , _blendWeight(1.0f)
        , _beginListeners(nullptr)
        , _endListeners(nullptr)
        , _listeners(nullptr)
        , _listenerItr(nullptr)
    {
        GP_ASSERT(_animation);
        GP_ASSERT(std::chrono::microseconds::zero() <= startTime && startTime <= _animation->_duration && std::chrono::microseconds::zero() <= endTime && endTime <= _animation->_duration);

        for( size_t i = 0, count = _animation->_channels.size(); i < count; i++ )
        {
            GP_ASSERT(_animation->_channels[i]);
            GP_ASSERT(_animation->_channels[i]->getCurve());
            _values.push_back(new AnimationValue(_animation->_channels[i]->getCurve()->getComponentCount()));
        }
    }


    AnimationClip::~AnimationClip()
    {
        std::vector<AnimationValue*>::iterator valueIter = _values.begin();
        while( valueIter != _values.end() )
        {
            SAFE_DELETE(*valueIter);
            ++valueIter;
        }
        _values.clear();

        SAFE_RELEASE(_crossFadeToClip);
        SAFE_DELETE(_beginListeners);
        SAFE_DELETE(_endListeners);

        if( _listeners )
        {
            *_listenerItr = _listeners->begin();
            while( *_listenerItr != _listeners->end() )
            {
                ListenerEvent* lEvt = **_listenerItr;
                SAFE_DELETE(lEvt);
                ++(*_listenerItr);
            }
            SAFE_DELETE(_listeners);
        }
        SAFE_DELETE(_listenerItr);
    }


    AnimationClip::ListenerEvent::ListenerEvent(Listener* listener, const std::chrono::microseconds& eventTime)
    {
        _listener = listener;
        _eventTime = eventTime;
    }


    AnimationClip::ListenerEvent::~ListenerEvent()
    {
    }


    const std::string& AnimationClip::getId() const
    {
        return _id;
    }


    Animation* AnimationClip::getAnimation() const
    {
        return _animation;
    }


    std::chrono::microseconds AnimationClip::getStartTime() const
    {
        return _startTime;
    }


    std::chrono::microseconds AnimationClip::getEndTime() const
    {
        return _endTime;
    }


    std::chrono::microseconds AnimationClip::getElapsedTime() const
    {
        return _elapsedTime;
    }


    void AnimationClip::setRepeatCount(float repeatCount)
    {
        GP_ASSERT(repeatCount == REPEAT_INDEFINITE || repeatCount > 0.0f);

        _repeatCount = repeatCount;

        if( repeatCount == REPEAT_INDEFINITE )
        {
            _activeDuration = _duration + _loopBlendTime;
        }
        else
        {
            _activeDuration = std::chrono::duration_cast<std::chrono::microseconds>( _duration * _repeatCount );

            if( repeatCount > 1.0f && _loopBlendTime > std::chrono::microseconds::zero() )
                _activeDuration += std::chrono::duration_cast<std::chrono::microseconds>( std::ceil(repeatCount - 1.0f) * _loopBlendTime );
        }
    }


    float AnimationClip::getRepeatCount() const
    {
        return _repeatCount;
    }


    void AnimationClip::setActiveDuration(const std::chrono::microseconds& duration)
    {
        GP_ASSERT(duration >= std::chrono::microseconds::zero());

        if( duration == std::chrono::microseconds::max() )
        {
            _activeDuration = _duration + _loopBlendTime;
        }
        else
        {
            _activeDuration = duration;
            _repeatCount = static_cast<float>(_activeDuration.count()) / _duration.count();
        }
    }


    std::chrono::microseconds AnimationClip::getActiveDuration() const
    {
        if( _repeatCount == REPEAT_INDEFINITE )
            return std::chrono::microseconds::max();

        return _activeDuration;
    }


    std::chrono::microseconds AnimationClip::getDuration() const
    {
        return _duration;
    }


    void AnimationClip::setSpeed(float speed)
    {
        _speed = speed;
    }


    float AnimationClip::getSpeed() const
    {
        return _speed;
    }


    void AnimationClip::setBlendWeight(float blendWeight)
    {
        _blendWeight = blendWeight;
    }


    float AnimationClip::getBlendWeight() const
    {
        return _blendWeight;
    }


    void AnimationClip::setLoopBlendTime(const std::chrono::microseconds& loopBlendTime)
    {
        if( loopBlendTime < std::chrono::microseconds::zero() )
        {
            _loopBlendTime = std::chrono::microseconds::zero();
        }
        else
        {
            _loopBlendTime = loopBlendTime;
        }
    }


    std::chrono::microseconds AnimationClip::getLoopBlendTime() const
    {
        return _loopBlendTime;
    }


    bool AnimationClip::isPlaying() const
    {
        return (isClipStateBitSet(CLIP_IS_PLAYING_BIT) && !isClipStateBitSet(CLIP_IS_PAUSED_BIT));
    }


    void AnimationClip::play(const std::chrono::microseconds& timeOffset)
    {
        if( isClipStateBitSet(CLIP_IS_PLAYING_BIT) )
        {
            // If paused, reset the bit and return.
            if( isClipStateBitSet(CLIP_IS_PAUSED_BIT) )
            {
                resetClipStateBit(CLIP_IS_PAUSED_BIT);
                return;
            }

            // If the clip is set to be removed, reset the flag.
            if( isClipStateBitSet(CLIP_IS_MARKED_FOR_REMOVAL_BIT) )
                resetClipStateBit(CLIP_IS_MARKED_FOR_REMOVAL_BIT);

            // Set the state bit to restart.
            setClipStateBit(CLIP_IS_RESTARTED_BIT);
        }
        else
        {
            setClipStateBit(CLIP_IS_PLAYING_BIT);
            GP_ASSERT(_animation);
            GP_ASSERT(_animation->_controller);
            _animation->_controller->schedule(this);
        }

        _timeStarted = Game::getGameTime() - timeOffset;
    }


    void AnimationClip::stop()
    {
        if( isClipStateBitSet(CLIP_IS_PLAYING_BIT) )
        {
            // Reset the restarted and paused bits.
            resetClipStateBit(CLIP_IS_RESTARTED_BIT);
            resetClipStateBit(CLIP_IS_PAUSED_BIT);

            // Mark the clip to removed from the AnimationController.
            setClipStateBit(CLIP_IS_MARKED_FOR_REMOVAL_BIT);
        }
    }


    void AnimationClip::pause()
    {
        if( isClipStateBitSet(CLIP_IS_PLAYING_BIT) && !isClipStateBitSet(CLIP_IS_MARKED_FOR_REMOVAL_BIT) )
        {
            setClipStateBit(CLIP_IS_PAUSED_BIT);
        }
    }


    void AnimationClip::crossFade(AnimationClip* clip, const std::chrono::microseconds& duration)
    {
        GP_ASSERT(clip);

        // Check if the given clip is fading into this clip.
        // We should reset the clip from fading out, and this one from fading in
        // in order to start the crossfade back the other way.
        if( clip->isClipStateBitSet(CLIP_IS_FADING_OUT_BIT) && clip->_crossFadeToClip == this )
        {
            clip->resetClipStateBit(CLIP_IS_FADING_OUT_BIT);
            clip->_crossFadeToClip->resetClipStateBit(CLIP_IS_FADING_IN_BIT);
            SAFE_RELEASE(clip->_crossFadeToClip);
        }

        // If I already have a clip I'm fading to and it's not the same as the given clip release it.
        // Assign the new clip and increase it's ref count.
        if( _crossFadeToClip )
        {
            SAFE_RELEASE(_crossFadeToClip);
        }

        // Set and initialize the crossfade clip
        _crossFadeToClip = clip;
        _crossFadeToClip->addRef();
        _crossFadeToClip->setClipStateBit(CLIP_IS_FADING_IN_BIT);
        _crossFadeToClip->_blendWeight = 0.0f;

        // Set and initialize this clip to fade out
        setClipStateBit(CLIP_IS_FADING_OUT_STARTED_BIT);
        setClipStateBit(CLIP_IS_FADING_OUT_BIT);
        _crossFadeOutElapsed = std::chrono::microseconds::zero();
        _crossFadeOutDuration = duration;

        // If this clip is currently not playing, we should start playing it.
        if( !isClipStateBitSet(CLIP_IS_PLAYING_BIT) )
            play();

        // Start playing the cross fade clip.
        _crossFadeToClip->play();
    }


    void AnimationClip::addListener(AnimationClip::Listener* listener, const std::chrono::microseconds& eventTime)
    {
        GP_ASSERT(listener);
        GP_ASSERT(eventTime < _activeDuration);

        ListenerEvent* listenerEvent = new ListenerEvent(listener, eventTime);

        if( !_listeners )
        {
            _listeners = new std::list<ListenerEvent*>;
            _listeners->push_front(listenerEvent);

            _listenerItr = new std::list<ListenerEvent*>::iterator;
            if( isClipStateBitSet(CLIP_IS_PLAYING_BIT) )
                *_listenerItr = _listeners->begin();
        }
        else
        {
            for( std::list<ListenerEvent*>::iterator itr = _listeners->begin(); itr != _listeners->end(); ++itr )
            {
                GP_ASSERT(*itr);
                if( eventTime < (*itr)->_eventTime )
                {
                    itr = _listeners->insert(itr, listenerEvent);

                    // If playing, update the iterator if we need to.
                    // otherwise, it will just be set the next time the clip gets played.
                    if( isClipStateBitSet(CLIP_IS_PLAYING_BIT) )
                    {
                        std::chrono::microseconds currentTime = _elapsedTime % _duration;
                        GP_ASSERT(**_listenerItr || *_listenerItr == _listeners->end());
                        if( (_speed >= 0.0f && currentTime < eventTime && (*_listenerItr == _listeners->end() || eventTime < (**_listenerItr)->_eventTime)) ||
                            (_speed <= 0 && currentTime > eventTime && (*_listenerItr == _listeners->begin() || eventTime > (**_listenerItr)->_eventTime)) )
                        {
                            *_listenerItr = itr;
                        }
                    }
                    return;
                }
            }
            _listeners->push_back(listenerEvent);
        }
    }


    void AnimationClip::removeListener(AnimationClip::Listener* listener, const std::chrono::microseconds& eventTime)
    {
        if( _listeners )
        {
            GP_ASSERT(listener);
            std::list<ListenerEvent*>::iterator iter = std::find_if(_listeners->begin(), _listeners->end(), [&](ListenerEvent* lst)
                                                                    {
                                                                        return lst->_eventTime == eventTime && lst->_listener == listener;
                                                                    });
            if( iter != _listeners->end() )
            {
                if( isClipStateBitSet(CLIP_IS_PLAYING_BIT) )
                {
                    std::chrono::microseconds currentTime = _elapsedTime % _duration;
                    GP_ASSERT(**_listenerItr || *_listenerItr == _listeners->end());

                    // We the listener has not been triggered yet, then check if it is next to be triggered, remove it, and update the iterator
                    if( ((_speed >= 0.0f && currentTime < eventTime) || (_speed <= 0 && currentTime > eventTime)) &&
                        *iter == **_listenerItr )
                    {
                        *_listenerItr = _listeners->erase(iter);
                        return;
                    }
                }
                _listeners->erase(iter);
            }
        }
    }


    void AnimationClip::addBeginListener(AnimationClip::Listener* listener)
    {
        if( !_beginListeners )
            _beginListeners = new std::vector<Listener*>;

        GP_ASSERT(listener);
        _beginListeners->push_back(listener);
    }


    void AnimationClip::removeBeginListener(AnimationClip::Listener* listener)
    {
        if( _beginListeners )
        {
            GP_ASSERT(listener);
            std::vector<Listener*>::iterator iter = std::find(_beginListeners->begin(), _beginListeners->end(), listener);
            if( iter != _beginListeners->end() )
            {
                _beginListeners->erase(iter);
            }
        }
    }


    void AnimationClip::addEndListener(AnimationClip::Listener* listener)
    {
        if( !_endListeners )
            _endListeners = new std::vector<Listener*>;

        GP_ASSERT(listener);
        _endListeners->push_back(listener);
    }


    void AnimationClip::removeEndListener(AnimationClip::Listener* listener)
    {
        if( _endListeners )
        {
            GP_ASSERT(listener);
            std::vector<Listener*>::iterator iter = std::find(_endListeners->begin(), _endListeners->end(), listener);
            if( iter != _endListeners->end() )
            {
                _endListeners->erase(iter);
            }
        }
    }


    bool AnimationClip::update(const std::chrono::microseconds& elapsedTime)
    {
        if( isClipStateBitSet(CLIP_IS_PAUSED_BIT) )
        {
            return false;
        }

        if( isClipStateBitSet(CLIP_IS_MARKED_FOR_REMOVAL_BIT) )
        {
            // If the marked for removal bit is set, it means stop() was called on the AnimationClip at some point
            // after the last update call. Reset the flag, and return true so the AnimationClip is removed from the
            // running clips on the AnimationController.
            onEnd();
            return true;
        }

        if( !isClipStateBitSet(CLIP_IS_STARTED_BIT) )
        {
            // Clip is just starting
            onBegin();
        }
        else
        {
            // Clip was already running
            _elapsedTime += std::chrono::duration_cast<std::chrono::microseconds>( elapsedTime * _speed );

            if( _repeatCount == REPEAT_INDEFINITE && _elapsedTime <= std::chrono::microseconds::zero() )
            {
                // Elapsed time is moving backwards, so wrap it back around the end when it falls below zero
                _elapsedTime = _activeDuration + _elapsedTime;

                // TODO: account for _loopBlendTime
            }
        }

        // Current time within a loop of the clip
        std::chrono::microseconds currentTime = std::chrono::microseconds::zero();

        // Check to see if clip is complete.
        if( _repeatCount != REPEAT_INDEFINITE && ((_speed >= 0.0f && _elapsedTime >= _activeDuration) || (_speed <= 0.0f && _elapsedTime <= std::chrono::microseconds::zero())) )
        {
            // We finished our active duration (including repeats), so clamp to our end value.
            resetClipStateBit(CLIP_IS_STARTED_BIT);

            // Ensure we end off at the endpoints of our clip (-speed==0, +speed==_duration)
            currentTime = _speed < 0.0f ? std::chrono::microseconds::zero() : _duration;
        }
        else
        {
            // If _duration == 0, we have a "pose". Just set currentTime to 0.
            if( _duration == std::chrono::microseconds::zero() )
            {
                currentTime = std::chrono::microseconds::zero();
            }
            else
            {
                // Animation is running normally.
                currentTime = _elapsedTime % (_duration + _loopBlendTime);
            }
        }

        // Notify any listeners of Animation events.
        if( _listeners )
        {
            GP_ASSERT(_listenerItr);

            if( _speed >= 0.0f )
            {
                while( *_listenerItr != _listeners->end() && _elapsedTime >= (**_listenerItr)->_eventTime )
                {
                    GP_ASSERT(_listenerItr);
                    GP_ASSERT(**_listenerItr);
                    GP_ASSERT((**_listenerItr)->_listener);

                    (**_listenerItr)->_listener->animationEvent(this, Listener::TIME);
                    ++(*_listenerItr);
                }
            }
            else
            {
                while( *_listenerItr != _listeners->begin() && _elapsedTime <= (**_listenerItr)->_eventTime )
                {
                    GP_ASSERT(_listenerItr);
                    GP_ASSERT(**_listenerItr);
                    GP_ASSERT((**_listenerItr)->_listener);

                    (**_listenerItr)->_listener->animationEvent(this, Listener::TIME);
                    --(*_listenerItr);
                }
            }
        }

        // Add back in start time, and divide by the total animation's duration to get the actual percentage complete
        GP_ASSERT(_animation);

        // Compute percentage complete for the current loop (prevent a divide by zero if _duration==0).
        // Note that we don't use (currentTime/(_duration+_loopBlendTime)). That's because we want a
        // % value that is outside the 0-1 range for loop smoothing/blending purposes.
        float percentComplete = _duration == std::chrono::microseconds::zero() ? 1 : static_cast<float>(currentTime.count()) / _duration.count();

        if( _loopBlendTime == std::chrono::microseconds::zero() )
            percentComplete = MATH_CLAMP(percentComplete, 0.0f, 1.0f);

        // If we're cross fading, compute blend weights
        if( isClipStateBitSet(CLIP_IS_FADING_OUT_BIT) )
        {
            GP_ASSERT(_crossFadeToClip);
            GP_ASSERT(_crossFadeOutDuration > std::chrono::microseconds::zero());

            if( isClipStateBitSet(CLIP_IS_FADING_OUT_STARTED_BIT) ) // Calculate elapsed time since the fade out begin.
            {
                GP_ASSERT(_crossFadeToClip);
                _crossFadeOutElapsed = std::chrono::duration_cast<std::chrono::microseconds>( (Game::getGameTime() - _crossFadeToClip->_timeStarted) * std::abs(_speed) );
                resetClipStateBit(CLIP_IS_FADING_OUT_STARTED_BIT);
            }
            else
            {
                // continue tracking elapsed time.
                _crossFadeOutElapsed += std::chrono::duration_cast<std::chrono::microseconds>( elapsedTime * std::abs(_speed) );
            }

            if( _crossFadeOutElapsed < _crossFadeOutDuration )
            {
                // Calculate this clip's blend weight.
                float tempBlendWeight = static_cast<float>((_crossFadeOutDuration - _crossFadeOutElapsed).count()) / _crossFadeOutDuration.count();

                // If this clip is fading in, adjust the crossfade clip's weight to be a percentage of your current blend weight
                if( isClipStateBitSet(CLIP_IS_FADING_IN_BIT) )
                {
                    _crossFadeToClip->_blendWeight = (1.0f - tempBlendWeight) * _blendWeight;
                    _blendWeight -= _crossFadeToClip->_blendWeight;
                }
                else
                {
                    // Just set the blend weight.
                    _crossFadeToClip->_blendWeight = (1.0f - tempBlendWeight);
                    _blendWeight = tempBlendWeight;
                }
            }
            else
            {
                // Fade is done.
                _crossFadeToClip->_blendWeight = 1.0f;
                _blendWeight = 0.0f;
                resetClipStateBit(CLIP_IS_STARTED_BIT);
                resetClipStateBit(CLIP_IS_FADING_OUT_BIT);
                _crossFadeToClip->resetClipStateBit(CLIP_IS_FADING_IN_BIT);
                SAFE_RELEASE(_crossFadeToClip);
            }
        }

        // Evaluate this clip.
        Animation::Channel* channel = nullptr;
        AnimationValue* value = nullptr;
        AnimationTarget* target = nullptr;
        size_t channelCount = _animation->_channels.size();
        float percentageStart = static_cast<float>(_startTime.count()) / _animation->_duration.count();
        float percentageEnd = static_cast<float>(_endTime.count()) / _animation->_duration.count();
        float percentageBlend = static_cast<float>(_loopBlendTime.count()) / _animation->_duration.count();
        for( size_t i = 0; i < channelCount; i++ )
        {
            channel = _animation->_channels[i];
            GP_ASSERT(channel);
            target = channel->_target;
            GP_ASSERT(target);
            value = _values[i];
            GP_ASSERT(value);

            // Evaluate the point on Curve
            GP_ASSERT(channel->getCurve());
            channel->getCurve()->evaluate(percentComplete, percentageStart, percentageEnd, percentageBlend, value->_value);

            // Set the animation value on the target property.
            target->setAnimationPropertyValue(channel->_propertyId, value, _blendWeight);
        }

        // When ended. Probably should move to it's own method so we can call it when the clip is ended early.
        if( isClipStateBitSet(CLIP_IS_MARKED_FOR_REMOVAL_BIT) || !isClipStateBitSet(CLIP_IS_STARTED_BIT) )
        {
            onEnd();
            return true;
        }

        return false;
    }


    void AnimationClip::onBegin()
    {
        addRef();

        // Initialize animation to play.
        setClipStateBit(CLIP_IS_STARTED_BIT);
        if( _speed >= 0 )
        {
            _elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>( (Game::getGameTime() - _timeStarted) * _speed );

            if( _listeners )
                *_listenerItr = _listeners->begin();
        }
        else
        {
            _elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>( _activeDuration + (Game::getGameTime() - _timeStarted) * _speed );

            if( _listeners )
                *_listenerItr = _listeners->end();
        }

        // Notify begin listeners if any.
        if( _beginListeners )
        {
            std::vector<Listener*>::iterator listener = _beginListeners->begin();
            while( listener != _beginListeners->end() )
            {
                GP_ASSERT(*listener);
                (*listener)->animationEvent(this, Listener::BEGIN);
                listener++;
            }
        }

        release();
    }


    void AnimationClip::onEnd()
    {
        addRef();

        _blendWeight = 1.0f;
        resetClipStateBit(CLIP_ALL_BITS);

        // Notify end listeners if any.
        if( _endListeners )
        {
            std::vector<Listener*>::iterator listener = _endListeners->begin();
            while( listener != _endListeners->end() )
            {
                GP_ASSERT(*listener);
                (*listener)->animationEvent(this, Listener::END);
                listener++;
            }
        }

        release();
    }


    bool AnimationClip::isClipStateBitSet(unsigned char bit) const
    {
        return (_stateBits & bit) == bit;
    }


    void AnimationClip::setClipStateBit(unsigned char bit)
    {
        _stateBits |= bit;
    }


    void AnimationClip::resetClipStateBit(unsigned char bit)
    {
        _stateBits &= ~bit;
    }

}
