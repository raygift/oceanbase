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

#define USING_LOG_PREFIX SQL_ENG

#include "ob_expr_object_construct.h"
#include "observer/ob_server_struct.h"
#include "observer/ob_server.h"
#include "sql/session/ob_sql_session_info.h"
#include "sql/engine/ob_exec_context.h"
#include "pl/ob_pl.h"
#include "pl/ob_pl_user_type.h"
#include "sql/ob_spi.h"

namespace oceanbase
{
using namespace common;
namespace sql
{

OB_SERIALIZE_MEMBER((ObExprObjectConstruct, ObFuncExprOperator), rowsize_, elem_types_, udt_id_);

ObExprObjectConstruct::ObExprObjectConstruct(common::ObIAllocator &alloc)
    : ObFuncExprOperator(alloc, T_FUN_PL_OBJECT_CONSTRUCT, N_PL_OBJECT_CONSTRUCT, PARAM_NUM_UNKNOWN, NOT_ROW_DIMENSION,
                         false, INTERNAL_IN_ORACLE_MODE),
      rowsize_(0),
      udt_id_(OB_INVALID_ID),
      elem_types_(OB_MALLOC_NORMAL_BLOCK_SIZE, ModulePageAllocator(alloc)) {}

ObExprObjectConstruct::~ObExprObjectConstruct() {}

int ObExprObjectConstruct::calc_result_typeN(ObExprResType &type,
                                             ObExprResType *types,
                                             int64_t param_num,
                                             ObExprTypeCtx &type_ctx) const
{
  int ret = OB_SUCCESS;
  UNUSED (type_ctx);
  CK (param_num == elem_types_.count());
  for (int64_t i = 0; OB_SUCC(ret) && i < param_num; i++) {
    types[i].set_calc_accuracy(elem_types_.at(i).get_accuracy());
    types[i].set_calc_meta(elem_types_.at(i).get_obj_meta());
    types[i].set_calc_type(elem_types_.at(i).get_type());
  }
  OX (type.set_type(ObExtendType));
  OX (type.set_udt_id(udt_id_));
  return ret;
}

int ObExprObjectConstruct::check_types(const ObObj *objs_stack,
                                       const common::ObIArray<ObExprResType> &elem_types,
                                       int64_t param_num)
{
  int ret = OB_SUCCESS;
  CK (OB_NOT_NULL(objs_stack));
  CK (OB_LIKELY(param_num == elem_types.count()));
  for (int64_t i = 0; OB_SUCC(ret) && i < param_num; i++) {
    if (!objs_stack[i].is_null()) {
      TYPE_CHECK(objs_stack[i], elem_types.at(i).get_type());
    }
  }
  return ret;
}

int ObExprObjectConstruct::cg_expr(ObExprCGCtx &op_cg_ctx,
                                   const ObRawExpr &raw_expr,
                                   ObExpr &rt_expr) const
{
  int ret = OB_SUCCESS;
  ObIAllocator &alloc = *op_cg_ctx.allocator_;
  const ObObjectConstructRawExpr &fun_sys
                      = static_cast<const ObObjectConstructRawExpr &>(raw_expr);
  ObExprObjectConstructInfo *info
              = OB_NEWx(ObExprObjectConstructInfo, (&alloc), alloc, T_FUN_PL_OBJECT_CONSTRUCT);
  if (NULL == info) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("allocate memory failed", K(ret));
  } else {
    OZ(info->from_raw_expr(fun_sys));
    rt_expr.extra_info_ = info;
  }
  rt_expr.eval_func_ = eval_object_construct;
 
  return ret;
}

int ObExprObjectConstruct::eval_object_construct(const ObExpr &expr, ObEvalCtx &ctx, ObDatum &res)
{
  int ret = OB_SUCCESS;
  pl::ObPLRecord *record = NULL;
  const ObExprObjectConstructInfo *info
                  = static_cast<ObExprObjectConstructInfo *>(expr.extra_info_);
  ObObj result;
  ObSQLSessionInfo *session = nullptr;
  CK(OB_NOT_NULL(info));
  CK(expr.arg_cnt_ >= info->elem_types_.count());
  CK(OB_NOT_NULL(session = ctx.exec_ctx_.get_my_session()));
  ObObj *objs = nullptr;
  if (OB_FAIL(ret)) {
  } else if (OB_FAIL(expr.eval_param_value(ctx))) {
    LOG_WARN("failed to eval param ", K(ret));
  } else if (expr.arg_cnt_ > 0
     && OB_ISNULL(objs = static_cast<ObObj *>
        (ctx.exec_ctx_.get_allocator().alloc(expr.arg_cnt_ * sizeof(ObObj))))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("failed to alloc mem for objs", K(ret));
  } else if (OB_FAIL(fill_obj_stack(expr, ctx, objs))) {
    LOG_WARN("failed to convert obj", K(ret));
  } else if (expr.arg_cnt_ > 0 && OB_FAIL(check_types(objs, info->elem_types_, expr.arg_cnt_))) {
    LOG_WARN("failed to check types", K(ret));
  } else if (info->rowsize_ != pl::ObRecordType::get_init_size(expr.arg_cnt_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("rowsize_ is not equel to input", K(ret), K(info->rowsize_), K(expr.arg_cnt_));
  } else if (OB_ISNULL(record
           = static_cast<pl::ObPLRecord*>
             (ctx.exec_ctx_.get_allocator().alloc(pl::ObRecordType::get_init_size(expr.arg_cnt_))))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    LOG_WARN("failed to alloc memory", K(ret));
  } else {
    new(record)pl::ObPLRecord(info->udt_id_, expr.arg_cnt_);
    for (int64_t i = 0; i < expr.arg_cnt_; ++i) {
      record->get_element()[i] = objs[i];
      if (OB_SUCC(ret) &&
          (ObCharType == info->elem_types_.at(i).get_type() || ObNCharType == info->elem_types_.at(i).get_type())) {
        OZ (ObSPIService::spi_pad_char_or_varchar(session,
                                                  info->elem_types_.at(i).get_type(),
                                                  info->elem_types_.at(i).get_accuracy(),
                                                  &ctx.exec_ctx_.get_allocator(),
                                                  &(record->get_element()[i])));
      }
    }
    result.set_extend(reinterpret_cast<int64_t>(record),
                      pl::PL_RECORD_TYPE, pl::ObRecordType::get_init_size(expr.arg_cnt_));
    OZ(res.from_obj(result, expr.obj_datum_map_));
  }
  return ret;
}

int ObExprObjectConstruct::fill_obj_stack(const ObExpr &expr, ObEvalCtx &ctx, ObObj *objs)
{
  int ret = OB_SUCCESS;
  for (int64_t i = 0; i < expr.arg_cnt_ && OB_SUCC(ret); ++i) {
    ObDatum &param = expr.locate_param_datum(ctx, i);
    if (OB_FAIL(param.to_obj(objs[i], expr.args_[i]->obj_meta_))) {
      LOG_WARN("failed to convert obj", K(ret), K(i));
    }
  }
  return ret;
}

OB_DEF_SERIALIZE(ObExprObjectConstructInfo)
{
  int ret = OB_SUCCESS;
  LST_DO_CODE(OB_UNIS_ENCODE,
              rowsize_,
              udt_id_,
              elem_types_);
  return ret;
}

OB_DEF_DESERIALIZE(ObExprObjectConstructInfo)
{
  int ret = OB_SUCCESS;
  LST_DO_CODE(OB_UNIS_DECODE,
              rowsize_,
              udt_id_,
              elem_types_);
  return ret;
}

OB_DEF_SERIALIZE_SIZE(ObExprObjectConstructInfo)
{
  int64_t len = 0;
  LST_DO_CODE(OB_UNIS_ADD_LEN,
              rowsize_,
              udt_id_,
              elem_types_);
  return len;
}

int ObExprObjectConstructInfo::deep_copy(common::ObIAllocator &allocator,
                                         const ObExprOperatorType type,
                                         ObIExprExtraInfo *&copied_info) const
{
  int ret = common::OB_SUCCESS;
  OZ(ObExprExtraInfoFactory::alloc(allocator, type, copied_info));
  ObExprObjectConstructInfo &other = *static_cast<ObExprObjectConstructInfo *>(copied_info);
  other.rowsize_ = rowsize_;
  other.udt_id_ = udt_id_;
  OZ(other.elem_types_.assign(elem_types_));
  return ret;
}

template <typename RE>
int ObExprObjectConstructInfo::from_raw_expr(RE &raw_expr)
{
  int ret = OB_SUCCESS;
  ObObjectConstructRawExpr &pl_expr
        = const_cast<ObObjectConstructRawExpr &>
            (static_cast<const ObObjectConstructRawExpr &>(raw_expr));
  rowsize_ = pl_expr.get_rowsize();
  udt_id_ = pl_expr.get_udt_id();
  OZ(elem_types_.assign(pl_expr.get_elem_types()));
  return ret;
}

} /* sql */
} /* oceanbase */
