# SSEQPlayer provenance

Source: https://github.com/kode54/SSEQPlayer

Imported revision: `77222d3657adff358fb4e610d3e56bb7ada8ec24`.

License: WTFPL v2, reproduced in `../../licenses/SSEQPlayer.txt`. Original file headers are retained.

The port uses the portable sequence, bank, wave, track and channel components. SDAT resource loading and Wii output are supplied by `source/audio.cpp` and `source/platform_wii.c`.

Local changes share the DS's 16 channels among players, enforce channel ownership, expose sequence ticks, implement per-handle volume/pan and pause suspension, and keep UI sounds audible while gameplay is paused. Parsing uses bounded little-endian reads; ADPCM signed-predictor handling and undefined signed shifts were corrected. Channel references are validated before indexing. Output uses nearest sample selection at 32 kHz and a shared DS driver clock.
