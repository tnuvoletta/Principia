﻿#pragma once

#include <optional>

#include "testing_utilities/approximate_quantity.hpp"

namespace principia {
namespace testing_utilities {
namespace internal_approximate_quantity {

template<typename Dimensions>
Quantity<Dimensions> ApproximateQuantity<Quantity<Dimensions>>::min() const {
  return min_multiplier_ * unit_;
}

template<typename Dimensions>
Quantity<Dimensions> ApproximateQuantity<Quantity<Dimensions>>::max() const {
  return max_multiplier_ * unit_;
}

template<typename Dimensions>
std::string ApproximateQuantity<Quantity<Dimensions>>::DebugString() const {
  return representation_ + "(" + std::to_string(ulp_) + ") * " +
         quantities::DebugString(unit_);
}

template<typename Dimensions>
ApproximateQuantity<Quantity<Dimensions>>::ApproximateQuantity(
    std::string const& representation,
    int const ulp,
    double const min_multiplier,
    double const max_multiplier,
    Quantity<Dimensions> const& unit)
    : representation_(representation),
      ulp_(ulp),
      min_multiplier_(min_multiplier),
      max_multiplier_(max_multiplier),
      unit_(unit) {}

ApproximateQuantity<double> ApproximateQuantity<double>::Parse(
    std::string_view const representation,
    int const ulp) {
  std::string error_representation(representation);
  std::optional<int> last_digit_index;
  bool const is_hexadecimal =
      error_representation.size() >= 2 &&
      error_representation[0] == '0' &&
      (error_representation[1] == 'x' || error_representation[1] == 'X');

  // Replace all the digits before the exponent by zeroes, except for the last
  // one which get the number of ulps.  The result is the string representation
  // of the error on the quantity.
  for (int i = 0; i < error_representation.size(); ++i) {
    char const c = error_representation[i];
    if (c >= '1' && c <= '9') {
      error_representation[i] = '0';
      last_digit_index = i;
    } else if (is_hexadecimal &&
               ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
      error_representation[i] = '0';
      last_digit_index = i;
    } else if ((!is_hexadecimal && (c == 'e' || c == 'E')) ||
               (is_hexadecimal && (c == 'p' || c == 'P'))) {
      CHECK(last_digit_index.has_value());
      break;
    }
  }
  if (ulp <= 9) {
    error_representation[*last_digit_index] = '0' + ulp;
  } else {
    CHECK(is_hexadecimal);
    error_representation[*last_digit_index] = 'A' + ulp - 10;
  }
  double const value = std::strtod(representation.data(), nullptr);
  double const error = std::strtod(error_representation.c_str(), nullptr);
  return ApproximateQuantity<double>(representation,
                                     ulp,
                                     value - error,
                                     value + error);
}

double ApproximateQuantity<double>::min() const {
  return min_multiplier_;
}

double ApproximateQuantity<double>::max() const {
  return max_multiplier_;
}

std::string ApproximateQuantity<double>::DebugString() const {
  return representation_ + "(" + std::to_string(ulp_) + ")";
}

ApproximateQuantity<double>::ApproximateQuantity(
    std::string_view const representation,
    int const ulp,
    double const min_multiplier,
    double const max_multiplier)
    : representation_(representation),
      ulp_(ulp),
      min_multiplier_(min_multiplier),
      max_multiplier_(max_multiplier) {}

template<typename Left, typename RDimensions>
ApproximateQuantity<Product<Left, Quantity<RDimensions>>> operator*(
    ApproximateQuantity<Left> const& left,
    Quantity<RDimensions> const& right) {
  return ApproximateQuantity<Product<Left, Quantity<RDimensions>>>(
      left.representation_,
      left.ulp_,
      left.min_multiplier_,
      left.max_multiplier_,
      left.unit_ * right);
}

template<typename Left, typename RDimensions>
ApproximateQuantity<Quotient<Left, Quantity<RDimensions>>> operator/(
    ApproximateQuantity<Left> const& left,
    Quantity<RDimensions> const& right) {
  return ApproximateQuantity<Quotient<Left, Quantity<RDimensions>>>(
      left.representation_,
      left.ulp_,
      left.min_multiplier_,
      left.max_multiplier_,
      left.unit_ / right);
}

template<typename Quantity>
std::ostream& operator<<(std::ostream& out,
                         ApproximateQuantity<Quantity> const& q) {
  out << q.DebugString();
  return out;
}

#define PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(symbol, ulp) \
  ApproximateQuantity<double> operator""_##symbol(                      \
      char const* const representation) {                               \
    return ApproximateQuantity<double>::Parse(representation, ulp);     \
  }

PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(⑴, 1)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(⑵, 2)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(⑶, 3)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(⑷, 4)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(⑸, 5)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(⑹, 6)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(⑺, 7)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(⑻, 8)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(⑼, 9)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(🄐, 10)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(🄑, 11)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(🄒, 12)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(🄓, 13)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(🄔, 14)
PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION(🄕, 15)

#undef PRINCIPIA_APPROXIMATE_QUANTITY_OPERATOR_DEFINITION

}  // namespace internal_approximate_quantity
}  // namespace testing_utilities
}  // namespace principia
