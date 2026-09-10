/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aftershock_layout.h"
#include "aftershock_scatter.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
static void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
struct Module {
  std::map<uint32_t, std::vector<uint32_t>> types, variables;
  std::map<std::pair<uint32_t, uint32_t>, uint32_t> decorations, offsets;
  explicit Module(const std::string &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    require(bool(file), "Missing compiled SPIR-V; run scripts/build.ps1");
    auto length = file.tellg();
    require(length >= 20 && length % 4 == 0, "Invalid SPIR-V length");
    std::vector<uint32_t> words(size_t(length) / 4);
    file.seekg(0);
    file.read((char *)words.data(), length);
    require(words[0] == 0x07230203, "Invalid SPIR-V magic");
    for (size_t i = 5; i < words.size();) {
      uint32_t n = words[i] >> 16, op = words[i] & 65535;
      require(n && i + n <= words.size(), "Invalid SPIR-V instruction");
      const auto *p = words.data() + i;
      if (op >= 19 && op <= 33 && n >= 2)
        types[p[1]] = {p, p + n};
      if (op == 59)
        variables[p[2]] = {p, p + n};
      if (op == 71 && n >= 4)
        decorations[{p[1], p[2]}] = p[3];
      if (op == 72 && n >= 5 && p[3] == 35)
        offsets[{p[1], p[2]}] = p[4];
      i += n;
    }
  }
  void inputs(const std::vector<int> &expected) {
    size_t count = 0;
    for (const auto &entry : variables) {
      const auto &v = entry.second;
      if (v[3] != 1)
        continue;
      auto location = decorations.find({entry.first, 30});
      if (location == decorations.end())
        continue;
      uint32_t l = location->second;
      require(l < expected.size(), "Unexpected vertex attribute");
      auto pointer = types.at(v[1]);
      auto type = types.at(pointer[3]);
      if (expected[l] == 1)
        require((type[0] & 65535) == 21 && type[2] == 32 && type[3] == 0,
                "Packed sample must be uint32");
      else {
        require((type[0] & 65535) == 23 && type[3] == 4,
                "Metadata attribute must be vec4");
        auto component = types.at(type[2]);
        require((component[0] & 65535) == 22 && component[2] == 32,
                "Metadata must use float32");
      }
      ++count;
    }
    require(count == expected.size(), "Missing vertex attributes");
  }
  void bindings(std::set<std::pair<uint32_t, uint32_t>> expected) {
    std::set<std::pair<uint32_t, uint32_t>> found;
    for (const auto &entry : variables) {
      auto set = decorations.find({entry.first, 34}),
           binding = decorations.find({entry.first, 33});
      if (set != decorations.end() && binding != decorations.end())
        found.insert({set->second, binding->second});
    }
    require(found == expected, "SPIR-V descriptor binding mismatch");
  }
  void aliasOffsets() {
    for (const auto &entry : variables) {
      if (decorations.find({entry.first, 34}) == decorations.end() ||
          decorations.at({entry.first, 34}) != 2)
        continue;
      uint32_t structure = types.at(entry.second[1])[3];
      uint32_t expected[] = {0, 64, 76, 80, 92, 96, 100, 104, 108, 112};
      for (uint32_t i = 0; i < 10; ++i)
        require(offsets.at({structure, i}) == expected[i],
                "Alias UBO reflected offset mismatch");
      return;
    }
    throw std::runtime_error("Missing alias UBO");
  }
};
int main(int argc, char **argv) {
  try {
    float worst = 0;
    for (uint32_t i = 0; i <= 65535; ++i) {
      as_sample_t packed = 0;
      float u = i * .125f, v = (65535 - i) * .125f;
      require(AS_PackSample(u, v, &packed),
              "Encoding rejected representable coordinates");
      require((packed & 65535) == i && (packed >> 16) == 65535 - i,
              "Packing round trip");
      for (float delta : {-.0625f, .03125f, .0625f}) {
        float value = u + delta;
        if (value < 0 || value > 8191.875f)
          continue;
        require(AS_PackSample(value, v, &packed),
                "Encoding rejected quantized coordinates");
        worst =
            std::max(worst, std::abs(float(packed & 65535) * .125f - value));
      }
    }
    require(worst <= .0625f, "Quantization exceeds half a texel step");
    as_sample_t untouched = 123;
    require(!AS_PackSample(-1, 0, &untouched) &&
                !AS_PackSample(8192, 0, &untouched) &&
                !AS_PackSample(std::numeric_limits<float>::infinity(), 0,
                               &untouched) &&
                !AS_PackSample(0, std::numeric_limits<float>::quiet_NaN(),
                               &untouched) &&
                untouched == 123,
            "Out-of-range encoding must reject without writes");
    require(argc == 2, "Pass compiled shader directory");
    std::string root = argv[1];
    Module surface(root + "/aftershock.vert.spv");
    surface.inputs({1, 4, 4, 4, 4, 4, 4});
    surface.bindings({});
    Module structure(root + "/aftershock_structure.vert.spv");
    structure.inputs({1, 4, 4, 4, 4, 4, 4, 4, 4, 4});
    structure.bindings({});
    const float poly[3][2] = {{0, 0}, {192, 0}, {32, 160}}, low[2] = {0, 0},
                high[2] = {192, 160};
    std::set<as_sample_t> points;
    int accepted = 0, offGrid = 0;
    for (int layer = 0; layer < 4; ++layer)
      for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
          as_sample_t a = 123, b = 123;
          int aa =
              AS_ScatterCell(poly, 3, low, high, x, y, 16, 16, 71, layer, &a);
          int bb =
              AS_ScatterCell(poly, 3, low, high, x, y, 16, 16, 71, layer, &b);
          require(aa == bb && a == b,
                  "Scatter must be repeatable without game RNG");
          if (!aa) {
            require(a == 123, "Rejected scatter must leave output untouched");
            continue;
          }
          require(
              AS_ScatterInside(poly, 3, (a & 65535) * .125f, (a >> 16) * .125f),
              "Scatter outside polygon");
          points.insert(a);
          ++accepted;
          if ((a & 65535) % 96 != 0 && (a >> 16) % 80 != 0)
            ++offGrid;
        }
    require(accepted > 450 && accepted < 600 &&
                points.size() > accepted * .97 && offGrid > accepted * .9,
            "Independent layers must be irregular and spatially distributed");
    // Validate the toroidal pattern, including the wrap seam. This catches
    // duplicate points and visible clusters introduced by table edits.
    for(int a=0;a<256;++a)for(int b=a+1;b<256;++b){
      float dx=std::abs(float(a%16-b%16)+as_blue_noise[a][0]-as_blue_noise[b][0]);
      float dy=std::abs(float(a/16-b/16)+as_blue_noise[a][1]-as_blue_noise[b][1]);
      dx=std::min(dx,16-dx);dy=std::min(dy,16-dy);
      require(dx*dx+dy*dy>.81f,"Blue-noise separation below .9 cells");
    }
    Module fragment(root + "/aftershock.frag.spv");
    fragment.bindings({{0, 0}, {1, 0}, {2, 0}});
    Module effects(root + "/aftershock_fx.vert.spv");
    effects.inputs({4, 4, 4, 4, 4});
    effects.bindings({});
    Module alias(root + "/aftershock_alias.vert.spv");
    alias.inputs({});
    alias.bindings({{2, 0}, {3, 0}});
    alias.aliasOffsets();
    Module neonAlias(root + "/aftershock_alias.frag.spv");
    neonAlias.bindings({{0, 0}, {1, 0}, {2, 0}});
    for (const char *variant : {"single", "msaa"}) {
      Module fidelity(root + "/fidelity_" + variant + ".spv");
      fidelity.bindings({{0,0},{0,1},{0,2},{0,3},{0,4},{0,5},{1,0}});
      for (const auto &v : fidelity.variables) {
        if (fidelity.decorations.find({v.first,34})==fidelity.decorations.end() || fidelity.decorations.at({v.first,34})!=1) continue;
        uint32_t structure=fidelity.types.at(v.second[1])[3];
        uint32_t expected[]={0,64,128,192,208,224,240,256,272,288,304};
        for(uint32_t i=0;i<11;++i) require(fidelity.offsets.at({structure,i})==expected[i],"Fidelity UBO offset mismatch");
      }
    }
    for (const char *bits : {"8bit", "10bit"})
      for (const char *scale : {"", "_scale", "_scale_sops"}) {
        Module post(root + "/screen_effects_" + bits + scale + ".comp.spv");
        post.bindings({{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}});
      }
    std::printf("PASS: 65,536 exact coordinate pairs; worst quantization error "
                "%.4f source texels/axis; invalid range rejection; reflected "
                "vertex types, descriptors and alias UBO offsets\n",
                worst);
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: %s\n", e.what());
    return 1;
  }
}
