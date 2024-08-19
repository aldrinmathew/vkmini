# VkMini

Simplify your Vulkan apps and improve the development experience

## Using this library

If your application uses _several threads_ or if you are unsure, include **`vkmini.hpp`** all the time, instead of any other header.

If your application uses just one thread, include **`vkmini_single.hpp`** all the time to avoid the cost of mutex locks.

