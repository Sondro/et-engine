/*
 * This file is part of `et engine`
 * Copyright 2009-2015 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <stack>
#include <et/rt/raytraceobjects.h>

namespace et
{
	class ET_ALIGNED(16) KDTree
	{
	public:
		struct Node
		{
			std::vector<uint32_t> triangles;
			uint32_t children[2];
			float splitDistance = 0.0f;
			int splitAxis = 0;
			int containsSubNodes = 0;
		};
		using NodeList = std::vector<Node, SharedBlockAllocatorSTDProxy<Node>>;
		
		struct ET_ALIGNED(16) TraverseResult
		{
			rt::float4 intersectionPoint;
			rt::float4 intersectionPointBarycentric;
			size_t triangleIndex = InvalidIndex;
		};
		
		enum class BuildMode
		{
			Bins,
			SortedArrays,
			BruteForce
		};

	public:
		~KDTree();
		
		void build(const rt::TriangleList&, size_t maxDepth, int splits);
		void cleanUp();
		
		const Node& root() const
			{ return _nodes.front(); }
		
		const Node& nodeAt(size_t i) const
			{ return _nodes.at(i); }

		const rt::BoundingBox& bboxAt(size_t i) const
			{ return _boundingBoxes.at(i); }
		
		TraverseResult traverse(const rt::Ray& r);
		void printStructure();
		
		const rt::Triangle& triangleAtIndex(size_t) const;
		
	private:
		void printStructure(const Node&, const std::string&);
		
		Node buildRootNode();
		void splitNodeUsingBins(Node&, size_t);
		void splitNodeUsingSortedArray(size_t, size_t);
		void buildSplitBoxesUsingAxisAndPosition(size_t nodeIndex, int axis, float position);
		void distributeTrianglesToChildren(Node&);
		
		float findIntersectionInNode(const rt::Ray&, const KDTree::Node&, TraverseResult&);
		
	private:
		NodeList _nodes;
		rt::BoundingBoxList _boundingBoxes;
		rt::TriangleList _triangles;
		size_t _maxDepth = 0;
		size_t _minTrianglesToSubdivide = 16;
		int _spaceSplitSize = 32;
		BuildMode _buildMode = BuildMode::SortedArrays;
	};
}