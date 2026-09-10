/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#include "aftershock_physics.h"
#include "NvBlast.h"
#include "PxPhysicsAPI.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

using namespace physx;
namespace {
constexpr float scale = 1.0f / 32.0f;
constexpr uint32_t capacity = 8192;
as_log_fn logger;
as_stats stats{};
void log(const char *s) {
  if (logger)
    logger(s);
}
void blastLog(int severity, const char *message, const char *, int) {
  if (severity <= 1)
    ++stats.errors;
  log(message);
}
class Errors final : public PxErrorCallback {
  void reportError(PxErrorCode::Enum code, const char *message, const char *,
                   int) override {
    if (code != PxErrorCode::eDEBUG_INFO && code != PxErrorCode::eDEBUG_WARNING)
      ++stats.errors;
    log(message);
  }
} errors;
class Allocator final : public PxAllocatorCallback {
public:
  std::atomic<uint64_t> live{0}, peak{0};
  void *allocate(size_t size, const char *, const char *, int) override {
    auto *base = static_cast<uint64_t *>(_aligned_malloc(size + 16, 16));
    if (!base)
      return nullptr;
    base[0] = size;
    uint64_t current = live.fetch_add(size) + size, previous = peak.load();
    while (current > previous &&
           !peak.compare_exchange_weak(previous, current)) {
    }
    return reinterpret_cast<unsigned char *>(base) + 16;
  }
  void deallocate(void *p) override {
    if (!p)
      return;
    auto *base =
        reinterpret_cast<uint64_t *>(static_cast<unsigned char *>(p) - 16);
    live.fetch_sub(base[0]);
    _aligned_free(base);
  }
} allocator;
PxFoundation *foundation;
PxPhysics *physics;
PxDefaultCpuDispatcher *dispatcher;
PxScene *scene;
PxMaterial *material;
PxRigidStatic *environment;
struct Aligned {
  void *p = nullptr;
  ~Aligned() { _aligned_free(p); }
  void clear() {
    _aligned_free(p);
    p = nullptr;
  }
  void resize(size_t size) {
    _aligned_free(p);
    p = _aligned_malloc(std::max(size, size_t(16)), 16);
  }
};
Aligned assetMem, familyMem, scratch;
NvBlastAsset *asset;
NvBlastFamily *family;
std::vector<NvBlastActor *> actors;
std::vector<as_chunk_desc> chunks;
std::vector<as_chunk_pose> poses;
std::vector<PxRigidDynamic *> chunkBody;
std::vector<PxVec3> localCenters;
std::vector<PxRigidStatic *> fixedChunks;
std::vector<NvBlastBondDesc> bonds;
std::vector<PxRigidDynamic *> bodies;
std::vector<PxTriangleMesh *> brushMeshes;
std::vector<PxRigidDynamic *> brushBodies;
std::vector<uint32_t> brushModels;
struct Explosion {
  float origin[3], radius, damage;
  uint32_t seed;
};
std::vector<Explosion> history;
std::vector<float> originalTriangles;
std::vector<as_impact> pendingContacts, visualContacts;
class Contacts final : public PxSimulationEventCallback {
public:
  void onConstraintBreak(PxConstraintInfo *, PxU32) override {}
  void onWake(PxActor **, PxU32) override {}
  void onSleep(PxActor **, PxU32) override {}
  void onTrigger(PxTriggerPair *, PxU32) override {}
  void onAdvance(const PxRigidBody *const *, const PxTransform *,
                 PxU32) override {}
  void onContact(const PxContactPairHeader &, const PxContactPair *pairs,
                 PxU32 count) override {
    for (PxU32 i = 0; i < count && pendingContacts.size() < 64; ++i) {
      PxContactPairPoint points[16];
      PxU32 n = pairs[i].extractContacts(points, 16);
      float impulse = 0;
      PxVec3 position(0);
      for (PxU32 j = 0; j < n; ++j) {
        float magnitude = points[j].impulse.magnitude();
        if (magnitude > impulse) {
          impulse = magnitude;
          position = points[j].position;
        }
      }
      if (impulse > 40)
        pendingContacts.push_back(
            {{position.x / scale, position.y / scale, position.z / scale},
             impulse});
    }
  }
} contacts;
PxFilterFlags contactFilter(PxFilterObjectAttributes a, PxFilterData af,
                            PxFilterObjectAttributes b, PxFilterData bf,
                            PxPairFlags &flags, const void *block, PxU32 size) {
  auto result =
      PxDefaultSimulationFilterShader(a, af, b, bf, flags, block, size);
  if (!PxFilterObjectIsTrigger(a) && !PxFilterObjectIsTrigger(b))
    flags |=
        PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_CONTACT_POINTS;
  return result;
}
double accumulator;

PxVec3 vec(const float *p) { return PxVec3(p[0], p[1], p[2]) * scale; }
PxVec3 center(const as_chunk_desc &c) {
  return (vec(c.mins) + vec(c.maxs)) * 0.5f;
}
PxVec3 half(const as_chunk_desc &c) {
  return (vec(c.maxs) - vec(c.mins)) * 0.5f;
}
float distance(const PxVec3 &a, const PxVec3 &b) { return (a - b).magnitude(); }
uint32_t hash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  return x ^ (x >> 16);
}
std::wstring wide(const char *s) {
  int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, nullptr, 0);
  if (n <= 0)
    return {};
  std::wstring result(n, L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, result.data(), n);
  result.resize(n - 1);
  return result;
}

void destroyScene() {
  pendingContacts.clear();
  visualContacts.clear();
  for (auto *body : brushBodies)
    if (body)
      body->release();
  for (auto *mesh : brushMeshes)
    if (mesh)
      mesh->release();
  brushBodies.clear();
  brushMeshes.clear();
  brushModels.clear();
  for (auto *body : bodies)
    body->release();
  for (auto *body : fixedChunks)
    if (body)
      body->release();
  if (environment)
    environment->release();
  if (scene)
    scene->release();
  scene = nullptr;
  environment = nullptr;
  if (dispatcher)
    dispatcher->release();
  dispatcher = nullptr;
  if (material)
    material->release();
  material = nullptr;
  actors.clear();
  chunks.clear();
  poses.clear();
  chunkBody.clear();
  localCenters.clear();
  fixedChunks.clear();
  bonds.clear();
  bodies.clear();
  history.clear();
  originalTriangles.clear();
  asset = nullptr;
  family = nullptr;
  assetMem.clear();
  familyMem.clear();
  scratch.clear();
  accumulator = 0;
}
void updatePoses() {
  stats.awake = 0;
  for (auto *body : bodies)
    if (!body->isSleeping())
      ++stats.awake;
  for (size_t i = 0; i < chunks.size(); ++i) {
    PxTransform t = chunkBody[i] ? chunkBody[i]->getGlobalPose()
                                 : PxTransform(center(chunks[i]));
    PxVec3 c = chunkBody[i] ? t.transform(localCenters[i]) : t.p;
    poses[i] = {{c.x / scale, c.y / scale, c.z / scale},
                {t.q.x, t.q.y, t.q.z, t.q.w},
                chunkBody[i] ? 1U : 0U,
                chunkBody[i] && chunkBody[i]->isSleeping() ? 1U : 0U};
  }
  stats.bodies = static_cast<uint32_t>(bodies.size());
}
void detach(NvBlastActor *actor, const PxVec3 &origin, float strength,
            uint32_t seed) {
  if (NvBlastActorHasExternalBonds(actor, blastLog))
    return;
  uint32_t n = NvBlastActorGetVisibleChunkCount(actor, blastLog);
  std::vector<uint32_t> ids(n);
  NvBlastActorGetVisibleChunkIndices(ids.data(), n, actor, blastLog);
  if (ids.empty())
    return;
  auto *parent = chunkBody[ids[0]];
  if (parent && parent->getNbShapes() == ids.size())
    return;
  PxVec3 c(0);
  uint32_t valid = 0;
  for (auto id : ids)
    if (id < chunks.size()) {
      c += chunkBody[id]
               ? chunkBody[id]->getGlobalPose().transform(localCenters[id])
               : center(chunks[id]);
      ++valid;
    }
  if (!valid)
    return;
  c /= float(valid);
  PxQuat orientation = parent ? parent->getGlobalPose().q : PxQuat(PxIdentity);
  auto *body = physics->createRigidDynamic(PxTransform(c, orientation));
  if (!body) {
    ++stats.errors;
    return;
  }
  body->userData = reinterpret_cast<void *>(uintptr_t(ids[0] + 1));
  float density = 0;
  for (auto id : ids)
    if (id < chunks.size()) {
      auto *old = chunkBody[id];
      PxVec3 current = old ? old->getGlobalPose().transform(localCenters[id])
                           : center(chunks[id]);
      auto *shape = PxRigidActorExt::createExclusiveShape(
          *body, PxBoxGeometry(half(chunks[id]) * 0.985f), *material);
      localCenters[id] = orientation.rotateInv(current - c);
      shape->setLocalPose(PxTransform(localCenters[id]));
      PxFilterData query(1, 0, 0, 0);
      shape->setQueryFilterData(query);
      chunkBody[id] = body;
      if (fixedChunks[id]) {
        fixedChunks[id]->release();
        fixedChunks[id] = nullptr;
        ++stats.detached;
      }
      density += chunks[id].material == 1   ? 240.0f
                 : chunks[id].material == 2 ? 35.0f
                                            : 90.0f;
    }
  PxRigidBodyExt::updateMassAndInertia(*body, density / valid);
  body->setAngularDamping(0.35f);
  body->setLinearDamping(0.08f);
  body->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
  body->setMaxDepenetrationVelocity(4.0f);
  scene->addActor(*body);
  bodies.push_back(body);
  PxVec3 direction = c - origin;
  if (direction.magnitudeSquared() < 0.00001f)
    direction = PxVec3(0, 0, 1);
  direction.normalize();
  direction.z += 0.3f;
  body->setLinearVelocity(parent ? parent->getLinearVelocity() +
                                       parent->getAngularVelocity().cross(
                                           c - parent->getGlobalPose().p)
                                 : direction * strength);
  const uint32_t h = hash(seed + uint32_t(bodies.size()));
  body->setAngularVelocity(parent ? parent->getAngularVelocity()
                                  : PxVec3(float(h & 255) / 64.0f - 2,
                                           float((h >> 8) & 255) / 64.0f - 2,
                                           float((h >> 16) & 255) / 64.0f - 2));
}
void splitAll(const PxVec3 &origin, float strength, uint32_t seed) {
  std::vector<NvBlastActor *> result;
  std::vector<NvBlastActor *> newActors(chunks.size() + 1);
  for (auto *actor : actors) {
    scratch.resize(NvBlastActorGetRequiredScratchForSplit(actor, blastLog));
    NvBlastActorSplitEvent event{};
    event.newActors = newActors.data();
    uint32_t n = NvBlastActorSplit(&event, actor,
                                   static_cast<uint32_t>(newActors.size()),
                                   scratch.p, blastLog, nullptr);
    if (!n)
      result.push_back(actor);
    else
      for (uint32_t i = 0; i < n; ++i)
        result.push_back(newActors[i]);
  }
  actors.swap(result);
  for (auto *actor : actors)
    detach(actor, origin, strength, seed);
  bodies.erase(std::remove_if(bodies.begin(), bodies.end(),
                              [](PxRigidDynamic *body) {
                                if (std::find(chunkBody.begin(),
                                              chunkBody.end(),
                                              body) != chunkBody.end())
                                  return false;
                                body->release();
                                return true;
                              }),
               bodies.end());
}
} // namespace

int AS_PhysicsInit(as_log_fn callback) {
  logger = callback;
  if (foundation)
    return 1;
  foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errors);
  if (!foundation)
    return 0;
  PxTolerancesScale tolerances;
  physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, tolerances, false);
  if (!physics) {
    foundation->release();
    foundation = nullptr;
    return 0;
  }
  PxInitExtensions(*physics, nullptr);
  log("Aftershock: PhysX 5.6 CPU and NVIDIA Blast initialized.");
  return 1;
}
void AS_PhysicsShutdown() {
  destroyScene();
  if (physics) {
    PxCloseExtensions();
    physics->release();
    physics = nullptr;
  }
  if (foundation) {
    foundation->release();
    foundation = nullptr;
  }
  stats = {};
}
int AS_PhysicsBuild(const as_chunk_desc *input, uint32_t count,
                    const float *triangles, uint32_t triangleCount) {
  if (!physics || count > capacity)
    return 0;
  destroyScene();
  stats = {};
  allocator.peak = allocator.live.load();
  if (count)
    chunks.assign(input, input + count);
  if (triangleCount)
    originalTriangles.assign(triangles, triangles + size_t(triangleCount) * 9);
  poses.resize(count);
  chunkBody.resize(count);
  localCenters.resize(count);
  fixedChunks.resize(count);
  PxSceneDesc desc(physics->getTolerancesScale());
  desc.gravity = PxVec3(0, 0, -800.0f * scale);
  dispatcher = PxDefaultCpuDispatcherCreate(2);
  desc.cpuDispatcher = dispatcher;
  desc.filterShader = contactFilter;
  desc.simulationEventCallback = &contacts;
  desc.flags |=
      PxSceneFlag::eENABLE_CCD | PxSceneFlag::eENABLE_ENHANCED_DETERMINISM;
  scene = physics->createScene(desc);
  material = physics->createMaterial(0.7f, 0.6f, 0.12f);
  if (!scene || !material)
    return 0;
  if (triangleCount) {
    std::vector<PxVec3> vertices(triangleCount * 3);
    std::vector<uint32_t> indices(triangleCount * 3);
    for (uint32_t i = 0; i < triangleCount * 3; ++i) {
      vertices[i] = vec(triangles + i * 3);
      indices[i] = i;
    }
    PxTriangleMeshDesc meshDesc;
    meshDesc.points.count = uint32_t(vertices.size());
    meshDesc.points.stride = sizeof(PxVec3);
    meshDesc.points.data = vertices.data();
    meshDesc.triangles.count = triangleCount;
    meshDesc.triangles.stride = 3 * sizeof(uint32_t);
    meshDesc.triangles.data = indices.data();
    PxCookingParams params(physics->getTolerancesScale());
    auto *mesh = PxCreateTriangleMesh(params, meshDesc,
                                      physics->getPhysicsInsertionCallback());
    if (!mesh)
      return 0;
    environment = physics->createRigidStatic(PxTransform(PxIdentity));
    PxRigidActorExt::createExclusiveShape(
        *environment,
        PxTriangleMeshGeometry(mesh, PxMeshScale(),
                               PxMeshGeometryFlag::eDOUBLE_SIDED),
        *material);
    scene->addActor(*environment);
    mesh->release();
  }
  if (!count)
    return 1;
  std::vector<NvBlastChunkDesc> chunkDescs(count);
  for (uint32_t i = 0; i < count; ++i) {
    PxVec3 c = center(chunks[i]), h = half(chunks[i]);
    if (h.minElement() <= 0)
      return 0;
    chunkDescs[i] = {{c.x, c.y, c.z},
                     8 * h.x * h.y * h.z,
                     UINT32_MAX,
                     NvBlastChunkDesc::SupportFlag,
                     i};
    fixedChunks[i] = physics->createRigidStatic(PxTransform(c));
    fixedChunks[i]->userData = reinterpret_cast<void *>(uintptr_t(i + 1));
    PxRigidActorExt::createExclusiveShape(*fixedChunks[i], PxBoxGeometry(h),
                                          *material);
    scene->addActor(*fixedChunks[i]);
    if (chunks[i].anchored) {
      NvBlastBondDesc b{};
      b.chunkIndices[0] = i;
      b.chunkIndices[1] = UINT32_MAX;
      b.bond.area = 1;
      b.bond.centroid[0] = c.x;
      b.bond.centroid[1] = c.y;
      b.bond.centroid[2] = c.z;
      bonds.push_back(b);
    }
    for (uint32_t j = 0; j < i; ++j) {
      int touching = 0;
      bool adjacent = true;
      for (int a = 0; a < 3; ++a) {
        float overlap = std::min(chunks[i].maxs[a], chunks[j].maxs[a]) -
                        std::max(chunks[i].mins[a], chunks[j].mins[a]);
        if (overlap < -0.1f)
          adjacent = false;
        if (std::abs(overlap) < 0.1f)
          ++touching;
      }
      if (adjacent && touching == 1) {
        NvBlastBondDesc b{};
        b.chunkIndices[0] = i;
        b.chunkIndices[1] = j;
        b.bond.area = 1;
        PxVec3 mid = (c + center(chunks[j])) * 0.5f,
               normal = (c - center(chunks[j])).getNormalized();
        b.bond.centroid[0] = mid.x;
        b.bond.centroid[1] = mid.y;
        b.bond.centroid[2] = mid.z;
        b.bond.normal[0] = normal.x;
        b.bond.normal[1] = normal.y;
        b.bond.normal[2] = normal.z;
        bonds.push_back(b);
      }
    }
  }
  NvBlastAssetDesc assetDesc{count, chunkDescs.data(), uint32_t(bonds.size()),
                             bonds.data()};
  assetMem.resize(NvBlastGetAssetMemorySize(&assetDesc, blastLog));
  scratch.resize(NvBlastGetRequiredScratchForCreateAsset(&assetDesc, blastLog));
  asset = NvBlastCreateAsset(assetMem.p, &assetDesc, scratch.p, blastLog);
  if (!asset)
    return 0;
  familyMem.resize(NvBlastAssetGetFamilyMemorySize(asset, blastLog));
  family = NvBlastAssetCreateFamily(familyMem.p, asset, blastLog);
  NvBlastActorDesc actorDesc{1.0f, nullptr, 100000.0f, nullptr};
  scratch.resize(
      NvBlastFamilyGetRequiredScratchForCreateFirstActor(family, blastLog));
  auto *actor =
      NvBlastFamilyCreateFirstActor(family, &actorDesc, scratch.p, blastLog);
  if (!actor)
    return 0;
  actors.push_back(actor);
  stats.chunks = count;
  stats.bonds = uint32_t(bonds.size());
  updatePoses();
  return 1;
}
static uint32_t explodeInternal(const float origin[3], float radius, float damage,
                           uint32_t seed, bool progressive) {
  if (!scene || !asset || !std::isfinite(radius) || !std::isfinite(damage) ||
      radius <= 0 || damage <= 0)
    return 0;
  for (int a = 0; a < 3; ++a)
    if (!std::isfinite(origin[a]))
      return 0;
  if (history.size() >= 65536) {
    log("Aftershock event history capacity reached");
    return 0;
  }
  history.push_back({{origin[0], origin[1], origin[2]}, radius, damage, seed});
  const auto before = stats.detached;
  ++stats.events;
  PxVec3 p = vec(origin);
  const float r = radius * scale;
  const NvBlastSupportGraph graph =
      NvBlastAssetGetSupportGraph(asset, blastLog);
  const NvBlastBond *assetBonds = NvBlastAssetGetBonds(asset, blastLog);
  uint32_t chip=UINT32_MAX;float chipDistance=r*.6f;
  if(progressive)for(uint32_t node=0;node<graph.nodeCount;++node){
    uint32_t id=graph.chunkIndices[node];
    if(id>=chunks.size()||chunks[id].anchored||chunkBody[id])continue;
    // A broad wall can lose one cell safely. Do not punch through the
    // sole load path of a narrow pillar or bridge on its first hit.
    if(graph.adjacencyPartition[node+1]-graph.adjacencyPartition[node]<4)continue;
    float d=distance(center(chunks[id]),p);
    if(d<chipDistance){chipDistance=d;chip=id;}
  }
  std::vector<NvBlastBondFractureData> fractures;
  for (uint32_t i = 0; i < graph.nodeCount; ++i)
    for (uint32_t k = graph.adjacencyPartition[i];
         k < graph.adjacencyPartition[i + 1]; ++k) {
      const uint32_t j = graph.adjacentNodeIndices[k];
      if (j < i)
        continue;
      const auto &b = assetBonds[graph.adjacentBondIndices[k]];
      PxVec3 c(b.centroid[0], b.centroid[1], b.centroid[2]);
      uint32_t chunk = graph.chunkIndices[i];
      if (chunk < chunks.size() && chunkBody[chunk])
        c = chunkBody[chunk]->getGlobalPose().transform(
            localCenters[chunk] + c - center(chunks[chunk]));
      const float d = distance(c, p);
      bool occluded = false;
      if (d < r && d > 0.01f) {
        PxRaycastBuffer hit;
        PxQueryFilterData filter(PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC);
        if (scene->raycast(p, (c - p) / d, d, hit, PxHitFlag::eDEFAULT,
                           filter)) {
          uintptr_t owner =
              reinterpret_cast<uintptr_t>(hit.block.actor->userData);
          occluded = chunk >= chunks.size() || !owner ||
                     owner > chunks.size() ||
                     (chunks[owner - 1].surface != chunks[chunk].surface &&
                      hit.block.actor != chunkBody[chunk]);
        }
      }
      if (d < r && !occluded) {
        float strength=damage*(1.0f-d/r);
        if(progressive) {
          uint32_t other=graph.chunkIndices[j];
          bool support=chunk>=chunks.size() || other>=chunks.size();

          float toughness=chunk<chunks.size()?(chunks[chunk].material==1?1.5f:chunks[chunk].material==2?.7f:1.f):1.f;
          strength=std::min(.45f,strength/(toughness*(support?1.75f:1.f)));
          // A few stable, non-support chips pop near the impact. The load-bearing
          // network always accumulates damage over multiple blasts.
          if(!support && chip!=UINT32_MAX && (chunk==chip||other==chip))strength=1.05f;
        }
        fractures.push_back({0,i,j,strength});
      }
    }
  if (!fractures.empty()) {
    std::vector<uint32_t> nodes(graph.nodeCount);
    std::vector<bool> owned(graph.nodeCount);
    std::vector<NvBlastBondFractureData> local;
    for (auto *actor : actors) {
      std::fill(owned.begin(), owned.end(), false);
      local.clear();
      uint32_t n = NvBlastActorGetGraphNodeIndices(
          nodes.data(), uint32_t(nodes.size()), actor, blastLog);
      for (uint32_t i = 0; i < n; ++i)
        owned[nodes[i]] = true;
      /* Blast omits the external world node from actor node iteration. */
      for (uint32_t i = 0; i < graph.nodeCount; ++i)
        if (graph.chunkIndices[i] == UINT32_MAX)
          owned[i] = true;
      for (const auto &f : fractures)
        if (owned[f.nodeIndex0] && owned[f.nodeIndex1])
          local.push_back(f);
      if (!local.empty()) {
        NvBlastFractureBuffers commands{uint32_t(local.size()), 0, local.data(),
                                        nullptr};
        NvBlastActorApplyFracture(nullptr, actor, &commands, blastLog, nullptr);
      }
    }
    splitAll(p, 8.0f, seed);
  }
  for (auto *body : bodies) {
    PxVec3 d = body->getGlobalPose().p - p;
    float len = d.magnitude();
    if (len > 0.001f && len < r)
      body->addForce(d / len * (1 - len / r) * 5.0f,
                     PxForceMode::eVELOCITY_CHANGE);
  }
  updatePoses();
  return stats.detached - before;
}
uint32_t AS_PhysicsExplode(const float origin[3],float radius,float damage,uint32_t seed) {
  return explodeInternal(origin,radius,damage,seed,false);
}
uint32_t AS_PhysicsExplodeProgressive(const float origin[3],float radius,float damage,uint32_t seed,float hits) {
  if(!std::isfinite(hits))return 0;
  return explodeInternal(origin,radius,damage/(6.f*std::clamp(hits,2.f,12.f)),seed,true);
}
void AS_PhysicsStep(double seconds) {
  if (!scene || !std::isfinite(seconds) || seconds <= 0)
    return;
  auto begin = std::chrono::steady_clock::now();
  accumulator += std::min(seconds, 0.1);
  for (int steps = 0; accumulator >= 1.0 / 120.0 && steps < 12; ++steps) {
    scene->simulate(1.0f / 120.0f);
    scene->fetchResults(true);
    /* Bound contact-driven damage; callbacks only collect immutable inputs. */
    unsigned secondary = 0;
    for (const auto &impact : pendingContacts) {
      if (visualContacts.size() < 64)
        visualContacts.push_back(impact);
      if (impact.energy > 180 && secondary++ < 4)
        AS_PhysicsExplode(impact.origin, 40,
                          std::min(2.0f, impact.energy * 0.002f),
                          hash(stats.events + 1));
    }
    pendingContacts.clear();
    accumulator -= 1.0 / 120.0;
  }
  updatePoses();
  stats.simulation_ms = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - begin)
                            .count();
}
const as_chunk_pose *AS_PhysicsPoses() { return poses.data(); }
uint32_t AS_PhysicsTakeImpacts(as_impact *output, uint32_t count) {
  count = std::min(count, uint32_t(visualContacts.size()));
  std::copy_n(visualContacts.begin(), count, output);
  visualContacts.erase(visualContacts.begin(), visualContacts.begin() + count);
  return count;
}
as_stats AS_PhysicsStats() {
  stats.allocator_live_bytes = allocator.live;
  stats.allocator_peak_bytes = allocator.peak;
  return stats;
}
int AS_PhysicsBrushMesh(uint32_t model, const float *triangles,
                        uint32_t count) {
  if (!scene || model >= 8192 || !count || count > 1048576)
    return 0;
  if (brushMeshes.size() <= model)
    brushMeshes.resize(model + 1);
  if (brushMeshes[model])
    return 1;
  std::vector<PxVec3> vertices(size_t(count) * 3);
  std::vector<uint32_t> indices(vertices.size());
  for (uint32_t i = 0; i < vertices.size(); ++i) {
    vertices[i] = vec(triangles + i * 3);
    indices[i] = i;
  }
  PxTriangleMeshDesc desc;
  desc.points.count = uint32_t(vertices.size());
  desc.points.stride = sizeof(PxVec3);
  desc.points.data = vertices.data();
  desc.triangles.count = count;
  desc.triangles.stride = 12;
  desc.triangles.data = indices.data();
  brushMeshes[model] =
      PxCreateTriangleMesh(PxCookingParams(physics->getTolerancesScale()), desc,
                           physics->getPhysicsInsertionCallback());
  return brushMeshes[model] != nullptr;
}
int AS_PhysicsBrushPose(uint32_t entity, uint32_t model, const float origin[3],
                        const float axes[9], int enabled) {
  if (!scene || entity >= 8192)
    return 0;
  if (brushBodies.size() <= entity) {
    brushBodies.resize(entity + 1);
    brushModels.resize(entity + 1);
  }
  auto *&body = brushBodies[entity];
  if (!enabled) {
    if (body)
      body->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, true);
    return 1;
  }
  if (model >= brushMeshes.size() || !brushMeshes[model])
    return 0;
  if (body && brushModels[entity] != model) {
    body->release();
    body = nullptr;
  }
  PxMat33 basis(PxVec3(axes[0], axes[1], axes[2]),
                PxVec3(axes[3], axes[4], axes[5]),
                PxVec3(axes[6], axes[7], axes[8]));
  PxTransform pose(vec(origin), PxQuat(basis).getNormalized());
  if (!body) {
    body = physics->createRigidDynamic(pose);
    if (!body)
      return 0;
    body->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
    if (!PxRigidActorExt::createExclusiveShape(
            *body,
            PxTriangleMeshGeometry(brushMeshes[model], PxMeshScale(),
                                   PxMeshGeometryFlag::eDOUBLE_SIDED),
            *material)) {
      body->release();
      body = nullptr;
      return 0;
    }
    scene->addActor(*body);
    brushModels[entity] = model;
  }
  body->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, false);
  body->setKinematicTarget(pose);
  return 1;
}
void AS_PhysicsPush(const float origin[3], const float velocity[3],
                    const float mins[3], const float maxs[3], double seconds) {
  if (!scene || seconds <= 0)
    return;
  PxVec3 movement = vec(velocity);
  movement.z = 0;
  float speed = movement.magnitude();
  if (speed < .1f)
    return;
  PxVec3 direction = movement / speed;
  float dt = float(std::min(seconds, .05));
  PxVec3 offset = (vec(mins) + vec(maxs)) * .5f;
  PxVec3 extent = ((vec(maxs) - vec(mins)) * .5f).maximum(PxVec3(.01f));
  PxOverlapHit hits[32];
  PxOverlapBuffer overlaps(hits, 32);
  scene->overlap(
      PxBoxGeometry(extent),
      PxTransform(vec(origin) + offset + movement * dt + direction * .02f),
      overlaps, PxQueryFilterData(PxQueryFlag::eDYNAMIC));
  std::vector<PxRigidDynamic *> pushed;
  for (PxU32 i = 0; i < overlaps.getNbTouches(); ++i) {
    auto *body = overlaps.getTouch(i).actor->is<PxRigidDynamic>();
    if (!body || (body->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) ||
        body->getMass() > 250 ||
        std::find(pushed.begin(), pushed.end(), body) != pushed.end())
      continue;
    pushed.push_back(body);
    float change = speed * .55f - body->getLinearVelocity().dot(direction);
    if (change > 0)
      body->addForce(direction *
                         std::min(body->getMass() * change, 80.f * dt * 20.f),
                     PxForceMode::eIMPULSE);
  }
}
int AS_PhysicsSweep(const float start[3], const float end[3],
                    const float mins[3], const float maxs[3], float *fraction,
                    float normal[3]) {
  if (!scene)
    return 0;
  PxVec3 offset = (vec(mins) + vec(maxs)) * 0.5f,
         ext = (vec(maxs) - vec(mins)) * 0.5f;
  ext = ext.maximum(PxVec3(0.01f));
  PxVec3 delta = vec(end) - vec(start);
  float len = delta.magnitude();
  if (len < 0.00001f)
    return 0;
  // A falling fragment can overlap the player between Quake movement steps.
  // Use the penetration normal instead of PhysX's default -sweepDirection so
  // escape and tangential motion remain possible while deeper motion blocks.
  struct EscapeFilter final : PxQueryFilterCallback {
    PxVec3 direction;
    explicit EscapeFilter(PxVec3 d) : direction(d) {}
    PxQueryHitType::Enum preFilter(const PxFilterData &, const PxShape *,
                                   const PxRigidActor *,
                                   PxHitFlags &) override {
      return PxQueryHitType::eBLOCK;
    }
    PxQueryHitType::Enum postFilter(const PxFilterData &,
                                    const PxQueryHit &query, const PxShape *,
                                    const PxRigidActor *actor) override {
      auto *body = actor->is<PxRigidDynamic>();
      if (body && (body->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC))
        return PxQueryHitType::eNONE;
      const auto &hit = static_cast<const PxSweepHit &>(query);
      if (hit.distance <= 0 && direction.dot(hit.normal) >= -0.0001f)
        return PxQueryHitType::eNONE;
      return PxQueryHitType::eBLOCK;
    }
  } escape(delta / len);
  PxSweepBuffer hit;
  PxQueryFilterData filter(PxQueryFlag::eDYNAMIC | PxQueryFlag::ePOSTFILTER);
  if (!scene->sweep(PxBoxGeometry(ext), PxTransform(vec(start) + offset),
                    delta / len, len, hit,
                    PxHitFlag::eDEFAULT | PxHitFlag::eMTD, filter, &escape))
    return 0;
  *fraction = std::max(0.0f, (hit.block.distance - 0.001f) / len);
  normal[0] = hit.block.normal.x;
  normal[1] = hit.block.normal.y;
  normal[2] = hit.block.normal.z;
  return 1;
}
namespace {
struct SaveHeader {
  uint32_t magic, version;
  uint64_t mapHash;
  uint32_t chunkCount, eventCount, bodyCount, reserved;
};
struct SavedBody {
  uint32_t chunk, sleeping;
  float position[3], rotation[4], linear[3], angular[3];
};
static_assert(sizeof(SaveHeader) == 32 && sizeof(SavedBody) == 60,
              "Save format ABI");
uint64_t checksum(uint64_t h, const void *data, size_t size) {
  const auto *p = static_cast<const unsigned char *>(data);
  for (size_t i = 0; i < size; ++i) {
    h ^= p[i];
    h *= 1099511628211ULL;
  }
  return h;
}
} // namespace
int AS_PhysicsSave(const char *path, uint64_t mapHash) {
  if (!scene)
    return 0;
  std::vector<SavedBody> states;
  for (auto *body : bodies) {
    uint32_t id = 0;
    while (id < chunkBody.size() && chunkBody[id] != body)
      ++id;
    auto p = body->getGlobalPose();
    auto v = body->getLinearVelocity();
    auto w = body->getAngularVelocity();
    states.push_back({id,
                      body->isSleeping() ? 1U : 0U,
                      {p.p.x, p.p.y, p.p.z},
                      {p.q.x, p.q.y, p.q.z, p.q.w},
                      {v.x, v.y, v.z},
                      {w.x, w.y, w.z}});
  }
  std::string temporary = std::string(path) + ".tmp";
  FILE *f = _wfopen(wide(temporary.c_str()).c_str(), L"wb");
  if (!f)
    return 0;
  const float *health = actors.empty()
                            ? nullptr
                            : NvBlastActorGetBondHealths(actors[0], blastLog);
  SaveHeader h{0x41535150,
               2,
               mapHash,
               uint32_t(chunks.size()),
               uint32_t(history.size()),
               uint32_t(states.size()),
               uint32_t(bonds.size())};
  uint64_t sum = checksum(1469598103934665603ULL, history.data(),
                          history.size() * sizeof(Explosion));
  sum = checksum(sum, health, bonds.size() * 4);
  sum = checksum(sum, states.data(), states.size() * sizeof(SavedBody));
  bool ok = std::fwrite(&h, sizeof(h), 1, f) == 1 &&
            std::fwrite(history.data(), sizeof(Explosion), history.size(), f) ==
                history.size() &&
            std::fwrite(health, 4, bonds.size(), f) == bonds.size() &&
            std::fwrite(states.data(), sizeof(SavedBody), states.size(), f) ==
                states.size() &&
            std::fwrite(&sum, 8, 1, f) == 1;
  ok = std::fclose(f) == 0 && ok;
  if (!ok) {
    _wremove(wide(temporary.c_str()).c_str());
    return 0;
  }
  return AS_AtomicReplace(temporary.c_str(), path);
}
int AS_AtomicReplace(const char *temporary, const char *target) {
  return MoveFileExW(wide(temporary).c_str(), wide(target).c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
int AS_RemoveFile(const char *path) {
  return _wremove(wide(path).c_str()) == 0 || errno == ENOENT;
}
FILE *AS_OpenFile(const char *path, const char *mode) {
  return _wfopen(wide(path).c_str(), wide(mode).c_str());
}
int AS_PhysicsLoad(const char *path, uint64_t mapHash) {
  if (!scene)
    return 0;
  FILE *f = _wfopen(wide(path).c_str(), L"rb");
  if (!f)
    return 0;
  SaveHeader h{};
  bool valid = std::fread(&h, sizeof(h), 1, f) == 1 && h.magic == 0x41535150 &&
               h.version == 2 && h.mapHash == mapHash &&
               h.chunkCount == chunks.size() && h.eventCount <= 65536 &&
               h.bodyCount <= chunks.size() && h.reserved == bonds.size();
  if (!valid) {
    std::fclose(f);
    return 0;
  }
  std::vector<Explosion> events(h.eventCount);
  std::vector<SavedBody> states(h.bodyCount);
  std::vector<float> health(h.reserved);
  uint64_t expected = 0;
  valid = std::fread(events.data(), sizeof(Explosion), events.size(), f) ==
              events.size() &&
          std::fread(health.data(), 4, health.size(), f) == health.size() &&
          std::fread(states.data(), sizeof(SavedBody), states.size(), f) ==
              states.size() &&
          std::fread(&expected, 8, 1, f) == 1 && std::fgetc(f) == EOF;
  std::fclose(f);
  uint64_t sum = checksum(1469598103934665603ULL, events.data(),
                          events.size() * sizeof(Explosion));
  sum = checksum(sum, health.data(), health.size() * 4);
  sum = checksum(sum, states.data(), states.size() * sizeof(SavedBody));
  valid = valid && sum == expected;
  for (float value : health)
    valid = valid && std::isfinite(value) && value <= 1 && value >= -100000;
  for (const auto &e : events) {
    for (auto value : e.origin)
      valid = valid && std::isfinite(value);
    valid = valid && std::isfinite(e.radius) && e.radius > 0 &&
            e.radius < 100000 && std::isfinite(e.damage) && e.damage > 0 &&
            e.damage < 100000;
  }
  for (const auto &s : states) {
    valid = valid && s.chunk < chunks.size();
    for (auto v : s.position)
      valid = valid && std::isfinite(v);
    for (auto v : s.rotation)
      valid = valid && std::isfinite(v);
    for (auto v : s.linear)
      valid = valid && std::isfinite(v);
    for (auto v : s.angular)
      valid = valid && std::isfinite(v);
    PxQuat q(s.rotation[0], s.rotation[1], s.rotation[2], s.rotation[3]);
    valid = valid && q.isSane();
  }
  if (!valid)
    return 0;
  auto initial = chunks;
  auto mesh = originalTriangles;
  if (!AS_PhysicsBuild(initial.data(), uint32_t(initial.size()), mesh.data(),
                       uint32_t(mesh.size() / 9)))
    return 0;
  if (asset) {
    const auto graph = NvBlastAssetGetSupportGraph(asset, blastLog);
    std::vector<NvBlastBondFractureData> commands;
    for (uint32_t i = 0; i < graph.nodeCount; ++i)
      for (uint32_t k = graph.adjacencyPartition[i];
           k < graph.adjacencyPartition[i + 1]; ++k) {
        uint32_t j = graph.adjacentNodeIndices[k];
        if (j < i)
          continue;
        float damage = 1 - health[graph.adjacentBondIndices[k]];
        if (damage > 0)
          commands.push_back({0, i, j, damage});
      }
    if (!commands.empty()) {
      NvBlastFractureBuffers buffer{uint32_t(commands.size()), 0,
                                    commands.data(), nullptr};
      NvBlastActorApplyFracture(nullptr, actors[0], &buffer, blastLog, nullptr);
      splitAll(PxVec3(0), 0, 0);
    }
  }
  history = events;
  stats.events = uint32_t(history.size());
  if (states.size() != bodies.size())
    return 0;
  std::vector<PxRigidDynamic *> restored;
  for (const auto &s : states) {
    auto *body = chunkBody[s.chunk];
    if (!body)
      return 0;
    if (std::find(restored.begin(), restored.end(), body) != restored.end())
      return 0;
    restored.push_back(body);
    body->setGlobalPose(PxTransform(
        PxVec3(s.position[0], s.position[1], s.position[2]),
        PxQuat(s.rotation[0], s.rotation[1], s.rotation[2], s.rotation[3])));
    body->setLinearVelocity(PxVec3(s.linear[0], s.linear[1], s.linear[2]));
    body->setAngularVelocity(PxVec3(s.angular[0], s.angular[1], s.angular[2]));
    if (s.sleeping)
      body->putToSleep();
  }
  updatePoses();
  return 1;
}
