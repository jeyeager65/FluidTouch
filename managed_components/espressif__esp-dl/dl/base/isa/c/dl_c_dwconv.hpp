#pragma once

// Compile-time names for the C depthwise conv kernels. Same contract as
// dl_c_conv.hpp: dl_kernel.inc casts these names to dl_kernel_erased_t.

#include "dl_define_private.hpp"
#include <cstdint>

namespace dl {
namespace base {

struct ConvArgsType;

template <typename feature_t, typename buffer_t>
void depthwise_conv2d_33c1(buffer_t *buffer, feature_t *input, const ConvArgsType &args);

template <typename feature_t, typename buffer_t>
void depthwise_conv2d_hwc1(buffer_t *buffer, feature_t *input, const ConvArgsType &args);

} // namespace base
} // namespace dl

inline constexpr auto dl_c_s16_depthwise_conv2d_33c1 = &dl::base::depthwise_conv2d_33c1<int16_t, DL_S16_BUFFER_TYPE>;
inline constexpr auto dl_c_s16_depthwise_conv2d_hwc1 = &dl::base::depthwise_conv2d_hwc1<int16_t, DL_S16_BUFFER_TYPE>;

inline constexpr auto dl_c_s8_depthwise_conv2d_33c1 = &dl::base::depthwise_conv2d_33c1<int8_t, int32_t>;
inline constexpr auto dl_c_s8_depthwise_conv2d_hwc1 = &dl::base::depthwise_conv2d_hwc1<int8_t, int32_t>;
