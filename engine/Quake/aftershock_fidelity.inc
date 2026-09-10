/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Josh Nicholls
 * Render-resource implementation included by gl_vidsdl.c. All images belong to
 * the render-resource lifetime; frame descriptors are immutable while in flight.
 */
#include "../Shaders/Compiled/fidelity_single.c"
#include "../Shaders/Compiled/fidelity_msaa.c"

static VkImage					as_history[2];
static VkImageView				as_history_view[2], as_depth_view;
static vulkan_memory_t			as_history_memory[2];
static VkSampler				as_fidelity_sampler;
static vulkan_desc_set_layout_t as_fidelity_set_layout;
static VkDescriptorSet			as_fidelity_sets[2];
static vulkan_pipeline_t		as_fidelity_pipeline;
static int						as_history_index;
static qboolean					as_history_initialized;

typedef struct
{
	float inverse_vp[16], previous_vp[16], vp[16];
	float previous_camera[4], camera[4], viewport[4], previous_viewport[4], screen[4], blend[4], settings[4], nail_ripple[4];
    float optics[4];
} as_fidelity_uniform_t;
_Static_assert (sizeof (as_fidelity_uniform_t) == 336, "Fidelity std140 contract");

static void AS_FidelityCheck (VkResult result)
{
	if (result != VK_SUCCESS)
		Sys_Error ("Aftershock fidelity Vulkan error %d", result);
}

static void AS_FidelityCreate (void)
{
	const VkSamplerCreateInfo sampler = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.maxLod = 0};
	AS_FidelityCheck (vkCreateSampler (vulkan_globals.device, &sampler, NULL, &as_fidelity_sampler));
	VkImageViewCreateInfo view = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = depth_buffer,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = vulkan_globals.depth_format,
		.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}};
	AS_FidelityCheck (vkCreateImageView (vulkan_globals.device, &view, NULL, &as_depth_view));
	for (int i = 0; i < 2; ++i)
	{
		const VkImageCreateInfo image = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_R16G16B16A16_SFLOAT,
			.extent = {vid.width, vid.height, 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};
		AS_FidelityCheck (vkCreateImage (vulkan_globals.device, &image, NULL, &as_history[i]));
		VkMemoryRequirements requirements;
		vkGetImageMemoryRequirements (vulkan_globals.device, as_history[i], &requirements);
		VkMemoryAllocateInfo allocation = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = requirements.size,
			.memoryTypeIndex = GL_MemoryTypeFromProperties (requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0)};
		R_AllocateVulkanMemory (&as_history_memory[i], &allocation, VULKAN_MEMORY_TYPE_DEVICE, &num_vulkan_misc_allocations);
		AS_FidelityCheck (vkBindImageMemory (vulkan_globals.device, as_history[i], as_history_memory[i].handle, 0));
		view.image = as_history[i];
		view.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		AS_FidelityCheck (vkCreateImageView (vulkan_globals.device, &view, NULL, &as_history_view[i]));
		GL_SetObjectName ((uint64_t)as_history[i], VK_OBJECT_TYPE_IMAGE, "Aftershock HDR temporal history");
	}
	VkDescriptorSetLayoutBinding bindings[6] = {0};
	for (int i = 0; i < 6; ++i)
	{
		bindings[i].binding = i;
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		bindings[i].descriptorType = (i == 3 || i == 5) ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	}
	const VkDescriptorSetLayoutCreateInfo layout = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 6, .pBindings = bindings};
	as_fidelity_set_layout.num_combined_image_samplers = 4;
	as_fidelity_set_layout.num_storage_images = 2;
	AS_FidelityCheck (vkCreateDescriptorSetLayout (vulkan_globals.device, &layout, NULL, &as_fidelity_set_layout.handle));
	for (int i = 0; i < 2; ++i)
	{
		as_fidelity_sets[i] = R_AllocateDescriptorSet (&as_fidelity_set_layout);
		VkDescriptorImageInfo images[6] = {
			{as_fidelity_sampler, color_buffers_view[1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
			{vulkan_globals.point_sampler, as_depth_view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},
			{as_fidelity_sampler, as_history_view[1 - i], VK_IMAGE_LAYOUT_GENERAL},
			{VK_NULL_HANDLE, as_history_view[i], VK_IMAGE_LAYOUT_GENERAL},
			{as_fidelity_sampler, as_history_view[i], VK_IMAGE_LAYOUT_GENERAL},
			{VK_NULL_HANDLE, color_buffers_view[0], VK_IMAGE_LAYOUT_GENERAL}};
		VkWriteDescriptorSet writes[6] = {0};
		for (int j = 0; j < 6; ++j)
			writes[j] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
											   .dstSet = as_fidelity_sets[i],
											   .dstBinding = j,
											   .descriptorCount = 1,
											   .descriptorType = bindings[j].descriptorType,
											   .pImageInfo = &images[j]};
		vkUpdateDescriptorSets (vulkan_globals.device, 6, writes, 0, NULL);
	}
	VkDescriptorSetLayout			 layouts[2] = {as_fidelity_set_layout.handle, vulkan_globals.ubo_set_layout.handle};
	const VkPushConstantRange		 push = {VK_SHADER_STAGE_COMPUTE_BIT, 0, 8};
	const VkPipelineLayoutCreateInfo pipeline_layout = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 2,
		.pSetLayouts = layouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push};
	AS_FidelityCheck (vkCreatePipelineLayout (vulkan_globals.device, &pipeline_layout, NULL, &as_fidelity_pipeline.layout.handle));
	as_fidelity_pipeline.layout.push_constant_range = push;
	VkShaderModule				   module;
	const qboolean				   msaa = vulkan_globals.sample_count != VK_SAMPLE_COUNT_1_BIT;
	const VkShaderModuleCreateInfo shader = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = msaa ? sizeof (fidelity_msaa_spv) : sizeof (fidelity_single_spv),
		.pCode = (const uint32_t *)(msaa ? fidelity_msaa_spv : fidelity_single_spv)};
	AS_FidelityCheck (vkCreateShaderModule (vulkan_globals.device, &shader, NULL, &module));
	const VkComputePipelineCreateInfo pipeline = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module, .pName = "main"},
		.layout = as_fidelity_pipeline.layout.handle};
	AS_FidelityCheck (vkCreateComputePipelines (vulkan_globals.device, VK_NULL_HANDLE, 1, &pipeline, NULL, &as_fidelity_pipeline.handle));
	vkDestroyShaderModule (vulkan_globals.device, module, NULL);
	as_history_initialized = false;
	as_history_index = 0;
}

static void AS_FidelityDestroy (void)
{
	for (int i = 0; i < 2; ++i)
	{
		if (as_fidelity_sets[i])
			R_FreeDescriptorSet (as_fidelity_sets[i], &as_fidelity_set_layout);
		as_fidelity_sets[i] = VK_NULL_HANDLE;
		vkDestroyImageView (vulkan_globals.device, as_history_view[i], NULL);
		vkDestroyImage (vulkan_globals.device, as_history[i], NULL);
		if (as_history_memory[i].handle)
			R_FreeVulkanMemory (&as_history_memory[i], &num_vulkan_misc_allocations);
		as_history_view[i] = VK_NULL_HANDLE;
		as_history[i] = VK_NULL_HANDLE;
	}
	vkDestroyImageView (vulkan_globals.device, as_depth_view, NULL);
	as_depth_view = VK_NULL_HANDLE;
	vkDestroySampler (vulkan_globals.device, as_fidelity_sampler, NULL);
	as_fidelity_sampler = VK_NULL_HANDLE;
	vkDestroyPipeline (vulkan_globals.device, as_fidelity_pipeline.handle, NULL);
	vkDestroyPipelineLayout (vulkan_globals.device, as_fidelity_pipeline.layout.handle, NULL);
	vkDestroyDescriptorSetLayout (vulkan_globals.device, as_fidelity_set_layout.handle, NULL);
	memset (&as_fidelity_pipeline, 0, sizeof (as_fidelity_pipeline));
	memset (&as_fidelity_set_layout, 0, sizeof (as_fidelity_set_layout));
	as_history_initialized = false;
}

static qboolean AS_InvertMatrix (const float *source, float *inverse)
{
	double a[4][8];
	for (int y = 0; y < 4; ++y)
		for (int x = 0; x < 8; ++x)
			a[y][x] = x < 4 ? source[x * 4 + y] : (x - 4 == y);
	for (int x = 0; x < 4; ++x)
	{
		int pivot = x;
		for (int y = x + 1; y < 4; ++y)
			if (fabs (a[y][x]) > fabs (a[pivot][x]))
				pivot = y;
		if (fabs (a[pivot][x]) < 1e-12)
			return false;
		for (int k = 0; k < 8; ++k)
		{
			double tmp = a[x][k];
			a[x][k] = a[pivot][k];
			a[pivot][k] = tmp;
		}
		double scale = a[x][x];
		for (int k = 0; k < 8; ++k)
			a[x][k] /= scale;
		for (int y = 0; y < 4; ++y)
			if (y != x)
			{
				double f = a[y][x];
				for (int k = 0; k < 8; ++k)
					a[y][k] -= f * a[x][k];
			}
	}
	for (int y = 0; y < 4; ++y)
		for (int x = 0; x < 4; ++x)
			inverse[x * 4 + y] = (float)a[y][x + 4];
	return true;
}

void AS_FidelityPrepare (end_rendering_parms_t *parms)
{
	static as_fidelity_uniform_t previous;
	static double				 previous_time;
	static qmodel_t				*previous_map;
	static qboolean				 active;
	if (!parms->fidelity)
	{
		active = false;
		return;
	}
	VkBuffer			   buffer;
	as_fidelity_uniform_t *u = (as_fidelity_uniform_t *)R_UniformAllocate (sizeof (*u), &buffer, &parms->fidelity_offset, &parms->fidelity_ubo);
	memset (u, 0, sizeof (*u));
	memcpy (u->vp, vulkan_globals.view_projection_matrix, 64);
	if (!AS_InvertMatrix (u->vp, u->inverse_vp))
	{
		parms->fidelity = false;
		active = false;
		return;
	}
	memcpy (u->previous_vp, previous.vp, 64);
	memcpy (u->previous_camera, previous.camera, 16);
	memcpy (u->camera, parms->origin, 12);
	u->viewport[0] = r_refdef.vrect.x;
	u->viewport[1] = vid.height - glheight + r_refdef.vrect.y;
	u->viewport[2] = r_refdef.vrect.width;
	u->viewport[3] = r_refdef.vrect.height;
	memcpy (u->previous_viewport, previous.viewport, 16);
	u->screen[0] = parms->vid_width;
	u->screen[1] = parms->vid_height;
	u->screen[2] = q_max (1, parms->render_scale);
	u->screen[3] = parms->reflections;
	for (int i = 0; i < 4; ++i)
		u->blend[i] = parms->v_blend[i] / 255.f;
	if (!parms->polyblend)
		u->blend[3] = 0;
	vec3_t delta;
	VectorSubtract (parms->origin, previous.camera, delta);
	u->settings[0] = active && previous_map == cl.worldmodel && cl.time >= previous_time && cl.time - previous_time < .25 &&
					 DotProduct (delta, delta) < 64 * 64 && !memcmp (u->viewport, previous.viewport, 16) && u->screen[0] == previous.screen[0] &&
					 u->screen[1] == previous.screen[1] && u->screen[2] == previous.screen[2];
	u->settings[1] = parms->menu;
	u->settings[2] = parms->render_warp;
	u->settings[3] = parms->time;
    AS_NailRipple(u->nail_ripple);
    u->optics[0]=isfinite(as_reflection_strength.value)?CLAMP(0,as_reflection_strength.value,4):1;
    u->optics[1]=isfinite(as_reflection_roughness.value)?CLAMP(.03f,as_reflection_roughness.value,.7f):.2f;
    u->optics[2]=isfinite(as_neon_glow.value)?CLAMP(0,as_neon_glow.value,2):1;
    u->optics[3]=as_neon_prism.value!=0;
	previous = *u;
	previous_time = cl.time;
	previous_map = cl.worldmodel;
	active = true;
}

void AS_FidelityDraw (cb_context_t *cbx, end_rendering_parms_t *parms)
{
	VkImageMemoryBarrier barriers[3];
	for (int i = 0; i < 2; ++i)
		barriers[i] = (VkImageMemoryBarrier){.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
											 .srcAccessMask = as_history_initialized ? (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT) : 0,
											 .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
											 .oldLayout = as_history_initialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
											 .newLayout = VK_IMAGE_LAYOUT_GENERAL,
											 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
											 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
											 .image = as_history[i],
											 .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
	barriers[2] = (VkImageMemoryBarrier){.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
										 .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
										 .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
										 .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
										 .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
										 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
										 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
										 .image = depth_buffer,
										 .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1}};
	vkCmdPipelineBarrier (
		cbx->cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 3, barriers);
	if (!as_history_initialized)
	{
		const VkClearColorValue clear = {{0, 0, 0, 0}};
		for (int i = 0; i < 2; ++i)
			vkCmdClearColorImage (cbx->cb, as_history[i], VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &barriers[i].subresourceRange);
		const VkMemoryBarrier ready = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
		vkCmdPipelineBarrier (cbx->cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &ready, 0, NULL, 0, NULL);
	}
	R_BindPipeline (cbx, VK_PIPELINE_BIND_POINT_COMPUTE, as_fidelity_pipeline);
	const VkDescriptorSet sets[2] = {as_fidelity_sets[as_history_index], parms->fidelity_ubo};
	vkCmdBindDescriptorSets (cbx->cb, VK_PIPELINE_BIND_POINT_COMPUTE, as_fidelity_pipeline.layout.handle, 0, 2, sets, 1, &parms->fidelity_offset);
	uint32_t phase[2] = {0, as_history_initialized};
	vkCmdPushConstants (cbx->cb, as_fidelity_pipeline.layout.handle, VK_SHADER_STAGE_COMPUTE_BIT, 0, 8, phase);
	vkCmdDispatch (cbx->cb, (parms->vid_width + 7) / 8, (parms->vid_height + 7) / 8, 1);
	const VkMemoryBarrier ready = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT, .dstAccessMask = VK_ACCESS_SHADER_READ_BIT};
	vkCmdPipelineBarrier (cbx->cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &ready, 0, NULL, 0, NULL);
	phase[0] = 1;
	vkCmdPushConstants (cbx->cb, as_fidelity_pipeline.layout.handle, VK_SHADER_STAGE_COMPUTE_BIT, 0, 8, phase);
	vkCmdDispatch (cbx->cb, (parms->vid_width + 7) / 8, (parms->vid_height + 7) / 8, 1);
	as_history_initialized = true;
	as_history_index = 1 - as_history_index;
}
