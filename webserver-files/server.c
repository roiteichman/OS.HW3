#include <stdbool.h>
#include "segel.h"
#include "request.h"
#include "Queue.h"

Queue* requests_queue = NULL;
//List* handled_queue = NULL;
pthread_mutex_t mutex_request;
pthread_mutex_t mutex_handled;
pthread_cond_t cond_request;
pthread_cond_t cond_handled;
pthread_cond_t cond_list_full;
int handled_requests = 0;

typedef enum  {BLOCK, DROP_TAIL, DROP_HEAD,
               BLOCK_FLUSH, DYNAMIC, DROP_RANDOM} OVERLOAD_HANDLE;

#define SKIP_CURRENT 1
#define HANDLE_CURRENT 0

// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// HW3: Parse the new arguments too

/*---------------------
 init functions:
 ----------------------*/

void init_args(){
    requests_queue = create_Queue();
    //handled_queue = create_list();
    if (pthread_mutex_init(&mutex_request, NULL) != 0) {
        perror ("pthread_mutex_init:");
        exit(1);
    }
    if (pthread_mutex_init(&mutex_handled, NULL) != 0) {
        perror ("pthread_mutex_init:");
        exit(1);
    }
    if (pthread_cond_init(&cond_request, NULL) != 0) {
        perror ("pthread_mutex_init:");
        exit(1);
    }
    if (pthread_cond_init(&cond_handled, NULL) != 0) {
        perror ("pthread_mutex_init:");
        exit(1);
    }
    if (pthread_cond_init(&cond_list_full, NULL) != 0) {
        perror ("pthread_mutex_init:");
        exit(1);
    }
}

void init_schedalg(char* input_string, OVERLOAD_HANDLE* schedalg) {
    if (strcmp(input_string, "block")==0) *schedalg = BLOCK;
    else if (strcmp(input_string, "dt")==0) *schedalg = DROP_TAIL;
    else if (strcmp(input_string, "dh")==0) *schedalg = DROP_HEAD;
    else if (strcmp(input_string, "bf")==0) *schedalg = BLOCK_FLUSH;
    else if (strcmp(input_string, "dynamic")==0) *schedalg = DYNAMIC;
    else if (strcmp(input_string, "random")==0) *schedalg = DROP_RANDOM;
    else {
#ifdef DEBUG_PRINT
        printf("invalid schedalg!\n");
#endif
    }
}


void getargs(int argc, char *argv[], int *port, int* threads, int* queue_size, OVERLOAD_HANDLE* schedalg, int* max_size)
{
    if (argc < 5 || argc > 6) {
	    fprintf(stderr, "Usage: %s <port> <threads_num> <requests_num> <schedalg>\n", argv[0]);
	    exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    init_schedalg(argv[4], schedalg);
    if (argc==6 && *schedalg == DYNAMIC) {
        *max_size = atoi(argv[5]);
    }
    if (*threads >= *queue_size && (*schedalg == DROP_HEAD || *schedalg == DROP_RANDOM)) {
        *schedalg = DROP_TAIL;
    }
    if ((argc == 5 && *schedalg == DYNAMIC) || (argc == 6 && *schedalg != DYNAMIC)) {
        fprintf(stderr, "Usage: %s <port> <threads_num> <requests_num> <schedalg>\n", argv[0]);
    }
}


/*--------------------------------------------
 queue functions:
---------------------------------------------*/


void enqueue_request(Queue* queue, request req ,pthread_mutex_t* p_mutex, pthread_cond_t* p_cond){
    if (pthread_mutex_lock(p_mutex) != 0) {
        perror("pthread_mutex_lock: ");
        exit(1);
    }

    add_to_Queue(queue, req);
    if (pthread_cond_signal(p_cond) != 0){
        perror("pthread_mutex_signal: ");
        exit(1);
    }

    if (pthread_mutex_unlock(p_mutex) != 0){
        perror("pthread_mutex_unlock: ");
        exit(1);
    }
}

request dequeue_request(Queue* queue ,pthread_mutex_t* p_mutex, pthread_cond_t* p_cond, int is_main_thread){
    if (pthread_mutex_lock(p_mutex) != 0) {
        perror("pthread_mutex_lock: ");
        exit(1);
    }

    while (queue->size==0){
        if (pthread_cond_wait(p_cond, p_mutex) != 0) {
            perror("pthread_cond_wait: ");
            exit(1);
        }
    }
    request res = remove_first(queue);

    if ( !is_main_thread ) handled_requests++;

    if (pthread_mutex_unlock(p_mutex) != 0) {
        perror("pthread_mutex_unlock: ");
        exit(1);
    }

    return res;
}


void dec_counter() {
    if (pthread_mutex_lock(&mutex_request) != 0){
        perror("pthread_mutex_lock: ");
        exit(1);
    }
    handled_requests--;

    if (pthread_cond_signal(&cond_list_full) != 0) {
        perror("pthread_cond_signal: ");
        exit(1);
    }

    //pthread_cond_signal(&cond_handled);
    if (pthread_mutex_unlock(&mutex_request) != 0) {
        perror("pthread_mutex_unlock: ");
        exit(1);
    }
}

/*-----------------------------------
 statistic functions:
 -----------------------------------*/
/*
void show_statistic(int id, int static_counter, int dynamic_counter, int total_counter, request req){
    char buf[MAXBUF];
    sprintf(buf, "Stat-Req-arrival:: %lu.%06lu\r\n", req.arrival.tv_sec, req.arrival.tv_usec);
    sprintf(buf, "%sStat-Req-Dispatch:: %lu.%06lu\r\n", buf, req.dispatch.tv_sec, req.dispatch.tv_usec);

    sprintf(buf, "%sStat-Thread-Id:: %d\r\n", buf, id);
    sprintf(buf, "%sStat-Thread-Count:: %d\r\n", buf ,total_counter);
    sprintf(buf, "%sStat-Thread-Static:: %d\r\n", buf, static_counter);
    sprintf(buf, "%sStat-Thread-Dynamic:: %d\r\n", buf, dynamic_counter);
    Rio_writen(req.fd, buf, strlen(buf));
}
*/

// needed t2 > t1
int passTime (struct timeval* t1, struct timeval *result) {
    struct timeval t2;
    if (gettimeofday(&t2, NULL) == -1) {
        perror("gettimeofday error:");
        return -1;
    }
    timersub(&t2, t1, result);
    return 0;
}



/*-----------------------------------
 thread functions:
 -----------------------------------*/

void* thread_job(void* thread_id){
    long id = (long) thread_id;
    int static_counter = 0;
    int dynamic_counter = 0;
    int total_counter = 0;
    while(1){
        // like dequeue in tutorial
        request curr_req = dequeue_request(requests_queue, &mutex_request, &cond_request, 0);
        passTime (&curr_req.arrival, &curr_req.dispatch);

        requestHandle(curr_req.fd, &(curr_req.arrival), &(curr_req.dispatch), id, &static_counter, &dynamic_counter, &total_counter);

        dec_counter();

        Close(curr_req.fd);

    }
    return NULL;
}


int create_threads (int num_threads){
    pthread_t* threads = (pthread_t*) malloc(sizeof(pthread_t)*num_threads);
    if (threads==NULL){
        printf("allocation error\n");
        return -1;
    }
    for (long i = 0; i < num_threads; ++i) {
        if (pthread_create(&threads[i], NULL, thread_job, (void *) i) != 0) {
            perror ("pthread_create:");
            exit(1);
        }
    }
    return 0;
}


/*------------------------------------
 overloadind handling:
 ------------------------------------*/

int get_requests_num() {
    int res = 0;

    if (pthread_mutex_lock(&mutex_request) != 0) {
        perror ("pthread_mutex_lock:");
        exit(1);
    }

    res += requests_queue->size;
    res += handled_requests;

    if (pthread_mutex_unlock(&mutex_request) != 0) {
        perror ("pthread_mutex_unlock:");
        exit(1);
    }

    return res;
}



int block_handler(Queue* queue, int* queue_size){
    // lock the mutex
    if (pthread_mutex_lock(&mutex_request) != 0) {
        perror ("pthread_mutex_lock:");
        exit(1);
    }
    // enter the main thread to wait by cond_wait
    while (queue->size + handled_requests >= *queue_size){
        if (pthread_cond_wait(&cond_list_full, &mutex_request) != 0) {
            perror ("pthread_cond_wait:");
            exit(1);
        }
    }
    if (pthread_mutex_unlock(&mutex_request) != 0) {
        perror ("pthread_mutex_unlock:");
        exit(1);
    }

    return HANDLE_CURRENT;
}

int block_flush_handler(Queue* queue, request curr){
    // lock the mutex
    if (pthread_mutex_lock(&mutex_request) != 0) {
        perror ("pthread_mutex_lock:");
        exit(1);
    }
    // enter the main thread to wait by cond_wait
    while (queue->size + handled_requests > 0){
        if (pthread_cond_wait(&cond_list_full, &mutex_request) != 0){
            perror ("pthread_cond_wait:");
            exit(1);
        }
    }
    if (pthread_mutex_unlock(&mutex_request) != 0) {
        perror ("pthread_mutex_unlock:");
        exit(1);
    }
    char buf[MAXBUF];
    Read(curr.fd, buf, MAXBUF);
    Close(curr.fd);
    return SKIP_CURRENT;
}

int drop_tail(request curr){
#ifdef DEBUG_PRINT
    printf("\nhi_drop_tail\n\n");
#endif
    char buf[MAXBUF];
    Read(curr.fd, buf, MAXBUF);
    Close(curr.fd);
#ifdef DEBUG_PRINT
    printf("\nafter_drop_head\n\n");
#endif
    return SKIP_CURRENT;
}
int drop_head(){
#ifdef DEBUG_PRINT
    printf("\nhi_drop_head\n\n");
    printf("\nhandled_request_num_is: %d\n\n", handled_requests);
#endif
    request r1 = dequeue_request(requests_queue, &mutex_request, &cond_request, 1);
    // pass 1 in is_main_thread because dont want to ++handle_requests counter because here just drop_head without handle it
    char buf[MAXBUF];
    Read(r1.fd, buf, MAXBUF);
    Close(r1.fd);
#ifdef DEBUG_PRINT
    printf("\nafter_drop_head\n\n");
#endif
    return HANDLE_CURRENT;
}

int dynamic(Queue* queue, int* queue_size, int max_size, OVERLOAD_HANDLE* handle_type, request curr_request){
    if (*queue_size<max_size){
#ifdef DEBUG_PRINT
        printf("\nhi_dynamic_++\n\n");
        //printf("\nqueue->size = %d < max_size = %d\n\n", queue->size, max_size);
        printf("\nqueue_size_old = %d\n\n", *queue_size);
#endif
        char buf[MAXBUF];
        Read(curr_request.fd, buf, MAXBUF);
        Close(curr_request.fd);
        (*queue_size)++;
#ifdef DEBUG_PRINT
        printf("\nqueue_size_new = %d\n\n", *queue_size);
#endif
        return SKIP_CURRENT;
    }
    else{
#ifdef DEBUG_PRINT
        printf("\nhi_dynamic_drop_tail\n\n");
#endif
        *handle_type = DROP_TAIL;
        return drop_tail(curr_request);
    }
}


int drop_random(Queue* queue){
    if (pthread_mutex_lock(&mutex_request) != 0){
        perror ("pthread_mutex_lock:");
        exit(1);
    }

    int num_to_remove = ((requests_queue->size+1) / 2);
    for (int i=0; i< num_to_remove; i++) {
        request r1 = remove_rand(requests_queue);
        char buf[MAXBUF];
        Read(r1.fd, buf, MAXBUF);
        Close(r1.fd);
    }

    if (pthread_mutex_unlock(&mutex_request) != 0){
        perror ("pthread_mutex_unlock:");
        exit(1);
    }
    return HANDLE_CURRENT;
}


int overload_handler(OVERLOAD_HANDLE* handle_type, Queue* queue, int* queue_size, int max_size, request curr_request) {
    switch (*handle_type) {
        case BLOCK: return block_handler(queue, queue_size);
        case BLOCK_FLUSH: return block_flush_handler(queue, curr_request);
        case DROP_TAIL: return drop_tail(curr_request);
        case DROP_HEAD: return drop_head();
        case DYNAMIC: return dynamic(queue, queue_size, max_size, handle_type, curr_request);
        case DROP_RANDOM: return drop_random(queue);

        default: {
#ifdef DEBUG_PRINT
            printf("invalid handle_type!\n");
#endif
            return -1;
        }
    }
}


/*------------------------------------------------------------------------------
 main program:
 ------------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
    init_args();

    int listenfd, connfd, port, clientlen, num_threads, queue_size, max_size;
    OVERLOAD_HANDLE schedalg;

    struct sockaddr_in clientaddr;

    getargs(argc, argv, &port, &num_threads, &queue_size, &schedalg, &max_size);

    if (create_threads(num_threads) == -1){
        return -1;
    }

    listenfd = Open_listenfd(port);

    while (1) {
        clientlen = sizeof(clientaddr);
        struct timeval arrival;
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
#ifdef DEBUG_PRINT
        printf("\nnew fd is: %d\n\n", connfd);
#endif
        if (gettimeofday(&arrival, NULL) == -1) {
            perror("gettimeofday error:");
        }
        request curr_req = {connfd, arrival, arrival};

        int requests_sum = get_requests_num();
#ifdef DEBUG_PRINT
        printf("\nqueue_size_general = %d \n\n", requests_queue->size);
#endif
        if (requests_sum >= queue_size) {
#ifdef DEBUG_PRINT
            printf("\nhi_overload\n\n");
            printf("\nrequests_sum = %d >= queue_size = %d\n\n", requests_sum, queue_size);
            printf("\nmax_size = %d\n\n", max_size);
#endif
            // handle overloading and check if skip the current request (1) or do the request (0):
            if (overload_handler(&schedalg, requests_queue ,&queue_size, max_size, curr_req) == SKIP_CURRENT) {
#ifdef DEBUG_PRINT
                printf("\nqueue_size = %d\n\n", queue_size);
#endif
                continue;
            }
        }
        // like enqueue in tutorial
#ifdef DEBUG_PRINT
        printf("\nenter_request\n\n");
#endif
        enqueue_request(requests_queue, curr_req, &mutex_request, &cond_request);
    }
    //don't need to free because run forever
}


 
