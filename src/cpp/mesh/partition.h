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

#ifndef __TOVE_MESH_PARTITION
#define __TOVE_MESH_PARTITION 1

#include <vector>
#include "../../thirdparty/polypartition.h"
#include "utils.h"

class Partition {
private:
	typedef std::vector<uint16_t> Indices;

	struct Part {
		Indices outline;
		int fail;
	};

	std::vector<Part> parts;

public:
	Partition() {
	}

	Partition(const std::list<TPPLPoly> &convex) {
		parts.reserve(convex.size());

		for (auto i = convex.begin(); i != convex.end(); i++) {
			const TPPLPoly &poly = *i;
			const int n = poly.GetNumPoints();

			Part part;
			part.outline.resize(n);
			for (int j = 0; j < n; j++) {
				part.outline[j] = poly[j].id;
			}
			part.fail = 0;

			parts.push_back(part);
		}
	}

	inline bool empty() const {
		return parts.empty();
	}

	bool check(const Vertices &vertices) {
		const int nparts = parts.size();

		if (nparts == 0) {
			return false;
		}

		for (int j = 0; j < nparts; j++) {
			Part &part = parts[j];

			const Indices &outline = part.outline;
			const int n = outline.size();

			int i = part.fail;
			int k = 0;

			do {
				assert(i >= 0 && i < n);
				const auto &p0 = vertices[outline[i]];

				int i1 = find_unequal_forward(vertices, outline.data(), i, n);
				const auto &p1 = vertices[outline[i1]];

				int i2 = find_unequal_forward(vertices, outline.data(), i1, n);
				const auto &p2 = vertices[outline[i2]];

				float area = cross(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y);

				const float eps = 0.1;
				if (area > eps) {
					part.fail = i;

					if (j != 0) {
						std::swap(parts[j].outline, parts[0].outline);
						std::swap(parts[j].fail, parts[0].fail);
					}
					return false;
				}

				if (i1 > i) {
					k += i1 - i;
				} else {
					k += (n - i) + i1;
				}
				i = i1;

			} while (k < n);
		}

		return true;
	}
};

#endif // __TOVE_MESH_PARTITION
