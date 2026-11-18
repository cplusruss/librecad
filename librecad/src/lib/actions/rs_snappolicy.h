#ifndef RS_SNAPPOLICY_H
#define RS_SNAPPOLICY_H

#include "lc_graphicviewport.h"

namespace RS_SnapPolicy {

inline double pxToWorld(const LC_GraphicViewport& viewport, int px) {
    return viewport.toUcsDX(px);
}

inline bool onEntityMayCompete(double bestCandidateDistWorld, double switchDistWorld) {
    return bestCandidateDistWorld > switchDistWorld;
}

inline int gripPickRadiusPx(int handleHalfSize, int overrideHalfSize) {
    return (overrideHalfSize > 0 ? overrideHalfSize : handleHalfSize) + 6;
}

}

#endif
