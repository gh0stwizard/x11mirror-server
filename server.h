#ifndef __SERVER_H
#define __SERVER_H

#include "mhd.h"

extern int
accept_policy_cb (void *cls, const struct sockaddr *addr, socklen_t addrlen);

extern int
answer_cb (	void *cls,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **con_cls);

extern void
request_completed_cb (	void *cls,
			struct MHD_Connection *connection,
			void **con_cls,
			enum MHD_RequestTerminationCode toe);

extern void
init_mhd_responses (void);

extern void
free_mhd_responses (void);

extern void
setup_daemon_handler (struct MHD_Daemon *d);

extern void
clear_daemon_handler (void);

#endif /* __SERVER_H */
