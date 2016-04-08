/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#include <et-ext/rt/kdtree.h>

namespace et
{
namespace rt
{

const size_t DepthLimit = 31;
const size_t MinTrianglesToSubdivide = 12;

struct Split
{
	vec3 cost = vec3(0.0f);
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
	
	float4 minVertex = _triangles.front().v[0];
	float4 maxVertex = minVertex;
	
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
	_boundingBoxes.emplace_back(center, halfSize);
	_indexes.reserve(sqr(_triangles.size() + 1));
	
	KDTree::Node result;
	result.children[0] = InvalidIndex;
	result.children[1] = InvalidIndex;
	result.axis = -1;
	result.distance = 0.0f;
	result.startIndex = 0;
	result.endIndex = static_cast<index>(_triangles.size());
	for (index i = 0; i < result.endIndex; ++i)
	{
		_indexes.push_back(i);
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
	_nodes.push_back(buildRootNode());
	
	splitNodeUsingSortedArray(0, 0);
}

vec3 triangleCentroid(const rt::Triangle& t)
{
	float4 maxV = t.v[0].maxWith(t.v[1].maxWith(t.v[2]));
	float4 minV = t.v[0].minWith(t.v[1].minWith(t.v[2]));
	return ((maxV + minV) * 0.5f).xyz();
}

void KDTree::buildSplitBoxesUsingAxisAndPosition(size_t nodeIndex, int axis, float position)
{
	auto bbox = _boundingBoxes.at(nodeIndex);
	
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
	
	_nodes.at(nodeIndex).axis = axis;
	_nodes.at(nodeIndex).distance = position;
	_nodes.at(nodeIndex).children[0] = static_cast<index>(_nodes.size());
	_nodes.emplace_back();
	_nodes.back().children[0] = InvalidIndex;
	_nodes.back().children[1] = InvalidIndex;
	_nodes.back().axis = -1;
	_nodes.back().distance = 0.0f;

	_boundingBoxes.emplace_back(bbox.center * axisScale + posScale * (middlePoint - leftSize),
		bbox.halfSize * axisScale + posScale * leftSize);

	_nodes.at(nodeIndex).children[1] = static_cast<index>(_nodes.size());
	_nodes.emplace_back();
	_nodes.back().children[0] = InvalidIndex;
	_nodes.back().children[1] = InvalidIndex;
	_nodes.back().axis = -1;
	_nodes.back().distance = 0.0f;

	_boundingBoxes.emplace_back(bbox.center * axisScale + posScale * (middlePoint + rightSize),
		bbox.halfSize * axisScale + posScale * rightSize);
}

void KDTree::distributeTrianglesToChildren(size_t nodeIndex)
{
	ET_ALIGNED(16) vec4 minVertex;
	ET_ALIGNED(16) vec4 maxVertex;
	
	auto& node = _nodes.at(nodeIndex);

	Vector<index> rightIndexes;
	rightIndexes.reserve(node.numIndexes());
	Vector<index> leftIndexes;
	leftIndexes.reserve(node.numIndexes());

	for (index i = node.startIndex, e = node.startIndex + node.numIndexes(); i < e; ++i)
	{
		index triIndex = _indexes[i];
		const auto& tri = _triangles.at(triIndex);
		tri.minVertex().loadToFloats(minVertex.data());
		tri.maxVertex().loadToFloats(maxVertex.data());
		
		if (minVertex[node.axis] > node.distance)
		{
			rightIndexes.push_back(triIndex);
		}
		else if (maxVertex[node.axis] < node.distance)
		{
			leftIndexes.push_back(triIndex);
		}
		else
		{
			rightIndexes.push_back(triIndex);
			leftIndexes.push_back(triIndex);
		}
	}


	auto& left = _nodes.at(node.children[0]);
	left.startIndex = static_cast<index>(_indexes.size());
	left.endIndex = left.startIndex + static_cast<index>(leftIndexes.size());
	_indexes.insert(_indexes.end(), leftIndexes.begin(), leftIndexes.end());

	auto& right = _nodes.at(node.children[1]);
	right.startIndex = static_cast<index>(_indexes.size());
	right.endIndex = right.startIndex + static_cast<index>(rightIndexes.size());
	_indexes.insert(_indexes.end(), rightIndexes.begin(), rightIndexes.end());
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
	const auto& bbox = _boundingBoxes.at(nodeIndex);
	
	auto estimateCostAtSplit = [&bbox, nodeIndex, this](float splitPlane, size_t leftTriangles,
		size_t rightTriangles, int axis) -> float
	{
		ET_ASSERT((leftTriangles + rightTriangles) == _nodes.at(nodeIndex).numIndexes());
		
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
	
	std::vector<vec3> minPoints;
	std::vector<vec3> maxPoints;
	const auto& localNode = _nodes.at(nodeIndex);
	for (index i = localNode.startIndex, e = localNode.startIndex + localNode.numIndexes(); i < e; ++i)
	{
		const auto& tri = _triangles.at(_indexes[i]);
		minPoints.push_back(tri.minVertex().xyz() - vec3(Constants::epsilon));
		maxPoints.push_back(tri.maxVertex().xyz() + vec3(Constants::epsilon));
	}
	
	vec3 splitPosition = minPoints.at(minPoints.size() / 2);
	vec3 splitCost(Constants::initialSplitValue);
	
	bool splitFound = false;
	int numElements = static_cast<int>(minPoints.size());
	
	for (int currentAxis = 0; currentAxis < 3; ++currentAxis)
	{
		std::sort(minPoints.begin(), minPoints.end(), [&currentAxis](const vec3& l, const vec3& r)
			{ return l[currentAxis] < r[currentAxis]; });
		
		std::sort(maxPoints.begin(), maxPoints.end(), [&currentAxis](const vec3& l, const vec3& r)
			{ return l[currentAxis] < r[currentAxis]; });
		
		for (int i = 1; i + 1 < numElements; ++i)
		{
			float costMin = estimateCostAtSplit(minPoints.at(i)[currentAxis], i, numElements - i, currentAxis);
			if (compareAndAssignMinimum(splitCost[currentAxis], costMin))
			{
				splitPosition[currentAxis] = minPoints.at(i)[currentAxis];
				splitFound = true;
			}
		}
		
		for (int i = numElements - 2; i > 0; --i)
		{
			float costMax = estimateCostAtSplit(maxPoints.at(i)[currentAxis], i, numElements - i, currentAxis);
			if (compareAndAssignMinimum(splitCost[currentAxis], costMax))
			{
				splitPosition[currentAxis] = maxPoints.at(i)[currentAxis];
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
			splitNodeUsingSortedArray(_nodes.at(nodeIndex).children[0], depth + 1);
			splitNodeUsingSortedArray(_nodes.at(nodeIndex).children[1], depth + 1);
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
	if (node.axis >= 0)
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
	return _triangles.at(i);
}

struct KDTreeSearchNode
{
	index ind;
    float time;
    
    KDTreeSearchNode() = default;
	KDTreeSearchNode(index n, float t) :
        ind(n), time(t) { }
};

KDTree::TraverseResult KDTree::traverse(const Ray& ray)
{
	auto localEpsilon = Constants::epsilon;
	auto localEpsilonSquared = Constants::epsilonSquared;
	auto localOnePlusEpsilon = Constants::onePlusEpsilon;

	KDTree::TraverseResult result;
	
	float tNear = 0.0f;
	float tFar = 0.0f;
	
	if (!rayToBoundingBox(ray, _boundingBoxes.front(), tNear, tFar))
		return result;
	
	if (tNear < localEpsilon)
		tNear = localEpsilon;
    
    tNear -= localEpsilon;
    tFar += localEpsilon;
	
	ET_ALIGNED(16) float direction[4];
	ray.direction.loadToFloats(direction);

	ET_ALIGNED(16) float originDivDirection[4];
	(ray.origin / ray.direction).loadToFloats(originDivDirection);

	Node localNode = _nodes.front();
	FastStack<DepthLimit + 1, KDTreeSearchNode> traverseStack;
	for (;;)
	{
		while (localNode.axis >= 0)
		{
			int side = floatIsNegative(direction[localNode.axis]);
			float tSplit = localNode.distance / direction[localNode.axis] - originDivDirection[localNode.axis];
			float tNearSplit = tSplit - tFar;
			float tFarSplit = tSplit - tNear;

			if (floatIsNegative(tFarSplit))
			{
				localNode = _nodes[localNode.children[1 - side]];
			}
			else if (floatIsPositive(tNearSplit))
			{
				localNode = _nodes[localNode.children[side]];
			}
			else
			{
				traverseStack.emplace(localNode.children[1 - side], tFar);
				localNode = _nodes[localNode.children[side]];
				tFar = tSplit;
			}
		}

		if (localNode.numIndexes() > 0)
		{
			result.triangleIndex = InvalidIndex;

			float minDistance = std::numeric_limits<float>::max();
			for (index i = localNode.startIndex, e = localNode.endIndex; i < e; ++i)
			{
				auto triangleIndex = _indexes[i];
				auto data = _intersectionData[triangleIndex];

				float4 pvec = ray.direction.crossXYZ(data.edge2to0);
				float det = data.edge1to0.dot(pvec);
				if (det * det >= localEpsilonSquared)
				{
					float4 tvec = ray.origin - data.v0;
					float u = tvec.dot(pvec) / det;
					if ((u >= localEpsilon) && (u <= localOnePlusEpsilon))
					{
						float4 qvec = tvec.crossXYZ(data.edge1to0);
						float v = ray.direction.dot(qvec) / det;
						float uv = u + v;
						if ((v >= localEpsilon) && (uv <= localOnePlusEpsilon))
						{
							float intersectionDistance = data.edge2to0.dot(qvec) / det;
							if ((intersectionDistance <= minDistance) && (intersectionDistance <= tFar) && (intersectionDistance >= localEpsilon))
							{
								result.triangleIndex = triangleIndex;
								result.intersectionPointBarycentric = float4(1.0f - uv, u, v, 0.0f);
								minDistance = intersectionDistance;
							}
						}
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
        tNear = tFar - Constants::epsilon;
		tFar = traverseStack.top().time + Constants::epsilon;
		
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
	
	uint32_t index = 0;
	for (const auto& node : _nodes)
	{
		if (node.axis == -1)
		{
			++result.leafNodes;

			if (node.numIndexes() == 0)
				++result.emptyLeafNodes;
		}
		
		if (node.numIndexes() > 0)
		{
			result.maxTrianglesPerNode = std::max(result.maxTrianglesPerNode, node.numIndexes());
			result.minTrianglesPerNode = std::min(result.minTrianglesPerNode, node.numIndexes());
		}
		
		result.distributedTriangles += node.numIndexes();
		
		++index;
	}
	return result;
}
}
}