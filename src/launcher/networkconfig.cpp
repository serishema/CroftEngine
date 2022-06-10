#include "networkconfig.h"

#include "paths.h"
#include "serialization/serialization.h"
#include "serialization/vector.h"
#include "serialization/yamldocument.h"

namespace launcher
{

void NetworkConfig::serialize(const serialization::Serializer<NetworkConfig>& ser)
{
  ser(S_NV("socket", socket), S_NV("color", color));
}

NetworkConfig NetworkConfig::load()
{
  serialization::YAMLDocument<true> doc{findUserDataDir().value() / "network.yaml"};
  NetworkConfig cfg{};
  doc.load("config", cfg, cfg);
  return cfg;
}

void NetworkConfig::save()
{
  serialization::YAMLDocument<false> doc{findUserDataDir().value() / "network.yaml"};
  doc.save("config", *this, *this);
  doc.write();
}
} // namespace launcher
