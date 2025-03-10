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

#ifndef _OB_TRANSFORM_TEMP_TABLE_H
#define _OB_TRANSFORM_TEMP_TABLE_H

#include "sql/rewrite/ob_transform_rule.h"
#include "sql/resolver/dml/ob_select_stmt.h"
#include "sql/rewrite/ob_transform_utils.h"
#include "ob_stmt_comparer.h"

namespace oceanbase
{
namespace sql
{

class ObTransformTempTable : public ObTransformRule
{
public:
  explicit ObTransformTempTable(ObTransformerCtx *ctx)
  : ObTransformRule(ctx, TransMethod::ROOT_ONLY, T_MATERIALIZE),
    allocator_("TempTable"),
    trans_param_(NULL) {}

  virtual ~ObTransformTempTable();

  virtual int transform_one_stmt(common::ObIArray<ObParentDMLStmt> &parent_stmts,
                                 ObDMLStmt *&stmt,
                                 bool &trans_happened) override;

  virtual int construct_transform_hint(ObDMLStmt &stmt, void *trans_params) override;

  struct TableInfo {
    TableInfo()
    :table_filters_(),
    column_ids_(),
    table_item_(NULL),
    upper_stmt_(NULL)
    {}

    virtual ~TableInfo(){}

    TO_STRING_KV(
      K_(table_filters),
      K_(column_ids),
      K_(table_item),
      K_(upper_stmt)
    );
    ObSEArray<ObRawExpr*, 8> table_filters_;
    ObSqlBitSet<> column_ids_;
    TableItem *table_item_;
    ObDMLStmt* upper_stmt_;
  };
  struct TempTableInfo {
    TempTableInfo()
    :table_infos_(),
    temp_table_query_(NULL)
    {}

    virtual ~TempTableInfo(){}

    TO_STRING_KV(
      K_(table_infos),
      K_(temp_table_query)
    );
    
    ObSEArray<TableInfo, 8> table_infos_;
    ObSelectStmt *temp_table_query_;
  };
  struct StmtClassifyHelper {
    StmtClassifyHelper()
    :stmts_(),
    table_size_(0),
    generate_table_size_(0)
    {}

    virtual ~StmtClassifyHelper(){}

    TO_STRING_KV(
      K_(stmts),
      K_(table_size),
      K_(generate_table_size)
    );

    ObSEArray<ObSelectStmt*, 8> stmts_;
    int64_t table_size_;
    int64_t generate_table_size_;
  };
  typedef ObSEArray<ObSelectStmt*, 4> MaterializeStmts;
  struct TempTableTransParam {
    TempTableTransParam()
      :trans_stmt_(NULL),
      trans_type_(T_MATERIALIZE)
    {}
    virtual ~TempTableTransParam()
    {
      for (int64_t i = 0; i < materialize_stmts_.count(); ++i) {
        if (OB_NOT_NULL(materialize_stmts_.at(i))) {
          materialize_stmts_.at(i)->~MaterializeStmts();
          materialize_stmts_.at(i) = NULL;
        }
      }
    }
    Ob2DArray<MaterializeStmts *> materialize_stmts_;
    ObSelectStmt *trans_stmt_;
    ObItemType trans_type_;
  };
  /**
   * @brief expand_temp_table
   * 如果temp table只被引用一次
   * 还原成generate table
   */
  int expand_temp_table(ObIArray<TempTableInfo> &temp_table_info,
                        bool &trans_happened);

  int inner_expand_temp_table(TempTableInfo &helper);

  int check_stmt_can_materialize(ObSelectStmt *stmt, bool &is_valid);

  int check_stmt_has_cross_product(ObSelectStmt *stmt, bool &has_cross_product);
  
  int extract_common_subquery_as_cte(ObDMLStmt *stmt,
                                     ObIArray<ObSelectStmt*> &stmts,
                                     hash::ObHashMap<uint64_t, ObDMLStmt *> &parent_map,
                                     bool &trans_happened);

  int inner_extract_common_subquery_as_cte(ObDMLStmt &root_stmt,
                                           ObIArray<ObSelectStmt*> &stmts,
                                           hash::ObHashMap<uint64_t, ObDMLStmt *> &parent_map,
                                           bool &trans_happened);

  int add_materialize_stmts(const ObIArray<ObSelectStmt*> &stms);

  int check_has_stmt(ObSelectStmt *left_stmt,
                     ObSelectStmt *right_stmt,
                     hash::ObHashMap<uint64_t, ObDMLStmt *> &parent_map,
                     bool &has_stmt);

  bool is_similar_stmt(ObSelectStmt& stmt,
                       const ObStmtMapInfo &map_info,
                       QueryRelation relation);

  int remove_simple_stmts(ObIArray<ObSelectStmt*> &stmts);

  int get_non_correlated_subquery(ObDMLStmt *stmt,
                                  const uint64_t recursive_level,
                                  hash::ObHashMap<uint64_t, uint64_t> &param_level,
                                  ObIArray<ObSelectStmt *> &non_correlated_stmts,
                                  uint64_t &min_param_level);

  int check_exec_param_level(const ObRawExpr *expr,
                             const hash::ObHashMap<uint64_t, uint64_t> &param_level,
                             uint64_t &min_param_level);

  int is_non_correlated(ObSelectStmt *stmt, bool &is_correlated);

  int classify_stmts(ObIArray<ObSelectStmt*> &stmts,
                     ObIArray<StmtClassifyHelper> &stmt_groups);

  int create_temp_table(StmtCompareHelper& compare_info);

  int compute_common_map_info(ObIArray<ObStmtMapInfo>& map_infos,
                              ObStmtMapInfo &common_map_info);

  int compute_common_map(ObIArray<int64_t> &source_map, ObIArray<int64_t> &common_map);

  int inner_create_temp_table(ObSelectStmt *stmt,
                              ObStmtMapInfo& map_info,
                              ObStmtMapInfo& common_map_info);

  int create_spj(ObSelectStmt *parent_stmt);

  int pushdown_conditions(ObSelectStmt *parent_stmt,
                        const ObIArray<int64_t> &cond_map,
                        const ObIArray<int64_t> &common_cond_map,
                        ObIArray<ObRawExpr*> &pushdown_exprs);

  int pushdown_having_conditions(ObSelectStmt *parent_stmt,
                              const ObIArray<int64_t> &having_map,
                              const ObIArray<int64_t> &common_having_map,
                              ObIArray<ObRawExpr*> &pushdown_exprs);

  int apply_temp_table(ObSelectStmt *parent_stmt,
                      TableItem *view_table,
                      ObSelectStmt *temp_table_query,
                      ObStmtMapInfo& map_info);

  int get_map_table_id(ObSelectStmt *view,
                      ObSelectStmt *temp_table_query,
                      ObStmtMapInfo& map_info,
                      const uint64_t &view_table_id,
                      uint64_t &table_id);

  int collect_temp_table_infos(ObDMLStmt *stmt,
                               ObIArray<TempTableInfo> &temp_table_infos);

  int collect_temp_table_infos(ObIArray<ObSelectStmt*> &stmts,
                               ObIArray<TempTableInfo> &temp_table_infos);

  int inner_collect_temp_table_info(TableInfo &table_info);

  int get_table_filters(ObDMLStmt *stmt, TableItem *table, ObIArray<ObRawExpr*> &table_filters);

  int get_table_filters_in_joined_table(JoinedTable *table,
                                        uint64_t table_id,
                                        const ObSqlBitSet<> &table_ids,
                                        ObIArray<ObRawExpr*> &table_filters);

  int get_candi_exprs(const ObSqlBitSet<> &table_ids,
                      const ObIArray<ObRawExpr*> &exprs,
                      ObIArray<ObRawExpr*> &candi_exprs);

  int project_pruning(ObIArray<TempTableInfo> &temp_table_info,
                      bool &trans_happened);

  int get_remove_select_item(TempTableInfo &info,
                             ObSqlBitSet<> &removed_idx);

  int remove_select_items(TempTableInfo &info,
                          ObSqlBitSet<> &removed_idxs);

  int push_down_filter(ObIArray<TempTableInfo> &temp_table_info,
                       bool &trans_happened);                      

  int inner_push_down_filter(TempTableInfo& info);

  int add_normal_temp_table_trans_hint(ObDMLStmt &stmt, ObItemType type);

  virtual int need_transform(const common::ObIArray<ObParentDMLStmt> &parent_stmts,
                             const int64_t current_level,
                             const ObDMLStmt &stmt,
                             bool &need_trans) override;

  int check_hint_allowed_trans(const ObSelectStmt &subquery,
                               bool &force_inline,
                               bool &force_materialize) const;

  int check_hint_allowed_trans(const ObSelectStmt &ref_query,
                               const ObItemType check_hint_type,
                               bool &allowed) const;

  int get_hint_force_set(const ObDMLStmt &stmt,
                         const ObSelectStmt &subquery,
                         QbNameList &qb_names,
                         bool &hint_force_no_trans);

  int sort_materialize_stmts(Ob2DArray<MaterializeStmts *> &materialize_stmts);

  int pushdown_shared_subqueries(ObSelectStmt *parent_stmt, ObIArray<ObRawExpr*> &candi_exprs);

private:
  ObArenaAllocator allocator_;
  TempTableTransParam *trans_param_;
};
}//namespace sql
}//namespace oceanbase

#endif  //_OB_TRANSFORM_TEMP_TABLE_H
