#ifndef __REQUEST_H__
#include "sys/time.h"

void requestHandle(int fd, struct timeval* arrival, struct timeval* dispatch,
        long id, int* static_counter, int* dynamic_counter, int* total_counter);

#endif
