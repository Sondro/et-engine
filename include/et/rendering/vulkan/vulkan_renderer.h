/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/rendering/interface/renderer.h>

namespace et {
class VulkanRendererPrivate;
class VulkanRenderer : public RenderInterface
{
public:
	ET_DECLARE_POINTER(VulkanRenderer);

public:
	RenderingAPI api() const override {
		return RenderingAPI::Vulkan;
	}

	VulkanRenderer();
	~VulkanRenderer();

	void init(const RenderContextParameters& params) override;
	void shutdown() override;
	void destroy() override;

	void resize(const vec2i&) override;
	vec2i contextSize() const override;

	RendererFrame allocateFrame() override;
	void submitFrame(const RendererFrame&) override;

	void present() override;

	RenderPass::Pointer allocateRenderPass(const RenderPass::ConstructionInfo&) override;
	void beginRenderPass(const RenderPass::Pointer&, const RenderPassBeginInfo&) override;
	void submitRenderPass(const RenderPass::Pointer&) override;

	/*
	 * Buffers
	 */
	Buffer::Pointer createBuffer(const std::string&, const Buffer::Description&) override;

	/*
	 * Textures
	 */
	Texture::Pointer createTexture(const TextureDescription::Pointer&) override;
	TextureSet::Pointer createTextureSet(const TextureSet::Description&) override;

	/*
	 * Samplers
	 */
	Sampler::Pointer createSampler(const Sampler::Description&) override;

	/*
	 * Programs
	 */
	Program::Pointer createProgram(uint32_t stages, const std::string& source) override;

	/*
	 * Pipeline state
	 */
	PipelineState::Pointer acquireGraphicsPipeline(const RenderPass::Pointer&, const Material::Pointer&, const VertexStream::Pointer&) override;

	/*
	 * Compute
	 */
	Compute::Pointer createCompute(const Material::Pointer&) override;

private:
	ET_DECLARE_PIMPL(VulkanRenderer, 4096);
};
}
