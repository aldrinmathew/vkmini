#ifndef VK_HPP
#define VK_HPP

#include <optional>
#include <vulkan/vulkan_core.h>

#define use [[nodiscard]]

namespace vk {

static const auto None = std::nullopt;
using u32 = uint32_t;
template <typename T> using Maybe = std::optional<T>;

enum VkMiniError {
  VKMINI_NO_ERROR = 0,

  VKMINI_FAILED_TO_CREATE_BUFFER,
  VKMINI_FAILED_TO_ALLOCATE_BUFFER_MEMORY,

  VKMINI_FAILED_TO_FIND_SUITABLE_MEMORY_TYPE,

  VKMINI_FAILED_TO_ALLOCATE_COMMAND_BUFFER,
  VKMINI_FAILED_TO_BEGIN_COMMAND_BUFFER,
  VKMINI_FAILED_TO_END_COMMAND_BUFFER,
  VKMINI_FAILED_TO_SUBMIT_COMMAND_BUFFER,

  VKMINI_FAILED_TO_MAP_MEMORY,
  VKMINI_FAILED_WAITING_FOR_QUEUE_TO_FINISH,
};

} // namespace vk

#endif