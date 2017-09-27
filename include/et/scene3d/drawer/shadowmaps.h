/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/scene3d/drawer/common.h>

namespace et
{
namespace s3d
{

class ShadowmapProcessor : public Shared
{
public:
	ET_DECLARE_POINTER(ShadowmapProcessor);

public:
	const Texture::Pointer& directionalShadowmap() const;
	const Texture::Pointer& directionalShadowmapMoments() const;

	void setScene(const Scene::Pointer& scene, Light::Pointer& light);
	void process(RenderInterface::Pointer& renderer, DrawerOptions& options);
	void updateLight(Light::Pointer& light);

	const BoundingBox& sceneBoundingBox() const 
		{ return _sceneBoundingBox; }

private:
	void validate(RenderInterface::Pointer& renderer);
	void setupProjection(DrawerOptions& options);

private:
	Texture::Pointer _directionalShadowmapMoments;
	Texture::Pointer _directionalShadowmapMomentsBuffer;
	Texture::Pointer _directionalShadowmap;
	Scene::Pointer _scene;
	BoundingBox _sceneBoundingBox;
	Light::Pointer _light;

	struct Renderables
	{
		RenderPass::Pointer shadowpass;
		Vector<Mesh::Pointer> meshes;

		RenderBatch::Pointer debugColorBatch;
		RenderBatch::Pointer debugDepthBatch;
		RenderPass::Pointer debugPass;

		RenderPass::Pointer blurPass0;
		RenderBatch::Pointer blurBatch0;
		
		RenderPass::Pointer blurPass1;
		RenderBatch::Pointer blurBatch1;

		bool initialized = false;
	} _renderables;
};

inline const Texture::Pointer& ShadowmapProcessor::directionalShadowmap() const {
	return _directionalShadowmap;
}

inline const Texture::Pointer& ShadowmapProcessor::directionalShadowmapMoments() const {
	return _directionalShadowmapMoments;
}

}
}
