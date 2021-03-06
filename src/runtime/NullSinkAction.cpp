/*
 * NullSinkAction.cpp
 *
 *  Created on: Aug 2, 2012
 *      Author: tcpan
 */

#include "NullSinkAction.h"
#include "Debug.h"

namespace cci {
namespace rt {

NullSinkAction::NullSinkAction(MPI_Comm const * _parent_comm, int const _gid,
		DataBuffer *_input, DataBuffer *_output,
		cci::common::LogSession *_logsession) :
		Action_I(_parent_comm, _gid, _input, _output, _logsession) {
	// TODO Auto-generated constructor stub
	assert(_input != NULL);
}

NullSinkAction::~NullSinkAction() {
	// TODO Auto-generated destructor stub
	cci::common::Debug::print("%s destructor:  discarded %d\n", getClassName(), call_count);

}



int NullSinkAction::run() {

	if (this->inputBuf->isFinished()) {
//		cci::common::Debug::print("%s input DONE.  input count = %d\n", getClassName(), call_count);
		return Communicator_I::DONE;
	} else if (!this->inputBuf->canPop()) {
		return Communicator_I::WAIT;
	}

	DataBuffer::DataType data;
	void *input = NULL;


	int bstat = this->inputBuf->pop(data);
	if (bstat == DataBuffer::EMPTY) {
		return Communicator_I::WAIT;
	}
	call_count++;
	input = data.second;
	if (input != NULL) {
		free(input);
		input = NULL;
	} else {
		cci::common::Debug::print("ERROR: %s NULL INPUT from buffer!!!\n", getClassName());
	}

	return Communicator_I::READY;

}

} /* namespace rt */
} /* namespace cci */
