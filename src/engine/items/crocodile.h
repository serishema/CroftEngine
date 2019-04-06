#pragma once

#include "aiagent.h"

#include "engine/ai/ai.h"

namespace engine
{
namespace items
{
class Crocodile final
        : public AIAgent
{
public:
    Crocodile(const gsl::not_null<Engine*>& engine,
              const gsl::not_null<const loader::file::Room*>& room,
              const loader::file::Item& item,
              const loader::file::SkeletalModelType& animatedModel)
            : AIAgent{engine, room, item, animatedModel}
    {
    }

    void update() override;

    void load(const YAML::Node& n) override
    {
        auto newType = n["state"]["type"].as<core::TypeId>();

        if( newType.get_as<TR1ItemId>() == TR1ItemId::CrocodileOnLand || newType.get_as<TR1ItemId>() == TR1ItemId::CrocodileInWater )
            m_state.type = newType;

        AIAgent::load( n );
    }
};
}
}