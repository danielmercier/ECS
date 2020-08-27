#include "entity.hpp"

#include <chrono>
#include <iostream>

struct Position {
  int x;
  int y;

  explicit Position(int newX, int newY) : x(newX), y(newY) {}
};

struct Velocity {
  int x;
  int y;

  explicit Velocity(int newX, int newY) : x(newX), y(newY) {}
};

struct Render {
  int color;

  explicit Render(int newColor) : color(newColor) {}
};

struct Comflabulation {
  float thingy;
  bool mingy;
  int dingy;

  explicit Comflabulation(float t, bool m, int d)
      : thingy(t), mingy(m), dingy(d) {}
};

struct EnemyTag {};

template <typename... C> ChunkLayout computeChunkLayout() {
  return computeChunkLayout(computeArchetype<C...>());
}

template <typename C1, typename C2> void checkLayout(ChunkLayout layout) {
  ComponentType c1 = ComponentTypeId::id<C1>();
  ComponentType c2 = ComponentTypeId::id<C2>();

  assert(layout.componentStart[c1] == 0);

  // Oracle for capacity of first chunk family
  size_t capacity = CHUNK_SIZE / (sizeof(C1) + sizeof(C2));
  assert(capacity == layout.capacity);

  // Check if the component start fill the entire memory
  assert(capacity * sizeof(C1) <= layout.componentStart[c2]);
  assert(layout.componentStart[c2] + capacity * sizeof(C2) <= CHUNK_SIZE);
}

int main() {
  ComponentType renderId = ComponentTypeId::id<Render>();
  ComponentType positionId = ComponentTypeId::id<Position>();
  ComponentType velocityId = ComponentTypeId::id<Velocity>();

  Archetype archetypeOracle;
  archetypeOracle.set(renderId);
  archetypeOracle.set(positionId);
  archetypeOracle.set(velocityId);

  // all archetypes should be equal to the oracle
  Archetype archetype1 = computeArchetype<Position, Render, Velocity>();
  Archetype archetype2 = computeArchetype<Velocity, Position, Render>();
  Archetype archetype3 = computeArchetype<Render, Velocity, Position>();

  assert(archetype1 == archetypeOracle);
  assert(archetype2 == archetypeOracle);
  assert(archetype3 == archetypeOracle);

  // Chunks should have the same chunk family
  ChunkLayout kind1 = computeChunkLayout<Render, Position>();
  ChunkLayout kind2 = computeChunkLayout<Position, Render>();
  ChunkLayout kind3 = computeChunkLayout<Position, Velocity>();

  assert(kind1.archetype != kind3.archetype);

  // Let's see how the components are layed out in chunk families
  // Kind1 and Kind2 should have the same layout
  assert(kind1.capacity == kind2.capacity);
  assert(kind1.componentStart[renderId] == kind2.componentStart[renderId]);
  assert(kind1.componentStart[positionId] == kind2.componentStart[positionId]);

  checkLayout<Render, Position>(kind1);
  checkLayout<Render, Position>(kind2);
  checkLayout<Position, Velocity>(kind3);

  // Test entity manager
  EntityManager em;

  Entity e0 = em.createEntity<Position, Render>();
  Entity e1 = em.createEntity<Render, Position>();

  assert(e0 == 0);
  assert(e1 == 1);

  // e0 and e1 should be in the same chunk
  EntityLocation loc0 = em.getLocation(e0);
  EntityLocation loc1 = em.getLocation(e1);

  assert(loc0.chunkFamily == loc1.chunkFamily);
  assert(loc0.chunkIndex == loc1.chunkIndex);
  assert(loc0.chunkLine + 1 == loc1.chunkLine);
  assert((em.getArchetype(e0) == computeArchetype<Position, Render>()));
  assert((em.getArchetype(e1) == computeArchetype<Render, Position>()));

  Entity e2 = em.createEntity<Position, Velocity>();
  Entity e3 = em.createEntity<Velocity, Position>();
  assert(e2 == 2);
  assert(e3 == 3);

  EntityLocation loc2 = em.getLocation(e2);
  EntityLocation loc3 = em.getLocation(e3);

  // e2 and e3 should be in the same chunk, in a new chunk family
  assert(loc2.chunkFamily == loc3.chunkFamily);
  assert(loc2.chunkIndex == loc3.chunkIndex);
  assert(loc2.chunkLine + 1 == loc3.chunkLine);
  assert(loc2.chunkFamily != loc1.chunkFamily);
  assert((em.getArchetype(e2) == computeArchetype<Position, Velocity>()));
  assert((em.getArchetype(e3) == computeArchetype<Velocity, Position>()));

  Entity e4 = em.createEntity<Position, Velocity, Render>();
  assert(e4 == 4);

  EntityLocation loc4 = em.getLocation(e4);

  // e4 should be in a new chunk, in a new chunk family
  assert(
      (em.getArchetype(e4) == computeArchetype<Position, Velocity, Render>()));
  assert(loc4.chunkFamily != loc0.chunkFamily);
  assert(loc4.chunkFamily != loc2.chunkFamily);
  assert(loc4.chunkIndex == 0);
  assert(loc4.chunkLine == 0);

  // Check set and get component
  em.setComponent<Position>(e0, Position(10, 20));
  Position pos = em.getComponent<Position>(e0);
  assert(pos.x == 10 && pos.y == 20);

  em.setComponent<Render>(e0, Render(10));
  Render render = em.getComponent<Render>(e0);
  assert(render.color == 10);

  em.setComponent<Velocity>(e2, Velocity(1, 2));
  Velocity velocity = em.getComponent<Velocity>(e2);
  assert(velocity.x == 1 && velocity.y == 2);

  Entity e5 = em.createEntity<Position, Velocity, Render>(
      Position(1, 10), Velocity(10, 20), Render(10));

  Position position = em.getComponent<Position>(e5);
  velocity = em.getComponent<Velocity>(e5);
  render = em.getComponent<Render>(e5);

  assert(position.x == 1 && position.y == 10);
  assert(velocity.x == 10 && velocity.y == 20);
  assert(render.color == 10);

  em.setComponent<Position>(e0, Position(0, 0));
  em.setComponent<Position>(e1, Position(1, 1));
  em.setComponent<Position>(e2, Position(2, 2));
  em.setComponent<Position>(e3, Position(3, 3));
  em.setComponent<Position>(e4, Position(4, 4));
  em.setComponent<Position>(e5, Position(5, 5));

  // ITERATIONS
  // Iterate over all position
  // Use incr to check for all position added just above
  size_t incr = 0;
  em.each<Position>([&incr](Chunk &chunk) {
    for (size_t i = 0; i < chunk.count; i++) {
      Position &pos = chunk.getComponent<Position>(i);
      assert(pos.x == incr && pos.y == incr);
      incr++;

      pos.x += 1;
      pos.y += 1;
    }
  });

  incr = 1;
  em.each<Position>([&incr](Chunk &chunk) {
    for (size_t i = 0; i < chunk.count; i++) {
      Position &pos = chunk.getComponent<Position>(i);
      assert(pos.x == incr && pos.y == incr);
      incr++;
    }
  });

  em = EntityManager();

  // Let's create a big number of entities
  const int NB_ENTITIES = 10000000;

  auto start = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < NB_ENTITIES; i++) {
    Entity e;
    int x = static_cast<int>(i);

    if (i % 2 != 0) {
      e = em.createEntity<Position, Velocity>(Position(x, x), Velocity(x, x));
    } else {
      e = em.createEntity<Position, Velocity, Comflabulation>(
          Position(x, x), Velocity(x, x), Comflabulation(1.0f, true, 0));
    }

    assert(e == i);
  }

  auto finish = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = finish - start;
  std::cout << elapsed.count() << std::endl;

  start = std::chrono::high_resolution_clock::now();

  em.each<Position, Velocity>([](Chunk &chunk) {
    for (size_t i = 0; i < chunk.count; i++) {
      Position &pos = chunk.getComponent<Position>(i);
      Velocity vel = chunk.getComponent<Velocity>(i);

      pos.x += vel.x;
      pos.y += vel.y;
    }
  });

  em.each<Comflabulation>([](Chunk &chunk) {
    for (size_t i = 0; i < chunk.count; i++) {
      Comflabulation &conf = chunk.getComponent<Comflabulation>(i);

      conf.thingy *= 1.000001f;
      conf.mingy = !conf.mingy;
      conf.dingy++;
    }
  });

  finish = std::chrono::high_resolution_clock::now();
  elapsed = finish - start;

  std::cout << elapsed.count() << std::endl;

  // Test tag
  Entity enemy = em.createEntity<Position, Velocity, EnemyTag>();
  assert(enemy == NB_ENTITIES);

  int called = 0;

  em.each<EnemyTag>([&called](Chunk &chunk) {
    assert(chunk.count == 1);
    called++;
  });

  assert(called == 2);

  return 0;
}
