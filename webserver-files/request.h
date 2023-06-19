#ifndef __REQUEST_H__
#include "Queue.h"

void requestHandle(int fd, request* req, int* id, int* static_counter, int* dynamic_counter, int* total_counter);

#endif
