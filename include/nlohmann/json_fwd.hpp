/**
 * @file json_fwd.hpp
 * @brief nlohmann/json前方宣言
 * @version 0.1.0
 * @license MIT
 */

#pragma once

#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace nlohmann {

// デフォルトJSONSerializer
template<typename T, typename SFINAE = void>
struct adl_serializer;

template<
    template<typename U, typename V, typename... Args> class ObjectType = std::map,
    template<typename U, typename... Args> class ArrayType = std::vector,
    class StringType = std::string,
    class BooleanType = bool,
    class NumberIntegerType = std::int64_t,
    class NumberUnsignedType = std::uint64_t,
    class NumberFloatType = double,
    template<typename U> class AllocatorType = std::allocator,
    template<typename T, typename SFINAE = void> class JSONSerializer = adl_serializer,
    class BinaryType = std::vector<std::uint8_t>
>
class basic_json;

using json = basic_json<>;

} // namespace nlohmann 