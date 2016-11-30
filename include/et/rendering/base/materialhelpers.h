/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/rendering/interface/program.h>
#include <et/rendering/interface/textureset.h>
#include <et/rendering/base/vertexdeclaration.h>

namespace et
{
enum class MaterialTexture : uint32_t
{
	Albedo,
	Reflectance,
	Emissive,

	Roughness,
	Opacity,
	Normal,
	
	Shadow,
	AmbientOcclusion,
	Environment,

	Count,

	FirstSharedTexture = Shadow,
};

enum class MaterialParameter : uint32_t
{
	AlbedoColor,
	ReflectanceColor,
	EmissiveColor,

	Roughness,
	Metallness,
	Opacity,
	NormalScale,

	IndexOfRefraction,
	SpecularExponent,

	Count
};


enum : uint32_t
{
	MaterialTexturesCount = static_cast<uint32_t>(MaterialTexture::Count),
	MaterialParametersCount = static_cast<uint32_t>(MaterialParameter::Count),
};

struct MaterialTextureHolder
{
	MaterialTexture binding = MaterialTexture::Count;
	Texture::Pointer texture;
	uint32_t index = 0;
};
using MaterialTexturesCollection = UnorderedMap<String, MaterialTextureHolder>;

struct MaterialSamplerHolder
{
	MaterialTexture binding = MaterialTexture::Count;
	Sampler::Pointer sampler;
	uint32_t index = 0;
};
using MaterialSamplersCollection = UnorderedMap<String, MaterialSamplerHolder>;

struct MaterialPropertyHolder
{
	MaterialParameter binding = MaterialParameter::Count;
	char data[sizeof(vec4)] { };
	uint32_t size = 0;
};
using MaterialPropertiesCollection = UnorderedMap<String, MaterialPropertyHolder>;

namespace mtl
{

template <class T>
struct OptionalObject
{
	T object;
	MaterialTexture binding = MaterialTexture::Count;
	uint32_t index = 0;

	void clear()
	{
		index = 0;
		object = nullptr;
	}
};

struct OptionalValue
{
	DataType storedType = DataType::max;
	MaterialParameter binding = MaterialParameter::Count;
	char data[sizeof(vec4)] { };
	uint32_t size = 0;

	bool isSet() const
		{ return storedType != DataType::max; }

	template <class T>
	const T& as() const
	{
		static_assert(sizeof(T) <= sizeof(vec4), "Requested type is too large");
		ET_ASSERT(is<T>());
		return *(reinterpret_cast<const T*>(data));
	};

	template <class T>
	bool is() const
	{
		static_assert(sizeof(T) <= sizeof(vec4), "Requested type is too large");
		return storedType == dataTypeFromClass<T>();
	}

	template <class T>
	void operator = (const T& value)
	{
		static_assert(sizeof(T) <= sizeof(vec4), "Requested type is too large");
		*(reinterpret_cast<T*>(data)) = value;
		storedType = dataTypeFromClass<T>();
		size = sizeof(value);
	}

	void clear()
	{
		memset(data, 0, sizeof(data));
		storedType = DataType::max;
	}
};

using Textures = std::array<OptionalObject<Texture::Pointer>, MaterialTexturesCount>;
using Samplers = std::array<OptionalObject<Sampler::Pointer>, MaterialTexturesCount>;
using Parameters = std::array<OptionalValue, MaterialParametersCount>;

const String& materialParameterToString(MaterialParameter);
const String& materialSamplerToString(MaterialTexture);

const String& materialTextureToString(MaterialTexture);
MaterialTexture stringToMaterialTexture(const String&);

}
}
