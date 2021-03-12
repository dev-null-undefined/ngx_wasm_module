#ifndef _NGX_WRT_H_INCLUDED_
#define _NGX_WRT_H_INCLUDED_


#include <ngx_core.h>


#if NGX_WASM_HAVE_WASMTIME
#include <wasm.h>
#include <wasmtime.h>


typedef struct wasmtime_error_t  ngx_wrt_res_t;
#elif NGX_WASM_HAVE_WASMER
#include <wasmer_wasm.h>


typedef ngx_str_t  ngx_wrt_res_t;
#endif /* NGX_WASM_HAVE_* */


typedef struct {
    wasm_trap_t     *trap;
    ngx_wrt_res_t   *res;
} ngx_wavm_err_t;


void ngx_wrt_config_init(wasm_config_t *config);
ngx_int_t ngx_wrt_wat2wasm(wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm,
    ngx_wavm_err_t *err);
ngx_int_t ngx_wrt_module_validate(wasm_store_t *s, wasm_byte_vec_t *bytes,
    ngx_wavm_err_t *err);
#if NGX_WASM_HAVE_WASMTIME
ngx_int_t ngx_wrt_module_new(wasm_engine_t *e, wasm_byte_vec_t *bytes,
    wasm_module_t **out, ngx_wavm_err_t *err);
#else
ngx_int_t ngx_wrt_module_new(wasm_store_t *s, wasm_byte_vec_t *bytes,
    wasm_module_t **out, ngx_wavm_err_t *err);
#endif
ngx_int_t ngx_wrt_instance_new(wasm_store_t *s, wasm_module_t *m,
    wasm_extern_vec_t *imports, wasm_instance_t **instance,
    ngx_wavm_err_t *err);
ngx_int_t ngx_wrt_func_call(wasm_func_t *f, wasm_val_vec_t *args,
    wasm_val_vec_t *rets, ngx_wavm_err_t *err);
u_char *ngx_wrt_error_log_handler(ngx_wrt_res_t *res, u_char *buf, size_t len);


#endif /* _NGX_WRT_H_INCLUDED_ */