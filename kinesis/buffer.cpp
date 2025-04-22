#include "buffer.h"
#include "window.h" // For createBuffer and findMemoryType helpers

#include <cassert>
#include <cstring> // For memcpy
#include <stdexcept> // For std::runtime_error

namespace Kinesis {

// Helper function to calculate aligned size
VkDeviceSize Buffer::getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment) {
  if (minOffsetAlignment > 0) {
    // Calculate the aligned size. The formula effectively rounds up instanceSize
    // to the nearest multiple of minOffsetAlignment.
    return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
  }
  return instanceSize; // No alignment needed if minOffsetAlignment is 0
}

Buffer::Buffer(
    VkDeviceSize instanceSize,
    uint32_t instanceCount,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags memoryPropertyFlags,
    VkDeviceSize minOffsetAlignment)
    : device{Kinesis::g_Device}, // Initialize with global device handle
      instanceSize{instanceSize},
      instanceCount{instanceCount},
      usageFlags{usageFlags},
      memoryPropertyFlags{memoryPropertyFlags} {
  // Ensure the device handle is valid before proceeding
  if (device == VK_NULL_HANDLE) {
      throw std::runtime_error("Device handle (g_Device) is null during Buffer creation!");
  }
  alignmentSize = getAlignment(instanceSize, minOffsetAlignment);
  bufferSize = alignmentSize * instanceCount; // Calculate total buffer size

  // Use the existing createBuffer helper from the Window namespace
  Kinesis::Window::createBuffer(bufferSize, usageFlags, memoryPropertyFlags, buffer, memory);
}

Buffer::~Buffer() {
  unmap(); // Unmap memory if it was mapped

  // Check device handle validity before destroying Vulkan objects
  if (device != VK_NULL_HANDLE) {
       if (buffer != VK_NULL_HANDLE) {
           vkDestroyBuffer(device, buffer, nullptr);
           buffer = VK_NULL_HANDLE; // Nullify handle after destruction
       }
       if (memory != VK_NULL_HANDLE) {
           vkFreeMemory(device, memory, nullptr);
           memory = VK_NULL_HANDLE; // Nullify handle after destruction
       }
   }
}

// Maps the entire buffer memory range unless size/offset are specified.
VkResult Buffer::map(VkDeviceSize size, VkDeviceSize offset) {
  assert(buffer && memory && "Called map on buffer before creation");
  // Ensure device handle is valid before mapping
  if (device == VK_NULL_HANDLE) {
       return VK_ERROR_INITIALIZATION_FAILED; // Or another appropriate error
  }
  return vkMapMemory(device, memory, offset, size, 0, &mapped);
}

// Unmaps the buffer memory. Safe to call even if not mapped.
void Buffer::unmap() {
  if (mapped && device != VK_NULL_HANDLE) {
    vkUnmapMemory(device, memory);
    mapped = nullptr;
  }
}

// Copies data to the mapped buffer region. Asserts that the buffer is mapped.
void Buffer::writeToBuffer(void *data, VkDeviceSize size, VkDeviceSize offset) {
  assert(mapped && "Cannot copy to unmapped buffer");
  assert(data && "Data pointer cannot be null");

  if (size == VK_WHOLE_SIZE) {
    memcpy(mapped, data, bufferSize);
  } else {
    // Ensure size and offset are within bounds (basic check)
    assert(offset + size <= bufferSize && "Write operation out of buffer bounds");
    char *memOffset = static_cast<char *>(mapped);
    memOffset += offset;
    memcpy(memOffset, data, size);
  }
}

// Flushes a specific range of mapped memory to ensure device visibility.
// Required for memory types without VK_MEMORY_PROPERTY_HOST_COHERENT_BIT.
VkResult Buffer::flush(VkDeviceSize size, VkDeviceSize offset) {
  // Ensure device handle is valid
  if (device == VK_NULL_HANDLE) {
      return VK_ERROR_INITIALIZATION_FAILED;
  }
  VkMappedMemoryRange mappedRange = {};
  mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  mappedRange.memory = memory;
  mappedRange.offset = offset;
  mappedRange.size = size;
  return vkFlushMappedMemoryRanges(device, 1, &mappedRange);
}

// Returns a descriptor buffer info struct used for updating descriptor sets.
VkDescriptorBufferInfo Buffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset) {
  return VkDescriptorBufferInfo{
      buffer, // Buffer handle
      offset, // Starting offset
      size,   // Size of the range
  };
}

// Invalidates a range of mapped memory to ensure host visibility of device writes.
// Used for memory types without VK_MEMORY_PROPERTY_HOST_COHERENT_BIT when reading back data.
VkResult Buffer::invalidate(VkDeviceSize size, VkDeviceSize offset) {
   // Ensure device handle is valid
   if (device == VK_NULL_HANDLE) {
       return VK_ERROR_INITIALIZATION_FAILED;
   }
  VkMappedMemoryRange mappedRange = {};
  mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  mappedRange.memory = memory;
  mappedRange.offset = offset;
  mappedRange.size = size;
  return vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);
}

// --- Instance-based helpers ---

// Writes data to the buffer at the location corresponding to the given index.
void Buffer::writeToIndex(void *data, int index) {
  assert(index >= 0 && index < instanceCount && "Index out of bounds");
  writeToBuffer(data, instanceSize, index * alignmentSize);
}

// Flushes the memory range corresponding to the given index.
VkResult Buffer::flushIndex(int index) {
  assert(index >= 0 && index < instanceCount && "Index out of bounds");
  return flush(alignmentSize, index * alignmentSize);
}

// Gets descriptor info for the memory range corresponding to the given index.
VkDescriptorBufferInfo Buffer::descriptorInfoForIndex(int index) {
   assert(index >= 0 && index < instanceCount && "Index out of bounds");
  return descriptorInfo(alignmentSize, index * alignmentSize);
}

// Invalidates the memory range corresponding to the given index.
VkResult Buffer::invalidateIndex(int index) {
   assert(index >= 0 && index < instanceCount && "Index out of bounds");
  return invalidate(alignmentSize, index * alignmentSize);
}

} // namespace Kinesis