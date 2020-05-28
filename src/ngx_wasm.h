/*
 * Copyright (C) Thibault Charbonnier
 */

#ifndef _NGX_WASM_H_INCLUDED_
#define _NGX_WASM_H_INCLUDED_


#if !(NGX_HAVE_VARIADIC_MACROS)
#   error "NYI: ngx_wasm_module requires C99 variadic macros"
#endif


#include <nginx.h>
#include <ngx_core.h>


#define NGX_WASM_MODULE       0x5741534d   /* "MSAW" */
#define NGX_WASM_CONF         0x00300000


#ifdef NGX_WASM_USE_ASSERT
#include <assert.h>
#   define ngx_wasm_assert(a)  assert(a)
#else
#   define ngx_wasm_assert(a)
#endif


#define NGX_LOG_DEBUG_WASM  NGX_LOG_DEBUG_CORE


typedef struct {
    ngx_pool_t         *pool;
    ngx_log_t          *log;
    void               *wvm;
} ngx_wasm_wvm_t;


typedef struct {
    ngx_str_t           path;
    ngx_wasm_wvm_t     *vm;
    void               *wmodule;
} ngx_wasm_wmodule_t;


typedef struct {
    ngx_wasm_wmodule_t  *module;
    void                *winstance;
} ngx_wasm_winstance_t;


typedef struct {
    ngx_int_t       (*init_vm)(ngx_wasm_wvm_t *vm, ngx_cycle_t *cycle);
    void            (*shutdown_vm)(ngx_wasm_wvm_t *vm);
    ngx_int_t       (*load_wasm_module)(ngx_wasm_wmodule_t *module,
                                        ngx_wasm_wvm_t *vm, const u_char *path);
    void            (*release_module)(ngx_wasm_wmodule_t *module);
    ngx_int_t       (*init_instance)(ngx_wasm_winstance_t *instance,
                                     ngx_wasm_wmodule_t *module);
    void            (*shutdown_instance)(ngx_wasm_winstance_t *instance);
    ngx_int_t       (*run_entrypoint)(ngx_wasm_winstance_t *instance,
                                      const u_char *name);
} ngx_wasm_vm_actions_t;


typedef struct {
    ngx_str_t                   *name;
    void                        *(*create_conf)(ngx_cycle_t *cycle);
    char                        *(*init_conf)(ngx_cycle_t *cycle, void *conf);
    ngx_int_t                    (*init)(ngx_cycle_t *cycle);
    ngx_wasm_vm_actions_t        vm_actions;
} ngx_wasm_module_t;


typedef struct {
    ngx_uint_t                   vm;
    u_char                      *vm_name;
} ngx_wasm_core_conf_t;


extern ngx_wasm_vm_actions_t     ngx_wasm_vm_actions;
extern ngx_module_t              ngx_wasm_module;


#endif /* _NGX_WASM_H_INCLUDED_ */


/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
