/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/scene3d/drawer/debugdrawer.h>
#include <et/scene3d/drawer/shadowmaps.h>
#include <et/scene3d/drawer/cubemaps.h>

namespace et {
namespace s3d {
class Drawer : public Object, public FlagsHolder
{
public:
	ET_DECLARE_POINTER(Drawer);

	DrawerOptions options;

	enum class SupportTexture : uint32_t
	{
		Velocity,
		ScreenspaceShadows,
		ScreenspaceAO,
	};

public:
	Drawer(const RenderInterface::Pointer&);

	void setRenderTarget(const Texture::Pointer&);
	void setScene(const Scene::Pointer&);
	void setEnvironmentMap(const std::string&);
	void updateBaseProjectionMatrix(const mat4&);
	void updateLight();

	void draw();

	const Light::Pointer& directionalLight();
	const Texture::Pointer& supportTexture(SupportTexture);
	const vec4& latestCameraJitter() const;

	RenderInterface::Pointer renderInterface() const;

private:
	void updateVisibleMeshes();
	void validate(RenderInterface::Pointer&);

private:
	ObjectsCache _cache;

	Scene::Pointer _scene;
	Camera::Pointer _frameCamera;
	Vector<Mesh::Pointer> _allMeshes;
	Vector<Mesh::Pointer> _visibleMeshes;

	RenderInterface::Pointer _renderer;
	DebugDrawer::Pointer _debugDrawer;
	CubemapProcessor::Pointer _cubemapProcessor = CubemapProcessor::Pointer(PointerInit::CreateInplace);
	ShadowmapProcessor::Pointer _shadowmapProcessor = ShadowmapProcessor::Pointer(PointerInit::CreateInplace);

	struct MainPass
	{
		RenderPass::Pointer zPrepass;
		RenderPass::Pointer forward;
		RenderPass::Pointer screenSpaceShadows;
		RenderBatch::Pointer screenSpaceShadowsBatch;
		RenderPass::Pointer screenSpaceAO;
		RenderBatch::Pointer screenSpaceAOBatch;
		Texture::Pointer color;
		Texture::Pointer depth;
		Texture::Pointer velocity;
		Texture::Pointer noise;
		Texture::Pointer screenSpaceShadowsTexture;
		Texture::Pointer screenSpaceAOTexture;
	} _main;

	struct Lighting
	{
		Light::Pointer directional = Light::Pointer::create(Light::Type::Directional);
		Material::Pointer environmentMaterial;
		RenderBatch::Pointer environmentBatch;
		std::string environmentTextureFile;
	} _lighting;

	mat4 _baseProjectionMatrix;
	vec4 _jitter;
	uint64_t _frameIndex = 0;
};

inline const Light::Pointer& Drawer::directionalLight() {
	return _lighting.directional;
}

inline const Texture::Pointer& Drawer::supportTexture(SupportTexture tex) {
	switch (tex)
	{
	case Drawer::SupportTexture::Velocity:
		return _main.velocity;

	case Drawer::SupportTexture::ScreenspaceShadows:
		return _main.screenSpaceShadowsTexture;

	case Drawer::SupportTexture::ScreenspaceAO:
		return _main.screenSpaceAOTexture;

	default:
	{
		ET_ASSERT(!"Invalid support texture requested");
		return _renderer->blackTexture();
	}
	}
}

inline const vec4& Drawer::latestCameraJitter() const {
	return _jitter;
}

inline RenderInterface::Pointer Drawer::renderInterface() const {
	return _renderer;
}

}
}
