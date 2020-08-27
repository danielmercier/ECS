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

ChunkLayout computeChunkLayout(Archetype archetype) noexcept
{
   ChunkLayout layout;
   layout.archetype = archetype;

   // First compute the size of an entity line in the chunk
   size_t entitySize = 0;

   for (size_t type = 0; type < archetype.size(); type++)
   {
      if (archetype[type])
      {
          entitySize += ComponentTypeId::size(static_cast<ComponentType>(type));
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
         currentStart += layout.capacity * ComponentTypeId::size(static_cast<ComponentType>(type));
      }
   }

   return layout;
}

struct Chunk
{
   // Pointer to a chunk kind, just a view to it
   const ChunkLayout* layout;

   // Number of entities in chunk
   size_t count;

   // The fixed memory content of this chunk
   std::vector<uint8_t> memory;

   explicit Chunk(const ChunkLayout* chunkLayout) :
      layout(chunkLayout), count(0), memory(CHUNK_SIZE)
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
      assert(layout->archetype[ComponentTypeId::id<C>()]);
      assert(index < count);

      size_t memoryIndex = computeIndex(ComponentTypeId::id<C>(), index, sizeof(C));

      assert(memoryIndex < CHUNK_SIZE);

      return reinterpret_cast<C&>(memory[memoryIndex]);
   }

   template<typename C>
   inline void setComponent(size_t index, const C& component)
   {
      assert(layout->archetype[ComponentTypeId::id<C>()]);

      size_t memoryIndex = computeIndex(ComponentTypeId::id<C>(), index, sizeof(C));

      assert(memoryIndex < CHUNK_SIZE);

      memcpy(&memory[memoryIndex], &component, sizeof(C));
   }
};

