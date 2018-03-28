#ifndef __GETWORK_H__
#define __GETWORK_H__

extern void scan_nonce_hash(struct thr_info *thr);

extern bool pool_active_getwork(struct pool *pool, bool pinging);

#endif /* __GETWORK_H__ */
