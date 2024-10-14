#ifndef VK_HELPER_HPP
#define VK_HELPER_HPP

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace vk {

#define use [[nodiscard]]

using u8     = uint8_t;
using u16    = uint16_t;
using u32    = uint32_t;
using u64    = uint64_t;
using i32    = int32_t;
using i64    = int64_t;
using uchar  = unsigned char;
using String = std::string;
using usize  = std::size_t;
using Path   = std::filesystem::path;

template <typename T> using Maybe           = std::optional<T>;
template <typename T> using Vec             = std::vector<T>;
template <typename A, typename B> using Map = std::map<A, B>;

static const auto None = std::nullopt;

class Slice {
	u8*   ptr;
	usize length;

public:
	Slice(u8* _ptr, usize _length) : ptr(_ptr), length(_length) {}

	use inline bool  is_null() const { return (ptr == nullptr) || (length == 0); }
	use inline u8*   get_ptr() const { return ptr; }
	use inline usize get_length() const { return length; }
};

} // namespace vk

#endif
