#include "vkmini.hpp"
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

} // namespace vk