/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GSC_CORE_TYPES_H_
#define _GSC_CORE_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	float	x;		//	left to right
	float	y;		//	near to far
	float	z;		//	low to high
} PlaneCoord;

typedef struct {
	float  rho;        ///< distance (metre)
	float  phi;        ///< horizontal angle (Radian), value range from 0 to 2*PI
	float  theta;      ///< vertical angle (Radian), value range from 0 to PI
} PolarCoord;

#ifdef __cplusplus
}
#endif

#endif /* _GSC_CORE_TYPES_H_ */
