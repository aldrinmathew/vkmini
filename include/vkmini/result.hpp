#ifndef VK_RESULT_HPP
#define VK_RESULT_HPP

#include <type_traits>
#include <utility>
#include <variant>
#include <vkmini/helper.hpp>
#include <vulkan/vulkan.h>

namespace vk {

enum ErrorCode {
	VKMINI_NO_ERROR = 0,

	VKMINI_PATH_DOES_NOT_EXIST,
	VKMINI_BUFFER_IS_EMPTY,

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
	VkResult  vulkan;
	ErrorCode vkMini;

	use bool is_ok() const { return vkMini == VKMINI_NO_ERROR; }
};

/// A datatype that represents the result of an operation, to be used to
/// do explicit error handling, in order to avoid exceptions
template <typename T, typename E = ErrorCode> struct Result {
	std::variant<T, E> data;

	Result(std::variant<T, E> _data) : data(std::move(_data)) {}

public:
	Result(Result<T, E> const& other)
	  requires std::is_copy_constructible<T>::value && std::is_copy_constructible<E>::value
	= default;

	Result(Result<T, E>&& other)
	  requires std::is_move_constructible<T>::value && std::is_move_constructible<E>::value
	= default;

	Result<T, E>& operator=(Result<T, E> const& other)
	  requires std::is_copy_assignable<T>::value && std::is_copy_assignable<E>::value
	= default;

	Result<T, E>& operator=(Result<T, E>&& other)
	  requires std::is_move_assignable<T>::value && std::is_move_assignable<E>::value
	= default;

	/// Create `Result` containing a successful value
	use static Result Ok(T value) { return Result(std::variant<T, E>(std::in_place_index<0>, value)); }

	/// Create `Result` containing error
	use static Result Error(E error) { return Result(std::variant<T, E>(std::in_place_index<1>, error)); }

	/// Whether this `Result` is successful and contains a valid value
	use bool is_ok() const { return data.index() == 0; }

	/// Get the value if this `Result` is successful.
	/// Call this function after making sure that `is_ok` returns `true`
	use T&& get_value() && { return std::move(std::get<0>(data)); }

	/// Get the value if this `Result` is successful.
	/// Call this function after making sure that `is_ok` returns `true`
	use T const&& get_value() const&& { return std::move(std::get<0>(data)); }

	/// Get a reference to the value if this `Result` is successful.
	/// Call this function after making sure that `is_ok` returns `true`
	use T& get_value() & { return std::get<0>(data); }

	/// Get a const-reference to the value if this result is successful
	/// Call this function after making sure that `is_ok` returns `true`
	use T const& get_value() const& { return std::get<0>(data); }

	/// Whether this `Result` has an error
	use bool is_error() const { return data.index() == 1; }

	/// Get the error value if this `Result` has an error.
	/// Call this function after making sure that `is_ok` returns `false` or `is_error` returns `true`
	use E&& get_error() && { return std::move(std::get<1>(data)); }

	/// Get the error value if this `Result` has an error.
	/// Call this function after making sure that `is_ok` returns `false` or `is_error` returns `true`
	use E const&& get_error() const&& { return std::move(std::get<1>(data)); }

	/// Get a reference to the error if this `Result` has an error.
	/// Call this function after making sure that `is_ok` returns `false` or `is_error` returns `true`
	use E& get_error() & { return std::get<1>(data); }

	/// Get a const-reference to the error if this `Result` has an error.
	/// Call this function after making sure that `is_ok` returns `false` or `is_error` returns `true`
	use E const& get_error() const& { return std::get<1>(data); }
};

} // namespace vk

#endif
