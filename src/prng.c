/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "prng.h"

static uint64_t rotl(const uint64_t x, int k)
{
    return (x << k) | (x >> (64 - k));
}


/**
 * prng_rand() - draw a pseudo-random number
 * @state:      state on the pseudo-random sequence
 *
 * Returns: a pseudo random number on 64bits
 */
LOCAL_SYMBOL
uint64_t prng_rand(struct prng_state* state)
{
	uint64_t res, tmp;
	uint64_t* s = state->s;

	res = rotl(s[0] * 5, 7) * 9;

	tmp = s[1] << 17;
	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];
	s[2] ^= tmp;

	s[3] = rotl(s[3], 45);

	return res;
}
