#ifndef VK_HPP
#define VK_HPP

#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <variant>
#include <vector>
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

struct ErrorDetail {
  VkResult vulkan;
  VkMiniError vkMini;

  use bool is_ok() const { return vkMini == VKMINI_NO_ERROR; }
  use operator bool() const { return is_ok(); }
};

template <typename T, typename E = VkMiniError> struct Result {
  std::variant<T, E> data;

  Result(std::optional<T> value, std::optional<E> error)
      : data(value.has_value()
                 ? std::variant<T, E>(std::in_place_index<0>, value.value())
                 : std::variant<T, E>(std::in_place_index<1>, error.value())) {}

public:
  use static Result Ok(T value) { return Result(value, None); }
  use static Result Error(E error) { return Result(None, error); }

  use bool is_ok() { return data.index() == 0; }
  use T get_value() && { return std::get<T>(data); }
  use T &get_value() & { return std::get<T>(data); }
  use T const &get_value() const & { return std::get<T>(data); }

  use bool is_error() { return !is_ok(); }
  use E get_error() && { return std::get<E>(data); }
  use E &get_error() & { return std::get<E>(data); }
  use E const &get_error() const & { return std::get<E>(data); }

  use operator bool() { return is_ok(); }
};

class CallAtDestruction {
  std::function<void()> fn;

public:
  CallAtDestruction(std::function<void()> _fn) : fn(_fn) {}

  ~CallAtDestruction() { fn(); }
};

} // namespace vk

#endif