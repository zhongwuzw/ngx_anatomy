
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_H_INCLUDED_
#define _NGX_EVENT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#define NGX_INVALID_INDEX  0xd0d0d0d0


#if (NGX_HAVE_IOCP)

typedef struct {
    WSAOVERLAPPED    ovlp;
    ngx_event_t     *event;
    int              error;
} ngx_event_ovlp_t;

#endif


typedef struct {
    ngx_uint_t       lock;

    ngx_event_t     *events;
    ngx_event_t     *last;
} ngx_event_mutex_t;

//每一个事件都有一个这样的结构体来表示
struct ngx_event_s {
    void            *data;  //事件相关的对象，通常data都是指向ngx_connection_t连接对象，开启文件异步I/O时，它可能指向ngx_event_aio_t结构体

    unsigned         write:1;   //为1时表示事件是可写的，通常情况下，它表示对应的TCP连接目前状态是可写的，也就是连接处于可以发送网络包的状态

    unsigned         accept:1;  //表示可以建立新的连接，通常情况下，只有监听套接字事件这个标志位才可能为1

    /* used to detect the stale events in kqueue, rtsig, and epoll */
    unsigned         instance:1;    //用于区分当前事件是否过期,至于过期事件到底是怎么回事，看一下我的印象笔记

    /*
     * the event was passed or would be passed to a kernel;
     * in aio mode - operation was posted.
     */
    unsigned         active:1;

    unsigned         disabled:1;    //为1时表示禁用事件，仅在kqueue或者rtsig事件驱动模块中有效，对epoll无意义

    /* the ready event; in aio mode 0 means that no operation can be posted */
    unsigned         ready:1;   //为1表示当前事件准备就绪，即允许这个事件的消费模块处理这个事件，在http框架中，经常会检查事件的ready标志位以确定是否可以接收请求或者发送响应

    unsigned         oneshot:1; //适用于kqueue，不适用于epoll

    /* aio operation is complete */
    unsigned         complete:1;    //用于异步AIO事件的处理

    unsigned         eof:1; //为1表示当前处理的字符流已经结束
    unsigned         error:1;   //为1表示事件在处理过程中出现错误

    unsigned         timedout:1;    //为1时表示这个事件已经超时，用以提示事件的消费模块做超时处理
    unsigned         timer_set:1;   //为1时表示这个事件存在与定时器中

    unsigned         delayed:1;

    unsigned         deferred_accept:1; //为1时表示延迟建立TCP连接，也就是说，经过TCP三次握手后并不建立连接，而是要等到真正收到数据报后才会建立TCP连接

    /* the pending eof reported by kqueue, epoll or in aio chain operation */
    unsigned         pending_eof:1; //为1时表示等待字符流结束

#if !(NGX_THREADS)
    unsigned         posted_ready:1;    //为1表示在处理post事件时，当前事件已经准备就绪
#endif

#if (NGX_WIN32)
    /* setsockopt(SO_UPDATE_ACCEPT_CONTEXT) was successful */
    unsigned         accept_context_updated:1;
#endif

#if (NGX_HAVE_KQUEUE)
    unsigned         kq_vnode:1;

    /* the pending errno reported by kqueue */
    int              kq_errno;
#endif

    /*
     * kqueue only:
     *   accept:     number of sockets that wait to be accepted
     *   read:       bytes to read when event is ready
     *               or lowat when event is set with NGX_LOWAT_EVENT flag
     *   write:      available space in buffer when event is ready
     *               or lowat when event is set with NGX_LOWAT_EVENT flag
     *
     * iocp: TODO
     *
     * otherwise:
     *   accept:     1 if accept many, 0 otherwise
     */

#if (NGX_HAVE_KQUEUE) || (NGX_HAVE_IOCP)
    int              available;
#else
    unsigned         available:1;   //在epoll事件驱动机制下表示每一次尽可能多的建立tcp连接，它与multi_accept配置项对应,不理解的话看一下multi_accept配置项的解释
#endif

    ngx_event_handler_pt  handler;  //这个事件发生时的处理方法，每个事件消费模块都会重新实现它


#if (NGX_HAVE_AIO)

#if (NGX_HAVE_IOCP)
    ngx_event_ovlp_t ovlp;
#else
    struct aiocb     aiocb;
#endif

#endif

    ngx_uint_t       index;

    ngx_log_t       *log;

    ngx_rbtree_node_t   timer;  //定时器节点，用于定时器红黑树中

    unsigned         closed:1;

    /* to test on worker exit */
    unsigned         channel:1;
    unsigned         resolver:1;

#if (NGX_THREADS)

    unsigned         locked:1;

    unsigned         posted_ready:1;
    unsigned         posted_timedout:1;
    unsigned         posted_eof:1;

#if (NGX_HAVE_KQUEUE)
    /* the pending errno reported by kqueue */
    int              posted_errno;
#endif

#if (NGX_HAVE_KQUEUE) || (NGX_HAVE_IOCP)
    int              posted_available;
#else
    unsigned         posted_available:1;
#endif

    ngx_atomic_t    *lock;
    ngx_atomic_t    *own_lock;

#endif

    /* the links of the posted queue */
    ngx_event_t     *next;  //post事件将会构成一个队列再统一处理，这个队列以next和prev作为链表指针，一次构成一个简易的双向链表
    ngx_event_t    **prev;


#if 0

    /* the threads support */

    /*
     * the event thread context, we store it here
     * if $(CC) does not understand __thread declaration
     * and pthread_getspecific() is too costly
     */

    void            *thr_ctx;

#if (NGX_EVENT_T_PADDING)

    /* event should not cross cache line in SMP */

    uint32_t         padding[NGX_EVENT_T_PADDING];
#endif
#endif
};


#if (NGX_HAVE_FILE_AIO)

struct ngx_event_aio_s {
    void                      *data;
    ngx_event_handler_pt       handler;
    ngx_file_t                *file;

    ngx_fd_t                   fd;

#if (NGX_HAVE_EVENTFD)
    int64_t                    res;
#if (NGX_TEST_BUILD_EPOLL)
    ngx_err_t                  err;
    size_t                     nbytes;
#endif
#else
    ngx_err_t                  err;
    size_t                     nbytes;
#endif

#if (NGX_HAVE_AIO_SENDFILE)
    off_t                      last_offset;
#endif

    ngx_aiocb_t                aiocb;
    ngx_event_t                event;
};

#endif


typedef struct {
    /* 添加事件方法，它将负责把1个感兴趣的事件添加到操作系统提供的事件驱动机制（如epoll、kqueue等）中，这样，在事件发生后，将可以在调用下面的process_events时获取这个事件 */
    ngx_int_t  (*add)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);
    /* 删除事件方法，它将把1个已经存在于事件驱动机制中的事件移除，这样以后即使这个事件发生，调用process_events方法时也无法再获取这个事件 */
    ngx_int_t  (*del)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);
    
    ngx_int_t  (*enable)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);
    ngx_int_t  (*disable)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);
    
    /* 向事件驱动机制中添加一个新的连接，这意味着连接上的读写事件都添加到事件驱动机制中了 */
    ngx_int_t  (*add_conn)(ngx_connection_t *c);
    /* 从事件驱动机制中移除一个连接的读写事件 */
    ngx_int_t  (*del_conn)(ngx_connection_t *c, ngx_uint_t flags);
    
    /* 该方法仅在多线程环境下会被调用，目前没啥用 */
    ngx_int_t  (*process_changes)(ngx_cycle_t *cycle, ngx_uint_t nowait);
    /* 在工作循环中，将通过调用process_events方法来处理事件，该方法仅在ngx_process_events_and_timers方法中被调用，它是处理、分发事件的核心 */
    ngx_int_t  (*process_events)(ngx_cycle_t *cycle, ngx_msec_t timer,
                                 ngx_uint_t flags);
    /* 初始化事件驱动模块的方法 */  //该方法的调用时间为Nginx启动过程中
    ngx_int_t  (*init)(ngx_cycle_t *cycle, ngx_msec_t timer);
    /* 退出事件驱动模块前调用的方法 */
    void       (*done)(ngx_cycle_t *cycle);
} ngx_event_actions_t;


extern ngx_event_actions_t   ngx_event_actions;


/*
 * The event filter requires to read/write the whole data:
 * select, poll, /dev/poll, kqueue, epoll.
 */
#define NGX_USE_LEVEL_EVENT      0x00000001

/*
 * The event filter is deleted after a notification without an additional
 * syscall: kqueue, epoll.
 */
#define NGX_USE_ONESHOT_EVENT    0x00000002

/*
 * The event filter notifies only the changes and an initial level:
 * kqueue, epoll.
 */
#define NGX_USE_CLEAR_EVENT      0x00000004

/*
 * The event filter has kqueue features: the eof flag, errno,
 * available data, etc.
 */
#define NGX_USE_KQUEUE_EVENT     0x00000008

/*
 * The event filter supports low water mark: kqueue's NOTE_LOWAT.
 * kqueue in FreeBSD 4.1-4.2 has no NOTE_LOWAT so we need a separate flag.
 */
#define NGX_USE_LOWAT_EVENT      0x00000010

/*
 * The event filter requires to do i/o operation until EAGAIN: epoll, rtsig.
 */
#define NGX_USE_GREEDY_EVENT     0x00000020

/*
 * The event filter is epoll.
 */
#define NGX_USE_EPOLL_EVENT      0x00000040

/*
 * No need to add or delete the event filters: rtsig.
 */
#define NGX_USE_RTSIG_EVENT      0x00000080

/*
 * No need to add or delete the event filters: overlapped, aio_read,
 * aioread, io_submit.
 */
#define NGX_USE_AIO_EVENT        0x00000100

/*
 * Need to add socket or handle only once: i/o completion port.
 * It also requires NGX_HAVE_AIO and NGX_USE_AIO_EVENT to be set.
 */
#define NGX_USE_IOCP_EVENT       0x00000200

/*
 * The event filter has no opaque data and requires file descriptors table:
 * poll, /dev/poll, rtsig.
 */
#define NGX_USE_FD_EVENT         0x00000400

/*
 * The event module handles periodic or absolute timer event by itself:
 * kqueue in FreeBSD 4.4, NetBSD 2.0, and MacOSX 10.4, Solaris 10's event ports.
 */
#define NGX_USE_TIMER_EVENT      0x00000800

/*
 * All event filters on file descriptor are deleted after a notification:
 * Solaris 10's event ports.
 */
#define NGX_USE_EVENTPORT_EVENT  0x00001000

/*
 * The event filter support vnode notifications: kqueue.
 */
#define NGX_USE_VNODE_EVENT      0x00002000


/*
 * The event filter is deleted just before the closing file.
 * Has no meaning for select and poll.
 * kqueue, epoll, rtsig, eventport:  allows to avoid explicit delete,
 *                                   because filter automatically is deleted
 *                                   on file close,
 *
 * /dev/poll:                        we need to flush POLLREMOVE event
 *                                   before closing file.
 */
#define NGX_CLOSE_EVENT    1

/*
 * disable temporarily event filter, this may avoid locks
 * in kernel malloc()/free(): kqueue.
 */
#define NGX_DISABLE_EVENT  2

/*
 * event must be passed to kernel right now, do not wait until batch processing.
 */
#define NGX_FLUSH_EVENT    4


/* these flags have a meaning only for kqueue */
#define NGX_LOWAT_EVENT    0
#define NGX_VNODE_EVENT    0


#if (NGX_HAVE_EPOLL) && !(NGX_HAVE_EPOLLRDHUP)
#define EPOLLRDHUP         0
#endif


#if (NGX_HAVE_KQUEUE)

#define NGX_READ_EVENT     EVFILT_READ
#define NGX_WRITE_EVENT    EVFILT_WRITE

#undef  NGX_VNODE_EVENT
#define NGX_VNODE_EVENT    EVFILT_VNODE

/*
 * NGX_CLOSE_EVENT, NGX_LOWAT_EVENT, and NGX_FLUSH_EVENT are the module flags
 * and they must not go into a kernel so we need to choose the value
 * that must not interfere with any existent and future kqueue flags.
 * kqueue has such values - EV_FLAG1, EV_EOF, and EV_ERROR:
 * they are reserved and cleared on a kernel entrance.
 */
#undef  NGX_CLOSE_EVENT
#define NGX_CLOSE_EVENT    EV_EOF

#undef  NGX_LOWAT_EVENT
#define NGX_LOWAT_EVENT    EV_FLAG1

#undef  NGX_FLUSH_EVENT
#define NGX_FLUSH_EVENT    EV_ERROR

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  EV_ONESHOT
#define NGX_CLEAR_EVENT    EV_CLEAR

#undef  NGX_DISABLE_EVENT
#define NGX_DISABLE_EVENT  EV_DISABLE


#elif (NGX_HAVE_DEVPOLL || NGX_HAVE_EVENTPORT)

#define NGX_READ_EVENT     POLLIN
#define NGX_WRITE_EVENT    POLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1


#elif (NGX_HAVE_EPOLL)

#define NGX_READ_EVENT     (EPOLLIN|EPOLLRDHUP)
#define NGX_WRITE_EVENT    EPOLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_CLEAR_EVENT    EPOLLET
#define NGX_ONESHOT_EVENT  0x70000000
#if 0
#define NGX_ONESHOT_EVENT  EPOLLONESHOT
#endif


#elif (NGX_HAVE_POLL)

#define NGX_READ_EVENT     POLLIN
#define NGX_WRITE_EVENT    POLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1


#else /* select */

#define NGX_READ_EVENT     0
#define NGX_WRITE_EVENT    1

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1

#endif /* NGX_HAVE_KQUEUE */


#if (NGX_HAVE_IOCP)
#define NGX_IOCP_ACCEPT      0
#define NGX_IOCP_IO          1
#define NGX_IOCP_CONNECT     2
#endif


#ifndef NGX_CLEAR_EVENT
#define NGX_CLEAR_EVENT    0    /* dummy declaration */
#endif


#define ngx_process_changes  ngx_event_actions.process_changes
#define ngx_process_events   ngx_event_actions.process_events
#define ngx_done_events      ngx_event_actions.done

#define ngx_add_event        ngx_event_actions.add
#define ngx_del_event        ngx_event_actions.del
#define ngx_add_conn         ngx_event_actions.add_conn
#define ngx_del_conn         ngx_event_actions.del_conn

#define ngx_add_timer        ngx_event_add_timer
#define ngx_del_timer        ngx_event_del_timer


extern ngx_os_io_t  ngx_io;

#define ngx_recv             ngx_io.recv
#define ngx_recv_chain       ngx_io.recv_chain
#define ngx_udp_recv         ngx_io.udp_recv
#define ngx_send             ngx_io.send
#define ngx_send_chain       ngx_io.send_chain


#define NGX_EVENT_MODULE      0x544E5645  /* "EVNT" */
#define NGX_EVENT_CONF        0x02000000


typedef struct {
    ngx_uint_t    connections;  //连接池的大小
    ngx_uint_t    use;  //选用的事件模块在所有事件模块中的序号，也就是ctx_index成员（每一个模块都有这个成员）
    
    ngx_flag_t    multi_accept;
    ngx_flag_t    accept_mutex;
    
    ngx_msec_t    accept_mutex_delay;
    
    u_char       *name;
    
#if (NGX_DEBUG)
    ngx_array_t   debug_connection;
#endif
} ngx_event_conf_t;


typedef struct {
    ngx_str_t              *name;
    
    //在解析配置项前，这个回调方法用于创建存储配置项参数的结构体
    void                 *(*create_conf)(ngx_cycle_t *cycle);
    //在解析配置项完成后，init_conf方法会被调用，用以综合处理当前事件模块感兴趣的全部配置项
    char                 *(*init_conf)(ngx_cycle_t *cycle, void *conf);
    
    ngx_event_actions_t     actions;    //每个事件模块需要实现的10个抽象方法
} ngx_event_module_t;   //事件模块的通用结构体，其作为模块的ctx成员保存


extern ngx_atomic_t          *ngx_connection_counter;

extern ngx_atomic_t          *ngx_accept_mutex_ptr;
extern ngx_shmtx_t            ngx_accept_mutex;
extern ngx_uint_t             ngx_use_accept_mutex;
extern ngx_uint_t             ngx_accept_events;
extern ngx_uint_t             ngx_accept_mutex_held;
extern ngx_msec_t             ngx_accept_mutex_delay;
extern ngx_int_t              ngx_accept_disabled;


#if (NGX_STAT_STUB)

extern ngx_atomic_t  *ngx_stat_accepted;
extern ngx_atomic_t  *ngx_stat_handled;
extern ngx_atomic_t  *ngx_stat_requests;
extern ngx_atomic_t  *ngx_stat_active;
extern ngx_atomic_t  *ngx_stat_reading;
extern ngx_atomic_t  *ngx_stat_writing;
extern ngx_atomic_t  *ngx_stat_waiting;

#endif


#define NGX_UPDATE_TIME         1
#define NGX_POST_EVENTS         2
#define NGX_POST_THREAD_EVENTS  4


extern sig_atomic_t           ngx_event_timer_alarm;
extern ngx_uint_t             ngx_event_flags;
extern ngx_module_t           ngx_events_module;
extern ngx_module_t           ngx_event_core_module;


#define ngx_event_get_conf(conf_ctx, module)                                  \
             (*(ngx_get_conf(conf_ctx, ngx_events_module))) [module.ctx_index];



void ngx_event_accept(ngx_event_t *ev);
ngx_int_t ngx_trylock_accept_mutex(ngx_cycle_t *cycle);
u_char *ngx_accept_log_error(ngx_log_t *log, u_char *buf, size_t len);


void ngx_process_events_and_timers(ngx_cycle_t *cycle);
ngx_int_t ngx_handle_read_event(ngx_event_t *rev, ngx_uint_t flags);
ngx_int_t ngx_handle_write_event(ngx_event_t *wev, size_t lowat);


#if (NGX_WIN32)
void ngx_event_acceptex(ngx_event_t *ev);
ngx_int_t ngx_event_post_acceptex(ngx_listening_t *ls, ngx_uint_t n);
u_char *ngx_acceptex_log_error(ngx_log_t *log, u_char *buf, size_t len);
#endif


ngx_int_t ngx_send_lowat(ngx_connection_t *c, size_t lowat);


/* used in ngx_log_debugX() */
#define ngx_event_ident(p)  ((ngx_connection_t *) (p))->fd


#include <ngx_event_timer.h>
#include <ngx_event_posted.h>
#include <ngx_event_busy_lock.h>

#if (NGX_WIN32)
#include <ngx_iocp_module.h>
#endif


#endif /* _NGX_EVENT_H_INCLUDED_ */
