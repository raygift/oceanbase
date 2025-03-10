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

#define USING_LOG_PREFIX SHARE_SCHEMA
#include "ob_inner_table_schema.h"

namespace oceanbase
{
namespace share
{
VTMapping vt_mappings[5000];
bool vt_mapping_init()
{
   int64_t start_idx = common::OB_MAX_MYSQL_VIRTUAL_TABLE_ID + 1;
   {
   int64_t idx = OB_ALL_VIRTUAL_AUTO_INCREMENT_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_AUTO_INCREMENT_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_COLL_TYPE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_COLL_TYPE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_COLUMN_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_COLUMN_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_COLUMN_STAT_HISTORY_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_COLUMN_STAT_HISTORY_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_COLUMN_STAT_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_COLUMN_STAT_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_COLUMN_USAGE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_COLUMN_USAGE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_CONSTRAINT_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_CONSTRAINT_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_CONTEXT_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_CONTEXT_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_DAM_CLEANUP_JOBS_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_DAM_CLEANUP_JOBS_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_DAM_LAST_ARCH_TS_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_DAM_LAST_ARCH_TS_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_DATABASE_PRIVILEGE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_DATABASE_PRIVILEGE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_DATABASE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_DATABASE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_DBLINK_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_DBLINK_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_DEF_SUB_PART_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_DEF_SUB_PART_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_FOREIGN_KEY_COLUMN_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_FOREIGN_KEY_COLUMN_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_FOREIGN_KEY_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_FOREIGN_KEY_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_FREEZE_INFO_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_FREEZE_INFO_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_HISTOGRAM_STAT_HISTORY_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_HISTOGRAM_STAT_HISTORY_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_HISTOGRAM_STAT_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_HISTOGRAM_STAT_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_JOB_LOG_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_JOB_LOG_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_JOB_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_JOB_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_MONITOR_MODIFIED_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_MONITOR_MODIFIED_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_OPTSTAT_GLOBAL_PREFS_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_OPTSTAT_GLOBAL_PREFS_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_OPTSTAT_USER_PREFS_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_OPTSTAT_USER_PREFS_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_OUTLINE_HISTORY_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_OUTLINE_HISTORY_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_OUTLINE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_OUTLINE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_PACKAGE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_PACKAGE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_PART_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_PART_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_PLAN_BASELINE_ITEM_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_PLAN_BASELINE_ITEM_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_PLAN_BASELINE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_PLAN_BASELINE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_RECYCLEBIN_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_RECYCLEBIN_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_RES_MGR_CONSUMER_GROUP_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_RES_MGR_CONSUMER_GROUP_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_RES_MGR_DIRECTIVE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_RES_MGR_DIRECTIVE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_RES_MGR_MAPPING_RULE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_RES_MGR_MAPPING_RULE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_RES_MGR_PLAN_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_RES_MGR_PLAN_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_RLS_ATTRIBUTE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_RLS_ATTRIBUTE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_RLS_CONTEXT_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_RLS_CONTEXT_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_RLS_GROUP_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_RLS_GROUP_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_RLS_POLICY_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_RLS_POLICY_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_RLS_SECURITY_COLUMN_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_RLS_SECURITY_COLUMN_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_ROUTINE_PARAM_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_ROUTINE_PARAM_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_ROUTINE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_ROUTINE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_SEQUENCE_OBJECT_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_SEQUENCE_OBJECT_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_SEQUENCE_VALUE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_SEQUENCE_VALUE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_SPM_CONFIG_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_SPM_CONFIG_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_SUB_PART_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_SUB_PART_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_SYNONYM_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_SYNONYM_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TABLEGROUP_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TABLEGROUP_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TABLET_TO_LS_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TABLET_TO_LS_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TABLE_PRIVILEGE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TABLE_PRIVILEGE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TABLE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TABLE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TABLE_STAT_HISTORY_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TABLE_STAT_HISTORY_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TABLE_STAT_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TABLE_STAT_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_CONSTRAINT_COLUMN_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_CONSTRAINT_COLUMN_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_DEPENDENCY_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_DEPENDENCY_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_DIRECTORY_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_DIRECTORY_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_ERROR_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_ERROR_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_KEYSTORE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_KEYSTORE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_OBJAUTH_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_OBJAUTH_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_OBJECT_TYPE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_OBJECT_TYPE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_OLS_COMPONENT_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_OLS_COMPONENT_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_OLS_LABEL_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_OLS_LABEL_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_OLS_POLICY_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_OLS_POLICY_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_OLS_USER_LEVEL_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_OLS_USER_LEVEL_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_PROFILE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_PROFILE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_REWRITE_RULES_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_REWRITE_RULES_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_ROLE_GRANTEE_MAP_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_ROLE_GRANTEE_MAP_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_SCHEDULER_JOB_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_SCHEDULER_JOB_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_SCHEDULER_PROGRAM_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_SCHEDULER_PROGRAM_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_SECURITY_AUDIT_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_SECURITY_AUDIT_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_SECURITY_AUDIT_RECORD_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_SECURITY_AUDIT_RECORD_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_SYSAUTH_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_SYSAUTH_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_TABLESPACE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_TABLESPACE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_TIME_ZONE_NAME_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_TIME_ZONE_NAME_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_TIME_ZONE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_TIME_ZONE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_TIME_ZONE_TRANSITION_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_TIME_ZONE_TRANSITION_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_TIME_ZONE_TRANSITION_TYPE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_TIME_ZONE_TRANSITION_TYPE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TENANT_TRIGGER_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TENANT_TRIGGER_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TYPE_ATTR_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TYPE_ATTR_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_TYPE_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_TYPE_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   {
   int64_t idx = OB_ALL_VIRTUAL_USER_REAL_AGENT_ORA_TID - start_idx;
   VTMapping &tmp_vt_mapping = vt_mappings[idx];
   tmp_vt_mapping.mapping_tid_ = OB_ALL_USER_TID;
   tmp_vt_mapping.is_real_vt_ = true;
   }

   return true;
} // end define vt_mappings

bool inited_vt = vt_mapping_init();

} // end namespace share
} // end namespace oceanbase
