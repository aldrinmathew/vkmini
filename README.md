# VkMini

Simplify your Vulkan apps and improve the development experience

## Using this library

If your application uses _several threads_ or if you are unsure, include **`vkmini.hpp`** all the time, instead of any other header.

If your application uses just one thread, include **`vkmini_single.hpp`** all the time to avoid the cost of mutex locks.

## Things to keep in mind

- Requires C++20 or above
- `vk::Result<T, E>` is a custom type that represents the result of a function or a group of functions, where T is the type that represents a value if the function is successful, and E is the type that represents an error.
- `VkMiniError` is an enum value that represents an error returned by functions in this library. Sometimes, the value represents standalone errors and sometimes it provides additional context to Vulkan errors.
- `vk::ErrorPair` is a struct that represents two error values. The first field `vulkan` is of type `VkResult` which is an error enum value from Vulkan itself. The second field `vkMini` is of type `VkMiniError` which provides additional context to the error, usually indicating at what point an operation failed.

