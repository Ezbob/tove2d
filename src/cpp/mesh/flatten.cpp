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

#include "flatten.h"
#include <cmath>
#include "mesh.h"
#include "../utils.h"
#include "../../thirdparty/nanosvg.h"
#include "turtle.h"
#include "../path.h"
#include "../subpath.h"

AdaptiveFlattener::AdaptiveFlattener(
	float scale, const ToveTesselationQuality *quality) : scale(scale) {

	if (quality) {
		assert(quality->adaptive.valid);
		const float eps = std::numeric_limits<float>::epsilon();
		distanceTolerance = quality->adaptive.distanceTolerance;
		colinearityEpsilon = quality->adaptive.colinearityEpsilon;
		angleEpsilon = quality->adaptive.angleEpsilon;
		angleTolerance = quality->adaptive.angleTolerance;
    	cuspLimit = quality->adaptive.cuspLimit;
		recursionLimit = std::min(
			MAX_FLATTEN_RECURSIONS, quality->recursionLimit);
	} else {
		distanceTolerance = 0.5;
		colinearityEpsilon = 0.5;
		angleEpsilon = 0.01;
		angleTolerance = 0.0;
		cuspLimit = 0.0;
		recursionLimit = 8;
	}

	distanceToleranceSquare = distanceTolerance * distanceTolerance;
}

void AdaptiveFlattener::flatten(
	float x1, float y1, float x2, float y2,
	float x3, float y3, float x4, float y4,
	ClipperPath &points) const {

	points.push_back(ClipperPoint(x1, y1));
	recursive(x1, y1, x2, y2, x3, y3, x4, y4, points, 0);
	points.push_back(ClipperPoint(x4, y4));
}

void AdaptiveFlattener::recursive(
	float x1, float y1, float x2, float y2,
	float x3, float y3, float x4, float y4,
	ClipperPath &points, int level) const {

	// the following function is taken from Maxim Shemanarev's AntiGrain:
	// https://github.com/pelson/antigrain/blob/master/agg-2.4/src/agg_curves.cpp

	if (level > recursionLimit) {
		points.push_back(ClipperPoint(x4, y4));
		return;
	}

	// Calculate all the mid-points of the line segments
	//----------------------
	double x12   = (x1 + x2) / 2;
	double y12   = (y1 + y2) / 2;
	double x23   = (x2 + x3) / 2;
	double y23   = (y2 + y3) / 2;
	double x34   = (x3 + x4) / 2;
	double y34   = (y3 + y4) / 2;
	double x123  = (x12 + x23) / 2;
	double y123  = (y12 + y23) / 2;
	double x234  = (x23 + x34) / 2;
	double y234  = (y23 + y34) / 2;
	double x1234 = (x123 + x234) / 2;
	double y1234 = (y123 + y234) / 2;


	// Try to approximate the full cubic curve by a single straight line
	//------------------
	double dx = x4-x1;
	double dy = y4-y1;

	double d2 = fabs(((x2 - x4) * dy - (y2 - y4) * dx));
	double d3 = fabs(((x3 - x4) * dy - (y3 - y4) * dx));
	double da1, da2, k;

	switch((int(d2 > colinearityEpsilon) << 1) +
			int(d3 > colinearityEpsilon))
	{
	case 0:
		// All collinear OR p1==p4
		//----------------------
		k = dx*dx + dy*dy;
		if(k == 0)
		{
			d2 = squareDistance(x1, y1, x2, y2);
			d3 = squareDistance(x4, y4, x3, y3);
		}
		else
		{
			k   = 1 / k;
			da1 = x2 - x1;
			da2 = y2 - y1;
			d2  = k * (da1*dx + da2*dy);
			da1 = x3 - x1;
			da2 = y3 - y1;
			d3  = k * (da1*dx + da2*dy);
			if(d2 > 0 && d2 < 1 && d3 > 0 && d3 < 1)
			{
				// Simple collinear case, 1---2---3---4
				// We can leave just two endpoints
				return;
			}
				 if(d2 <= 0) d2 = squareDistance(x2, y2, x1, y1);
			else if(d2 >= 1) d2 = squareDistance(x2, y2, x4, y4);
			else             d2 = squareDistance(x2, y2, x1 + d2*dx, y1 + d2*dy);

				 if(d3 <= 0) d3 = squareDistance(x3, y3, x1, y1);
			else if(d3 >= 1) d3 = squareDistance(x3, y3, x4, y4);
			else             d3 = squareDistance(x3, y3, x1 + d3*dx, y1 + d3*dy);
		}
		if(d2 > d3)
		{
			if(d2 < distanceToleranceSquare)
			{
				points.push_back(ClipperPoint(x2, y2));
				return;
			}
		}
		else
		{
			if(d3 < distanceToleranceSquare)
			{
				points.push_back(ClipperPoint(x3, y3));
				return;
			}
		}
		break;

	case 1:
		// p1,p2,p4 are collinear, p3 is significant
		//----------------------
		if(d3 * d3 <= distanceToleranceSquare * (dx*dx + dy*dy))
		{
			if(angleTolerance < angleEpsilon)
			{
				points.push_back(ClipperPoint(x23, y23));
				return;
			}

			// Angle Condition
			//----------------------
			da1 = fabs(atan2(y4 - y3, x4 - x3) - atan2(y3 - y2, x3 - x2));
			if(da1 >= M_PI) da1 = 2*M_PI - da1;

			if(da1 < angleTolerance)
			{
				points.push_back(ClipperPoint(x2, y2));
				points.push_back(ClipperPoint(x3, y3));
				return;
			}

			if(cuspLimit != 0.0)
			{
				if(da1 > cuspLimit)
				{
					points.push_back(ClipperPoint(x3, y3));
					return;
				}
			}
		}
		break;

	case 2:
		// p1,p3,p4 are collinear, p2 is significant
		//----------------------
		if(d2 * d2 <= distanceToleranceSquare * (dx*dx + dy*dy))
		{
			if(angleTolerance < angleEpsilon)
			{
				points.push_back(ClipperPoint(x23, y23));
				return;
			}

			// Angle Condition
			//----------------------
			da1 = fabs(atan2(y3 - y2, x3 - x2) - atan2(y2 - y1, x2 - x1));
			if(da1 >= M_PI) da1 = 2*M_PI - da1;

			if(da1 < angleTolerance)
			{
				points.push_back(ClipperPoint(x2, y2));
				points.push_back(ClipperPoint(x3, y3));
				return;
			}

			if(cuspLimit != 0.0)
			{
				if(da1 > cuspLimit)
				{
					points.push_back(ClipperPoint(x2, y2));
					return;
				}
			}
		}
		break;

	case 3:
		// Regular case
		//-----------------
		if((d2 + d3)*(d2 + d3) <= distanceToleranceSquare * (dx*dx + dy*dy))
		{
			// If the curvature doesn't exceed the distance_tolerance value
			// we tend to finish subdivisions.
			//----------------------
			if(angleTolerance < angleEpsilon)
			{
				points.push_back(ClipperPoint(x23, y23));
				return;
			}

			// Angle & Cusp Condition
			//----------------------
			k   = atan2(y3 - y2, x3 - x2);
			da1 = fabs(k - atan2(y2 - y1, x2 - x1));
			da2 = fabs(atan2(y4 - y3, x4 - x3) - k);
			if(da1 >= M_PI) da1 = 2*M_PI - da1;
			if(da2 >= M_PI) da2 = 2*M_PI - da2;

			if(da1 + da2 < angleTolerance)
			{
				// Finally we can stop the recursion
				//----------------------
				points.push_back(ClipperPoint(x23, y23));
				return;
			}

			if(cuspLimit != 0.0)
			{
				if(da1 > cuspLimit)
				{
					points.push_back(ClipperPoint(x2, y2));
					return;
				}

				if(da2 > cuspLimit)
				{
					points.push_back(ClipperPoint(x3, y3));
					return;
				}
			}
		}
		break;
	}

	recursive(x1, y1, x12, y12, x123, y123, x1234, y1234, points, level + 1);
	recursive(x1234, y1234, x234, y234, x34, y34, x4, y4, points, level + 1);
}

ClipperPath AdaptiveFlattener::flatten(const SubpathRef &subpath) const {
	NSVGpath *path = &subpath->nsvg;
	ClipperPath result;

	const ClipperPoint p0 = ClipperPoint(
		path->pts[0] * scale, path->pts[1] * scale);
	result.push_back(p0);

	for (int i = 0; i * 2 + 7 < path->npts * 2; i += 3) {
		const float* p = &path->pts[i * 2];
		flatten(p[0] * scale,p[1] * scale, p[2] * scale,p[3] * scale,
			p[4] * scale,p[5] * scale, p[6] * scale,p[7] * scale, result);
	}

	if (path->closed) {
		result.push_back(p0);
	}

	return result;
}

ClipperPaths AdaptiveFlattener::computeDashes(const NSVGshape *shape, const ClipperPaths &lines) const {
	const int dashCount = shape->strokeDashCount;
	if (dashCount <= 0) {
		return lines;
	}

	ClipperPaths dashes;
	const float *dashArray = shape->strokeDashArray;

	float dashLength = 0.0;
	for (int i = 0; i < dashCount; i++) {
		dashLength += dashArray[i];
	}

	for (int i = 0; i < lines.size(); i++) {
		const ClipperPath &path = lines[i];
		if (path.size() < 2) {
			continue;
		}

		Turtle turtle(path, shape->strokeDashOffset, dashes);
		int dashIndex = 0;

		while (turtle.push(dashArray[dashIndex])) {
			turtle.toggle();
			dashIndex = (dashIndex + 1) % dashCount;
		}
	}

	return dashes;
}

inline ClipperLib::JoinType joinType(int t) {
	switch (t) {
		case NSVG_JOIN_MITER:
		default:
			return ClipperLib::jtMiter;
		case NSVG_JOIN_ROUND:
			return ClipperLib::jtRound;
		case NSVG_JOIN_BEVEL:
			return ClipperLib::jtSquare;
	}
}

inline ClipperLib::EndType endType(int t, bool closed) {
	if (closed) {
		return ClipperLib::etClosedLine;
	}
	switch (t) {
		case NSVG_CAP_BUTT:
			return ClipperLib::etOpenButt;
		case NSVG_CAP_ROUND:
		default:
			return ClipperLib::etOpenRound;
		case NSVG_CAP_SQUARE:
			return ClipperLib::etOpenSquare;
	}
}

void AdaptiveFlattener::operator()(const PathRef &path, Tesselation &tesselation) const {
	const int n = path->getNumSubpaths();
	bool closed = true;
	for (int i = 0; i < n; i++) {
		const auto subpath = path->getSubpath(i);
		tesselation.fill.push_back(flatten(subpath));
		closed = closed && subpath->isClosed();
	}

	NSVGshape * const shape = &path->nsvg;

	ClipperLib::PolyFillType fillType;
	switch (shape->fillRule) {
		case NSVG_FILLRULE_NONZERO: {
			fillType = ClipperLib::pftNonZero;
		} break;
		case NSVG_FILLRULE_EVENODD: {
			fillType = ClipperLib::pftEvenOdd;
		} break;
		default: {
			return;
		} break;
	}

	const bool hasStroke = shape->stroke.type != NSVG_PAINT_NONE && shape->strokeWidth > 0.0f;

	ClipperPaths lines;
	if (hasStroke) {
		lines = computeDashes(shape, tesselation.fill);
	}

	ClipperLib::SimplifyPolygons(tesselation.fill, fillType);

	if (hasStroke) {
		float lineOffset = shape->strokeWidth * scale * 0.5f;
		if (lineOffset < 1.0f) {
			// scaled offsets < 1 will generate artefacts as the ClipperLib's
			// underlying integer resolution cannot handle them.
			lineOffset = 0.0f;
			TOVE_WARN("Ignoring line width < 2. Please use setResolution().");
		}

		ClipperLib::ClipperOffset offset(shape->miterLimit);
		offset.AddPaths(lines,
			joinType(shape->strokeLineJoin),
			endType(shape->strokeLineCap, closed && shape->strokeDashCount == 0));
		offset.Execute(tesselation.stroke, lineOffset);

		ClipperPaths stroke;
		ClipperLib::PolyTreeToPaths(tesselation.stroke, stroke);
		ClipperLib::Clipper clipper;
		clipper.AddPaths(tesselation.fill, ClipperLib::ptSubject, true);
		clipper.AddPaths(stroke, ClipperLib::ptClip, true);
		clipper.Execute(ClipperLib::ctDifference, tesselation.fill);
	}
}



int FixedFlattener::flatten(
	const Vertices &vertices,
	int index, int level,
	float x1, float y1, float x2, float y2,
	float x3, float y3, float x4, float y4) const
{
	if (level >= _depth) {
		auto &v = vertices[index++];
		if (_offset != 0.0) {
			float dx = x4 - x1;
			float dy = y4 - y1;
			float s = _offset / sqrt(dx * dx + dy * dy);
			v.x = x4 - s * dy;
			v.y = y4 + s * dx;
		} else {
			v.x = x4;
			v.y = y4;
		}
		return index;
	}

	float x12,y12,x23,y23,x34,y34,x123,y123,x234,y234,x1234,y1234;

	x12 = (x1+x2)*0.5f;
	y12 = (y1+y2)*0.5f;
	x23 = (x2+x3)*0.5f;
	y23 = (y2+y3)*0.5f;
	x34 = (x3+x4)*0.5f;
	y34 = (y3+y4)*0.5f;
	x123 = (x12+x23)*0.5f;
	y123 = (y12+y23)*0.5f;

	x234 = (x23+x34)*0.5f;
	y234 = (y23+y34)*0.5f;
	x1234 = (x123+x234)*0.5f;
	y1234 = (y123+y234)*0.5f;

	index = flatten(vertices, index, level+1,
		x1,y1, x12,y12, x123,y123, x1234,y1234);
	index = flatten(vertices, index, level+1,
		x1234,y1234, x234,y234, x34,y34, x4,y4);
	return index;
}


int FixedFlattener::size(const SubpathRef &subpath) const {

	NSVGpath *path = &subpath->nsvg;
	const int npts = path->npts;
	const int n = ncurves(npts);
	const int verticesPerCurve = (1 << _depth);
	return 1 + n * verticesPerCurve;
}

int FixedFlattener::flatten(
	const SubpathRef &subpath, const MeshRef &mesh, int index) const {

	NSVGpath *path = &subpath->nsvg;
	const int npts = path->npts;
	const int n = ncurves(npts);

	const int verticesPerCurve = (1 << _depth);
	const int nvertices = 1 + n * verticesPerCurve;
	const auto vertices = mesh->vertices(index, nvertices);

	vertices[0].x = path->pts[0];
	vertices[0].y = path->pts[1];

	int v = 1;
	int k = 0;
	for (int i = 0; i < n; i++) {
		const float *p = &path->pts[k * 2];
		k += 3;
		const int v0 = v;
		v = flatten(vertices, v, 0,
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
		assert(v - v0 == verticesPerCurve);
	}

	assert(v == nvertices);
	return nvertices;
}
