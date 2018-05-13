#ifndef ARC4RANDOM_H
#define ARC4RANDOM_H

#include <stddef.h>
#include <unistd.h>

void arc4random_buf(void *buf_, size_t n);
unsigned int arc4random_uniform(unsigned int upper_bound);

#endif // ARC4RANDOM_H
