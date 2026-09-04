#include "baldr/directededge.h"
#include "baldr/graphreader.h"
#include "gurka.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace valhalla;

namespace {

const std::string kAsciiMap = R"(
  A---B---C
      |
      D
)";

gurka::ways make_ways() {
  return {
      {"AB", {{"highway", "track"}, {"tracktype", "grade1"}, {"surface", "ground"}}},
      {"BC", {{"highway", "track"}, {"tracktype", "grade4"}, {"surface", "ground"}}},
      {"BD", {{"highway", "residential"}, {"surface", "gravel"}}},
  };
}

// every way class gets a flat 99, distinct from anything lua's track ladder assigns
std::string write_bumped_speeds_config() {
  auto path = std::filesystem::temp_directory_path() / "tracks_keep_lua_speed_gurka_test.json";
  std::ofstream f(path);
  f << R"([{
    "rural": {"way":[99,99,99,99,99,99,99,99],"link_exiting":[99,99,99,99,99],
              "link_turning":[99,99,99,99,99],"roundabout":[99,99,99,99,99,99,99,99],
              "driveway":99,"alley":99,"parking_aisle":99,"drive-through":99},
    "suburban": {"way":[99,99,99,99,99,99,99,99],"link_exiting":[99,99,99,99,99],
              "link_turning":[99,99,99,99,99],"roundabout":[99,99,99,99,99,99,99,99],
              "driveway":99,"alley":99,"parking_aisle":99,"drive-through":99},
    "urban": {"way":[99,99,99,99,99,99,99,99],"link_exiting":[99,99,99,99,99],
              "link_turning":[99,99,99,99,99],"roundabout":[99,99,99,99,99,99,99,99],
              "driveway":99,"alley":99,"parking_aisle":99,"drive-through":99}
  }])";
  return path.string();
}

uint32_t speed_of(gurka::map& map, const std::string& way, const std::string& end_node) {
  baldr::GraphReader reader(map.config.get_child("mjolnir"));
  auto edge = std::get<1>(gurka::findEdge(reader, map.nodes, way, end_node));
  EXPECT_NE(edge, nullptr) << way << " should exist in the built graph";
  return edge ? edge->speed() : 0;
}

} // namespace

TEST(TracksKeepLuaSpeed, FlagOffTracksLoseTheirGradeDifferenceToConfig) {
  auto config_path = write_bumped_speeds_config();
  auto map = gurka::buildtiles(gurka::detail::map_to_coordinates(kAsciiMap, 100), make_ways(), {}, {},
                               "test/data/tracks_keep_lua_speed_off",
                               {{"mjolnir.tracks_keep_lua_speed", "false"},
                                {"mjolnir.default_speeds_config", config_path}});

  EXPECT_EQ(speed_of(map, "AB", "B"), 99u)
      << "without the flag, default_speeds_config should still win for tracks";
  EXPECT_EQ(speed_of(map, "BC", "C"), 99u)
      << "without the flag, default_speeds_config should still win for tracks";

  std::filesystem::remove(config_path);
}

TEST(TracksKeepLuaSpeed, FlagOnPreservesPerGradeSpeedsAndIgnoresConfig) {
  auto map = gurka::buildtiles(gurka::detail::map_to_coordinates(kAsciiMap, 100), make_ways(), {}, {},
                               "test/data/tracks_keep_lua_speed_on",
                               {{"mjolnir.tracks_keep_lua_speed", "true"}});

  auto grade1_speed = speed_of(map, "AB", "B");
  auto grade4_speed = speed_of(map, "BC", "C");

  EXPECT_GT(grade1_speed, grade4_speed) << "grade1 should keep a faster lua speed than grade4";

  auto config_path = write_bumped_speeds_config();
  auto map_configured =
      gurka::buildtiles(gurka::detail::map_to_coordinates(kAsciiMap, 100), make_ways(), {}, {},
                        "test/data/tracks_keep_lua_speed_on_config",
                        {{"mjolnir.tracks_keep_lua_speed", "true"},
                         {"mjolnir.default_speeds_config", config_path}});

  EXPECT_EQ(speed_of(map_configured, "AB", "B"), grade1_speed)
      << "flag on: default_speeds_config must not override a track's lua speed";
  EXPECT_EQ(speed_of(map_configured, "BC", "C"), grade4_speed)
      << "flag on: default_speeds_config must not override a track's lua speed";
  EXPECT_EQ(speed_of(map_configured, "BD", "D"), 99u)
      << "flag on: a non-track edge must still take its speed from the config";

  std::filesystem::remove(config_path);
}
