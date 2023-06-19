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
    pthread_mutex_init(&mutex_request, NULL);
    pthread_mutex_init(&mutex_handled, NULL);
    pthread_cond_init(&cond_request, NULL);
    pthread_cond_init(&cond_handled, NULL);
    pthread_cond_init(&cond_list_full, NULL);
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
    if (argc < 5) {
	    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        // TODO: what to print here in error handling
	    exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    init_schedalg(argv[4], schedalg);
    if (argc==6 && *schedalg == DYNAMIC) {
        *max_size = atoi(argv[5]);
    }
}


/*--------------------------------------------
 queue functions:
---------------------------------------------*/


void enqueue_request(Queue* queue, request req ,pthread_mutex_t* p_mutex, pthread_cond_t* p_cond){
    pthread_mutex_lock(p_mutex);

    add_to_Queue(queue, req);
    pthread_cond_signal(p_cond);

    pthread_mutex_unlock(p_mutex);
}

request dequeue_request(Queue* queue ,pthread_mutex_t* p_mutex, pthread_cond_t* p_cond, int is_main_thread){
    pthread_mutex_lock(p_mutex);

    while (queue->size==0){
        pthread_cond_wait(p_cond, p_mutex);
    }
    request res = remove_first(queue);

    if ( !is_main_thread ) handled_requests++;

    pthread_mutex_unlock(p_mutex);

    return res;
}


void dec_counter() {
    pthread_mutex_lock(&mutex_request);
    handled_requests--;

    //TODO: make here if size == full or send everytime?
    pthread_cond_signal(&cond_list_full);

    //pthread_cond_signal(&cond_handled);
    pthread_mutex_unlock(&mutex_request);
}

/*-----------------------------------
 statistic functions:
 -----------------------------------*/

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

// needed t2 > t1
int passTime (struct timeval* t1, struct timeval *result) {
    struct timeval t2;
    if (gettimeofday(&t2, NULL) == -1) {
        // TODO: error format
        perror("gettimeofday error:");
        return -1;
    }
    result->tv_sec = t2.tv_sec - t1->tv_sec;
    result->tv_usec = t2.tv_usec - t1->tv_usec;
    if (result->tv_usec < 0) {
        result->tv_sec--;
        result->tv_usec += 1000000;
    }
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
        total_counter++;

        // TODO: add and remove from the other list?
        // like enqueue in tutorial
        //add_to_list(handled_queue ,socket_fd);

        requestHandle(curr_req.fd);
        dec_counter();

        // statistics:
        show_statistic(id, static_counter, dynamic_counter, total_counter, curr_req);

        // TODO: do we need to put mutex on close because after a lot of request we get Rio_readlineb error and one of the options is the open and close mechanism
        printf("\nClose the fd: %d\n\n", curr_req.fd);
        Close(curr_req.fd);
        printf("\nafter Close the fd: %d\n\n", curr_req.fd);
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
        pthread_create(&threads[i], NULL, thread_job, (void *) i);
    }
    return 0;
}


/*------------------------------------
 overloadind handling:
 ------------------------------------*/

int get_requests_num() {
    int res = 0;

    pthread_mutex_lock(&mutex_request);

    res += requests_queue->size;
    res += handled_requests;

    pthread_mutex_unlock(&mutex_request);

    return res;
}



int block_handler(Queue* queue, int* queue_size){
    // lock the mutex
    pthread_mutex_lock(&mutex_request);
    // enter the main thread to wait by cond_wait
    while (queue->size + handled_requests >= *queue_size){
        pthread_cond_wait(&cond_list_full, &mutex_request);
    }
    pthread_mutex_unlock(&mutex_request);

    return HANDLE_CURRENT;
}

int block_flush_handler(Queue* queue){
    // lock the mutex
    pthread_mutex_lock(&mutex_request);
    // enter the main thread to wait by cond_wait
    while (queue->size + handled_requests > 0){
        pthread_cond_wait(&cond_list_full, &mutex_request);
    }
    pthread_mutex_unlock(&mutex_request);

    return SKIP_CURRENT;
}

int drop_tail(request curr){
    printf("\nhi_drop_tail\n\n");
    char buf[MAXBUF];
    Read(curr.fd, buf, MAXBUF);
    Close(curr.fd);
    printf("\nafter_drop_head\n\n");
    return SKIP_CURRENT;
}
int drop_head(){
    printf("\nhi_drop_head\n\n");
    printf("\nhandled_request_num_is: %d\n\n", handled_requests);
    request r1 = dequeue_request(requests_queue, &mutex_request, &cond_request, 1);
    // pass 1 in is_main_thread because dont want to ++handle_requests counter because here just drop_head without handle it
    char buf[MAXBUF];
    Read(r1.fd, buf, MAXBUF);
    Close(r1.fd);
    printf("\nafter_drop_head\n\n");

    /*if  (requests_queue->first!=NULL){
        if (r1.fd != requests_queue->first->data.fd){
            printf("\nsuccess_drop_head\n\n");
        }
    }*/

    return HANDLE_CURRENT;
}

int dynamic(Queue* queue, int* queue_size, int max_size, OVERLOAD_HANDLE* handle_type, request curr_request){
    if (*queue_size<max_size){
        printf("\nhi_dynamic_++\n\n");
        //printf("\nqueue->size = %d < max_size = %d\n\n", queue->size, max_size);
        printf("\nqueue_size_old = %d\n\n", *queue_size);
        char buf[MAXBUF];
        Read(curr_request.fd, buf, MAXBUF);
        Close(curr_request.fd);
        // TODO: is needed Mutex here on ++ ?
        (*queue_size)++;
        printf("\nqueue_size_new = %d\n\n", *queue_size);
        return SKIP_CURRENT;
    }
    else{
        printf("\nhi_dynamic_drop_tail\n\n");
        *handle_type = DROP_TAIL;
        return drop_tail(curr_request);
    }
}


int drop_random(Queue* queue){
    pthread_mutex_lock(&mutex_request);

    int num_to_remove = ((requests_queue->size+1) / 2);
    for (int i=0; i< num_to_remove; i++) {
        request r1 = remove_rand(requests_queue);
        char buf[MAXBUF];
        Read(r1.fd, buf, MAXBUF);
        Close(r1.fd);
    }

    pthread_mutex_unlock(&mutex_request);
    return HANDLE_CURRENT;
}


int overload_handler(OVERLOAD_HANDLE* handle_type, Queue* queue, int* queue_size, int max_size, request curr_request) {
    //TODO: call the matching functions (and add cases)
    switch (*handle_type) {
        case BLOCK: return block_handler(queue, queue_size);
        case BLOCK_FLUSH: return block_flush_handler(queue);
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

// if we have to skip the current request return 1.
// if we have to handle it after the overloading handle, return 0.

// TODO: block with new cond (in same mutex), when thread finish handle request, signal for the cond, inside the function that --counter;
// TODO: drop_tail - close the new fd that created in accept, give the function the fd of request
// TODO: drop_head - do dequeue to the list
// TODO: block_fluse - like block maybe another cond for empty list;
// TODO: Dynamic - increase queue_size by 1 until become equal to max_size, then assignment of dynamic to drop_tail and call the handler again in this specific case

// TODO: check if lonely request in queue - edge case

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
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
        printf("\nnew fd is: %d\n\n", connfd);
        struct timeval arrival;
        if (gettimeofday(&arrival, NULL) == -1) {
            // TODO: error format
            perror("gettimeofday error:");
        }
        request curr_req = {connfd, arrival, arrival};

        int requests_sum = get_requests_num();
        printf("\nqueue_size_general = %d \n\n", requests_queue->size);

        if (requests_sum >= queue_size) {
            printf("\nhi_overload\n\n");
            printf("\nrequests_sum = %d >= queue_size = %d\n\n", requests_sum, queue_size);
            printf("\nmax_size = %d\n\n", max_size);
            // handle overloading and check if skip the current request (1) or do the request (0):
            if (overload_handler(&schedalg, requests_queue ,&queue_size, max_size, curr_req) == SKIP_CURRENT) {
                printf("\nqueue_size = %d\n\n", queue_size);
                continue;
            }
        }

        // like enqueue in tutorial
        printf("\nenter_request\n\n");
        enqueue_request(requests_queue, curr_req, &mutex_request, &cond_request);
    }
    //don't need to free because run forever
}


    
// TODO: check systems calls if fails

 
