
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CONNECTION_H_INCLUDED_
#define _NGX_CONNECTION_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_listening_s  ngx_listening_t;

//监听套接字的封装
struct ngx_listening_s {
    ngx_socket_t        fd;
    
    struct sockaddr    *sockaddr;
    socklen_t           socklen;    /* size of sockaddr */
    size_t              addr_text_max_len;  //存储ip地址的字符串addr_text的最大长度，即它指定了addr_text所分配的内存大小
    ngx_str_t           addr_text;  //以字符串形式存储ip地址
    
    int                 type;   //套接字类型，如TCP等
    
    int                 backlog;    //TCP实现监听时的backlog队列，它表示允许正在通过三次握手建立tcp连接但还没有任何进程开始处理的连接最大个数
    int                 rcvbuf; //内核中对于这个套接字的接收缓存区大小
    int                 sndbuf; ////内核中对于这个套接字的发送缓存区大小
#if (NGX_HAVE_KEEPALIVE_TUNABLE)
    int                 keepidle;
    int                 keepintvl;
    int                 keepcnt;
#endif
    
    /* handler of accepted connection */
    ngx_connection_handler_pt   handler;    //在这个监听端口上成功建立新的TCP连接后，就会调用这个方法
    
    void               *servers;  /* array of ngx_http_in_addr_t, for example */
    
    ngx_log_t           log;
    ngx_log_t          *logp;
    
    size_t              pool_size;
    /* should be here because of the AcceptEx() preread */
    size_t              post_accept_buffer_size;
    /* should be here because of the deferred accept */
    ngx_msec_t          post_accept_timeout;    //连接建立成功后，如果post_accept_timeout秒后仍然没有收到用户数据，则内核直接丢弃连接
    
    ngx_listening_t    *previous;
    ngx_connection_t   *connection;
    
    unsigned            open:1; //为1表示当前监听套接字有效，该标志位框架会自动设置
    unsigned            remain:1;   //为1表示使用已有的ngx_cycle_t来初始化新的ngx_cycle_t结构体时，不关闭原先打开的监听端口，这对运行中升级程序很有用，框架会自动设置这个标志位
    unsigned            ignore:1;
    
    unsigned            bound:1;       /* already bound */
    unsigned            inherited:1;   /* inherited from previous process 比如升级Nginx程序，一般会保留之前已经设置好的套接字，不做改变*/
    unsigned            nonblocking_accept:1;
    unsigned            listen:1;
    unsigned            nonblocking:1;
    unsigned            shared:1;    /* shared between threads or processes */
    unsigned            addr_ntop:1;    //为1时Nginx会将网络地址转变为字符串形式的地址
    
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
    unsigned            ipv6only:1;
#endif
    unsigned            keepalive:2;
    
#if (NGX_HAVE_DEFERRED_ACCEPT)
    unsigned            deferred_accept:1;
    unsigned            delete_deferred:1;
    unsigned            add_deferred:1;
#ifdef SO_ACCEPTFILTER
    char               *accept_filter;
#endif
#endif
#if (NGX_HAVE_SETFIB)
    int                 setfib;
#endif
    
#if (NGX_HAVE_TCP_FASTOPEN)
    int                 fastopen;
#endif
    
};


typedef enum {
    NGX_ERROR_ALERT = 0,
    NGX_ERROR_ERR,
    NGX_ERROR_INFO,
    NGX_ERROR_IGNORE_ECONNRESET,
    NGX_ERROR_IGNORE_EINVAL
} ngx_connection_log_error_e;


typedef enum {
    NGX_TCP_NODELAY_UNSET = 0,
    NGX_TCP_NODELAY_SET,
    NGX_TCP_NODELAY_DISABLED
} ngx_connection_tcp_nodelay_e;


typedef enum {
    NGX_TCP_NOPUSH_UNSET = 0,
    NGX_TCP_NOPUSH_SET,
    NGX_TCP_NOPUSH_DISABLED
} ngx_connection_tcp_nopush_e;


#define NGX_LOWLEVEL_BUFFERED  0x0f
#define NGX_SSL_BUFFERED       0x01
#define NGX_SPDY_BUFFERED      0x02

//表示被动连接，即nginx服务器进行侦听被动建立的连接
struct ngx_connection_s {
    /* 连接未使用时，data成员用于充当连接池中空闲连接链表中的next指针，当连接被使用时，data的意义由使用它的Nginx模块而定，如在HTTP框架中，data指向ngx_http_request_t请求 */
    void               *data;
    ngx_event_t        *read;   //连接对应的读事件
    ngx_event_t        *write;  //连接对应的写事件
    
    ngx_socket_t        fd; //套接字句柄
    
    ngx_recv_pt         recv;   //直接接收网络字符流的函数指针
    ngx_send_pt         send;   //直接发送网络字符流的函数指针
    ngx_recv_chain_pt   recv_chain; //链表为参数来接收网络字符流的函数指针
    ngx_send_chain_pt   send_chain; //链表为参数来发送网络字符流的函数指针
    
    ngx_listening_t    *listening;  //这个连接对应的ngx_listening_t监听对象，此连接由listening监听端口的事件建立
    
    off_t               sent;   //这个连接上已经发送出去的字节数
    
    ngx_log_t          *log;
    
    ngx_pool_t         *pool;   //一般在accept一个新连接时，会创建一个内存池，而在这个连接结束时会销毁内存池，这里所说的连接是指成功建立的TCP连接，所有的ngx_connection_t结构体都是预分配的，这个内存池的大小将由上面的listening监听对象中的pool_size成员决定
    
    struct sockaddr    *sockaddr;   //客户端的sockaddr结构体
    socklen_t           socklen;
    ngx_str_t           addr_text;  //客户端的ip字符串表示
    
    ngx_str_t           proxy_protocol_addr;
    
#if (NGX_SSL)
    ngx_ssl_connection_t  *ssl;
#endif
    
    struct sockaddr    *local_sockaddr; //本机监听端口对应的sockaddr结构体，也就是listening监听对象中的sockaddr成员
    socklen_t           local_socklen;
    
    ngx_buf_t          *buffer; //用于接收、缓存客户端发来的字符流，每个事件消费模块可自由决定从连接池中分配多大的空间给buffer这个接收缓存字段。例如，在http模块中，它的大小决定于client_header_buffer_size配置项
    
    ngx_queue_t         queue;  //该字段用来将当前连接以双向链表元素的形式添加到ngx_cycle_t核心结构体的reusable_connections_queue双向链表中，表示可以重用的连接
    
    ngx_atomic_uint_t   number; //连接的使用次数。ngx_connection_t结构体每次建立一条来自客户端的连接，或者用于主动向后端服务器发起连接（ngx_peer_connection_t里有一个ngx_connection_t成员）时，number都会加1
    
    ngx_uint_t          requests;   //处理的请求次数
    
    unsigned            buffered:8;
    
    unsigned            log_error:3;     /* ngx_connection_log_error_e */
    
    unsigned            unexpected_eof:1;
    unsigned            timedout:1;
    unsigned            error:1;
    unsigned            destroyed:1;    //为1时表示该结构体对应的套接字、内存池等已不可用
    
    unsigned            idle:1; //为1时表示连接处于空闲状态，如keepalive请求中两次请求之间的状态
    unsigned            reusable:1; //表示连接可重用，它与上面的queue字段是对应使用的
    unsigned            close:1;
    
    unsigned            sendfile:1;
    unsigned            sndlowat:1; //如果为1，则表示只有在连接套接字对应的发送缓冲区必须满足最低设置的大小阈值时，事件驱动模块才会分发该事件
    unsigned            tcp_nodelay:2;   /* ngx_connection_tcp_nodelay_e */
    unsigned            tcp_nopush:2;    /* ngx_connection_tcp_nopush_e */
    
    unsigned            need_last_buf:1;
    
#if (NGX_HAVE_IOCP)
    unsigned            accept_context_updated:1;
#endif
    
#if (NGX_HAVE_AIO_SENDFILE)
    unsigned            aio_sendfile:1;
    unsigned            busy_count:2;
    ngx_buf_t          *busy_sendfile;
#endif
    
#if (NGX_THREADS)
    ngx_atomic_t        lock;
#endif
};


ngx_listening_t *ngx_create_listening(ngx_conf_t *cf, void *sockaddr,
                                      socklen_t socklen);
ngx_int_t ngx_set_inherited_sockets(ngx_cycle_t *cycle);
ngx_int_t ngx_open_listening_sockets(ngx_cycle_t *cycle);
void ngx_configure_listening_sockets(ngx_cycle_t *cycle);
void ngx_close_listening_sockets(ngx_cycle_t *cycle);
void ngx_close_connection(ngx_connection_t *c);
ngx_int_t ngx_connection_local_sockaddr(ngx_connection_t *c, ngx_str_t *s,
                                        ngx_uint_t port);
ngx_int_t ngx_connection_error(ngx_connection_t *c, ngx_err_t err, char *text);

ngx_connection_t *ngx_get_connection(ngx_socket_t s, ngx_log_t *log);
void ngx_free_connection(ngx_connection_t *c);

void ngx_reusable_connection(ngx_connection_t *c, ngx_uint_t reusable);

#endif /* _NGX_CONNECTION_H_INCLUDED_ */
