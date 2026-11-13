#pragma once
#include <stdint.h>

namespace seeed {
struct RadioPreset {
  const char* title;
  float frequency, bandwidth;
  uint8_t sf, cr, pathBytes;
};
// Snapshot: https://api.meshcore.nz/api/v1/config, 2026-11-10.
// Community interoperability presets, not a regional regulatory database.
// Excludes 433MHz and 927.875MHz: user-confirmed L2 RF range is 863-925MHz.
constexpr RadioPreset presets[] = {
  {"Australia",915.800f,250,10,5,1},
  {"Australia (Narrow)",916.575f,62.5f,7,7,1},
  {"Australia (Mid)",915.075f,125,9,5,1},
  {"Australia: SA, WA",923.125f,62.5f,8,8,1},
  {"Australia: QLD",923.125f,62.5f,8,5,1},
  {"Brazil",923.125f,62.5f,8,8,1},
  {"Canada",910.525f,62.5f,7,5,3},
  {"Costa Rica",910.525f,125,11,5,1},
  {"EU/UK (Narrow)",869.618f,62.5f,8,8,1},
  {"EU/UK (Deprecated)",869.525f,250,11,5,1},
  {"Czech Republic (Narrow)",869.432f,62.5f,7,5,2},
  {"Hungary",869.618f,62.5f,7,5,2},
  {"Netherlands",869.618f,62.5f,7,5,1},
  {"Netherlands (Limburg)",869.618f,62.5f,8,8,2},
  {"New Zealand (Narrow)",917.375f,62.5f,7,5,2},
  {"New Zealand (Gisborne)",917.375f,250,11,5,1},
  {"Portugal 868",869.618f,62.5f,7,6,1},
  {"Slovakia",869.618f,62.5f,7,5,2},
  {"Switzerland",869.618f,62.5f,8,8,1},
  {"Thailand",921.000f,62.5f,7,5,1},
  {"USA",910.525f,62.5f,7,5,1},
  {"Vietnam (Narrow)",920.250f,62.5f,8,5,1},
  {"Vietnam (Deprecated)",920.250f,250,11,5,1},
};
constexpr unsigned presetCount = sizeof(presets)/sizeof(presets[0]);
constexpr bool inHardwareBand(float freq) { return freq >= 863.0f && freq <= 925.0f; }
}
