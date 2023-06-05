#include "segel.h"
#include "request.h"
#include "list.h"

#define SCHEDALG_MAX_SIZE 8

List* requests_queue = NULL;
List* handled_queue = NULL;
pthread_mutex_t mutex_request;
pthread_mutex_t mutex_handled;
pthread_cond_t cond_request;
pthread_cond_t cond_handled;


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
void getargs(int argc, char *argv[], int *port, int* threads, int* queue_size, char* schedalg, int* max_size)
{
    if (argc < 5) {
	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    // TODO: what to print here in error handling
	exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    strcpy(schedalg,argv[4]);
    if (argc==6 && strcmp(argv[4], "dynamic")==0 ){
        *queue_size = atoi(argv[5]);
    }
}

void enqueue_request(List* list, int request ,pthread_mutex_t* p_mutex, pthread_cond_t* p_cond){
    pthread_mutex_lock(p_mutex);
    add_to_list(list, request);
    pthread_cond_signal(p_cond);
    pthread_mutex_unlock(p_mutex);
}

int dequeue_request(List* list ,pthread_mutex_t* p_mutex, pthread_cond_t* p_cond){
    pthread_mutex_lock(p_mutex);
    while (list->size==0){
        pthread_cond_wait(p_cond, p_mutex);
    }
    int socket_fd = remove_first(list);
    pthread_mutex_unlock(p_mutex);

    return socket_fd;
}

void* thread_job(){
    while(1){
        // like dequeue in tutorial
        int socket_fd = dequeue_request(requests_queue, &mutex_request, &cond_request);

        // TODO: add and remove from the other list
        // like enqueue in tutorial
        //add_to_list(handled_queue ,socket_fd);

        requestHandle(socket_fd);
        Close(socket_fd);
    }
    return NULL;
}

void init_args(){
    requests_queue = create_list();
    handled_queue = create_list();
    pthread_mutex_init(&mutex_request, NULL);
    pthread_mutex_init(&mutex_handled, NULL);
    pthread_cond_init(&cond_request, NULL);
    pthread_cond_init(&cond_handled, NULL);
}

int create_threads(int num_threads){
    pthread_t* threads = (pthread_t*) malloc(sizeof(pthread_t)*num_threads);
    if (threads==NULL){
        printf("allocation error\n");
        return -1;
    }
    for (int i = 0; i < num_threads; ++i) {
        pthread_create(&threads[i], NULL, thread_job, NULL);
    }
    return 0;
}




int main(int argc, char *argv[])
{
    init_args();

    int listenfd, connfd, port, clientlen, num_threads, queue_size, max_size;
    char schedalg[SCHEDALG_MAX_SIZE];
    struct sockaddr_in clientaddr;

    getargs(argc, argv, &port, &num_threads, &queue_size, schedalg, &max_size);

    if (create_threads(num_threads) == -1){
        return -1;
    }

    listenfd = Open_listenfd(port);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        // like enqueue in tutorial
        enqueue_request(requests_queue, connfd, &mutex_request, &cond_request);

        /*pthread_t t1;
        pthread_create(&t1, NULL, thread_job, NULL);
        pthread_join(t1, NULL);*/

        //
        // HW3: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads
        // do the work.
        //

        //requestHandle(connfd);

        //Close(connfd);
    }

    //don't need to free because run forever
}


    


 
