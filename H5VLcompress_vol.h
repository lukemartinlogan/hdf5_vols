/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://www.hdfgroup.org/licenses.               *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Purpose:	The public header file for the pass-through VOL connector.
 */

#ifndef H5VLcompress_vol_H
#define H5VLcompress_vol_H

/* Public headers needed by this file */
#include "H5VLpublic.h" /* Virtual Object Layer                 */

/* Identifier for the pass-through VOL connector */
#define H5VL_HERMES_PASSTHRU (H5VL_hermes_passthru_register())

/* Characteristics of the pass-through VOL connector */
#define H5VL_COMPRESS_VOL_NAME    "compress_vol"
#define H5VL_COMPRESS_VOL_VALUE   4 /* VOL connector ID */
#define H5VL_COMPRESS_VOL_VERSION 0

/* Pass-through VOL connector info */
typedef struct H5VL_compress_vol_t {
    hid_t next_vol_id_;       /* VOL ID for under VOL */
    void *next_vol_info_;     /* VOL info for under VOL */
} H5VL_compress_vol_t;

#ifdef __cplusplus
extern "C" {
#endif

H5_DLL hid_t H5VL_hermes_passthru_register(void);
H5_DLL const void* H5PLget_plugin_info(void);
H5_DLL H5PL_type_t H5PLget_plugin_type(void);

#ifdef __cplusplus
}
#endif

#endif /* H5VLcompress_vol_H */
