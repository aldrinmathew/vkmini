#include "vkmini.hpp"
#include <cstring>
#include <vulkan/vulkan_core.h>

namespace vk {

VKMINI_IF_MULTITHREAD(std::mutex CtxTy::globalMutex{};)
std::vector<Ctx> CtxTy::allContexts{};
std::vector<Buffer> BufferTy::allBuffers{};
std::vector<CommandBuffer> CommandBufferTy::allCommandBuffers{};

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

std::optional<uint32_t> find_memory_type(Ctx ctx, uint32_t typeFilter,
                                         VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(ctx->physical, &memProperties);
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
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

Result<CommandBuffer, ErrorPair>
CommandBufferTy::create(Ctx ctx, VkCommandBufferLevel level) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = ctx->commandPool;
  allocInfo.level = level;
  allocInfo.commandBufferCount = 1;
  VkCommandBuffer buffer;
  auto res = vkAllocateCommandBuffers(ctx->logical, &allocInfo, &buffer);
  if (res != VK_SUCCESS) {
    return Result<CommandBuffer, ErrorPair>::Error(
        {res, VKMINI_FAILED_TO_ALLOCATE_COMMAND_BUFFER});
  }
  auto bufferResult = new CommandBufferTy(ctx, buffer);

  VKMINI_INSIDE_LOCK(allCommandBuffers.push_back(bufferResult);)

  return Result<CommandBuffer, ErrorPair>::Ok(bufferResult);
}

ErrorPair CommandBufferTy::begin(VkCommandBufferUsageFlags flags) {
  switch (state) {
  case CommandBufferState::NONE: {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;
    beginInfo.pInheritanceInfo = nullptr;
    auto res = vkBeginCommandBuffer(buffer, &beginInfo);
    if (res == VK_SUCCESS) {
      state = CommandBufferState::BEGUN;
      return {res, VKMINI_NO_ERROR};
    }
    return {res, VKMINI_FAILED_TO_BEGIN_COMMAND_BUFFER};
  }
  case CommandBufferState::BEGUN:
    return {VK_ERROR_UNKNOWN, VKMINI_COMMAND_BUFFER_ALREADY_BEGUN};
  case CommandBufferState::END:
    return {VK_ERROR_UNKNOWN, VKMINI_COMMAND_BUFFER_ALREADY_ENDED};
  case CommandBufferState::RECORDING:
    return {VK_ERROR_UNKNOWN, VKMINI_COMMAND_BUFFER_ALREADY_RECORDING};
  }
  return {VK_SUCCESS, VKMINI_NO_ERROR};
}

VkMiniError
CommandBufferTy::record(std::function<void(VkCommandBuffer)> callback) {
  switch (state) {
  case CommandBufferState::RECORDING:
  case CommandBufferState::BEGUN: {
    callback(buffer);
    state = CommandBufferState::RECORDING;
    return VKMINI_NO_ERROR;
  }
  case CommandBufferState::END:
    return VKMINI_COMMAND_BUFFER_ALREADY_ENDED;
  case CommandBufferState::NONE:
    return VKMINI_COMMAND_BUFFER_HAS_NOT_BEGUN;
  }
  return VKMINI_NO_ERROR;
}

ErrorPair CommandBufferTy::end() {
  switch (state) {
  case CommandBufferState::BEGUN:
  case CommandBufferState::RECORDING: {
    auto res = vkEndCommandBuffer(buffer);
    if (res == VK_SUCCESS) {
      state = CommandBufferState::END;
      return {res, VKMINI_NO_ERROR};
    }
    return {res, VKMINI_FAILED_TO_END_COMMAND_BUFFER};
  }
  case CommandBufferState::END:
    return {VK_ERROR_UNKNOWN, VKMINI_COMMAND_BUFFER_ALREADY_ENDED};
  case CommandBufferState::NONE:
    return {VK_ERROR_UNKNOWN, VKMINI_COMMAND_BUFFER_HAS_NOT_BEGUN};
  }
  return {VK_SUCCESS, VKMINI_NO_ERROR};
}

ErrorPair CommandBufferTy::submit(VkQueue graphicsQueue,
                                  std::optional<VkFence> fence) {
  switch (state) {
  case CommandBufferState::END: {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;
    auto res =
        vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                      fence.has_value() ? fence.value() : VK_NULL_HANDLE);
    if (res == VK_SUCCESS) {
      state = CommandBufferState::NONE;
      return {VK_SUCCESS, VKMINI_NO_ERROR};
    }
    return {res, VKMINI_FAILED_TO_SUBMIT_COMMAND_BUFFER};
  }
  case CommandBufferState::BEGUN:
  case CommandBufferState::RECORDING:
    return {VK_ERROR_UNKNOWN, VKMINI_COMMAND_BUFFER_HAS_NOT_END};
  case CommandBufferState::NONE:
    return {VK_ERROR_UNKNOWN, VKMINI_COMMAND_BUFFER_NOTHING_TO_SUBMIT};
  }
  return {VK_SUCCESS, VKMINI_NO_ERROR};
}

ErrorPair
CommandBufferTy::perform(std::function<void(VkCommandBuffer)> callback,
                         VkQueue graphicsQueue, VkCommandBufferUsageFlags flags,
                         VkFence fence) {
  auto resPair = begin(flags);
  if (resPair.vulkan != VK_SUCCESS) {
    return resPair;
  }
  auto res = record(callback);
  if (res != VKMINI_NO_ERROR) {
    return {VK_ERROR_UNKNOWN, res};
  }
  resPair = end();
  if (resPair.vulkan != VK_SUCCESS) {
    return resPair;
  }
  resPair = submit(graphicsQueue, fence);
  if (resPair.vulkan != VK_SUCCESS) {
    return resPair;
  }
  return {VK_SUCCESS, VKMINI_NO_ERROR};
}

CommandBufferTy::~CommandBufferTy() {
  vkFreeCommandBuffers(ctx->logical, ctx->commandPool, 1, &buffer);
}

} // namespace vk