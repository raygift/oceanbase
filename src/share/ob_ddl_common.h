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

#ifndef OCEANBASE_SHARE_OB_DDL_COMMON_H
#define OCEANBASE_SHARE_OB_DDL_COMMON_H

#include "lib/allocator/page_arena.h"
#include "share/schema/ob_table_schema.h"
#include "share/schema/ob_schema_service.h"
#include "share/location_cache/ob_location_struct.h"
#include "storage/tablet/ob_tablet_common.h"

namespace oceanbase
{
namespace obrpc
{
class ObSrvRpcProxy;
class ObAlterTableArg;
}
namespace sql
{
class ObPhysicalPlan;
class ObOpSpec;
}
namespace storage
{
class ObTabletHandle;
class ObLSHandle;
}
namespace share
{
class ObLocationService;
enum ObDDLType
{
  DDL_INVALID = 0,

  ///< @note add new normal long running ddl type before this line  
  DDL_CHECK_CONSTRAINT = 1,
  DDL_FOREIGN_KEY_CONSTRAINT = 2,
  DDL_ADD_NOT_NULL_COLUMN = 3,
  DDL_MODIFY_AUTO_INCREMENT = 4,
  DDL_CREATE_INDEX = 5,
  DDL_DROP_INDEX = 6,
  ///< @note Drop schema, and refuse concurrent trans.  
  DDL_DROP_SCHEMA_AVOID_CONCURRENT_TRANS = 500,
  DDL_DROP_DATABASE = 501,
  DDL_DROP_TABLE = 502,
  DDL_TRUNCATE_TABLE = 503,
  DDL_DROP_PARTITION = 504,
  DDL_DROP_SUB_PARTITION = 505,
  DDL_TRUNCATE_PARTITION = 506,
  DDL_TRUNCATE_SUB_PARTITION = 507,

  ///< @note add new double table long running ddl type before this line  
  DDL_DOUBLE_TABLE_OFFLINE = 1000,
  DDL_MODIFY_COLUMN = 1001, // only modify columns
  DDL_ADD_PRIMARY_KEY = 1002,
  DDL_DROP_PRIMARY_KEY = 1003,
  DDL_ALTER_PRIMARY_KEY = 1004,
  DDL_ALTER_PARTITION_BY = 1005,
  DDL_DROP_COLUMN = 1006, // only drop columns
  DDL_CONVERT_TO_CHARACTER = 1007,
  DDL_ADD_COLUMN_OFFLINE = 1008, // only add columns
  DDL_COLUMN_REDEFINITION = 1009, // only add/drop columns
  DDL_TABLE_REDEFINITION = 1010,
  DDL_DIRECT_LOAD = 1011,


  // @note new normal ddl type to be defined here !!!
  DDL_NORMAL_TYPE = 10001,
  DDL_ADD_COLUMN_ONLINE = 10002, // only add trailing columns
  DDL_CHANGE_COLUMN_NAME = 10003,
  ///< @note add new normal ddl type before this line
  DDL_MAX
};

enum ObDDLTaskType
{
  INVALID_TASK = 0,
  REBUILD_INDEX_TASK = 1,
  REBUILD_CONSTRAINT_TASK = 2,
  REBUILD_FOREIGN_KEY_TASK = 3,
  MAKE_DDL_TAKE_EFFECT_TASK = 4,
  CLEANUP_GARBAGE_TASK = 5,
  MODIFY_FOREIGN_KEY_STATE_TASK = 6,
  // used in rollback_failed_add_not_null_columns() in ob_constraint_task.cpp.
  DELETE_COLUMN_FROM_SCHEMA = 7,
  // remap all index tables to hidden table and take effect through one rpc, applied in drop column for 4.0.
  REMAP_INDEXES_AND_TAKE_EFFECT_TASK = 8,
  UPDATE_AUTOINC_SCHEMA = 9,
  CANCEL_DDL_TASK = 10,
  MODIFY_NOT_NULL_COLUMN_STATE_TASK = 11,
};

enum ObDDLTaskStatus {
  PREPARE = 0,
  LOCK_TABLE = 1,
  WAIT_TRANS_END = 2,
  REDEFINITION = 3,
  VALIDATE_CHECKSUM = 4,
  COPY_TABLE_DEPENDENT_OBJECTS = 5,
  TAKE_EFFECT = 6,
  CHECK_CONSTRAINT_VALID = 7,
  SET_CONSTRAINT_VALIDATE = 8,
  MODIFY_AUTOINC = 9,
  SET_WRITE_ONLY = 10, // disused, just for compatibility.
  WAIT_TRANS_END_FOR_WRITE_ONLY = 11,
  SET_UNUSABLE = 12,
  WAIT_TRANS_END_FOR_UNUSABLE = 13,
  DROP_SCHEMA = 14,
  CHECK_TABLE_EMPTY = 15,
  WAIT_CHILD_TASK_FINISH = 16,
  REPENDING = 17,
  FAIL = 99,
  SUCCESS = 100
};

static inline bool is_simple_table_long_running_ddl(const ObDDLType type)
{
  return type > DDL_INVALID && type < DDL_DROP_SCHEMA_AVOID_CONCURRENT_TRANS;
}

static inline bool is_drop_schema_block_concurrent_trans(const ObDDLType type)
{
  return type > DDL_DROP_SCHEMA_AVOID_CONCURRENT_TRANS && type < DDL_DOUBLE_TABLE_OFFLINE;
}

static inline bool is_double_table_long_running_ddl(const ObDDLType type)
{
  return type > DDL_DOUBLE_TABLE_OFFLINE && type < DDL_NORMAL_TYPE;
}

static inline bool is_long_running_ddl(const ObDDLType type)
{
  return is_simple_table_long_running_ddl(type) || is_double_table_long_running_ddl(type);
}

static inline bool is_invalid_ddl_type(const ObDDLType type)
{
  return DDL_INVALID == type;
}

struct ObColumnNameInfo final
{
public:
  ObColumnNameInfo()
    : column_name_(), is_shadow_column_(false)
  {}
  ObColumnNameInfo(const ObString &column_name, const bool is_shadow_column)
    : column_name_(column_name), is_shadow_column_(is_shadow_column)
  {}
  ~ObColumnNameInfo() = default;
  TO_STRING_KV(K_(column_name), K_(is_shadow_column));
public:
  ObString column_name_;
  bool is_shadow_column_;
};

class ObColumnNameMap final {
public:
  ObColumnNameMap() {}
  ~ObColumnNameMap() {}
  int init(const schema::ObTableSchema &orig_table_schema,
           const schema::AlterTableSchema &alter_table_arg);
  int assign(const ObColumnNameMap &other);
  int set(const ObString &orig_column_name, const ObString &new_column_name);
  int get(const ObString &orig_column_name, ObString &new_column_name) const;
  int get_orig_column_name(const ObString &new_column_name, ObString &orig_column_name) const;
  int get_changed_names(ObIArray<std::pair<ObString, ObString>> &changed_names) const;
  DECLARE_TO_STRING;

private:
  ObArenaAllocator allocator_;
  lib::Worker::CompatMode compat_mode_;
  common::hash::ObHashMap<schema::ObColumnNameHashWrapper, ObString> col_name_map_;

  DISALLOW_COPY_AND_ASSIGN(ObColumnNameMap);
};

class ObDDLUtil
{
public:
  struct ObReplicaKey final
  {
  public:
    ObReplicaKey(): partition_id_(common::OB_INVALID_ID), addr_()
    {}
    ObReplicaKey(const int64_t partition_id, common::ObAddr addr): partition_id_(partition_id), addr_(addr)
    {}
    ~ObReplicaKey() = default;
    uint64_t hash() const
    {
      uint64_t hash_val = addr_.hash();
      hash_val = murmurhash(&partition_id_, sizeof(partition_id_), hash_val);
      return hash_val;
    }
    bool operator ==(const ObReplicaKey &other) const
    {
      return partition_id_ == other.partition_id_ && addr_ == other.addr_;
    }

    TO_STRING_KV(K_(partition_id), K_(addr));
  public:
    int64_t partition_id_;
    common::ObAddr addr_;
  };

  // get all tablets of a table by table_id
  static int get_tablets(
      const uint64_t tenant_id,
      const int64_t table_id,
      common::ObIArray<common::ObTabletID> &tablet_ids);

  static int get_tablet_count(const uint64_t tenant_id,
                              const int64_t table_id,
                              int64_t &tablet_count);

  // get all tablets of a table by table_schema
  static int get_tablets(
      const share::schema::ObTableSchema &table_schema,
      common::ObIArray<common::ObTabletID> &tablet_ids);

  // check if the major sstable of a table are exist in all needed replicas
  static int check_major_sstable_complete(
      const uint64_t data_table_id,
      const uint64_t index_table_id,
      const int64_t snapshot_version,
      bool &is_complete);

  static int generate_spatial_index_column_names(const share::schema::ObTableSchema &dest_table_schema,
                                                 const share::schema::ObTableSchema &source_table_schema,
                                                 ObArray<ObColumnNameInfo> &insert_column_names,
                                                 ObArray<ObColumnNameInfo> &column_names,
                                                 ObArray<int64_t> &select_column_ids);

  static int generate_build_replica_sql(
      const uint64_t tenant_id,
      const int64_t data_table_id,
      const int64_t dest_table_id,
      const int64_t schema_version,
      const int64_t snapshot_version,
      const int64_t execution_id,
      const int64_t task_id,
      const int64_t parallelism,
      const bool use_heap_table_ddl_plan,
      const bool use_schema_version_hint_for_src_table,
      const ObColumnNameMap *col_name_map,
      ObSqlString &sql_string);

  static int get_tablet_leader_addr(
      share::ObLocationService *location_service,
      const uint64_t tenant_id,
      const common::ObTabletID &tablet_id,
      const int64_t timeout,
      share::ObLSID &ls_id,
      common::ObAddr &leader_addr);

  static int refresh_alter_table_arg(
      const uint64_t tenant_id,
      const int64_t orig_table_id,
      obrpc::ObAlterTableArg &alter_table_arg);

  static int generate_ddl_schema_hint_str(
      const ObString &table_name,
      const int64_t schema_version,
      const bool is_oracle_mode,
      ObSqlString &sql_string);

  static int ddl_get_tablet(
      storage::ObLSHandle &ls_handle,
      const ObTabletID &tablet_id,
      storage::ObTabletHandle &tablet_handle,
      const int64_t timeout_us = storage::ObTabletCommon::DEFAULT_GET_TABLET_TIMEOUT_US);

  static int clear_ddl_checksum(sql::ObPhysicalPlan *phy_plan);
  
  static bool is_table_lock_retry_ret_code(int ret)
  {
    return OB_TRY_LOCK_ROW_CONFLICT == ret || OB_NOT_MASTER == ret || OB_TIMEOUT == ret
           || OB_EAGAIN == ret || OB_LS_LOCATION_LEADER_NOT_EXIST == ret;
  }
  static bool need_remote_write(const int ret_code);

  static int check_can_convert_character(const ObObjMeta &obj_meta)
  {
    return (obj_meta.is_string_type() || obj_meta.is_enum_or_set())
              && CS_TYPE_BINARY != obj_meta.get_collation_type();
  }

  static int get_sys_ls_leader_addr(
    const uint64_t cluster_id,
    const uint64_t tenant_id,
    common::ObAddr &leader_addr);

  static int get_tablet_paxos_member_list(
    const uint64_t tenant_id,
    const common::ObTabletID &tablet_id,
    common::ObIArray<common::ObAddr> &paxos_server_list,
    int64_t &paxos_member_count);

  static int get_tablet_replica_location(
    const uint64_t tenant_id,
    const common::ObTabletID &tablet_id,
    ObLSLocation &location);

  static int check_table_exist(
     const uint64_t tenant_id,
     const uint64_t table_id,
     share::schema::ObSchemaGetterGuard &schema_guard);
  static int get_ddl_rpc_timeout(const int64_t tablet_count, int64_t &ddl_rpc_timeout_us);
  static int get_ddl_rpc_timeout(const int64_t tenant_id, const int64_t table_id, int64_t &ddl_rpc_timeout_us);
  static int get_ddl_tx_timeout(const int64_t tablet_count, int64_t &ddl_tx_timeout_us);
  static int get_ddl_tx_timeout(const int64_t tenant_id, const int64_t table_id, int64_t &ddl_tx_timeout_us);
  static int64_t get_default_ddl_rpc_timeout();
  static int64_t get_default_ddl_tx_timeout();

  static int get_ddl_cluster_version(
     const uint64_t tenant_id,
     const uint64_t task_id,
     int64_t &ddl_cluster_version);

private:
  static int generate_column_name_str(
    const common::ObIArray<ObColumnNameInfo> &column_names,
    const bool is_oracle_mode,
    const bool with_origin_name,
    const bool with_alias_name,
    const bool use_heap_table_ddl_plan,
    ObSqlString &column_name_str);
  static int generate_column_name_str(
      const ObColumnNameInfo &column_name_info,
      const bool is_oracle_mode,
      const bool with_origin_name,
      const bool with_alias_name,
      const bool with_comma,
      ObSqlString &sql_string);
  static int generate_order_by_str(
      const ObIArray<int64_t> &select_column_ids,
      const ObIArray<int64_t> &order_column_ids,
      ObSqlString &sql_string);
  static int find_table_scan_table_id(
      const sql::ObOpSpec *spec,
      uint64_t &table_id);
};


class ObCheckTabletDataComplementOp
{
public:

  static int check_and_wait_old_complement_task(
      const uint64_t tenant_id,
      const uint64_t index_table_id,
      const uint64_t ddl_task_id,
      const int64_t execution_id,
      const common::ObAddr &inner_sql_exec_addr,
      const common::ObCurTraceId::TraceId &trace_id,
      const int64_t schema_version,
      const int64_t scn,
      bool &need_exec_new_inner_sql);

private:

  static int check_all_tablet_sstable_status(
      const uint64_t tenant_id,
      const uint64_t index_table_id,
      const int64_t snapshot_version,
      const int64_t execution_id,
      const uint64_t ddl_task_id,
      bool &is_all_sstable_build_finished);

  static int check_task_inner_sql_session_status(
      const common::ObAddr &inner_sql_exec_addr,
      const common::ObCurTraceId::TraceId &trace_id,
      const uint64_t tenant_id,
      const int64_t execution_id,
      const int64_t scn,
      bool &is_old_task_session_exist);

  static int do_check_tablets_merge_status(
      const uint64_t tenant_id,
      const int64_t snapshot_version,
      const ObIArray<ObTabletID> &tablet_ids,
      const ObLSID &ls_id,
      hash::ObHashMap<ObAddr, ObArray<ObTabletID>> &ip_tablets_map,
      hash::ObHashMap<ObTabletID, int32_t> &tablets_commited_map,
      int64_t &tablet_commit_count);

  static int check_tablet_merge_status(
      const uint64_t tenant_id,
      const ObIArray<common::ObTabletID> &tablet_ids,
      const int64_t snapshot_version,
      bool &is_all_tablets_commited);

  static int update_replica_merge_status(
      const ObTabletID &tablet_id,
      const int merge_status,
      hash::ObHashMap<ObTabletID, int32_t> &tablets_commited_map);


  static int calculate_build_finish(
      const uint64_t tenant_id,
      const common::ObIArray<common::ObTabletID> &tablet_ids,
      hash::ObHashMap<ObTabletID, int32_t> &tablets_commited_map,
      int64_t &commit_succ_count);

  static int construct_ls_tablet_map(
      const uint64_t tenant_id,
      const common::ObTabletID &tablet_id,
      hash::ObHashMap<ObLSID, ObArray<ObTabletID>> &ls_tablets_map);

  static int construct_tablet_ip_map(
      const uint64_t tenant_id,
      const ObTabletID &tablet_id,
      hash::ObHashMap<ObAddr, ObArray<ObTabletID>> &ip_tablets_map);

  static int check_tablet_checksum_update_status(
      const uint64_t tenant_id,
      const uint64_t index_table_id,
      const uint64_t ddl_task_id,
      const int64_t execution_id,
      ObIArray<ObTabletID> &tablet_ids,
      bool &tablet_checksum_status);

};



}  // end namespace share
}  // end namespace oceanbase

#endif  // OCEANBASE_SHARE_OB_DDL_COMMON_H

