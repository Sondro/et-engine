/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#include <et-ext/rt/kdtree.h>
#include <et/core/tools.h>

namespace et
{
namespace rt
{

const size_t DepthLimit = 128;
const size_t MinTrianglesToSubdivide = 12;

struct Split
{
	float3 cost = float3(0.0f);
	int index = 0;
	int axis = 0;
};

KDTree::~KDTree()
{
	cleanUp();
}

rt::KDTree::Node KDTree::buildRootNode()
{
	_intersectionData.reserve(_triangles.size());
	_boundingBoxes.reserve(32 + _triangles.size() / 32);
	
	float4 minVertex = float4(+std::numeric_limits<float>::max());
	float4 maxVertex = float4(-std::numeric_limits<float>::max());;
	
	for (const auto& t : _triangles)
	{
		minVertex = minVertex.minWith(t.v[0]);
		minVertex = minVertex.minWith(t.v[1]);
		minVertex = minVertex.minWith(t.v[2]);
		maxVertex = maxVertex.maxWith(t.v[0]);
		maxVertex = maxVertex.maxWith(t.v[1]);
		maxVertex = maxVertex.maxWith(t.v[2]);
		_intersectionData.emplace_back(t.v[0], t.edge1to0, t.edge2to0);
	}
	
	float4 center = (minVertex + maxVertex) * float4(0.5f);
	float4 halfSize = (maxVertex - minVertex) * float4(0.5f);
	_indices.reserve(16 * _triangles.size());
	
	_boundingBoxes.clear();
    _boundingBoxes.emplace_back(center, halfSize);
	_sceneBoundingBox = _boundingBoxes.back();

	KDTree::Node result;
	result.endIndex = static_cast<uint32_t>(_triangles.size());
	for (uint32_t i = 0; i < result.endIndex; ++i)
	{
		_indices.emplace_back(i);
	}
	return result;
}

void KDTree::build(const TriangleList& triangles, size_t maxDepth)
{
	cleanUp();
	
	_maxBuildDepth = 0;
	_triangles = triangles;
	
	_maxDepth = std::min(DepthLimit, maxDepth);
	_nodes.reserve(maxDepth * maxDepth);
	_nodes.emplace_back(buildRootNode());

	uint64_t t0 = queryContinuousTimeInMilliSeconds();
	splitNodeUsingSortedArray(0, 0);
	uint64_t t1 = queryContinuousTimeInMilliSeconds();
	log::info("kD-tree building time: %llu", t1 - t0);
}

void KDTree::buildSplitBoxesUsingAxisAndPosition(size_t nodeIndex, int axis, float position)
{
	auto bbox = _boundingBoxes[nodeIndex];
	
	float4 lowerCorner = bbox.minVertex();
	float4 upperCorner = bbox.maxVertex();
	
	vec4 axisScale4(1.0f);
	axisScale4[axis] = 0.0f;
	
	vec4 posScale4(0.0f);
	posScale4[axis] = 1.0f;
	
	float4 axisScale(axisScale4);
	float4 posScale(posScale4);
	
	float4 middlePoint = lowerCorner * axisScale + posScale * position;
	float4 leftSize = (middlePoint - lowerCorner) * posScale * 0.5f;
	float4 rightSize = (upperCorner - middlePoint) * posScale * 0.5f;
	
	_nodes[nodeIndex].axis = axis;
	_nodes[nodeIndex].distance = position;
	_nodes[nodeIndex].children[0] = static_cast<uint32_t>(_nodes.size());
	_nodes.emplace_back();
	_nodes.back().children[0] = InvalidIndex;
	_nodes.back().children[1] = InvalidIndex;
	_nodes.back().axis = InvalidIndex;
	_nodes.back().distance = 0.0f;

	_boundingBoxes.emplace_back(bbox.center * axisScale + posScale * (middlePoint - leftSize),
		bbox.halfSize * axisScale + posScale * leftSize);

	_nodes[nodeIndex].children[1] = static_cast<uint32_t>(_nodes.size());
	_nodes.emplace_back();
	_nodes.back().children[0] = InvalidIndex;
	_nodes.back().children[1] = InvalidIndex;
	_nodes.back().axis = InvalidIndex;
	_nodes.back().distance = 0.0f;

	_boundingBoxes.emplace_back(bbox.center * axisScale + posScale * (middlePoint + rightSize),
		bbox.halfSize * axisScale + posScale * rightSize);
}

void KDTree::distributeTrianglesToChildren(size_t nodeIndex)
{
	ET_ALIGNED(16) vec4 minVertex;
	ET_ALIGNED(16) vec4 maxVertex;
	
	auto& node = _nodes[nodeIndex];

	static Vector<uint32_t> rightIndexes;
	rightIndexes.reserve(32 * 1024);
	rightIndexes.clear();
	
	static Vector<uint32_t> leftIndexes;
	leftIndexes.reserve(32 * 1024);
	leftIndexes.clear();

	for (uint32_t i = node.startIndex, e = node.startIndex + node.numIndexes(); i < e; ++i)
	{
		uint32_t triIndex = _indices[i];
		const auto& tri = _triangles[triIndex];
		tri.minVertex().loadToFloats(minVertex.data());
		tri.maxVertex().loadToFloats(maxVertex.data());
		
		if (minVertex[node.axis] > node.distance)
		{
			rightIndexes.emplace_back(triIndex);
		}
		else if (maxVertex[node.axis] < node.distance)
		{
			leftIndexes.emplace_back(triIndex);
		}
		else
		{
			rightIndexes.emplace_back(triIndex);
			leftIndexes.emplace_back(triIndex);
		}
	}

	auto& left = _nodes[node.children[0]];
	left.startIndex = static_cast<uint32_t>(_indices.size());
	left.endIndex = left.startIndex + static_cast<uint32_t>(leftIndexes.size());
	_indices.insert(_indices.end(), leftIndexes.begin(), leftIndexes.end());

	auto& right = _nodes[node.children[1]];
	right.startIndex = static_cast<uint32_t>(_indices.size());
	right.endIndex = right.startIndex + static_cast<uint32_t>(rightIndexes.size());
	_indices.insert(_indices.end(), rightIndexes.begin(), rightIndexes.end());
}

void KDTree::cleanUp()
{
	_nodes.clear();
	_triangles.clear();
}

void KDTree::splitNodeUsingSortedArray(size_t nodeIndex, size_t depth)
{
	auto numTriangles = _nodes[nodeIndex].numIndexes();
	if ((depth > _maxDepth) || (numTriangles < MinTrianglesToSubdivide))
	{
		return;
	}
		
	_maxBuildDepth = std::max(_maxBuildDepth, depth);
	const auto& bbox = _boundingBoxes[nodeIndex];
	
	auto estimateCostAtSplit = [bbox, nodeIndex, this](float splitPlane, size_t leftTriangles,
		size_t rightTriangles, int axis) -> float
	{
		ET_ASSERT((leftTriangles + rightTriangles) == _nodes[nodeIndex].numIndexes());
		
		const vec4& minVertex = bbox.minVertex().toVec4();
		if (splitPlane <= minVertex[axis] + Constants::epsilon)
			return std::numeric_limits<float>::max();

		const vec4& maxVertex = bbox.maxVertex().toVec4();
		if (splitPlane >= maxVertex[axis] - Constants::epsilon)
			return std::numeric_limits<float>::max();

		vec4 axisScale(1.0f);
		vec4 axisOffset(0.0f);
		axisScale[axis] = 0.0f;
		axisOffset[axis] = splitPlane;
		BoundingBox leftBox(bbox.minVertex(), bbox.maxVertex() * float4(axisScale) + float4(axisOffset), 0);
		BoundingBox rightBox(bbox.minVertex() * float4(axisScale) + float4(axisOffset), bbox.maxVertex(), 0);
		
		float totalSquare = bbox.square();
		float leftSquare = leftBox.square() / totalSquare;
		float rightSquare = rightBox.square() / totalSquare;
		float costLeft = leftSquare * float(leftTriangles);
		float costRight = rightSquare * float(rightTriangles);
		return costLeft + costRight;
	};
	
	auto compareAndAssignMinimum = [](float& minCost, float cost) -> bool
	{
		bool result = (cost < minCost);
		if (result)
			minCost = cost;
		return result;
	};
	
	const auto& localNode = _nodes[nodeIndex];

	static Vector<float3> minPoints;
	static Vector<float3> maxPoints;
	minPoints.reserve(32 * 1024);
	maxPoints.reserve(32 * 1024);

	minPoints.clear();
	maxPoints.clear();
	for (uint32_t i = localNode.startIndex, e = localNode.endIndex; i < e; ++i)
	{
		const auto& tri = _triangles[_indices[i]];
		minPoints.emplace_back(tri.minVertex().xyz());
		maxPoints.emplace_back(tri.maxVertex().xyz());
	}
	
	float3 splitPosition = minPoints[minPoints.size() / 2];
	float3 splitCost(Constants::initialSplitValue);
	
	bool splitFound = false;
	int numElements = static_cast<int>(minPoints.size());

	for (int currentAxis = 0; currentAxis < 3; ++currentAxis)
	{
		std::sort(minPoints.begin(), minPoints.end(), [currentAxis](const vec3& l, const vec3& r)
			{ return l[currentAxis] < r[currentAxis]; });
		
		std::sort(maxPoints.begin(), maxPoints.end(), [currentAxis](const vec3& l, const vec3& r)
			{ return l[currentAxis] < r[currentAxis]; });

		for (int i = 1; i + 1 < numElements; ++i)
		{
			float costMin = estimateCostAtSplit(minPoints[i][currentAxis], i, numElements - i, currentAxis);
			if (compareAndAssignMinimum(splitCost[currentAxis], costMin))
			{
				splitPosition[currentAxis] = minPoints[i][currentAxis];
				splitFound = true;
			}
		}
		
		for (int i = numElements - 2; i > 0; --i)
		{
			float costMax = estimateCostAtSplit(maxPoints[i][currentAxis], i, numElements - i, currentAxis);
			if (compareAndAssignMinimum(splitCost[currentAxis], costMax))
			{
				splitPosition[currentAxis] = maxPoints[i][currentAxis];
				splitFound = true;
			}
		}
	}
	
	float targetValue = std::min(splitCost.x, std::min(splitCost.y, splitCost.z));
	for (int currentAxis = 0; splitFound && (currentAxis < 3); ++currentAxis)
	{
		if (splitCost[currentAxis] == targetValue)
		{
			buildSplitBoxesUsingAxisAndPosition(nodeIndex, currentAxis, splitPosition[currentAxis]);
			distributeTrianglesToChildren(nodeIndex);
			splitNodeUsingSortedArray(_nodes[nodeIndex].children[0], depth + 1);
			splitNodeUsingSortedArray(_nodes[nodeIndex].children[1], depth + 1);
			break;
		}
	}
}

void KDTree::printStructure()
{
	printStructure(_nodes.front(), std::string());
}

void KDTree::printStructure(const Node& node, const std::string& tag)
{
	const char* axis[] = { "X", "Y", "Z" };
	if (node.axis <= MaxAxisIndex)
	{
		log::info("%s %s, %.2f", tag.c_str(), axis[node.axis], node.distance);
		printStructure(_nodes.at(node.children[0]), tag + "--|");
		printStructure(_nodes.at(node.children[1]), tag + "--|");
	}
	else
	{
		log::info("%s %u tris", tag.c_str(), node.numIndexes());
	}
}

const Triangle& KDTree::triangleAtIndex(size_t i) const
{
	return _triangles[i];
}

struct KDTreeSearchNode
{
	uint32_t ind;
    float time;
    
	KDTreeSearchNode(uint32_t n, float t) :
        ind(n), time(t) { }
};

KDTree::TraverseResult KDTree::traverse(const Ray& ray) const
{
	KDTree::TraverseResult result;
	
    float eps = Constants::epsilon;

	float tNear = 0.0f;
	float tFar = 0.0f;
	
	if (!rayToBoundingBox(ray, _sceneBoundingBox, tNear, tFar))
		return result;
	
	if (tNear < 0.0f)
		tNear = 0.0f;

	ET_ALIGNED(16) float direction[4];
	ray.direction.reciprocal().loadToFloats(direction);

	ET_ALIGNED(16) float originDivDirection[4];
	(ray.origin / (ray.direction + rt::float4(std::numeric_limits<float>::epsilon()))).loadToFloats(originDivDirection);
    
	const IntersectionData* intersectionDataPtr = _intersectionData.data();
	const uint32_t* indicesPtr = _indices.data();

	Node localNode = _nodes.front();
	FastStack<DepthLimit + 1, KDTreeSearchNode> traverseStack;
	for (;;)
	{
		while (localNode.axis <= MaxAxisIndex)
		{
            union { float f; int i; } side = { direction[localNode.axis] };
            side.i = (side.i & 0x80000000) >> 31;
			float tSplit = localNode.distance * direction[localNode.axis] - originDivDirection[localNode.axis];
            
			if (tSplit < tNear)
			{
				localNode = _nodes[localNode.children[1 - side.i]];
			}
			else if (tSplit > tFar)
			{
				localNode = _nodes[localNode.children[side.i]];
			}
			else
			{
				traverseStack.emplace(localNode.children[1 - side.i], tFar);
				localNode = _nodes[localNode.children[side.i]];
				tFar = tSplit;
			}
		}

		if (localNode.nonEmpty())
		{
			result.triangleIndex = InvalidIndex;

			ET_ALIGNED(16) float minDistance = std::numeric_limits<float>::max();
			for (uint32_t i = localNode.startIndex, e = localNode.endIndex; i < e; ++i)
			{
				uint32_t triangleIndex = indicesPtr[i];
				IntersectionData data = intersectionDataPtr[triangleIndex];
				
				float4 pvec = ray.direction.crossXYZ(data.edge2to0);
				union
				{
					float f;
					uint32_t i;
				} det = { data.edge1to0.dot(pvec) };

				if (!(det.i & 0x7fffffff))
					continue;

				float inv_dev = 1.0f / det.f;

				float4 tvec = ray.origin - data.v0;
				float u = tvec.dot(pvec) * inv_dev;
				if ((u < 0.0f) || (u > 1.0f))
					continue;

				float4 qvec = tvec.crossXYZ(data.edge1to0);
				float t = data.edge2to0.dot(qvec) * inv_dev;
				if ((t < minDistance) && (t <= tFar) && (t > Constants::epsilon))
				{
					float v = ray.direction.dot(qvec) * inv_dev;
					float uv = u + v;
					if ((v >= 0.0f) && (uv <= 1.0f))
					{
						minDistance = t;
						result.triangleIndex = triangleIndex;
						result.intersectionPointBarycentric = float4(1.0f - uv, u, v, 0.0f);
					}
				}
			}

			if (result.triangleIndex < InvalidIndex)
			{
				result.intersectionPoint = ray.origin + ray.direction * minDistance;
				return result;
			}
		}
		
		if (traverseStack.empty())
		{
			result.triangleIndex = InvalidIndex;
			return result;
		}
		
		localNode = _nodes[traverseStack.top().ind];
        tNear = tFar - eps;
		tFar = traverseStack.top().time + eps;

		traverseStack.pop();
	}
	
	return result;
}

KDTree::Stats KDTree::nodesStatistics() const
{
	KDTree::Stats result;
	result.totalNodes = _nodes.size();
	result.maxDepth = _maxBuildDepth;
	result.totalTriangles = _triangles.size();
	for (const auto& node : _nodes)
	{
		if (node.axis == InvalidIndex)
		{
			++result.leafNodes;

			if (node.empty())
				++result.emptyLeafNodes;
		}
		
		if ((node.children[0] == InvalidIndex) && (node.children[1] == InvalidIndex) && (node.numIndexes() > 0))
		{
			result.maxTrianglesPerNode = std::max(result.maxTrianglesPerNode, node.numIndexes());
			result.minTrianglesPerNode = std::min(result.minTrianglesPerNode, node.numIndexes());
		}
		
		result.distributedTriangles += node.numIndexes();
	}
	return result;
}

}
}
