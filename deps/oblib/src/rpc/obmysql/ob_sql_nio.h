/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#ifndef OCEANBASE_OBMYSQL_OB_SQL_NIO_H_
#define OCEANBASE_OBMYSQL_OB_SQL_NIO_H_
#include <pthread.h>
#include <stdint.h>
#include "lib/thread/threads.h"
#include "lib/ssl/ob_ssl_config.h"

namespace oceanbase
{
namespace obmysql
{
class ObSqlNioImpl;
class ObISqlSockHandler;
class ObSqlNio: public lib::Threads
{
public:
  ObSqlNio(): impl_(NULL) {}
  virtual ~ObSqlNio() {}
  int start(int port, ObISqlSockHandler* handler, int n_thread);
  bool has_error(void* sess);
  void destroy_sock(void* sess);
  void revert_sock(void* sess);
  int peek_data(void* sess, int64_t limit, const char*& buf, int64_t& sz);
  int consume_data(void* sess, int64_t sz);
  int write_data(void* sess, const char* buf, int64_t sz);
  void async_write_data(void* sess, const char* buf, int64_t sz);
  void stop();
  void wait();
  void destroy();
  void set_last_decode_succ_time(void* sess, int64_t time);
  void reset_sql_session_info(void* sess);
  void set_sql_session_info(void* sess, void* sql_session);
  void set_shutdown(void* sess);
  void shutdown(void* sess);
  int set_ssl_enabled(void* sess);
  SSL* get_ssl_st(void* sess);
  void update_tcp_keepalive_params(int keepalive_enabled, uint32_t tcp_keepidle, uint32_t tcp_keepintvl, uint32_t tcp_keepcnt);
private:
  void run(int64_t idx);
private:
  ObSqlNioImpl* impl_;
};

}; // end namespace obmysql
}; // end namespace oceanbase

#endif /* OCEANBASE_OBMYSQL_OB_SQL_NIO_H_ */

