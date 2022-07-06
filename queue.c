#include "memcached.h"
#include <stdlib.h>
//队列的描述符
typedef struct {
    size_t size, head, tail, max;
    uint64_t itimes;                //增加的次数
    uint64_t gtimes;                //获取的次数
    uint64_t ltimes;                //查下下一个元素的次数
    uint64_t ftimes;                //清空
    uint64_t etimes;                //获取时候队列为空
    uint64_t otimes;                //溢出的次数
    bool loop;                      //是否套圈
    void **ptr;                     //数据指针
} queued_t;

//多队列的描述符
typedef struct {
    size_t nqueues;
    queued_t queues[QUEUE_NUMS_MAX];
    void **ptr;
} queueds_t;

static queueds_t *qdst = (queueds_t []){{0}};

#define QUEUE_ITEM(q, offset)    (q->ptr[offset])
#define QUEUE_HEAD(q)    QUEUE_ITEM(q, q->head)
#define QUEUE_TAIL(q)    QUEUE_ITEM(q, q->tail)
#define QUEUE_ID_TO_QDT(ret)                    \
    queued_t *qdt = _queue_id_to_qdt(qid);      \
    if(qdt == NULL)                             \
        return ret;

#define QUEUE_ID_TO_QDT_NULL()                  \
    queued_t *qdt = _queue_id_to_qdt(qid);      \
    if(qdt == NULL)                             \
        return ;
/**
 * qdt 队列的描述符的指针
 * fetch 是否从队列中取出数据
 */
static inline void* _queue_get(queued_t *qdt, bool fetch) {
    if(qdt->size == 0) {
        qdt->etimes++;
        return NULL;
    } else {
        void *tmp = NULL;
        if(fetch == false) {
            tmp = QUEUE_HEAD(qdt);
            qdt->ltimes++;
        } else {
            qdt->size--;
            qdt->gtimes++;
            tmp = QUEUE_HEAD(qdt);
            qdt->head++;
            //如果已经到队列的末尾,就跳回队列头
            if(qdt->head == qdt->max){
                qdt->head = 0;    
                qdt->loop = false;    
            }
        }
        return tmp;
    }
}

/**
 * qdt 队列的描述符的指针
 * item 要存储的指针值
 */
static inline void* _queue_add(queued_t *qdt, void* item) {
    if(qdt == NULL)
        return NULL;
    if(qdt->size == qdt->max) {
        qdt->otimes++;
        return NULL;
    } else {
        void **ret;
        qdt->size++;
        qdt->itimes++;
        QUEUE_TAIL(qdt) = item;
        ret = qdt->ptr + qdt->tail;
        qdt->tail++;
        if(qdt->tail == qdt->max){
            qdt->tail = 0;
            qdt->loop = true;
        }
        return ret;
    }
}
/**
 * 重置队列
 */
static void _queue_reset(queued_t *qdt) {
    qdt->size = 0;
    qdt->head = 0;
    qdt->tail = 0;
    qdt->loop = false;
    qdt->ftimes++;
}
/**
 * 重置队列状态
 */
static void _queue_stat_reset(queued_t *qdt) {
    qdt->itimes = 0;
    qdt->gtimes = 0;
    qdt->ftimes = 0;
    qdt->ltimes = 0;
    qdt->etimes = 0;
    qdt->otimes = 0;
}
/**
 * 初始化队列
 */
static void _queue_init(queued_t *qdt, size_t max, void** ptr) {
    _queue_reset(qdt);
    _queue_stat_reset(qdt);
    qdt->max = max;
    qdt->ptr = ptr;
}
/**
 * 根据队列id获得队列的描述符
 */
static queued_t* _queue_id_to_qdt(size_t id) {
    if(id >= qdst->nqueues)
        return NULL;
    else
        return &qdst->queues[id];
}

void queues_init(size_t max, size_t n) {
    qdst->nqueues = n;
    qdst->ptr = malloc(sizeof(void*) * n * max);
    size_t i;
    for(i = 0;i < n;i++){
        _queue_init(&qdst->queues[i], max, qdst->ptr + i * max);
    }
}
void *queue_add(void *item, size_t qid) {
    QUEUE_ID_TO_QDT(NULL);
    return _queue_add(qdt, item);
}
void *queue_get(bool fetch, size_t qid) {
    QUEUE_ID_TO_QDT(NULL);
    return _queue_get(qdt, fetch);
}
#ifdef ENABLE_UPDATE
void *queue_replace(void **iptr, void *item) {
    return *iptr = item;
}
#endif
/**
 * 释放队列内存
 */
void queues_free(void) {
    free(qdst->ptr);
}
/**
 * 根据qid判断队列是否满
 */
bool queue_full(size_t qid) {
    QUEUE_ID_TO_QDT(true);
    return qdt->size >= qdt->max;
}
/**
 * 增加队列溢出的次数
 */
void queue_plus_otimes(size_t qid) {
    QUEUE_ID_TO_QDT_NULL();
    qdt->otimes++;
}
/**
 * 清空一个队列
 */
void queue_reset(size_t qid) {
    QUEUE_ID_TO_QDT_NULL();
    _queue_reset(qdt);
}
/**
 * 清空所有队列
 */
void queue_reset_all(void) {
    size_t i;
    for(i = 0; i < qdst->nqueues; i++)
        _queue_reset(_queue_id_to_qdt(i));    
}
/**
 * 重置队列状态
 */
void queue_stat_reset(size_t qid) {
    QUEUE_ID_TO_QDT_NULL();
    _queue_stat_reset(qdt);
}
/**
 * 重置所有队列状态
 */
void queue_stat_reset_all(void) {
    size_t i;
    for(i = 0; i < qdst->nqueues; i++)
        _queue_stat_reset(_queue_id_to_qdt(i));    
}
/**
 * 根据id确认是否存在队列
 */
bool queue_exists(size_t qid) {
    return qid < qdst->nqueues;
}

uint32_t queue_get_id(uint32_t qid) {
    return qid % QUEUE_NUMS_MAX;
}
static void _queue_stat(ADD_STAT add_stats, void* c, queued_t *qdt) {
    APPEND_STAT("queue_size", "%llu",
            (unsigned long long)qdt->size);
    APPEND_STAT("queue_max", "%llu",
            (unsigned long long)qdt->max);
    APPEND_STAT("queue_itimes", "%llu",
            (unsigned long long)qdt->itimes);
    APPEND_STAT("queue_gtimes", "%llu",
            (unsigned long long)qdt->gtimes);
    APPEND_STAT("queue_ftimes", "%llu",
            (unsigned long long)qdt->ftimes);
    APPEND_STAT("queue_etimes", "%llu",
            (unsigned long long)qdt->etimes);
    APPEND_STAT("queue_ltimes", "%llu",
            (unsigned long long)qdt->ltimes);
    APPEND_STAT("queue_otimes", "%llu",
            (unsigned long long)qdt->otimes);
}

static bool _queues_stats(ADD_STAT add_stats, void* c, bool detail) {
    uint64_t size, max, itimes, gtimes, ftimes, etimes, ltimes, otimes;
    size = max = itimes = gtimes = ftimes = etimes = ltimes =otimes = 0;
    queued_t *qdt;
    size_t i;
    for(i = 0; i < qdst->nqueues; i++) {
        qdt = _queue_id_to_qdt(i);
        itimes += qdt->itimes;
        gtimes += qdt->gtimes;
        ftimes += qdt->ftimes;
        etimes += qdt->etimes;
        ltimes += qdt->ltimes;
        otimes += qdt->otimes;
        size += qdt->size;
        max += qdt->max;
        if(detail) {
            APPEND_STAT("queue_id", "%llu",
                    (unsigned long long)i);
            _queue_stat(add_stats, c, qdt);
        }
    }
    APPEND_STAT("queue_size", "%llu",
            (unsigned long long)size);
    APPEND_STAT("queue_max", "%llu",
            (unsigned long long)max);
    APPEND_STAT("queue_itimes", "%llu",
            (unsigned long long)itimes);
    APPEND_STAT("queue_gtimes", "%llu",
            (unsigned long long)gtimes);
    APPEND_STAT("queue_ftimes", "%llu",
            (unsigned long long)ftimes);
    APPEND_STAT("queue_etimes", "%llu",
            (unsigned long long)etimes);
    APPEND_STAT("queue_ltimes", "%llu",
            (unsigned long long)ltimes);
    APPEND_STAT("queue_otimes", "%llu",
            (unsigned long long)otimes);
    return true;
}
/**
 * 输出队列的状态信息
 */
bool queue_stats(ADD_STAT add_stats, void* c) {
    return _queues_stats(add_stats, c, false);
}
bool queues_stats(ADD_STAT add_stats, void* c) {
    return _queues_stats(add_stats, c, true);
}
bool queue_stat(ADD_STAT add_stats, void* c, size_t qid) {
    QUEUE_ID_TO_QDT(false);
    _queue_stat(add_stats, c, qdt);
    return true;
}

