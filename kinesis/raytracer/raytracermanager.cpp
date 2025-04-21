// --- kinesis/RayTracingManager.cpp ---
#include "RayTracingManager.h"
#include "GUI.h"
#include "mesh/mesh.h"
#include "gameobject.h"
#include <stdexcept>
#include <vector>
#include <array>
#include <iostream> // For std::cout

namespace Kinesis::RayTracingManager {

    // Define extern variables
    VkDescriptorSetLayout rtDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet rtDescriptorSet = VK_NULL_HANDLE;
    // Define other extern variables here...

    // --- Helper Functions (Internal to this translation unit) ---
    namespace { // Use an anonymous namespace for internal helpers

    void createDescriptorSetLayout() {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        //BIBI
        bindings.push_back({0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr});
        bindings.push_back({1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr});
        bindings.push_back({2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, nullptr});
        // Binding 3+: Vertex/Index/Material Storage Buffers (Add as needed)

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(g_Device, &layoutInfo, g_Allocator, &rtDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Ray Tracing descriptor set layout!");
        }
         std::cout << "Ray Tracing Descriptor Set Layout created." << std::endl;
    }

    void createPipelineLayout() {
        if (rtDescriptorSetLayout == VK_NULL_HANDLE) {
            throw std::runtime_error("RT Descriptor Set Layout must be created before pipeline layout!");
        }
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &rtDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(g_Device, &pipelineLayoutInfo, g_Allocator, &rtPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Ray Tracing pipeline layout!");
        }
        std::cout << "Ray Tracing Pipeline Layout created." << std::endl;
    }

    void allocateDescriptorSet() {
        if (rtDescriptorSetLayout == VK_NULL_HANDLE || g_DescriptorPool == VK_NULL_HANDLE) {
            throw std::runtime_error("Cannot allocate descriptor set without layout or pool!");
        }
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = g_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &rtDescriptorSetLayout;

        if (vkAllocateDescriptorSets(g_Device, &allocInfo, &rtDescriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate Ray Tracing descriptor set!");
        }
         std::cout << "Ray Tracing Descriptor Set allocated." << std::endl;
    }

    } // end anonymous namespace

    // --- Public Namespace Functions ---

    void initialize() {
        // Only proceed if ray tracing is actually available
        if (!Kinesis::GUI::raytracing_available) {
            std::cout << "Ray Tracing not available on this hardware. Skipping RT Manager initialization." << std::endl;
            return;
        }
        if(g_Device == VK_NULL_HANDLE) {
             std::cerr << "Warning: Vulkan device not ready during RayTracingManager::initialize(). Skipping." << std::endl;
             return;
        }


        std::cout << "Initializing Ray Tracing Manager..." << std::endl;
        try {
            createDescriptorSetLayout();
            createPipelineLayout();
            allocateDescriptorSet();
            // Initialize other RT resources here (Pipeline, SBT, etc.)
            // e.g., createRayTracingPipeline();
            // e.g., createShaderBindingTable();
        } catch (const std::exception& e) {
            std::cerr << "Ray Tracing Manager Initialization failed: " << e.what() << std::endl;
            // Clean up partially created resources before re-throwing or exiting
            cleanup(); // Attempt cleanup
            throw;     // Re-throw
        }
         std::cout << "Ray Tracing Manager Initialized Successfully." << std::endl;
    }

    void cleanup() {
        // Check if resources were potentially initialized (RT available and device exists)
        // No need to check raytracing_available here, as handles will be VK_NULL_HANDLE if initialize skipped.
        if(g_Device == VK_NULL_HANDLE) {
            // Nothing to clean if device doesn't exist
            return;
        }

        std::cout << "Cleaning up Ray Tracing Manager..." << std::endl;

        // Destroy resources in reverse order of creation
        // vkDestroyPipeline(g_Device, rtPipeline, g_Allocator); // If pipeline was created
        // vkDestroyBuffer(g_Device, sbtBuffer, g_Allocator); // If SBT buffer was created
        // vkFreeMemory(g_Device, sbtBufferMemory, g_Allocator); // If SBT memory was allocated

        if (rtPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(g_Device, rtPipelineLayout, g_Allocator);
            rtPipelineLayout = VK_NULL_HANDLE;
             std::cout << "  - RT Pipeline Layout destroyed." << std::endl;
        }
        if (rtDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(g_Device, rtDescriptorSetLayout, g_Allocator);
            rtDescriptorSetLayout = VK_NULL_HANDLE;
             std::cout << "  - RT Descriptor Set Layout destroyed." << std::endl;
        }
        // Descriptor sets are automatically freed when the pool is destroyed.
        // If you needed vkFreeDescriptorSets, call it here *before* destroying the layout/pool.
        rtDescriptorSet = VK_NULL_HANDLE; // Mark as cleaned

         std::cout << "Ray Tracing Manager Cleanup Finished." << std::endl;

    }

    // Call this AFTER tlas, outputImgView, camBuffer etc. are valid handles!
    void updateDescriptorSet(VkAccelerationStructureKHR tlas, VkImageView outputImgView, VkBuffer camBuffer, VkDeviceSize camBufSize /*, other resources...*/) {
        //BIBI
        // Only proceed if ray tracing was initialized
        if (rtDescriptorSet == VK_NULL_HANDLE) {
             if (Kinesis::GUI::raytracing_available) {
                 std::cerr << "Warning: Attempting to update RT descriptor set, but it was not allocated (check initialization)." << std::endl;
             }
            return; // Skip if not initialized (or RT not available)
        }

        // --- Check resource validity before updating ---
        if (tlas == VK_NULL_HANDLE || outputImgView == VK_NULL_HANDLE || camBuffer == VK_NULL_HANDLE) {
             std::cerr << "Warning: Skipping RT descriptor set update due to NULL resource handle." << std::endl;
            return;
        }

        //BIBI
        std::vector<VkWriteDescriptorSet> descriptorWrites;
        VkWriteDescriptorSetAccelerationStructureKHR tlasWriteInfo{};
        VkDescriptorImageInfo imageInfo{};
        VkDescriptorBufferInfo cameraBufferInfo{};
        // Add other buffer/image info structs as needed

        // 1. Write for TLAS (Binding 0)
        //BIBI
        tlasWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        tlasWriteInfo.accelerationStructureCount = 1;
        tlasWriteInfo.pAccelerationStructures = &tlas;
        VkWriteDescriptorSet tlasWrite{};
        tlasWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        tlasWrite.dstSet = rtDescriptorSet;
        tlasWrite.dstBinding = 0;
        tlasWrite.dstArrayElement = 0;
        tlasWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        tlasWrite.descriptorCount = 1;
        tlasWrite.pNext = &tlasWriteInfo;
        descriptorWrites.push_back(tlasWrite);

        // 2. Write for Output Image (Binding 1)
        imageInfo.imageView = outputImgView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet imageWrite{};
        imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        imageWrite.dstSet = rtDescriptorSet;
        imageWrite.dstBinding = 1;
        imageWrite.dstArrayElement = 0;
        imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        imageWrite.descriptorCount = 1;
        imageWrite.pImageInfo = &imageInfo;
        descriptorWrites.push_back(imageWrite);

        // 3. Write for Camera Buffer (Binding 2)
        cameraBufferInfo.buffer = camBuffer;
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = camBufSize;
        VkWriteDescriptorSet cameraWrite{};
        cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cameraWrite.dstSet = rtDescriptorSet;
        cameraWrite.dstBinding = 2;
        cameraWrite.dstArrayElement = 0;
        cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraWrite.descriptorCount = 1;
        cameraWrite.pBufferInfo = &cameraBufferInfo;
        descriptorWrites.push_back(cameraWrite);

        // 4. Add writes for other storage buffers...

        vkUpdateDescriptorSets(g_Device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

     void bind(VkCommandBuffer commandBuffer) {
        // Only bind if RT is available and resources are ready
        if (!Kinesis::GUI::raytracing_available || rtDescriptorSet == VK_NULL_HANDLE || rtPipelineLayout == VK_NULL_HANDLE /* || rtPipeline == VK_NULL_HANDLE */ ) {
            // Don't attempt to bind if RT isn't set up
            return;
        }

        // Bind the Ray Tracing Pipeline (assuming you have created rtPipeline)
        // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);

        // Bind the Ray Tracing Descriptor Set
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            rtPipelineLayout, // Use the RT pipeline layout
            0,                // First set index
            1,                // Number of sets to bind
            &rtDescriptorSet, // Pointer to the descriptor set handle
            0, nullptr);
    }


    /*************************************************************************************************************/
    //                                     Acceleration Structure functions
    /*************************************************************************************************************/
    uint64_t getBufferAddress(VkBuffer buffer) const {
        VkBufferDeviceAddressInfoKHR buffer_device_address_info{};
        buffer_device_address_info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        buffer_device_address_info.buffer = buffer;
        return vkGetBufferDeviceAddressKHR(g_Device, &buffer_device_address_info);
    }

	ScratchBuffer create_scratch_buffer(VkDeviceSize size) {
        ScratchBuffer scratch_buffer{};
    
        VkBufferCreateInfo buffer_create_info = {};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = size;
        buffer_create_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        VK_CHECK(vkCreateBuffer(g_Device, &buffer_create_info, nullptr, &scratch_buffer.buffer));
    
        VkMemoryRequirements memory_requirements = {};
        vkGetBufferMemoryRequirements(g_Device, scratch_buffer.buffer, &memory_requirements);
    
        VkMemoryAllocateFlagsInfo memory_allocate_flags_info = {};
        memory_allocate_flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        memory_allocate_flags_info.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
    
        VkMemoryAllocateInfo memory_allocate_info = {};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.pNext = &memory_allocate_flags_info;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = get_device().get_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(g_Device, &memory_allocate_info, nullptr, &scratch_buffer.memory));
        VK_CHECK(vkBindBufferMemory(g_Device, scratch_buffer.buffer, scratch_buffer.memory, 0));
    
        VkBufferDeviceAddressInfoKHR buffer_device_address_info{};
        buffer_device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        buffer_device_address_info.buffer = scratch_buffer.buffer;
        scratch_buffer.address = vkGetBufferDeviceAddressKHR(g_Device, &buffer_device_address_info);
    
        return scratch_buffer;
    }
    
	void delete_scratch_buffer(ScratchBuffer &scratch_buffer) {
        if (scratch_buffer.memory != NULL) vkFreeMemory(g_Device, scratch_buffer.memory, nullptr);
        if (scratch_buffer.buffer != NULL) vkDestroyBuffer(g_Device, scratch_buffer.buffer, nullptr);
    }

    void create_blas() {
        /*
        TODO: this below
            Though we use similar code to handle static and dynamic objects, several parts differ:
                1. Static / dynamic objects have different buffers (device-only vs host-visible)
                2. Dynamic objects use different flags (i.e. for fast rebuilds)
        */
        // create vectors to store necessary info for construction
        blas = std::vector<AccelerationStructure>(gameObjects.size());

        for (size_t i = 0; i < gameObjects.size(); i++) {
            GameObject *obj = &gameObjects[i];
            AccelerationStructure acc_struct;

            // get the mesh attached to the game object and the model attached to the mesh
            Model *model = obj->model;
            Mesh::Mesh *mesh = model->getMesh();
            uint32_t prim_count = mesh->numTriangles();

            /*
            TODO: HIGHLY recommend adding an index buffer to model so nothing gets constructed here
            */
            VkDeviceOrHostAddressConstKHR vertex_address{};
            VkDeviceOrHostAddressConstKHR index_address{};
            vertex_address.deviceAddress = get_buffer_device_address(model->getVertexBuffer());
	        index_address.deviceAddress = get_buffer_device_address(/*model->getIndexBuffer()*/);
            
            // make the geometry info
            VkAccelerationStructureGeometryTrianglesDataKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
            triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            triangles.vertexData.deviceAddress = vertexAddress;
            triangles.vertexStride = sizeof(Mesh::Vertex);
            triangles.indexType = VK_INDEX_TYPE_UINT32;
            triangles.indexData.deviceAddress = get_buffer_device_address(/*model->getIndexBuffer()*/);
            triangles.transformData = {};
            triangles.maxVertex = prim_count*3-1;

            VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
            asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
            asGeom.geometry.triangles = triangles;

            VkAccelerationStructureBuildRangeInfoKHR offset;
            offset.firstVertex = 0;
            offset.primitiveCount  = prim_count;
            offset.primitiveOffset = 0;
            offset.transformOffset = 0;
            
            VkAccelerationStructureBuildGeometryInfoKHR structure_geometry_info{};
            structure_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            structure_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            structure_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            structure_geometry_info.geometryCount = 1;
            structure_geometry_info.pGeometries = &asGeom;

            VkAccelerationStructureBuildSizesInfoKHR build_size_info{};
            build_size_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            vkGetAccelerationStructureBuildSizesKHR(g_Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &structure_geometry_info, &prim_count, &build_size_info);

            // make the buffer to store
            blas[i] = AccelerationStructure{};
            blas[i].buffer = /* TODO: make new buffer to correct size */;

            VkAccelerationStructureCreateInfoKHR accel_info{};
            accel_info.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            accel_info.buffer = blas[i].buffer;
            accel_info.size   = build_size_info.accelerationStructureSize;
            accel_info.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            vkCreateAccelerationStructureKHR(g_Device, &acceleration_structure_create_info, nullptr, &blas[i].structure);

            // making the actual acceleration structure now
            ScratchBuffer scratch_buffer = create_scratch_buffer(build_size_info.buildScratchSize);

            // new geometry info :(
            VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info{};
            build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            build_geometry_info.dstAccelerationStructure  = blas[i].structure;
            build_geometry_info.geometryCount = 1;
            build_geometry_info.pGeometries = &asGeom;
            build_geometry_info.scratchData.deviceAddress = scratch_buffer.device_address;

            /* TODO: create command buffer for this */
            VkCommandBufferAllocateInfo buffer_allocate{};
            buffer_allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            buffer_allocate.commandPool = /* TODO: command pool here :( */;
            buffer_allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            buffer_allocate.commandBufferCount = 1;
            
            VkCommandBuffer command_buffer;
            VK_CHECK(vkAllocateCommandBuffers(g_Device, &buffer_allocate, &command_buffer));
            VkCommandBufferBeginInfo buffer_info{};
            buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            VK_CHECK(vkBeginCommandBuffer(command_buffer, &buffer_info));

            vkCmdBuildAccelerationStructuresKHR(command_buffer, 1, &structure_geometry_info, &offset);
            /* TODO: flush the command buffer here */

            delete_scratch_buffer(scratch_buffer);

            // get address info
            VkAccelerationStructureDeviceAddressInfoKHR address_info{};
            address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            address_info.accelerationStructure = blas[i].structure;
            blas[i].address = vkGetAccelerationStructureDeviceAddressKHR(g_Device, &address_info);
        }
    }

	void create_tlas() {
        std::vector<VkAccelerationStructureInstanceKHR> temp_tlas = std::vector<VkAccelerationStructureInstanceKHR>(gameObjects.size());
        //VkDeviceOrHostAddressConstKHR
        
        for (size_t i = 0; i < gameObjects.size(); i++) {
            GameObject *obj = &gameObjects[i];
            VkAccelerationStructureInstanceKHR instance{};
            // TODO: convert/append VkTransformMatrixKHR matrix to transform.h so it can be grabbed here
            instance.transform; // = obj.transform.(VkTransformMatrixKHR ver here);
            instance.instanceCustomIndex = i;
            instance.mask = 0xFF;
            /* TODO: add in backface hit for transmissive objects, which will be a different offest */
            // if (obj->getMesh()->getMaterial()->isTransmissive()) instance.instanceShaderBindingTableRecordOffset = 1; 
            // else
            instance.instanceShaderBindingTableRecordOffset = 0;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance.accelerationStructureReference = blas[i].address;

            temp_tlas.push_back(instance);
        }

        VkBufferCreateInfo buff_create{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buff_create.size = gameObjects.size();
        buff_create.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
       
        //
        if (!instances_buffer) {
            /* TODO: make instance buffer out of temp_tlas data here */
        }
        // TODO: update buffer if any changes

        VkDeviceOrHostAddressConstKHR instance_address{};
        instance_address.deviceAddress = get_buffer_device_address(instances_buffer);
        
        VkAccelerationStructureGeometryKHR structure_geometry{};
        structure_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        structure_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        structure_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        structure_geometry.geometry.instances = {};
        structure_geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        structure_geometry.geometry.instances.arrayOfPointers = VK_FALSE;
        structure_geometry.geometry.instances.data = instance_address;

        //
        VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info{};
        build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build_geometry_info.geometryCount = 1;
        build_geometry_info.pGeometries = &structure_geometry;

        const uint32_t prim_count = gameObjects.size();

        VkAccelerationStructureBuildSizesInfoKHR build_size{};
        build_size.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(g_Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_geometry_info, &prim_count, &build_size);

        if (!tlas.buffer) {
            tlas.buffer = /* TODO: make new buffer to correct size */;
        }

        bool is_update = false;
        if (!tlas.structure) {
            VkAccelerationStructureCreateInfoKHR create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            create_info.buffer = tlas.buffer;
            create_info.size = build_size.accelerationStructureSize;
            create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            vkCreateAccelerationStructureKHR(g_Device, &create_info, nullptr, &tlas.structure);
        }
        else is_update = true;

        /* TODO: scratch buffer AUUUUG */
        ScratchBuffer scratch_buffer = create_scratch_buffer(build_size.buildScratchSize);

        build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        build_geometry_info.mode = is_update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build_geometry_info.dstAccelerationStructure = tlas.structure;
        if (is_update) build_geometry_info.srcAccelerationStructure = tlas.structure;
        build_geometry_info.scratchData.deviceAddress

        VkAccelerationStructureBuildRangeInfoKHR range_info;
        range_info.primitiveCount = prim_count;
        range_info.primitiveOffset = 0;
        range_info.firstVertex = 0;
        range_info.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR *> acceleration_build_structure_range_infos = {&range_info};

        /* TODO: create command buffer for this */
        VkCommandBufferAllocateInfo buffer_allocate{};
        buffer_allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        buffer_allocate.commandPool = /* TODO: command pool here :( */;
        buffer_allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        buffer_allocate.commandBufferCount = 1;
        
        VkCommandBuffer command_buffer;
        VK_CHECK(vkAllocateCommandBuffers(g_Device, &buffer_allocate, &command_buffer));
        VkCommandBufferBeginInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK(vkBeginCommandBuffer(command_buffer, &buffer_info));

        vkCmdBuildAccelerationStructuresKHR(command_buffer, 1, &structure_geometry_info, &offset);

        /* TODO: flush the command buffer here */

        delete_scratch_buffer(scratch_buffer);

        VkAccelerationStructureDeviceAddressInfoKHR address_info{};
        address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        address_info.accelerationStructure = tlas.structure;
        tlas.device_address = vkGetAccelerationStructureDeviceAddressKHR(g_Device, &address_info);
    }

	void delete_acceleration_structure(AccelerationStructure &acceleration_structure) {
        /*
        TODO: reset/clean buffer of structure
        */
        if (acceleration_structure.handle) vkDestroyAccelerationStructureKHR(g_Device, acceleration_structure.handle, nullptr);
    }
} 