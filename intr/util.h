#ifndef UTIL_H
#define UTIL_H

#include "dtypes.h"

#define min(a, b) ((a) < (b))? (a) : (b)  
#define max(a, b) ((a) > (b))? (a) : (b)  


uint round_up(uint n, int multiple);

int strip_bytes(uchar *buf, int n, uchar *remove_set, int sn);



#endif