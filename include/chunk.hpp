#pragma once

#include "component.hpp"

// The size in byte of a chunk
const size_t CHUNK_SIZE = 16384;

struct ChunkLayout
{
   // The component archetype of this chunk kind
   Archetype archetype{};

   // Map a component to the start in the actual chunk
   std::array<size_t, MAX_COMPONENTS> componentStart{};

   // Number of entities that can go in a chunk
   size_t capacity{0};

   ChunkLayout() = default;
};

static ChunkLayout computeChunkLayout(Archetype archetype) noexcept
{
   ChunkLayout layout;
   layout.archetype = archetype;

   // First compute the size of an entity line in the chunk
   size_t entitySize = 0;

   for (size_t type = 0; type < archetype.size(); type++)
   {
      if (archetype[type])
      {
          entitySize += componentSize(static_cast<ComponentType>(type));
      }
   }

   // Then compute how many entities can fit in this chunk
   layout.capacity = CHUNK_SIZE / entitySize;

   assert(layout.capacity > 0);

   size_t currentStart = 0;

   for (size_t type = 0; type < archetype.size(); type++)
   {
      if (archetype[type])
      {
         layout.componentStart[type] = currentStart;
         currentStart += layout.capacity * componentSize(static_cast<ComponentType>(type));
      }
   }

   return layout;
}

// Set all pointers in the tuple to the start in memory according to the layout
template<size_t I = 0, typename... Ts>
void set(const ChunkLayout& layout, std::vector<uint8_t>& memory, std::tuple<Ts*...>& t)
{
   using Component = std::tuple_element_t<I, std::tuple<Ts...>>;

   ComponentType type = componentType<std::decay_t<Component>>();
   size_t start = layout.componentStart[type];
   std::get<I>(t) = reinterpret_cast<Component*>(&memory[start]);

   if constexpr (I + 1 != sizeof...(Ts))
   {
      set<I + 1>(layout, memory, t);
   }
}

template<typename... Ts, size_t... I>
auto deref_helper(std::tuple<Ts...>& pointers, std::index_sequence<I...>)
{
   return std::tie(*std::get<I>(pointers)...);
}

// Return a new tuple dereferencing each value in the given tuple
template<typename... Ts>
auto deref(std::tuple<Ts...>& pointers)
{
   return deref_helper(pointers, std::make_index_sequence<sizeof...(Ts)>{});
}

struct Chunk
{
   const ChunkLayout& layout;

   explicit Chunk(const ChunkLayout& chunkLayout) : layout(chunkLayout), m_count(0), m_memory(CHUNK_SIZE)
   {
   }

   Chunk& operator=(const Chunk&) = delete;

   inline size_t computeIndex(ComponentType type, size_t index, size_t size)
   {
      assert(layout.archetype[type]);

      size_t start = layout.componentStart[type];
      return start + index * size;
   }

   template<typename C>
   inline C& getComponent(size_t index)
   {
      assert(layout.archetype[componentType<C>()]);
      assert(index < m_count);

      size_t memoryIndex = computeIndex(componentType<C>(), index, sizeof(C));

      assert(memoryIndex < CHUNK_SIZE);

      return reinterpret_cast<C&>(m_memory[memoryIndex]);
   }

   template<typename C>
   inline void setComponent(size_t index, const C& component)
   {
      assert(layout.archetype[componentType<C>()]);

      size_t memoryIndex = computeIndex(componentType<C>(), index, sizeof(C));

      assert(memoryIndex < CHUNK_SIZE);

      memcpy(&m_memory[memoryIndex], &component, sizeof(C));
   }

   // Call the given functor with the components specified by the functor's parameter types.
   template<typename F>
   inline void each(F&& func)
   {
      each_helper(std::forward<F>(func), typename functor_traits<F>::args_pointer_t{});
   }

   inline size_t count()
   {
      return m_count;
   }

private:
   friend struct EntityManager;

   // Number of entities in chunk
   size_t m_count;

   // The fixed memory content of this chunk
   std::vector<uint8_t> m_memory;

   template<typename F, typename... Cs>
   inline void each_helper(F&& func, std::tuple<Cs...> pointers)
   {
      set(layout, m_memory, pointers);

      for (size_t i = 0; i < m_count; i++)
      {
         std::apply(func, deref(pointers));
         std::apply([](auto&&... args) {((args++), ...);}, pointers);
      }
   }
};
