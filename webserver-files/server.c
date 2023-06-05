#include "segel.h"
#include "request.h"
#include "list.h"

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

int requests_queue[3];

void* thread_job(){
    requestHandle(requests_queue[0]);
    return NULL;
}


int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen, num_threads, queue_size, max_size;
    char* schedalg;
    struct sockaddr_in clientaddr;

    getargs(argc, argv, &port, &num_threads, &queue_size, &schedalg, &max_size);

    // 
    // HW3: Create some threads...
    //

    listenfd = Open_listenfd(port);
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

    requests_queue[0]=connfd;
    pthread_t t1;
    pthread_create(&t1, NULL, thread_job, NULL);
    pthread_join(t1, NULL);

	// 
	// HW3: In general, don't handle the request in the main thread.
	// Save the relevant info in a buffer and have one of the worker threads 
	// do the work. 
	// 

    //requestHandle(connfd);

	Close(connfd);
    }

}


    


 
