
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_CONNECT_H_INCLUDED_
#define _NGX_EVENT_CONNECT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


#define NGX_PEER_KEEPALIVE           1
#define NGX_PEER_NEXT                2
#define NGX_PEER_FAILED              4


typedef struct ngx_peer_connection_s  ngx_peer_connection_t;
//当使用长连接与上游服务器通信时，可通过该方法由连接池中获取一个新连接
typedef ngx_int_t (*ngx_event_get_peer_pt)(ngx_peer_connection_t *pc,
                                           void *data);
//通信完成后，通过该方法将使用完毕的连接释放给连接池
typedef void (*ngx_event_free_peer_pt)(ngx_peer_connection_t *pc, void *data,
ngx_uint_t state);
#if (NGX_SSL)

typedef ngx_int_t (*ngx_event_set_peer_session_pt)(ngx_peer_connection_t *pc,
                                                   void *data);
typedef void (*ngx_event_save_peer_session_pt)(ngx_peer_connection_t *pc,
void *data);
#endif

//表示主动连接，如Nginx试图主动向其他上游服务器建立连接通信
struct ngx_peer_connection_s {
    ngx_connection_t                *connection;
    
    struct sockaddr                 *sockaddr;  //远程服务器的socket地址
    socklen_t                        socklen;
    ngx_str_t                       *name;  //远程服务器的名称
    
    ngx_uint_t                       tries; //表示在连接一个远端服务器时，当前连接出现异常失败后可以重试的次数，也就是允许的最多失败次数
    
    ngx_event_get_peer_pt            get;
    ngx_event_free_peer_pt           free;
    void                            *data;  //配合上面的get、free方法的参数传递
    
#if (NGX_SSL)
    ngx_event_set_peer_session_pt    set_session;
    ngx_event_save_peer_session_pt   save_session;
#endif
    
#if (NGX_THREADS)
    ngx_atomic_t                    *lock;
#endif
    
    ngx_addr_t                      *local; //本机地址信息
    
    int                              rcvbuf;    //套接字的接收缓冲区大小
    
    ngx_log_t                       *log;
    
    unsigned                         cached:1;
    
    /* ngx_connection_log_error_e */
    unsigned                         log_error:2;
};


ngx_int_t ngx_event_connect_peer(ngx_peer_connection_t *pc);
ngx_int_t ngx_event_get_peer(ngx_peer_connection_t *pc, void *data);


#endif /* _NGX_EVENT_CONNECT_H_INCLUDED_ */
