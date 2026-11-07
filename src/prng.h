/*
 * @mindmaze_header@
 */
#ifndef PRNG_H
#define PRNG_H

#include <stdint.h>

struct prng_state {
	uint64_t s[4];
};


uint64_t prng_rand(struct prng_state* state);
void prng_state_init_random(struct prng_state* state);



#endif /* PRNG_H */
