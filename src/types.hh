#pragma once

#include <memory>

/// @brief A value that can be stored in a specific endianness.
/// @tparam T the type of the value
/// @tparam E the endianness of the value
template <typename T, const std::endian E>
class Value {
  T value_;

  template <const std::endian SOURCE, const std::endian TARGET>
  static constexpr T transform(T input) {
    if constexpr (SOURCE == TARGET) {
      return input;
    } else {
      return std::byteswap(input);
    }
  }

 public:
  constexpr operator T() const {
    return value();
  }

  constexpr void operator=(T value) {
    value_ = transform<std::endian::native, E>(value);
  }

  constexpr T value() const {
    return transform<E, std::endian::native>(value_);
  }
} __attribute__((packed));

static_assert(sizeof(uint32_t) == sizeof(Value<uint32_t, std::endian::big>));
static_assert(sizeof(uint32_t) == sizeof(Value<uint32_t, std::endian::little>));

template <typename T, const std::endian E>
class TypedValue {
  static_assert(
      std::is_union_v<T> || std::is_class_v<T>, "T must be a class, struct, or union type.");
  static_assert(
      std::is_integral_v<typename T::RealType>, "T must define a RealType integral type alias.");
  static_assert(std::is_constructible_v<T, typename T::RealType>,
      "T must be constructible from T::RealType.");
  static_assert(
      sizeof(T) == sizeof(typename T::RealType), "T must be the same size as T::RealType.");

  Value<typename T::RealType, E> value_;

 public:
  constexpr operator T() const {
    return T(value_);
  }

  constexpr void operator=(T value) {
    value_ = value;
  }

  constexpr T value() const {
    return T(value_.value());
  }
};