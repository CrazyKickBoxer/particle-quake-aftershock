/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aftershock_physics.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);        \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main() {
  CHECK(AS_PhysicsInit([](const char *s) { std::puts(s); }));
  std::vector<as_chunk_desc> chunks;
  for (int z = 0; z < 6; ++z)
    for (int x = 0; x < 6; ++x) {
      as_chunk_desc c = {{float(x * 24), 0, float(z * 24)},
                         {float((x + 1) * 24), 24, float((z + 1) * 24)},
                         0,
                         0,
                         z == 0 ? 1U : 0U};
      chunks.push_back(c);
    }
  const float floorMesh[] = {-1024, -1024, -24, 1024,  -1024, -24,
                             1024,  1024,  -24, -1024, -1024, -24,
                             1024,  1024,  -24, -1024, 1024,  -24};
  CHECK(AS_PhysicsBuild(chunks.data(), unsigned(chunks.size()), floorMesh, 2));
  CHECK(AS_PhysicsStats().chunks == 36);
  float p[] = {72, -2, 30};
  auto detached = AS_PhysicsExplode(p, 100, 10, 123);
  CHECK(detached > 0);
  CHECK(AS_PhysicsStats().bodies > 0);
  AS_PhysicsExplode(p, 100, 10, 124);
  CHECK(AS_PhysicsStats().errors == 0);
  for (int i = 0; i < 120; ++i)
    AS_PhysicsStep(1.0 / 120.0);
  auto poses = AS_PhysicsPoses();
  bool moved = false;
  for (unsigned i = 0; i < chunks.size(); ++i) {
    CHECK(std::isfinite(poses[i].center[2]));
    if (poses[i].detached &&
        std::abs(poses[i].center[2] - (chunks[i].mins[2] + 12)) > 2)
      moved = true;
  }
  CHECK(moved);
  CHECK(AS_PhysicsStats().errors == 0);
  const auto saved = AS_PhysicsStats().detached;
  const std::vector<as_chunk_pose> savedPoses(
      AS_PhysicsPoses(), AS_PhysicsPoses() + chunks.size());
  CHECK(AS_PhysicsSave("physics-test.pqas", 0x1234));
  CHECK(!AS_PhysicsLoad("physics-test.pqas", 0x5678));
  CHECK(AS_PhysicsStats().detached == saved);
  CHECK(AS_PhysicsBuild(chunks.data(), unsigned(chunks.size()), nullptr, 0));
  CHECK(AS_PhysicsLoad("physics-test.pqas", 0x1234));
  CHECK(AS_PhysicsStats().detached == saved);
  for (size_t i = 0; i < chunks.size(); ++i)
    for (int a = 0; a < 3; ++a)
      CHECK(std::abs(AS_PhysicsPoses()[i].center[a] - savedPoses[i].center[a]) <
            0.001f);
  {
    FILE *f = std::fopen("physics-test.pqas", "r+b");
    CHECK(f);
    CHECK(std::fseek(f, 40, SEEK_SET) == 0);
    int value = std::fgetc(f);
    CHECK(std::fseek(f, 40, SEEK_SET) == 0);
    std::fputc(value ^ 1, f);
    std::fclose(f);
  }
  CHECK(!AS_PhysicsLoad("physics-test.pqas", 0x1234));
  CHECK(AS_PhysicsStats().detached == saved);
  std::remove("physics-test.pqas");
  CHECK(AS_PhysicsSave("physics-test-\xC3\xA9.pqas", 0x1234));
  CHECK(AS_PhysicsLoad("physics-test-\xC3\xA9.pqas", 0x1234));
  std::filesystem::remove(
      std::filesystem::u8path("physics-test-\xC3\xA9.pqas"));
  CHECK(AS_PhysicsBuild(chunks.data(), unsigned(chunks.size()), nullptr, 0));
  CHECK(AS_PhysicsStats().detached == 0);
  std::vector<as_chunk_desc> tower;
  for (int z = 0; z < 3; ++z)
    tower.push_back({{0, 0, float(z * 24)},
                     {24, 24, float((z + 1) * 24)},
                     0,
                     0,
                     z == 0 ? 1U : 0U});
  CHECK(AS_PhysicsBuild(tower.data(), 3, nullptr, 0));
  float support[] = {12, -1, 5};
  CHECK(AS_PhysicsExplode(support, 24, 10, 10) == 3);
  CHECK(AS_PhysicsStats().bodies == 1);
  AS_PhysicsStep(.1);
  auto center = AS_PhysicsPoses()[1];
  CHECK(AS_PhysicsExplode(center.center, 20, 100, 11) == 0);
  CHECK(AS_PhysicsStats().bodies == 3);
  CHECK(AS_PhysicsStats().errors == 0);
  CHECK(AS_PhysicsSave("secondary-test.pqas", 123));
  CHECK(AS_PhysicsLoad("secondary-test.pqas", 123));
  CHECK(AS_PhysicsStats().bodies == 3);
  std::remove("secondary-test.pqas");
  as_chunk_desc overlapCube = {{0, 0, 0}, {24, 24, 24}, 0, 0, 1};
  CHECK(AS_PhysicsBuild(&overlapCube, 1, nullptr, 0));
  float cubeBlast[] = {12, -1, 12};
  CHECK(AS_PhysicsExplode(cubeBlast, 100, 100, 1) == 1);
  float boxMin[] = {-4, -4, -4}, boxMax[] = {4, 4, 4},
        overlapStart[] = {25, 12, 12}, escapeEnd[] = {40, 12, 12},
        enterEnd[] = {12, 12, 12}, fraction = 1, normal[3];
  CHECK(!AS_PhysicsSweep(overlapStart, escapeEnd, boxMin, boxMax, &fraction,
                         normal));
  CHECK(AS_PhysicsSweep(overlapStart, enterEnd, boxMin, boxMax, &fraction,
                        normal));
  CHECK(fraction == 0);
  CHECK(normal[0] > .9f);
  CHECK(AS_PhysicsBuild(&overlapCube, 1, nullptr, 0));
  CHECK(AS_PhysicsBrushMesh(2, floorMesh, 2));
  float brushOrigin[] = {0, 0, 0}, identityAxes[] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  CHECK(AS_PhysicsBrushPose(7, 2, brushOrigin, identityAxes, 1));
  float centralBlast[] = {12, 12, 12};
  CHECK(AS_PhysicsExplode(centralBlast, 100, 100, 1) == 1);
  for (int i = 0; i < 480; ++i)
    AS_PhysicsStep(1. / 120.);
  float restingZ = AS_PhysicsPoses()[0].center[2];
  CHECK(restingZ > -24 && restingZ < 12);
  for (int i = 1; i <= 120; ++i) {
    brushOrigin[2] = i * .2f;
    CHECK(AS_PhysicsBrushPose(7, 2, brushOrigin, identityAxes, 1));
    AS_PhysicsStep(1. / 120.);
  }
  CHECK(AS_PhysicsPoses()[0].center[2] > restingZ + 18);
  CHECK(AS_PhysicsStats().errors == 0);
  CHECK(AS_PhysicsBrushPose(7, 2, brushOrigin, identityAxes, 0));
  for (int i = 0; i < 120; ++i)
    AS_PhysicsStep(1. / 120.);
  CHECK(AS_PhysicsPoses()[0].center[2] < restingZ - 50);
  CHECK(AS_RemoveFile("no-such-aftershock-test-file.pqas"));

  // Multi-hit wall erosion: the first blast cannot remove the wall, and
  // cumulative bond damage survives save/load before any large collapse.
  CHECK(AS_PhysicsBuild(chunks.data(), uint32_t(chunks.size()), nullptr, 0));
  float progressiveHit[]={72,-1,60};
  AS_PhysicsExplodeProgressive(progressiveHit,96,6,44,4);
  auto firstHit=AS_PhysicsStats().detached;
  CHECK(firstHit>0 && firstHit<chunks.size()/2);
  CHECK(AS_PhysicsSave("progressive-test.pqas",456));
  CHECK(AS_PhysicsLoad("progressive-test.pqas",456));
  CHECK(AS_PhysicsStats().detached==firstHit);
  for(int shot=0;shot<16;++shot) AS_PhysicsExplodeProgressive(progressiveHit,96,6,45+shot,4);
  CHECK(AS_PhysicsStats().detached>firstHit);
  std::printf("Progressive wall: first=%u repeated=%u\n",firstHit,AS_PhysicsStats().detached);
  std::remove("progressive-test.pqas");
  // Pillar: chopping through the base releases the supported upper island.
  CHECK(AS_PhysicsBuild(tower.data(),uint32_t(tower.size()),nullptr,0));
  CHECK(AS_PhysicsExplodeProgressive(support,48,6,80,4)==0);
  for(int shot=0;shot<24;++shot)AS_PhysicsExplodeProgressive(support,48,6,81+shot,4);
  CHECK(AS_PhysicsStats().detached==tower.size());
  // Bridge slab, anchored at both ends. A center breach leaves the end supports.
  std::vector<as_chunk_desc> bridge;
  for(int x=0;x<9;++x)bridge.push_back({{float(x*24),0,72},{float((x+1)*24),48,88},2,0,(x==0||x==8)?1u:0u});
  CHECK(AS_PhysicsBuild(bridge.data(),uint32_t(bridge.size()),nullptr,0));
  float bridgeHit[]={108,24,90};
  CHECK(AS_PhysicsExplodeProgressive(bridgeHit,64,6,120,4)<bridge.size()/2);
  for(int shot=0;shot<20;++shot)AS_PhysicsExplodeProgressive(bridgeHit,64,6,121+shot,4);
  CHECK(AS_PhysicsStats().detached>0 && AS_PhysicsStats().detached<bridge.size());
  CHECK(!AS_PhysicsPoses()[0].detached && !AS_PhysicsPoses()[8].detached);
  CHECK(AS_PhysicsStats().errors==0);
  AS_PhysicsShutdown();
  std::puts("PASS: Blast repeated fracture, PhysX motion, finite transforms, "
            "exact save/load positions, corruption rejection, Unicode paths, "
            "unsupported island collapse, secondary split, clean reset");
  return 0;
}
