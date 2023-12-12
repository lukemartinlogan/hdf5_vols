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
 * Purpose:     This is a "pass through" VOL connector, which forwards each
 *              VOL callback to an underlying connector.
 *
 *              It is designed as an example VOL connector for developers to
 *              use when creating new connectors, especially connectors that
 *              are outside of the HDF5 library.  As such, it should _NOT_
 *              include _any_ private HDF5 header files.  This connector should
 *              therefore only make public HDF5 API calls and use standard C /
 *              POSIX calls.
 *
 *              Note that the HDF5 error stack must be preserved on code paths
 *              that could be invoked when the underlying VOL connector's
 *              callback can fail.
 *
 */

/* Header files needed */
/* Do NOT include private HDF5 files here! */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "connector_helpers.h"

/* Public HDF5 file */
#include "hdf5.h"

/* This connector's header */
#include "H5VLreplicate_vol.h"

/**********/
/* Macros */
/**********/

/* Whether to display log message when callback is invoked */
/* (Uncomment to enable) */
/* #define ENABLE_PASSTHRU_LOGGING */

/* Hack for missing va_copy() in old Visual Studio editions
 * (from H5win2_defs.h - used on VS2012 and earlier)
 */
#if defined(_WIN32) && defined(_MSC_VER) && (_MSC_VER < 1800)
#define va_copy(D, S) ((D) = (S))
#endif

/************/
/* Typedefs */
/************/

/********************* */
/* Function prototypes */
/********************* */

/* "Management" callbacks */
static herr_t H5VL_replicate_vol_init(hid_t vipl_id);
static herr_t H5VL_replicate_vol_term(void);

/* VOL info callbacks */
static void  *H5VL_replicate_vol_info_copy(const void *info);
static herr_t H5VL_replicate_vol_info_cmp(int *cmp_value, const void *info1, const void *info2);
static herr_t H5VL_replicate_vol_info_free(void *info);
static herr_t H5VL_replicate_vol_to_str(const void *info, char **str);
static herr_t H5VL_replicate_vol_str_to_info(const char *str, void **info);

/* VOL object wrap / retrieval callbacks */
static void  *H5VL_replicate_vol_get_object(const void *obj);
static herr_t H5VL_replicate_vol_get_wrap_ctx(const void *obj, void **wrap_ctx);
static void  *H5VL_replicate_vol_wrap_object(void *obj, H5I_type_t obj_type, void *wrap_ctx);
static void  *H5VL_replicate_vol_unwrap_object(void *obj);
static herr_t H5VL_replicate_vol_free_wrap_ctx(void *obj);

/* Attribute callbacks */
static void  *H5VL_replicate_vol_attr_create(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                                             hid_t type_id, hid_t space_id, hid_t acpl_id, hid_t aapl_id,
                                             hid_t dxpl_id, void **req);
static void  *H5VL_replicate_vol_attr_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                                           hid_t aapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_attr_read(void *attr, hid_t mem_type_id, void *buf, hid_t dxpl_id,
                                           void **req);
static herr_t H5VL_replicate_vol_attr_write(void *attr, hid_t mem_type_id, const void *buf, hid_t dxpl_id,
                                            void **req);
static herr_t H5VL_replicate_vol_attr_get(void *obj, H5VL_attr_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_attr_specific(void *obj, const H5VL_loc_params_t *loc_params,
                                               H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_attr_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id,
                                               void **req);
static herr_t H5VL_replicate_vol_attr_close(void *attr, hid_t dxpl_id, void **req);

/* Dataset callbacks */
static void  *H5VL_replicate_vol_dataset_create(void *obj, const H5VL_loc_params_t *loc_params,
                                                const char *name, hid_t lcpl_id, hid_t type_id, hid_t space_id,
                                                hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id, void **req);
static void  *H5VL_replicate_vol_dataset_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                                              hid_t dapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_dataset_read(size_t count, void *dset[], hid_t mem_type_id[],
                                              hid_t mem_space_id[], hid_t file_space_id[], hid_t plist_id,
                                              void *buf[], void **req);
static herr_t H5VL_replicate_vol_dataset_write(size_t count, void *dset[], hid_t mem_type_id[],
                                               hid_t mem_space_id[], hid_t file_space_id[], hid_t plist_id,
                                               const void *buf[], void **req);
static herr_t H5VL_replicate_vol_dataset_get(void *dset, H5VL_dataset_get_args_t *args, hid_t dxpl_id,
                                             void **req);
static herr_t H5VL_replicate_vol_dataset_specific(void *obj, H5VL_dataset_specific_args_t *args, hid_t dxpl_id,
                                                  void **req);
static herr_t H5VL_replicate_vol_dataset_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id,
                                                  void **req);
static herr_t H5VL_replicate_vol_dataset_close(void *dset, hid_t dxpl_id, void **req);

/* Datatype callbacks */
static void *H5VL_replicate_vol_datatype_commit(void *obj, const H5VL_loc_params_t *loc_params,
                                                const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id,
                                                hid_t tapl_id, hid_t dxpl_id, void **req);
static void *H5VL_replicate_vol_datatype_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                                              hid_t tapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_datatype_get(void *dt, H5VL_datatype_get_args_t *args, hid_t dxpl_id,
                                              void **req);
static herr_t H5VL_replicate_vol_datatype_specific(void *obj, H5VL_datatype_specific_args_t *args,
                                                   hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_datatype_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id,
                                                   void **req);
static herr_t H5VL_replicate_vol_datatype_close(void *dt, hid_t dxpl_id, void **req);

/* File callbacks */
static void  *H5VL_replicate_vol_file_create(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id,
                                             hid_t dxpl_id, void **req);
static void  *H5VL_replicate_vol_file_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id,
                                           void **req);
static herr_t H5VL_replicate_vol_file_get(void *file, H5VL_file_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_file_specific(void *file, H5VL_file_specific_args_t *args, hid_t dxpl_id,
                                               void **req);
static herr_t H5VL_replicate_vol_file_optional(void *file, H5VL_optional_args_t *args, hid_t dxpl_id,
                                               void **req);
static herr_t H5VL_replicate_vol_file_close(void *file, hid_t dxpl_id, void **req);

/* Group callbacks */
static void  *H5VL_replicate_vol_group_create(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                                              hid_t lcpl_id, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id,
                                              void **req);
static void  *H5VL_replicate_vol_group_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                                            hid_t gapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_group_get(void *obj, H5VL_group_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_group_specific(void *obj, H5VL_group_specific_args_t *args, hid_t dxpl_id,
                                                void **req);
static herr_t H5VL_replicate_vol_group_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id,
                                                void **req);
static herr_t H5VL_replicate_vol_group_close(void *grp, hid_t dxpl_id, void **req);

/* Link callbacks */
static herr_t H5VL_replicate_vol_link_create(H5VL_link_create_args_t *args, void *obj,
                                             const H5VL_loc_params_t *loc_params, hid_t lcpl_id, hid_t lapl_id,
                                             hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_link_copy(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj,
                                           const H5VL_loc_params_t *loc_params2, hid_t lcpl_id, hid_t lapl_id,
                                           hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_link_move(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj,
                                           const H5VL_loc_params_t *loc_params2, hid_t lcpl_id, hid_t lapl_id,
                                           hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_link_get(void *obj, const H5VL_loc_params_t *loc_params,
                                          H5VL_link_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_link_specific(void *obj, const H5VL_loc_params_t *loc_params,
                                               H5VL_link_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_link_optional(void *obj, const H5VL_loc_params_t *loc_params,
                                               H5VL_optional_args_t *args, hid_t dxpl_id, void **req);

/* Object callbacks */
static void  *H5VL_replicate_vol_object_open(void *obj, const H5VL_loc_params_t *loc_params,
                                             H5I_type_t *opened_type, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_object_copy(void *src_obj, const H5VL_loc_params_t *src_loc_params,
                                             const char *src_name, void *dst_obj,
                                             const H5VL_loc_params_t *dst_loc_params, const char *dst_name,
                                             hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_object_get(void *obj, const H5VL_loc_params_t *loc_params,
                                            H5VL_object_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_object_specific(void *obj, const H5VL_loc_params_t *loc_params,
                                                 H5VL_object_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_replicate_vol_object_optional(void *obj, const H5VL_loc_params_t *loc_params,
                                                 H5VL_optional_args_t *args, hid_t dxpl_id, void **req);

/* Container/connector introspection callbacks */
static herr_t H5VL_replicate_vol_introspect_get_conn_cls(void *obj, H5VL_get_conn_lvl_t lvl,
                                                         const H5VL_class_t **conn_cls);
static herr_t H5VL_replicate_vol_introspect_get_cap_flags(const void *info, uint64_t *cap_flags);
static herr_t H5VL_replicate_vol_introspect_opt_query(void *obj, H5VL_subclass_t cls, int opt_type,
                                                      uint64_t *flags);

/* Async request callbacks */
static herr_t H5VL_replicate_vol_request_wait(void *req, uint64_t timeout, H5VL_request_status_t *status);
static herr_t H5VL_replicate_vol_request_notify(void *obj, H5VL_request_notify_t cb, void *ctx);
static herr_t H5VL_replicate_vol_request_cancel(void *req, H5VL_request_status_t *status);
static herr_t H5VL_replicate_vol_request_specific(void *req, H5VL_request_specific_args_t *args);
static herr_t H5VL_replicate_vol_request_optional(void *req, H5VL_optional_args_t *args);
static herr_t H5VL_replicate_vol_request_free(void *req);

/* Blob callbacks */
static herr_t H5VL_replicate_vol_blob_put(void *obj, const void *buf, size_t size, void *blob_id, void *ctx);
static herr_t H5VL_replicate_vol_blob_get(void *obj, const void *blob_id, void *buf, size_t size, void *ctx);
static herr_t H5VL_replicate_vol_blob_specific(void *obj, void *blob_id, H5VL_blob_specific_args_t *args);
static herr_t H5VL_replicate_vol_blob_optional(void *obj, void *blob_id, H5VL_optional_args_t *args);

/* Token callbacks */
static herr_t H5VL_replicate_vol_token_cmp(void *obj, const H5O_token_t *token1, const H5O_token_t *token2,
                                           int *cmp_value);
static herr_t H5VL_replicate_vol_token_to_str(void *obj, H5I_type_t obj_type, const H5O_token_t *token,
                                              char **token_str);
static herr_t H5VL_replicate_vol_token_from_str(void *obj, H5I_type_t obj_type, const char *token_str,
                                                H5O_token_t *token);

/* Generic optional callback */
static herr_t H5VL_replicate_vol_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);

/*******************/
/* Local variables */
/*******************/

/* Pass through VOL connector class struct */
static const H5VL_class_t H5VL_replicate_vol_g = {
    H5VL_VERSION,                            /* VOL class struct version */
    (H5VL_class_value_t)H5VL_REPLICATE_VOL_VALUE, /* value        */
    H5VL_REPLICATE_VOL_NAME,                      /* name         */
    H5VL_REPLICATE_VOL_VERSION,                   /* connector version */
    0,                                       /* capability flags */
    H5VL_replicate_vol_init,                  /* initialize   */
    H5VL_replicate_vol_term,                  /* terminate    */
    {
        /* info_cls */
        sizeof(H5VL_replicate_vol_t), /* size    */
        H5VL_replicate_vol_info_copy,      /* copy    */
        H5VL_replicate_vol_info_cmp,       /* compare */
        H5VL_replicate_vol_info_free,      /* free    */
        H5VL_replicate_vol_to_str,    /* to_str  */
        H5VL_replicate_vol_str_to_info     /* from_str */
    },
    {
        /* wrap_cls */
        H5VL_replicate_vol_get_object,    /* get_object   */
        H5VL_replicate_vol_get_wrap_ctx,  /* get_wrap_ctx */
        H5VL_replicate_vol_wrap_object,   /* wrap_object  */
        H5VL_replicate_vol_unwrap_object, /* unwrap_object */
        H5VL_replicate_vol_free_wrap_ctx  /* free_wrap_ctx */
    },
    {
        /* attribute_cls */
        H5VL_replicate_vol_attr_create,   /* create */
        H5VL_replicate_vol_attr_open,     /* open */
        H5VL_replicate_vol_attr_read,     /* read */
        H5VL_replicate_vol_attr_write,    /* write */
        H5VL_replicate_vol_attr_get,      /* get */
        H5VL_replicate_vol_attr_specific, /* specific */
        H5VL_replicate_vol_attr_optional, /* optional */
        H5VL_replicate_vol_attr_close     /* close */
    },
    {
        /* dataset_cls */
        H5VL_replicate_vol_dataset_create,   /* create */
        H5VL_replicate_vol_dataset_open,     /* open */
        H5VL_replicate_vol_dataset_read,     /* read */
        H5VL_replicate_vol_dataset_write,    /* write */
        H5VL_replicate_vol_dataset_get,      /* get */
        H5VL_replicate_vol_dataset_specific, /* specific */
        H5VL_replicate_vol_dataset_optional, /* optional */
        H5VL_replicate_vol_dataset_close     /* close */
    },
    {
        /* datatype_cls */
        H5VL_replicate_vol_datatype_commit,   /* commit */
        H5VL_replicate_vol_datatype_open,     /* open */
        H5VL_replicate_vol_datatype_get,      /* get_size */
        H5VL_replicate_vol_datatype_specific, /* specific */
        H5VL_replicate_vol_datatype_optional, /* optional */
        H5VL_replicate_vol_datatype_close     /* close */
    },
    {
        /* file_cls */
        H5VL_replicate_vol_file_create,   /* create */
        H5VL_replicate_vol_file_open,     /* open */
        H5VL_replicate_vol_file_get,      /* get */
        H5VL_replicate_vol_file_specific, /* specific */
        H5VL_replicate_vol_file_optional, /* optional */
        H5VL_replicate_vol_file_close     /* close */
    },
    {
        /* group_cls */
        H5VL_replicate_vol_group_create,   /* create */
        H5VL_replicate_vol_group_open,     /* open */
        H5VL_replicate_vol_group_get,      /* get */
        H5VL_replicate_vol_group_specific, /* specific */
        H5VL_replicate_vol_group_optional, /* optional */
        H5VL_replicate_vol_group_close     /* close */
    },
    {
        /* link_cls */
        H5VL_replicate_vol_link_create,   /* create */
        H5VL_replicate_vol_link_copy,     /* copy */
        H5VL_replicate_vol_link_move,     /* move */
        H5VL_replicate_vol_link_get,      /* get */
        H5VL_replicate_vol_link_specific, /* specific */
        H5VL_replicate_vol_link_optional  /* optional */
    },
    {
        /* object_cls */
        H5VL_replicate_vol_object_open,     /* open */
        H5VL_replicate_vol_object_copy,     /* copy */
        H5VL_replicate_vol_object_get,      /* get */
        H5VL_replicate_vol_object_specific, /* specific */
        H5VL_replicate_vol_object_optional  /* optional */
    },
    {
        /* introspect_cls */
        H5VL_replicate_vol_introspect_get_conn_cls,  /* get_conn_cls */
        H5VL_replicate_vol_introspect_get_cap_flags, /* get_cap_flags */
        H5VL_replicate_vol_introspect_opt_query,     /* opt_query */
    },
    {
        /* request_cls */
        H5VL_replicate_vol_request_wait,     /* wait */
        H5VL_replicate_vol_request_notify,   /* notify */
        H5VL_replicate_vol_request_cancel,   /* cancel */
        H5VL_replicate_vol_request_specific, /* specific */
        H5VL_replicate_vol_request_optional, /* optional */
        H5VL_replicate_vol_request_free      /* free */
    },
    {
        /* blob_cls */
        H5VL_replicate_vol_blob_put,      /* put */
        H5VL_replicate_vol_blob_get,      /* get */
        H5VL_replicate_vol_blob_specific, /* specific */
        H5VL_replicate_vol_blob_optional  /* optional */
    },
    {
        /* token_cls */
        H5VL_replicate_vol_token_cmp,     /* cmp */
        H5VL_replicate_vol_token_to_str,  /* to_str */
        H5VL_replicate_vol_token_from_str /* from_str */
    },
    H5VL_replicate_vol_optional /* optional */
};

H5PL_type_t
H5PLget_plugin_type(void) {
  return H5PL_TYPE_VOL;
}

const void*
H5PLget_plugin_info(void) {
  return &H5VL_replicate_vol_g;
}

/* The connector identification number, initialized at runtime */
static hid_t H5VL_REPLICATE_VOL_g = H5I_INVALID_HID;

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_register
 *
 * Purpose:     Register the pass-through VOL connector and retrieve an ID
 *              for it.
 *
 * Return:      Success:    The ID for the pass-through VOL connector
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL_replicate_vol_register(void)
{
  /* Singleton register the pass-through VOL connector ID */
  if (H5VL_REPLICATE_VOL_g < 0)
    H5VL_REPLICATE_VOL_g = H5VLregister_connector(&H5VL_replicate_vol_g, H5P_DEFAULT);

  return H5VL_REPLICATE_VOL_g;
} /* end H5VL_replicate_vol_register() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_init
 *
 * Purpose:     Initialize this VOL connector, performing any necessary
 *              operations for the connector that will apply to all containers
 *              accessed with the connector.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_init(hid_t vipl_id)
{
#ifdef ENABLE_PASSTHRU_LOGGING
  printf("------- PASS THROUGH VOL INIT\n");
#endif

  /* Shut compiler up about unused parameter */
  (void)vipl_id;

  return 0;
} /* end H5VL_replicate_vol_init() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_term
 *
 * Purpose:     Terminate this VOL connector, performing any necessary
 *              operations for the connector that release connector-wide
 *              resources (usually created / initialized with the 'init'
 *              callback).
 *
 * Return:      Success:    0
 *              Failure:    (Can't fail)
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_term(void)
{
#ifdef ENABLE_PASSTHRU_LOGGING
  printf("------- PASS THROUGH VOL TERM\n");
#endif

  /* Reset VOL ID */
  H5VL_REPLICATE_VOL_g = H5I_INVALID_HID;

  return 0;
} /* end H5VL_replicate_vol_term() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_info_copy
 *
 * Purpose:     Duplicate the connector's info object.
 *
 * Returns:     Success:    New connector info object
 *              Failure:    NULL
 *
 *---------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_info_copy(const void *_info)
{
  return new H5VL_replicate_vol_t(*(H5VL_replicate_vol_t*)_info);
} /* end H5VL_replicate_vol_info_copy() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_info_cmp
 *
 * Purpose:     Compare two of the connector's info objects, setting *cmp_value,
 *              following the same rules as strcmp().
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_info_cmp(int *cmp_value, const void *_info1, const void *_info2)
{
  return 0;
} /* end H5VL_replicate_vol_info_cmp() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_info_free
 *
 * Purpose:     Release an info object for the connector.
 *
 * Note:	Take care to preserve the current HDF5 error stack
 *		when calling HDF5 API calls.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_info_free(void *_info)
{
  return 0;
} /* end H5VL_replicate_vol_info_free() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_to_str
 *
 * Purpose:     Serialize an info object for this connector into a string
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_to_str(const void *_info, char **str)
{
  return 0;
} /* end H5VL_replicate_vol_to_str() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_str_to_info
 *
 * Purpose:     Deserialize a string into an info object for this connector.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_str_to_info(const char *str, void **_info)
{
  H5VL_replicate_vol_t *info = (H5VL_replicate_vol_t*)calloc(sizeof(H5VL_replicate_vol_t), 1);
  h5::ParseConn parser;
  parser.parse(str);
  std::string next_vol_name = parser.GetNextVolName();
  std::string next_vol_params = parser.GetNextVolParams();
  info->next_vol_id_[0] = H5VLregister_connector_by_name(next_vol_name.c_str(), H5P_DEFAULT);
  if (next_vol_params.size()) {
    H5VLconnector_str_to_info(next_vol_params.c_str(), info->next_vol_id_[0], (void **) &info->next_vol_info_[0]);
  }

  /* Set return value */
  *_info = info;

  return 0;
} /* end H5VL_replicate_vol_str_to_info() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_get_object
 *
 * Purpose:     Retrieve the 'data' for a VOL object.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_get_object(const void *obj)
{
  const H5VL_replicate_vol_t *o = (const H5VL_replicate_vol_t *)obj;

#ifdef ENABLE_PASSTHRU_LOGGING
  printf("------- PASS THROUGH VOL Get object\n");
#endif

  return H5VLget_object(o->next_vol_info_[0], o->next_vol_id_[0]);
} /* end H5VL_replicate_vol_get_object() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_get_wrap_ctx
 *
 * Purpose:     Retrieve a "wrapper context" for an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_get_wrap_ctx(const void *obj, void **wrap_ctx)
{
  return 0;
} /* end H5VL_replicate_vol_get_wrap_ctx() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_wrap_object
 *
 * Purpose:     Use a "wrapper context" to wrap a data object
 *
 * Return:      Success:    Pointer to wrapped object
 *              Failure:    NULL
 *
 *---------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_wrap_object(void *obj, H5I_type_t obj_type, void *_wrap_ctx)
{
  return nullptr;
} /* end H5VL_replicate_vol_wrap_object() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_unwrap_object
 *
 * Purpose:     Unwrap a wrapped object, discarding the wrapper, but returning
 *		underlying object.
 *
 * Return:      Success:    Pointer to unwrapped object
 *              Failure:    NULL
 *
 *---------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_unwrap_object(void *obj)
{
  return nullptr;
} /* end H5VL_replicate_vol_unwrap_object() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_free_wrap_ctx
 *
 * Purpose:     Release a "wrapper context" for an object
 *
 * Note:	Take care to preserve the current HDF5 error stack
 *		when calling HDF5 API calls.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_free_wrap_ctx(void *_wrap_ctx)
{
  return 0;
} /* end H5VL_replicate_vol_free_wrap_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_attr_create
 *
 * Purpose:     Creates an attribute on an object.
 *
 * Return:      Success:    Pointer to attribute object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_attr_create(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t type_id,
                               hid_t space_id, hid_t acpl_id, hid_t aapl_id, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_attr_create() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_attr_open
 *
 * Purpose:     Opens an attribute on an object.
 *
 * Return:      Success:    Pointer to attribute object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_attr_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t aapl_id,
                             hid_t dxpl_id, void **req)
{
  return nullptr;
} /* end H5VL_replicate_vol_attr_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_attr_read
 *
 * Purpose:     Reads data from attribute.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_attr_read(void *attr, hid_t mem_type_id, void *buf, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_attr_read() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_attr_write
 *
 * Purpose:     Writes data to attribute.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_attr_write(void *attr, hid_t mem_type_id, const void *buf, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_attr_write() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_attr_get
 *
 * Purpose:     Gets information about an attribute
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_attr_get(void *obj, H5VL_attr_get_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_attr_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_attr_specific
 *
 * Purpose:     Specific operation on attribute
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_attr_specific(void *obj, const H5VL_loc_params_t *loc_params,
                                 H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_attr_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_attr_optional
 *
 * Purpose:     Perform a connector-specific operation on an attribute
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_attr_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_attr_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_attr_close
 *
 * Purpose:     Closes an attribute.
 *
 * Return:      Success:    0
 *              Failure:    -1, attr not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_attr_close(void *attr, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_attr_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_dataset_create
 *
 * Purpose:     Creates a dataset in a container
 *
 * Return:      Success:    Pointer to a dataset object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_dataset_create(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                                  hid_t lcpl_id, hid_t type_id, hid_t space_id, hid_t dcpl_id, hid_t dapl_id,
                                  hid_t dxpl_id, void **req)
{
  H5VL_replicate_vol_t *o = (H5VL_replicate_vol_t *)obj;
  H5VL_replicate_vol_t *new_obj = new H5VL_replicate_vol_t(*o);
  if (o->next_vol_info_[0] != nullptr) {
    new_obj->next_vol_info_[0] = H5VLdataset_create(o->next_vol_info_[0], loc_params,
                                                    o->next_vol_id_[0], name, lcpl_id, type_id, space_id,
                                                    dcpl_id, dapl_id, dxpl_id, req);
  } else {
    new_obj->next_vol_info_[0] = H5VLdataset_create((void*)32, loc_params,
                                                    o->next_vol_id_[0], name, lcpl_id, type_id, space_id,
                                                    dcpl_id, dapl_id, dxpl_id, req);
  }
  return new_obj;
} /* end H5VL_replicate_vol_dataset_create() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_dataset_open
 *
 * Purpose:     Opens a dataset in a container
 *
 * Return:      Success:    Pointer to a dataset object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_dataset_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                                hid_t dapl_id, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_dataset_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_dataset_read
 *
 * Purpose:     Reads data elements from a dataset into a buffer.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_dataset_read(size_t count, void *dset[], hid_t mem_type_id[], hid_t mem_space_id[],
                                hid_t file_space_id[], hid_t plist_id, void *buf[], void **req)
{
  std::vector<H5VL_replicate_vol_t*> obj(count);
  size_t i;                /* Local index variable */
  herr_t ret_value;

  /* Allocate obj array if necessary */
  for (i = 0; i < count; i++) {
    /* Get the object */
    obj[i] = (H5VL_replicate_vol_t*)((H5VL_replicate_vol_t*)dset[i])->next_vol_info_;

    /* Make sure the class matches */
    if (((H5VL_replicate_vol_t*) dset[i])->next_vol_id_[0] != ((H5VL_replicate_vol_t*) dset[0])->next_vol_id_[0])
      return -1;
  }

  ret_value = H5VLdataset_read(count, (void**)obj.data(), ((H5VL_replicate_vol_t*)dset[0])->next_vol_id_[0], mem_type_id,
                               mem_space_id, file_space_id, plist_id, buf, req);

  return ret_value;
} /* end H5VL_replicate_vol_dataset_read() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_dataset_write
 *
 * Purpose:     Writes data elements from a buffer into a dataset.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_dataset_write(size_t count, void *dset[], hid_t mem_type_id[], hid_t mem_space_id[],
                                 hid_t file_space_id[], hid_t plist_id, const void *buf[], void **req)
{
  std::vector<H5VL_replicate_vol_t*> obj(count);
  size_t i;                /* Local index variable */
  herr_t ret_value;

  /* Allocate obj array if necessary */
  for (i = 0; i < count; i++) {
    /* Get the object */
    obj[i] = (H5VL_replicate_vol_t*)((H5VL_replicate_vol_t*)dset[i])->next_vol_info_;

    /* Make sure the class matches */
    if (((H5VL_replicate_vol_t*) dset[i])->next_vol_id_ != ((H5VL_replicate_vol_t*) dset[0])->next_vol_id_)
      return -1;
  }

  ret_value = H5VLdataset_write(count, (void**)obj.data(), ((H5VL_replicate_vol_t*)dset[0])->next_vol_id_[0], mem_type_id,
                                mem_space_id, file_space_id, plist_id, buf, req);

  return ret_value;
} /* end H5VL_replicate_vol_dataset_write() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_dataset_get
 *
 * Purpose:     Gets information about a dataset
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_dataset_get(void *dset, H5VL_dataset_get_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_dataset_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_dataset_specific
 *
 * Purpose:     Specific operation on a dataset
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_dataset_specific(void *obj, H5VL_dataset_specific_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_dataset_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_dataset_optional
 *
 * Purpose:     Perform a connector-specific operation on a dataset
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_dataset_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_dataset_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_dataset_close
 *
 * Purpose:     Closes a dataset.
 *
 * Return:      Success:    0
 *              Failure:    -1, dataset not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_dataset_close(void *dset, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_dataset_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_datatype_commit
 *
 * Purpose:     Commits a datatype inside a container.
 *
 * Return:      Success:    Pointer to datatype object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_datatype_commit(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                                   hid_t type_id, hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id, hid_t dxpl_id,
                                   void **req)
{
  return 0;
} /* end H5VL_replicate_vol_datatype_commit() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_datatype_open
 *
 * Purpose:     Opens a named datatype inside a container.
 *
 * Return:      Success:    Pointer to datatype object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_datatype_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                                 hid_t tapl_id, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_datatype_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_datatype_get
 *
 * Purpose:     Get information about a datatype
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_datatype_get(void *dt, H5VL_datatype_get_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_datatype_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_datatype_specific
 *
 * Purpose:     Specific operations for datatypes
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_datatype_specific(void *obj, H5VL_datatype_specific_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_datatype_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_datatype_optional
 *
 * Purpose:     Perform a connector-specific operation on a datatype
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_datatype_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_datatype_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_datatype_close
 *
 * Purpose:     Closes a datatype.
 *
 * Return:      Success:    0
 *              Failure:    -1, datatype not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_datatype_close(void *dt, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_datatype_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_file_create
 *
 * Purpose:     Creates a container using this connector
 *
 * Return:      Success:    Pointer to a file object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_file_create(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t dxpl_id,
                               void **req)
{
  H5VL_replicate_vol_t *file = new H5VL_replicate_vol_t(), *info;
  H5Pget_vol_info(fapl_id, (void **)&info);
  (*file) = (*info);
  hid_t under_fapl_id = H5Pcopy(fapl_id);
  H5Pset_vol(under_fapl_id, info->next_vol_id_[0], info->next_vol_info_[0]);
  H5VLfile_create(name, flags, fcpl_id, under_fapl_id, dxpl_id, req);
  H5Pclose(under_fapl_id);
  return file;
} /* end H5VL_replicate_vol_file_create() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_file_open
 *
 * Purpose:     Opens a container created with this connector
 *
 * Return:      Success:    Pointer to a file object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_file_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_file_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_file_get
 *
 * Purpose:     Get info about a file
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_file_get(void *file, H5VL_file_get_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_file_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_file_specific
 *
 * Purpose:     Specific operation on file
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_file_specific(void *file, H5VL_file_specific_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_file_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_file_optional
 *
 * Purpose:     Perform a connector-specific operation on a file
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_file_optional(void *file, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_file_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_file_close
 *
 * Purpose:     Closes a file.
 *
 * Return:      Success:    0
 *              Failure:    -1, file not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_file_close(void *file, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_file_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_group_create
 *
 * Purpose:     Creates a group inside a container
 *
 * Return:      Success:    Pointer to a group object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_group_create(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                                hid_t lcpl_id, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_group_create() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_group_open
 *
 * Purpose:     Opens a group inside a container
 *
 * Return:      Success:    Pointer to a group object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_group_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t gapl_id,
                              hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_group_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_group_get
 *
 * Purpose:     Get info about a group
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_group_get(void *obj, H5VL_group_get_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_group_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_group_specific
 *
 * Purpose:     Specific operation on a group
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_group_specific(void *obj, H5VL_group_specific_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_group_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_group_optional
 *
 * Purpose:     Perform a connector-specific operation on a group
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_group_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_group_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_group_close
 *
 * Purpose:     Closes a group.
 *
 * Return:      Success:    0
 *              Failure:    -1, group not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_group_close(void *grp, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_group_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_link_create
 *
 * Purpose:     Creates a hard / soft / UD / external link.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_link_create(H5VL_link_create_args_t *args, void *obj, const H5VL_loc_params_t *loc_params,
                               hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_link_create() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_link_copy
 *
 * Purpose:     Renames an object within an HDF5 container and copies it to a new
 *              group.  The original name SRC is unlinked from the group graph
 *              and then inserted with the new name DST (which can specify a
 *              new path for the object) as an atomic operation. The names
 *              are interpreted relative to SRC_LOC_ID and
 *              DST_LOC_ID, which are either file IDs or group ID.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_link_copy(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj,
                             const H5VL_loc_params_t *loc_params2, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id,
                             void **req)
{
  return 0;
} /* end H5VL_replicate_vol_link_copy() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_link_move
 *
 * Purpose:     Moves a link within an HDF5 file to a new group.  The original
 *              name SRC is unlinked from the group graph
 *              and then inserted with the new name DST (which can specify a
 *              new path for the object) as an atomic operation. The names
 *              are interpreted relative to SRC_LOC_ID and
 *              DST_LOC_ID, which are either file IDs or group ID.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_link_move(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj,
                             const H5VL_loc_params_t *loc_params2, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id,
                             void **req)
{
  return 0;
} /* end H5VL_replicate_vol_link_move() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_link_get
 *
 * Purpose:     Get info about a link
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_link_get(void *obj, const H5VL_loc_params_t *loc_params, H5VL_link_get_args_t *args,
                            hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_link_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_link_specific
 *
 * Purpose:     Specific operation on a link
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_link_specific(void *obj, const H5VL_loc_params_t *loc_params,
                                 H5VL_link_specific_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_link_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_link_optional
 *
 * Purpose:     Perform a connector-specific operation on a link
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_link_optional(void *obj, const H5VL_loc_params_t *loc_params, H5VL_optional_args_t *args,
                                 hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_link_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_object_open
 *
 * Purpose:     Opens an object inside a container.
 *
 * Return:      Success:    Pointer to object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_replicate_vol_object_open(void *obj, const H5VL_loc_params_t *loc_params, H5I_type_t *opened_type,
                               hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_object_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_object_copy
 *
 * Purpose:     Copies an object inside a container.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_object_copy(void *src_obj, const H5VL_loc_params_t *src_loc_params, const char *src_name,
                               void *dst_obj, const H5VL_loc_params_t *dst_loc_params, const char *dst_name,
                               hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_object_copy() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_object_get
 *
 * Purpose:     Get info about an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_object_get(void *obj, const H5VL_loc_params_t *loc_params, H5VL_object_get_args_t *args,
                              hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_object_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_object_specific
 *
 * Purpose:     Specific operation on an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_object_specific(void *obj, const H5VL_loc_params_t *loc_params,
                                   H5VL_object_specific_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_object_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_object_optional
 *
 * Purpose:     Perform a connector-specific operation for an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_object_optional(void *obj, const H5VL_loc_params_t *loc_params, H5VL_optional_args_t *args,
                                   hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_object_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_introspect_get_conn_cls
 *
 * Purpose:     Query the connector class.
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_replicate_vol_introspect_get_conn_cls(void *obj, H5VL_get_conn_lvl_t lvl, const H5VL_class_t **conn_cls)
{
  return 0;
} /* end H5VL_replicate_vol_introspect_get_conn_cls() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_introspect_get_cap_flags
 *
 * Purpose:     Query the capability flags for this connector and any
 *              underlying connector(s).
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_replicate_vol_introspect_get_cap_flags(const void *_info, uint64_t *cap_flags)
{
  return 0;
} /* end H5VL_replicate_vol_introspect_get_cap_flags() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_introspect_opt_query
 *
 * Purpose:     Query if an optional operation is supported by this connector
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_replicate_vol_introspect_opt_query(void *obj, H5VL_subclass_t cls, int opt_type, uint64_t *flags)
{
  return 0;
} /* end H5VL_replicate_vol_introspect_opt_query() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_request_wait
 *
 * Purpose:     Wait (with a timeout) for an async operation to complete
 *
 * Note:        Releases the request if the operation has completed and the
 *              connector callback succeeds
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_request_wait(void *obj, uint64_t timeout, H5VL_request_status_t *status)
{
  return 0;
} /* end H5VL_replicate_vol_request_wait() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_request_notify
 *
 * Purpose:     Registers a user callback to be invoked when an asynchronous
 *              operation completes
 *
 * Note:        Releases the request, if connector callback succeeds
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_request_notify(void *obj, H5VL_request_notify_t cb, void *ctx)
{
  return 0;
} /* end H5VL_replicate_vol_request_notify() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_request_cancel
 *
 * Purpose:     Cancels an asynchronous operation
 *
 * Note:        Releases the request, if connector callback succeeds
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_request_cancel(void *obj, H5VL_request_status_t *status)
{
  return 0;
} /* end H5VL_replicate_vol_request_cancel() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_request_specific
 *
 * Purpose:     Specific operation on a request
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_request_specific(void *obj, H5VL_request_specific_args_t *args)
{
  return 0;
} /* end H5VL_replicate_vol_request_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_request_optional
 *
 * Purpose:     Perform a connector-specific operation for a request
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_request_optional(void *obj, H5VL_optional_args_t *args)
{
  return 0;
} /* end H5VL_replicate_vol_request_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_request_free
 *
 * Purpose:     Releases a request, allowing the operation to complete without
 *              application tracking
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_request_free(void *obj)
{
  return 0;
} /* end H5VL_replicate_vol_request_free() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_blob_put
 *
 * Purpose:     Handles the blob 'put' callback
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_replicate_vol_blob_put(void *obj, const void *buf, size_t size, void *blob_id, void *ctx)
{
  return 0;
} /* end H5VL_replicate_vol_blob_put() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_blob_get
 *
 * Purpose:     Handles the blob 'get' callback
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_replicate_vol_blob_get(void *obj, const void *blob_id, void *buf, size_t size, void *ctx)
{
  return 0;
} /* end H5VL_replicate_vol_blob_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_blob_specific
 *
 * Purpose:     Handles the blob 'specific' callback
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_replicate_vol_blob_specific(void *obj, void *blob_id, H5VL_blob_specific_args_t *args)
{
  return 0;
} /* end H5VL_replicate_vol_blob_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_blob_optional
 *
 * Purpose:     Handles the blob 'optional' callback
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_replicate_vol_blob_optional(void *obj, void *blob_id, H5VL_optional_args_t *args)
{
  return 0;
} /* end H5VL_replicate_vol_blob_optional() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_token_cmp
 *
 * Purpose:     Compare two of the connector's object tokens, setting
 *              *cmp_value, following the same rules as strcmp().
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_token_cmp(void *obj, const H5O_token_t *token1, const H5O_token_t *token2, int *cmp_value)
{
  return 0;
} /* end H5VL_replicate_vol_token_cmp() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_token_to_str
 *
 * Purpose:     Serialize the connector's object token into a string.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_token_to_str(void *obj, H5I_type_t obj_type, const H5O_token_t *token, char **token_str)
{
  return 0;
} /* end H5VL_replicate_vol_token_to_str() */

/*---------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_token_from_str
 *
 * Purpose:     Deserialize the connector's object token from a string.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_replicate_vol_token_from_str(void *obj, H5I_type_t obj_type, const char *token_str, H5O_token_t *token)
{
  return 0;
} /* end H5VL_replicate_vol_token_from_str() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_replicate_vol_optional
 *
 * Purpose:     Handles the generic 'optional' callback
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_replicate_vol_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
  return 0;
} /* end H5VL_replicate_vol_optional() */
