/*
*   RT64VK
*/

#pragma once

#include "rt64_common.h"

#include "rt64_view.h"
#include "rt64_scene.h"
#include "rt64_mesh.h"
#include "rt64_texture.h"
#include "rt64_shader.h"
#include "rt64_inspector.h"
#include "rt64_mipmaps.h"

#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <optional>
#include <vulkan/vulkan.h>
#include <array>
#include <nvvk/context_vk.hpp>
#include <nvvk/raytraceKHR_vk.hpp>
#include <nvvk/resourceallocator_vk.hpp>
#include <unordered_map>
#include <imgui/backends/imgui_impl_vulkan.h>

#define MAX_FRAMES_IN_FLIGHT    2

#define PS_ENTRY    "PSMain"
#define VS_ENTRY    "VSMain"
#define GS_ENTRY    "GSMain"
#define CS_ENTRY    "mainCS"

// The windows
#ifdef _WIN32
#define RT64_WINDOW  HWND
#else
#include <GLFW/glfw3.h>
#include <imgui/backends/imgui_impl_glfw.h>
#define RT64_WINDOW GLFWwindow*
#endif

namespace RT64
{
	class Scene;
	class Shader;
	class Mesh;
	class Texture;
	class Inspector;
	class Mipmaps;

    struct IndexedQueue {
        int familyIndex;
        VkQueue queue;
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct RaygenPushConstant {
        float giBounceDivisor;
        float giResolutionScale;
        unsigned int giBounce;
    };
    
    class Device
    {
        private:
            nvvk::Context vkctx {};
            VkPhysicalDevice& physicalDevice = vkctx.m_physicalDevice;
            VkInstance& vkInstance = vkctx.m_instance;
            VkDevice& vkDevice = vkctx.m_device;
            IndexedQueue graphicsQueue;
            IndexedQueue computeQueue;
            IndexedQueue presentQueue;
            VkSwapchainKHR swapChain;
            std::vector<VkImage> swapChainImages;
            VkFormat swapChainImageFormat;
            VkExtent2D swapChainExtent;
            std::vector<VkImageView> swapChainImageViews;
            Mipmaps* mipmaps = nullptr;
            bool disableMipmaps = false;
            bool vsyncEnabled = true;

            inline void createVkInstanceNV();
            SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
            VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
            VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
            VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

            static void framebufferResizeCallback(RT64_WINDOW window, int width, int height);

#ifndef RT64_MINIMAL
            void createDxcCompiler();
            void createMemoryAllocator();
            void createSurface();
            void cleanupSwapChain();
            void createSwapChain();
            void createImageViews();
            void createCommandPool();
            void createCommandBuffers();
            void createFramebuffers();
            void createSyncObjects();
            void createRayTracingPipeline();
            void createDescriptorPool();
            void preparePipelines();

            void initRayTracing();
            void recreateSwapChain();
            bool updateSize(VkResult result, bool vsync, const char* error);
            void updateViewport();
            void updateScenes();
            void resizeScenes();
            void generateRTDescriptorSetLayout();
            void loadBlueNoise();
            void generateSamplers();

            RT64_WINDOW window;
            VkSurfaceKHR vkSurface;
            int width;
            int height;
            float anisotropy = 1.0f;
            float aspectRatio;
            bool framebufferCreated = false;
            bool framebufferResized = false;

            std::vector<Scene*> scenes;
            std::unordered_set<Shader*> shaders;
            std::vector<Mesh*> meshes;
            std::vector<Texture*> textures;
            std::vector<Inspector*> oldInspectors;
            std::unordered_map<unsigned int, VkSampler> samplers;

            Inspector inspector;
            bool showInspector = false;
            
		    Texture* blueNoise;
            VkSampler gaussianSampler;
            VkSampler composeSampler;
            VkSampler tonemappingSampler;
            VkSampler postProcessSampler;

            VkPhysicalDeviceProperties physDeviceProperties {};   // Properties of the physical device
            VmaAllocator allocator;
            VkRenderPass presentRenderPass;         // The render pass for presenting to the screen
            VkRenderPass offscreenRenderPass;       // The render pass for rendering to an r32g32b32a32 image
            std::vector<VkFramebuffer> swapChainFramebuffers;

            VkCommandPool commandPool;
            VkCommandPool computeCommandPool;
            std::vector<VkCommandBuffer> commandBuffers;
            bool commandBufferActive = false;
            bool presentPassActive = false;
            bool offscreenPassActive = false;

            std::vector<VkSemaphore> imageAvailableSemaphores;
            std::vector<VkSemaphore> renderFinishedSemaphores;
            std::vector<VkFence> inFlightFences;
            std::vector<VkImageView*> depthViews;
            std::array<bool, MAX_FRAMES_IN_FLIGHT> fencesUp;

            bool rtStateDirty = false;
            bool descPoolDirty = false;
            bool recreateSamplers = false;
            VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
            nvvk::RaytracingBuilderKHR rtBlasBuilder;
            nvvk::ResourceAllocatorDma rtAllocator;

            std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
            std::vector<VkDescriptorPoolSize> gaussianDescriptorPoolSizes;
            VkDescriptorPool descriptorPool;

            uint32_t currentFrame = 0;
            uint32_t framebufferIndex = 0;
            uint32_t shaderGroupCount = 0;
            uint32_t rasterGroupCount = 0;
            uint32_t hitGroupCount = 0;

            VkViewport vkViewport;
            VkRect2D vkScissorRect;

            IDxcCompiler* d3dDxcCompiler;   // Who invited my man blud XDXDXD
            IDxcLibrary* d3dDxcLibrary;     // Bro thinks he's on the team  XDXDXDXDXDXD

            //***********************************************************
            // The Shaders
            VkShaderModule primaryRayGenModule;
            VkShaderModule directRayGenModule;
            VkShaderModule indirectRayGenModule;
            VkShaderModule reflectionRayGenModule;
            VkShaderModule refractionRayGenModule;
            VkShaderModule surfaceMissModule;
            VkShaderModule shadowMissModule;
            VkShaderModule fullscreenVSModule;
            VkShaderModule composePSModule;
            VkShaderModule postProcessPSModule;
            VkShaderModule tonemappingPSModule;
            VkShaderModule debugPSModule;
            VkShaderModule im3dVSModule;
            VkShaderModule im3dPSModule;
            VkShaderModule im3dGSPointsModule;
            VkShaderModule im3dGSLinesModule;
            VkShaderModule gaussianFilterRGB3x3CSModule;
            // And their shader stage infos
            VkPipelineShaderStageCreateInfo primaryRayGenStage          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo directRayGenStage           {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo indirectRayGenStage         {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo reflectionRayGenStage       {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo refractionRayGenStage       {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo surfaceMissStage            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo shadowMissStage             {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo fullscreenVSStage           {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo composePSStage              {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo postProcessPSStage          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo tonemappingPSStage          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo debugPSStage                {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo im3dVSStage                 {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo im3dPSStage                 {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo im3dGSPointsStage           {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo im3dGSLinesStage            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            VkPipelineShaderStageCreateInfo gaussianFilterRGB3x3CSStage {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            // And pipelines
            VkPipelineLayout        rtPipelineLayout;
            VkPipeline              rtPipeline;
            VkPipelineLayout        composePipelineLayout;
            VkPipeline              composePipeline;
            VkPipelineLayout        tonemappingPipelineLayout;
            VkPipeline              tonemappingPipeline;
            VkPipelineLayout        postProcessPipelineLayout;
            VkPipeline              postProcessPipeline;
            VkPipelineLayout        debugPipelineLayout;
            VkPipeline              debugPipeline;
            VkPipelineLayout        im3dPipelineLayout;
            VkPipeline              im3dPipeline;
            VkPipelineLayout        im3dPointsPipelineLayout;
            VkPipeline              im3dPointsPipeline;
            VkPipelineLayout        im3dLinesPipelineLayout;
            VkPipeline              im3dLinesPipeline;
            VkPipelineLayout        gaussianFilterRGB3x3PipelineLayout;
            VkPipeline              gaussianFilterRGB3x3Pipeline;
            // Did I mention the descriptors?
            VkDescriptorSetLayout   rtDescriptorSetLayout;
            VkDescriptorSet         rtDescriptorSet;
            VkDescriptorSetLayout   composeDescriptorSetLayout;
            VkDescriptorSet         composeDescriptorSet;
            VkDescriptorSetLayout   tonemappingDescriptorSetLayout;
            VkDescriptorSet         tonemappingDescriptorSet;
            VkDescriptorSetLayout   postProcessDescriptorSetLayout;
            VkDescriptorSet         postProcessDescriptorSet;
            VkDescriptorSetLayout   debugDescriptorSetLayout;
            VkDescriptorSet         debugDescriptorSet;
            VkDescriptorSetLayout   im3dDescriptorSetLayout;
            VkDescriptorSet         im3dDescriptorSet;
            VkDescriptorSetLayout   gaussianFilterRGB3x3DescriptorSetLayout;
#endif

            const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
            const std::vector<const char *> deviceExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                // The things that allow for raytracing
                VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                VK_KHR_RAY_QUERY_EXTENSION_NAME,
                VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
            };

        public:
#ifdef RT64_DEBUG
            const bool enableValidationLayers = false;
#else
            const bool enableValidationLayers = true;
#endif

            Device(RT64_WINDOW window);
		    virtual ~Device();

#ifndef RT64_MINIMAL

            /********************** Getters **********************/
            RT64_WINDOW getWindow();
		    VkInstance& getVkInstance();
		    VkDevice& getVkDevice();
		    VkPhysicalDevice& getPhysicalDevice();
		    nvvk::ResourceAllocator& getRTAllocator();
		    VmaAllocator& getMemAllocator();
		    VkExtent2D& getSwapchainExtent();
            VkRenderPass& getPresentRenderPass();
            VkRenderPass& getOffscreenRenderPass();
		    VkViewport& getViewport();
		    VkRect2D& getScissors();
		    int getWidth();
		    int getHeight();
		    double getAspectRatio();
            int getCurrentFrameIndex();
		    VkCommandBuffer& getCurrentCommandBuffer();
            VkDescriptorPool& getDescriptorPool();
		    VkFramebuffer& getCurrentSwapchainFramebuffer();
            IDxcCompiler* getDxcCompiler();
            IDxcLibrary* getDxcLibrary();
            VkPhysicalDeviceRayTracingPipelinePropertiesKHR getRTProperties() const;
            VkPipeline& getRTPipeline();
            VkPipelineLayout& getRTPipelineLayout();
            VkDescriptorSet& getRTDescriptorSet();
            VkDescriptorSetLayout& getRTDescriptorSetLayout();
            Texture* getBlueNoise() const;
            uint32_t getHitGroupCount() const;
            uint32_t getRasterGroupCount() const;
            VkFence& getCurrentFence();
            Inspector& getInspector();
            Mipmaps* getMipmaps();
            float getAnisotropyLevel();
            void setAnisotropyLevel(float level);
            VkPhysicalDeviceProperties getPhysicalDeviceProperties();
            // Samplers
            VkSampler& getGaussianSampler();
            VkSampler& getComposeSampler();
            VkSampler& getTonemappingSampler();
            VkSampler& getPostProcessSampler();
            // Shader getters
            VkPipelineShaderStageCreateInfo getPrimaryShaderStage() const;
            VkPipelineShaderStageCreateInfo getDirectShaderStage() const;
            VkPipelineShaderStageCreateInfo getIndirectShaderStage() const;
            VkPipelineShaderStageCreateInfo getReflectionShaderStage() const;
            VkPipelineShaderStageCreateInfo getRefractionShaderStage() const;
            // Rasterization pipelines and descriptor sets
            VkPipeline&                 getComposePipeline();
            VkPipelineLayout&           getComposePipelineLayout();
            VkDescriptorSet&            getComposeDescriptorSet();
            VkPipeline&                 getTonemappingPipeline();
            VkPipelineLayout&           getTonemappingPipelineLayout();
            VkDescriptorSet&            getTonemappingDescriptorSet();
            VkPipeline&                 getPostProcessPipeline();
            VkPipelineLayout&           getPostProcessPipelineLayout();
            VkDescriptorSet&            getPostProcessDescriptorSet();
            VkPipeline&                 getDebugPipeline();
            VkPipelineLayout&           getDebugPipelineLayout();
            VkDescriptorSet&            getDebugDescriptorSet();
            VkPipeline&                 getIm3dPipeline();
            VkPipelineLayout&           getIm3dPipelineLayout();
            VkPipeline&                 getIm3dPointsPipeline();
            VkPipelineLayout&           getIm3dPointsPipelineLayout();
            VkPipeline&                 getIm3dLinesPipeline();
            VkPipelineLayout&           getIm3dLinesPipelineLayout();
            VkDescriptorSet&            getIm3dDescriptorSet();
            VkPipeline&                 getGaussianFilterRGB3x3Pipeline();
            VkPipelineLayout&           getGaussianFilterRGB3x3PipelineLayout();
            VkDescriptorSetLayout&      getGaussianFilterRGB3x3DescriptorSetLayout();
            std::vector<VkDescriptorPoolSize>& getGaussianDescriptorPoolSizes();

            VkCommandBuffer* beginSingleTimeCommands();
            VkCommandBuffer* beginSingleTimeCommands(VkCommandBuffer* commandBuffer);
            void endSingleTimeCommands(VkCommandBuffer* commandBuffer);
            VkCommandBuffer& beginCommandBuffer();
            void endCommandBuffer();
            void beginPresentRenderPass(VkRenderPassBeginInfo& info);
            void endPresentRenderPass();
            void beginOffscreenRenderPass(VkRenderPassBeginInfo& info);
            void endOffscreenRenderPass();

		    void createRenderPass(VkRenderPass& renderPass, bool useDepth, VkFormat imageFormat, VkImageLayout finalLayout);
            VkResult allocateBuffer(VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memUsage, VmaAllocationCreateFlags allocProperties, AllocatedBuffer* alre);
            VkResult allocateImage(AllocatedImage* alre, VkImageCreateInfo createInfo, VmaMemoryUsage memUsage, VmaAllocationCreateFlags allocProperties);
            VkResult allocateImage(uint32_t width, uint32_t height, VkImageType imageType, VkFormat imageFormat, VkImageTiling imageTiling, VkImageLayout initLayout, VkImageUsageFlags imageUsage, VmaMemoryUsage memUsage, VmaAllocationCreateFlags allocProperties, AllocatedImage* alre);
            void copyBuffer(VkBuffer src, VkBuffer dest, VkDeviceSize size, VkCommandBuffer* commandBuffer);
            void copyImage(AllocatedImage& src, AllocatedImage& dest, VkExtent3D dimensions, VkImageAspectFlags srcFlags, VkImageAspectFlags destFlags, VkCommandBuffer* commandBuffer);
            void copyImageToBuffer(VkImage image, VkBuffer buffer, uint32_t width, uint32_t height, VkCommandBuffer* commandBuffer);
            void matchLayoutToAccessMask(VkImageLayout inLayout, VkAccessFlags& outMask);
            void memoryBarrier(VkAccessFlags oldMask, VkAccessFlags newMask, VkPipelineStageFlags oldStage, VkPipelineStageFlags newStage, VkCommandBuffer* commandBuffer);
            void bufferMemoryBarrier(AllocatedBuffer& buffer, VkAccessFlags newMask, VkPipelineStageFlags newStage, VkCommandBuffer* commandBuffer);
            void transitionImageLayout(AllocatedImage& image, VkImageLayout newLayout, VkAccessFlags newMask, VkPipelineStageFlags newStage, VkCommandBuffer* commandBuffer);
            void transitionImageLayout(AllocatedImage** images, uint32_t imageCount, VkImageLayout newLayout, VkAccessFlags newMask, VkPipelineStageFlags oldStage, VkPipelineStageFlags newStage, VkCommandBuffer* commandBuffer);
            void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkCommandBuffer* commandBuffer);
            VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
            VkBufferView createBufferView(VkBuffer& buffer, VkFormat format, VkBufferViewCreateFlags flags, VkDeviceSize size);
            void draw(int vsyncInterval, double delta);
            void setInspectorVisibility(bool v);
		    void addScene(Scene* scene);
		    void removeScene(Scene* scene);
		    void addMesh(Mesh* mesh);
		    void removeMesh(Mesh* mesh);
		    void addTexture(Texture* texture);
		    void removeTexture(Texture* texture);
		    void addInspectorOld(Inspector* inspect);
		    void removeInspectorOld(Inspector* inspect);
            void addShader(Shader* shader);
            void removeShader(Shader* shader);
            std::unordered_map<unsigned int, VkSampler>& getSamplerMap();
            VkSampler& getSampler(unsigned int index);
            ImGui_ImplVulkan_InitInfo generateImguiInitInfo();
            void addDepthImageView(VkImageView* depthImageView);
            void dirtyDescriptorPool();
            void removeDepthImageView(VkImageView* depthImageView);
            void createShaderModule(const void* code, size_t size, const char* entryName, VkShaderStageFlagBits stage, VkPipelineShaderStageCreateInfo& shaderStageInfo, VkShaderModule& shader, std::vector<VkPipelineShaderStageCreateInfo>* shaderStages);
            void initRTBuilder(nvvk::RaytracingBuilderKHR& rtBuilder);
            void generateDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorBindingFlags& flags, VkDescriptorSetLayout& descriptorSetLayout, std::vector<VkDescriptorPoolSize>& poolSizes);
            void addToDescriptorPool(std::vector<VkDescriptorSetLayoutBinding>& bindings);
            void allocateDescriptorSet(VkDescriptorSetLayout& descriptorSetLayout, VkDescriptorSet& descriptorSet, VkDescriptorPool& descriptorPool);
            void createFramebuffer(VkFramebuffer& framebuffer, VkRenderPass& renderPass, VkImageView& imageView, VkImageView* depthView, VkExtent2D extent);
            void waitForGPU();

            // More stuff for window resizing
            bool wasWindowResized() { return framebufferResized; }
            void resetWindowResized() { framebufferResized = false; }
#endif
    };
}