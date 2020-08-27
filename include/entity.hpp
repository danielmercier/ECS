#pragma once

#include <cstring>
#include <cstdint>
#include <bitset>
#include <array>
#include <vector>
#include <tuple>
#include <functional>
#include <cassert>
#include <optional>
#include <memory>

#include <iostream>

#include "component.hpp"
#include "chunk.hpp"

// A simple type alias
using Entity = uint64_t;

// A chunk family is a list of chunk categorized by their archetype
struct ChunkFamily
{
   const Archetype archetype;
   // TODO: maybe a list would be better here
   std::vector<Chunk> chunks;

   ChunkFamily(Archetype newArchetype):
      archetype(newArchetype)
   {
   }
};

// Structure to locate an entity in the chunk data structure
struct EntityLocation
{
   // The index in the chunk family list
   size_t chunkFamily;

   // The index in the chunk list (in one chunk family)
   size_t chunkIndex;

   // The index inside the chunk
   size_t chunkLine;
};

// Manages entities in the chunks
struct EntityManager
{
public:
   EntityManager() : currentEntity(0) {}

   // Create an uninitialized entity, setComponent can be called to initialize
   // it
   template<typename... Cs>
   inline Entity createEntity()
   {
      return createEntity(computeArchetype<Cs...>());
   }

   // Create an entity with a fixed archetype, and initialized with given
   // components
   template<typename... Cs>
   inline Entity createEntity(const Cs&... components)
   {
      Entity e = createEntity<Cs...>();

      setComponents<Cs...>(e, components...);

      return e;
   }
   
   // Set the given comoponent of the given entity
   template<typename C>
   inline void setComponent(Entity e, const C& component)
   {
      EntityLocation loc = getLocation(e);
      get(loc).setComponent(loc.chunkLine, component);
   }

   // Get the given comoponent of the given entity
   template<typename C>
   inline C& getComponent(Entity e)
   {
      EntityLocation loc = getLocation(e);
      return get(loc).getComponent<C>(loc.chunkLine);
   }

   // Call the given function with all the chunks that contains the given
   // components
   template<typename... Cs>
   inline void each(std::function<void(Chunk& chunk)> exec)
   {
      return each(computeArchetype<Cs...>(), exec);
   }

   // Get the location of an entity in the chunk data structure
   inline EntityLocation getLocation(Entity e) const
   {
      assert(isValid(e));
      return entityToLocation[e].value();
   }

   // Get the archetype of an entity
   inline Archetype getArchetype(Entity e)
   {
      EntityLocation loc = getLocation(e);
      return chunkFamilies[loc.chunkFamily].archetype;
   }

   // Return if the given entity is a valid entity
   inline bool isValid(Entity e) const
   {
      return e < entityToLocation.size() && entityToLocation[e].has_value();
   }

private:
   size_t currentEntity;

   // The chunk data structure.
   // Each chunk family have a list of chunks that all have the same archetype
   std::vector<ChunkFamily> chunkFamilies;

   // Keep track of where an entity is in the chunkFamilies
   std::vector<std::optional<EntityLocation>> entityToLocation;
   
   // Keep ownership of al chunk kinds created
   std::vector<std::unique_ptr<ChunkLayout>> layouts;

   std::optional<size_t> chunkFamilyIndex(Archetype archetype)
   {
      std::optional<size_t> familyIndex;

      for (size_t i = 0; i < chunkFamilies.size(); i++)
      {
         if (chunkFamilies[i].archetype == archetype)
         {
            familyIndex = i;
            break;
         }
      }

      return familyIndex;
   }

   EntityLocation availableLocation(Archetype archetype)
   {
      std::optional<size_t> familyIndex = chunkFamilyIndex(archetype);

      if(familyIndex.has_value())
      {
         ChunkFamily& family = chunkFamilies[familyIndex.value()];
         size_t lastChunk = family.chunks.size() - 1;
         // Check if the last chunk is ok
         Chunk& last = family.chunks[lastChunk];
         if (last.layout->capacity > last.count)
         {
            // Fine, return it
            return { familyIndex.value(), lastChunk, last.count };
         }
         else
         {
            // Need a new chunk, same pointer for ChecKind
            family.chunks.emplace_back(last.layout);
            return { familyIndex.value(), lastChunk + 1, 0 };
         }
      }
      else
      {
         // Not yet any chunk for this archetype
         familyIndex = chunkFamilies.size();
         chunkFamilies.emplace_back(archetype);

         size_t layoutIndex = layouts.size();
         layouts.emplace_back(new ChunkLayout(computeChunkLayout(archetype)));
         ChunkLayout* layout = layouts[layoutIndex].get();
         
         // First chunk
         chunkFamilies[familyIndex.value()].chunks.emplace_back(layout);
         return { familyIndex.value(), 0, 0 };
      }
   }
   
   inline Chunk& get(EntityLocation loc) noexcept
   {
      return chunkFamilies[loc.chunkFamily].chunks[loc.chunkIndex];
   }

   void pushEntity(Entity e, Archetype archetype)
   {
      EntityLocation loc = availableLocation(archetype);
      entityToLocation[e] = loc;
      get(loc).count++;
   }
   
   // TODO: might be usefull to add a create entity function that creates a
   // bunch of entities instead of one by one. Woule increase perfs. This
   // might be done with a command buffer to prepare some creation before
   // submitting.
   Entity createEntity(Archetype archetype)
   {
      Entity e = currentEntity;

      // Add a new invalid entry to the entityToLocation vector
      entityToLocation.push_back(std::nullopt);

      pushEntity(e, archetype);

      currentEntity++;
      return e;
   }

   template<typename C>
   inline void setComponents(Entity e, const C& component)
   {
      setComponent<C>(e, component);
   }

   template<typename C1, typename C2, typename... Cs>
   inline void setComponents(Entity e, const C1& component1,
         const C2& component2, const Cs&... components)
   {
      setComponent<C1>(e, component1);
      setComponents<C2, Cs...>(e, component2, components...);
   }

   void each(Archetype archetype, std::function<void(Chunk& chunk)> exec)
   {
      for (auto& chunkFamily : chunkFamilies)
      {
         // First loop over chunk families to find one with a suitable archetype
         if ((chunkFamily.archetype & archetype) == archetype)
         {
            // We found one, loop over all chunks in this family
            for (Chunk& chunk : chunkFamily.chunks)
            {
               exec(chunk);
            }
         }
      }
   }
};
