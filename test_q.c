#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <pthread.h>
#include <libmemcached/memcached.h>

#define S_IP		"127.0.0.1"
#define S_PORT		11211
#define TIMES		1024*1024
#define THREAD_NUMS		200
#define THREAD_G_NUMS	200

#define ADD_KEY_PREFIX	"queue_add_"
#define GET_KEY_PREFIX	"queue_get_"
#define MAX_KEY_LENGTH	250
enum {
	STANDARD = 0,
	RAND,
};
typedef struct _thread{
	pthread_t t;
	uint64_t tps;
} thread;
typedef struct {
	char *ip;
	unsigned int port;
	int times;
	int snums;
	int gnums;
	int qnums;
	int mode;
} settings;
static volatile sig_atomic_t recieved_signal;
static settings *setting = (settings[]){{0}};
static char buffer[3072];
static thread tarray[THREAD_NUMS];
static thread garray[THREAD_G_NUMS];
static struct timeval b, e;

	static void sigint(int signal){
		if(signal == SIGINT)
			recieved_signal |= 1;
		else if(signal == SIGALRM)
			recieved_signal |= 2;
	}
/**
 * 初始化一个到指定ip和port的实例
 */
static memcached_st *memcached_connect(const char *ip, unsigned int port){
	memcached_st *server;
	server = memcached_create(NULL);
	memcached_server_add(server, ip, port);
	return server;
}
/*
   const char *
   memcached_strerror (memcached_st *ptr,
   memcached_return_t rc);
 */
static void* worker(void* arg){
	memcached_return ret;
	memcached_st *server;
	size_t id = (size_t)arg;
	server = memcached_connect(setting->ip, setting->port);
	char key[MAX_KEY_LENGTH];
	int key_len = 0, rnd;
	uint64_t i = 0;
	uint32_t qid;
	//如果是固定模式
	if(setting->mode == STANDARD){
		qid = (uint32_t)arg;
	}
	for(;setting->mode == RAND || i < setting->times; ++i){
		if(recieved_signal !=0 )
			return NULL;
		rnd = rand();
		if(setting->qnums > 0){
			qid = (rnd % setting->qnums);
		}

		key_len = snprintf(key, MAX_KEY_LENGTH, "%s_%ld_%lld", ADD_KEY_PREFIX, qid, i);

		ret = memcached_add(server, key, key_len, buffer, rnd % sizeof(buffer), 0, 0);
		while( ret != MEMCACHED_SUCCESS ){
			if(recieved_signal !=0 )
				return NULL;
			//memcached_strerror(server, ret);
      //printf("retry: %ld\n", flag);
			ret = memcached_add(server, key, key_len, buffer, rnd % sizeof(buffer), 0, 0);
		}
		tarray[id].tps++;
	}
	return NULL;
}

static void* getter(void* arg){
	memcached_return ret;
	memcached_st *server;
	size_t id = (size_t)arg;
	server = memcached_connect(setting->ip, setting->port);
	char key[MAX_KEY_LENGTH] ,*value;
	int key_len;
	uint64_t times = 0;
	key_len = sprintf(key, "%s%ld", GET_KEY_PREFIX, (long)arg);
	size_t value_length;
	uint32_t flag;
	for(;;){
		if(recieved_signal !=0)
			return NULL;
		flag = 0;
		if(setting->mode == RAND && setting->qnums > 0){
			int rnd = rand();
			key_len = sprintf(key, "%s%ld", GET_KEY_PREFIX, rnd % setting->qnums);
		}
		value = memcached_get(server, key, key_len, &value_length, &flag, &ret);
		if( ret == MEMCACHED_SUCCESS){
			free(value);
			times++;
			if(times >= setting->times && setting->mode == STANDARD){
				return NULL;
			}
		}else{
			memcached_strerror(server, ret);
		}
		garray[id].tps++;
	}
	return NULL;
}
static void buffer_init(char* buffer, size_t size){
	memset(buffer, 'a', size);
}
static void setting_init(settings* s){
	s->ip		= S_IP;
	s->port		= S_PORT;
	s->times	= TIMES;
	s->snums	= THREAD_NUMS;
	s->gnums	= THREAD_G_NUMS;
	s->qnums	= 0;
	s->mode		= STANDARD;
}
int main(int argc, char *argv[]){
	srand(time(NULL));
	setting_init(setting);
	int c;
	while (-1 != (c = getopt(argc, argv,
					"s:"//set 
					"g:"//get
					"t:"//time
					"h:"//host 
					"q:"//rand mode
					"p:"//port
					"m::"//mode
			  ) ) ){
		switch (c){
			case 'q':
				setting->qnums = atoi(optarg);
				break;
			case 's':
				setting->snums = atoi(optarg);
				break;
			case 'g':
				setting->gnums = atoi(optarg);
				break;
			case 't':
				setting->times = atoi(optarg);
				break;
			case 'p':
				setting->port = atoi(optarg);
				break;
			case 'h':
				setting->ip = strdup(optarg);
				break;
			case 'm':
				setting->mode = RAND;
				break;
		}
	}


	//捕获两个信号
	signal(SIGINT, sigint);
	signal(SIGALRM, sigint);
	buffer_init(buffer, 3072);
	if(setting->mode == RAND){
		alarm((unsigned int)setting->times);
	}
	int i;
	static uint64_t sts, gts, tps;
	gettimeofday(&b, NULL);
	for(i = 0;i < setting->snums; ++i){
		pthread_create(&tarray[i].t, NULL, worker, (void*)i);
	}
	for(i = 0;i < setting->gnums; ++i){
		pthread_create(&garray[i].t, NULL, getter, (void*)i);
	}

	for(i = 0;setting->snums && i < setting->snums; ++i){
		pthread_join(tarray[i].t, NULL);
	}
	for(i = 0;setting->gnums && i < setting->gnums; ++i){
		pthread_join(garray[i].t, NULL);
	}
	if(setting->mode == RAND) {
		while(recieved_signal == 0) {
			pause();
		}
	}
	gettimeofday(&e, NULL);
	long sec = ((e.tv_sec * 1000 + e.tv_usec /1000) - (b.tv_sec * 1000 + b.tv_usec /1000) )/1000;
	for(i = 0;i < setting->snums; ++i){
		sts += tarray[i].tps;
	}
	for(i = 0;i < setting->gnums; ++i){
		gts += garray[i].tps;
	}
	tps = gts + sts;
	printf("time:%ld\
			add:%"PRId64"\
			get:%"PRId64"\
			tps:%"PRId64"\n",
			sec, sts, gts, sec > 0 ? tps/sec : tps);
	return 0;
}

//gcc test_q.c -L/usr/local/libmemcached/lib -lmemcached -I/usr/local/libmemcached/include/ -Wl,-rpath -Wl,/usr/local/libmemcached/lib
