#include "skeletalmodelnode.h"

#include "engine/engine.h"
#include "engine/objects/object.h"
#include "loader/file/mesh.h"
#include "serialization/glm.h"
#include "serialization/not_null.h"
#include "serialization/skeletalmodeltype_ptr.h"
#include "serialization/vector.h"

#include <stack>

namespace engine
{
SkeletalModelNode::SkeletalModelNode(const std::string& id,
                                     const gsl::not_null<const Engine*>& engine,
                                     const gsl::not_null<const loader::file::SkeletalModelType*>& model)
    : Node{id}
    , m_engine{engine}
    , m_model{model}
{
}

core::Speed SkeletalModelNode::calculateFloorSpeed(const objects::ObjectState& state, const core::Frame& frameOffset)
{
  const auto scaled
    = state.anim->speed + state.anim->acceleration * (state.frame_number - state.anim->firstFrame + frameOffset);
  return scaled / (1 << 16);
}

SkeletalModelNode::InterpolationInfo SkeletalModelNode::getInterpolationInfo(const objects::ObjectState& state) const
{
  /*
     * == Animation Layout ==
     *
     * Each character in the timeline depicts a single frame.
     *
     * First frame                Last frame/end of animation
     * v                          v
     * |-----|-----|-----|-----|--x..|
     *       ^           <----->     ^
     *       Keyframe    Segment     Last keyframe
     */
  InterpolationInfo result;

  Expects(state.anim != nullptr);
  Expects(state.anim->segmentLength > 0_frame);

  Expects(state.frame_number >= state.anim->firstFrame && state.frame_number <= state.anim->lastFrame);
  const auto firstKeyframeIndex = (state.frame_number - state.anim->firstFrame) / state.anim->segmentLength;

  result.firstFrame = state.anim->frames->next(firstKeyframeIndex);
  Expects(m_engine->isValid(result.firstFrame));

  if(state.frame_number >= state.anim->lastFrame)
  {
    result.secondFrame = result.firstFrame;
    return result;
  }

  result.secondFrame = result.firstFrame->next();
  Expects(m_engine->isValid(result.secondFrame));

  auto segmentDuration = state.anim->segmentLength;
  if((firstKeyframeIndex + 1) * state.anim->segmentLength >= state.anim->getFrameCount())
  {
    // second keyframe beyond end
    const auto tmp = state.anim->getFrameCount() % state.anim->segmentLength;
    if(tmp != 0_frame)
      segmentDuration = tmp + 1_frame;
  }

  const auto segmentFrame = (state.frame_number - state.anim->firstFrame) % segmentDuration;
  result.bias = segmentFrame.retype_as<float>() / segmentDuration.retype_as<float>();
  BOOST_ASSERT(result.bias >= 0 && result.bias <= 1);

  if(segmentFrame == 0_frame)
  {
    return result;
  }

  return result;
}

void SkeletalModelNode::updatePose(objects::ObjectState& state)
{
  if(getChildren().empty())
    return;

  BOOST_ASSERT(getChildren().size() >= m_model->meshes.size());

  updatePose(getInterpolationInfo(state));
}

void SkeletalModelNode::updatePoseInterpolated(const InterpolationInfo& framePair)
{
  BOOST_ASSERT(!m_model->meshes.empty());

  BOOST_ASSERT(framePair.bias > 0);
  BOOST_ASSERT(framePair.secondFrame != nullptr);

  BOOST_ASSERT(framePair.firstFrame->numValues > 0);
  BOOST_ASSERT(framePair.secondFrame->numValues > 0);

  if(m_bonePatches.empty())
    resetPose();
  BOOST_ASSERT(m_bonePatches.size() == getChildren().size());

  const auto angleDataFirst = framePair.firstFrame->getAngleData();
  std::stack<glm::mat4> transformsFirst;
  transformsFirst.push(translate(glm::mat4{1.0f}, framePair.firstFrame->pos.toGl())
                       * core::fromPackedAngles(angleDataFirst[0]) * m_bonePatches[0]);

  const auto angleDataSecond = framePair.secondFrame->getAngleData();
  std::stack<glm::mat4> transformsSecond;
  transformsSecond.push(translate(glm::mat4{1.0f}, framePair.secondFrame->pos.toGl())
                        * core::fromPackedAngles(angleDataSecond[0]) * m_bonePatches[0]);

  BOOST_ASSERT(framePair.bias >= 0 && framePair.bias <= 2);

  getChildren()[0]->setLocalMatrix(util::mix(transformsFirst.top(), transformsSecond.top(), framePair.bias));

  if(m_model->meshes.size() <= 1)
    return;

  for(size_t i = 1; i < m_model->meshes.size(); ++i)
  {
    if(m_model->boneTree[i - 1].flags & 0x01u)
    {
      transformsFirst.pop();
      transformsSecond.pop();
    }
    if(m_model->boneTree[i - 1].flags & 0x02u)
    {
      transformsFirst.push({transformsFirst.top()});   // make sure to have a copy, not a reference
      transformsSecond.push({transformsSecond.top()}); // make sure to have a copy, not a reference
    }

    BOOST_ASSERT((m_model->boneTree[i - 1].flags & 0x1cu) == 0);

    if(framePair.firstFrame->numValues < i)
      transformsFirst.top() *= translate(glm::mat4{1.0f}, m_model->boneTree[i - 1].toGl()) * m_bonePatches[i];
    else
      transformsFirst.top() *= translate(glm::mat4{1.0f}, m_model->boneTree[i - 1].toGl())
                               * core::fromPackedAngles(angleDataFirst[i]) * m_bonePatches[i];

    if(framePair.firstFrame->numValues < i)
      transformsSecond.top() *= translate(glm::mat4{1.0f}, m_model->boneTree[i - 1].toGl()) * m_bonePatches[i];
    else
      transformsSecond.top() *= translate(glm::mat4{1.0f}, m_model->boneTree[i - 1].toGl())
                                * core::fromPackedAngles(angleDataSecond[i]) * m_bonePatches[i];

    getChildren()[i]->setLocalMatrix(util::mix(transformsFirst.top(), transformsSecond.top(), framePair.bias));
  }
}

void SkeletalModelNode::updatePoseKeyframe(const InterpolationInfo& framePair)
{
  BOOST_ASSERT(!m_model->meshes.empty());

  BOOST_ASSERT(framePair.firstFrame->numValues > 0);

  if(m_bonePatches.empty())
    resetPose();
  BOOST_ASSERT(m_bonePatches.size() == getChildren().size());

  const auto angleData = framePair.firstFrame->getAngleData();

  std::stack<glm::mat4> transforms;
  transforms.push(translate(glm::mat4{1.0f}, framePair.firstFrame->pos.toGl()) * core::fromPackedAngles(angleData[0])
                  * m_bonePatches[0]);

  getChildren()[0]->setLocalMatrix(transforms.top());

  if(m_model->meshes.size() <= 1)
    return;

  for(size_t i = 1; i < m_model->meshes.size(); ++i)
  {
    BOOST_ASSERT((m_model->boneTree[i - 1].flags & 0x1cu) == 0);

    if(m_model->boneTree[i - 1].flags & 0x01u)
    {
      transforms.pop();
    }
    if(m_model->boneTree[i - 1].flags & 0x02u)
    {
      transforms.push({transforms.top()}); // make sure to have a copy, not a reference
    }

    if(framePair.firstFrame->numValues < i)
      transforms.top() *= translate(glm::mat4{1.0f}, m_model->boneTree[i - 1].toGl()) * m_bonePatches[i];
    else
      transforms.top() *= translate(glm::mat4{1.0f}, m_model->boneTree[i - 1].toGl())
                          * core::fromPackedAngles(angleData[i]) * m_bonePatches[i];

    getChildren()[i]->setLocalMatrix(transforms.top());
  }
}

loader::file::BoundingBox SkeletalModelNode::getBoundingBox(const objects::ObjectState& state) const
{
  const auto framePair = getInterpolationInfo(state);
  BOOST_ASSERT(framePair.bias >= 0 && framePair.bias <= 1);

  if(framePair.secondFrame != nullptr)
  {
    return {framePair.firstFrame->bbox.toBBox(), framePair.secondFrame->bbox.toBBox(), framePair.bias};
  }
  return framePair.firstFrame->bbox.toBBox();
}

bool SkeletalModelNode::handleStateTransitions(objects::ObjectState& state)
{
  Expects(state.anim != nullptr);
  if(state.anim->state_id == state.goal_anim_state)
    return false;

  for(const loader::file::Transitions& tr : state.anim->transitions)
  {
    if(tr.stateId != state.goal_anim_state)
      continue;

    const auto it = std::find_if(
      tr.transitionCases.cbegin(), tr.transitionCases.cend(), [&state](const loader::file::TransitionCase& trc) {
        return state.frame_number >= trc.firstFrame && state.frame_number <= trc.lastFrame;
      });

    if(it != tr.transitionCases.cend())
    {
      setAnimation(state, it->targetAnimation, it->targetFrame);
      return true;
    }
  }

  return false;
}

// ReSharper disable once CppMemberFunctionMayBeConst
void SkeletalModelNode::setAnimation(objects::ObjectState& state,
                                     const gsl::not_null<const loader::file::Animation*>& animation,
                                     core::Frame frame)
{
  BOOST_ASSERT(m_model->meshes.empty() || animation->frames->numValues == m_model->meshes.size());

  if(frame < animation->firstFrame || frame > animation->lastFrame)
    frame = animation->firstFrame;

  state.anim = animation;
  state.frame_number = frame;
  state.current_anim_state = state.anim->state_id;
}

bool SkeletalModelNode::advanceFrame(objects::ObjectState& state)
{
  state.frame_number += 1_frame;
  if(handleStateTransitions(state))
  {
    state.current_anim_state = state.anim->state_id;
    if(state.current_anim_state == state.required_anim_state)
      state.required_anim_state = 0_as;
  }

  return state.frame_number > state.anim->lastFrame;
}

std::vector<SkeletalModelNode::Sphere> SkeletalModelNode::getBoneCollisionSpheres(const objects::ObjectState& state,
                                                                                  const loader::file::AnimFrame& frame,
                                                                                  const glm::mat4* baseTransform)
{
  BOOST_ASSERT(frame.numValues > 0);
  BOOST_ASSERT(!m_model->meshes.empty());

  if(m_bonePatches.empty())
    resetPose();
  BOOST_ASSERT(m_bonePatches.size() == getChildren().size());

  const auto angleData = frame.getAngleData();

  std::stack<glm::mat4> transforms;

  core::TRVec pos;

  if(baseTransform == nullptr)
  {
    pos = state.position.position;
    transforms.push(state.rotation.toMatrix());
  }
  else
  {
    pos = core::TRVec{};
    transforms.push(*baseTransform * state.rotation.toMatrix());
  }

  transforms.top()
    = translate(transforms.top(), frame.pos.toGl()) * core::fromPackedAngles(angleData[0]) * m_bonePatches[0];

  std::vector<Sphere> result;
  result.emplace_back(translate(glm::mat4{1.0f}, pos.toRenderSystem())
                        + translate(transforms.top(), m_model->meshes[0]->center.toRenderSystem()),
                      m_model->meshes[0]->collision_size);

  for(gsl::index i = 1; i < m_model->meshes.size(); ++i)
  {
    BOOST_ASSERT((m_model->boneTree[i - 1].flags & 0x1cu) == 0);

    if(m_model->boneTree[i - 1].flags & 0x01u)
    {
      transforms.pop();
    }
    if(m_model->boneTree[i - 1].flags & 0x02u)
    {
      transforms.push({transforms.top()}); // make sure to have a copy, not a reference
    }

    if(frame.numValues < i)
      transforms.top() *= translate(glm::mat4{1.0f}, m_model->boneTree[i - 1].toGl()) * m_bonePatches[i];
    else
      transforms.top() *= translate(glm::mat4{1.0f}, m_model->boneTree[i - 1].toGl())
                          * core::fromPackedAngles(angleData[i]) * m_bonePatches[i];

    auto m = translate(transforms.top(), m_model->meshes[i]->center.toRenderSystem());
    m[3] += glm::vec4(pos.toRenderSystem(), 0);
    result.emplace_back(m, m_model->meshes[i]->collision_size);
  }

  return result;
}

void SkeletalModelNode::serialize(const serialization::Serializer& ser)
{
  auto id = getId();
  ser("id", id, "model", m_model, S_NVP(m_bonePatches));
}

void serialize(std::shared_ptr<SkeletalModelNode>& data, const serialization::Serializer& ser)
{
  if(ser.loading)
  {
    const loader::file::SkeletalModelType* model = nullptr;
    ser("model", model);
    data = std::make_shared<SkeletalModelNode>(
      create(serialization::TypeId<std::string>{}, ser["id"]), &ser.engine, model);
  }
  else
  {
    Expects(data != nullptr);
    data->serialize(ser);
  }
}

} // namespace engine
