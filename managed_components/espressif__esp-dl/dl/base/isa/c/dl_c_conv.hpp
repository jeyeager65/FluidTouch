#pragma once

// Compile-time names for the C conv kernels. dl_kernel.inc casts these
// names to dl_kernel_erased_t; they are pointers to the template
// instantiations, not wrapper functions. inline constexpr is one entity across
// translation units. The table stores that pointer value.

#include "dl_define_private.hpp"
#include <cstdint>

namespace dl {
namespace base {

struct ConvArgsType;

template <typename feature_t, typename buffer_t, typename filter_t = feature_t>
void conv2d_11cn(buffer_t *buffer, feature_t *input, const ConvArgsType &args);

template <typename feature_t, typename buffer_t, typename filter_t = feature_t>
void conv2d_33cn(buffer_t *buffer, feature_t *input, const ConvArgsType &args);

template <typename feature_t, typename buffer_t, typename filter_t = feature_t>
void conv2d_hwcn(buffer_t *buffer, feature_t *input, const ConvArgsType &args);

} // namespace base
} // namespace dl

inline constexpr auto dl_c_s16_conv2d_11cn = &dl::base::conv2d_11cn<int16_t, DL_S16_BUFFER_TYPE, int16_t>;
inline constexpr auto dl_c_s16_conv2d_33cn = &dl::base::conv2d_33cn<int16_t, DL_S16_BUFFER_TYPE, int16_t>;
inline constexpr auto dl_c_s16_conv2d_hwcn = &dl::base::conv2d_hwcn<int16_t, DL_S16_BUFFER_TYPE, int16_t>;

inline constexpr auto dl_c_s8_conv2d_11cn = &dl::base::conv2d_11cn<int8_t, int32_t, int8_t>;
inline constexpr auto dl_c_s8_conv2d_33cn = &dl::base::conv2d_33cn<int8_t, int32_t, int8_t>;
inline constexpr auto dl_c_s8_conv2d_hwcn = &dl::base::conv2d_hwcn<int8_t, int32_t, int8_t>;

inline constexpr auto dl_c_w8a16_conv2d_11cn = &dl::base::conv2d_11cn<int16_t, DL_S16_BUFFER_TYPE, int8_t>;
inline constexpr auto dl_c_w8a16_conv2d_33cn = &dl::base::conv2d_33cn<int16_t, DL_S16_BUFFER_TYPE, int8_t>;
inline constexpr auto dl_c_w8a16_conv2d_hwcn = &dl::base::conv2d_hwcn<int16_t, DL_S16_BUFFER_TYPE, int8_t>;
