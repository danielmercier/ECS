#pragma once

// Value to define a component
using ComponentType = uint8_t;

// Used to define the size of arrays later on
const ComponentType MAX_COMPONENTS = 32;

// Type for a computeArchetype of components
using Archetype = std::bitset<MAX_COMPONENTS>;

static std::array<size_t, MAX_COMPONENTS> componentSizes;

inline ComponentType nextId()
{
   static ComponentType next = 0;
   return next++;
}

inline size_t componentSize(ComponentType type)
{
   return componentSizes[type];
}

template<typename C>
ComponentType componentType()
{
   static const ComponentType id = nextId();
   componentSizes[id] = sizeof(C);
   return id;
}

template <typename C>
Archetype computeArchetype() noexcept
{
   Archetype mask;
   mask.set(componentType<std::decay_t<C>>());
   return mask;
}

template <typename C1, typename C2, typename... Components>
Archetype computeArchetype() noexcept
{
   return computeArchetype<C1>() | computeArchetype<C2, Components...>();
}

