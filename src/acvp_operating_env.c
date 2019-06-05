/** @file */
/*
 * Copyright (c) 2019, Cisco Systems, Inc.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://github.com/cisco/libacvp/LICENSE
 */

#include <stdlib.h>

#include "acvp.h"
#include "acvp_lcl.h"
#include "parson.h"
#include "safe_lib.h"

/* Keeps track of what to use the next Dependency ID */
static unsigned int glb_dependency_id = 1; 

static ACVP_RESULT copy_oe_string(char **dest, const char *src) {
    if (dest == NULL) {
        return ACVP_MISSING_ARG;
    }
    if (src == NULL) {
        return ACVP_MISSING_ARG;
    }
    if (!string_fits(src, ACVP_OE_STR_MAX)) {
        return ACVP_INVALID_ARG;
    }

    if (*dest) { 
        memzero_s(*dest, ACVP_OE_STR_MAX + 1);
    } else {
        *dest = calloc(ACVP_OE_STR_MAX + 1, sizeof(char));
    }
    strcpy_s(*dest, ACVP_OE_STR_MAX + 1, src);

    return ACVP_SUCCESS;
}

static ACVP_DEPENDENCY *find_dependency(ACVP_CTX *ctx,
                                        unsigned int id) {
    ACVP_DEPENDENCIES *dependencies = NULL;
    int k = 0;

    if (!ctx) return NULL;

    /* Get a handle on the Dependencies */
    dependencies = &ctx->op_env.dependencies;

    if (id == 0) {
        ACVP_LOG_ERR("Invalid 'id', must be non-zero");
        return NULL;
    }
    for (k = 0; k < dependencies->count; k++) {
        if (id == dependencies->deps[k].id) {
            /* Match */
            return &dependencies->deps[k];
        }
    }

    ACVP_LOG_ERR("Invalid 'id' (%u)", id);
    return NULL;
}

/**
 * @brief Designate a new Dependency entry for this session.
 *
 * @return non-zero value representing the "dependency_id"
 * @return 0 fail
 */
ACVP_RESULT acvp_oe_dependency_new(ACVP_CTX *ctx, unsigned int id) {
    ACVP_DEPENDENCIES *dependencies = NULL;
    ACVP_DEPENDENCY *new_dep = NULL;
    int k = 0;

    if (!ctx) return ACVP_NO_CTX;

    /* Get a handle on the Dependencies */
    dependencies = &ctx->op_env.dependencies;

    if (dependencies->count == LIBACVP_DEPENDENCIES_MAX) {
        ACVP_LOG_ERR("Libacvp already reached max Dependency capacity (%u)",
                     LIBACVP_DEPENDENCIES_MAX);
        return 0;
    }

    if (!id) {
        ACVP_LOG_ERR("Required parameter 'id' must be non-zero");
        return ACVP_INVALID_ARG;
    }

    for (k = 0; k < dependencies->count; k++) {
        if (id == dependencies->deps[k].id) {
            ACVP_LOG_ERR("A Dependency already exists with this same 'id'(%d)", id);
            return ACVP_INVALID_ARG;
        }
    }

    new_dep = &dependencies->deps[dependencies->count];
    dependencies->count++;

    /* Set the ID */
    new_dep->id = id;

    return ACVP_SUCCESS;
}

typedef enum dependency_field {
    DEPENDENCY_FIELD_TYPE = 1,
    DEPENDENCY_FIELD_NAME,
    DEPENDENCY_FIELD_DESC
} DEPENDENCY_FIELD;

static ACVP_RESULT acvp_oe_dependency_set_field(ACVP_CTX *ctx,
                                                ACVP_DEPENDENCY *dep,
                                                DEPENDENCY_FIELD field,
                                                const char *value) {
    ACVP_RESULT rv = 0;

    if (!ctx) return ACVP_NO_CTX;

    if (dep == NULL) {
        ACVP_LOG_ERR("Required parameter 'dep' is NULL");
        return ACVP_INVALID_ARG;
    }

    if (DEPENDENCY_FIELD_TYPE == field) {
        rv = copy_oe_string(&dep->type, value);
    } else if (DEPENDENCY_FIELD_NAME == field) {
        rv = copy_oe_string(&dep->name, value);
    } else if (DEPENDENCY_FIELD_DESC == field) {
        rv = copy_oe_string(&dep->description, value);
    } else {
        ACVP_LOG_ERR("Invalid value for parameter 'field'");
        return ACVP_INVALID_ARG;
    }

    if (ACVP_INVALID_ARG == rv) {
        ACVP_LOG_ERR("'value' string too long");
        return rv;
    }
    if (ACVP_MISSING_ARG == rv) {
        ACVP_LOG_ERR("Required parameter 'value` is NULL");
        return rv;
    }

    return ACVP_SUCCESS; 
}

ACVP_RESULT acvp_oe_dependency_set_type(ACVP_CTX *ctx,
                                        unsigned int dependency_id,
                                        const char *value) {
    ACVP_DEPENDENCY *dep = NULL;

    if (!ctx) return ACVP_NO_CTX;

    if (!(dep = find_dependency(ctx, dependency_id))) {
        return ACVP_INVALID_ARG;
    }

    return acvp_oe_dependency_set_field(ctx, dep, DEPENDENCY_FIELD_TYPE, value);
}

ACVP_RESULT acvp_oe_dependency_set_name(ACVP_CTX *ctx,
                                        unsigned int dependency_id,
                                        const char *value) {
    ACVP_DEPENDENCY *dep = NULL;

    if (!ctx) return ACVP_NO_CTX;

    if (!(dep = find_dependency(ctx, dependency_id))) {
        return ACVP_INVALID_ARG;
    }

    return acvp_oe_dependency_set_field(ctx, dep, DEPENDENCY_FIELD_NAME, value);
}

ACVP_RESULT acvp_oe_dependency_set_description(ACVP_CTX *ctx,
                                               unsigned int dependency_id,
                                               const char *value) {
    ACVP_DEPENDENCY *dep = NULL;

    if (!ctx) return ACVP_NO_CTX;

    if (!(dep = find_dependency(ctx, dependency_id))) {
        return ACVP_INVALID_ARG;
    }

    return acvp_oe_dependency_set_field(ctx, dep, DEPENDENCY_FIELD_DESC, value);
}

/**
 * @brief Designate a new OE entry for this session.
 *
 * @return non-zero value representing the "oe_id"
 * @return 0 fail
 */
ACVP_RESULT acvp_oe_oe_new(ACVP_CTX *ctx,
                           unsigned int id,
                           const char *name) {
    ACVP_OES *oes = NULL;
    ACVP_OE *new_oe = NULL;
    ACVP_RESULT rv = 0;
    int k = 0;

    if (!ctx) return ACVP_NO_CTX;

    /* Get a handle on the OES */
    oes = &ctx->op_env.oes;

    if (oes->count == LIBACVP_OES_MAX) {
        ACVP_LOG_ERR("Libacvp already reached max OE capacity (%u)",
                     LIBACVP_OES_MAX);
        return ACVP_UNSUPPORTED_OP;
    }

    if (!id) {
        ACVP_LOG_ERR("Required parameter 'id' must be non-zero");
        return ACVP_INVALID_ARG;
    }

    for (k = 0; k < oes->count; k++) {
        if (id == oes->oe[k].id) {
            ACVP_LOG_ERR("An OE already exists with this same 'id'(%d)", id);
            return ACVP_INVALID_ARG;
        }
    }

    new_oe = &oes->oe[oes->count];
    oes->count++;

    /* Set the ID */
    new_oe->id = id;

    rv = copy_oe_string(&new_oe->name, name);
    if (ACVP_INVALID_ARG == rv) {
        ACVP_LOG_ERR("'name` string too long");
        return rv;
    }
    if (ACVP_MISSING_ARG == rv) {
        ACVP_LOG_ERR("Required parameter 'name` is NULL");
        return rv;
    }

    return ACVP_SUCCESS;
}

static ACVP_OE *find_oe(ACVP_CTX *ctx,
                        unsigned int id) {
    ACVP_OES *oes = NULL;
    int k = 0;

    if (!ctx) return NULL;

    /* Get a handle on the Vendors */
    oes = &ctx->op_env.oes;

    if (id == 0) {
        ACVP_LOG_ERR("Invalid 'id', must be non-zero");
        return NULL;
    }
    for (k = 0; k < oes->count; k++) {
        if (id == oes->oe[k].id) {
            /* Match */
            return &oes->oe[k];
        }
    }

    ACVP_LOG_ERR("Invalid 'id' (%u)", id);
    return NULL;
}

ACVP_RESULT acvp_oe_oe_set_dependency(ACVP_CTX *ctx,
                                      unsigned int oe_id,
                                      unsigned int dependency_id) {
    ACVP_OE *oe = NULL;
    ACVP_DEPENDENCY *dep = NULL;

    if (!ctx) return ACVP_NO_CTX;

    /* Get a handle on the selected OE */
    if (!(oe = find_oe(ctx, oe_id))) {
        return ACVP_INVALID_ARG;
    }

    /* Insert a pointer to the actual Dependency struct location */
    if (!(dep = find_dependency(ctx, dependency_id))) {
        return ACVP_INVALID_ARG;
    }

    if (oe->dependencies.count == LIBACVP_DEPENDENCIES_MAX) {
        ACVP_LOG_ERR("Libacvp already reached max OE(%u) dependency capacity (%u)",
                     oe_id, LIBACVP_VENDORS_MAX);
        return ACVP_UNSUPPORTED_OP;
    }

    /* Set pointer to the dependency */
    oe->dependencies.deps[oe->dependencies.count] = dep;
    oe->dependencies.count++;

    return ACVP_SUCCESS;
}

static ACVP_VENDOR *find_vendor(ACVP_CTX *ctx,
                                unsigned int id) {
    ACVP_VENDORS *vendors = NULL;
    int k = 0;

    if (!ctx) return NULL;

    /* Get a handle on the Vendors */
    vendors = &ctx->op_env.vendors;

    if (id == 0) {
        ACVP_LOG_ERR("Invalid 'id', must be non-zero");
        return NULL;
    }
    for (k = 0; k < vendors->count; k++) {
        if (id == vendors->v[k].id) {
            /* Match */
            return &vendors->v[k];
        }
    }

    ACVP_LOG_ERR("Invalid 'id' (%u)", id);
    return NULL;
}

static ACVP_RESULT acvp_oe_vendor_new(ACVP_CTX *ctx,
                                      unsigned int id,
                                      const char *name) {
    ACVP_VENDORS *vendors = NULL;
    ACVP_VENDOR *new_vendor = NULL;
    ACVP_RESULT rv = 0;
    int i = 0;

    if (!ctx) return ACVP_NO_CTX;

    /* Get handle on vendor fields */
    vendors = &ctx->op_env.vendors;

    if (vendors->count == LIBACVP_VENDORS_MAX) {
        ACVP_LOG_ERR("Libacvp already reached max Vendor capacity (%u)",
                     LIBACVP_VENDORS_MAX);
        return ACVP_UNSUPPORTED_OP;
    }

    if (!id) {
        ACVP_LOG_ERR("Required parameter 'id' must be non-zero");
        return ACVP_INVALID_ARG;
    }

    for (i = 0; i < vendors->count; i++) {
        if (id == vendors->v[i].id) {
            ACVP_LOG_ERR("A Vendor already exists with this same 'id'(%d)", id);
            return ACVP_INVALID_ARG;
        }
    }

    new_vendor = &vendors->v[vendors->count];
    vendors->count++;

    /* Set the ID */
    new_vendor->id = id;

    rv = copy_oe_string(&new_vendor->name, name);
    if (ACVP_INVALID_ARG == rv) {
        ACVP_LOG_ERR("'name` string too long");
        return rv;
    }
    if (ACVP_MISSING_ARG == rv) {
        ACVP_LOG_ERR("Required parameter 'name` is NULL");
        return rv;
    }

    return ACVP_SUCCESS;
}

static ACVP_RESULT acvp_oe_vendor_add_address(ACVP_CTX *ctx,
                                              ACVP_VENDOR *vendor,
                                              const char *street_1,
                                              const char *street_2,
                                              const char *street_3,
                                              const char *locality,
                                              const char *region,
                                              const char *country,
                                              const char *postal_code) {
    ACVP_VENDOR_ADDRESS *address = NULL;
    ACVP_RESULT rv = 0;

    if (!ctx) return ACVP_NO_CTX;

    if (!street_1 && !street_2 && !street_3 &&
        !locality && !region && !country && !postal_code) {
        ACVP_LOG_ERR("Need at least 1 of the parameters to be non-NULL");
        return ACVP_INVALID_ARG;
    }

    /* Get handle on the address field */
    address = &vendor->address;

    if (street_1) {
        rv = copy_oe_string(&address->street_1, street_1);
        if (ACVP_INVALID_ARG == rv) {
            ACVP_LOG_ERR("'street1' string too long");
            return rv;
        }
    }
    if (street_2) {
        rv = copy_oe_string(&address->street_2, street_2);
        if (ACVP_INVALID_ARG == rv) {
            ACVP_LOG_ERR("'street2' string too long");
            return rv;
        }
    }
    if (street_3) {
        rv = copy_oe_string(&address->street_3, street_3);
        if (ACVP_INVALID_ARG == rv) {
            ACVP_LOG_ERR("'street3' string too long");
            return rv;
        }
    }
    if (locality) {
        rv = copy_oe_string(&address->locality, locality);
        if (ACVP_INVALID_ARG == rv) {
            ACVP_LOG_ERR("'locality' string too long");
            return rv;
        }
    }
    if (region) {
        rv = copy_oe_string(&address->region, region);
        if (ACVP_INVALID_ARG == rv) {
            ACVP_LOG_ERR("'region' string too long");
            return rv;
        }
    }
    if (country) {
        rv = copy_oe_string(&address->country, country);
        if (ACVP_INVALID_ARG == rv) {
            ACVP_LOG_ERR("'country' string too long");
            return rv;
        }
    }
    if (postal_code) {
        rv = copy_oe_string(&address->postal_code, postal_code);
        if (ACVP_INVALID_ARG == rv) {
            ACVP_LOG_ERR("'postal_code' string too long");
            return rv;
        }
    }

    return ACVP_SUCCESS;
}

/**
 * @brief Designate a new Module entry for this session.
 *
 * @param name Name of the module
 *
 * @return non-zero value representing the "id"
 * @return 0 fail
 */
ACVP_RESULT acvp_oe_module_new(ACVP_CTX *ctx,
                               unsigned int id,
                               unsigned int vendor_id,
                               const char *name) {
    ACVP_MODULES *modules = NULL;
    ACVP_MODULE *new_module = NULL;
    ACVP_VENDOR *vendor = NULL;
    ACVP_RESULT rv = 0;
    int k = 0;

    if (!ctx) return ACVP_NO_CTX;

    /* Get a handle on the Modules */
    modules = &ctx->op_env.modules;

    if (modules->count == LIBACVP_MODULES_MAX) {
        ACVP_LOG_ERR("Libacvp already reached max MODULE capacity (%u)",
                     LIBACVP_MODULES_MAX);
        return ACVP_UNSUPPORTED_OP;
    }
    if (!id) {
        ACVP_LOG_ERR("Required parameter 'id' must be non-zero");
        return ACVP_INVALID_ARG;
    }

    for (k = 0; k < modules->count; k++) {
        if (id == modules->module[k].id) {
            ACVP_LOG_ERR("A Module already exists with this same 'id'(%d)", id);
            return ACVP_INVALID_ARG;
        }
    }

    new_module = &modules->module[modules->count];
    modules->count++;

    /* Set the ID */
    new_module->id = id;

    /* Insert a pointer to the actual Vendor struct location */
    if (!(vendor = find_vendor(ctx, vendor_id))) return ACVP_INVALID_ARG;
    new_module->vendor = vendor;

    rv = copy_oe_string(&new_module->name, name);
    if (ACVP_INVALID_ARG == rv) {
        ACVP_LOG_ERR("'name` string too long");
        return rv;
    }
    if (ACVP_MISSING_ARG == rv) {
        ACVP_LOG_ERR("Required parameter 'name` is NULL");
        return rv;
    }

    return ACVP_SUCCESS; /** Return the array position + 1 */
}

static ACVP_MODULE *find_module(ACVP_CTX *ctx,
                                unsigned int id) {
    ACVP_MODULES *modules = NULL;
    int k = 0;

    if (!ctx) return NULL;

    modules = &ctx->op_env.modules;

    if (id == 0) {
        ACVP_LOG_ERR("Invalid 'id', must be non-zero");
        return NULL;
    }
    for (k = 0; k < modules->count; k++) {
        if (id == modules->module[k].id) {
            /* Match */
            return &modules->module[k];
        }
    }

    ACVP_LOG_ERR("Invalid 'id' (%u)", id);
    return NULL;
}

ACVP_RESULT acvp_oe_module_set_type_version_desc(ACVP_CTX *ctx,
                                                 unsigned int id,
                                                 const char *type,
                                                 const char *version,
                                                 const char *description) {
    ACVP_MODULE *module = NULL;
    ACVP_RESULT rv = 0;

    if (!ctx) return ACVP_NO_CTX;

    if (!type && !version && !description) {
        ACVP_LOG_ERR("Need at least 1 of the parameters to be non-NULL");
        return ACVP_INVALID_ARG;
    } 

    module = find_module(ctx, id);
    if (!module) return ACVP_INVALID_ARG;

    if (type) {
        rv = copy_oe_string(&module->type, type);
        if (ACVP_INVALID_ARG == rv) {
            ACVP_LOG_ERR("'type' string too long");
            return rv;
        }
    }
    if (version) {
        rv = copy_oe_string(&module->version, version);
        if (ACVP_INVALID_ARG == rv) {
            ACVP_LOG_ERR("'version' string too long");
            return rv;
        }
    }
    if (description) {
        rv = copy_oe_string(&module->description, description);
        if (ACVP_INVALID_ARG == rv) {
            ACVP_LOG_ERR("'description' string too long");
            return rv;
        }
    }

    return ACVP_SUCCESS;
}

#if 0
static ACVP_RESULT acvp_oe_vendor_record_identifier(ACVP_CTX *ctx,
                                                    ACVP_VENDOR *vendor) {
    ACVP_RESULT rv = ACVP_SUCCESS;
    JSON_Value *val = NULL;
    JSON_Object *obj = NULL;
    const char *url = NULL;

    val = acvp_validate_identifier(ctx);
    if (!val) return ACVP_JSON_ERR;

    /* Grab the 'approvedUrl' identifier */
    obj = acvp_get_obj_from_rsp(val);
    url = json_object_get_string(obj, "approvedUrl");
    if (!url) return ACVP_JSON_ERR;

    /* Record it */
    vendor->url = calloc(ACVP_ATTR_URL_MAX + 1, sizeof(char));
    strcpy_s(vendor->url, ACVP_ATTR_URL_MAX, url);

    json_value_free(val);

    return rv;
}

static ACVP_RESULT acvp_oe_person_record_identifier(ACVP_CTX *ctx,
                                                    ACVP_PERSON *person) {
    ACVP_RESULT rv = ACVP_SUCCESS;
    JSON_Value *val = NULL;
    JSON_Object *obj = NULL;
    const char *url = NULL;

    val = acvp_validate_identifier(ctx);
    if (!val) return ACVP_JSON_ERR;

    /* Grab the 'approvedUrl' identifier */
    obj = acvp_get_obj_from_rsp(val);
    url = json_object_get_string(obj, "approvedUrl");
    if (!url) return ACVP_JSON_ERR;

    /* Record it */
    person->url = calloc(ACVP_ATTR_URL_MAX + 1, sizeof(char));
    strcpy_s(person->url, ACVP_ATTR_URL_MAX, url);

    json_value_free(val);

    return rv;
}

static ACVP_RESULT acvp_oe_oe_record_identifier(ACVP_CTX *ctx,
                                                ACVP_OE *oe) {
    ACVP_RESULT rv = ACVP_SUCCESS;
    JSON_Value *val = NULL;
    JSON_Object *obj = NULL;
    const char *url = NULL;

    val = acvp_validate_identifier(ctx);
    if (!val) return ACVP_JSON_ERR;

    /* Grab the 'approvedUrl' identifier */
    obj = acvp_get_obj_from_rsp(val);
    url = json_object_get_string(obj, "approvedUrl");
    if (!url) return ACVP_JSON_ERR;

    /* Record it */
    oe->url = calloc(ACVP_ATTR_URL_MAX + 1, sizeof(char));
    strcpy_s(oe->url, ACVP_ATTR_URL_MAX, url);

    json_value_free(val);

    return rv;
}

static ACVP_RESULT acvp_oe_dependency_record_identifier(ACVP_CTX *ctx,
                                                        ACVP_DEPENDENCY *dep) {
    ACVP_RESULT rv = ACVP_SUCCESS;
    JSON_Value *val = NULL;
    JSON_Object *obj = NULL;
    const char *url = NULL;

    val = acvp_validate_identifier(ctx);
    if (!val) return ACVP_JSON_ERR;

    /* Grab the 'approvedUrl' identifier */
    obj = acvp_get_obj_from_rsp(val);
    url = json_object_get_string(obj, "approvedUrl");
    if (!url) return ACVP_JSON_ERR;

    /* Record it */
    dep->url = calloc(ACVP_ATTR_URL_MAX + 1, sizeof(char));
    strcpy_s(dep->url, ACVP_ATTR_URL_MAX, url);

    json_value_free(val);

    return rv;
}

/*
 * This routine performs the JSON parsing of the modules response
 * from the ACVP server.  The response should contain a url to
 * access the registered module
 */
static ACVP_RESULT acvp_oe_module_record_identifier(ACVP_CTX *ctx, ACVP_MODULE *module) {
    ACVP_RESULT rv = ACVP_SUCCESS;
    JSON_Value *val = NULL;
    JSON_Object *obj = NULL;
    const char *url = NULL;

    val = acvp_validate_identifier(ctx);
    if (!val) return ACVP_JSON_ERR;

    /* Grab the 'approvedUrl' identifier */
    obj = acvp_get_obj_from_rsp(val);
    url = json_object_get_string(obj, "approvedUrl");
    if (!url) return ACVP_JSON_ERR;

    /* Record it */
    module->url = calloc(ACVP_ATTR_URL_MAX + 1, sizeof(char));
    strcpy_s(module->url, ACVP_ATTR_URL_MAX, url);

    json_value_free(val);

    return rv;
}

ACVP_RESULT acvp_oe_register_oes(ACVP_CTX *ctx) {
    ACVP_RESULT rv = 0;
    char *json_str = NULL;
    int i = 0;

    if (!ctx) return ACVP_NO_CTX;

    for (i = 0; i < ctx->oes.count; i++) {
        ACVP_OE *cur_oe = &ctx->oes.oe[i];
        int json_len = 0;

        rv = acvp_register_build_oe(ctx, cur_oe, &json_str, &json_len);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Unable to build oe message");
            goto end;
        }

        rv = acvp_transport_send_oe_registration(ctx, json_str, json_len);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Failed to send OE registration");
            goto end;
        }
        ACVP_LOG_STATUS("200 OK %s", ctx->curl_buf);

        rv = acvp_oe_oe_record_identifier(ctx, cur_oe);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Failed to record OE identifier");
            goto end;
        }

        /* Free for the next iteration */
        if (json_str) json_free_serialized_string(json_str);
        json_str = NULL;
    }

end:
    if (json_str) json_free_serialized_string(json_str);

    return rv;
}

ACVP_RESULT acvp_oe_register_dependencies(ACVP_CTX *ctx) {
    ACVP_RESULT rv = 0;
    char *json_str = NULL;
    int i = 0;

    if (!ctx) return ACVP_NO_CTX;

    for (i = 0; i < ctx->dependencies.count; i++) {
        ACVP_DEPENDENCY *cur_dep = &ctx->dependencies.deps[i];
        int json_len = 0;

        rv = acvp_register_build_dependency(ctx, cur_dep, &json_str, &json_len);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Unable to build Dependency message");
            goto end;
        }

        rv = acvp_transport_send_dependency_registration(ctx, json_str, json_len);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Failed to send Dependency registration");
            goto end;
        }
        ACVP_LOG_STATUS("200 OK %s", ctx->curl_buf);

        rv = acvp_oe_dependency_record_identifier(ctx, cur_dep);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Failed to record Dependency identifier");
            goto end;
        }

        /* Free for the next iteration */
        if (json_str) json_free_serialized_string(json_str);
        json_str = NULL;
    }

end:
    if (json_str) json_free_serialized_string(json_str);

    return rv;
}

ACVP_RESULT acvp_oe_register_vendors(ACVP_CTX *ctx) {
    ACVP_RESULT rv = 0;
    char *json_str = NULL;
    int i = 0;

    if (!ctx) return ACVP_NO_CTX;

    for (i = 0; i < ctx->vendors.count; i++) {
        ACVP_VENDOR *cur_vendor = &ctx->vendors.v[i];
        int json_len = 0;

        rv = acvp_register_build_vendor(ctx, cur_vendor, &json_str, &json_len);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Unable to build Vendor message");
            goto end;
        }

        rv = acvp_transport_send_vendor_registration(ctx, json_str, json_len);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Failed to send Vendor registration");
            goto end;
        }
        ACVP_LOG_STATUS("200 OK %s", ctx->curl_buf);

        rv = acvp_oe_vendor_record_identifier(ctx, cur_vendor);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Failed to record Vendor identifier");
            goto end;
        }

        /* Free for the next iteration */
        if (json_str) json_free_serialized_string(json_str);
        json_str = NULL;
    }

end:
    if (json_str) json_free_serialized_string(json_str);

    return rv;
}

ACVP_RESULT acvp_oe_register_modules(ACVP_CTX *ctx) {
    ACVP_RESULT rv = 0;
    char *json_str = NULL;
    int i = 0;

    if (!ctx) return ACVP_NO_CTX;

    for (i = 0; i < ctx->modules.count; i++) {
        ACVP_MODULE *cur_module = &ctx->modules.module[i];
        int json_len = 0;

        rv = acvp_register_build_module(ctx, cur_module, &json_str, &json_len);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Unable to build Module message");
            goto end;
        }

        rv = acvp_transport_send_module_registration(ctx, json_str, json_len);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Failed to send Module registration");
            goto end;
        }
        ACVP_LOG_STATUS("200 OK %s", ctx->curl_buf);

        rv = acvp_oe_module_record_identifier(ctx, cur_module);
        if (rv != ACVP_SUCCESS) {
            ACVP_LOG_ERR("Failed to record Module identifier");
            goto end;
        }

        /* Free for the next iteration */
        if (json_str) json_free_serialized_string(json_str);
        json_str = NULL;
    }

end:
    if (json_str) json_free_serialized_string(json_str);

    return rv;
}

ACVP_RESULT acvp_oe_register_operating_env(ACVP_CTX *ctx) {
    ACVP_RESULT rv = 0;

    /*
     * Register the Vendors
     */
    rv = acvp_oe_register_vendors(ctx);
    if (rv != ACVP_SUCCESS) {
        ACVP_LOG_ERR("Unable to register Vendors");
        return rv;
    }

    /*
     * Register the Modules
     */
    rv = acvp_oe_register_modules(ctx);
    if (rv != ACVP_SUCCESS) {
        ACVP_LOG_ERR("Unable to register Modules");
        return rv;
    }

    /*
     * Register the Dependencies
     */
    rv = acvp_oe_register_dependencies(ctx);
    if (rv != ACVP_SUCCESS) {
        ACVP_LOG_ERR("Unable to register Dependencies");
        return rv;
    }

    /*
     * Register the Operating Environments (OES)
     */
    rv = acvp_oe_register_oes(ctx);
    if (rv != ACVP_SUCCESS) {
        ACVP_LOG_ERR("Unable to register OES");
        return rv;
    }

    return ACVP_SUCCESS;
}
#endif

/******************
 * ****************
 * Cleanup functions
 * ****************
 *****************/

static void free_phone_list(ACVP_OE_PHONE_LIST **phone_list) {
    ACVP_OE_PHONE_LIST *p = NULL;
    ACVP_OE_PHONE_LIST *tmp = NULL;

    if (phone_list == NULL) return;
    p = *phone_list;
    if (p == NULL) return;

    while (p) {
        if (p->number) free(p->number);
        if (p->type) free(p->type);
        tmp = p;
        p = p->next;
        free(tmp);
    }

    *phone_list = NULL;
}

static void free_dependencies(ACVP_DEPENDENCIES *dependencies) {
    int i = 0;

    for (i = 0; i < dependencies->count; i++) {
        ACVP_DEPENDENCY *dep = &dependencies->deps[i];
        if (dep->url) free(dep->url);
        if (dep->type) free(dep->type);
        if (dep->name) free(dep->name);
        if (dep->description) free(dep->description);
    }
}

static void free_oes(ACVP_OES *oes) {
    int i = 0;

    for (i = 0; i < oes->count; i++) {
        ACVP_OE *oe = &oes->oe[i];
        if (oe->name) free(oe->name);
        if (oe->url) free(oe->url);
    }
}

static void free_vendor_persons(ACVP_VENDOR *vendor) {
    ACVP_PERSONS *persons = &vendor->persons;
    int i = 0;

    for (i = 0; i < persons->count; i++) {
        ACVP_PERSON *person = &persons->person[i];

        if (person->url) free(person->url);
        if (person->full_name) free(person->full_name);
        acvp_free_str_list(&person->emails);
        free_phone_list(&person->phone_numbers);
    }
}

static void free_vendor_address(ACVP_VENDOR *vendor) {
    ACVP_VENDOR_ADDRESS *address = &vendor->address;

    if (address->street_1) free(address->street_1);
    if (address->street_2) free(address->street_2);
    if (address->street_3) free(address->street_3);
    if (address->locality) free(address->locality);
    if (address->region) free(address->region);
    if (address->country) free(address->country);
    if (address->postal_code) free(address->postal_code);
    if (address->url) free(address->url);
}

static void free_vendors(ACVP_VENDORS *vendors) {
    int i = 0;

    for (i = 0; i < vendors->count; i++) {
        ACVP_VENDOR *vendor = &vendors->v[i];

        if (vendor->url) free(vendor->url);
        if (vendor->name) free(vendor->name);
        if (vendor->website) free(vendor->website);
        acvp_free_str_list(&vendor->emails);
        free_phone_list(&vendor->phone_numbers);

        free_vendor_address(vendor);
        free_vendor_persons(vendor);
    }
}

static void free_modules(ACVP_MODULES *modules) {
    int i = 0;

    for (i = 0; i < modules->count; i++) {
        ACVP_MODULE *module = &modules->module[i];

        if (module->name) free(module->name);
        if (module->type) free(module->type);
        if (module->version) free(module->version);
        if (module->description) free(module->description);
        if (module->url) free(module->url);
    }
}

void acvp_oe_free_operating_env(ACVP_CTX *ctx) {
    free_vendors(&ctx->op_env.vendors);
    free_modules(&ctx->op_env.modules);
    free_dependencies(&ctx->op_env.dependencies);
    free_oes(&ctx->op_env.oes);
}

/******************
 * ****************
 * Metadata functions
 * ****************
 *****************/

static ACVP_RESULT acvp_oe_metadata_parse_vendor_address(ACVP_CTX *ctx,
                                                         JSON_Object *obj,
                                                         ACVP_VENDOR *vendor) {
    JSON_Object *a_obj = NULL;
    const char *street_1 = NULL, *street_2 = NULL, *street_3 = NULL,
               *locality = NULL, *region= NULL, *country = NULL,
               *postal_code = NULL;
    ACVP_RESULT rv = ACVP_SUCCESS;

    if (!ctx) return ACVP_NO_CTX;
    if (!obj) {
        ACVP_LOG_ERR("Requried parameter 'obj' is NULL");
        return ACVP_INVALID_ARG;
    }
    if (!vendor) {
        ACVP_LOG_ERR("Requried parameter 'vendor' is NULL");
        return ACVP_INVALID_ARG;
    }

    a_obj = json_object_get_object(obj, "address");
    if (!a_obj) {
        ACVP_LOG_ERR("Json missing 'address'");
        return ACVP_MISSING_ARG;
    }

    street_1 = json_object_get_string(a_obj, "street1");
    street_2 = json_object_get_string(a_obj, "street2");
    street_3 = json_object_get_string(a_obj, "street3");
    locality = json_object_get_string(a_obj, "locality");
    region = json_object_get_string(a_obj, "region");
    country = json_object_get_string(a_obj, "country");
    postal_code = json_object_get_string(a_obj, "postalCode");

    rv = acvp_oe_vendor_add_address(ctx, vendor, street_1, street_2, street_3,
                                    locality, region, country, postal_code);
    if (ACVP_SUCCESS != rv) {
        ACVP_LOG_ERR("Failed to parse Vendor Address");
        return rv;
    }

    return ACVP_SUCCESS;
}

static ACVP_RESULT acvp_oe_metadata_parse_emails(ACVP_CTX *ctx,
                                                 JSON_Object *obj,
                                                 ACVP_STRING_LIST **email_list) {
    JSON_Array *emails_array = NULL;
    ACVP_STRING_LIST *email = NULL;
    int i = 0, count = 0;
    ACVP_RESULT rv = ACVP_SUCCESS;

    if (!ctx) return ACVP_NO_CTX;
    if (!obj) {
        ACVP_LOG_ERR("Requried parameter 'obj' is NULL");
        return ACVP_INVALID_ARG;
    }
    if (!email_list) {
        ACVP_LOG_ERR("Requried parameter 'email_list' is NULL");
        return ACVP_INVALID_ARG;
    }
    if (*email_list != NULL) {
        ACVP_LOG_ERR("Dereferencing parameter 'email_list' must be NULL");
        return ACVP_INVALID_ARG;
    }

    emails_array = json_object_get_array(obj, "emails");
    count = (int)json_array_get_count(emails_array);
    if (emails_array && count) {
        for (i = 0; i < count; i++) {
            const char *email_str = json_array_get_string(emails_array, i);
            if (!email_str) {
                ACVP_LOG_ERR("Problem parsing email string from JSON");
                return ACVP_JSON_ERR;
            }

            if (i == 0) {
                *email_list = calloc(1, sizeof(ACVP_STRING_LIST));
                if (*email_list == NULL) return ACVP_MALLOC_FAIL;
                email = *email_list;
            } else {
                email->next = calloc(1, sizeof(ACVP_STRING_LIST));
                if (email->next == NULL) return ACVP_MALLOC_FAIL;
                email = email->next;
            }

            rv = copy_oe_string(&email->string, email_str);
            if (ACVP_INVALID_ARG == rv) {
                ACVP_LOG_ERR("'street' string too long");
                return rv;
            }
        }
    }

    return ACVP_SUCCESS;
}

static ACVP_RESULT acvp_oe_metadata_parse_phone_numbers(ACVP_CTX *ctx,
                                                        JSON_Object *obj,
                                                        ACVP_OE_PHONE_LIST **phone_list) {
    JSON_Array *phones_array = NULL;
    ACVP_OE_PHONE_LIST *phone = NULL;
    int i = 0, count = 0;
    ACVP_RESULT rv = ACVP_SUCCESS;

    if (!ctx) return ACVP_NO_CTX;
    if (!obj) {
        ACVP_LOG_ERR("Requried parameter 'obj' is NULL");
        return ACVP_INVALID_ARG;
    }
    if (!phone_list) {
        ACVP_LOG_ERR("Requried parameter 'phone_list' is NULL");
        return ACVP_INVALID_ARG;
    }
    if (*phone_list != NULL) {
        ACVP_LOG_ERR("Dereferencing parameter 'phone_list' must be NULL");
        return ACVP_INVALID_ARG;
    }

    phones_array = json_object_get_array(obj, "phoneNumbers");
    count = (int)json_array_get_count(phones_array);
    if (phones_array && count) {
        for (i = 0; i < count; i++) {
            JSON_Object *phone_obj = NULL;
            const char *number_str = NULL, *type_str = NULL;

            phone_obj = json_array_get_object(phones_array, i);
            if (!phone_obj) {
                ACVP_LOG_ERR("Problem parsing phone object from JSON");
                return ACVP_JSON_ERR;
            }

            number_str = json_object_get_string(phone_obj, "number");
            if (!number_str) {
                ACVP_LOG_ERR("Problem parsing 'number' string from JSON");
                return ACVP_JSON_ERR;
            }

            type_str = json_object_get_string(phone_obj, "type");
            if (!type_str) {
                ACVP_LOG_ERR("Problem parsing 'type' string from JSON");
                return ACVP_JSON_ERR;
            }

            if (i == 0) {
                *phone_list = calloc(1, sizeof(ACVP_OE_PHONE_LIST));
                if (*phone_list == NULL) return ACVP_MALLOC_FAIL;
                phone = *phone_list;
            }else {
                phone->next = calloc(1, sizeof(ACVP_OE_PHONE_LIST));
                if (phone->next == NULL) return ACVP_MALLOC_FAIL;
                phone = phone->next;
            }

            rv = copy_oe_string(&phone->number, number_str);
            if (ACVP_INVALID_ARG == rv) {
                ACVP_LOG_ERR("'number' string too long");
                return rv;
            }
            rv = copy_oe_string(&phone->type, type_str);
            if (ACVP_INVALID_ARG == rv) {
                ACVP_LOG_ERR("'type' string too long");
                return rv;
            }
        }
    }

    return ACVP_SUCCESS;
}

static ACVP_RESULT acvp_oe_metadata_parse_vendor_contacts(ACVP_CTX *ctx,
                                                          JSON_Object *obj,
                                                          ACVP_VENDOR *vendor) {
    JSON_Array *contacts_array = NULL;
    int i = 0, count = 0;
    ACVP_RESULT rv = ACVP_SUCCESS;

    if (!ctx) return ACVP_NO_CTX;
    if (!obj) {
        ACVP_LOG_ERR("Requried parameter 'obj' is NULL");
        return ACVP_INVALID_ARG;
    }
    if (!vendor) {
        ACVP_LOG_ERR("Requried parameter 'vendor' is NULL");
        return ACVP_INVALID_ARG;
    }

    if (vendor->persons.count) {
        ACVP_LOG_ERR("Need to start with person.count == 0");
        return ACVP_INVALID_ARG;
    }

    contacts_array = json_object_get_array(obj, "contacts");
    count = (int)json_array_get_count(contacts_array);
    if (count > LIBACVP_PERSONS_MAX) {
        ACVP_LOG_ERR("Number of contacts (%d) > max allowed (%d)", count, LIBACVP_PERSONS_MAX);
        return ACVP_JSON_ERR;
    }
    if (count == 0) {
        ACVP_LOG_ERR("Need at least 1 contact");
        return ACVP_JSON_ERR;
    }

    for (i = 0; i < count; i++) {
        const char *name_str = NULL;
        ACVP_PERSON *person = &vendor->persons.person[i];
        JSON_Object *contact_obj = json_array_get_object(contacts_array, i);
        if (!contact_obj) {
            ACVP_LOG_ERR("Problem parsing 'contact' object from JSON");
            return ACVP_JSON_ERR;
        }

        /* Increment (in case of error below, we will still cleanup) */
        vendor->persons.count++;

        name_str = json_object_get_string(contact_obj, "fullName");
        if (!name_str) {
            ACVP_LOG_ERR("Problem parsing 'fullName' string from JSON");
            return ACVP_JSON_ERR;
        }
        rv = copy_oe_string(&person->full_name, name_str);
        if (ACVP_INVALID_ARG == rv) {
            ACVP_LOG_ERR("'fullName' string too long");
            return rv;
        }

        /* Parse the Emails (if it exists)*/
        rv = acvp_oe_metadata_parse_emails(ctx, obj, &person->emails);
        if (ACVP_SUCCESS != rv) return rv;

        /* Parse the Phone Numbers (if it exists)*/
        rv = acvp_oe_metadata_parse_phone_numbers(ctx, obj, &person->phone_numbers);
        if (ACVP_SUCCESS != rv) return rv;
    }

    return ACVP_SUCCESS;
}

static ACVP_RESULT acvp_oe_metadata_parse_vendor(ACVP_CTX *ctx, JSON_Object *obj) {
    ACVP_VENDOR *vendor = NULL;
    const char *name = NULL, *website = NULL;
    unsigned int vendor_id = 0;
    ACVP_RESULT rv = ACVP_SUCCESS;

    if (!ctx) return ACVP_NO_CTX;
    if (!obj) {
        ACVP_LOG_ERR("Requried parameter 'obj' is NULL");
        return ACVP_INVALID_ARG;
    } 

    vendor_id = (unsigned int)json_object_get_number(obj, "id");
    if (vendor_id == 0) {
        ACVP_LOG_ERR("Metadata JSON 'id' must be non-zero");
        return ACVP_INVALID_ARG;
    }

    name = json_object_get_string(obj, "name");
    if (!name) {
        ACVP_LOG_ERR("Metadata JSON missing 'name'");
        return ACVP_INVALID_ARG;
    }

    /* Designate and init new Vendor struct */
    rv = acvp_oe_vendor_new(ctx, vendor_id, name);
    if (rv != ACVP_SUCCESS) return rv;

    /* Get pointer to the new vendor */
    vendor = find_vendor(ctx, vendor_id);
    if (!vendor) return ACVP_INVALID_ARG;

    website = json_object_get_string(obj, "website");
    if (website) {
        /* Copy the "website" */
        rv = copy_oe_string(&vendor->website, website);
        if (ACVP_INVALID_ARG == rv) {
            ACVP_LOG_ERR("'website' string too long");
            return rv;
        }
    }

    /* Parse the Emails (if it exists) */
    rv = acvp_oe_metadata_parse_emails(ctx, obj, &vendor->emails);
    if (ACVP_SUCCESS != rv) return rv;

    /* Parse the Phone Numbers (if it exists) */
    rv = acvp_oe_metadata_parse_phone_numbers(ctx, obj, &vendor->phone_numbers);
    if (ACVP_SUCCESS != rv) return rv;

    /* Parse the Address */
    rv = acvp_oe_metadata_parse_vendor_address(ctx, obj, vendor);
    if (ACVP_SUCCESS != rv) return rv;

    /* Parse the Contacts */
    rv = acvp_oe_metadata_parse_vendor_contacts(ctx, obj, vendor);
    if (ACVP_SUCCESS != rv) return rv;

    return ACVP_SUCCESS;
}

static ACVP_RESULT acvp_oe_metadata_parse_vendors(ACVP_CTX *ctx, JSON_Object *obj) {
    ACVP_RESULT rv = ACVP_SUCCESS;
    JSON_Array *vendors_array = NULL;
    int i = 0, vendors_count = 0;

    if (!ctx) return ACVP_NO_CTX;
    if (!obj) {
        ACVP_LOG_ERR("Requried parameter 'obj' is NULL");
        return ACVP_INVALID_ARG;
    }

    vendors_array = json_object_get_array(obj, "vendors");
    if (!vendors_array) {
        ACVP_LOG_ERR("Unable to resolve the 'vendors' array");
        return ACVP_JSON_ERR;
    }

    vendors_count = json_array_get_count(vendors_array);
    if (vendors_count == 0) {
        ACVP_LOG_ERR("Need at least one object in the 'vendors' array");
        return ACVP_MALFORMED_JSON;
    }
    for (i = 0; i < vendors_count; i++) {
        JSON_Object *vendor_obj = json_array_get_object(vendors_array, i);
        if (!vendor_obj) {
            ACVP_LOG_ERR("Unable to parse object at 'vendors'[%d]", i);
            return ACVP_JSON_ERR;
        }

        rv = acvp_oe_metadata_parse_vendor(ctx, vendor_obj);
        if (ACVP_SUCCESS != rv) return rv; /* Fail */
    }

    /* Success */
    return ACVP_SUCCESS;
}

static ACVP_RESULT acvp_oe_metadata_parse_module(ACVP_CTX *ctx, JSON_Object *obj) {
    const char *name = NULL, *version = NULL, *type = NULL, *description = NULL;
    unsigned int module_id = 0, vendor_id = 0;
    ACVP_RESULT rv = ACVP_SUCCESS;

    if (!ctx) return ACVP_NO_CTX;
    if (!obj) {
        ACVP_LOG_ERR("Requried parameter 'obj' is NULL");
        return ACVP_INVALID_ARG;
    } 

    module_id = (unsigned int)json_object_get_number(obj, "id");
    if (module_id == 0) {
        ACVP_LOG_ERR("Metadata JSON 'id' must be non-zero");
        return ACVP_INVALID_ARG;
    }

    vendor_id = (unsigned int)json_object_get_number(obj, "vendorId");
    if (vendor_id == 0) {
        ACVP_LOG_ERR("Metadata JSON 'vendorId' must be non-zero");
        return ACVP_INVALID_ARG;
    }

    name = json_object_get_string(obj, "name");
    if (!name) {
        ACVP_LOG_ERR("Metadata JSON missing 'name'");
        return ACVP_INVALID_ARG;
    }

    /* Designate and init new Module struct */
    rv = acvp_oe_module_new(ctx, module_id, vendor_id, name);
    if (rv != ACVP_SUCCESS) return rv;

    type = json_object_get_string(obj, "type");
    version = json_object_get_string(obj, "version");
    description = json_object_get_string(obj, "description");

    rv = acvp_oe_module_set_type_version_desc(ctx, module_id, type, version, description);
    if (ACVP_SUCCESS != rv) return rv;

    return ACVP_SUCCESS;
}

static ACVP_RESULT acvp_oe_metadata_parse_modules(ACVP_CTX *ctx, JSON_Object *obj) {
    ACVP_RESULT rv = ACVP_SUCCESS;
    JSON_Array *modules_array = NULL;
    int i = 0, modules_count = 0;

    if (!ctx) return ACVP_NO_CTX;
    if (!obj) {
        ACVP_LOG_ERR("Requried parameter 'obj' is NULL");
        return ACVP_INVALID_ARG;
    }

    modules_array = json_object_get_array(obj, "modules");
    if (!modules_array) {
        ACVP_LOG_ERR("Unable to resolve the 'modules' array");
        return ACVP_JSON_ERR;
    }

    modules_count = json_array_get_count(modules_array);
    /* 
     * Not required to be in the metadata file.
     * The user can specify modules via libacvp API.
     */
    if (modules_count == 0) return ACVP_SUCCESS; 

    for (i = 0; i < modules_count; i++) {
        JSON_Object *module_obj = json_array_get_object(modules_array, i);
        if (!module_obj) {
            ACVP_LOG_ERR("Unable to parse object at 'modules'[%d]", i);
            return ACVP_JSON_ERR;
        }

        rv = acvp_oe_metadata_parse_module(ctx, module_obj);
        if (ACVP_SUCCESS != rv) return rv; /* Fail */
    }

    /* Success */
    return ACVP_SUCCESS;
}

static int compare_dependencies(const ACVP_DEPENDENCY *a, const ACVP_DEPENDENCY *b) {
    int diff = 0;

    strcmp_s(a->type, ACVP_OE_STR_MAX, b->type, &diff);
    if (diff != 0) return 0;

    strcmp_s(a->name, ACVP_OE_STR_MAX, b->name, &diff);
    if (diff != 0) return 0;

    strcmp_s(a->description, ACVP_OE_STR_MAX, b->description, &diff);
    if (diff != 0) return 0;

    /* Reached the end, we have a full match */
    return 1;
}

static unsigned int match_dependency(ACVP_CTX *ctx, const ACVP_DEPENDENCY *dep) {
    int i = 0;

    if (!ctx) return 0;
    if (dep == NULL) {
        ACVP_LOG_ERR("Required parameter 'dep' must be non-NULL");
        return 0;
    }

    if (ctx->op_env.dependencies.count == 0) return 0;

    for (i = 0; i < ctx->op_env.dependencies.count; i++) {
        ACVP_DEPENDENCY *this_dep = &ctx->op_env.dependencies.deps[i];
        int match = 0;

        match = compare_dependencies(dep, this_dep);
        if (match) return dep->id;
    }

    return 0;
}

static ACVP_RESULT acvp_oe_metadata_parse_oe_dependencies(ACVP_CTX *ctx,
                                                          JSON_Object *obj,
                                                          unsigned int oe_id) {
    ACVP_DEPENDENCY *dep = NULL;
    JSON_Array *deps_array = NULL;
    int i = 0, count = 0;
    ACVP_RESULT rv = ACVP_SUCCESS;

    if (!ctx) return ACVP_NO_CTX;
    if (!obj) {
        ACVP_LOG_ERR("Requried parameter 'obj' is NULL");
        return ACVP_INVALID_ARG;
    }

    deps_array = json_object_get_array(obj, "dependencies");
    if (!deps_array) {
        ACVP_LOG_ERR("Missing 'dependencies' array in JSON");
        return ACVP_JSON_ERR;
    }
    count = (int)json_array_get_count(deps_array);
    if (!count) {
        ACVP_LOG_ERR("Requires at least 1 item in 'dependencies' array JSON");
        return ACVP_JSON_ERR;
    }

    for (i = 0; i < count; i++) {
        ACVP_DEPENDENCY tmp_dep = {0};
        const char *type_str = NULL, *name_str = NULL, *desc_str = NULL;
        JSON_Object *dep_obj = json_array_get_object(deps_array, i);
        unsigned int dep_id = 0;

        type_str = json_object_get_string(dep_obj, "type");
        name_str = json_object_get_string(dep_obj, "name");
        desc_str = json_object_get_string(dep_obj, "description");

        // Soft copy, no need to free
        tmp_dep.type = (char*)type_str;
        tmp_dep.name = (char*)name_str;
        tmp_dep.description = (char*)desc_str;

        dep_id = match_dependency(ctx, &tmp_dep);
        if (dep_id == 0) {
            /*
             * We didn't find a Dependency in memory that matches exactly.
             * Make a new one!
             */
            dep_id = glb_dependency_id;

            rv = acvp_oe_dependency_new(ctx, dep_id);
            if (ACVP_SUCCESS != rv) {
                ACVP_LOG_ERR("Failed to create new Dependency");
                return rv;
            }

            dep = find_dependency(ctx, dep_id);
            if (!dep) {
                rv = ACVP_INVALID_ARG;
                return rv;
            }

            if (type_str) {
                rv = acvp_oe_dependency_set_type(ctx, dep_id, type_str);
                if (ACVP_SUCCESS != rv) return rv;
            }

            if (name_str) {
                rv = acvp_oe_dependency_set_name(ctx, dep_id, name_str);
                if (ACVP_SUCCESS != rv) return rv;
            }

            if (desc_str) {
                rv = acvp_oe_dependency_set_description(ctx, dep_id, desc_str);
                if (ACVP_SUCCESS != rv) return rv;
            }

            /* Increment Global dependency ID*/
            glb_dependency_id++;
        }

        /* Add the Dependency to the OE */
        acvp_oe_oe_set_dependency(ctx, oe_id, dep_id);
    }

    return rv;
}

static ACVP_RESULT acvp_oe_metadata_parse_oe(ACVP_CTX *ctx, JSON_Object *obj) {
    const char *name = NULL;
    unsigned int oe_id = 0;
    ACVP_RESULT rv = ACVP_SUCCESS;

    if (!ctx) return ACVP_NO_CTX;
    if (!obj) {
        ACVP_LOG_ERR("Requried parameter 'obj' is NULL");
        return ACVP_INVALID_ARG;
    } 

    oe_id = (unsigned int)json_object_get_number(obj, "id");
    if (oe_id == 0) {
        ACVP_LOG_ERR("Metadata JSON 'id' must be non-zero");
        return ACVP_INVALID_ARG;
    }

    name = json_object_get_string(obj, "name");
    if (!name) {
        ACVP_LOG_ERR("Metadata JSON missing 'name'");
        return ACVP_INVALID_ARG;
    }

    /* Designate and init new OE struct */
    rv = acvp_oe_oe_new(ctx, oe_id, name);
    if (rv != ACVP_SUCCESS) return rv;

    /*
     * Parse the dependencies
     */
    rv = acvp_oe_metadata_parse_oe_dependencies(ctx, obj, oe_id);
    if (ACVP_SUCCESS != rv) return rv;

    return ACVP_SUCCESS;
}

static ACVP_RESULT acvp_oe_metadata_parse_oes(ACVP_CTX *ctx, JSON_Object *obj) {
    ACVP_RESULT rv = ACVP_SUCCESS;
    JSON_Array *oes_array = NULL;
    int i = 0, oes_count = 0;

    if (!ctx) return ACVP_NO_CTX;
    if (!obj) {
        ACVP_LOG_ERR("Requried parameter 'obj' is NULL");
        return ACVP_INVALID_ARG;
    }

    oes_array = json_object_get_array(obj, "operating_environments");
    if (!oes_array) {
        ACVP_LOG_ERR("Unable to resolve the 'operating_environments' array");
        return ACVP_JSON_ERR;
    }

    oes_count = json_array_get_count(oes_array);
    /* 
     * Not required to be in the metadata file.
     * The user can specify oes via libacvp API.
     */
    if (oes_count == 0) return ACVP_SUCCESS; 

    for (i = 0; i < oes_count; i++) {
        JSON_Object *oe_obj = json_array_get_object(oes_array, i);
        if (!oe_obj) {
            ACVP_LOG_ERR("Unable to parse object at 'operating_environments'[%d]", i);
            return ACVP_JSON_ERR;
        }

        rv = acvp_oe_metadata_parse_oe(ctx, oe_obj);
        if (ACVP_SUCCESS != rv) return rv; /* Fail */
    }

    /* Success */
    return ACVP_SUCCESS;
}

ACVP_RESULT acvp_oe_ingest_metadata(ACVP_CTX *ctx, const char *metadata_file) {
    JSON_Value *val = NULL;
    JSON_Object *obj = NULL;
    ACVP_RESULT rv = ACVP_SUCCESS;

    if (!ctx) return ACVP_NO_CTX;
    if (!metadata_file) {
        ACVP_LOG_ERR("Must provide string value for 'metadata_file'");
        return ACVP_MISSING_ARG;
    }

    if (strnlen_s(metadata_file, ACVP_JSON_FILENAME_MAX + 1) > ACVP_JSON_FILENAME_MAX) {
        ACVP_LOG_ERR("Provided 'metadata_file' string length > max(%d)", ACVP_JSON_FILENAME_MAX);
        return ACVP_INVALID_ARG;
    }

    val = json_parse_file(metadata_file);
    if (!val) return ACVP_JSON_ERR;
    obj = json_value_get_object(val);
    if (!obj) {
        rv = ACVP_JSON_ERR;
        goto end;
    }

    rv = acvp_oe_metadata_parse_vendors(ctx, obj);
    if (ACVP_SUCCESS != rv) {
        ACVP_LOG_ERR("Failed to parse 'vendors' from metadata JSON");
        goto end;
    }

    rv = acvp_oe_metadata_parse_modules(ctx, obj);
    if (ACVP_SUCCESS != rv) {
        ACVP_LOG_ERR("Failed to parse 'modules' from metadata JSON");
        goto end;
    }

    rv = acvp_oe_metadata_parse_oes(ctx, obj);
    if (ACVP_SUCCESS != rv) {
        ACVP_LOG_ERR("Failed to parse 'operating_environments' from metadata JSON");
        goto end;
    }

    /*
     * The metadata is loaded into memory.
     * It must be verified before running a validation on a testSession.
     */
    ctx->fips.metadata_loaded = 1;

end:
    if (val) json_value_free(val);

    return rv;
}

ACVP_RESULT acvp_oe_set_fips_validation_metadata(ACVP_CTX *ctx,
                                                 unsigned int module_id,
                                                 unsigned int oe_id) {
    ACVP_MODULE *module = NULL;
    ACVP_OE *oe = NULL;

    if (ctx == NULL) return ACVP_NO_CTX;

    /*
     * Check that everything needed for the FIPS validation is sane.
     */
    if (!ctx->fips.metadata_loaded) {
        ACVP_LOG_ERR("User needs to load a valid metadata JSON file via acvp_oe_ingest_metadata()");
        return ACVP_INVALID_ARG;
    }

    if (module_id == 0 && oe_id == 0) {
        ACVP_LOG_ERR("Required parameters 'module_id' and 'oe_id' both == 0."
                     "At least one parameter must be non-zero");
        return ACVP_INVALID_ARG;
    }

    if (module_id) {
        module = find_module(ctx, module_id);
        if (module == NULL) {
            ACVP_LOG_ERR("Failed to find module with id(%u)", module_id);
            return ACVP_INVALID_ARG;
        }

        // Set the Module for the validation
        ctx->fips.module = module;
    }

    if (oe_id) {
        oe = find_oe(ctx, oe_id);
        if (oe == NULL) {
            ACVP_LOG_ERR("Failed to find oe with id(%u)", oe_id);
            return ACVP_INVALID_ARG;
        }

        // Set the OE for the validation
        ctx->fips.oe = oe;
    }

    return ACVP_SUCCESS;
}

