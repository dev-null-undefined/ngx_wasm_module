#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include <ngx_proxy_wasm_maps.h>
#ifdef NGX_WASM_HTTP
#include <ngx_http_proxy_wasm.h>
#endif


static ngx_list_t *ngx_proxy_wasm_maps_get_map(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type);
static ngx_str_t *ngx_proxy_wasm_maps_get_special_key(ngx_wavm_instance_t *instance,
    ngx_uint_t map_type, ngx_str_t *key);
static ngx_int_t ngx_proxy_wasm_maps_set_special_key(
    ngx_wavm_instance_t *instance, ngx_uint_t map_type,
    ngx_str_t *key, ngx_str_t *value);
#ifdef NGX_WASM_HTTP
static ngx_str_t *ngx_proxy_wasm_maps_get_path(ngx_wavm_instance_t *instance);
static ngx_int_t ngx_proxy_wasm_maps_set_path(ngx_wavm_instance_t *instance,
    ngx_str_t *value);
static ngx_str_t *ngx_proxy_wasm_maps_get_method(ngx_wavm_instance_t *instance);
static ngx_int_t ngx_proxy_wasm_maps_set_method(ngx_wavm_instance_t *instance,
    ngx_str_t *value);
static ngx_str_t *ngx_proxy_wasm_maps_get_scheme(ngx_wavm_instance_t *instance);
static ngx_str_t *ngx_proxy_wasm_maps_get_authority(
    ngx_wavm_instance_t *instance);
#endif


static ngx_proxy_wasm_maps_key_t  ngx_proxy_wasm_maps_special_keys[] = {

#ifdef NGX_WASM_HTTP
    { ngx_string(":path"),
      NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS,
      ngx_proxy_wasm_maps_get_path,
      ngx_proxy_wasm_maps_set_path },

    { ngx_string(":method"),
      NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS,
      ngx_proxy_wasm_maps_get_method,
      ngx_proxy_wasm_maps_set_method },

    { ngx_string(":scheme"),
      NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS,
      ngx_proxy_wasm_maps_get_scheme,
      NULL },

    { ngx_string(":authority"),
      NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS,
      ngx_proxy_wasm_maps_get_authority,
      NULL },
#endif

    { ngx_null_string, 0, NULL, NULL }

};


static ngx_list_t *
ngx_proxy_wasm_maps_get_map(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type)
{
#ifdef NGX_WASM_HTTP
    ngx_http_wasm_req_ctx_t  *rctx = ngx_http_proxy_wasm_host_get_rctx(instance);
    ngx_http_request_t       *r = rctx->r;
#endif

    switch (map_type) {

#ifdef NGX_WASM_HTTP
    case NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS:
        return &r->headers_in.headers;

    case NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS:
        return &r->headers_out.headers;
#endif

    default:
        ngx_wasm_assert(0);
        ngx_wavm_log_error(NGX_LOG_WASM_NYI, instance->log, NULL,
                           "NYI - get_map: %d", map_type);
        return NULL;

    }
}


ngx_list_t *
ngx_proxy_wasm_maps_get_all(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type, ngx_array_t *extras)
{
    size_t                        i;
    ngx_list_t                   *list;
    ngx_str_t                    *value;
    ngx_table_elt_t              *elt;
    ngx_proxy_wasm_maps_key_t    *mkey;
#ifdef NGX_WASM_HTTP
    ngx_table_elt_t              *shim;
    ngx_array_t                  *shims;
#endif

    list = ngx_proxy_wasm_maps_get_map(instance, map_type);
    if (list == NULL) {
        return NULL;
    }

    if (extras) {
        for (i = 0; ngx_proxy_wasm_maps_special_keys[i].key.len; i++) {
            mkey = &ngx_proxy_wasm_maps_special_keys[i];

            ngx_wasm_assert(mkey->get_);

            if (map_type != mkey->map_type) {
                continue;
            }

            value = mkey->get_(instance);

            ngx_wasm_assert(value);

            elt = ngx_array_push(extras);
            if (elt == NULL) {
                return NULL;
            }

            elt->hash = 0;
            elt->key = mkey->key;
            elt->value = *value;
            elt->lowcase_key = NULL;
        }

#ifdef NGX_WASM_HTTP
        if (map_type == NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS) {
            /* inject shim response headers
             * (produced by ngx_http_header_filter)
             */
            shims = ngx_http_wasm_get_shim_headers(
                        ngx_http_proxy_wasm_host_get_rctx(instance));

            shim = shims->elts;

            for (i = 0; i < shims->nelts; i++) {
                elt = ngx_array_push(extras);
                if (elt == NULL) {
                    return NULL;
                }

                elt->hash = 0;
                elt->key = shim[i].key;
                elt->value = shim[i].value;
                elt->lowcase_key = NULL;
            }
        }
#endif
    }

    return list;
}


ngx_int_t
ngx_proxy_wasm_maps_set_all(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type, ngx_array_t *pairs)
{
    size_t            i;
    ngx_int_t         rc;
    ngx_table_elt_t  *elt;

    for (i = 0; i < pairs->nelts; i++) {
        elt = &((ngx_table_elt_t *) pairs->elts)[i];

        rc = ngx_proxy_wasm_maps_set(instance, map_type,
                                     &elt->key, &elt->value,
                                     NGX_PROXY_WASM_MAP_SET);
        if (rc != NGX_OK) {
            return rc;
        }
    }

    return NGX_OK;
}


ngx_str_t *
ngx_proxy_wasm_maps_get(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type, ngx_str_t *key)
{
    ngx_str_t         *value;
    ngx_list_t        *list;

    /* special keys lookup */

    value = ngx_proxy_wasm_maps_get_special_key(instance, map_type, key);
    if (value) {
        goto found;
    }

    list = ngx_proxy_wasm_maps_get_map(instance, map_type);
    if (list == NULL) {
        return NULL;
    }

    /* key lookup */

    value = ngx_wasm_get_list_elem(list, key->data, key->len);
    if (value) {
        goto found;
    }

#ifdef NGX_WASM_HTTP
    if (map_type == NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS) {
        /* shim header lookup */

        value = ngx_http_wasm_get_shim_header(ngx_http_proxy_wasm_host_get_rctx(instance),
                                              key->data, key->len);
        if (value) {
            goto found;
        }
    }
#endif

    return NULL;

found:

    ngx_wasm_assert(value);

    return value;
}


ngx_int_t
ngx_proxy_wasm_maps_set(ngx_wavm_instance_t *instance,
    ngx_proxy_wasm_map_type_e map_type, ngx_str_t *key, ngx_str_t *value,
    ngx_uint_t map_op)
{
    ngx_int_t                 rc = NGX_ERROR;
#ifdef NGX_WASM_HTTP
    ngx_str_t                 skey, svalue;
    ngx_uint_t                mode = NGX_HTTP_WASM_HEADERS_SET;
    ngx_http_wasm_req_ctx_t  *rctx;
    ngx_http_request_t       *r;

    rctx = ngx_http_proxy_wasm_host_get_rctx(instance);
    r = rctx->r;

    switch (map_op) {

    case NGX_PROXY_WASM_MAP_SET:
        mode = NGX_HTTP_WASM_HEADERS_SET;
        break;

    case NGX_PROXY_WASM_MAP_ADD:
        mode = NGX_HTTP_WASM_HEADERS_APPEND;
        break;

    case NGX_PROXY_WASM_MAP_REMOVE:
        mode = NGX_HTTP_WASM_HEADERS_REMOVE;
        break;

    }

    if (map_op == NGX_PROXY_WASM_MAP_SET
        || map_op == NGX_PROXY_WASM_MAP_ADD)
    {
        skey.len = key->len;
        skey.data = ngx_pstrdup(instance->pool, key);
        if (skey.data == NULL) {
            return NGX_ERROR;
        }

        svalue.len = value->len;
        svalue.data = ngx_pstrdup(instance->pool, value);
        if (svalue.data == NULL) {
            return NGX_ERROR;
        }

        key = &skey;
        value = &svalue;
    }
#endif

    rc = ngx_proxy_wasm_maps_set_special_key(instance, map_type, key, value);
    if (rc == NGX_DONE) {
        rc = NGX_OK;
        goto done;
    }

    if (rc == NGX_ABORT) {
        ngx_wavm_log_error(NGX_LOG_ERR, instance->log, NULL,
                           "cannot set read-only \"%V\" header", key);
        goto done;
    }

    ngx_wasm_assert(rc == NGX_DECLINED);

    switch (map_type) {

#ifdef NGX_WASM_HTTP
    case NGX_PROXY_WASM_MAP_HTTP_REQUEST_HEADERS:
        rc = ngx_http_wasm_set_req_header(r, key, value, mode);
        if (rc == NGX_DECLINED) {
            /* bad header value, error logged */
            rc = NGX_OK;
        }

        break;

    case NGX_PROXY_WASM_MAP_HTTP_RESPONSE_HEADERS:
        rc = ngx_http_wasm_set_resp_header(r, key, value, mode);
        if (rc == NGX_DECLINED) {
            /* bad header value, error logged */
            rc = NGX_OK;
        }

        break;
#endif

    default:
        ngx_wasm_assert(0);
        break;

    }

done:

    return rc;
}


static ngx_str_t *
ngx_proxy_wasm_maps_get_special_key(ngx_wavm_instance_t *instance,
    ngx_uint_t map_type, ngx_str_t *key)
{
    size_t                      i;
    ngx_proxy_wasm_maps_key_t  *mkey;

    for (i = 0; ngx_proxy_wasm_maps_special_keys[i].key.len; i++) {
        mkey = &ngx_proxy_wasm_maps_special_keys[i];

        dd("key: %.*s", (int) mkey->key.len, mkey->key.data);

        ngx_wasm_assert(mkey->get_);

        if (map_type != mkey->map_type
            || ngx_strncmp(mkey->key.data, key->data, key->len) != 0)
        {
            continue;
        }

        return mkey->get_(instance);
    }

    return NULL;
}


ngx_int_t
ngx_proxy_wasm_maps_set_special_key(ngx_wavm_instance_t *instance,
    ngx_uint_t map_type, ngx_str_t *key, ngx_str_t *value)
{
    size_t                      i;
    ngx_proxy_wasm_maps_key_t  *mkey;

    for (i = 0; ngx_proxy_wasm_maps_special_keys[i].key.len; i++) {
        mkey = &ngx_proxy_wasm_maps_special_keys[i];

        if (map_type != mkey->map_type
            || ngx_strncmp(mkey->key.data, key->data, key->len) != 0)
        {
            continue;
        }

        if (mkey->set_ == NULL) {
            return NGX_ABORT;
        }

        if (mkey->set_(instance, value) == NGX_OK) {
            return NGX_DONE;
        }
    }

    return NGX_DECLINED;
}


#ifdef NGX_WASM_HTTP
static ngx_str_t *
ngx_proxy_wasm_maps_get_path(ngx_wavm_instance_t *instance)
{
    ngx_http_wasm_req_ctx_t  *rctx = ngx_http_proxy_wasm_host_get_rctx(instance);
    ngx_http_request_t       *r = rctx->r;

    return &r->uri;
}


static ngx_int_t
ngx_proxy_wasm_maps_set_path(ngx_wavm_instance_t *instance, ngx_str_t *value)
{
    ngx_http_wasm_req_ctx_t  *rctx = ngx_http_proxy_wasm_host_get_rctx(instance);
    ngx_http_request_t       *r = rctx->r;

    r->uri.len = value->len;
    r->uri.data = value->data;

    return NGX_OK;
}


static ngx_str_t *
ngx_proxy_wasm_maps_get_method(ngx_wavm_instance_t *instance)
{
    ngx_http_wasm_req_ctx_t  *rctx = ngx_http_proxy_wasm_host_get_rctx(instance);
    ngx_http_request_t       *r = rctx->r;

    return &r->method_name;
}


static ngx_int_t
ngx_proxy_wasm_maps_set_method(ngx_wavm_instance_t *instance, ngx_str_t *value)
{
    ngx_http_wasm_req_ctx_t  *rctx = ngx_http_proxy_wasm_host_get_rctx(instance);
    ngx_http_request_t       *r = rctx->r;

    r->method_name.len = value->len;
    r->method_name.data = value->data;

    return NGX_OK;
}


static ngx_str_t *
ngx_proxy_wasm_maps_get_scheme(ngx_wavm_instance_t *instance)
{
    u_char                       *p;
    ngx_uint_t                    hash;
    ngx_http_variable_value_t    *vv;
    ngx_proxy_wasm_stream_ctx_t  *sctx;
    ngx_http_wasm_req_ctx_t      *rctx;
    ngx_http_request_t           *r;
    static ngx_str_t              name = ngx_string("scheme");

    rctx = ngx_http_proxy_wasm_host_get_rctx(instance);
    r = rctx->r;

    sctx = ngx_proxy_wasm_host_get_sctx(instance);
    if (sctx->scheme.len) {
        return &sctx->scheme;
    }

    p = ngx_palloc(sctx->pool, name.len);
    if (p == NULL) {
        return NULL;
    }

    hash = ngx_hash_strlow(p, name.data, name.len);

    vv = ngx_http_get_variable(r, &name, hash);

    ngx_pfree(sctx->pool, p);

    if (vv == NULL || vv->not_found) {
        return NULL;
    }

    sctx->scheme.data = vv->data;
    sctx->scheme.len = vv->len;

    return &sctx->scheme;
}


static ngx_str_t *
ngx_proxy_wasm_maps_get_authority(ngx_wavm_instance_t *instance)
{
    u_char                       *p;
    ngx_uint_t                    port;
    ngx_str_t                    *server_name;
    ngx_proxy_wasm_stream_ctx_t  *sctx;
    ngx_http_core_srv_conf_t     *cscf;
    ngx_http_wasm_req_ctx_t      *rctx;
    ngx_http_request_t           *r;

    rctx = ngx_http_proxy_wasm_host_get_rctx(instance);
    r = rctx->r;

    sctx = ngx_proxy_wasm_host_get_sctx(instance);
    if (sctx->authority.len) {
        return &sctx->authority;
    }

    if (ngx_connection_local_sockaddr(r->connection, NULL, 0)
        != NGX_OK)
    {
        return NULL;
    }

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    server_name = &cscf->server_name;
    if (!server_name->len) {
        server_name = (ngx_str_t *) &ngx_cycle->hostname;
    }

    sctx->authority.len = server_name->len;

    port = ngx_inet_get_port(r->connection->local_sockaddr);
    if (port && port < 65536) {
        sctx->authority.len += 1 + sizeof("65535") - 1; /* ':' */
    }

    sctx->authority.data = ngx_pnalloc(sctx->pool, sctx->authority.len);
    if (sctx->authority.data == NULL) {
        return NULL;
    }

    p = ngx_sprintf(sctx->authority.data, "%V", server_name);

    if (port && port < 65536) {
        sctx->authority.len = ngx_sprintf(p, ":%ui", port)
                               - sctx->authority.data;
    }

    return &sctx->authority;
}
#endif
