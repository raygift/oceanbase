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

#define USING_LOG_PREFIX STORAGE

#include "storage/backup/ob_backup_utils.h"
#include "lib/container/ob_iarray.h"
#include "lib/lock/ob_mutex.h"
#include "lib/hash/ob_hashmap.h"
#include "lib/oblog/ob_log_module.h"
#include "lib/time/ob_time_utility.h"
#include "share/rc/ob_tenant_base.h"
#include "storage/backup/ob_backup_factory.h"
#include "storage/backup/ob_backup_operator.h"
#include "storage/backup/ob_backup_reader.h"
#include "storage/ls/ob_ls.h"
#include "storage/ls/ob_ls_tablet_service.h"
#include "storage/meta_mem/ob_tablet_handle.h"
#include "storage/tablet/ob_tablet_meta.h"
#include "storage/tablet/ob_tablet_table_store.h"
#include "storage/tx_storage/ob_ls_map.h"
#include "storage/tx_storage/ob_ls_service.h"
#include "storage/high_availability/ob_storage_ha_utils.h"
#include "observer/ob_server_event_history_table_operator.h"
#include "share/scn.h"
#include "storage/blocksstable/ob_logic_macro_id.h"

#include <algorithm>

using namespace oceanbase::lib;
using namespace oceanbase::common;
using namespace oceanbase::share;
using namespace oceanbase::storage;
using namespace oceanbase::blocksstable;
using namespace oceanbase::palf;

namespace oceanbase {
namespace backup {

/* ObBackupUtils */

int ObBackupUtils::get_sstables_by_data_type(const storage::ObTabletHandle &tablet_handle, const share::ObBackupDataType &backup_data_type,
    storage::ObTabletTableStore &tablet_table_store, common::ObIArray<storage::ObITable *> &sstable_array)
{
  int ret = OB_SUCCESS;
  sstable_array.reset();
  if (!backup_data_type.is_valid() || !tablet_table_store.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(backup_data_type), K(tablet_table_store));
  } else if (backup_data_type.is_sys_backup()) {
    storage::ObSSTableArray *minor_sstable_array_ptr = NULL;
    storage::ObSSTableArray *major_sstable_array_ptr = NULL;
    minor_sstable_array_ptr = &tablet_table_store.get_minor_sstables();
    major_sstable_array_ptr = &tablet_table_store.get_major_sstables();
    ObArray<storage::ObITable *> minor_sstable_array;
    ObArray<storage::ObITable *> major_sstable_array;
    if (OB_FAIL(minor_sstable_array_ptr->get_all_tables(minor_sstable_array))) {
      LOG_WARN("failed to get all tables", K(ret), KPC(minor_sstable_array_ptr));
    } else if (OB_FAIL(check_tablet_minor_sstable_validity_(tablet_handle, minor_sstable_array))) {
      LOG_WARN("failed to check tablet minor sstable validity", K(ret), K(tablet_handle), K(minor_sstable_array));
    } else if (OB_FAIL(major_sstable_array_ptr->get_all_tables(major_sstable_array))) {
      LOG_WARN("failed to get all tables", K(ret), KPC(minor_sstable_array_ptr));
    } else if (OB_FAIL(append(sstable_array, minor_sstable_array))) {
      LOG_WARN("failed to append", K(ret), K(minor_sstable_array));
    } else if (OB_FAIL(append(sstable_array, major_sstable_array))) {
      LOG_WARN("failed to append", K(ret), K(major_sstable_array));
    }
  } else if (backup_data_type.is_minor_backup()) {
    storage::ObSSTableArray *minor_sstable_array_ptr = NULL;
    storage::ObSSTableArray *ddl_sstable_array_ptr = NULL;
    minor_sstable_array_ptr = &tablet_table_store.get_minor_sstables();
    ddl_sstable_array_ptr = &tablet_table_store.get_ddl_sstables();
    ObArray<storage::ObITable *> minor_sstable_array;
    ObArray<storage::ObITable *> ddl_sstable_array;
    if (OB_FAIL(minor_sstable_array_ptr->get_all_tables(minor_sstable_array))) {
      LOG_WARN("failed to get all tables", K(ret), KPC(minor_sstable_array_ptr));
    } else if (OB_FAIL(ddl_sstable_array_ptr->get_all_tables(ddl_sstable_array))) {
      LOG_WARN("failed to get all tables", K(ret), KPC(ddl_sstable_array_ptr));
    } else if (OB_FAIL(check_tablet_minor_sstable_validity_(tablet_handle, minor_sstable_array))) {
      LOG_WARN("failed to check tablet minor sstable validity", K(ret), K(tablet_handle), K(minor_sstable_array));
    } else if (OB_FAIL(check_tablet_ddl_sstable_validity_(tablet_handle, ddl_sstable_array))) {
      LOG_WARN("failed to check tablet ddl sstable validity", K(ret), K(tablet_handle), K(ddl_sstable_array));
    } else if (OB_FAIL(append(sstable_array, minor_sstable_array))) {
      LOG_WARN("failed to append", K(ret), K(minor_sstable_array));
    } else if (OB_FAIL(append(sstable_array, ddl_sstable_array))) {
      LOG_WARN("failed to append", K(ret), K(ddl_sstable_array));
    }
  } else if (backup_data_type.is_major_backup()) {
    storage::ObSSTableArray *major_sstable_array_ptr = NULL;
    major_sstable_array_ptr = &tablet_table_store.get_major_sstables();
    ObITable *last_major_sstable_ptr = NULL;
    bool with_major_sstable = true;
    if (OB_ISNULL(last_major_sstable_ptr = major_sstable_array_ptr->get_boundary_table(true /*last*/))) {
      if (OB_FAIL(check_tablet_with_major_sstable(tablet_handle, with_major_sstable))) {
        LOG_WARN("failed to check tablet with major sstable", K(ret));
      }
      if (OB_SUCC(ret) && with_major_sstable) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("last major sstable should not be null", K(ret), K(tablet_handle));
      }
    } else if (OB_FAIL(sstable_array.push_back(last_major_sstable_ptr))) {
      LOG_WARN("failed to push back", K(ret), KPC(last_major_sstable_ptr));
    }
  }
  return ret;
}

int ObBackupUtils::check_tablet_with_major_sstable(const storage::ObTabletHandle &tablet_handle, bool &with_major)
{
  int ret = OB_SUCCESS;
  if (!tablet_handle.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_handle));
  } else {
    with_major = tablet_handle.get_obj()->get_tablet_meta().table_store_flag_.with_major_sstable();
    if (!with_major) {
      LOG_INFO("tablet not with major", K(tablet_handle));
    }
  }
  return ret;
}

int ObBackupUtils::fetch_macro_block_logic_id_list(const storage::ObTabletHandle &tablet_handle,
    const blocksstable::ObSSTable &sstable, common::ObIArray<blocksstable::ObLogicMacroBlockId> &logic_id_list)
{
  int ret = OB_SUCCESS;
  logic_id_list.reset();
  ObArenaAllocator allocator;
  ObDatumRange datum_range;
  SMART_VAR(ObSSTableSecMetaIterator, meta_iter)
  {
    if (!sstable.is_valid()) {
      ret = OB_INVALID_ARGUMENT;
      LOG_WARN("get invalid args", K(ret), K(sstable));
    } else if (FALSE_IT(datum_range.set_whole_range())) {
    } else if (OB_FAIL(meta_iter.open(datum_range,
                   ObMacroBlockMetaType::DATA_BLOCK_META,
                   sstable,
                   tablet_handle.get_obj()->get_index_read_info(),
                   allocator))) {
      LOG_WARN("failed to open sec meta iterator", K(ret));
    } else {
      ObDataMacroBlockMeta data_macro_block_meta;
      ObLogicMacroBlockId logic_id;
      while (OB_SUCC(ret)) {
        data_macro_block_meta.reset();
        logic_id.reset();
        if (OB_FAIL(meta_iter.get_next(data_macro_block_meta))) {
          if (OB_ITER_END == ret) {
            ret = OB_SUCCESS;
            break;
          } else {
            LOG_WARN("failed to get next", K(ret));
          }
        } else {
          logic_id = data_macro_block_meta.get_logic_id();
          if (OB_FAIL(logic_id_list.push_back(logic_id))) {
            LOG_WARN("failed to push back", K(ret), K(logic_id));
          }
        }
      }
    }
  }
  return ret;
}

int ObBackupUtils::report_task_result(const int64_t job_id, const int64_t task_id, const uint64_t tenant_id,
    const share::ObLSID &ls_id, const int64_t turn_id, const int64_t retry_id, const share::ObTaskId trace_id,
    const int64_t result, ObBackupReportCtx &report_ctx)
{
  int ret = OB_SUCCESS;
  common::ObAddr rs_addr;
  obrpc::ObBackupTaskRes backup_ls_res;
  backup_ls_res.job_id_ = job_id;
  backup_ls_res.task_id_ = task_id;
  backup_ls_res.tenant_id_ = tenant_id;
  backup_ls_res.src_server_ = GCTX.self_addr();
  backup_ls_res.ls_id_ = ls_id;
  backup_ls_res.result_ = result;
  backup_ls_res.trace_id_ = trace_id;

#ifdef ERRSIM
  ret = OB_E(EventTable::EN_BACKUP_META_REPORT_RESULT_FAILED) OB_SUCCESS;
  if (OB_FAIL(ret)) {
    SERVER_EVENT_SYNC_ADD("backup_errsim", "before report task result");
    LOG_WARN("errsim backup meta task failed", K(ret), K(backup_ls_res));
  }
#endif
  if (OB_FAIL(ret)) {
  } else if (job_id <= 0 || task_id <= 0 || OB_INVALID_ID == tenant_id || !ls_id.is_valid() || !report_ctx.is_valid()
      || trace_id.is_invalid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(job_id), K(task_id), K(tenant_id), K(ls_id));
  } else if (OB_FAIL(report_ctx.rs_mgr_->get_master_root_server(rs_addr))) {
    LOG_WARN("failed to get rootservice address", K(ret));
  } else if (OB_FAIL(report_ctx.rs_rpc_proxy_->to(rs_addr).backup_ls_data_res(backup_ls_res))) {
    LOG_WARN("failed to post backup ls data res", K(ret), K(backup_ls_res));
  } else {
    SERVER_EVENT_ADD("backup_data", "report_result",
        "job_id", job_id,
        "task_id", task_id,
        "tenant_id", tenant_id,
        "ls_id", ls_id.id(),
        "turn_id", turn_id,
        "retry_id", retry_id,
        result);
    LOG_INFO("finish task post rpc result", K(backup_ls_res));
  }
  return ret;
}

int ObBackupUtils::check_tablet_minor_sstable_validity_(const storage::ObTabletHandle &tablet_handle,
    const common::ObIArray<storage::ObITable *> &minor_sstable_array)
{
  int ret = OB_SUCCESS;
  ObTablet *tablet = NULL;
  ObITable *last_table_ptr = NULL;
  ObTabletID tablet_id;
  SCN start_scn = SCN::min_scn();
  SCN clog_checkpoint_scn = SCN::min_scn();
  if (OB_ISNULL(tablet = tablet_handle.get_obj())) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("invalid tablet handle", K(ret), K(tablet_handle));
  } else {
    const ObTabletMeta &tablet_meta = tablet->get_tablet_meta();
    tablet_id = tablet_meta.tablet_id_;
    start_scn = tablet_meta.start_scn_;
    clog_checkpoint_scn = tablet_meta.clog_checkpoint_scn_;
  }

  if (OB_FAIL(ret)) {
  } else if ((minor_sstable_array.empty() && start_scn != clog_checkpoint_scn)
      || (!minor_sstable_array.empty() && start_scn >= clog_checkpoint_scn)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("clog checkpoint ts unexpected", K(tablet_id), K(clog_checkpoint_scn), K(start_scn));
  }

  if (OB_FAIL(ret)) {
  } else if (minor_sstable_array.empty()) {
    // do nothing
  } else if (OB_ISNULL(last_table_ptr = minor_sstable_array.at(minor_sstable_array.count() - 1))) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("get invalid table ptr", K(ret), K(minor_sstable_array));
  } else if (!last_table_ptr->is_minor_sstable()) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("table ptr not correct", K(ret), KPC(last_table_ptr));
  } else {
    const ObITable::TableKey &table_key = last_table_ptr->get_key();
    if (table_key.get_end_scn() < clog_checkpoint_scn) {
      ret = OB_ERR_UNEXPECTED;
      LOG_ERROR("tablet meta is not valid", K(ret), K(table_key), K(clog_checkpoint_scn));
    }
  }
  return ret;
}

int ObBackupUtils::check_tablet_ddl_sstable_validity_(const storage::ObTabletHandle &tablet_handle,
    const common::ObIArray<storage::ObITable *> &ddl_sstable_array)
{
  int ret = OB_SUCCESS;
  ObTablet *tablet = NULL;
  ObITable *last_table_ptr = NULL;
  SCN tablet_ddl_checkpoint_scn = SCN::min_scn();
  if (OB_ISNULL(tablet = tablet_handle.get_obj())) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("invalid tablet handle", K(ret), K(tablet_handle));
  } else {
    const ObTabletMeta &tablet_meta = tablet->get_tablet_meta();
    tablet_ddl_checkpoint_scn = tablet_meta.ddl_checkpoint_scn_;
  }
  if (OB_FAIL(ret)) {
  } else if (ddl_sstable_array.empty()) {
    // do nothing
  } else if (OB_ISNULL(last_table_ptr = ddl_sstable_array.at(ddl_sstable_array.count() - 1))) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("get invalid table ptr", K(ret), K(ddl_sstable_array));
  } else if (!last_table_ptr->is_ddl_dump_sstable()) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("table ptr not correct", K(ret), KPC(last_table_ptr));
  } else {
    const ObITable::TableKey &table_key = last_table_ptr->get_key();
    if (table_key.get_end_scn() != tablet_ddl_checkpoint_scn) {
      ret = OB_ERR_UNEXPECTED;
      LOG_ERROR("tablet meta is not valid", K(ret), K(table_key), K(tablet_ddl_checkpoint_scn));
    }
  }
  return ret;
}

int ObBackupUtils::check_ls_validity(const uint64_t tenant_id, const share::ObLSID &ls_id)
{
  int ret = OB_SUCCESS;
  const int64_t cluster_id = GCONF.cluster_id;
  const common::ObAddr &local_addr = GCTX.self_addr();
  bool in_member_list = false;
  common::ObAddr leader_addr;
  common::ObArray<common::ObAddr> addr_list;
  if (OB_INVALID_ID == tenant_id || !ls_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tenant_id), K(ls_id));
  } else if (OB_FAIL(get_ls_leader_(tenant_id, ls_id, leader_addr))) {
    LOG_WARN("failed to get ls leader", K(ret), K(tenant_id), K(ls_id));
  } else if (OB_FAIL(fetch_ls_member_list_(tenant_id, ls_id, leader_addr, addr_list))) {
    LOG_WARN("failed to fetch ls leader member list", K(ret), K(tenant_id), K(ls_id), K(leader_addr));
  } else {
    for (int64_t i = 0; OB_SUCC(ret) && i < addr_list.count(); ++i) {
      const common::ObAddr &addr = addr_list.at(i);
      if (addr == local_addr) {
        in_member_list = true;
        break;
      }
    }
  }
  if (OB_SUCC(ret) && !in_member_list) {
    ret = OB_REPLICA_CANNOT_BACKUP;
    LOG_WARN("not valid ls replica", K(ret), K(tenant_id), K(ls_id), K(leader_addr), K(local_addr), K(addr_list));
  }
  return ret;
}

int ObBackupUtils::check_ls_valid_for_backup(const uint64_t tenant_id, const share::ObLSID &ls_id, const int64_t local_rebuild_seq)
{
  int ret = OB_SUCCESS;
  storage::ObLS *ls = NULL;
  ObLSService *ls_service = NULL;
  ObLSHandle handle;
  int64_t cur_rebuild_seq = 0;
  ObLSMeta ls_meta;
  const bool check_archive = false;
  if (OB_INVALID_ID == tenant_id || !ls_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tenant_id), K(ls_id));
  } else if (OB_ISNULL(ls_service = MTL_WITH_CHECK_TENANT(ObLSService *, tenant_id))) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("log stream service is NULL", K(ret));
  } else if (OB_FAIL(ls_service->get_ls(ls_id, handle, ObLSGetMod::STORAGE_MOD))) {
    LOG_WARN("failed to get log stream", K(ret), K(ls_id));
  } else if (OB_ISNULL(ls = handle.get_ls())) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("log stream not exist", K(ret), K(ls_id));
  } else if (ls->is_stopped()) {
    ret = OB_REPLICA_CANNOT_BACKUP;
    LOG_WARN("ls has stopped, can not backup", K(ret), K(tenant_id), K(ls_id));
  } else if (OB_FAIL(ls->get_ls_meta(ls_meta))) {
    LOG_WARN("failed to get ls meta", K(ret), K(tenant_id), K(ls_id));
  } else if (OB_FAIL(ls_meta.check_valid_for_backup())) {
    LOG_WARN("failed to check valid for backup", K(ret), K(ls_meta));
  } else {
    cur_rebuild_seq = ls_meta.get_rebuild_seq();
    if (local_rebuild_seq != cur_rebuild_seq) {
      ret = OB_REPLICA_CANNOT_BACKUP;
      LOG_WARN("rebuild seq has changed, can not backup", K(ret), K(tenant_id), K(ls_id), K(local_rebuild_seq), K(cur_rebuild_seq));
    }
  }
  return ret;
}

int ObBackupUtils::get_ls_leader_(const uint64_t tenant_id, const share::ObLSID &ls_id, common::ObAddr &leader)
{
  int ret = OB_SUCCESS;
  int tmp_ret = OB_SUCCESS;
  static const int64_t DEFAULT_CHECK_LS_LEADER_TIMEOUT = 30 * 1000 * 1000L; // 30s
  const int64_t cluster_id = GCONF.cluster_id;
  if (OB_ISNULL(GCTX.location_service_)) {
    ret = OB_NOT_INIT;
    LOG_WARN("location cache is NULL", K(ret));
  } else if (OB_INVALID_ID == tenant_id || !ls_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tenant_id), K(ls_id));
  } else {
    const int64_t retry_us = 200 * 1000;
    const int64_t start_ts = ObTimeUtility::current_time();
    do {
      if (OB_FAIL(GCTX.location_service_->nonblock_get_leader(cluster_id, tenant_id, ls_id, leader))) {
        if (OB_LS_LOCATION_NOT_EXIST == ret) {
          LOG_WARN("failed to get location and force renew", K(ret), K(tenant_id), K(ls_id), K(cluster_id));
          if (OB_TMP_FAIL(GCTX.location_service_->nonblock_renew(cluster_id, tenant_id, ls_id))) {
            LOG_WARN("failed to nonblock renew from location cache", K(tmp_ret), K(cluster_id), K(tenant_id), K(ls_id));
          }
          if (ObTimeUtility::current_time() - start_ts > DEFAULT_CHECK_LS_LEADER_TIMEOUT) {
            break;
          } else {
            ob_usleep(retry_us);
          }
        } else {
          LOG_WARN("failed to nonblock get leader", K(ret), K(cluster_id), K(tenant_id), K(ls_id));
        }
      } else {
        LOG_INFO("nonblock get leader", K(tenant_id), K(ls_id), K(leader), K(cluster_id));
      }
    } while (OB_LS_LOCATION_NOT_EXIST == ret);

    if (OB_SUCC(ret) && !leader.is_valid()) {
      ret = OB_ERR_UNEXPECTED;
      LOG_WARN("leader addr is invalid", K(ret), K(tenant_id), K(ls_id), K(leader), K(cluster_id));
    }
  }
  return ret;
}

int ObBackupUtils::fetch_ls_member_list_(const uint64_t tenant_id, const share::ObLSID &ls_id,
    const common::ObAddr &leader_addr, common::ObIArray<common::ObAddr> &addr_list)
{
  int ret = OB_SUCCESS;
  ObLSService *ls_service = NULL;
  storage::ObStorageRpc *storage_rpc = NULL;
  storage::ObStorageHASrcInfo src_info;
  src_info.src_addr_ = leader_addr;
  src_info.cluster_id_ = GCONF.cluster_id;
  obrpc::ObFetchLSMemberListInfo member_info;
  if (OB_ISNULL(ls_service = MTL_WITH_CHECK_TENANT(ObLSService *, tenant_id))) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("log stream service is NULL", K(ret));
  } else if (OB_ISNULL(storage_rpc = ls_service->get_storage_rpc())) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("storage rpc proxy is NULL", K(ret));
  } else if (OB_FAIL(storage_rpc->post_ls_member_list_request(tenant_id, src_info, ls_id, member_info))) {
    LOG_WARN("failed to post ls member list request", K(ret), K(tenant_id), K(src_info), K(ls_id));
  } else if (OB_FAIL(member_info.member_list_.get_addr_array(addr_list))) {
    LOG_WARN("failed to get addr array", K(ret), K(member_info));
  }
  return ret;
}

/* ObBackupTabletCtx */

ObBackupTabletCtx::ObBackupTabletCtx()
    : total_tablet_meta_count_(0),
      total_sstable_meta_count_(0),
      reused_macro_block_count_(0),
      total_macro_block_count_(0),
      finish_tablet_meta_count_(0),
      finish_sstable_meta_count_(0),
      finish_macro_block_count_(0),
      is_all_loaded_(false),
      mappings_()
{}

ObBackupTabletCtx::~ObBackupTabletCtx()
{}

void ObBackupTabletCtx::reuse()
{
  total_tablet_meta_count_ = 0;
  total_sstable_meta_count_ = 0;
  reused_macro_block_count_ = 0;
  total_macro_block_count_ = 0;
  finish_tablet_meta_count_ = 0;
  finish_sstable_meta_count_ = 0;
  finish_macro_block_count_ = 0;
  is_all_loaded_ = false;
  mappings_.reuse();
}

void ObBackupTabletCtx::print_ctx()
{
  LOG_INFO("print ctx", K_(total_tablet_meta_count), K_(finish_tablet_meta_count), K_(total_sstable_meta_count),
      K_(finish_sstable_meta_count), K_(reused_macro_block_count), K_(total_macro_block_count),
      K_(finish_macro_block_count), K_(is_all_loaded), "sstable_count", mappings_.sstable_count_);
  for (int64_t i = 0; i < mappings_.sstable_count_; ++i) {
    ObBackupMacroBlockIDMapping &mapping = mappings_.id_map_list_[i];
    LOG_INFO("print backup macro block id mapping", K(i), K(mapping));
  }
}

int ObBackupTabletCtx::record_macro_block_physical_id(const storage::ObITable::TableKey &table_key,
    const blocksstable::ObLogicMacroBlockId &logic_id, const ObBackupPhysicalID &physical_id)
{
  int ret = OB_SUCCESS;
  bool found = false;
  if (!table_key.is_valid() || !logic_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(table_key), K(logic_id));
  }
  for (int64_t i = 0; OB_SUCC(ret) && i < mappings_.sstable_count_; ++i) {
    ObBackupMacroBlockIDMapping &mapping = mappings_.id_map_list_[i];
    if (mapping.table_key_ == table_key) {
      found = true;
      int64_t idx = 0;
      if (!mapping.map_.created()) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("id map not created", K(ret), K(i));
      } else if (OB_FAIL(mapping.map_.get_refactored(logic_id, idx))) {
        LOG_WARN("failed to get refactored", K(ret), K(i), K(logic_id));
      } else if (idx >= mapping.id_pair_list_.count()) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("idx out of range", K(ret), K(idx), K(i), "array_count", mapping.id_pair_list_.count());
      } else {
        ObBackupMacroBlockIDPair pair;
        pair.logic_id_ = logic_id;
        pair.physical_id_ = physical_id;
        mapping.id_pair_list_.at(idx) = pair;
        LOG_INFO("record macro block id", K(table_key), K(logic_id), K(physical_id));
      }
    }
  }
  if (OB_SUCC(ret) && !found) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("table key do not exist",
        K(ret),
        K(table_key),
        "sstable_count",
        mappings_.sstable_count_,
        K(logic_id),
        K(physical_id));
  }
  return ret;
}

/* ObBackupTabletStat */

ObBackupTabletStat::ObBackupTabletStat()
    : is_inited_(false),
      mutex_(common::ObLatchIds::BACKUP_LOCK),
      tenant_id_(OB_INVALID_ID),
      backup_set_id_(0),
      ls_id_(),
      stat_map_(),
      backup_data_type_()
{}

ObBackupTabletStat::~ObBackupTabletStat()
{
  reset();
}

int ObBackupTabletStat::init(const uint64_t tenant_id, const int64_t backup_set_id, const share::ObLSID &ls_id,
    const share::ObBackupDataType &backup_data_type)
{
  int ret = OB_SUCCESS;
  if (IS_INIT) {
    ret = OB_INIT_TWICE;
    LOG_WARN("backup tablet stat init twice", K(ret));
  } else if (OB_INVALID_ID == tenant_id || backup_set_id <= 0 || !ls_id.is_valid() || !backup_data_type.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tenant_id), K(backup_set_id), K(ls_id), K(backup_data_type));
  } else if (OB_FAIL(stat_map_.create(DEFAULT_BUCKET_COUNT, ObModIds::BACKUP))) {
    LOG_WARN("failed to create stat map", K(ret));
  } else {
    tenant_id_ = tenant_id;
    backup_set_id_ = backup_set_id;
    ls_id_ = ls_id;
    backup_data_type_ = backup_data_type;
    is_inited_ = true;
  }
  return ret;
}

int ObBackupTabletStat::prepare_tablet_sstables(const share::ObBackupDataType &backup_data_type,
    const common::ObTabletID &tablet_id, const storage::ObTabletHandle &tablet_handle,
    const common::ObIArray<storage::ObITable *> &sstable_array)
{
  int ret = OB_SUCCESS;
  ObMutexGuard guard(mutex_);
  const bool create_if_not_exist = true;
  ObBackupTabletCtx *stat = NULL;
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("backup tablet stat do not init", K(ret));
  } else if (OB_FAIL(get_tablet_stat_(tablet_id, create_if_not_exist, stat))) {
    LOG_WARN("failed to get tablet stat", K(ret), K(tablet_id));
  } else if (OB_ISNULL(stat)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("stat should not be null", K(ret), K(tablet_id));
  } else if (backup_data_type.type_ != backup_data_type_.type_) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("backup data type not match", K(backup_data_type), K(backup_data_type_));
  } else {
    stat->mappings_.version_ = ObBackupMacroBlockIDMappingsMeta::MAPPING_META_VERSION_V1;
    stat->mappings_.sstable_count_ = sstable_array.count();
    for (int64_t i = 0; OB_SUCC(ret) && i < sstable_array.count(); ++i) {
      ObITable *table_ptr = sstable_array.at(i);
      if (OB_ISNULL(table_ptr)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("table ptr should not be null", K(ret), K(i), K(sstable_array));
      } else if (!table_ptr->is_sstable()) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("table is not sstable", K(ret), KPC(table_ptr));
      } else {
        const ObITable::TableKey &table_key = table_ptr->get_key();
        ObBackupMacroBlockIDMapping &id_mapping = stat->mappings_.id_map_list_[i];
        common::ObArray<ObLogicMacroBlockId> logic_id_list;
        ObSSTable *sstable_ptr = static_cast<ObSSTable *>(table_ptr);
        if (OB_FAIL(ObBackupUtils::fetch_macro_block_logic_id_list(tablet_handle, *sstable_ptr, logic_id_list))) {
          LOG_WARN("failed to fetch macro block logic id list", K(ret), K(tablet_handle), KPC(sstable_ptr));
        } else if (OB_FAIL(id_mapping.prepare_tablet_sstable(table_key, logic_id_list))) {
          LOG_WARN("failed to prepare tablet sstable", K(ret), K(table_key), K(logic_id_list));
        } else {
          LOG_INFO("prepare tablet sstable", K(backup_data_type), K(tablet_id), K(table_key), K(logic_id_list));
        }
      }
    }
  }
  return ret;
}

int ObBackupTabletStat::mark_items_pending(
    const share::ObBackupDataType &backup_data_type, const common::ObIArray<ObBackupProviderItem> &items)
{
  int ret = OB_SUCCESS;
  ObMutexGuard guard(mutex_);
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("backup tablet stat do not init", K(ret));
  } else if (backup_data_type.type_ != backup_data_type_.type_) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("backup data type not match", K(backup_data_type), K(backup_data_type_));
  } else {
    for (int64_t i = 0; OB_SUCC(ret) && i < items.count(); ++i) {
      const ObBackupProviderItem &item = items.at(i);
      const ObBackupProviderItemType &item_type = item.get_item_type();
      if (OB_FAIL(do_with_stat_when_pending_(item))) {
        LOG_WARN("failed to do with stat when pending", K(ret), K(item));
      }
    }
  }
  return ret;
}

int ObBackupTabletStat::mark_items_reused(const share::ObBackupDataType &backup_data_type,
    const common::ObIArray<ObBackupProviderItem> &items, common::ObIArray<ObBackupPhysicalID> &physical_ids)
{
  int ret = OB_SUCCESS;
  ObMutexGuard guard(mutex_);
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("backup tablet stat do not init", K(ret));
  } else if (backup_data_type.type_ != backup_data_type_.type_) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("backup data type not match", K(backup_data_type), K(backup_data_type_));
  } else if (items.count() != physical_ids.count()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("item count should be same", K(items.count()), K(physical_ids.count()));
  } else {
    for (int64_t i = 0; OB_SUCC(ret) && i < items.count(); ++i) {
      const ObBackupProviderItem &item = items.at(i);
      const ObBackupPhysicalID &physical_id = physical_ids.at(i);
      if (OB_FAIL(do_with_stat_when_reused_(item, physical_id))) {
        LOG_WARN("failed to do with stat when reused", K(ret), K(item));
      } else {
        LOG_DEBUG("backup reuse macro block", K_(tenant_id), K_(backup_set_id), K_(ls_id), K(item));
      }
    }
  }
  return ret;
}

int ObBackupTabletStat::mark_item_reused(const share::ObBackupDataType &backup_data_type,
    const ObITable::TableKey &table_key, const ObBackupMacroBlockIDPair &id_pair)
{
  int ret = OB_SUCCESS;
  ObMutexGuard guard(mutex_);
  ObBackupProviderItem item;
  ObBackupProviderItemType item_type = PROVIDER_ITEM_MACRO_ID;
  ObBackupMacroBlockId macro_id;
  macro_id.macro_block_id_ = ObBackupProviderItem::get_fake_macro_id_();
  macro_id.logic_id_ = id_pair.logic_id_;
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("backup tablet stat do not init", K(ret));
  } else if (backup_data_type.type_ != backup_data_type_.type_) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("backup data type not match", K(backup_data_type), K(backup_data_type_));
  } else if (OB_FAIL(item.set(item_type, macro_id, table_key, ObTabletID(id_pair.logic_id_.tablet_id_)))) {
    LOG_WARN("failed to set provider item", K(ret), K(item));
  } else if (OB_FAIL(do_with_stat_when_reused_(item, id_pair.physical_id_))) {
    LOG_WARN("failed to do with stat when reused", K(ret));
  }
  return ret;
}

int ObBackupTabletStat::mark_item_finished(const share::ObBackupDataType &backup_data_type,
    const ObBackupProviderItem &item, const ObBackupPhysicalID &physical_id, bool &is_all_finished)
{
  int ret = OB_SUCCESS;
  ObMutexGuard guard(mutex_);
  const ObTabletID &tablet_id = item.get_tablet_id();
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("backup tablet stat do not init", K(ret));
  } else if (backup_data_type.type_ != backup_data_type_.type_) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("backup data type not match", K(backup_data_type), K(backup_data_type_));
  } else if (OB_FAIL(do_with_stat_when_finish_(item, physical_id))) {
    LOG_WARN("failed to do with stat", K(ret), K(item), K(physical_id));
  } else if (OB_FAIL(check_tablet_finished_(tablet_id, is_all_finished))) {
    LOG_WARN("failed to check tablet finished", K(ret), K(tablet_id));
  } else {
    LOG_INFO("mark backup item finished", K(backup_data_type), K(item), K(physical_id));
  }
  return ret;
}

int ObBackupTabletStat::get_tablet_stat(const common::ObTabletID &tablet_id, ObBackupTabletCtx *&ctx)
{
  int ret = OB_SUCCESS;
  ObMutexGuard guard(mutex_);
  const bool create_if_not_exist = false;
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("backup tablet stat do not init", K(ret));
  } else if (!tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_id));
  } else if (OB_FAIL(get_tablet_stat_(tablet_id, create_if_not_exist, ctx))) {
    LOG_WARN("failed to get tablet stat", K(ret), K(tablet_id));
  }
  return ret;
}

int ObBackupTabletStat::free_tablet_stat(const common::ObTabletID &tablet_id)
{
  int ret = OB_SUCCESS;
  ObMutexGuard guard(mutex_);
  const bool create_if_not_exist = false;
  ObBackupTabletCtx *ctx = NULL;
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("backup tablet stat do not init", K(ret));
  } else if (!tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_id));
  } else if (OB_FAIL(get_tablet_stat_(tablet_id, create_if_not_exist, ctx))) {
    LOG_WARN("failed to get tablet stat", K(ret), K(tablet_id));
  } else {
    free_stat_(ctx);
    if (OB_FAIL(stat_map_.erase_refactored(tablet_id))) {
      LOG_WARN("failed to erase", K(ret), K(tablet_id));
    }
  }
  return ret;
}

int ObBackupTabletStat::print_tablet_stat() const
{
  int ret = OB_SUCCESS;
  PrintTabletStatOp op;
  if (OB_FAIL(stat_map_.foreach_refactored(op))) {
    LOG_WARN("failed to forearch", K(ret));
  }
  return ret;
}

void ObBackupTabletStat::set_backup_data_type(const share::ObBackupDataType &backup_data_type)
{
  ObMutexGuard guard(mutex_);
  backup_data_type_ = backup_data_type;
}

void ObBackupTabletStat::reuse()
{
  ObMutexGuard guard(mutex_);
  ObBackupTabletCtxMap::iterator iter;
  for (iter = stat_map_.begin(); iter != stat_map_.end(); ++iter) {
    if (OB_NOT_NULL(iter->second)) {
      free_stat_(iter->second);
    }
  }
  stat_map_.reuse();
}

void ObBackupTabletStat::reset()
{
  is_inited_ = false;
  reuse();
}

int ObBackupTabletStat::do_with_stat_when_pending_(const ObBackupProviderItem &item)
{
  int ret = OB_SUCCESS;
  ObBackupTabletCtx *stat = NULL;
  const bool create_if_not_exist = false;
  const ObTabletID &tablet_id = item.get_tablet_id();
  if (!item.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(item));
  } else if (OB_FAIL(get_tablet_stat_(tablet_id, create_if_not_exist, stat))) {
    LOG_WARN("failed to get tablet stat", K(ret), K(tablet_id));
  } else if (OB_ISNULL(stat)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("stat should not be null", K(ret), K_(backup_data_type), K(tablet_id));
  } else {
    const ObBackupProviderItemType type = item.get_item_type();
    if (PROVIDER_ITEM_MACRO_ID == type) {
      const ObITable::TableKey &table_key = item.get_table_key();
      const ObLogicMacroBlockId &logic_id = item.get_logic_id();
      const ObBackupPhysicalID &fake_physical_id = ObBackupPhysicalID::get_default();
      if (OB_FAIL(stat->record_macro_block_physical_id(table_key, logic_id, fake_physical_id))) {
        LOG_WARN("failed to record macro block physical id", K(ret), K(table_key), K(logic_id), K(fake_physical_id));
      } else {
        ++stat->total_macro_block_count_;
      }
    } else if (PROVIDER_ITEM_SSTABLE_META == type) {
      ++stat->total_sstable_meta_count_;
    } else if (PROVIDER_ITEM_TABLET_META == type) {
      ++stat->total_tablet_meta_count_;
      stat->is_all_loaded_ = true;
    }
  }
  return ret;
}

int ObBackupTabletStat::do_with_stat_when_reused_(
    const ObBackupProviderItem &item, const ObBackupPhysicalID &physical_id)
{
  int ret = OB_SUCCESS;
  ObBackupTabletCtx *stat = NULL;
  const bool create_if_not_exist = false;
  const ObTabletID &tablet_id = item.get_tablet_id();
  const ObITable::TableKey &table_key = item.get_table_key();
  const ObLogicMacroBlockId &logic_id = item.get_logic_id();
  const ObBackupProviderItemType type = item.get_item_type();
  if (!item.is_valid() || !physical_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(item), K(physical_id));
  } else if (OB_FAIL(get_tablet_stat_(tablet_id, create_if_not_exist, stat))) {
    LOG_WARN("failed to get tablet stat", K(ret), K(tablet_id));
  } else if (OB_ISNULL(stat)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("stat should not be null", K(ret), K_(backup_data_type), K(tablet_id));
  } else if (PROVIDER_ITEM_MACRO_ID != type) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("only macro block can reuse", K(type));
  } else if (OB_FAIL(stat->record_macro_block_physical_id(table_key, logic_id, physical_id))) {
    LOG_WARN("failed to record macro block physical id", K(ret), K(table_key), K(logic_id), K(physical_id));
  } else {
    ++stat->reused_macro_block_count_;
    ++stat->finish_macro_block_count_;
    ++stat->total_macro_block_count_;
  }
  return ret;
}

int ObBackupTabletStat::do_with_stat_when_finish_(
    const ObBackupProviderItem &item, const ObBackupPhysicalID &physical_id)
{
  int ret = OB_SUCCESS;
  ObBackupTabletCtx *stat = NULL;
  const bool create_if_not_exist = false;
  const ObTabletID &tablet_id = item.get_tablet_id();
  if (!item.is_valid() || !physical_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(item), K(physical_id));
  } else if (OB_FAIL(get_tablet_stat_(tablet_id, create_if_not_exist, stat))) {
    LOG_WARN("failed to get tablet stat", K(ret), K(tablet_id));
  } else if (OB_ISNULL(stat)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("stat should not be null", K(ret), K_(backup_data_type), K(tablet_id));
  } else {
    const ObBackupProviderItemType type = item.get_item_type();
    if (PROVIDER_ITEM_MACRO_ID == type) {
      const ObITable::TableKey &table_key = item.get_table_key();
      const ObLogicMacroBlockId &logic_id = item.get_logic_id();
      if (OB_FAIL(stat->record_macro_block_physical_id(table_key, logic_id, physical_id))) {
        LOG_WARN("failed to record macro block physical id", K(ret), K(table_key), K(logic_id), K(physical_id));
      } else {
        ++stat->finish_macro_block_count_;
      }
    } else if (PROVIDER_ITEM_SSTABLE_META == type) {
      ++stat->finish_sstable_meta_count_;
    } else if (PROVIDER_ITEM_TABLET_META == type) {
      ++stat->finish_tablet_meta_count_;
    }
  }
  return ret;
}

int ObBackupTabletStat::check_tablet_finished_(const common::ObTabletID &tablet_id, bool &is_finished)
{
  int ret = OB_SUCCESS;
  is_finished = false;
  ObBackupTabletCtx *stat = NULL;
  const bool create_if_not_exist = false;
  if (OB_FAIL(get_tablet_stat_(tablet_id, create_if_not_exist, stat))) {
    LOG_WARN("failed to get tablet stat", K(ret), K(stat));
  } else if (OB_ISNULL(stat)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("stat should not be null", K(ret), K_(backup_data_type), K(tablet_id));
  } else if (!stat->is_all_loaded_) {
    // not all loaded, false
  } else {
    is_finished = stat->total_macro_block_count_ == stat->finish_macro_block_count_ &&
                  stat->total_sstable_meta_count_ == stat->finish_sstable_meta_count_ &&
                  stat->total_tablet_meta_count_ == stat->finish_tablet_meta_count_;
    if (is_finished) {
      report_event_(tablet_id, *stat);
      LOG_INFO("tablet is finished", K(tablet_id), KPC(stat));
    }
  }
  return ret;
}

int ObBackupTabletStat::get_tablet_stat_(
    const common::ObTabletID &tablet_id, const bool create_if_not_exist, ObBackupTabletCtx *&stat)
{
  int ret = OB_SUCCESS;
  int hash_ret = OB_SUCCESS;
  stat = NULL;
  hash_ret = stat_map_.get_refactored(tablet_id, stat);
  if (OB_HASH_NOT_EXIST == hash_ret) {
    if (create_if_not_exist) {
      if (OB_FAIL(alloc_stat_(stat))) {
        LOG_WARN("failed to alloc stat", K(ret));
      } else if (OB_FAIL(stat_map_.set_refactored(tablet_id, stat, 1))) {
        LOG_WARN("failed to set refactored", K(ret), K(tablet_id), KPC(stat));
      } else {
        stat->mappings_.version_ = ObBackupMacroBlockIDMappingsMeta::MAPPING_META_VERSION_V1;
      }
    } else {
      ret = hash_ret;
    }
  } else if (OB_SUCCESS == hash_ret) {
    // do nothing
  } else {
    ret = hash_ret;
    LOG_WARN("get tablet stat meet error", K(ret));
  }
  return ret;
}

int ObBackupTabletStat::alloc_stat_(ObBackupTabletCtx *&stat)
{
  int ret = OB_SUCCESS;
  stat = NULL;
  ObBackupTabletCtx *tmp_ctx = NULL;
  if (OB_ISNULL(tmp_ctx = ObLSBackupFactory::get_backup_tablet_ctx())) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("failed to allocate ctx", K(ret));
  } else {
    stat = tmp_ctx;
  }
  return ret;
}

void ObBackupTabletStat::free_stat_(ObBackupTabletCtx *&stat)
{
  if (OB_NOT_NULL(stat)) {
    ObLSBackupFactory::free(stat);
  }
}

void ObBackupTabletStat::report_event_(const common::ObTabletID &tablet_id, const ObBackupTabletCtx &tablet_ctx)
{
  const char *backup_event = NULL;
  if (backup_data_type_.is_sys_backup()) {
    backup_event = "backup_sys_tablet";
  } else if (backup_data_type_.is_minor_backup()) {
    backup_event = "backup_minor_tablet";
  } else if (backup_data_type_.is_major_backup()) {
    backup_event = "backup_major_tablet";
  }
  SERVER_EVENT_ADD("backup",
      backup_event,
      "tenant_id",
      tenant_id_,
      "backup_set_id",
      backup_set_id_,
      "ls_id",
      ls_id_.id(),
      "tablet_id",
      tablet_id.id(),
      "finished_macro_block_count",
      tablet_ctx.finish_macro_block_count_,
      "reused_macro_block_count",
      tablet_ctx.reused_macro_block_count_,
      tablet_ctx.total_macro_block_count_);
}

int ObBackupTabletStat::PrintTabletStatOp::operator()(
    common::hash::HashMapPair<common::ObTabletID, ObBackupTabletCtx *> &entry)
{
  int ret = OB_SUCCESS;
  const common::ObTabletID &tablet_id = entry.first;
  const ObBackupTabletCtx *ctx = entry.second;
  LOG_INFO("backup tablet stat entry", K(tablet_id), KPC(ctx));
  return ret;
}

/* ObBackupTabletHolder */

ObBackupTabletHolder::ObBackupTabletHolder() : is_inited_(false), ls_id_(), holder_map_()
{}

ObBackupTabletHolder::~ObBackupTabletHolder()
{}

int ObBackupTabletHolder::init(const share::ObLSID &ls_id)
{
  int ret = OB_SUCCESS;
  const int64_t MAX_BUCKET_NUM = 1024;
  if (IS_INIT) {
    ret = OB_INIT_TWICE;
    LOG_WARN("backup tablet holder init twice", K(ret));
  } else if (!ls_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(ls_id));
  } else if (OB_FAIL(holder_map_.create(MAX_BUCKET_NUM, ObModIds::BACKUP))) {
    LOG_WARN("failed to create tablet handle map", K(ret));
  } else {
    is_inited_ = true;
    ls_id_ = ls_id;
  }
  return ret;
}

int ObBackupTabletHolder::hold_tablet(const common::ObTabletID &tablet_id, storage::ObTabletHandle &tablet_handle)
{
  int ret = OB_SUCCESS;
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("tablet holder not init", K(ret));
  } else if (!tablet_id.is_valid() || !tablet_handle.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_id), K(tablet_handle));
  } else if (OB_FAIL(holder_map_.set_refactored(tablet_id, tablet_handle))) {
    if (OB_HASH_EXIST == ret) {
      ret = OB_SUCCESS;
      LOG_WARN("tablet handle hold before", K(tablet_id));
    } else {
      LOG_WARN("failed to set refactored", K(ret), K(tablet_id), K(tablet_handle));
    }
  }
  return ret;
}

int ObBackupTabletHolder::get_tablet(const common::ObTabletID &tablet_id, storage::ObTabletHandle &tablet_handle)
{
  int ret = OB_SUCCESS;
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("tablet holder not init", K(ret));
  } else if (!tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_id), K(tablet_handle));
  } else if (OB_FAIL(holder_map_.get_refactored(tablet_id, tablet_handle))) {
    LOG_WARN("failed to set refactored", K(ret), K(tablet_id), K(tablet_handle));
  }
  return ret;
}

int ObBackupTabletHolder::release_tablet(const common::ObTabletID &tablet_id)
{
  int ret = OB_SUCCESS;
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("tablet holder not init", K(ret));
  } else if (!tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_id));
  } else if (OB_FAIL(holder_map_.erase_refactored(tablet_id))) {
    if (OB_HASH_NOT_EXIST == ret) {
      ret = OB_SUCCESS;
      LOG_WARN("tablet handle do not exit", K(ret), K(tablet_id));
    } else {
      LOG_WARN("failed to erase refactored", K(ret), K(tablet_id));
    }
  }
  return ret;
}

bool ObBackupTabletHolder::is_empty() const
{
  return 0 == holder_map_.size();
}

void ObBackupTabletHolder::reuse()
{
  holder_map_.reuse();
}

void ObBackupTabletHolder::reset()
{
  holder_map_.reuse();
  is_inited_ = false;
}

/* ObBackupDiskChecker */

ObBackupDiskChecker::ObBackupDiskChecker() : is_inited_(false), tablet_holder_(NULL)
{}

ObBackupDiskChecker::~ObBackupDiskChecker()
{}

int ObBackupDiskChecker::init(ObBackupTabletHolder &tablet_holder)
{
  int ret = OB_SUCCESS;
  if (IS_INIT) {
    ret = OB_INIT_TWICE;
    LOG_WARN("init twice", K(ret));
  } else {
    tablet_holder_ = &tablet_holder;
    is_inited_ = true;
  }
  return ret;
}

int ObBackupDiskChecker::check_disk_space()
{
  int ret = OB_SUCCESS;
  const int64_t required_size = 0;
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("do not init", K(ret));
  } else if (OB_FAIL(THE_IO_DEVICE->check_space_full(required_size))) {
    LOG_WARN("failed to check space full", K(ret));
  }
  return ret;
}

/* ObBackupProviderItem */

ObBackupProviderItem::ObBackupProviderItem()
    : item_type_(PROVIDER_ITEM_MAX), logic_id_(),
      macro_block_id_(), table_key_(), tablet_id_(),
      nested_offset_(0), nested_size_(0)
{}

ObBackupProviderItem::~ObBackupProviderItem()
{}

int ObBackupProviderItem::set_with_fake(const ObBackupProviderItemType &item_type, const common::ObTabletID &tablet_id)
{
  int ret = OB_SUCCESS;
  if (PROVIDER_ITEM_SSTABLE_META != item_type && PROVIDER_ITEM_TABLET_META != item_type) {
    ret = OB_ERR_SYS;
    LOG_WARN("get invalid args", K(ret));
  } else if (!tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_id));
  } else {
    item_type_ = item_type;
    logic_id_ = get_fake_logic_id_();
    macro_block_id_ = get_fake_macro_id_();
    table_key_ = get_fake_table_key_();
    tablet_id_ = tablet_id;
    if (!is_valid()) {
      ret = OB_INVALID_ARGUMENT;
      LOG_WARN("provider item not valid", K(ret), KPC(this));
    }
  }
  return ret;
}

int ObBackupProviderItem::set(const ObBackupProviderItemType &item_type, const ObBackupMacroBlockId &macro_id,
     const ObITable::TableKey &table_key, const common::ObTabletID &tablet_id)
{
  int ret = OB_SUCCESS;
  if (PROVIDER_ITEM_MACRO_ID != item_type) {
    ret = OB_ERR_SYS;
    LOG_WARN("get invalid args", K(ret));
  } else if (!macro_id.is_valid() || !table_key.is_valid() || !tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(macro_id), K(table_key), K(tablet_id));
  } else {
    item_type_ = item_type;
    logic_id_ = macro_id.logic_id_;
    macro_block_id_ = macro_id.macro_block_id_;
    table_key_ = table_key;
    tablet_id_ = tablet_id;
    nested_offset_ = macro_id.nested_offset_;
    nested_size_ = macro_id.nested_size_;
    if (!is_valid()) {
      ret = OB_INVALID_ARGUMENT;
      LOG_WARN("provider item not valid", K(ret), KPC(this));
    }
  }
  return ret;
}

bool ObBackupProviderItem::operator==(const ObBackupProviderItem &other) const
{
  return item_type_ == other.item_type_ && logic_id_ == other.logic_id_ &&
         macro_block_id_ == other.macro_block_id_ && table_key_ == other.table_key_ &&
         tablet_id_ == other.tablet_id_ && nested_size_ == other.nested_size_ && nested_offset_ == other.nested_offset_;
}

bool ObBackupProviderItem::operator!=(const ObBackupProviderItem &other) const
{
  return !(*this == other);
}

ObBackupProviderItemType ObBackupProviderItem::get_item_type() const
{
  return item_type_;
}

blocksstable::ObLogicMacroBlockId ObBackupProviderItem::get_logic_id() const
{
  return logic_id_;
}

blocksstable::MacroBlockId ObBackupProviderItem::get_macro_block_id() const
{
  return macro_block_id_;
}

const ObITable::TableKey &ObBackupProviderItem::get_table_key() const
{
  return table_key_;
}

common::ObTabletID ObBackupProviderItem::get_tablet_id() const
{
  return tablet_id_;
}

int64_t ObBackupProviderItem::get_nested_offset() const
{
  return nested_offset_;
}

int64_t ObBackupProviderItem::get_nested_size() const
{
  return nested_size_;
}

int64_t ObBackupProviderItem::get_deep_copy_size() const
{
  return 0;
}

int ObBackupProviderItem::deep_copy(const ObBackupProviderItem &src, char *buf, int64_t len, int64_t &pos)
{
  int ret = OB_SUCCESS;
  UNUSEDx(buf, len, pos);
  item_type_ = src.item_type_;
  logic_id_ = src.logic_id_;
  macro_block_id_ = src.macro_block_id_;
  table_key_ = src.table_key_;
  tablet_id_ = src.tablet_id_;
  nested_offset_ = src.nested_offset_;
  nested_size_ = src.nested_size_;
  return ret;
}

bool ObBackupProviderItem::is_valid() const
{
  bool bret = false;
  if (PROVIDER_ITEM_MACRO_ID != item_type_
      && PROVIDER_ITEM_SSTABLE_META != item_type_
      && PROVIDER_ITEM_TABLET_META != item_type_) {
    bret = false;
  } else {
    bret = logic_id_.is_valid() && macro_block_id_.is_valid()
        && table_key_.is_valid() && tablet_id_.is_valid();
  }
  return bret;
}

void ObBackupProviderItem::reset()
{
  item_type_ = PROVIDER_ITEM_MAX;
  logic_id_.reset();
  macro_block_id_.reset();
  table_key_.reset();
  tablet_id_.reset();
  nested_offset_ = 0;
  nested_size_ = 0;
}

DEFINE_SERIALIZE(ObBackupProviderItem)
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(serialization::encode_vi64(buf, buf_len, pos, item_type_))) {
    LOG_WARN("failed to encode key", K(ret));
  } else if (OB_FAIL(logic_id_.serialize(buf, buf_len, pos))) {
    LOG_WARN("failed to serialize logic id", K(ret));
  } else if (OB_FAIL(macro_block_id_.serialize(buf, buf_len, pos))) {
    LOG_WARN("failed to serialize macro block id", K(ret));
  } else if (OB_FAIL(table_key_.serialize(buf, buf_len, pos))) {
    LOG_WARN("failed to serialize table key", K(ret));
  } else if (OB_FAIL(tablet_id_.serialize(buf, buf_len, pos))) {
    LOG_WARN("failed to serialize tablet id", K(ret));
  } else if (OB_FAIL(serialization::encode_vi64(buf, buf_len, pos, nested_offset_))) {
    LOG_WARN("failed to serialize nested_offset_", K(ret));
  } else if (OB_FAIL(serialization::encode_vi64(buf, buf_len, pos, nested_size_))) {
    LOG_WARN("failed to serialize nested_size_", K(ret));
  }
  return ret;
}

DEFINE_DESERIALIZE(ObBackupProviderItem)
{
  int ret = OB_SUCCESS;
  int64_t item_type_value = 0;
  if (pos < data_len && OB_FAIL(serialization::decode_vi64(buf, data_len, pos, &item_type_value))) {
    LOG_WARN("failed to decode key", K(ret), K(data_len), K(pos));
  } else if (pos < data_len && OB_FAIL(logic_id_.deserialize(buf, data_len, pos))) {
    LOG_WARN("failed to deserialize logic id", K(ret), K(data_len), K(pos));
  } else if (pos < data_len && OB_FAIL(macro_block_id_.deserialize(buf, data_len, pos))) {
    LOG_WARN("failed to deserialize macro block id", K(ret), K(data_len), K(pos));
  } else if (pos < data_len && OB_FAIL(table_key_.deserialize(buf, data_len, pos))) {
    LOG_WARN("failed to deserialize table key", K(ret), K(data_len), K(pos));
  } else if (pos < data_len && OB_FAIL(tablet_id_.deserialize(buf, data_len, pos))) {
    LOG_WARN("failed to deserialize tablet id", K(ret), K(data_len), K(pos));
  } else if (pos < data_len && OB_FAIL(serialization::decode_vi64(buf, data_len, pos, &nested_offset_))) {
    LOG_WARN("failed to deserialize nested_offset_", K(ret), K(data_len), K(pos));
  } else if (pos < data_len && OB_FAIL(serialization::decode_vi64(buf, data_len, pos, &nested_size_))) {
    LOG_WARN("failed to deserialize nested_size_", K(ret), K(data_len), K(pos));
  } else {
    item_type_ = static_cast<ObBackupProviderItemType>(item_type_value);
  }
  return ret;
}

DEFINE_GET_SERIALIZE_SIZE(ObBackupProviderItem)
{
  int64_t size = 0;
  size += serialization::encoded_length_vi64(item_type_);
  size += logic_id_.get_serialize_size();
  size += macro_block_id_.get_serialize_size();
  size += table_key_.get_serialize_size();
  size += tablet_id_.get_serialize_size();
  size += serialization::encoded_length_vi64(nested_offset_);
  size += serialization::encoded_length_vi64(nested_size_);
  return size;
}

ObITable::TableKey ObBackupProviderItem::get_fake_table_key_()
{
  ObITable::TableKey table_key;
  table_key.tablet_id_ = ObTabletID(1);
  table_key.table_type_ = ObITable::TableType::MAJOR_SSTABLE;
  table_key.version_range_.snapshot_version_ = 0;
  table_key.column_group_idx_ = 0;
  return table_key;
}

ObLogicMacroBlockId ObBackupProviderItem::get_fake_logic_id_()
{
  return ObLogicMacroBlockId(0/*data_seq*/, 1/*logic_version*/, 1/*tablet_id*/);
}

MacroBlockId ObBackupProviderItem::get_fake_macro_id_()
{
  return MacroBlockId(4096/*first_id*/, 0/*second_id*/, 0/*third_id*/);
}

/* ObBackupProviderItemCompare */

ObBackupProviderItemCompare::ObBackupProviderItemCompare(int &sort_ret) : result_code_(sort_ret), backup_data_type_()
{}

void ObBackupProviderItemCompare::set_backup_data_type(const share::ObBackupDataType &backup_data_type)
{
  backup_data_type_ = backup_data_type;
}

// the adapt of backup at sstable level instead of tablet is too costly for now
// so rewriting the sort function seem to be fine for now and will be rewrite when
// TODO(yangyi.yyy): is more available

// when backup minor, the logic_id will be sorted by table_key first, then logic_id,
// which lead to logic id of same sstable will be grouped together, and the logic id
// is sorted in the same sstable
// when backup major, the logic id will be the logic id directly
bool ObBackupProviderItemCompare::operator()(const ObBackupProviderItem *left, const ObBackupProviderItem *right)
{
  bool bret = false;
  if (OB_ISNULL(left) || OB_ISNULL(right)) {
    result_code_ = OB_INVALID_DATA;
    LOG_WARN_RET(result_code_, "provider item should not be null", K_(result_code), KP(left), KP(right));
  } else if (backup_data_type_.is_minor_backup()) {  // minor sstable is sorted by log ts range end log ts
    if (left->get_tablet_id().id() < right->get_tablet_id().id()) {
      bret = true;
    } else if (left->get_tablet_id().id() > right->get_tablet_id().id()) {
      bret = false;
    } else if (left->get_item_type() < right->get_item_type()) {
      bret = true;
    } else if (left->get_item_type() > right->get_item_type()) {
      bret = false;
    } else if (left->get_table_key().scn_range_.end_scn_ < right->get_table_key().scn_range_.end_scn_) {
      bret = true;
    } else if (left->get_table_key().scn_range_.end_scn_ > right->get_table_key().scn_range_.end_scn_) {
      bret = false;
    } else if (left->get_logic_id() < right->get_logic_id()) {
      bret = true;
    } else if (left->get_logic_id() > right->get_logic_id()) {
      bret = false;
    }
  } else {
    if (left->get_tablet_id().id() < right->get_tablet_id().id()) {
      bret = true;
    } else if (left->get_tablet_id().id() > right->get_tablet_id().id()) {
      bret = false;
    } else if (left->get_item_type() < right->get_item_type()) {
      bret = true;
    } else if (left->get_item_type() > right->get_item_type()) {
      bret = false;
    } else if (left->get_logic_id() < right->get_logic_id()) {
      bret = true;
    } else if (left->get_logic_id() > right->get_logic_id()) {
      bret = false;
    }
  }
  return bret;
}

/* ObBackupTabletProvider */

ObBackupTabletProvider::ObBackupTabletProvider()
    : is_inited_(),
      is_run_out_(false),
      meet_end_(false),
      sort_ret_(0),
      mutex_(common::ObLatchIds::BACKUP_LOCK),
      param_(),
      backup_data_type_(),
      cur_task_id_(),
      external_sort_(),
      ls_backup_ctx_(NULL),
      index_kv_cache_(NULL),
      sql_proxy_(NULL),
      backup_item_cmp_(sort_ret_),
      meta_index_store_(),
      prev_item_(),
      has_prev_item_(false)
{}

ObBackupTabletProvider::~ObBackupTabletProvider()
{
  reset();
}

int ObBackupTabletProvider::init(const ObLSBackupParam &param, const share::ObBackupDataType &backup_data_type,
    ObLSBackupCtx &ls_backup_ctx, ObBackupIndexKVCache &index_kv_cache, common::ObMySQLProxy &sql_proxy)
{
  int ret = OB_SUCCESS;
  if (IS_INIT) {
    ret = OB_INIT_TWICE;
    LOG_WARN("provider init twice", K(ret));
  } else if (!param.is_valid() || !backup_data_type.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(param), K(backup_data_type));
  } else if (FALSE_IT(backup_item_cmp_.set_backup_data_type(backup_data_type))) {
    LOG_WARN("failed to set backup data type", K(ret), K(backup_data_type));
  } else if (OB_FAIL(external_sort_.init(
                 BUF_MEM_LIMIT, FILE_BUF_SIZE, EXPIRE_TIMESTAMP, OB_SYS_TENANT_ID, &backup_item_cmp_))) {
    LOG_WARN("failed to init external sort", K(ret));
  } else if (OB_FAIL(param_.assign(param))) {
    LOG_WARN("failed to assign param", K(ret), K(param));
  } else {
    backup_data_type_ = backup_data_type;
    ls_backup_ctx_ = &ls_backup_ctx;
    index_kv_cache_ = &index_kv_cache;
    sql_proxy_ = &sql_proxy;
    cur_task_id_ = 0;
    is_run_out_ = false;
    is_inited_ = true;
  }
  return ret;
}

void ObBackupTabletProvider::reset()
{
  ObMutexGuard guard(mutex_);
  is_inited_ = true;
  sort_ret_ = OB_SUCCESS;
  external_sort_.clean_up();
  ls_backup_ctx_ = NULL;
}

void ObBackupTabletProvider::reuse()
{
  ObMutexGuard guard(mutex_);
  is_run_out_ = false;
  meet_end_ = false;
  cur_task_id_ = 0;
  if (OB_NOT_NULL(ls_backup_ctx_)) {
    ls_backup_ctx_->reuse();
  }
}

bool ObBackupTabletProvider::is_run_out()
{
  ObMutexGuard guard(mutex_);
  return is_run_out_;
}

void ObBackupTabletProvider::set_backup_data_type(const share::ObBackupDataType &backup_data_type)
{
  ObMutexGuard guard(mutex_);
  backup_data_type_ = backup_data_type;
  backup_item_cmp_.set_backup_data_type(backup_data_type);
}

ObBackupDataType ObBackupTabletProvider::get_backup_data_type() const
{
  ObMutexGuard guard(mutex_);
  return backup_data_type_;
}

int ObBackupTabletProvider::get_next_batch_items(common::ObIArray<ObBackupProviderItem> &items, int64_t &task_id)
{
  int ret = OB_SUCCESS;
  items.reset();
  ObMutexGuard guard(mutex_);
  const uint64_t tenant_id = param_.tenant_id_;
  const share::ObLSID &ls_id = param_.ls_id_;
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("backup tablet provider do not init", K(ret));
  } else if (OB_FAIL(prepare_batch_tablet_(tenant_id, ls_id))) {
    LOG_WARN("failed to prepare batch tablet", K(ret), K(tenant_id), K(ls_id));
  } else {
    ObArray<ObBackupProviderItem> tmp_items;
    int64_t batch_size = BATCH_SIZE;
#ifdef ERRSIM
    if (!ls_id.is_sys_ls()) {
      const int64_t errsim_batch_size = GCONF.errsim_tablet_batch_count;
      if (0 != errsim_batch_size) {
        batch_size = errsim_batch_size;
        LOG_INFO("get errsim batch size", K(errsim_batch_size));
      }
    }
#endif
    while (OB_SUCC(ret)) {
      tmp_items.reset();
      if (OB_FAIL(inner_get_batch_items_(batch_size, tmp_items))) {
        LOG_WARN("failed to inner get batch item", K(ret), K(batch_size));
      } else if (tmp_items.empty() && meet_end_) {
        is_run_out_ = true;
        LOG_INFO("no provider items");
        break;
      } else if (OB_FAIL(append(items, tmp_items))) {
        LOG_WARN("failed to append array", K(ret), K(tmp_items));
      } else if (OB_FAIL(remove_duplicates_(items))) {
        LOG_WARN("failed to remove duplicates", K(ret));
      } else if (items.count() >= batch_size) {
        break;
      } else if (OB_FAIL(prepare_batch_tablet_(tenant_id, ls_id))) {
        LOG_WARN("failed to prepare batch tablet", K(ret), K(tenant_id), K(ls_id));
      }
    }
    if (OB_SUCC(ret)) {
      task_id = cur_task_id_++;
    }
    LOG_INFO("get next batch items", K(ret), K_(backup_data_type), K_(param), K(items));
  }
  return ret;
}

int ObBackupTabletProvider::inner_get_batch_items_(
    const int64_t batch_size, common::ObIArray<ObBackupProviderItem> &items)
{
  int ret = OB_SUCCESS;
  items.reset();
  const ObBackupProviderItem *next_item = NULL;
  if (batch_size <= 0) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(batch_size));
  }
  while (OB_SUCC(ret) && items.count() < batch_size) {
    if (OB_FAIL(external_sort_.get_next_item(next_item))) {
      if (OB_ITER_END == ret) {
        ret = OB_SUCCESS;
        external_sort_.clean_up();
        if (OB_FAIL(external_sort_.init(
                BUF_MEM_LIMIT, FILE_BUF_SIZE, EXPIRE_TIMESTAMP, OB_SYS_TENANT_ID, &backup_item_cmp_))) {
          LOG_WARN("failed to init external sort", K(ret));
        }
        break;
      } else {
        LOG_WARN("failed to get next item", K(ret));
      }
    } else if (!next_item->is_valid()) {
      ret = OB_INVALID_DATA;
      LOG_WARN("next item is not valid", K(ret), KPC(next_item));
    } else if (OB_FAIL(items.push_back(*next_item))) {
      LOG_WARN("failed to push back", K(ret), K(next_item));
    } else if (has_prev_item_ && OB_FAIL(compare_prev_item_(*next_item))) {
      LOG_WARN("failed to compare prev item", K(ret), K(prev_item_), KPC(next_item));
    } else {
      has_prev_item_ = true;
      prev_item_ = *next_item;

    }
  }
  LOG_INFO("inner get batch item", K(items), K_(backup_data_type));
  return ret;
}

int ObBackupTabletProvider::prepare_batch_tablet_(const uint64_t tenant_id, const share::ObLSID &ls_id)
{
  int ret = OB_SUCCESS;
  int64_t total_count = 0;
  while (OB_SUCC(ret) && total_count < BATCH_SIZE) {
    ObTabletID tablet_id;
    int64_t count = 0;
    if (OB_ISNULL(ls_backup_ctx_)) {
      ret = OB_ERR_UNEXPECTED;
      LOG_WARN("log stream backup ctx should not be null", K(ret));
    } else if (OB_FAIL(ls_backup_ctx_->next(tablet_id))) {
      if (OB_ITER_END == ret) {
        meet_end_ = true;
        LOG_INFO("tablet meet end", K(ret), K(tenant_id), K(ls_id), K_(backup_data_type));
        ret = OB_SUCCESS;
        break;
      } else {
        LOG_WARN("failed to get next tablet", K(ret));
      }
    } else if (OB_FAIL(prepare_tablet_(tenant_id, ls_id, tablet_id, backup_data_type_, count))) {
      LOG_WARN("failed to prepare tablet", K(ret), K(tenant_id), K(ls_id), K(tablet_id));
    } else if (OB_FAIL(ObBackupUtils::check_ls_valid_for_backup(tenant_id, ls_id, ls_backup_ctx_->rebuild_seq_))) {
      LOG_WARN("failed to check ls valid for backup", K(ret), K(tenant_id), K(ls_id));
    } else {
      total_count += count;
    }
  }
  if (OB_SUCC(ret)) {
    if (OB_FAIL(external_sort_.do_sort(true /*final_merge*/))) {
      LOG_WARN("failed to do external sort", K(ret));
    }
  }
  return ret;
}

int ObBackupTabletProvider::prepare_tablet_(const uint64_t tenant_id, const share::ObLSID &ls_id,
    const common::ObTabletID &tablet_id, const share::ObBackupDataType &backup_data_type, int64_t &total_count)
{
  int ret = OB_SUCCESS;
  total_count = 0;
  ObArray<storage::ObITable *> sstable_array;
  ObTabletHandle tablet_handle;
  if (OB_ISNULL(ls_backup_ctx_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("ls backup ctx should not be null", K(ret));
  } else if (!tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_id));
  } else if (OB_FAIL(get_tablet_handle_(tenant_id, ls_id, tablet_id, tablet_handle))) {
    if (OB_TABLET_NOT_EXIST == ret) {
      LOG_WARN("failed to get tablet handle", K(ret), K(tenant_id), K(ls_id), K(tablet_id));
      ret = OB_SUCCESS;
      ObBackupSkippedType skipped_type(ObBackupSkippedType::MAX_TYPE);
      if (OB_FAIL(get_tablet_skipped_type_(param_.tenant_id_, ls_id, tablet_id, skipped_type))) {
        LOG_WARN("failed to get tablet skipped type", K(ret), K(param_), K(ls_id), K(tablet_id));
      } else if (OB_FAIL(report_tablet_skipped_(tablet_id, skipped_type))) {
        LOG_WARN("failed to report tablet skipped", K(ret), K(tablet_id), K_(param), K(skipped_type));
      } else {
        LOG_INFO("report tablet skipped", K(ret), K(tablet_id), K_(param), K(skipped_type));
      }
    } else {
      LOG_WARN("failed to get tablet handle", K(ret), K(tenant_id), K(ls_id), K(tablet_id));
    }
  } else if (OB_FAIL(check_tablet_continuity_(ls_id, tablet_id, tablet_handle))) {
    LOG_WARN("failed to check tablet continuity", K(ret), K(ls_id), K(tablet_id), K(tablet_handle));
  } else if (OB_FAIL(check_tablet_replica_validity_(tenant_id, ls_id, tablet_id, backup_data_type))) {
    LOG_WARN("failed to check tablet replica validity", K(ret), K(tenant_id), K(ls_id), K(tablet_id), K(backup_data_type));
  } else if (OB_FAIL(hold_tablet_handle_(tablet_id, tablet_handle))) {
    LOG_WARN("failed to hold tablet handle", K(ret), K(tablet_id), K(tablet_handle));
  } else if (OB_FAIL(fetch_tablet_sstable_array_(tablet_id, tablet_handle, backup_data_type, sstable_array))) {
    LOG_WARN("failed to fetch tablet sstable array", K(ret), K(tablet_id), K(tablet_handle), K(backup_data_type));
  } else {
    for (int64_t i = 0; OB_SUCC(ret) && i < sstable_array.count(); ++i) {
      int64_t count = 0;
      ObITable *table_ptr = sstable_array.at(i);
      if (OB_ISNULL(table_ptr)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("table should not be null", K(ret));
      } else {
        const ObITable::TableKey &table_key = table_ptr->get_key();
        ObSSTable *sstable = static_cast<ObSSTable *>(table_ptr);
        if (OB_FAIL(fetch_all_logic_macro_block_id_(tablet_id, tablet_handle, table_key, *sstable, count))) {
          LOG_WARN("failed to fetch all logic macro block id", K(ret), K(tablet_id), K(tablet_handle), K(table_key));
        } else {
          total_count += count;
        }
      }
    }
    if (OB_SUCC(ret) && !sstable_array.empty()) {
      if (OB_FAIL(add_sstable_item_(tablet_id))) {
        LOG_WARN("failed to add sstable item", K(ret), K(tablet_id));
      } else {
        total_count += 1;
      }
    }
    if (OB_SUCC(ret)) {
      if (OB_FAIL(ls_backup_ctx_->tablet_stat_.prepare_tablet_sstables(
              backup_data_type, tablet_id, tablet_handle, sstable_array))) {
        LOG_WARN("failed to prepare tablet sstable", K(ret), K(backup_data_type), K(tablet_id), K(sstable_array));
      } else if (OB_FAIL(add_tablet_item_(tablet_id))) {
        LOG_WARN("failed to add tablet item", K(ret), K(tablet_id));
      } else {
        total_count += 1;
      }
    }
  }
  LOG_INFO("prepare tablet", K(tenant_id), K(ls_id), K(tablet_id), K_(backup_data_type), K(total_count));
  return ret;
}

int ObBackupTabletProvider::get_tablet_handle_(const uint64_t tenant_id, const share::ObLSID &ls_id,
    const common::ObTabletID &tablet_id, ObTabletHandle &tablet_handle)
{
  int ret = OB_SUCCESS;
  tablet_handle.reset();
  if (OB_INVALID_ID == tenant_id || !ls_id.is_valid() || !tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tenant_id), K(ls_id), K(tablet_id));
  } else if (OB_ISNULL(ls_backup_ctx_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("ls backup ctx should not be null", K(ret));
  } else {
    const int64_t rebuild_seq = ls_backup_ctx_->rebuild_seq_;
    MTL_SWITCH(tenant_id) {
      ObLS *ls = NULL;
      ObLSHandle ls_handle;
      ObLSService *ls_svr = NULL;
      const int64_t timeout_us = ObTabletCommon::NO_CHECK_GET_TABLET_TIMEOUT_US;
      if (OB_ISNULL(ls_svr = MTL(ObLSService *))) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("MTL ObLSService is null", K(ret), K(tenant_id));
      } else if (OB_FAIL(ls_svr->get_ls(ls_id, ls_handle, ObLSGetMod::STORAGE_MOD))) {
        LOG_WARN("fail to get ls handle", K(ret), K(ls_id));
      } else if (OB_ISNULL(ls = ls_handle.get_ls())) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("LS is null", K(ret));
      } else if (OB_FAIL(ObBackupUtils::check_ls_valid_for_backup(tenant_id, ls_id, rebuild_seq))) {
        LOG_WARN("failed to check ls valid for backup", K(ret), K(tenant_id), K(ls_id), K(rebuild_seq));
      } else if (OB_FAIL(ls->get_tablet(tablet_id, tablet_handle, timeout_us))) {
        LOG_WARN("failed to get tablet handle", K(ret), K(tenant_id), K(ls_id), K(tablet_id));
      } else if (OB_FAIL(ObBackupUtils::check_ls_valid_for_backup(tenant_id, ls_id, rebuild_seq))) {
        LOG_WARN("failed to check ls valid for backup", K(ret), K(tenant_id), K(ls_id), K(rebuild_seq));
      }
    }
  }
  return ret;
}

int ObBackupTabletProvider::get_tablet_skipped_type_(const uint64_t tenant_id, const share::ObLSID &ls_id,
    const common::ObTabletID &tablet_id, ObBackupSkippedType &skipped_type)
{
  int ret = OB_SUCCESS;
  ObSqlString sql;
  int64_t tablet_count = 0;
  int64_t tmp_ls_id = 0;
  HEAP_VAR(ObMySQLProxy::ReadResult, res)
  {
    common::sqlclient::ObMySQLResult *result = NULL;
    if (OB_FAIL(sql.assign_fmt(
            "select count(*) as count, ls_id from %s where tablet_id = %ld",
            OB_ALL_TABLET_TO_LS_TNAME, tablet_id.id()))) {
      LOG_WARN("failed to assign sql", K(ret), K(tablet_id));
    } else if (OB_ISNULL(sql_proxy_)) {
      ret = OB_ERR_UNEXPECTED;
      LOG_WARN("sql proxy should not be null", K(ret));
    } else if (OB_FAIL(sql_proxy_->read(res, tenant_id, sql.ptr()))) {
      LOG_WARN("failed to execute sql", KR(ret), K(sql));
    } else if (OB_ISNULL(result = res.get_result())) {
      ret = OB_ERR_UNEXPECTED;
      LOG_WARN("result is NULL", KR(ret), K(sql));
    } else if (OB_FAIL(result->next())) {
      LOG_WARN("failed to get next result", KR(ret), K(sql));
    } else {
      EXTRACT_INT_FIELD_MYSQL(*result, "count", tablet_count, int64_t);
      EXTRACT_INT_FIELD_MYSQL_SKIP_RET(*result, "ls_id", tmp_ls_id, int64_t);
    }
  }
  if (OB_SUCC(ret)) {
    if (0 == tablet_count) {
      skipped_type = ObBackupSkippedType(ObBackupSkippedType::DELETED);
    } else if (1 == tablet_count) {
      if (tmp_ls_id == ls_id.id()) {
        storage::ObLS *ls = NULL;
        ObLSService *ls_service = NULL;
        ObLSHandle handle;
        if (OB_ISNULL(ls_service = MTL_WITH_CHECK_TENANT(ObLSService *, tenant_id))) {
          ret = OB_ERR_UNEXPECTED;
          LOG_WARN("log stream service is NULL", K(ret), K(tenant_id));
        } else if (OB_FAIL(ls_service->get_ls(ls_id, handle, ObLSGetMod::STORAGE_MOD))) {
          LOG_WARN("failed to get log stream", K(ret), K(ls_id));
        } else if (OB_ISNULL(ls = handle.get_ls())) {
          ret = OB_ERR_UNEXPECTED;
          LOG_WARN("log stream not exist", K(ret), K(ls_id));
        } else if (ls->is_stopped()) {
          ret = OB_REPLICA_CANNOT_BACKUP;
          LOG_WARN("ls has stopped, can not backup", K(ret), K(tenant_id), K(ls_id));
        } else {
          ret = OB_ERR_UNEXPECTED;
          LOG_WARN("tablet not exist, but __all_tablet_to_ls still exist",
              K(ret), K(tenant_id), K(ls_id), K(tablet_id));
        }
      } else {
        skipped_type = ObBackupSkippedType(ObBackupSkippedType::TRANSFER);
        LOG_INFO("tablet transfered, need change turn", K(ls_id));
      }
    } else {
      ret = OB_ERR_UNEXPECTED;
      LOG_WARN("tablet count should not greater than 1", K(ret), K(tablet_count));
    }
  }
  return ret;
}

int ObBackupTabletProvider::report_tablet_skipped_(const common::ObTabletID &tablet_id,
    const share::ObBackupSkippedType &skipped_type)
{
  int ret = OB_SUCCESS;
  ObBackupSkippedTablet skipped_tablet;
  skipped_tablet.task_id_ = param_.task_id_;
  skipped_tablet.tenant_id_ = param_.tenant_id_;
  skipped_tablet.turn_id_ = param_.turn_id_;
  skipped_tablet.retry_id_ = param_.retry_id_;
  skipped_tablet.tablet_id_ = tablet_id;
  skipped_tablet.backup_set_id_ = param_.backup_set_desc_.backup_set_id_;
  skipped_tablet.ls_id_ = param_.ls_id_;
  skipped_tablet.skipped_type_ = skipped_type;
  if (!tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_id));
  } else if (OB_FAIL(ObLSBackupOperator::report_tablet_skipped(param_.tenant_id_, skipped_tablet, *sql_proxy_))) {
    LOG_WARN("failed to report tablet skipped", K(ret), K_(param), K(tablet_id));
  } else {
    LOG_INFO("report tablet skipping", K(tablet_id));
  }
  return ret;
}

int ObBackupTabletProvider::hold_tablet_handle_(
    const common::ObTabletID &tablet_id, storage::ObTabletHandle &tablet_handle)
{
  int ret = OB_SUCCESS;
  if (!tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_id));
  } else if (OB_ISNULL(ls_backup_ctx_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("ls backup ctx should not be null", K(ret));
  } else if (OB_FAIL(ls_backup_ctx_->hold_tablet(tablet_id, tablet_handle))) {
    LOG_WARN("failed to hold tablet", K(ret), K(tablet_id), K(tablet_handle));
  } else {
    LOG_INFO("hold tablet handle", K(tablet_id), K(tablet_handle));
  }
  return ret;
}

int ObBackupTabletProvider::fetch_tablet_sstable_array_(const common::ObTabletID &tablet_id,
    storage::ObTabletHandle &tablet_handle, const share::ObBackupDataType &backup_data_type,
    common::ObIArray<storage::ObITable *> &sstable_array)
{
  int ret = OB_SUCCESS;
  sstable_array.reset();
  if (!tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_id));
  } else {
    ObTablet &tablet = *tablet_handle.get_obj();
    ObTabletTableStore &table_store = tablet.get_table_store();
    if (OB_FAIL(ObBackupUtils::get_sstables_by_data_type(tablet_handle, backup_data_type, table_store, sstable_array))) {
      LOG_WARN("failed to get sstables by data type", K(ret), K(tablet_handle), K(backup_data_type), K(table_store));
    } else {
      LOG_INFO("fetch tablet sstable array", K(ret), K(tablet_id), K(backup_data_type), K(sstable_array));
    }
  }
  return ret;
}

int ObBackupTabletProvider::prepare_tablet_logic_id_reader_(const common::ObTabletID &tablet_id,
    const storage::ObTabletHandle &tablet_handle, const ObITable::TableKey &table_key,
    const blocksstable::ObSSTable &sstable, ObITabletLogicMacroIdReader *&reader)
{
  int ret = OB_SUCCESS;
  ObITabletLogicMacroIdReader *tmp_reader = NULL;
  const ObTabletLogicIdReaderType type = TABLET_LOGIC_ID_READER;
  if (!tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_id));
  } else if (OB_ISNULL(tmp_reader = ObLSBackupFactory::get_tablet_logic_macro_id_reader(type))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("faild to alloc memory", K(ret));
  } else if (OB_FAIL(tmp_reader->init(tablet_id, tablet_handle, table_key, sstable, BATCH_SIZE))) {
    LOG_WARN("failed to init reader", K(ret), K(tablet_id), K(tablet_handle), K(table_key));
  } else {
    reader = tmp_reader;
  }
  return ret;
}

int ObBackupTabletProvider::fetch_all_logic_macro_block_id_(const common::ObTabletID &tablet_id,
    const storage::ObTabletHandle &tablet_handle, const ObITable::TableKey &table_key,
    const blocksstable::ObSSTable &sstable, int64_t &total_count)
{
  int ret = OB_SUCCESS;
  total_count = 0;
  ObITabletLogicMacroIdReader *macro_id_reader = NULL;
  ObArray<ObBackupMacroBlockId> id_array;
  if (!tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(tablet_id));
  } else if (OB_FAIL(prepare_tablet_logic_id_reader_(tablet_id, tablet_handle, table_key, sstable, macro_id_reader))) {
    LOG_WARN("failed to prepare tablet logic id reader", K(ret), K(tablet_id), K(tablet_handle), K(table_key));
  } else if (OB_ISNULL(macro_id_reader)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("macro id reader should not be null", K(ret));
  } else {
    while (OB_SUCC(ret)) {
      id_array.reset();
      if (OB_FAIL(macro_id_reader->get_next_batch(id_array))) {
        if (OB_ITER_END == ret) {
          ret = OB_SUCCESS;
        } else {
          LOG_WARN("failed to get next batch macro block ids", KR(ret));
        }
      }
      if (OB_SUCC(ret)) {
        if (OB_FAIL(add_macro_block_id_item_list_(tablet_id, table_key, id_array))) {
          LOG_WARN("failed to add macro block id list", K(ret), K(tablet_id), K(table_key), K(id_array));
        } else if (id_array.count() > 0) {
          total_count += id_array.count();
        } else {
          break;
        }
      }
    }
  }
  if (OB_NOT_NULL(macro_id_reader)) {
    ObLSBackupFactory::free(macro_id_reader);
  }
  return ret;
}

int ObBackupTabletProvider::add_macro_block_id_item_list_(const common::ObTabletID &tablet_id,
    const ObITable::TableKey &table_key, const common::ObIArray<ObBackupMacroBlockId> &list)
{
  int ret = OB_SUCCESS;
  for (int64_t i = 0; OB_SUCC(ret) && i < list.count(); ++i) {
    const ObBackupMacroBlockId &macro_id = list.at(i);
    ObBackupProviderItem item;
    bool need_skip = false;
    if (OB_FAIL(check_macro_block_need_skip_(macro_id.logic_id_, need_skip))) {
      LOG_WARN("failed to check macro block need skip", K(ret), K(macro_id));
    } else if (need_skip) {
      // do nothing
    } else if (OB_FAIL(item.set(PROVIDER_ITEM_MACRO_ID, macro_id, table_key, tablet_id))) {
      LOG_WARN("failed to set item", K(ret), K(macro_id), K(table_key), K(tablet_id));
    } else if (!item.is_valid()) {
      ret = OB_INVALID_DATA;
      LOG_WARN("backup item is not valid", K(ret), K(item));
    } else if (OB_FAIL(external_sort_.add_item(item))) {
      LOG_WARN("failed to add item", KR(ret), K(item));
    } else {
      LOG_INFO("add macro block id", K(tablet_id), K(table_key), K(macro_id));
    }
  }
  return ret;
}

int ObBackupTabletProvider::add_sstable_item_(const common::ObTabletID &tablet_id)
{
  int ret = OB_SUCCESS;
  ObBackupProviderItem item;
  if (OB_FAIL(item.set_with_fake(PROVIDER_ITEM_SSTABLE_META, tablet_id))) {
    LOG_WARN("failed to set item", K(ret), K(tablet_id));
  } else if (!item.is_valid()) {
    ret = OB_INVALID_DATA;
    LOG_WARN("backup item is not valid", K(ret), K(item));
  } else if (OB_FAIL(external_sort_.add_item(item))) {
    LOG_WARN("failed to add item", KR(ret), K(item));
  } else {
    LOG_INFO("add sstable item", K(tablet_id));
  }
  return ret;
}

int ObBackupTabletProvider::add_tablet_item_(const common::ObTabletID &tablet_id)
{
  int ret = OB_SUCCESS;
  ObBackupProviderItem item;
  if (OB_FAIL(item.set_with_fake(PROVIDER_ITEM_TABLET_META, tablet_id))) {
    LOG_WARN("failed to set item", K(ret), K(tablet_id));
  } else if (!item.is_valid()) {
    ret = OB_INVALID_DATA;
    LOG_WARN("backup item is not valid", K(ret), K(item));
  } else if (OB_FAIL(external_sort_.add_item(item))) {
    LOG_WARN("failed to add item", KR(ret), K(item));
  } else {
    LOG_INFO("add tablet item", K(tablet_id));
  }
  return ret;
}

int ObBackupTabletProvider::remove_duplicates_(common::ObIArray<ObBackupProviderItem> &array)
{
  int ret = OB_SUCCESS;
  const int64_t count = array.count();
  if (0 == count || 1 == count) {
    // do nothing
  } else {
    ObArray<ObBackupProviderItem> tmp_array;
    int64_t j = 0;
    for (int64_t i = 0; OB_SUCC(ret) && i < count - 1; i++) {
      if (array.at(i) != array.at(i + 1)) {
        array.at(j++) = array.at(i);
      }
    }
    array.at(j++) = array.at(count - 1);
    for (int64_t i = 0; OB_SUCC(ret) && i < j; i++) {
      if (OB_FAIL(tmp_array.push_back(array.at(i)))) {
        LOG_WARN("failed to push back", K(ret), K(i), K(j));
      }
    }
    if (OB_SUCC(ret)) {
      array.reset();
      if (OB_FAIL(array.assign(tmp_array))) {
        LOG_WARN("failed to assign", K(ret));
      }
    }
  }
  return ret;
}

int ObBackupTabletProvider::check_macro_block_need_skip_(const blocksstable::ObLogicMacroBlockId &logic_id, bool &need_skip)
{
  int ret = OB_SUCCESS;
  need_skip = false;
  if (!logic_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(logic_id));
  } else if (OB_ISNULL(ls_backup_ctx_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("ls backup ctx should not be null", K(ret));
  } else if (!ls_backup_ctx_->backup_retry_ctx_.has_need_skip_logic_id_) {
    need_skip = false;
  } else {
    // the reused pair list is sorted by logic_id
    const ObArray<ObBackupMacroBlockIDPair> &reused_pair_list = ls_backup_ctx_->backup_retry_ctx_.reused_pair_list_;
    if (OB_FAIL(inner_check_macro_block_need_skip_(logic_id, reused_pair_list, need_skip))) {
      LOG_WARN("failed to inner check macro block need skip", K(ret), K(logic_id), K(reused_pair_list));
    }
  }
  return ret;
}

int ObBackupTabletProvider::inner_check_macro_block_need_skip_(const blocksstable::ObLogicMacroBlockId &logic_id,
    const common::ObArray<ObBackupMacroBlockIDPair> &reused_pair_list, bool &need_skip)
{
  int ret = OB_SUCCESS;
  need_skip = false;
  ObBackupMacroBlockIDPair search_pair;
  search_pair.logic_id_ = logic_id;
  ObArray<ObBackupMacroBlockIDPair>::const_iterator iter = std::lower_bound(reused_pair_list.begin(),
                                                                            reused_pair_list.end(),
                                                                            search_pair);
  if (iter != reused_pair_list.end()) {
    need_skip = iter->logic_id_ == logic_id;
    LOG_INFO("logic id need skip", K(need_skip), "iter_logic_id", iter->logic_id_, K(logic_id));
  }
  return ret;
}

int ObBackupTabletProvider::check_tablet_continuity_(const share::ObLSID &ls_id, const common::ObTabletID &tablet_id,
    const storage::ObTabletHandle &tablet_handle)
{
  int ret = OB_SUCCESS;
  const ObBackupMetaType meta_type = BACKUP_TABLET_META;
  ObBackupDataType backup_data_type;
  backup_data_type.set_minor_data_backup();
  ObBackupMetaIndex tablet_meta_index;
  ObBackupTabletMeta prev_backup_tablet_meta;
  share::ObBackupPath backup_path;
  if (!backup_data_type_.is_major_backup()) {
    // do nothing
  } else if (OB_FAIL(build_meta_index_store_(backup_data_type))) {
    LOG_WARN("failed to init meta index store", K(ret));
  } else if (OB_FAIL(meta_index_store_.get_backup_meta_index(tablet_id, meta_type, tablet_meta_index))) {
    LOG_WARN("failed to get backup meta index", K(ret), K(tablet_id));
  } else if (OB_FAIL(ObBackupPathUtil::get_macro_block_backup_path(param_.backup_dest_,
      param_.backup_set_desc_, tablet_meta_index.ls_id_, backup_data_type, tablet_meta_index.turn_id_,
      tablet_meta_index.retry_id_, tablet_meta_index.file_id_, backup_path))) {
    LOG_WARN("failed to get macro block backup path", K(ret), K_(param), K(backup_data_type), K(tablet_meta_index));
  } else if (OB_FAIL(ObLSBackupRestoreUtil::read_tablet_meta(backup_path.get_obstr(),
      param_.backup_dest_.get_storage_info(), backup_data_type, tablet_meta_index, prev_backup_tablet_meta))) {
    LOG_WARN("failed to read tablet meta", K(ret), K(backup_path), K_(param));
  } else {
    const ObTabletMeta &cur_tablet_meta = tablet_handle.get_obj()->get_tablet_meta();
    const int64_t cur_snapshot_version = cur_tablet_meta.report_status_.merge_snapshot_version_;
    const int64_t prev_backup_snapshot_version = prev_backup_tablet_meta.tablet_meta_.report_status_.merge_snapshot_version_;
    if (cur_snapshot_version < prev_backup_snapshot_version) {
      ret = OB_BACKUP_MAJOR_NOT_COVER_MINOR;
      LOG_WARN("tablet is not valid", K(ret), K(cur_tablet_meta), K(prev_backup_tablet_meta));
    } else {
      LOG_DEBUG("tablet is valid", K(cur_tablet_meta), K(prev_backup_tablet_meta));
    }
  }
#ifdef ERRSIM
  if (OB_SUCC(ret)) {
    const int64_t errsim_tablet_id = GCONF.errsim_backup_tablet_id;
    if (errsim_tablet_id == tablet_id.id() && backup_data_type_.is_major_backup() && 0 == param_.retry_id_) {
      ret = OB_E(EventTable::EN_BACKUP_CHECK_TABLET_CONTINUITY_FAILED) OB_SUCCESS;
      FLOG_WARN("errsim backup check tablet continuity", K(ret), K(ls_id), K(tablet_id));
      SERVER_EVENT_SYNC_ADD("backup_errsim", "check_tablet_continuity",
                            "ls_id", ls_id.id(), "tablet_id", tablet_id.id());
    }
  }
#endif
  return ret;
}

int ObBackupTabletProvider::build_meta_index_store_(const share::ObBackupDataType &backup_data_type)
{
  int ret = OB_SUCCESS;
  ObBackupRestoreMode mode = BACKUP_MODE;
  ObBackupIndexLevel index_level = BACKUP_INDEX_LEVEL_TENANT;
  ObBackupIndexStoreParam index_store_param;
  index_store_param.index_level_ = index_level;
  index_store_param.tenant_id_ = param_.tenant_id_;
  index_store_param.backup_set_id_ = param_.backup_set_desc_.backup_set_id_;
  index_store_param.ls_id_ = param_.ls_id_;
  index_store_param.is_tenant_level_ = true;
  index_store_param.backup_data_type_ = backup_data_type;
  index_store_param.turn_id_ = param_.turn_id_;
  int64_t retry_id = 0;
  if (meta_index_store_.is_inited()) {
    // do nothing
  } else if (OB_FAIL(get_tenant_meta_index_retry_id_(backup_data_type, retry_id))) {
    LOG_WARN("failed to find meta index retry id", K(ret), K(backup_data_type));
  } else if (FALSE_IT(index_store_param.retry_id_ = retry_id)) {
    // assign
  } else if (OB_FAIL(meta_index_store_.init(mode,
                index_store_param,
                param_.backup_dest_,
                param_.backup_set_desc_,
                false/*is_sec_meta*/,
                *index_kv_cache_))) {
    LOG_WARN("failed to init macro index store", K(ret), K_(param), K_(backup_data_type));
  }
  return ret;
}

int ObBackupTabletProvider::get_tenant_meta_index_retry_id_(
    const share::ObBackupDataType &backup_data_type, int64_t &retry_id)
{
  int ret = OB_SUCCESS;
  const bool is_restore = false;
  const bool is_macro_index = false;
  const bool is_sec_meta = false;
  ObBackupTenantIndexRetryIDGetter retry_id_getter;
  if (OB_FAIL(retry_id_getter.init(param_.backup_dest_, param_.backup_set_desc_,
      backup_data_type, param_.turn_id_, is_restore, is_macro_index, is_sec_meta))) {
    LOG_WARN("failed to init retry id getter", K(ret), K_(param));
  } else if (OB_FAIL(retry_id_getter.get_max_retry_id(retry_id))) {
    LOG_WARN("failed to get max retry id", K(ret));
  }
  return ret;
}

int ObBackupTabletProvider::check_tablet_replica_validity_(const uint64_t tenant_id, const share::ObLSID &ls_id,
    const common::ObTabletID &tablet_id, const share::ObBackupDataType &backup_data_type)
{
  int ret = OB_SUCCESS;
  int64_t start_ts = ObTimeUtility::current_time();
  if (!backup_data_type.is_major_backup()) {
    // do nothing
  } else if (OB_ISNULL(sql_proxy_) || OB_ISNULL(ls_backup_ctx_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("sql proxy should not be null", K(ret), KP_(sql_proxy), KP_(ls_backup_ctx));
  } else if (OB_INVALID_ID == tenant_id || !ls_id.is_valid() || !tablet_id.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(tenant_id), K(ls_id), K(tablet_id));
  } else {
    const common::ObAddr &src_addr = GCTX.self_addr();
    if (OB_FAIL(ObStorageHAUtils::check_tablet_replica_validity(tenant_id, ls_id, src_addr, tablet_id, *sql_proxy_))) {
      LOG_WARN("failed to check tablet replica validity", K(ret), K(tenant_id), K(ls_id), K(src_addr), K(tablet_id));
    } else {
      ls_backup_ctx_->check_tablet_info_cost_time_ += ObTimeUtility::current_time() - start_ts;
    }
  }
  return ret;
}

int ObBackupTabletProvider::compare_prev_item_(const ObBackupProviderItem &cur_item)
{
  int ret = OB_SUCCESS;
  ObBackupProviderItemCompare compare(ret);
  compare.set_backup_data_type(backup_data_type_);
  bool bret = compare(&prev_item_, &cur_item); // check if smaller
  if (!bret) {
    if (prev_item_ == cur_item && PROVIDER_ITEM_MACRO_ID == cur_item.get_item_type()) {
      // macro id might be same
    } else {
      ret = OB_ERR_UNEXPECTED;
      LOG_WARN("comparing item not match", K(ret), K(prev_item_), K(cur_item));
    }
  }
  return ret;
}

/* ObBackupMacroBlockTaskMgr */

ObBackupMacroBlockTaskMgr::ObBackupMacroBlockTaskMgr()
    : is_inited_(false),
      backup_data_type_(),
      batch_size_(0),
      mutex_(common::ObLatchIds::BACKUP_LOCK),
      cond_(),
      max_task_id_(0),
      file_id_(0),
      cur_task_id_(0),
      pending_list_(),
      ready_list_()
{}

ObBackupMacroBlockTaskMgr::~ObBackupMacroBlockTaskMgr()
{
  reset();
}

int ObBackupMacroBlockTaskMgr::init(const share::ObBackupDataType &backup_data_type, const int64_t batch_size)
{
  int ret = OB_SUCCESS;
  if (IS_INIT) {
    ret = OB_INIT_TWICE;
    LOG_WARN("task mgr init twice", K(ret));
  } else if (batch_size < 0) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("get invalid args", K(ret), K(batch_size));
  } else if (OB_FAIL(cond_.init(ObWaitEventIds::IO_CONTROLLER_COND_WAIT))) {
    LOG_WARN("failed to init condition", K(ret));
  } else {
    backup_data_type_ = backup_data_type;
    batch_size_ = batch_size;
    max_task_id_ = 0;
    cur_task_id_ = 0;
    is_inited_ = true;
  }
  return ret;
}

void ObBackupMacroBlockTaskMgr::set_backup_data_type(const share::ObBackupDataType &backup_data_type)
{
  ObMutexGuard guard(mutex_);
  backup_data_type_ = backup_data_type;
}

ObBackupDataType ObBackupMacroBlockTaskMgr::get_backup_data_type() const
{
  ObMutexGuard guard(mutex_);
  return backup_data_type_;
}

int ObBackupMacroBlockTaskMgr::receive(const int64_t task_id, const common::ObIArray<ObBackupProviderItem> &id_list)
{
  int ret = OB_SUCCESS;
  if (IS_NOT_INIT) {
    ret = OB_NOT_INIT;
    LOG_WARN("mgr do not init", K(ret));
  } else if (OB_FAIL(wait_task_(task_id))) {
    LOG_WARN("failed to wait task", K(ret), K(task_id));
  } else if (OB_FAIL(put_to_pending_list_(id_list))) {
    LOG_WARN("failed to put to pending list", K(ret), K(task_id), K(id_list));
  } else if (OB_FAIL(finish_task_(task_id))) {
    LOG_WARN("failed to finish task", K(ret), K(task_id));
  } else {
    LOG_INFO("receive id list", K(task_id), K(id_list.count()));
    max_task_id_ = std::max(max_task_id_, task_id);
  }
  return ret;
}

int ObBackupMacroBlockTaskMgr::deliver(common::ObIArray<ObBackupProviderItem> &id_list, int64_t &file_id)
{
  int ret = OB_SUCCESS;
  id_list.reset();
  ObThreadCondGuard guard(cond_);
  int64_t begin_ms = ObTimeUtility::fast_current_time();
  while (OB_SUCC(ret) && id_list.empty()) {
    if (OB_FAIL(get_from_ready_list_(id_list))) {
      LOG_WARN("failed to get from ready list", K(ret));
    } else if (!id_list.empty()) {
      break;
    } else if (OB_FAIL(cond_.wait(DEFAULT_WAIT_TIME_MS))) {
      int64_t duration_ms = ObTimeUtility::fast_current_time() - begin_ms;
      if (duration_ms >= DEFAULT_WAIT_TIMEOUT_MS) {
        ret = OB_EAGAIN;
        break;
      }
      if (OB_TIMEOUT == ret) {
        LOG_WARN("waiting for task too slow", K(ret));
        ret = OB_SUCCESS;
      }
    }
  }
  if (OB_SUCC(ret) && !id_list.empty()) {
    file_id = file_id_;
    ++file_id_;
  }
  return ret;
}

int64_t ObBackupMacroBlockTaskMgr::get_pending_count() const
{
  ObMutexGuard guard(mutex_);
  return pending_list_.count();
}

int64_t ObBackupMacroBlockTaskMgr::get_ready_count() const
{
  ObMutexGuard guard(mutex_);
  return ready_list_.count();
}

bool ObBackupMacroBlockTaskMgr::has_remain() const
{
  ObMutexGuard guard(mutex_);
  return !pending_list_.empty() || !ready_list_.empty();
}

void ObBackupMacroBlockTaskMgr::reset()
{
  ObMutexGuard guard(mutex_);
  is_inited_ = false;
  cond_.destroy();
  max_task_id_ = 0;
  cur_task_id_ = 0;
  pending_list_.reset();
  ready_list_.reset();
}

void ObBackupMacroBlockTaskMgr::reuse()
{
  ObMutexGuard guard(mutex_);
  max_task_id_ = 0;
  file_id_ = 0;
  cur_task_id_ = 0;
  pending_list_.reset();
  ready_list_.reset();
}

int ObBackupMacroBlockTaskMgr::wait_task_(const int64_t task_id)
{
  int ret = OB_SUCCESS;
  while (OB_SUCC(ret) && task_id != ATOMIC_LOAD(&cur_task_id_)) {
    ObThreadCondGuard guard(cond_);
    if (OB_FAIL(cond_.wait(DEFAULT_WAIT_TIME_MS))) {
      if (OB_TIMEOUT == ret) {
        ret = OB_SUCCESS;
        LOG_WARN("waiting for task too slow", K(ret), K(task_id), K(cur_task_id_));
      }
    }
  }
  return ret;
}

int ObBackupMacroBlockTaskMgr::finish_task_(const int64_t task_idx)
{
  int ret = OB_SUCCESS;
  if (OB_UNLIKELY(task_idx != ATOMIC_LOAD(&cur_task_id_))) {
    ret = OB_ERR_UNEXPECTED;
    LOG_ERROR("finish task order unexpected", K(ret), K(task_idx), K(cur_task_id_));
  } else {
    ObThreadCondGuard guard(cond_);
    ATOMIC_INC(&cur_task_id_);
    if (OB_FAIL(cond_.broadcast())) {
      LOG_ERROR("failed to broadcast condition", K(ret));
    }
  }
  return ret;
}

int ObBackupMacroBlockTaskMgr::transfer_list_without_lock_()
{
  int ret = OB_SUCCESS;
  if (ready_list_.count() > 0) {
    LOG_INFO("no need to transfer", K(ready_list_.count()), K(pending_list_.count()), K_(batch_size));
  } else {
    ready_list_.reset();
    ObArray<ObBackupProviderItem> tmp_pending_list;
    for (int64_t i = 0; OB_SUCC(ret) && i < pending_list_.count(); ++i) {
      const ObBackupProviderItem &item = pending_list_.at(i);
      if (i < batch_size_) {
        if (OB_FAIL(ready_list_.push_back(item))) {
          LOG_WARN("failed to push back", K(ret), K(item));
        }
      } else {
        if (OB_FAIL(tmp_pending_list.push_back(item))) {
          LOG_WARN("failed to push back", K(ret), K(item));
        }
      }
    }
    if (OB_SUCC(ret)) {
      pending_list_.reset();
      if (OB_FAIL(pending_list_.assign(tmp_pending_list))) {
        LOG_WARN("failed to push back", K(ret));
      } else {
        LOG_INFO("remaining pending count", K(pending_list_.count()));
      }
    }
  }
  return ret;
}

int ObBackupMacroBlockTaskMgr::get_from_ready_list_(common::ObIArray<ObBackupProviderItem> &list)
{
  int ret = OB_SUCCESS;
  ObMutexGuard guard(mutex_);
  list.reset();
  if (ready_list_.empty()) {
    if (OB_FAIL(transfer_list_without_lock_())) {
      LOG_WARN("failed to transfer list without lock", K(ret));
    }
  }
  if (OB_SUCC(ret)) {
    if (OB_FAIL(list.assign(ready_list_))) {
      LOG_WARN("failed to assign list", K(ret));
    } else {
      ready_list_.reset();
    }
  }
  return ret;
}

int ObBackupMacroBlockTaskMgr::put_to_pending_list_(const common::ObIArray<ObBackupProviderItem> &list)
{
  int ret = OB_SUCCESS;
  ObMutexGuard guard(mutex_);
  for (int64_t i = 0; OB_SUCC(ret) && i < list.count(); ++i) {
    const ObBackupProviderItem &item = list.at(i);
    if (!item.is_valid()) {
      ret = OB_INVALID_DATA;
      LOG_WARN("backup item is not valid", K(ret), K(i), K(item), "count", list.count());
    } else if (OB_FAIL(pending_list_.push_back(item))) {
      LOG_WARN("failed to push back", K(ret), K(i), K(item));
    }
  }
  if (OB_SUCC(ret)) {
    if (pending_list_.count() >= batch_size_ && 0 == ready_list_.count()) {
      if (OB_FAIL(transfer_list_without_lock_())) {
        LOG_WARN("failed to transfer list without lock", K(ret));
      }
    }
  }
  return ret;
}

}  // namespace backup
}  // namespace oceanbase
