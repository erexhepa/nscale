/*
 * PullCommHandler.h
 *
 *  Created on: Jun 13, 2012
 *      Author: tcpan
 */

#ifndef PULLCOMMHANDLER_H_
#define PULLCOMMHANDLER_H_

#include "CommHandler_I.h"

namespace cci {
namespace rt {

class PullCommHandler: public cci::rt::CommHandler_I {
public:
	PullCommHandler(MPI_Comm const *_parent_comm, int const _gid,
			MPIDataBuffer *_buffer, Scheduler_I * _scheduler,
			cci::common::LogSession *_logsession = NULL);

	virtual ~PullCommHandler();

	virtual const char* getClassName() { return "PullCommHandler"; };

	virtual int run();

private:
	int send_count;
};

} /* namespace rt */
} /* namespace cci */
#endif /* PULLCOMMHANDLER_H_ */
