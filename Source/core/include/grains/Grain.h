#pragma once

namespace grains {

// Plain per-grain state. Rendering lives in the engine (work unit 3).
struct Grain {
    double pos = 0.0;     // fractional read position in source frames
    double rate = 1.0;    // source frames advanced per output sample
    int length = 0;       // grain length in output samples
    int age = 0;          // output samples rendered so far
    int startOffset = 0;  // sample offset inside the current block where rendering starts
    float gainL = 0.0f;
    float gainR = 0.0f;
    bool active = false;
};

} // namespace grains
