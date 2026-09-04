#include "mjolnir/speed_assigner.h"
#include "baldr/directededge.h"
#include "baldr/graphconstants.h"

#include <boost/optional.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace valhalla::baldr;
using namespace valhalla::mjolnir;

namespace {

// kMaxRuralDensity(8) is the rural/urban cutoff
constexpr uint32_t kDensityRural = 3;
constexpr uint32_t kDensityUrban = 12;
constexpr uint32_t kLuaSpeed = 32;
constexpr uint32_t kUrbanUnclassifiedSpeed = 35; // urban_rc_speed[kUnclassified]

DirectedEdge make_edge(Use use, RoadClass rc, Surface surface, uint32_t speed) {
  DirectedEdge de{};
  de.set_use(use);
  de.set_classification(rc);
  de.set_surface(surface);
  de.set_forwardaccess(kVehicularAccess);
  de.set_reverseaccess(kVehicularAccess);
  de.set_speed(speed);
  de.set_speed_type(SpeedType::kClassified); // kTagged is 0 and takes a different branch
  return de;
}

DirectedEdge track_edge(uint32_t speed = kLuaSpeed) {
  return make_edge(Use::kTrack, RoadClass::kUnclassified, Surface::kGravel, speed);
}

DirectedEdge road_edge(uint32_t speed = kLuaSpeed) {
  return make_edge(Use::kRoad, RoadClass::kResidential, Surface::kGravel, speed);
}

// every way class gets speed 77, distinct from kLuaSpeed
std::string write_speeds_config() {
  auto path = std::filesystem::temp_directory_path() / "speed_assigner_test_config.json";
  std::ofstream f(path);
  f << R"([{
    "rural": {"way":[10,10,10,10,10,77,10,10],"link_exiting":[10,10,10,10,10],
              "link_turning":[10,10,10,10,10],"roundabout":[10,10,10,10,10,10,10,10],
              "driveway":10,"alley":10,"parking_aisle":10,"drive-through":10},
    "suburban": {"way":[10,10,10,10,10,77,10,10],"link_exiting":[10,10,10,10,10],
              "link_turning":[10,10,10,10,10],"roundabout":[10,10,10,10,10,10,10,10],
              "driveway":10,"alley":10,"parking_aisle":10,"drive-through":10},
    "urban": {"way":[10,10,10,10,10,77,10,10],"link_exiting":[10,10,10,10,10],
              "link_turning":[10,10,10,10,10],"roundabout":[10,10,10,10,10,10,10,10],
              "driveway":10,"alley":10,"parking_aisle":10,"drive-through":10}
  }])";
  return path.string();
}

} // namespace

TEST(SpeedAssigner, DefaultsToFalseWhenOmitted) {
  SpeedAssigner assigner(boost::none);
  auto edge = track_edge();
  assigner.UpdateSpeed(edge, kDensityRural, false, "", "");
  EXPECT_EQ(edge.speed(), kLuaSpeed / 2) << "surface halving should still apply by default";
}

TEST(SpeedAssigner, FlagOffTrackGetsSurfaceHalved) {
  SpeedAssigner assigner(boost::none, /*tracks_keep_lua_speed=*/false);
  auto edge = track_edge();
  bool used_shortcut = assigner.UpdateSpeed(edge, kDensityRural, false, "", "");
  EXPECT_FALSE(used_shortcut);
  EXPECT_EQ(edge.speed(), kLuaSpeed / 2);
}

TEST(SpeedAssigner, FlagOnTrackKeepsLuaSpeedOnRoughSurface) {
  SpeedAssigner assigner(boost::none, /*tracks_keep_lua_speed=*/true);
  auto edge = track_edge();
  bool short_circuited = assigner.UpdateSpeed(edge, kDensityRural, false, "", "");
  EXPECT_TRUE(short_circuited) << "UpdateSpeed should report it already handled this edge";
  EXPECT_EQ(edge.speed(), kLuaSpeed) << "the surface-halving rule must not run for tracks";
}

TEST(SpeedAssigner, FlagOnDoesNotAffectNonTrackEdges) {
  SpeedAssigner assigner(boost::none, /*tracks_keep_lua_speed=*/true);
  auto edge = road_edge();
  assigner.UpdateSpeed(edge, kDensityRural, false, "", "");
  EXPECT_EQ(edge.speed(), kLuaSpeed / 2)
      << "the early return must be scoped to Use::kTrack, not every edge";
}

TEST(SpeedAssigner, FlagOnTrackIgnoresDefaultSpeedsConfig) {
  auto config_path = write_speeds_config();
  SpeedAssigner assigner(config_path, /*tracks_keep_lua_speed=*/true);
  auto edge = track_edge();
  assigner.UpdateSpeed(edge, kDensityRural, false, "", "");
  EXPECT_EQ(edge.speed(), kLuaSpeed)
      << "default_speeds_config must not override a track's lua speed when the flag is set";
  std::filesystem::remove(config_path);
}

TEST(SpeedAssigner, FlagOffTrackIsOverriddenByDefaultSpeedsConfig) {
  auto config_path = write_speeds_config();
  SpeedAssigner assigner(config_path, /*tracks_keep_lua_speed=*/false);
  auto edge = track_edge();
  assigner.UpdateSpeed(edge, kDensityRural, false, "", "");
  EXPECT_EQ(edge.speed(), 77u);
  std::filesystem::remove(config_path);
}

TEST(SpeedAssigner, FlagOnTrackIgnoresUrbanDensityOverride) {
  SpeedAssigner assigner(boost::none, /*tracks_keep_lua_speed=*/true);
  auto edge = track_edge();
  assigner.UpdateSpeed(edge, kDensityUrban, false, "", "");
  EXPECT_EQ(edge.speed(), kLuaSpeed)
      << "the urban/rural density override must not apply to a protected track";
}

TEST(SpeedAssigner, FlagOffTrackGetsUrbanDensityOverride) {
  SpeedAssigner assigner(boost::none, /*tracks_keep_lua_speed=*/false);
  auto edge = track_edge();
  assigner.UpdateSpeed(edge, kDensityUrban, false, "", "");
  EXPECT_EQ(edge.speed(), kUrbanUnclassifiedSpeed / 2);
}
