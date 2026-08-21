#pragma once

#include "engine/types.hpp"

// Structural equality check for Paint. Paint has no operator== and lives in the
// engine submodule, which must not be modified, so this lives in editor code.
inline bool paintEquals(const Paint& a, const Paint& b)
{
    if (a.type != b.type) return false;

    for (int i = 0; i < 6; ++i) {
        if (a.params[i] != b.params[i]) return false;
    }

    if (a.imagePath != b.imagePath) return false;
    if (a.imageScaleMode != b.imageScaleMode) return false;
    if (a.tileScale != b.tileScale) return false;

    if (a.stops.size() != b.stops.size()) return false;
    for (size_t i = 0; i < a.stops.size(); ++i) {
        const Paint::Stop& sa = a.stops[i];
        const Paint::Stop& sb = b.stops[i];
        if (sa.offset != sb.offset) return false;
        if (sa.r != sb.r) return false;
        if (sa.g != sb.g) return false;
        if (sa.b != sb.b) return false;
        if (sa.a != sb.a) return false;
    }

    return true;
}
