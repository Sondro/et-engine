/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#include <et/app/application.h>
#include <et/rendering/rendercontext.h>
#include <et/scene3d/particlesystem.h>

using namespace et;
using namespace et::s3d;

ParticleSystem::ParticleSystem(RenderInterface::Pointer& rc, uint32_t maxSize, const std::string& name, BaseElement* parent)
    : BaseElement(name, parent)
    , _emitter(maxSize)
    , _decl(true, VertexAttributeUsage::Position, DataType::Vec3)
{
	_decl.push_back(VertexAttributeUsage::Color, DataType::Vec4);

	/*
	 * Init geometry
	 */
	VertexStorage::Pointer vs = VertexStorage::Pointer::create(_decl, maxSize);
	IndexArray::Pointer ia = IndexArray::Pointer::create(IndexArrayFormat::Format_16bit, maxSize, PrimitiveType::Points);
	auto pos = vs->accessData<DataType::Vec3>(VertexAttributeUsage::Position, 0);
	auto clr = vs->accessData<DataType::Vec4>(VertexAttributeUsage::Color, 0);
	for (uint32_t i = 0; i < pos.size(); ++i)
	{
		const auto& p = _emitter.particle(i);
		pos[i] = p.position;
		clr[i] = p.color;
	}
	ia->linearize(maxSize);

	_capacity = vs->capacity();

	auto vb = rc->createVertexBuffer(name + "-vb", vs, Buffer::Location::Host);
	auto ib = rc->createIndexBuffer(name + "-ib", ia, Buffer::Location::Device);
	
	_vertexStream = VertexStream::Pointer::create();
	_vertexStream->setVertexBuffer(vb, vs->declaration());
	_vertexStream->setIndexBuffer(ib, ia->format());
	_vertexStream->setPrimitiveType(ia->primitiveType());
	
	_timer.expired.connect(this, &ParticleSystem::onTimerUpdated);
	_timer.start(currentTimerPool(), 0.0f, NotifyTimer::RepeatForever);
}

ParticleSystem* ParticleSystem::duplicate()
{
	ET_FAIL("Not implemeneted");
	return nullptr;
}

void ParticleSystem::onTimerUpdated(NotifyTimer* timer)
{
	_emitter.update(timer->actualTime());
	
	void* bufferData = _vertexStream->vertexBuffer()->map(0, _capacity);

	auto posOffset = _decl.elementForUsage(VertexAttributeUsage::Position).offset();
	auto clrOffset = _decl.elementForUsage(VertexAttributeUsage::Color).offset();
	
	RawDataAcessor<vec3> pos(reinterpret_cast<char*>(bufferData), _capacity, _decl.sizeInBytes(), posOffset);
	RawDataAcessor<vec4> clr(reinterpret_cast<char*>(bufferData), _capacity + clrOffset, _decl.sizeInBytes(), clrOffset);
	
	for (uint32_t i = 0; i < _emitter.activeParticlesCount(); ++i)
	{
		const auto& p = _emitter.particle(i);
		pos[i] = p.position;
		clr[i] = p.color;
	}
	_vertexStream->vertexBuffer()->unmap();
}
