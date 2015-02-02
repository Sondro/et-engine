/*
 * This file is part of `et engine`
 * Copyright 2009-2015 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/rendering/vertexbuffer.h>
#include <et/rendering/indexbuffer.h>

namespace et
{
	class RenderState;
	class VertexArrayObjectData : public APIObject
	{
	public:
		VertexArrayObjectData(RenderContext*, VertexBuffer::Pointer, IndexBuffer,
			const std::string& name = emptyString);
		
		VertexArrayObjectData(RenderContext*, const std::string& name = emptyString);
		
		~VertexArrayObjectData();

		VertexBuffer::Pointer& vertexBuffer()
			{ return _vb; };

		const VertexBuffer::Pointer& vertexBuffer() const
			{ return _vb; };

		IndexBuffer& indexBuffer()
			{ return _ib; };

		const IndexBuffer& indexBuffer() const
			{ return _ib; };

		void setVertexBuffer(VertexBuffer::Pointer ib);
		void setIndexBuffer(IndexBuffer ib);
		void setBuffers(VertexBuffer::Pointer vb, IndexBuffer ib);

	private:
		void init();

	private:
		RenderContext* _rc = nullptr;
		VertexBuffer::Pointer _vb;
		IndexBuffer _ib;
	};
	
	typedef IntrusivePtr<VertexArrayObjectData> VertexArrayObject;
}
