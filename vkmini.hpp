#ifndef VK_HPP
#define VK_HPP

#ifndef VKMINI_MULTITHREAD
#define VKMINI_MULTITHREAD true
#endif

#if VKMINI_MULTITHREAD
#define VKMINI_IF_MULTITHREAD(x) x
#define VKMINI_INSIDE_LOCK(x)                                                  \
  while (!CtxTy::globalMutex.try_lock()) {                                     \
  }                                                                            \
  x CtxTy::globalMutex.unlock();
#else
#define VKMINI_IF_MULTITHREAD(x)
#define VKMINI_INSIDE_LOCK(x) x
#endif

#include <functional>
#include <mutex>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

#define use [[nodiscard]]

namespace vk {

static const auto None = std::nullopt;
using u32 = uint32_t;
template <typename T> using Maybe = std::optional<T>;
template <typename T> using Vec = std::vector<T>;

/// Always call this function before quitting the program, before cleaning up
/// other Vulkan resources created by you
void cleanup();

enum VkMiniError {
  VKMINI_NO_ERROR = 0,

  VKMINI_BUFFER_SIZE_MISMATCH,
  VKMINI_FAILED_TO_CREATE_BUFFER,
  VKMINI_FAILED_TO_ALLOCATE_BUFFER_MEMORY,

  VKMINI_FAILED_TO_FIND_SUITABLE_MEMORY_TYPE,

  VKMINI_FAILED_TO_ALLOCATE_COMMAND_BUFFER,
  VKMINI_FAILED_TO_BEGIN_COMMAND_BUFFER,
  VKMINI_FAILED_TO_END_COMMAND_BUFFER,
  VKMINI_FAILED_TO_SUBMIT_COMMAND_BUFFER,

  VKMINI_FAILED_TO_MAP_MEMORY,
  VKMINI_FAILED_WAITING_FOR_QUEUE_TO_FINISH,

  VKMINI_COMMAND_BUFFER_HAS_NOT_BEGUN,
  VKMINI_COMMAND_BUFFER_HAS_NOT_END,
  VKMINI_COMMAND_BUFFER_ALREADY_BEGUN,
  VKMINI_COMMAND_BUFFER_ALREADY_RECORDING,
  VKMINI_COMMAND_BUFFER_ALREADY_ENDED,
  VKMINI_COMMAND_BUFFER_NOTHING_TO_SUBMIT,
};

/// `ErrorPair` represents two error values, one from Vulkan itself,
/// and another from VkMini.
/// If `vulkan` field value is VK_SUCCESS, then there is no error to worry
/// about.
/// If it has another value, you can use the `vkMini` field to get more
/// context about the error
struct ErrorPair {
  VkResult vulkan;
  VkMiniError vkMini;

  use bool is_ok() const { return vkMini == VKMINI_NO_ERROR; }
};

/// A datatype that represents the result of an operation, to be used to
/// do explicit error handling, in order to avoid exceptions
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
};

class CtxTy;
using Ctx = CtxTy const *;

/// `CtxType` is used to represent common values of datatypes that are used
/// commonly by functions in this library
class CtxTy {
  friend class BufferTy;
  friend class CommandBufferTy;
  static Vec<Ctx> allContexts;
  VKMINI_IF_MULTITHREAD(static std::mutex globalMutex;)

  CtxTy(VkPhysicalDevice _physical, VkDevice _logical, VkQueue _graphicsQueue,
        VkCommandPool _commandPool)
      : physical(_physical), logical(_logical), graphicsQueue(_graphicsQueue),
        commandPool(_commandPool) {}

public:
  VkPhysicalDevice physical;
  VkDevice logical;
  VkQueue graphicsQueue;
  VkCommandPool commandPool;

  /// Create a `Ctx` in a thread-safe manner. This uses a mutex lock to make
  /// this thread-safe.
  static Ctx create(VkPhysicalDevice physical, VkDevice logical,
                    VkQueue graphicsQueue, VkCommandPool commandPool) {
    auto res = new CtxTy(physical, logical, graphicsQueue, commandPool);
    VKMINI_INSIDE_LOCK(allContexts.push_back(res);)
    return res;
  }

  static void cleanup();
};

class WithCtx {
protected:
  Ctx ctx;

  WithCtx(Ctx _ctx) : ctx(_ctx) {}

public:
  use Ctx get_ctx() const { return ctx; }
};

/// Used to find the appropriate memory types for buffers, based on
/// memory requirements and based on the Physical Device memory properties.
/// Potential `typeFilter` value can be the `memoryTypeBits` field of
/// `VkMemoryRequirements` which is obtained using
/// `vkGetBufferMemoryRequirements`
use Maybe<u32> find_memory_type(Ctx ctx, u32 typeFilter,
                                VkMemoryPropertyFlags properties);

class BufferTy;
using Buffer = BufferTy const *;

class BufferTy : public WithCtx {
  friend class CtxTy;
  static Vec<Buffer> allBuffers;

  VkDeviceSize size;
  VkBuffer buffer;
  VkDeviceMemory memory;
  void *mapping;

  BufferTy(Ctx _ctx, VkDeviceSize _size, VkBuffer _buffer,
           VkDeviceMemory _memory)
      : WithCtx(_ctx), size(_size), buffer(_buffer), memory(_memory) {}

public:
  /// Create a `Buffer`.
  /// Can return errors:
  /// `VKMINI_FAILED_TO_CREATE_BUFFER`,
  /// `VKMINI_FAILED_TO_ALLOCATE_BUFFER_MEMORY`
  use static Result<Buffer, ErrorPair> create(Ctx ctx, VkDeviceSize size,
                                              VkBufferUsageFlags usage,
                                              VkMemoryPropertyFlags flags);

  /// Get the intended size of this buffer
  use VkDeviceSize get_size() const { return size; }

  /// The allocation size determined by Vulkan for this buffer.
  /// This uses the Vulkan API. If you need this value several times, it is
  /// recommended to store this value and reuse it.
  use VkDeviceSize get_allocation_size() const {
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(ctx->logical, buffer, &memReq);
    return memReq.size;
  }

  /// Get the underlying `VkBuffer` of this instance
  use VkBuffer get_buffer() const { return buffer; }

  /// Get the underlying `VkDeviceMemory` of this instance
  use VkDeviceMemory get_memory() const { return memory; }

  use bool is_memory_mapped() const { return mapping != nullptr; }

  /// This is possible only if `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` was used
  /// in the `VkMemoryPropertyFlags` value while creating the buffer
  use VkResult map_memory();
  void unmap_memory();

  /// Get the active mapping of this buffer
  use void *get_mapping() const { return mapping; }

  /// Copy data from a pointer representing the data. The size of the data is
  /// unchecked.
  /// Can return error `VKMINI_FAILED_TO_MAP_MEMORY`
  use ErrorPair copy_unchecked_from(void *data);

  /// Copy contents of this buffer to another `VkBuffer` without checking
  /// if the size matches.
  /// Can return errors:
  /// `VKMINI_FAILED_TO_ALLOCATE_COMMAND_BUFFER`,
  /// `VKMINI_FAILED_TO_BEGIN_COMMAND_BUFFER`,
  /// `VKMINI_FAILED_TO_END_COMMAND_BUFFER`,
  /// `VKMINI_FAILED_TO_SUBMIT_COMMAND_BUFFER`,
  /// `VKMINI_FAILED_WAITING_FOR_QUEUE_TO_FINISH`
  use ErrorPair copy_to_vk_buffer_unchecked(VkBuffer destination) const;

  /// Copy contents of this buffer to another `Buffer`. The size of these
  /// buffers should be equal.
  /// Can return errors:
  /// `VKMINI_BUFFER_SIZE_MISMATCH`,
  /// `VKMINI_FAILED_TO_ALLOCATE_COMMAND_BUFFER`,
  /// `VKMINI_FAILED_TO_BEGIN_COMMAND_BUFFER`,
  /// `VKMINI_FAILED_TO_END_COMMAND_BUFFER`,
  /// `VKMINI_FAILED_TO_SUBMIT_COMMAND_BUFFER`,
  /// `VKMINI_FAILED_WAITING_FOR_QUEUE_TO_FINISH`
  use ErrorPair copy_to(BufferTy destination) const;

  ~BufferTy();
};

class CommandBufferTy;
using CommandBuffer = CommandBufferTy const *;

enum class CommandBufferState {
  BEGUN,
  RECORDING,
  END,
  NONE,
};

class CommandBufferTy : public WithCtx {
  friend class CtxTy;
  static Vec<CommandBuffer> allCommandBuffers;

  VkCommandBuffer buffer;
  CommandBufferState state;

  CommandBufferTy(Ctx _ctx, VkCommandBuffer _buffer)
      : WithCtx(_ctx), buffer(_buffer) {}

public:
  /// Create a `CommandBuffer`
  /// Can return error `VKMINI_FAILED_TO_ALLOCATE_COMMAND_BUFFER`
  use static Result<CommandBuffer, ErrorPair>
  create(Ctx ctx, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  /// Get the underlying `VkCommandBuffer`
  use VkCommandBuffer get_buffer() const { return buffer; }

  /// Begin the CommandBuffer. This prepares for commands to be recorded.
  use ErrorPair begin(VkCommandBufferUsageFlags beginFlags = 0);

  /// Record commands to the buffer. The commands won't be executed.
  use VkMiniError record(std::function<void(VkCommandBuffer)> callback);

  /// End recording to the command buffer
  use ErrorPair end();

  /// Submit the command buffer to the graphics queue
  use ErrorPair submit(VkQueue graphicsQueue, Maybe<VkFence> fence = None);

  /// Perform all commands as part of the callback function and submit the
  /// commands to the graphics queue.
  /// This begins the command buffer, records to it, ends it and then submits
  /// the commands to the queue.
  use ErrorPair perform(std::function<void(VkCommandBuffer)> callback,
                        VkQueue graphicsQueue,
                        VkCommandBufferUsageFlags beginFlags = 0,
                        VkFence fence = VK_NULL_HANDLE);

  ~CommandBufferTy();
};

} // namespace vk

#endif