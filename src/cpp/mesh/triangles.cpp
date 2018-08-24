/*
 * TÖVE - Animated vector graphics for LÖVE.
 * https://github.com/poke1024/tove2d
 *
 * Copyright (c) 2018, Bernhard Liebl
 *
 * Distributed under the MIT license. See LICENSE file for details.
 *
 * All rights reserved.
 */

#include "triangles.h"

BEGIN_TOVE_NAMESPACE

#if TOVE_TARGET == TOVE_TARGET_LOVE2D
inline ToveVertexIndex ToLoveVertexMapIndex(ToveVertexIndex i) {
	// convert to 1-based indices for LÖVE's Mesh:setVertexMap()
	return i + 1;
}
#else
inline ToveVertexIndex ToLoveVertexMapIndex(ToveVertexIndex i) {
	return i;
}
#endif

ToveVertexIndex *TriangleStore::allocate(int n, bool isFinalSize) {
    const int offset = size;

    const int k = (mode == TRIANGLES_LIST) ? 3 : 1;
    size += n * k;

    const int count = isFinalSize ? size : nextpow2(size);
    triangles = static_cast<ToveVertexIndex*>(realloc(
        triangles, count * sizeof(ToveVertexIndex)));

    if (!triangles) {
        TOVE_BAD_ALLOC();
        return nullptr;
    }

    return &triangles[offset];
}

void TriangleStore::_add(
    const std::list<TPPLPoly> &triangles,
    bool isFinalSize) {

    ToveVertexIndex *indices = allocate(triangles.size(), isFinalSize);
    for (auto i = triangles.begin(); i != triangles.end(); i++) {
        const TPPLPoly &poly = *i;
        for (int j = 0; j < 3; j++) {
            *indices++ = ToLoveVertexMapIndex(poly[j].id);
        }
    }
}

void TriangleStore::_add(
    const std::vector<ToveVertexIndex> &triangles,
    const ToveVertexIndex i0,
    bool isFinalSize) {

    const int n = triangles.size();
    assert(n % 3 == 0);
    ToveVertexIndex *indices = allocate(n / 3, isFinalSize);
    const ToveVertexIndex *input = triangles.data();
    for (int i = 0; i < n; i++) {
        *indices++ = i0 + ToLoveVertexMapIndex(*input++);
    }
}


TriangleCache::~TriangleCache() {
    for (int i = 0; i < triangulations.size(); i++) {
        delete triangulations[i];
    }
}

void TriangleCache::evict() {
    uint64_t minCount = std::numeric_limits<uint64_t>::max();
    int minIndex = -1;

    for (int i = 0; i < triangulations.size(); i++) {
        const Triangulation *t = triangulations[i];
        if (!t->keyframe) {
            const uint64_t count = t->useCount;
            if (count < minCount) {
                minCount = count;
                minIndex = i;
            }
        }
    }

    if (minIndex >= 0) {
        triangulations.erase(triangulations.begin() + minIndex);
        current = std::min(current, (int)(triangulations.size() - 1));
    }
}

bool TriangleCache::check(const Vertices &vertices, bool &trianglesChanged) {
    const int n = triangulations.size();
    if (n == 0) {
        return false;
    }

    assert(current < n);
    if (triangulations[current]->partition.check(vertices)) {
        triangulations[current]->useCount++;
        return true;
    }

    const int k = std::max(current, n - current);
    for (int i = 1; i <= k; i++) {
        if (current + i < n) {
            const int forward = (current + i) % n;
            if (triangulations[forward]->partition.check(vertices)) {
                std::swap(
                    triangulations[(current + 1) % n],
                    triangulations[forward]);
                current = (current + 1) % n;
                triangulations[current]->useCount++;
                trianglesChanged = true;
                return true;
            }
        }

        if (current - i >= 0) {
            const int backward = (current + n - i) % n;
            if (triangulations[backward]->partition.check(vertices)) {
                std::swap(
                    triangulations[(current + n - 1) % n],
                    triangulations[backward]);
                current = (current + n - 1) % n;
                triangulations[current]->useCount++;
                trianglesChanged = true;
                return true;
            }
        }
    }

    return false;
}

END_TOVE_NAMESPACE