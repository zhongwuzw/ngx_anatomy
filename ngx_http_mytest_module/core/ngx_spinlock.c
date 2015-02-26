
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>

//实现自旋锁
void
ngx_spinlock(ngx_atomic_t *lock, ngx_atomic_int_t value, ngx_uint_t spin)
{

#if (NGX_HAVE_ATOMIC_OPS)

    ngx_uint_t  i, n;
    
    //无法获取锁时进程的代码将一直在这个循环中执行
    for ( ;; ) {
        //lock为0表示锁是没有被其他进程持有的，这时将lock值设为value参数表示当前进程持有了锁
        if (*lock == 0 && ngx_atomic_cmp_set(lock, 0, value)) {
            return; //获取锁后返回
        }

        //ngx_ncpu是处理器的个数，大于1表示出于多处理器系统中
        if (ngx_ncpu > 1) {
            
            //n <<= 1的意思是将n左移一位然后赋给n
            for (n = 1; n < spin; n <<= 1) {
                //ngx_cpu_pause是许多架构体系中专门为了自旋锁而提供的指令，它会告诉CPU现在处于自旋锁等待状态，通常一些CPU会将自己置于节能状态，降低功耗。注意，在执行ngx_cpu_pause后，当前进程没有让出正使用的处理器
                for (i = 0; i < n; i++) {
                    ngx_cpu_pause();
                }

                if (*lock == 0 && ngx_atomic_cmp_set(lock, 0, value)) {
                    return;
                }
            }
        }
        //ngx_sched_yield暂时让出处理器
        ngx_sched_yield();
    }

#else

#if (NGX_THREADS)

#error ngx_spinlock() or ngx_atomic_cmp_set() are not defined !

#endif

#endif

}
