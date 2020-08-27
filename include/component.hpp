#pragma once

// Value to define a component
using ComponentType = uint8_t;

// Used to define the size of arrays later on
const ComponentType MAX_COMPONENTS = 32;

// Type for a computeArchetype of components
using Archetype = std::bitset<MAX_COMPONENTS>;

class ComponentTypeId
{
public:
   template <typename C>
   static ComponentType id() noexcept
   {
      static const ComponentType id = mNextTypeId++;
      mSizes[id] = sizeof(C);
      return id;
   }

   static size_t size(ComponentType type) noexcept
   {
      return mSizes[type];
   }

private:
   static ComponentType mNextTypeId;
   static std::array<size_t, MAX_COMPONENTS> mSizes;
};

ComponentType ComponentTypeId::mNextTypeId{0};
std::array<size_t, MAX_COMPONENTS> ComponentTypeId::mSizes{};

template <typename C>
Archetype computeArchetype() noexcept
{
   Archetype mask;
   mask.set(ComponentTypeId::id<std::decay_t<C>>());
   return mask;
}

template <typename C1, typename C2, typename... Components>
Archetype computeArchetype() noexcept
{
   return computeArchetype<C1>() | computeArchetype<C2, Components...>();
}

