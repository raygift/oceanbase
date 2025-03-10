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

#include "ob_simple_log_cluster_testbase.h"
#include "common/ob_member_list.h"
#include "share/allocator/ob_tenant_mutil_allocator_mgr.h"
#include "ob_mittest_utils.h"
#include "lib/alloc/memory_dump.h"
#include "lib/alloc/alloc_func.cpp"
#include "lib/allocator/ob_mem_leak_checker.h"
#include "lib/allocator/ob_libeasy_mem_pool.h"
#include <algorithm>

namespace oceanbase
{
namespace unittest
{
// int64_t ObSimpleLogClusterTestBase::member_cnt_ = 3;
// int64_t ObSimpleLogClusterTestBase::node_cnt_ = 7;
std::vector<ObISimpleLogServer*> ObSimpleLogClusterTestBase::cluster_;
bool ObSimpleLogClusterTestBase::is_started_ = false;
common::ObMemberList ObSimpleLogClusterTestBase::member_list_ = ObMemberList();
common::ObArrayHashMap<common::ObAddr, common::ObRegion> ObSimpleLogClusterTestBase::member_region_map_;
common::ObMemberList ObSimpleLogClusterTestBase::node_list_ = ObMemberList();
int64_t ObSimpleLogClusterTestBase::node_idx_base_ = 1000;
char ObSimpleLogClusterTestBase::sig_buf_[sizeof(ObSignalWorker) + sizeof(observer::ObSignalHandle)];
ObSignalWorker *ObSimpleLogClusterTestBase::sig_worker_ = new (sig_buf_) ObSignalWorker();
observer::ObSignalHandle *ObSimpleLogClusterTestBase::signal_handle_ = new (sig_worker_ + 1) observer::ObSignalHandle();

void ObSimpleLogClusterTestBase::SetUpTestCase()
{
  SERVER_LOG(INFO, "SetUpTestCase", K(member_cnt_), K(node_cnt_));
  int ret = OB_SUCCESS;
  if (!is_started_) {
    ret = start();
  }
  ASSERT_EQ(ret, OB_SUCCESS);
}

void ObSimpleLogClusterTestBase::TearDownTestCase()
{
  SERVER_LOG(INFO, "TearDownTestCase", K(member_cnt_), K(node_cnt_));
  int ret = OB_SUCCESS;

  if (cluster_.size() != 0) {
    ret = close();
    ASSERT_EQ(ret, OB_SUCCESS);
  }
  for (auto svr : cluster_) {
    if (OB_NOT_NULL(svr)) {
      ob_delete(svr);
    }
  }
}

int ObSimpleLogClusterTestBase::start()
{
  int ret = OB_SUCCESS;
  int64_t member_cnt = 0;
  int64_t node_id = node_idx_base_;
  ObTenantMutilAllocatorMgr::get_instance().init();
  ObMemoryDump::get_instance().init();
  // set easy allocator for watching easy memory holding
  easy_pool_set_allocator(ob_easy_realloc);
  ev_set_allocator(ob_easy_realloc);
  lib::set_memory_limit(10L * 1000L * 1000L * 1000L);
  if (sig_worker_ != nullptr && OB_FAIL(sig_worker_->start())) {
    SERVER_LOG(ERROR, "Start signal worker error", K(ret));
  } else if (signal_handle_ != nullptr && OB_FAIL(signal_handle_->start())) {
    SERVER_LOG(ERROR, "Start signal handle error", K(ret));
  } else if (OB_FAIL(member_region_map_.init("TestBase", OB_MAX_MEMBER_NUMBER))) {
  } else if (OB_FAIL(generate_sorted_server_list_(node_cnt_))) {
  } else {
    // 如果需要新增arb server，将其作为memberlist最后一项
    // TODO by runlin, 这个是暂时的解决方法，以后可以走加减成员的流程
    const int64_t arb_idx = member_cnt_ - 1;
    for (int i = 0; OB_SUCC(ret) && i < node_cnt_; i++) {
      ObISimpleLogServer *svr = NULL;
      if (i == arb_idx && true == need_add_arb_server_) {
        svr = OB_NEW(ObSimpleArbServer, "TestBase");
      } else {
        svr = OB_NEW(ObSimpleLogServer, "TestBase");
      }
      common::ObAddr server;
      if (OB_FAIL(node_list_.get_server_by_index(i, server))) {
      } else if (OB_FAIL(svr->simple_init(test_name_, server, node_id++, true))) {
        SERVER_LOG(WARN, "simple_init failed", K(ret), K(i), K_(node_list));
      } else if (OB_FAIL(svr->simple_start(true))) {
        SERVER_LOG(WARN, "simple_start failed", K(ret), K(i), K_(node_list));
      } else {
        cluster_.push_back(svr);
      }
      if (i < ObSimpleLogClusterTestBase::member_cnt_ && OB_SUCC(ret)) {
        common::ObMember member;
        if (OB_FAIL(member_list_.add_member(ObMember(server, 1)))) {
        } else if (OB_FAIL(member_region_map_.insert(server, DEFAULT_REGION_NAME))) {
          SERVER_LOG(WARN, "member_region_map_.insert failed", K(ret), K(server), K_(node_list));
        }
      }
      usleep(500);
      SERVER_LOG(INFO, "ObSimpleLogClusterTestBase start success");
    }
    if (OB_SUCC(ret)) {
      is_started_ = true;
    }
    SERVER_LOG(INFO, "ObSimpleLogClusterTestBase started", K(ret), K_(member_cnt), K_(node_cnt), K_(node_list));
  }
  return ret;
}

int ObSimpleLogClusterTestBase::close()
{
  int ret = OB_SUCCESS;
  for (auto svr : cluster_) {
    ret = svr->simple_close(true);
    if (OB_FAIL(ret)) {
      SERVER_LOG(WARN, "simple_close failed", K(ret));
      break;
    }
  }
  return ret;
}

int ObSimpleLogClusterTestBase::generate_sorted_server_list_(const int64_t node_cnt)
{
  int ret = OB_SUCCESS;
  // each get_rpc_port calling will get two available ports,
  // so just get node_cnt / 2 + 1 times
  const int64_t get_port_cnt = node_cnt / 2 + 1;
  for (int i = 0; i < get_port_cnt; i++) {
    int server_fd = 0;
    const std::string local_ip = get_local_ip();
    const int64_t port = get_rpc_port(server_fd);
    common::ObAddr addr;
    if (0 == port) {
      ret = OB_ERR_UNEXPECTED;
      SERVER_LOG(ERROR, "get_rpc_port failed", K(ret), K(port));
      break;
    } else if (local_ip == "") {
      ret = OB_ERR_UNEXPECTED;
    } else if (false == addr.set_ip_addr(local_ip.c_str(), port)) {
      SERVER_LOG(ERROR, "set_ip_addr failed", K(local_ip.c_str()), K(port), K(addr));
    } else if (OB_FAIL(node_list_.add_server(addr))) {
      PALF_LOG(WARN, "add_server failed", K(ret));
    } else if (false == addr.set_ip_addr(local_ip.c_str(), port + 1)) {
      SERVER_LOG(ERROR, "set_ip_addr failed", K(local_ip.c_str()), K(port), K(addr));
    } else if (node_list_.get_member_number() < node_cnt && OB_FAIL(node_list_.add_server(addr))) {
      PALF_LOG(WARN, "add_server failed", K(ret));
    }
  }
  if (OB_SUCC(ret)) {
    SERVER_LOG(INFO, "simple log cluster node_list", K_(node_list));
  }
  return ret;
}

} // end unittest
} // end oceanbase
