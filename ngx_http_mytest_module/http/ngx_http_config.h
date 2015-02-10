
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_CONFIG_H_INCLUDED_
#define _NGX_HTTP_CONFIG_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

//核心结构体ngx_cycle_t的conf_ctx成员指向的指针数组中，第七个指针由ngx_http_module模块使用（可参考ngx_modules数组），这个指针就会指向解析http{}块时生成的ngx_http_conf_ctx_t结构体,解析http、server、location块时，都会创建一个这样的结构体，比如解析到server块时，创建这个结构体，将该结构体的main_conf指向所属的http块下ngx_http_conf_t结构体的main_conf指针数组，而srv_conf、loc_conf都将重新分配指针数组
//这方面的流程怎么走，可参看《深入理解Nginx...》第356页
typedef struct {
    //指向一个指针数组，数组中的每个成员都是由所有http模块的create_main_conf方法创建的存放全局配置项的结构体，他们存放着直接解析直属于http{}块内的main级别的配置项参数
    void        **main_conf;
    //指向一个指针数组，数组中的每个成员都是由所有http模块的create_srv_conf方法创建的与server相关的结构体，它们或存放main级别配置项，或存放srv级别配置项
    void        **srv_conf;
    void        **loc_conf;
} ngx_http_conf_ctx_t;


typedef struct {
    //在解析http{...}内的配置项前回调
    ngx_int_t   (*preconfiguration)(ngx_conf_t *cf);
    //解析完http{...}内的所有配置项后回调
    ngx_int_t   (*postconfiguration)(ngx_conf_t *cf);

    //创建用于存储http模块的全局配置项的结构体，该结构体中的成员将保存直属于http{}块的配置项参数，在解析main配置项之前调用
    void       *(*create_main_conf)(ngx_conf_t *cf);
    //解析完main配置项后回调
    char       *(*init_main_conf)(ngx_conf_t *cf, void *conf);

    //创建用于存储可同时出现在main、srv级别配置项的结构体，该结构体中的成员与server配置是相关联的
    void       *(*create_srv_conf)(ngx_conf_t *cf);
    //create_srv_conf产生的结构体所要解析的配置项，可能同时出现在main、srv级别中，merge_srv_conf方法可以把出现在main级别中的配置项值合并到srv级别配置项中
    char       *(*merge_srv_conf)(ngx_conf_t *cf, void *prev, void *conf);

    //创建用于存储可同时出现在main、srv、loc级别配置项的结构体，该结构体中的成员与location配置是相关联的
    void       *(*create_loc_conf)(ngx_conf_t *cf);
    //把同时出现在main、srv级别的配置项合并到loc级别的配置项中
    char       *(*merge_loc_conf)(ngx_conf_t *cf, void *prev, void *conf);
} ngx_http_module_t;


#define NGX_HTTP_MODULE           0x50545448   /* "HTTP" */

#define NGX_HTTP_MAIN_CONF        0x02000000
#define NGX_HTTP_SRV_CONF         0x04000000
#define NGX_HTTP_LOC_CONF         0x08000000
#define NGX_HTTP_UPS_CONF         0x10000000
#define NGX_HTTP_SIF_CONF         0x20000000
#define NGX_HTTP_LIF_CONF         0x40000000
#define NGX_HTTP_LMT_CONF         0x80000000


#define NGX_HTTP_MAIN_CONF_OFFSET  offsetof(ngx_http_conf_ctx_t, main_conf)
#define NGX_HTTP_SRV_CONF_OFFSET   offsetof(ngx_http_conf_ctx_t, srv_conf)
#define NGX_HTTP_LOC_CONF_OFFSET   offsetof(ngx_http_conf_ctx_t, loc_conf)


#define ngx_http_get_module_main_conf(r, module)                             \
    (r)->main_conf[module.ctx_index]
#define ngx_http_get_module_srv_conf(r, module)  (r)->srv_conf[module.ctx_index]
#define ngx_http_get_module_loc_conf(r, module)  (r)->loc_conf[module.ctx_index]


#define ngx_http_conf_get_module_main_conf(cf, module)                        \
    ((ngx_http_conf_ctx_t *) cf->ctx)->main_conf[module.ctx_index]
#define ngx_http_conf_get_module_srv_conf(cf, module)                         \
    ((ngx_http_conf_ctx_t *) cf->ctx)->srv_conf[module.ctx_index]
#define ngx_http_conf_get_module_loc_conf(cf, module)                         \
    ((ngx_http_conf_ctx_t *) cf->ctx)->loc_conf[module.ctx_index]

#define ngx_http_cycle_get_module_main_conf(cycle, module)                    \
    (cycle->conf_ctx[ngx_http_module.index] ?                                 \
        ((ngx_http_conf_ctx_t *) cycle->conf_ctx[ngx_http_module.index])      \
            ->main_conf[module.ctx_index]:                                    \
        NULL)


#endif /* _NGX_HTTP_CONFIG_H_INCLUDED_ */
