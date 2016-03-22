//
//  ngx_kafka_module.c
//  ngx_anatomy
//
//  Created by 钟武 on 16/3/9.
//  Copyright © 2016年 boloomo. All rights reserved.
//

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <librdkafka/rdkafka.h>

static void *ngx_http_kafka_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_set_kafka_topic(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_set_kafka_broker(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_kafka_handler(ngx_http_request_t *r);
static void ngx_http_kafka_post_callback_handler(ngx_http_request_t *r);

typedef enum {
    ngx_str_push = 0,
    ngx_str_pop = 1
} ngx_str_op;

static void ngx_str_helper(ngx_str_t *str, ngx_str_op op);


typedef struct {
    ngx_str_t topic;
    ngx_str_t broker;
    
} ngx_http_kafka_loc_conf_t;

static ngx_command_t ngx_http_kafka_commands[] = {
    {
        ngx_string("kafka_topic"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_set_kafka_topic,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_kafka_loc_conf_t, topic),
        NULL },
    {
        ngx_string("kafka_broker"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_set_kafka_broker,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_kafka_loc_conf_t, broker),
        NULL },
    ngx_null_command
};

static ngx_http_module_t ngx_http_kafka_module_ctx = {
    NULL,      /* pre conf */
    NULL,      /* post conf */
    
    /* create main conf */
    NULL,
    NULL,      /* init main conf */
    
    NULL,      /* create server conf */
    NULL,      /* init server conf */
    
    /* create local conf */
    ngx_http_kafka_create_loc_conf,
    NULL,      /* merge location conf */
};

ngx_module_t ngx_http_kafka_module = {
    NGX_MODULE_V1,
    &ngx_http_kafka_module_ctx, /* module context */
    ngx_http_kafka_commands,    /* module directives */
    NGX_HTTP_MODULE,            /* module type */
    
    NULL,          /* init master */
    NULL,          /* init module */
    NULL,   /* init process */    //初始化worker进程
    NULL,          /* init thread */
    NULL,          /* exit thread */
    NULL,   /* exit process */
    NULL,          /* exit master */
    
    NGX_MODULE_V1_PADDING
};

ngx_int_t ngx_str_equal(ngx_str_t *s1, ngx_str_t *s2)
{
    if (s1->len != s1->len) {
        return 0;
    }
    if (ngx_memcmp(s1->data, s2->data, s1->len) != 0) {
        return 0;
    }
    return 1;
}

void *ngx_http_kafka_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_kafka_loc_conf_t  *conf;
    
    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_kafka_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }
    ngx_str_null(&conf->topic);
    ngx_str_null(&conf->broker);
    return conf;
}

void kafka_callback_handler(rd_kafka_t *rk, void *msg, size_t len, int err, void *opaque, void *msg_opaque)
{
    if (err != 0) {
        ngx_log_error(NGX_LOG_NOTICE, (ngx_log_t *)msg_opaque, 0, "produce errors");
    }
}

char *ngx_http_set_kafka_topic(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t   *clcf;
    
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    if (clcf == NULL) {
        return NGX_CONF_ERROR;
    }
    clcf->handler = ngx_http_kafka_handler;
    
    if (ngx_conf_set_str_slot(cf, cmd, conf) != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }
    
    return NGX_CONF_OK;
}

char *ngx_http_set_kafka_broker(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    if (ngx_conf_set_str_slot(cf, cmd, conf) != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }
    
    return NGX_CONF_OK;
}

static ngx_int_t ngx_http_kafka_handler(ngx_http_request_t *r)
{
    ngx_int_t  rv;
    
    if (!(r->method & NGX_HTTP_POST)) {
        return NGX_HTTP_NOT_ALLOWED;
    }
    
    rv = ngx_http_read_client_request_body(r, ngx_http_kafka_post_callback_handler);
    if (rv >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rv;
    }
    
    return NGX_DONE;
}

static void ngx_http_kafka_post_callback_handler(ngx_http_request_t *r)
{
    static const char rc[] = "ok\n";
    
    int                          nbufs;
    u_char                      *msg;
    size_t                       len;
    ngx_buf_t                   *buf;
    ngx_chain_t                  out;
    ngx_chain_t                 *cl, *in;
    ngx_http_request_body_t     *body;
    ngx_http_kafka_loc_conf_t   *local_conf;
    
    /* get body */
    body = r->request_body;
    if (body == NULL || body->bufs == NULL) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }
    
    rd_kafka_conf_t *rkc = rd_kafka_conf_new();
    rd_kafka_conf_set_dr_cb(rkc, kafka_callback_handler);
    
    rd_kafka_t *rk = rd_kafka_new(RD_KAFKA_PRODUCER, rkc, NULL, 0);
    
    rd_kafka_topic_conf_t *rktc = rd_kafka_topic_conf_new();
    rd_kafka_topic_t *rkt = NULL;

    local_conf = ngx_http_get_module_loc_conf(r, ngx_http_kafka_module);
    
    ngx_str_helper(&local_conf->broker, ngx_str_push);
    if(rd_kafka_brokers_add(rk, (const char *)local_conf->broker.data) < 1){
        ngx_log_error(NGX_LOG_NOTICE, r->connection->log,0,"no valid brokers specified\n");
    }
    ngx_str_helper(&local_conf->broker, ngx_str_pop);
    
    ngx_str_helper(&local_conf->topic, ngx_str_push);
    rkt = rd_kafka_topic_new(rk,
                             (const char *)local_conf->topic.data, rktc);
    ngx_str_helper(&local_conf->topic, ngx_str_pop);
    
    /* calc len and bufs */
    len = 0;
    nbufs = 0;
    in = body->bufs;
    for (cl = in; cl != NULL; cl = cl->next) {
        nbufs++;
        len += (size_t)(cl->buf->last - cl->buf->pos);
    }
    
    /* get msg */
    if (nbufs == 0) {
        goto end;
    }
    
    if (nbufs == 1 && ngx_buf_in_memory(in->buf)) {
        
        msg = in->buf->pos;
        
    } else {
        
        if ((msg = ngx_pnalloc(r->pool, len)) == NULL) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        for (cl = in; cl != NULL; cl = cl->next) {
            if (ngx_buf_in_memory(cl->buf)) {
                msg = ngx_copy(msg, cl->buf->pos, cl->buf->last - cl->buf->pos);
            } else {
                /* TODO: handle buf in file */
                ngx_log_error(NGX_LOG_NOTICE, r->connection->log, 0,
                              "ngx_http_kafka_handler cannot handler in-file-post-buf");
                goto end;
            }
        }
        
        msg -= len;
        
    }
    
    
    if(rd_kafka_produce(rkt, RD_KAFKA_PARTITION_UA,
                     RD_KAFKA_MSG_F_COPY, (void *)msg, len, NULL, 0, r->connection->log) == 0)
    {
        ngx_log_error(NGX_LOG_NOTICE, r->connection->log,0,"produce error %d\n",len);
    }
    
end:
    
    buf = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    out.buf = buf;
    out.next = NULL;
    buf->pos = (u_char *)rc;
    buf->last = (u_char *)rc + sizeof(rc) - 1;
    buf->memory = 1;
    buf->last_buf = 1;
    
    ngx_str_set(&(r->headers_out.content_type), "text/html");
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = sizeof(rc) - 1;
    ngx_http_send_header(r);
    ngx_http_output_filter(r, &out);
    ngx_http_finalize_request(r, NGX_OK);
    
    rd_kafka_poll(rk, 0);
    
    rd_kafka_topic_destroy(rkt);
    rd_kafka_destroy(rk);
}

void ngx_str_helper(ngx_str_t *str, ngx_str_op op)
{
    static char backup;
    
    switch (op) {
        case ngx_str_push:
            backup = str->data[str->len];
            str->data[str->len] = 0;
            break;
        case ngx_str_pop:
            str->data[str->len] = backup;
            break;
        default:
            ngx_abort();
    }
}


