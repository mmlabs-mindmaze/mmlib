/*
 * @mindmaze_header@
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "prng.h"

#include <windows.h>
#include <bcrypt.h>

/**
 * prng_state_init_random() - init PRNG state from good random source
 * @state:      state on the pseudo-random sequence to initialize
 */
LOCAL_SYMBOL
void prng_state_init_random(struct prng_state* state)
{
	FILETIME curr;
	NTSTATUS res;
	int i;

	res = BCryptGenRandom(NULL, (PUCHAR)&state, sizeof(*state),
	                      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	if (!NT_SUCCESS(res)) {
		// Fallback on time in case of failure
		for (i = 0; i < 4; i++) {
			GetSystemTimeAsFileTime(&curr);
			memcpy(state.s + i, curr, sizeof(*state.s));
		}
	}
}
