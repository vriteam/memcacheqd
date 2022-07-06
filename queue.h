#ifndef QUEUE_H
#define QUEUE_H
#define QUEUE_NUMS_MAX               (1<<5)
#define QUEUE_ID_UNKNOWN             QUEUE_NUMS_MAX

#define QUEUE_RESERVE_CONTEXT        "queue"
#define QUEUE_RESERVE_KEY_ADD        "add"
#define QUEUE_RESERVE_KEY_RADD       "radd"
#define QUEUE_RESERVE_KEY_GET        "get"
#define QUEUE_RESERVE_KEY_LOOK       "look"
#define QUEUE_RESERVE_KEY_STAT       "stat"
#define QUEUE_RESERVE_KEY_STATALL    "statall"
void queues_init(size_t, size_t);
void *queue_add(void *item, size_t);
void *queue_get(bool, size_t);
#ifdef ENABLE_UPDATE
void *queue_replace(void**, void*);
#endif
bool queue_full(size_t);
bool queue_exists(size_t);
void queue_reset(size_t);
void queue_reset_all(void);
void queue_stat_reset(size_t);
void queue_stat_reset_all(void);
bool queue_stats(ADD_STAT, void*);
void queue_plus_otimes(size_t);
bool queue_stat(ADD_STAT, void*, size_t);
uint32_t queue_get_id(uint32_t);
void queues_free(void);
bool queues_stats(ADD_STAT, void*);
#endif
