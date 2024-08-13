#include "vkmini.hpp"
#include <cstring>
#include <vulkan/vulkan_core.h>

namespace vk {

std::mutex Ctx::ctxMutex{};

CallAtDestruction Ctx::deleter = CallAtDestruction([]() {
  for (auto ptr : Ctx::contextRefs) {
    delete ptr;
  }
  Ctx::contextRefs.clear();
});

std::vector<CtxRef> Ctx::contextRefs{};

Result<u32> find_memory_type(CtxRef ctx, u32 typeFilter,
                             VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(ctx->physical, &memProperties);
  for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) &&
        ((memProperties.memoryTypes[i].propertyFlags & properties) ==
         properties)) {
      return Result<u32>::Ok(i);
    }
  }
  return Result<u32>::Error(VKMINI_FAILED_TO_FIND_SUITABLE_MEMORY_TYPE);
}

Result<Buffer, ErrorDetail> Buffer::create(CtxRef ctx, VkDeviceSize size,
                                           VkBufferUsageFlags usage,
                                           VkMemoryPropertyFlags properties) {
  VkBuffer buffer;
  VkDeviceMemory memory;

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  auto res = vkCreateBuffer(ctx->logical, &bufferInfo, nullptr, &buffer);
  if (res != VK_SUCCESS) {
    return Result<Buffer, ErrorDetail>::Error(
        {res, VKMINI_FAILED_TO_CREATE_BUFFER});
  }

  VkMemoryRequirements memReq;
  vkGetBufferMemoryRequirements(ctx->logical, buffer, &memReq);
  VkDeviceSize allocationSize = memReq.size;
  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex =
      find_memory_type(ctx, memReq.memoryTypeBits, properties);
  res = vkAllocateMemory(ctx->logical, &allocInfo, nullptr, &memory);
  if (res != VK_SUCCESS) {
    return Result<Buffer, ErrorDetail>::Error(
        {res, VKMINI_FAILED_TO_ALLOCATE_BUFFER_MEMORY});
  }

  vkBindBufferMemory(ctx->logical, buffer, memory, 0);

  return Result<Buffer, ErrorDetail>::Ok(
      Buffer(ctx, allocationSize, buffer, memory));
}

VkResult Buffer::map_memory() {
  if (mapping == nullptr) {
    auto res = vkMapMemory(ctx->logical, memory, 0, size, 0, &mapping);
    if (res != VK_SUCCESS) {
      return res;
    }
  }
  return VK_SUCCESS;
}

void Buffer::unmap_memory() {
  if (mapping != nullptr) {
    vkUnmapMemory(ctx->logical, memory);
    mapping = nullptr;
  }
}

ErrorDetail Buffer::copy_data(void *data) {
  auto res = map_memory();
  if (res != VK_SUCCESS) {
    return {res, VKMINI_FAILED_TO_MAP_MEMORY};
  }
  std::memcpy(mapping, data, (size_t)size);
  unmap_memory();
  return {VK_SUCCESS, VKMINI_NO_ERROR};
}

ErrorDetail Buffer::copy_to(Buffer destination) const {
  return copy_to_vk(destination.buffer);
}

ErrorDetail Buffer::copy_to_vk(VkBuffer destination) const {
  VkMemoryRequirements destRequirements;
  vkGetBufferMemoryRequirements(ctx->logical, destination, &destRequirements);
  if (destRequirements.size != size) {
    return {VK_ERROR_UNKNOWN, VKMINI_BUFFER_SIZE_MISMATCH};
  }
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = ctx->commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer copyCommandBuffer;
  auto res =
      vkAllocateCommandBuffers(ctx->logical, &allocInfo, &copyCommandBuffer);
  if (res != VK_SUCCESS) {
    return {res, VKMINI_FAILED_TO_ALLOCATE_COMMAND_BUFFER};
  }

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  res = vkBeginCommandBuffer(copyCommandBuffer, &beginInfo);
  if (res != VK_SUCCESS) {
    return {res, VKMINI_FAILED_TO_BEGIN_COMMAND_BUFFER};
  }

  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = 0;
  copyRegion.size = size;
  vkCmdCopyBuffer(copyCommandBuffer, buffer, destination, 1, &copyRegion);

  res = vkEndCommandBuffer(copyCommandBuffer);
  if (res != VK_SUCCESS) {
    return {res, VKMINI_FAILED_TO_END_COMMAND_BUFFER};
  }

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &copyCommandBuffer;
  res = vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  if (res != VK_SUCCESS) {
    return {res, VKMINI_FAILED_TO_SUBMIT_COMMAND_BUFFER};
  }
  res = vkQueueWaitIdle(ctx->graphicsQueue);
  if (res != VK_SUCCESS) {
    return {res, VKMINI_FAILED_WAITING_FOR_QUEUE_TO_FINISH};
  }
  vkFreeCommandBuffers(ctx->logical, ctx->commandPool, 1, &copyCommandBuffer);

  return {VK_SUCCESS, VKMINI_NO_ERROR};
}

Buffer::~Buffer() {
  unmap_memory();
  vkDestroyBuffer(ctx->logical, buffer, nullptr);
  vkFreeMemory(ctx->logical, memory, nullptr);
}

} // namespace vk