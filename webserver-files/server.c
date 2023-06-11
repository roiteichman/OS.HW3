#include "segel.h"
#include "request.h"
#include "list.h"


List* requests_queue = NULL;
//List* handled_queue = NULL;
pthread_mutex_t mutex_request;
pthread_mutex_t mutex_handled;
pthread_cond_t cond_request;
pthread_cond_t cond_handled;
pthread_cond_t cond_list_full;
int handled_requests = 0;

typedef enum  {BLOCK, DROP_TAIL, DROP_HEAD,
               BLOCK_FLUSH, DYNAMIC, DROP_RANDOM} OVERLOAD_HANDLE;

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
    requests_queue = create_list();
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
        *queue_size = atoi(argv[5]);
    }
}

/*--------------------------------------------
 queue functions:
---------------------------------------------*/


void enqueue_request(List* list, int request ,pthread_mutex_t* p_mutex, pthread_cond_t* p_cond){
    pthread_mutex_lock(p_mutex);

    add_to_list(list, request);
    pthread_cond_signal(p_cond);

    pthread_mutex_unlock(p_mutex);
}

int dequeue_request(List* list ,pthread_mutex_t* p_mutex, pthread_cond_t* p_cond, int is_main_thread){
    pthread_mutex_lock(p_mutex);

    while (list->size==0){
        pthread_cond_wait(p_cond, p_mutex);
    }
    int socket_fd = remove_first(list);

    if ( !is_main_thread ) handled_requests++;

    pthread_mutex_unlock(p_mutex);

    return socket_fd;
}

void dec_counter(List* list) {
    pthread_mutex_lock(&mutex_request);
    handled_requests--;

    //TODO: make here if size == full or send everytime?
    pthread_cond_signal(&cond_list_full);

    //pthread_cond_signal(&cond_handled);
    pthread_mutex_unlock(&mutex_request);
}

/*-----------------------------------
 thread function:
 -----------------------------------*/

void show_statistic(int id, int static_counter, int dynamic_counter, int total_counter, int fd){
    char buf[MAXBUF];
    sprintf(buf, "Stat-Thread-Id:: %d\r\n", id);
    sprintf(buf, "%sStat-Thread-Count:: %d\r\n", buf ,total_counter);
    sprintf(buf, "%sStat-Thread-Static:: %d\r\n", buf, static_counter);
    sprintf(buf, "%sStat-Thread-Dynamic:: %d\r\n", buf, dynamic_counter);
    Rio_writen(fd, buf, strlen(buf));
}


void* thread_job(void* thread_id){
    long id = (long) thread_id; //TODO: check if need to change to pid or thread_id because thread_id here is pointer to temporary variable
    int static_counter = 0;
    int dynamic_counter = 0;
    int total_counter = 0;
    while(1){
        // like dequeue in tutorial
        int socket_fd = dequeue_request(requests_queue, &mutex_request, &cond_request, 0);
        total_counter++;

        // TODO: add and remove from the other list?
        // like enqueue in tutorial
        //add_to_list(handled_queue ,socket_fd);

        requestHandle(socket_fd);
        dec_counter(requests_queue);

        // statistics:
        show_statistic(id, static_counter, dynamic_counter, total_counter, socket_fd);

        // TODO: do we need to put mutex on close because after a lot of request we get Rio_readlineb error and one of the options is the open and close mechanism
        Close(socket_fd);
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
/*
void enqueue_request(List* list, int request ,pthread_mutex_t* p_mutex, pthread_cond_t* p_cond){
    pthread_mutex_lock(p_mutex);

    add_to_list(list, request);
    pthread_cond_signal(p_cond);

    pthread_mutex_unlock(p_mutex);
}

int dequeue_request(List* list ,pthread_mutex_t* p_mutex, pthread_cond_t* p_cond, int is_main_thread){
    pthread_mutex_lock(p_mutex);

    while (list->size==0){
        pthread_cond_wait(p_cond, p_mutex);
    }
    int socket_fd = remove_first(list);

    if ( !is_main_thread ) handled_requests++;

    pthread_mutex_unlock(p_mutex);

    return socket_fd;
}
*/


int fictive_handler(){return 0;}

int block_handler(List* list, int queue_size){
    // lock the mutex
    pthread_mutex_lock(&mutex_request);
    // enter the main thread to wait by cond_wait
    while (list->size+handled_requests>=queue_size){
        pthread_cond_wait(&cond_list_full, &mutex_request);
    }
    pthread_mutex_unlock(&mutex_request);

    return 0;
}

int block_flush_handler(List* list){
    // lock the mutex
    pthread_mutex_lock(&mutex_request);
    // enter the main thread to wait by cond_wait
    while (list->size+handled_requests>0){
        pthread_cond_wait(&cond_list_full, &mutex_request);
    }
    pthread_mutex_unlock(&mutex_request);

    return 0;
}

int overload_handler(OVERLOAD_HANDLE handle_type, List* list, int queue_size, int max_size) {
    //TODO: call the matching functions (and add cases)
    switch (handle_type) {
        case BLOCK: return block_handler(list, queue_size);
        case BLOCK_FLUSH: return block_flush_handler(list);
        case DROP_TAIL: return fictive_handler();

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

        int requests_sum = get_requests_num();

        if (requests_sum >= queue_size) {

            // handle overloading and check if skip (1) or do the request (0):
            if (overload_handler(schedalg, requests_queue ,queue_size, max_size) == 1) {
                continue;
            }
        }

        // like enqueue in tutorial

        enqueue_request(requests_queue, connfd, &mutex_request, &cond_request);
    }
    //don't need to free because run forever
}


    
// TODO: check systems calls if fails

 
