/*
  Copyright 2014 DataStax

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef __CASS_REQUEST_HPP_INCLUDED__
#define __CASS_REQUEST_HPP_INCLUDED__

#include "macros.hpp"
#include "ref_counted.h"
#include "buffer.hpp"
#include "writer.hpp"

namespace cass {

class RequestMessage;

class Request : public RefCounted<Request> {
public:
  Request(uint8_t opcode)
      : opcode_(opcode) {}

  virtual ~Request() {}

  uint8_t opcode() const { return opcode_; }

private:
  uint8_t opcode_;

private:
  DISALLOW_COPY_AND_ASSIGN(Request);
};

class RequestMessage {
public:
  virtual ~RequestMessage() {}

  const Request* request() const { return request_.get(); }

  uint8_t opcode() const { return request_->opcode(); }

  static RequestMessage* create(Request* request);

  bool encode(int version, int flags, int stream, Writer::Bufs* bufs);

protected:
  RequestMessage(const Request* request)
    : request_(request) {}

  virtual int32_t encode(int version, Writer::Bufs* bufs) = 0;

  Buffer body_head_buf_;
  BufferVec body_collection_bufs_;
  Buffer body_tail_buf_;

private:
  ScopedRefPtr<const Request> request_;
  uint8_t opcode_;
  Buffer header_buf_;

private:
  DISALLOW_COPY_AND_ASSIGN(RequestMessage);
};

} // namespace cass

#endif
