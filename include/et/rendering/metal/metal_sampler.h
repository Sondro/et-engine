/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/rendering/interface/sampler.h>

namespace et
{

class MetalState;
class MetalNativeSampler;
class MetalSamplerPrivate;
class MetalSampler : public Sampler
{
public:
	ET_DECLARE_POINTER(MetalSampler);

public:
	MetalSampler(MetalState&);
	~MetalSampler();

	const MetalNativeSampler& nativeSampler() const;

private:
	ET_DECLARE_PIMPL(MetalSampler, 64);
};

}
