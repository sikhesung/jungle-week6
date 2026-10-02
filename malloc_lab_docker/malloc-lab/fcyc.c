/*
 * fcyc.c - Estimate the time (in CPU cycles) used by a function f 
 * 
 * Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 *
 * Uses the cycle timer routines in clock.c to estimate the
 * the time in CPU cycles for a function f.
 */
#include <stdlib.h>
#include <sys/times.h>
#include <stdio.h>

#include "fcyc.h"
#include "clock.h"

/* Default values */
#define K 3                  /* Value of K in K-best scheme */
#define MAXSAMPLES 20        /* Give up after MAXSAMPLES */
#define EPSILON 0.01         /* K samples should be EPSILON of each other*/
#define COMPENSATE 0         /* 1-> try to compensate for clock ticks */
#define CLEAR_CACHE 0        /* Clear cache before running test function */
#define CACHE_BYTES (1<<19)  /* Max cache size in bytes */
#define CACHE_BLOCK 32       /* Cache block size in bytes */

static int kbest = K;
static int maxsamples = MAXSAMPLES;
static double epsilon = EPSILON;
static int compensate = COMPENSATE;
static int clear_cache = CLEAR_CACHE;
static int cache_bytes = CACHE_BYTES;
static int cache_block = CACHE_BLOCK;

static int *cache_buf = NULL;

static double *values = NULL;
static int samplecount = 0;

/* for debugging only */
#define KEEP_VALS 0
#define KEEP_SAMPLES 0

#if KEEP_SAMPLES
static double *samples = NULL;
#endif

/* 
 * init_sampler - Start new sampling process 
 */
static void init_sampler()
{
    if (values)
	free(values);
    values = calloc(kbest, sizeof(double)); //만약 values라면 벨류를 메모리에 반환하고 초기화한다 kbest의 개수만큼 연속적으로 할당하고 모든공간을 초기화한다.
#if KEEP_SAMPLES
    if (samples)
	free(samples);
    /* Allocate extra for wraparound analysis */
    samples = calloc(maxsamples+kbest, sizeof(double)); //만약 samples이라면 벨류를 메모리에 반환하고 초기화한다 maxsamples+kbest의 개수만큼 연속적으로 할당하고 모든공간을 초기화한다.
#endif
    samplecount = 0;
}

/* 
 * add_sample - Add new sample  
 */
static void add_sample(double val)
{
    int pos = 0;
    if (samplecount < kbest) {
	pos = samplecount;
	values[pos] = val; //pos에 0을 넣는다 그리고 만약 samplecount가 kbest보다 작을시 pos에 samplecount를 넣는다 그리고 벨류안에 val에들어 있는 값을 values[pos]에 넣는다
    } else if (val < values[kbest-1]) {
	pos = kbest-1;
	values[pos] = val; //만약 val 이 values[kbest-1]보다 작을시 pos에 kbest-1한 값을 넣는다 그리고 벨류안에 val안에 들어 있는값을 values[pos]에 넣는다
    }
#if KEEP_SAMPLES
    samples[samplecount] = val;
#endif
    samplecount++;
    /* Insertion sort */
    while (pos > 0 && values[pos-1] > values[pos]) {//pos와 values[pos-1]의 값이 values[pos]보다 클때까지 반복한다 
	double temp = values[pos-1];//더블속성에 temp안에 values[pos-1]의 값을 넣는다
	values[pos-1] = values[pos];//values[pos-1] 안에 values[pos]의 값을 넣는다
	values[pos] = temp;//values[pos] 안에 temp의 값을 넣는다
	pos--; //pos 의 값을 1만큼 감소시킨다
    }
}

/* 
 * has_converged- Have kbest minimum measurements converged within epsilon? 
 */
static int has_converged()
{
    return
	(samplecount >= kbest) && //samplecount 이 kbest보다 크거나 같을때 그리고 (1 + epsilon)*values[0]의 값이 values[kbest-1]보다 크거나 같을때 has_converged에 리턴한다
	((1 + epsilon)*values[0] >= values[kbest-1]);
}

/* 
 * clear - Code to clear cache 
 */
static volatile int sink = 0; // volatile(컴파일러의 최적화를 방지하고 변수의 메모리 가시성을 보장하기 위해 사용한다) int타입에 sink를 0으로 지정해서 만든다

static void clear()
{
    int x = sink; //x안에 sink값을 넣는다
    int *cptr, *cend; //int 속성 포인터를 만든다
    int incr = cache_block/sizeof(int); //int 속성 incr 안에 cache_block/sizeof(int)의 값을 넣는다
    if (!cache_buf) { //만약 !cache_buf(캐시 재확인 명령어)일시 cache_buf안에 malloc(cache_bytes)만큼에 메모리를 할당한다
	cache_buf = malloc(cache_bytes);
	if (!cache_buf) { //만약 !cache_buf(캐시 재확인 명령어)일시
	    fprintf(stderr, "Fatal error.  Malloc returned null when trying to clear cache\n");// 프린트한다 Fatal error.  Malloc returned null when trying to clear cache 문장을 출력한다
	    exit(1); //강제 종료 시킨다
	}
    }
    cptr = (int *) cache_buf; //cptr안에 인트속성 포인터 cache_buf의 주소값을 넣는다
    cend = cptr + cache_bytes/sizeof(int); //cend 안에 cptr + cache_bytes/sizeof(int)의 값을 넣는다
    while (cptr < cend) { //만약 cptr보다 cend가 클시
	x += *cptr; // x 에 포인터cptr의 값을 더한다
	cptr += incr; //cptr 과 incr를 더한다
    }
    sink = x; //sink에 x의 값을 넣는다
}

/*
 * fcyc - Use K-best scheme to estimate the running time of function f
 */
double fcyc(test_funct f, void *argp)
{
    double result;
    init_sampler();
    if (compensate) {
	do {
	    double cyc;
	    if (clear_cache)
		clear();
	    start_comp_counter();
	    f(argp);
	    cyc = get_comp_counter();
	    add_sample(cyc);
	} while (!has_converged() && samplecount < maxsamples);
    } else {
	do {
	    double cyc;
	    if (clear_cache)
		clear();
	    start_counter();
	    f(argp);
	    cyc = get_counter();
	    add_sample(cyc);
	} while (!has_converged() && samplecount < maxsamples);
    }
#ifdef DEBUG
    {
	int i;
	printf(" %d smallest values: [", kbest);
	for (i = 0; i < kbest; i++)
	    printf("%.0f%s", values[i], i==kbest-1 ? "]\n" : ", ");
    }
#endif
    result = values[0];
#if !KEEP_VALS
    free(values); 
    values = NULL;
#endif
    return result;  
}


/*************************************************************
 * Set the various parameters used by the measurement routines 
 ************************************************************/

/* 
 * set_fcyc_clear_cache - When set, will run code to clear cache 
 *     before each measurement. 
 *     Default = 0
 */
void set_fcyc_clear_cache(int clear)
{
    clear_cache = clear;
}

/* 
 * set_fcyc_cache_size - Set size of cache to use when clearing cache 
 *     Default = 1<<19 (512KB)
 */
void set_fcyc_cache_size(int bytes)
{
    if (bytes != cache_bytes) {
	cache_bytes = bytes;
	if (cache_buf) {
	    free(cache_buf);
	    cache_buf = NULL;
	}
    }
}

/* 
 * set_fcyc_cache_block - Set size of cache block 
 *     Default = 32
 */
void set_fcyc_cache_block(int bytes) {
    cache_block = bytes;
}


/* 
 * set_fcyc_compensate- When set, will attempt to compensate for 
 *     timer interrupt overhead 
 *     Default = 0
 */
void set_fcyc_compensate(int compensate_arg)
{
    compensate = compensate_arg;
}

/* 
 * set_fcyc_k - Value of K in K-best measurement scheme
 *     Default = 3
 */
void set_fcyc_k(int k)
{
    kbest = k;
}

/* 
 * set_fcyc_maxsamples - Maximum number of samples attempting to find 
 *     K-best within some tolerance.
 *     When exceeded, just return best sample found.
 *     Default = 20
 */
void set_fcyc_maxsamples(int maxsamples_arg)
{
    maxsamples = maxsamples_arg;
}

/* 
 * set_fcyc_epsilon - Tolerance required for K-best
 *     Default = 0.01
 */
void set_fcyc_epsilon(double epsilon_arg)
{
    epsilon = epsilon_arg;
}





