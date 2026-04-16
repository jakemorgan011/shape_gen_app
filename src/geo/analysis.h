#pragma once
#include <math.h>
#include <thread>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>


// pass in m_lastMeshData
// it'll contain all the xyz in vector format..
// no extra libraries needed just strait c++
// probably can use in JUCE context too.


struct MeshContainer{
  int size;
  std::vector<float> verts;
  MeshContainer(std::vector<float> in_verts): verts(in_verts){ // copies the mesh for analysis.
    size = verts.size();
  }
  int get_size(){
    return size;
  }
};
struct OSCOut{
  std::string tag;
  std::string port;

  OSCOut(std::string t, std::string p): tag(t), port(p){

  }
};


// should be used in multithreading.
class AnalysisEngine {
public:

  AnalysisEngine() : stats_ready(false) {}
  ~AnalysisEngine(){}

  void update_stats(){
    std::vector<float> xs, ys, zs;
    collect_xyz(xs, ys, zs);
    if (xs.empty()) { stats_ready = false; return; }

    int n = (int)xs.size();

    float min_x = *std::min_element(xs.begin(), xs.end());
    float max_x = *std::max_element(xs.begin(), xs.end());
    float min_y = *std::min_element(ys.begin(), ys.end());
    float max_y = *std::max_element(ys.begin(), ys.end());
    float min_z = *std::min_element(zs.begin(), zs.end());
    float max_z = *std::max_element(zs.begin(), zs.end());

    float xSpread = max_x - min_x;
    float ySpread = max_y - min_y;
    float zSpread = max_z - min_z;
    float spread  = std::max(xSpread, std::max(ySpread, zSpread));
    float spread2 = spread * spread;
    float spread3 = spread * spread2;

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    for (int i = 0; i < n; i++) { cx += xs[i]; cy += ys[i]; cz += zs[i]; }
    cx /= n; cy /= n; cz /= n;

    float var_sum = 0.0f;
    for (int i = 0; i < n; i++) {
      float dx = xs[i]-cx, dy = ys[i]-cy, dz = zs[i]-cz;
      var_sum += dx*dx + dy*dy + dz*dz;
    }
    float variance = var_sum / n;
    float radius   = std::sqrt(variance);
    float volume   = std::abs(xSpread * ySpread * zSpread);

    // Normalize centroid to [0,1] within bounding box
    stats["mean_x"]   = xSpread > 0.0f ? (cx - min_x) / xSpread : 0.5f;
    stats["mean_y"]   = ySpread > 0.0f ? (cy - min_y) / ySpread : 0.5f;
    stats["mean_z"]   = zSpread > 0.0f ? (cz - min_z) / zSpread : 0.5f;
    // Normalize by spread-based reference (spread is raw reference scale)
    stats["variance"] = spread2 > 0.0f ? variance / spread2 : 0.0f;
    stats["spread"]   = spread;
    stats["volume"]   = spread3 > 0.0f ? volume   / spread3 : 0.0f;  // fill factor [0,1]
    stats["radius"]   = spread  > 0.0f ? radius   / spread  : 0.0f;
    stats["density"]  = volume  > 0.0f ? (float)n / volume  : 0.0f;  // raw verts/world³

    compute_profiles(ys, zs, min_y, ySpread, min_z, zSpread);
    stats_ready = true;
  }

  bool ready(){
    return stats_ready;
  }

  void clear_meshes(){
    meshes.clear();
    y_profile.clear();
    z_profile.clear();
    stats_ready = false;
  }

  void add_mesh(std::vector<float> verts){
    meshes.emplace_back(verts);
  }

  std::unordered_map<std::string, float> get_stats(){
    return stats;
  }

  const std::vector<float>& get_y_profile() const { return y_profile; }
  const std::vector<float>& get_z_profile() const { return z_profile; }

private:
  // size of stats should be 8 !
  std::unordered_map<std::string, float> stats;
  std::vector<MeshContainer> meshes;
  std::vector<float> y_profile;
  std::vector<float> z_profile;
  bool stats_ready;

  // Collect all x, y, z coords across every mesh into separate sorted arrays.
  void collect_xyz(std::vector<float>& xs, std::vector<float>& ys, std::vector<float>& zs) const {
    for (const auto& m : meshes) {
      for (int i = 0; i + 2 < (int)m.verts.size(); i += 3) {
        xs.push_back(m.verts[i]);
        ys.push_back(m.verts[i + 1]);
        zs.push_back(m.verts[i + 2]);
      }
    }
  }

  void compute_profiles(std::vector<float>& ys, std::vector<float>& zs,
                        float min_y, float y_spread, float min_z, float z_spread){
    std::sort(ys.begin(), ys.end());
    std::sort(zs.begin(), zs.end());

    // 21-point Y sample normalised to [0,1]
    y_profile.resize(21);
    for (int i = 0; i <= 20; i++) {
      int idx = (int)((float)i / 20.0f * (float)(ys.size() - 1));
      y_profile[i] = y_spread > 0.0f ? (ys[idx] - min_y) / y_spread : 0.5f;
    }

    // 11-point Z sample normalised to [0,1]
    z_profile.resize(11);
    for (int i = 0; i <= 10; i++) {
      int idx = (int)((float)i / 10.0f * (float)(zs.size() - 1));
      z_profile[i] = z_spread > 0.0f ? (zs[idx] - min_z) / z_spread : 0.5f;
    }
  }
};
