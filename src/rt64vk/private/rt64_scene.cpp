/*
 *   RT64VK
 */

#ifndef RT64_MINIMAL

#include "../public/rt64.h"

#include <map>
#include <random>
#include <set>

#include "rt64_scene.h"

#include "rt64_device.h"
#include "rt64_instance.h"
#include "rt64_view.h"

namespace RT64
{
    Scene::Scene(Device* device) {
        assert(device != nullptr);
        this->device = device;

        description.eyeLightDiffuseColor = { 0.08f, 0.08f, 0.08f };
        description.eyeLightSpecularColor = { 0.04f, 0.04f, 0.04f };
        description.skyDiffuseMultiplier = { 1.0f, 1.0f, 1.0f };
        description.skyHSLModifier = { 0.0f, 0.0f, 0.0f };
        description.skyYawOffset = 0.0f;
        description.giDiffuseStrength = 0.7f;
        description.giSkyStrength = 0.35f;
        lightsBufferSize = 0;
        lightsCount = 0;

        device->addScene(this);
    }

    Scene::~Scene() {
        device->removeScene(this);

        lightsBuffer.destroyResource();

        auto viewsCopy = views;
        for (View *view : viewsCopy) {
            delete view;
        }

        auto instancesCopy = instances;
        for (Instance *instance : instancesCopy) {
            delete instance;
        }
    }

    void Scene::update() {
        RT64_LOG_PRINTF("Started scene update");

        for (View *view : views) {
            view->update();
        }

        RT64_LOG_PRINTF("Finished scene update");
    }

    void Scene::render(float deltaTimeMs) {
        RT64_LOG_PRINTF("Started scene render");

        for (View *view : views) {
            view->render(deltaTimeMs);
        }

        RT64_LOG_PRINTF("Finished scene render");
    }

    void Scene::resize() {
        for (View *view : views) {
            view->resize();
        }
    }

    void Scene::setDescription(RT64_SCENE_DESC v) {
        description = v;
    }

    RT64_SCENE_DESC Scene::getDescription() const {
        return description;
    }

    void Scene::addInstance(Instance* instance) {
        assert(instance != nullptr);
        instances.push_back(instance);
    }

    void Scene::removeInstance(Instance* instance) {
        assert(instance != nullptr);

        auto it = std::find(instances.begin(), instances.end(), instance);
        if (it != instances.end()) {
            instances.erase(it);
        }
    }

    void Scene::addView(View* view) {
        views.push_back(view);
    }

    void Scene::removeView(View* view) {
        // views.erase(std::remove(views.begin(), views.end(), view), views.end());
    }

    const std::vector<RT64::View*>& RT64::Scene::getViews() const {
        return views;
    }

    void Scene::setLights(RT64_LIGHT* lightArray, int lightCount) {
        static std::default_random_engine randomEngine;
        static std::uniform_real_distribution<float> randomDistribution(0.0f, 1.0f);

        assert(lightCount > 0);
        VkDeviceSize newSize = sizeof(Light) * lightCount;
        if (newSize != lightsBufferSize) {
            lightsBuffer.destroyResource();
            getDevice()->allocateBuffer(
                newSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_HOST, 
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 
                &lightsBuffer
                );
            lightsBufferSize = newSize;
        }

        uint64_t* pData;
        void* lightBufferAddr = lightsBuffer.mapMemory((void**)&pData);
        if (lightArray != nullptr) {
            // Convert the RT64_LIGHT array to a vector of Light structs.
            //  For compatibility with Vulkan and RT64DX
            std::vector<Light> vkLights;
            vkLights.resize(lightCount);
            for (VkDeviceSize i = 0; i < lightCount; i++) {
                RT64_LIGHT& rt64Light = lightArray[i];
                Light& lightStruct = vkLights[i];
                lightStruct.position = rt64Light.position;
                lightStruct.diffuseColor = rt64Light.diffuseColor;
                lightStruct.specularColor = rt64Light.specularColor;
                lightStruct.attenuationRadius = rt64Light.attenuationRadius;
                lightStruct.pointRadius = rt64Light.pointRadius;
                lightStruct.shadowOffset = rt64Light.shadowOffset;
                lightStruct.attenuationExponent = rt64Light.attenuationExponent;
                lightStruct.flickerIntensity = rt64Light.flickerIntensity;
                lightStruct.groupBits = rt64Light.groupBits;

                // Modify light colors with flicker intensity if necessary.
                const float flickerIntensity = lightStruct.flickerIntensity;
                if (flickerIntensity > 0.0) {
                    const float flickerMult = 1.0f + ((randomDistribution(randomEngine) * 2.0f - 1.0f) * flickerIntensity);
                    lightStruct.diffuseColor.x *= flickerMult;
                    lightStruct.diffuseColor.y *= flickerMult;
                    lightStruct.diffuseColor.z *= flickerMult;
                }
            }
            
            memcpy(pData, vkLights.data(), sizeof(Light) * lightCount);
        }

        lightsBuffer.unmapMemory();
        lightsCount = lightCount;
    }

    AllocatedBuffer& Scene::getLightsBuffer() {
        return lightsBuffer;
    }

    int Scene::getLightsCount() const {
        return lightsCount;
    }

    const std::vector<Instance*>& Scene::getInstances() const {
        return instances;
    }

    Device* Scene::getDevice() const {
        return device;
    }
};

// Library exports

DLEXPORT RT64_SCENE* RT64_CreateScene(RT64_DEVICE*devicePtr) {
    RT64::Device* device = (RT64::Device*)(devicePtr);
    return (RT64_SCENE*)(new RT64::Scene(device));
}

DLEXPORT void RT64_SetSceneDescription(RT64_SCENE* scenePtr, RT64_SCENE_DESC sceneDesc) {
    RT64::Scene* scene = (RT64::Scene*)(scenePtr);
    scene->setDescription(sceneDesc);
}

DLEXPORT void RT64_SetSceneLights(RT64_SCENE* scenePtr, RT64_LIGHT* lightArray, int lightCount) {
    RT64::Scene* scene = (RT64::Scene*)(scenePtr);
    scene->setLights(lightArray, lightCount);
}

DLEXPORT void RT64_DestroyScene(RT64_SCENE* scenePtr) {
    delete (RT64::Scene*)(scenePtr);
}
#endif