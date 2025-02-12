/*
*   RT64VK
*/

#ifndef RT64_MINIMAL

#include "../public/rt64.h"
#include "rt64_mesh.h"

// Private

namespace RT64 
{
    Mesh::Mesh(Device *device, int flags) {
        assert(device != nullptr);
        this->device = device;
        this->flags = flags;
        vertexCount = 0;
        indexCount = 0;
        vertexStride = 0;
        blasAddress = (VkDeviceAddress)nullptr;
        this->device->initRTBuilder(builder);

		device->addMesh(this);
    }

    Mesh::~Mesh() {
        device->removeMesh(this);

        vertexBuffer.destroyResource();
        stagingVertexBuffer.destroyResource();
        indexBuffer.destroyResource();
        stagingIndexBuffer.destroyResource();
        builder.destroy();
    }

    // This function copies the passed in vertex array into the buffer
    void Mesh::updateVertexBuffer(void *vertices, int vertexCount, int vertexStride) {
        const VkDeviceSize vertexBufferSize = vertexCount * vertexStride;

        // Delete if the vertex buffers are out of date
        if (!vertexBuffer.isNull() && ((this->vertexCount != vertexCount) || (this->vertexStride != vertexStride))) {
            vertexBuffer.destroyResource();
            stagingVertexBuffer.destroyResource();
            // Discard the BLAS since it won't be compatible anymore even if it's updatable.
            builderActive = false;
            builder.destroy();
        }

        if (vertexBuffer.isNull()) {
            device->allocateBuffer(vertexBufferSize, 
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_HOST, 
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 
                &stagingVertexBuffer);
            device->allocateBuffer(vertexBufferSize, 
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, 
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, 
                &vertexBuffer);
        }

        // Get the data into the staging buffer's memory
        stagingVertexBuffer.setData(vertices, vertexBufferSize);
        
        // Copy staging buffer to main buffer
        device->copyBuffer(stagingVertexBuffer.getBuffer(), vertexBuffer.getBuffer(), vertexBufferSize, nullptr);

        this->vertexCount = vertexCount;
        this->vertexStride = vertexStride;
    }

    // Updates the index buffer with the passed in index array
    // Similar to updateVertexBuffer() but for indices.
    void Mesh::updateIndexBuffer(unsigned int *indices, int indexCount) {
        const VkDeviceSize indexBufferSize = indexCount * sizeof(unsigned int);

        // Delete if the index buffers are out of date
        if (!indexBuffer.isNull() && ((this->indexCount != indexCount))) {
            indexBuffer.destroyResource();
            stagingIndexBuffer.destroyResource();
            builder.destroy();
            builderActive = false;
            // Discard the BLAS since it won't be compatible anymore even if it's updatable.
            // blasBuffers.destroyResource();
        }

        if (indexBuffer.isNull()) {
            device->allocateBuffer(indexBufferSize, 
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_HOST, 
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT, 
                &stagingIndexBuffer);
            device->allocateBuffer(indexBufferSize, 
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, 
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT, 
                &indexBuffer);
        }

        // Get the data into the staging buffer's memory
        stagingIndexBuffer.setData(indices, indexBufferSize);
        
        // Copy staging buffer to main buffer
        device->copyBuffer(stagingIndexBuffer.getBuffer(), indexBuffer.getBuffer(), indexBufferSize, nullptr);
        
        this->indexCount = indexCount;
    }

    // vVertexBuffers is a tuple, with the 1st thing being a vertex 
    //  buffer and the second thing being the number of verticies
    // vIndexBuffers is another tuple, a pairing of an index buffer
    //  (a buffer of pointers into the vertex buffer) and the number of
    //  indicies.
    void Mesh::createBottomLevelAS(std::pair<VkBuffer&, uint32_t> vVertexBuffers, std::pair<VkBuffer&, uint32_t> vIndexBuffers) {
        bool updatable = flags & RT64_MESH_RAYTRACE_UPDATABLE;
        bool fastTrace = flags & RT64_MESH_RAYTRACE_FAST_TRACE;
        bool compact = flags & RT64_MESH_RAYTRACE_COMPACT;
        
        // BLAS - Storing each primitive in a geometry
        nvvk::RaytracingBuilderKHR::BlasInput blasInput;
        modelIntoVkGeo(vVertexBuffers.first, vVertexBuffers.second, vIndexBuffers.first, vIndexBuffers.second, blasInput);
        if (updatable && builderActive) {
            builder.updateBlas(0, blasInput, (flags & (RT64_MESH_RAYTRACE_UPDATABLE | RT64_MESH_RAYTRACE_FAST_TRACE  | RT64_MESH_RAYTRACE_COMPACT)) >> 1);
        } else {
            if (builderActive) {
                builder.destroy();
                builderActive = false;
            }
            std::vector<nvvk::RaytracingBuilderKHR::BlasInput> blas;
            blas.reserve(1);
            blas.emplace_back(blasInput);
            builder.buildBlas(blas, (flags & (RT64_MESH_RAYTRACE_UPDATABLE | RT64_MESH_RAYTRACE_FAST_TRACE | RT64_MESH_RAYTRACE_COMPACT)) >> 1);
            builderActive = true;
            blasAddress = builder.getBlasDeviceAddress(0);
        }
    }

    //--------------------------------------------------------------------------------------------------
    // Convert the mesh into the ray tracing geometry used to build the BLAS
    //  From nvpro-samples/vk_raytracing_tutorial_KHR
    //
    void Mesh::modelIntoVkGeo(VkBuffer& vertexBuffer, uint32_t vertexCount, VkBuffer& indexBuffer, uint32_t indexCount, nvvk::RaytracingBuilderKHR::BlasInput& input)
    {
        // BLAS builder requires raw device addresses.
        VkDeviceAddress vertexAddress = nvvk::getBufferDeviceAddress(device->getVkDevice(), vertexBuffer);
        VkDeviceAddress indexAddress  = nvvk::getBufferDeviceAddress(device->getVkDevice(), indexBuffer);

        uint32_t maxPrimitiveCount = indexCount / 3;

        // Describe buffer as array of VertexObj.
        VkAccelerationStructureGeometryTrianglesDataKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
        triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
        triangles.vertexData.deviceAddress = vertexAddress;
        triangles.vertexStride             = vertexStride;
        // Describe index data (32-bit unsigned int)
        triangles.indexType               = VK_INDEX_TYPE_UINT32;
        triangles.indexData.deviceAddress = indexAddress;
        // Indicate identity transform by setting transformData to null device pointer.
        //triangles.transformData = {};
        triangles.maxVertex = vertexCount;

        // Identify the above data as containing opaque triangles.
        VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        asGeom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        asGeom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
        asGeom.geometry.triangles = triangles;

        // The entire array will be used to build the BLAS.
        VkAccelerationStructureBuildRangeInfoKHR offset;
        offset.firstVertex     = 0;
        offset.primitiveCount  = maxPrimitiveCount;
        offset.primitiveOffset = 0;
        offset.transformOffset = 0;

        // Our blas is made from only one geometry, but could be made of many geometries
        input.asGeometry.emplace_back(asGeom);
        input.asBuildOffsetInfo.emplace_back(offset);
    }

    // Public 

    AllocatedBuffer& Mesh::getVertexBuffer() { return vertexBuffer; }
    AllocatedBuffer& Mesh::getIndexBuffer() { return indexBuffer; }
    int Mesh::getIndexCount() const { return indexCount; }
    int Mesh::getVertexCount() const { return vertexCount; }
    nvvk::AccelKHR& Mesh::getBlas() { return builder.getFirstBlas(); }
    bool Mesh::blasBuilderActive() const { return builderActive; }
    VkDeviceAddress Mesh::getBlasAddress() const { return blasAddress; }

    void Mesh::updateBottomLevelAS() {
        if (flags & RT64_MESH_RAYTRACE_ENABLED) {
            // Create and store the bottom level AS buffers.
            createBottomLevelAS({ getVertexBuffer().getBuffer(), getVertexCount() }, { getIndexBuffer().getBuffer(), getIndexCount() });
        }
    }

};

// Library Exports

DLEXPORT RT64_MESH *RT64_CreateMesh(RT64_DEVICE *devicePtr, int flags) {
	RT64::Device *device = (RT64::Device *)(devicePtr);
	return (RT64_MESH *)(new RT64::Mesh(device, flags));
}

DLEXPORT void RT64_SetMesh(RT64_MESH* meshPtr, void* vertexArray, int vertexCount, int vertexStride, unsigned int* indexArray, int indexCount) {
	assert(meshPtr != nullptr);
	assert(vertexArray != nullptr);
	assert(vertexCount > 0);
	assert(indexArray != nullptr);
	assert(indexCount > 0);
	RT64::Mesh* mesh = (RT64::Mesh*)(meshPtr);
	mesh->updateVertexBuffer(vertexArray, vertexCount, vertexStride);
	mesh->updateIndexBuffer(indexArray, indexCount);
	mesh->updateBottomLevelAS();
}

DLEXPORT void RT64_DestroyMesh(RT64_MESH * meshPtr) {
	delete (RT64::Mesh *)(meshPtr);
}
#endif