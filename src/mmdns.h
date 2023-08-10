/*
 * @mindmaze_header@
 */
#ifndef MMDNS_H
#define MMDNS_H


#define MMDNS_NST_TXT        16
#define MMDNS_NST_SRV        33


struct mmdns_txt {
	struct mmdns_rr_txt* next;
	char value[];
};


struct mmdns_srv {
	struct mmdns_rr_srv* next;
	int priority;
	int weight;
	char target[];
};


struct mmdns_request {
	const char* name;
	int type;
	void* data;
	size_t datalen;
};

struct mmdns_ctx* mmdns_ctx_create(void);
void mmdns_ctx_destroy(struct mmdns_ctx* ctx);

int mmdns_query(struct mmdns_ctx* ctx, struct mmdns_request* req);

#endif  /* MMDNS_H */
