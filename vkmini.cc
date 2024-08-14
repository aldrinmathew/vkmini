#include "vkmini.hpp"
#include <cstring>
#include <vulkan/vulkan_core.h>

namespace vk {

VKMINI_IF_MULTITHREAD(std::mutex CtxTy::globalMutex{};)
Vec<Ctx> CtxTy::allContexts{};
Vec<Buffer> BufferTy::allBuffers{};
Vec<CommandBuffer> CommandBufferTy::allCommandBuffers{};

void CtxTy::cleanup() {
  VKMINI_INSIDE_LOCK({
    for (auto ptr : CtxTy::allContexts) {
      delete ptr;
    }
    for (auto ptr : BufferTy::allBuffers) {
      delete ptr;
    }
    for (auto ptr : CommandBufferTy::allCommandBuffers) {
      delete ptr;
    }
  });
}

void cleanup() { CtxTy::cleanup(); }

Maybe<u32> find_memory_type(Ctx ctx, u32 typeFilter,
                            VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(ctx->physical, &memProperties);
  for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) &&
        ((memProperties.memoryTypes[i].propertyFlags & properties) ==
         properties)) {
      return i;
    }
  }
  return None;
}

Result<Buffer, ErrorPair> BufferTy::create(Ctx ctx, VkDeviceSize size,
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
    return Result<Buffer, ErrorPair>::Error(
        {res, VKMINI_FAILED_TO_CREATE_BUFFER});
  }

  VkMemoryRequirements memReq;
  vkGetBufferMemoryRequirements(ctx->logical, buffer, &memReq);
  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memReq.size;
  auto memTy = find_memory_type(ctx, memReq.memoryTypeBits, properties);
  if (memTy.has_value()) {
    allocInfo.memoryTypeIndex = memTy.value();
  } else {
    return Result<Buffer, ErrorPair>::Error(
        {VK_ERROR_UNKNOWN, VKMINI_FAILED_TO_FIND_SUITABLE_MEMORY_TYPE});
  }
  res = vkAllocateMemory(ctx->logical, &allocInfo, nullptr, &memory);
  if (res != VK_SUCCESS) {
    return Result<Buffer, ErrorPair>::Error(
        {res, VKMINI_FAILED_TO_ALLOCATE_BUFFER_MEMORY});
  }

  vkBindBufferMemory(ctx->logical, buffer, memory, 0);
  auto bufferResult = new BufferTy(ctx, size, buffer, memory);

  VKMINI_INSIDE_LOCK(allBuffers.push_back(bufferResult);)

  return Result<Buffer, ErrorPair>::Ok(bufferResult);
}

VkResult BufferTy::map_memory() {
  if (mapping == nullptr) {
    auto res = vkMapMemory(ctx->logical, memory, 0, size, 0, &mapping);
    if (res != VK_SUCCESS) {
      return res;
    }
  }
  return VK_SUCCESS;
}

void BufferTy::unmap_memory() {
  if (mapping != nullptr) {
    vkUnmapMemory(ctx->logical, memory);
    mapping = nullptr;
  }
}

ErrorPair BufferTy::copy_unchecked_from(void *data) {
  auto res = map_memory();
  if (res != VK_SUCCESS) {
    return {res, VKMINI_FAILED_TO_MAP_MEMORY};
  }
  std::memcpy(mapping, data, (size_t)size);
  unmap_memory();
  return {VK_SUCCESS, VKMINI_NO_ERROR};
}

ErrorPair BufferTy::copy_to(BufferTy destination) const {
  if (size != destination.size) {
    return {VK_ERROR_UNKNOWN, VKMINI_BUFFER_SIZE_MISMATCH};
  }
  return copy_to_vk_buffer_unchecked(destination.buffer);
}

ErrorPair BufferTy::copy_to_vk_buffer_unchecked(VkBuffer destination) const {
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

BufferTy::~BufferTy() {
  unmap_memory();
  vkDestroyBuffer(ctx->logical, buffer, nullptr);
  vkFreeMemory(ctx->logical, memory, nullptr);
}

} // namespace vk