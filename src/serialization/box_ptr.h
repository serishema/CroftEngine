#pragma once

#include "engine/engine.h"
#include "optional.h"
#include "ptr.h"

namespace serialization
{
inline std::optional<uint32_t> ptrSave(const loader::file::Box* box, const Serializer& ser)
{
  if(box == nullptr)
    return std::nullopt;

  ser.tag("box");
  return gsl::narrow<uint32_t>(std::distance(&ser.engine.getBoxes().at(0), box));
}

inline std::optional<uint32_t> ptrSave(loader::file::Box* box, const Serializer& ser)
{
  return ptrSave(const_cast<const loader::file::Box*>(box), ser);
}

inline const loader::file::Box*
  ptrLoad(const TypeId<const loader::file::Box*>&, std::optional<uint32_t> idx, const Serializer& ser)
{
  if(!idx.has_value())
    return nullptr;

  ser.tag("box");
  return &ser.engine.getBoxes().at(idx.value());
}

inline loader::file::Box* ptrLoad(const TypeId<loader::file::Box*>&, std::optional<uint32_t> idx, const Serializer& ser)
{
  return const_cast<loader::file::Box*>(ptrLoad(TypeId<const loader::file::Box*>{}, idx, ser));
}
} // namespace serialization