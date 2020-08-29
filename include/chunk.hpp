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
void set(const ChunkLayout *layout, std::vector<uint8_t>& memory, std::tuple<Ts*...>& t)
{
   using Component = std::tuple_element_t<I, std::tuple<Ts...>>;

   ComponentType type = componentType<std::decay_t<Component>>();
   size_t start = layout->componentStart[type];
   std::get<I>(t) = reinterpret_cast<Component*>(&memory[start]);

   if constexpr (I + 1 != sizeof...(Ts))
   {
      set<I + 1>(layout, memory, t);
   }
}

struct Chunk
{
   // Pointer to a chunk kind, just a view to it
   const ChunkLayout* layout;

   explicit Chunk(const ChunkLayout* chunkLayout) : layout(chunkLayout), m_count(0), m_memory(CHUNK_SIZE)
   {
   }

   Chunk(const ChunkLayout&) = delete;
   Chunk(ChunkLayout&&) = delete;
   Chunk& operator=(const Chunk&) = delete;

   inline size_t computeIndex(ComponentType type, size_t index, size_t size)
   {
      assert(layout->archetype[type]);

      size_t start = layout->componentStart[type];
      return start + index * size;
   }

   template<typename C>
   inline C& getComponent(size_t index)
   {
      assert(layout->archetype[componentType<C>()]);
      assert(index < m_count);

      size_t memoryIndex = computeIndex(componentType<C>(), index, sizeof(C));

      assert(memoryIndex < CHUNK_SIZE);

      return reinterpret_cast<C&>(m_memory[memoryIndex]);
   }

   template<typename C>
   inline void setComponent(size_t index, const C& component)
   {
      assert(layout->archetype[componentType<C>()]);

      size_t memoryIndex = computeIndex(componentType<C>(), index, sizeof(C));

      assert(memoryIndex < CHUNK_SIZE);

      memcpy(&m_memory[memoryIndex], &component, sizeof(C));
   }

   template<typename F>
   inline void each(F&& exec)
   {
      using functor_trait = functor_traits<F>;

      each_helper(std::forward<F>(exec),
         std::make_index_sequence<functor_trait::nargs::value>{});
   }

   inline size_t count()
   {
      return m_count;
   }

private:
   friend struct EntityManager;

   template<typename F, size_t... I>
   inline void each_helper(F&& func, std::index_sequence<I...>)
   {
      using functor_trait = functor_traits<F>;
      static_assert((std::is_pointer_v<functor_trait::arg_t<I>> && ... && true),
         "Function parameters should be pointers");

      std::tuple<std::remove_const_t<functor_trait::arg_t<I>>...> pointers;
      set(layout, m_memory, pointers);

      for (size_t i = 0; i < m_count; i++)
      {
         std::apply(func, pointers);
         std::apply([](auto&&... args) {((args++), ...);}, pointers);
      }
   }

   // Number of entities in chunk
   size_t m_count;

   // The fixed memory content of this chunk
   std::vector<uint8_t> m_memory;
};
