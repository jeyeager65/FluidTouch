#include "dl_base_c.hpp"
#include "dl_base_conv_args.hpp"
#include "dl_compile_config.h"

namespace dl {
namespace base {
template <typename feature_t, typename buffer_t, typename filter_t>
void conv2d_11cn(buffer_t *buffer_ptr, feature_t *input_ptr, const ConvArgsType &args)
{
    const filter_t *filter_element = (const filter_t *)args.filter_element;

    // filter in sequence [H, W, C, N]
    // for (size_t input_c = 0; input_c < args.input_channel; input_c++)
    // {
    //     for (size_t output_c = 0; output_c < args.output_channel; output_c++)
    //     {
    //         buffer_ptr[output_c] += input_ptr[input_c] * (*filter_element++);
    //     }
    // }

    // filter in sequence [N, H, W, C]
    for (size_t output_c = 0; output_c < args.output_channel; output_c++) // N
    {
        buffer_t acc = 0;
        for (size_t input_c = 0; input_c < args.input_channel; input_c++) // C
        {
            acc += input_ptr[input_c] * (*filter_element++);
        }
        buffer_ptr[output_c] = acc;
    }
}

template <typename feature_t, typename buffer_t, typename filter_t>
void conv2d_33cn(buffer_t *buffer_ptr, feature_t *input_ptr, const ConvArgsType &args)
{
    // filter in sequence [H, W, C, N]
    // const filter_t *filter_r0 = args.filter_element;
    // const filter_t *filter_r1 = filter_r0 + args.filter_y_offset;
    // const filter_t *filter_r2 = filter_r1 + args.filter_y_offset;
    // feature_t *&input_syx_d0 = input_ptr;

    // for (size_t filter_x = 0; filter_x < 3; filter_x++)                           // W
    // {                                                                             //
    //     feature_t *input_syx_d1 = input_syx_d0 + args.input_dilation_y_offset;      //
    //     feature_t *input_syx_d2 = input_syx_d1 + args.input_dilation_y_offset;      //
    //     for (size_t input_c = 0; input_c < args.input_channel; input_c++)         // C
    //     {                                                                         //
    //         for (size_t output_c = 0; output_c < args.output_channel; output_c++) // N
    //         {
    //             buffer_ptr[output_c] += input_syx_d0[input_c] * (*filter_r0);
    //             buffer_ptr[output_c] += input_syx_d1[input_c] * (*filter_r1);
    //             buffer_ptr[output_c] += input_syx_d2[input_c] * (*filter_r2);

    //             filter_r0++;
    //             filter_r1++;
    //             filter_r2++;
    //         }
    //     }
    //     input_syx_d0 += args.input_dilation_x_offset;
    // }

    // filter in sequence [N, H, W, C]
    feature_t *input_00 = input_ptr;
    feature_t *input_01 = input_00 + args.input_dilation_x_offset;
    feature_t *input_02 = input_01 + args.input_dilation_x_offset;

    feature_t *input_10 = input_00 + args.input_dilation_y_offset;
    feature_t *input_11 = input_10 + args.input_dilation_x_offset;
    feature_t *input_12 = input_11 + args.input_dilation_x_offset;

    feature_t *input_20 = input_10 + args.input_dilation_y_offset;
    feature_t *input_21 = input_20 + args.input_dilation_x_offset;
    feature_t *input_22 = input_21 + args.input_dilation_x_offset;

    const filter_t *filter_00 = (const filter_t *)args.filter_element;
    const filter_t *filter_01 = filter_00 + args.input_channel;
    const filter_t *filter_02 = filter_01 + args.input_channel;

    const filter_t *filter_10 = filter_00 + args.filter_y_offset_c;
    const filter_t *filter_11 = filter_10 + args.input_channel;
    const filter_t *filter_12 = filter_11 + args.input_channel;

    const filter_t *filter_20 = filter_10 + args.filter_y_offset_c;
    const filter_t *filter_21 = filter_20 + args.input_channel;
    const filter_t *filter_22 = filter_21 + args.input_channel;

    for (size_t output_c = 0; output_c < args.output_channel; output_c++) {
        buffer_t acc = 0;
        for (size_t input_c = 0; input_c < args.input_channel; input_c++) {
            acc += input_00[input_c] * filter_00[input_c];
            acc += input_01[input_c] * filter_01[input_c];
            acc += input_02[input_c] * filter_02[input_c];
            acc += input_10[input_c] * filter_10[input_c];
            acc += input_11[input_c] * filter_11[input_c];
            acc += input_12[input_c] * filter_12[input_c];
            acc += input_20[input_c] * filter_20[input_c];
            acc += input_21[input_c] * filter_21[input_c];
            acc += input_22[input_c] * filter_22[input_c];
        }
        filter_00 += args.filter_n_offset_c;
        filter_01 += args.filter_n_offset_c;
        filter_02 += args.filter_n_offset_c;
        filter_10 += args.filter_n_offset_c;
        filter_11 += args.filter_n_offset_c;
        filter_12 += args.filter_n_offset_c;
        filter_20 += args.filter_n_offset_c;
        filter_21 += args.filter_n_offset_c;
        filter_22 += args.filter_n_offset_c;

        buffer_ptr[output_c] = acc;
    }
}

template <typename feature_t, typename buffer_t, typename filter_t>
void conv2d_hwcn(buffer_t *buffer_ptr, feature_t *input_ptr, const ConvArgsType &args)
{
    // filter in sequence [H, W, C, N]
    // const filter_t *filter_element = args.filter_element;                             // Reload filter
    // feature_t *&input_syx_dy = input_ptr;                                               //
    // for (size_t filter_y = 0; filter_y < args.filter_height; filter_y++)              // H
    // {                                                                                 //
    //     feature_t *input_syx_dyx = input_syx_dy;                                        //
    //     for (size_t filter_x = 0; filter_x < args.filter_width; filter_x++)           // W
    //     {                                                                             //
    //         for (size_t input_c = 0; input_c < args.input_channel; input_c++)         // C
    //         {                                                                         //
    //             for (size_t output_c = 0; output_c < args.output_channel; output_c++) // N
    //             {
    //                 buffer_ptr[output_c] += input_syx_dyx[input_c] * (*filter_element);
    //                 filter_element++;
    //             }
    //         }
    //         input_syx_dyx += args.input_dilation_x_offset;
    //     }
    //     input_syx_dy += args.input_dilation_y_offset;
    // }

    // filter in sequence [N, H, W, C]
    const filter_t *filter_element = (const filter_t *)args.filter_element;
    for (size_t output_c = 0; output_c < args.output_channel; output_c++)         // N
    {                                                                             //
        feature_t *input_syx_dy = input_ptr;                                      //
        buffer_t acc = 0;                                                         //
        for (size_t filter_y = 0; filter_y < args.filter_height; filter_y++)      // H
        {                                                                         //
            feature_t *input_syx_dyx = input_syx_dy;                              //
            for (size_t filter_x = 0; filter_x < args.filter_width; filter_x++)   // W
            {                                                                     //
                for (size_t input_c = 0; input_c < args.input_channel; input_c++) // C
                {
                    acc += input_syx_dyx[input_c] * (*filter_element++);
                }
                input_syx_dyx += args.input_dilation_x_offset;
            }
            filter_element += args.filter_y_offset;
            input_syx_dy += args.input_dilation_y_offset;
        }
        filter_element += args.filter_n_offset;
        buffer_ptr[output_c] = acc;
    }
}

#if DL_COMPILE_ALL || DL_KERNEL_DL_C_S16_CONV2D_11CN
template void conv2d_11cn<int16_t, DL_S16_BUFFER_TYPE, int16_t>(DL_S16_BUFFER_TYPE *, int16_t *, const ConvArgsType &);
#endif
#if DL_COMPILE_ALL || DL_KERNEL_DL_C_S16_CONV2D_33CN
template void conv2d_33cn<int16_t, DL_S16_BUFFER_TYPE, int16_t>(DL_S16_BUFFER_TYPE *, int16_t *, const ConvArgsType &);
#endif
#if DL_COMPILE_ALL || DL_KERNEL_DL_C_S16_CONV2D_HWCN
template void conv2d_hwcn<int16_t, DL_S16_BUFFER_TYPE, int16_t>(DL_S16_BUFFER_TYPE *, int16_t *, const ConvArgsType &);
#endif
#if DL_COMPILE_ALL || DL_KERNEL_DL_C_S8_CONV2D_11CN
template void conv2d_11cn<int8_t, int32_t, int8_t>(int32_t *, int8_t *, const ConvArgsType &);
#endif
#if DL_COMPILE_ALL || DL_KERNEL_DL_C_S8_CONV2D_33CN
template void conv2d_33cn<int8_t, int32_t, int8_t>(int32_t *, int8_t *, const ConvArgsType &);
#endif
#if DL_COMPILE_ALL || DL_KERNEL_DL_C_S8_CONV2D_HWCN
template void conv2d_hwcn<int8_t, int32_t, int8_t>(int32_t *, int8_t *, const ConvArgsType &);
#endif
#if DL_COMPILE_ALL || DL_KERNEL_DL_C_W8A16_CONV2D_11CN
template void conv2d_11cn<int16_t, DL_S16_BUFFER_TYPE, int8_t>(DL_S16_BUFFER_TYPE *, int16_t *, const ConvArgsType &);
#endif
#if DL_COMPILE_ALL || DL_KERNEL_DL_C_W8A16_CONV2D_33CN
template void conv2d_33cn<int16_t, DL_S16_BUFFER_TYPE, int8_t>(DL_S16_BUFFER_TYPE *, int16_t *, const ConvArgsType &);
#endif
#if DL_COMPILE_ALL || DL_KERNEL_DL_C_W8A16_CONV2D_HWCN
template void conv2d_hwcn<int16_t, DL_S16_BUFFER_TYPE, int8_t>(DL_S16_BUFFER_TYPE *, int16_t *, const ConvArgsType &);
#endif

} // namespace base
} // namespace dl
