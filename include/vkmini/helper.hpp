#ifndef VK_HELPER_HPP
#define VK_HELPER_HPP

#ifndef VKMINI_MULTITHREAD
#define VKMINI_MULTITHREAD true
#endif

#if VKMINI_MULTITHREAD
#define VKMINI_IF_MULTITHREAD(x) x
#define VKMINI_INSIDE_LOCK(x)                                                                                          \
	while (!Ctx::globalMutex.try_lock()) {                                                                               \
	}                                                                                                                    \
	x Ctx::globalMutex.unlock();
#else
#define VKMINI_IF_MULTITHREAD(x)
#define VKMINI_INSIDE_LOCK(x) x
#include <mutex>
#endif

#include <boost/assert/source_location.hpp>
#include <boost/stacktrace/stacktrace.hpp>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#define use [[nodiscard]]

namespace vk {

#define use [[nodiscard]]

using u8         = uint8_t;
using u16        = uint16_t;
using u32        = uint32_t;
using u64        = uint64_t;
using i8         = int8_t;
using i16        = int16_t;
using i32        = int32_t;
using i64        = int64_t;
using uchar      = unsigned char;
using String     = std::string;
using StringView = std::string_view;
using usize      = std::size_t;
using Path       = std::filesystem::path;

template <typename T> using Maybe = std::optional<T>;
template <typename T> using Vec   = std::vector<T>;
template <typename T> using Set   = std::set<T>;

template <typename A, typename B> using Map  = std::map<A, B>;
template <typename A, typename B> using Pair = std::pair<A, B>;

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
