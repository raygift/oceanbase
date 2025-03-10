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
#include "share/object/ob_obj_cast.h"
#include "common/sql_mode/ob_sql_mode_utils.h"
#include "sql/session/ob_sql_session_info.h"
#include "sql/engine/expr/ob_expr_cast.h"
#include "sql/engine/expr/ob_datum_cast.h"
#include "sql/resolver/expr/ob_raw_expr_util.h"
#include "lib/geo/ob_geometry_cast.h"
#include "sql/engine/expr/ob_expr_subquery_ref.h"
#include "sql/engine/subquery/ob_subplan_filter_op.h"
#include "pl/ob_pl_user_type.h"
#include "pl/ob_pl_allocator.h"
#include "pl/ob_pl_stmt.h"
#include "pl/ob_pl_resolver.h"

// from sql_parser_base.h
#define DEFAULT_STR_LENGTH -1

namespace oceanbase
{
using namespace common;
namespace sql
{

ObExprCast::ObExprCast(ObIAllocator &alloc)
    : ObFuncExprOperator::ObFuncExprOperator(alloc, T_FUN_SYS_CAST,
                                             N_CAST,
                                             2,
                                             NOT_ROW_DIMENSION)
{
  extra_serialize_ = 0;
  disable_operand_auto_cast();
}

int ObExprCast::get_cast_inttc_len(ObExprResType &type1,
                                   ObExprResType &type2,
                                   ObExprTypeCtx &type_ctx,
                                   int32_t &res_len,
                                   int16_t &length_semantics,
                                   ObCollationType conn,
                                   ObCastMode cast_mode) const
{
  int ret = OB_SUCCESS;
  if (type1.is_literal()) { // literal
    if (ObStringTC == type1.get_type_class()) {
      res_len = type1.get_accuracy().get_length();
      length_semantics = type1.get_length_semantics();
    } else if (OB_FAIL(ObField::get_field_mb_length(type1.get_type(),
        type1.get_accuracy(), type1.get_collation_type(), res_len))) {
      LOG_WARN("failed to get filed mb length");
    }
  } else {
    res_len = CAST_STRING_DEFUALT_LENGTH[type1.get_type()];
    ObObjTypeClass tc1 = type1.get_type_class();
    int16_t scale = type1.get_accuracy().get_scale();
    if (ObDoubleTC == tc1) {
      res_len -= 1;
    } else if (ObDateTimeTC == tc1 && scale > 0) {
      res_len += scale - 1;
    } else if (OB_FAIL(get_cast_string_len(type1, type2, type_ctx, res_len, length_semantics, conn, cast_mode))) {
      LOG_WARN("fail to get cast string length", K(ret));
    } else {
      // do nothing
    }
  }
  return ret;
}


// @res_len: the result length of any type be cast to string
// for column type
// such as c1(int),  cast(c1 as binary), the result length is 11.
int ObExprCast::get_cast_string_len(ObExprResType &type1,
                                    ObExprResType &type2,
                                    ObExprTypeCtx &type_ctx,
                                    int32_t &res_len,
                                    int16_t &length_semantics,
                                    ObCollationType conn,
                                    ObCastMode cast_mode) const
{
  int ret = OB_SUCCESS;
  const ObObj &val = type1.get_param();
  if (!type1.is_literal()) { // column
    res_len = CAST_STRING_DEFUALT_LENGTH[type1.get_type()];
    int16_t prec = type1.get_accuracy().get_precision();
    int16_t scale = type1.get_accuracy().get_scale();
    switch(type1.get_type()) {
    case ObTinyIntType:
    case ObSmallIntType:
    case ObMediumIntType:
    case ObInt32Type:
    case ObIntType:
    case ObUTinyIntType:
    case ObUSmallIntType:
    case ObUMediumIntType:
    case ObUInt32Type:
    case ObUInt64Type: {
        int32_t prec = static_cast<int32_t>(type1.get_accuracy().get_precision());
        res_len = prec > res_len ? prec : res_len;
        break;
      }
    case ObNumberType:
    case ObUNumberType: {
        if (lib::is_oracle_mode()) {
          if (0 < prec) {
            if (0 < scale) {
              res_len =  prec + 2;
            } else if (0 == scale){
              res_len = prec + 1;
            } else {
              res_len = prec - scale;
            }
          }
        } else {
          if (0 < prec) {
            if (0 < scale) {
              res_len =  prec + 2;
            } else {
              res_len = prec + 1;
            }
          }
        }
        break;
      }
    case ObTimestampTZType:
    case ObTimestampLTZType:
    case ObTimestampNanoType:
    case ObDateTimeType:
    case ObTimestampType: {
        if (scale > 0) {
          res_len += scale + 1;
        }
        break;
      }
    case ObTimeType: {
        if (scale > 0) {
          res_len += scale + 1;
        }
        break;
      }
    // TODO@hanhui text share with varchar temporarily
    case ObTinyTextType:
    case ObTextType:
    case ObMediumTextType:
    case ObLongTextType:
    case ObVarcharType:
    case ObCharType:
    case ObHexStringType:
    case ObRawType:
    case ObNVarchar2Type:
    case ObNCharType:
    case ObEnumType:
    case ObSetType:
    case ObEnumInnerType:
    case ObSetInnerType:
    case ObLobType:
    case ObJsonType:
    case ObGeometryType: {
      res_len = type1.get_length();
      length_semantics = type1.get_length_semantics();
      break;
    }
    case ObBitType: {
      if (scale > 0) {
        res_len = scale;
      }
      res_len = (res_len + 7) / 8;
      break;
    }
    default: {
        break;
      }
    }
  } else if (type1.is_null()) {
    res_len = 0;//compatible with mysql;
  } else if (OB_ISNULL(type_ctx.get_session())) {
    // calc type don't set ret, just print the log. by design.
    LOG_WARN("my_session is null");
  } else { // literal
    ObArenaAllocator oballocator(ObModIds::BLOCK_ALLOC);
    ObCollationType cast_coll_type = (CS_TYPE_INVALID != type2.get_collation_type())
        ? type2.get_collation_type()
        : conn;
    const ObDataTypeCastParams dtc_params =
          ObBasicSessionInfo::create_dtc_params(type_ctx.get_session());
    ObCastCtx cast_ctx(&oballocator,
                       &dtc_params,
                       0,
                       cast_mode,
                       cast_coll_type);
    ObString val_str;
    EXPR_GET_VARCHAR_V2(val, val_str);
    //这里设置的len为字符个数
    if (OB_SUCC(ret) && NULL != val_str.ptr()) {
      int32_t len_byte = val_str.length();
      res_len = len_byte;
      length_semantics = LS_CHAR;
      if (NULL != val_str.ptr()) {
        int32_t trunc_len_byte = static_cast<int32_t>(ObCharset::strlen_byte_no_sp(cast_coll_type,
            val_str.ptr(), len_byte));
        res_len = static_cast<int32_t>(ObCharset::strlen_char(cast_coll_type,
            val_str.ptr(), trunc_len_byte));
      }
      if (type1.is_numeric_type() && !type1.is_integer_type()) {
        res_len += 1;
      }
    }
  }
  return ret;
}

// this is only for engine 3.0. old engine will get cast mode from expr_ctx.
// only for explicit cast, implicit cast's cm is setup while deduce type(in type_ctx.cast_mode_)
int ObExprCast::get_explicit_cast_cm(const ObExprResType &src_type,
                              const ObExprResType &dst_type,
                              const ObSQLSessionInfo &session,
                              const ObRawExpr &cast_raw_expr,
                              ObCastMode &cast_mode) const
{
  int ret = OB_SUCCESS;
  cast_mode = CM_NONE;
  const bool is_explicit_cast = CM_IS_EXPLICIT_CAST(cast_raw_expr.get_extra());
  const int32_t result_flag = src_type.get_result_flag();
  if (OB_FAIL(ObSQLUtils::get_default_cast_mode(is_explicit_cast, result_flag,
                                                &session, cast_mode))) {
    LOG_WARN("get_default_cast_mode failed", K(ret));
  } else {
    const ObObjTypeClass dst_tc = ob_obj_type_class(dst_type.get_type());
    const ObObjTypeClass src_tc = ob_obj_type_class(src_type.get_type());
    if (ObDateTimeTC == dst_tc || ObDateTC == dst_tc || ObTimeTC == dst_tc) {
      cast_mode |= CM_NULL_ON_WARN;
    } else if (ob_is_int_uint(src_tc, dst_tc)) {
      cast_mode |= CM_NO_RANGE_CHECK;
    }
    if (!is_oracle_mode() && CM_IS_EXPLICIT_CAST(cast_mode)) {
      // CM_STRING_INTEGER_TRUNC is only for string to int cast in mysql mode
      if (ob_is_string_type(src_type.get_type()) &&
          (ob_is_int_tc(dst_type.get_type()) || ob_is_uint_tc(dst_type.get_type()))) {
        cast_mode |= CM_STRING_INTEGER_TRUNC;
      }
      // select cast('1e500' as decimal);  -> max_val
      // select cast('-1e500' as decimal); -> min_val
      if (ob_is_string_type(src_type.get_type()) && ob_is_number_tc(dst_type.get_type())) {
        cast_mode |= CM_SET_MIN_IF_OVERFLOW;
      }
      if (!is_called_in_sql() && CM_IS_WARN_ON_FAIL(cast_raw_expr.get_extra())) {
        cast_mode |= CM_WARN_ON_FAIL;
      }
    }
  }
  return ret;
}

bool ObExprCast::check_cast_allowed(const ObObjType orig_type,
                                    const ObCollationType orig_cs_type,
                                    const ObObjType expect_type,
                                    const ObCollationType expect_cs_type,
                                    const bool is_explicit_cast) const
{
  UNUSED(expect_cs_type);
  bool res = true;
  ObObjTypeClass ori_tc = ob_obj_type_class(orig_type);
  ObObjTypeClass expect_tc = ob_obj_type_class(expect_type);
  if (is_oracle_mode() && is_explicit_cast) {
    // can't cast lob to other type except char/varchar/nchar/nvarchar2/raw. clob to raw not allowed too.
    if (ObLobTC == ori_tc || ObTextTC == ori_tc) {
      if (expect_tc == ObJsonTC) {
        /* oracle mode, json text use lob store */
      } else if (ObStringTC == expect_tc) {
        // do nothing
      } else if (ObRawTC == expect_tc) {
        res = CS_TYPE_BINARY == orig_cs_type;
      } else {
        res = false;
      }
    }
    // any type to lob type not allowed.
    if (ObLobTC == expect_tc || ObTextTC == expect_tc) {
      res = false;
    }
  }
  return res;
}

int ObExprCast::calc_result_type2(ObExprResType &type,
                                  ObExprResType &type1,
                                  ObExprResType &type2,
                                  ObExprTypeCtx &type_ctx) const
{
  int ret = OB_SUCCESS;
  ObExprResType dst_type;
  ObRawExpr *cast_raw_expr = NULL;
  const sql::ObSQLSessionInfo *session = NULL;
  bool is_explicit_cast = false;
  ObCollationLevel cs_level = CS_LEVEL_INVALID;
  if (OB_ISNULL(session = type_ctx.get_session()) ||
      OB_ISNULL(cast_raw_expr = get_raw_expr())) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("ptr is NULL", K(ret), KP(session), KP(cast_raw_expr));
  } else if (OB_UNLIKELY(NOT_ROW_DIMENSION != row_dimension_)) {
    ret = OB_ERR_INVALID_TYPE_FOR_OP;
    LOG_WARN("invalid row_dimension_", K(row_dimension_), K(ret));
  } else if (OB_FAIL(get_cast_type(type2, cast_raw_expr->get_extra(), dst_type))) {
    LOG_WARN("get cast dest type failed", K(ret));
  } else if (OB_UNLIKELY(!cast_supported(type1.get_type(), type1.get_collation_type(),
                                        dst_type.get_type(), dst_type.get_collation_type()))) {
    ret = OB_ERR_INVALID_TYPE_FOR_OP;
    LOG_WARN("transition does not support", "src", ob_obj_type_str(type1.get_type()),
               "dst", ob_obj_type_str(dst_type.get_type()));
  } else if (FALSE_IT(is_explicit_cast = CM_IS_EXPLICIT_CAST(cast_raw_expr->get_extra()))) {
  // check cast supported in cast_map but not support here.
  } else if (OB_FAIL(ObSQLUtils::get_cs_level_from_cast_mode(cast_raw_expr->get_extra(),
                                                             type1.get_collation_level(),
                                                             cs_level))) {
    LOG_WARN("failed to get collation level", K(ret));
  } else if (!check_cast_allowed(type1.get_type(), type1.get_collation_type(),
                                 dst_type.get_type(), dst_type.get_collation_type(),
                                 is_explicit_cast)) {
    ret = OB_ERR_INVALID_TYPE_FOR_OP;
    LOG_WARN("explicit cast to lob type not allowed", K(ret), K(dst_type));
  } else {
    // always cast to user requested type
    if (is_explicit_cast && !lib::is_oracle_mode() &&
        ObCharType == dst_type.get_type()) {
      // cast(x as binary(10)), in parser,binary->T_CHAR+bianry, but, result type should be varchar, so set it.
      type.set_type(ObVarcharType);
    } else if (lib::is_mysql_mode() && ObFloatType == dst_type.get_type()) {
      // Compatible with mysql. If the precision p is not specified, produces a result of type FLOAT. 
      // If p is provided and 0 <=  p <= 24, the result is of type FLOAT. If 25 <= p <= 53, 
      // the result is of type DOUBLE. If p < 0 or p > 53, an error is returned
      // however, ob use -1 as default precision, so it is a valid value
      type.set_collation_type(dst_type.get_collation_type());
      ObPrecision float_precision = dst_type.get_precision();
      ObScale float_scale = dst_type.get_scale();
      if (OB_UNLIKELY(float_scale > OB_MAX_DOUBLE_FLOAT_SCALE)) {
        ret = OB_ERR_TOO_BIG_SCALE;
        LOG_USER_ERROR(OB_ERR_TOO_BIG_SCALE, float_scale, "CAST", OB_MAX_DOUBLE_FLOAT_SCALE);
        LOG_WARN("scale of float overflow", K(ret), K(float_scale), K(float_precision));
      } else if (float_precision < -1 ||
          (SCALE_UNKNOWN_YET == float_scale && float_precision > OB_MAX_DOUBLE_FLOAT_PRECISION)) {
        ret = OB_ERR_TOO_BIG_PRECISION;
        LOG_USER_ERROR(OB_ERR_TOO_BIG_PRECISION, float_precision, "CAST", OB_MAX_DOUBLE_FLOAT_PRECISION);
      } else if (SCALE_UNKNOWN_YET == float_scale) {
        if (float_precision <= OB_MAX_FLOAT_PRECISION) {
          type.set_type(ObFloatType);
        } else {
          type.set_type(ObDoubleType);
        }
      } else {
        type.set_type(ObFloatType);
        type.set_precision(float_precision);
        type.set_scale(float_scale);
      }
    } else {
      type.set_type(dst_type.get_type());
      type.set_collation_type(dst_type.get_collation_type());
    }
    int16_t scale = dst_type.get_scale();
    if (is_explicit_cast
        && !lib::is_oracle_mode()
        && (ObTimeType == dst_type.get_type() || ObDateTimeType == dst_type.get_type())
        && scale > 6) {
      ret = OB_ERR_TOO_BIG_PRECISION;
      LOG_USER_ERROR(OB_ERR_TOO_BIG_PRECISION, scale, "CAST", OB_MAX_DATETIME_PRECISION);
    }
    if (OB_SUCC(ret)) {
      ObCompatibilityMode compatibility_mode = get_compatibility_mode();
      ObCollationType collation_connection = type_ctx.get_coll_type();
      ObCollationType collation_nation = session->get_nls_collation_nation();
      type1.set_calc_type(get_calc_cast_type(type1.get_type(), dst_type.get_type()));
      int32_t length = 0;
      if (ob_is_string_or_lob_type(dst_type.get_type()) || ob_is_raw(dst_type.get_type()) || ob_is_json(dst_type.get_type())
          || ob_is_geometry(dst_type.get_type())) {
        type.set_collation_level(cs_level);
        int32_t len = dst_type.get_length();
        int16_t length_semantics = ((dst_type.is_string_or_lob_locator_type() || dst_type.is_json())
            ? dst_type.get_length_semantics()
            : (OB_NOT_NULL(type_ctx.get_session())
                ? type_ctx.get_session()->get_actual_nls_length_semantics()
                : LS_BYTE));
        if (len > 0) { // cast(1 as char(10))
          type.set_full_length(len, length_semantics);
        } else if (OB_FAIL(get_cast_string_len(type1, dst_type, type_ctx, len, length_semantics,
                                               collation_connection,
                                               cast_raw_expr->get_extra()))) { // cast (1 as char)
          LOG_WARN("fail to get cast string length", K(ret));
        } else {
          type.set_full_length(len, length_semantics);
        }
        if (CS_TYPE_INVALID != dst_type.get_collation_type()) {
          // cast as binary
          type.set_collation_type(dst_type.get_collation_type());
        } else {
          // use collation of current session
          type.set_collation_type(ob_is_nstring_type(dst_type.get_type()) ?
                                  collation_nation : collation_connection);
        }
      } else if (ob_is_extend(dst_type.get_type())) {
        type.set_udt_id(type2.get_udt_id());
      } else {
        type.set_length(length);
        if (ObNumberTC == dst_type.get_type_class() && 0 == dst_type.get_precision()) {
          // MySql:cast (1 as decimal(0)) = cast(1 as decimal)
          // Oracle: cast(1.4 as number) = cast(1.4 as number(-1, -1))
          type.set_precision(ObAccuracy::DDL_DEFAULT_ACCURACY2[compatibility_mode][ObNumberType].get_precision());
        } else if (ObIntTC == dst_type.get_type_class() || ObUIntTC == dst_type.get_type_class()) {
          // for int or uint , the precision = len
          int32_t len = 0;
          int16_t length_semantics = LS_BYTE;//unused
          if (OB_FAIL(get_cast_inttc_len(type1, dst_type, type_ctx, len, length_semantics,
                                         collation_connection, cast_raw_expr->get_extra()))) {
            LOG_WARN("fail to get cast inttc length", K(ret));
          } else {
            len = len > OB_LITERAL_MAX_INT_LEN ? OB_LITERAL_MAX_INT_LEN : len;
            type.set_precision(static_cast<int16_t>(len));
          }
        } else if (ORACLE_MODE == compatibility_mode && ObDoubleType == dst_type.get_type()) {
          ObAccuracy acc = ObAccuracy::DDL_DEFAULT_ACCURACY2[compatibility_mode][dst_type.get_type()];
          type.set_accuracy(acc);
          type1.set_accuracy(acc);
        } else if (ObYearType == dst_type.get_type()) {
          ObAccuracy acc = ObAccuracy::DDL_DEFAULT_ACCURACY2[compatibility_mode][dst_type.get_type()];
          type.set_accuracy(acc);
          type1.set_accuracy(acc);
        } else {
          type.set_precision(dst_type.get_precision());
        }
        type.set_scale(dst_type.get_scale());
      }
    }
    CK(OB_NOT_NULL(type_ctx.get_session()));
    if (OB_SUCC(ret)) {
      // interval expr need NOT_NULL_FLAG
      // bug: https://code.aone.alibaba-inc.com/oceanbase/oceanbase/codereview/2961005
      calc_result_flag2(type, type1, type2);
    }
  }
  if (OB_SUCC(ret)) {
    ObCastMode explicit_cast_cm = CM_NONE;
    if (OB_FAIL(get_explicit_cast_cm(type1, dst_type, *session, *cast_raw_expr,
                                     explicit_cast_cm))) {
      LOG_WARN("set cast mode failed", K(ret));
    } else if (CM_IS_EXPLICIT_CAST(explicit_cast_cm)) {
      // cast_raw_expr.extra_ store explicit cast's cast mode
      cast_raw_expr->set_extra(explicit_cast_cm);
      // type_ctx.cast_mode_ sotre implicit cast's cast mode.
      // cannot use def cm, because it may change explicit cast behavior.
      // eg: select cast(18446744073709551615 as signed) -> -1
      //     because exprlicit case need CM_NO_RANGE_CHECK
      type_ctx.set_cast_mode(explicit_cast_cm & ~CM_EXPLICIT_CAST);
      if (lib::is_mysql_mode() && !ob_is_numeric_type(type.get_type()) && type1.is_double()) {
        // for double type cast non-numeric type, no need set calc accuracy to dst type.
      } else {
        type1.set_calc_accuracy(type.get_accuracy());
      }

      // in engine 3.0, let implicit cast do the real cast
      bool need_extra_cast_for_src_type = false;
      bool need_extra_cast_for_dst_type = false;

      ObRawExprUtils::need_extra_cast(type1, type, need_extra_cast_for_src_type,
                                      need_extra_cast_for_dst_type);
      if (need_extra_cast_for_src_type) {
        ObExprResType src_type_utf8;
        OZ(ObRawExprUtils::setup_extra_cast_utf8_type(type1, src_type_utf8));
        OX(type1.set_calc_meta(src_type_utf8.get_obj_meta()));
      } else if (need_extra_cast_for_dst_type) {
        ObExprResType dst_type_utf8;
        OZ(ObRawExprUtils::setup_extra_cast_utf8_type(dst_type, dst_type_utf8));
        OX(type1.set_calc_meta(dst_type_utf8.get_obj_meta()));
      } else {
        bool need_warp = false;
        if (ob_is_enumset_tc(type1.get_type())) {
          // For enum/set type, need to check whether warp to string is required.
          if (OB_FAIL(ObRawExprUtils::need_wrap_to_string(type1.get_type(), type1.get_calc_type(),
                                                          false, need_warp))) {
            LOG_WARN("need_wrap_to_string failed", K(ret), K(type1));
          }
        } else if (OB_LIKELY(need_warp)) {
          // need_warp is true, no-op and keep type1's calc_type is dst_type. It will be wrapped
          // to string in ObRawExprWrapEnumSet::visit(ObSysFunRawExpr &expr) later.
        } else {
          if (ob_is_geometry_tc(dst_type.get_type())) {
            ObCastMode cast_mode = cast_raw_expr->get_extra();
            const ObObj &param = type2.get_param();
            ParseNode parse_node;
            parse_node.value_ = param.get_int();
            ObGeoType geo_type = static_cast<ObGeoType>(parse_node.int16_values_[OB_NODE_CAST_GEO_TYPE_IDX]);
            if (OB_FAIL(ObGeoCastUtils::set_geo_type_to_cast_mode(geo_type, cast_mode))) {
              LOG_WARN("fail to set geometry type to cast mode", K(ret), K(geo_type));
            } else {
              cast_raw_expr->set_extra(cast_mode);
            }
          }

          if (OB_SUCC(ret)) {
            // need_warp is false, set calc_type to type1 itself.
            type1.set_calc_meta(type1.get_obj_meta());
          }
        }
      }
    } else {
      // no need to set cast mode, already setup while deduce type.
      //
      // implicit cast, no need to add cast again, but the ObRawExprWrapEnumSet depend on this
      // to add enum_to_str(), so we still set the calc type but skip add implicit cast in decuding.
      type1.set_calc_type(type.get_type());
      type1.set_calc_collation_type(type.get_collation_type());
      type1.set_calc_accuracy(type.get_accuracy());
    }
  }
  LOG_DEBUG("calc result type", K(type1), K(type2), K(type), K(dst_type));
  return ret;
}

int ObExprCast::get_cast_type(const ObExprResType param_type2,
                              const ObCastMode cast_mode,
                              ObExprResType &dst_type) const
{
  int ret = OB_SUCCESS;
  if (!param_type2.is_int() && !param_type2.get_param().is_int()) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("cast param type is unexpected", K(param_type2));
  } else {
    const ObObj &param = param_type2.get_param();
    ParseNode parse_node;
    parse_node.value_ = param.get_int();
    ObObjType obj_type = static_cast<ObObjType>(parse_node.int16_values_[OB_NODE_CAST_TYPE_IDX]);
    bool is_explicit_cast = CM_IS_EXPLICIT_CAST(cast_mode);
    dst_type.set_collation_type(static_cast<ObCollationType>(parse_node.int16_values_[OB_NODE_CAST_COLL_IDX]));
    dst_type.set_type(obj_type);
    int64_t maxblen = ObCharset::CharConvertFactorNum;
    if (ob_is_lob_locator(obj_type)) {
      // cast(x as char(10)) or cast(x as binary(10))
      dst_type.set_full_length(parse_node.int32_values_[OB_NODE_CAST_C_LEN_IDX], param_type2.get_accuracy().get_length_semantics());
    } else if (ob_is_string_type(obj_type)) {
      dst_type.set_full_length(parse_node.int32_values_[OB_NODE_CAST_C_LEN_IDX], param_type2.get_accuracy().get_length_semantics());
      if (lib::is_mysql_mode() && is_explicit_cast && !dst_type.is_binary() && !dst_type.is_varbinary()) {
        if (dst_type.get_length() > OB_MAX_CAST_CHAR_VARCHAR_LENGTH && dst_type.get_length() <= OB_MAX_CAST_CHAR_TEXT_LENGTH) {
          dst_type.set_type(ObTextType);
          dst_type.set_length(OB_MAX_CAST_CHAR_TEXT_LENGTH);
        } else if (dst_type.get_length() > OB_MAX_CAST_CHAR_TEXT_LENGTH && dst_type.get_length() <= OB_MAX_CAST_CHAR_MEDIUMTEXT_LENGTH) {
          dst_type.set_type(ObMediumTextType);
          dst_type.set_length(OB_MAX_CAST_CHAR_MEDIUMTEXT_LENGTH);
        } else if (dst_type.get_length() > OB_MAX_CAST_CHAR_MEDIUMTEXT_LENGTH) {
          dst_type.set_type(ObLongTextType);
          dst_type.set_length(OB_MAX_LONGTEXT_LENGTH / maxblen);
        }
      }
    } else if (ob_is_raw(obj_type)) {
      dst_type.set_length(parse_node.int32_values_[OB_NODE_CAST_C_LEN_IDX]);
    } else if (ob_is_extend(obj_type)) {
      dst_type.set_udt_id(param_type2.get_udt_id());
    } else if (lib::is_mysql_mode() && ob_is_json(obj_type)) {
      dst_type.set_collation_type(CS_TYPE_UTF8MB4_BIN);
    } else if (ob_is_geometry(obj_type)) {
      if (lib::is_mysql_mode()) {
        dst_type.set_collation_type(CS_TYPE_BINARY);
        dst_type.set_collation_level(CS_LEVEL_IMPLICIT);
      } else {
        ret = OB_NOT_SUPPORTED;
        LOG_WARN("not support cast to geometry in oracle mode", K(ret));
      }
    } else if (ob_is_interval_tc(obj_type)) {
      if (CM_IS_EXPLICIT_CAST(cast_mode) &&
          ((ObIntervalYMType != obj_type && !ObIntervalScaleUtil::scale_check(parse_node.int16_values_[OB_NODE_CAST_N_PREC_IDX])) ||
           !ObIntervalScaleUtil::scale_check(parse_node.int16_values_[OB_NODE_CAST_N_SCALE_IDX]))) {
        ret = OB_ERR_DATETIME_INTERVAL_PRECISION_OUT_OF_RANGE;
        LOG_WARN("target interval type precision out of range", K(ret), K(obj_type));
      } else if (ObIntervalYMType == obj_type) {
        // interval year to month type has no precision
        dst_type.set_scale(parse_node.int16_values_[OB_NODE_CAST_N_SCALE_IDX]);
      } else if (ob_is_interval_ds(obj_type)) {
        //scale in day seconds type is day_scale * 10 + seconds_scale.
        dst_type.set_scale(parse_node.int16_values_[OB_NODE_CAST_N_PREC_IDX] * 10 +
                                                parse_node.int16_values_[OB_NODE_CAST_N_SCALE_IDX]);
      } else {
        dst_type.set_precision(parse_node.int16_values_[OB_NODE_CAST_N_PREC_IDX]);
        dst_type.set_scale(parse_node.int16_values_[OB_NODE_CAST_N_SCALE_IDX]);
      }
    } else {
      dst_type.set_precision(parse_node.int16_values_[OB_NODE_CAST_N_PREC_IDX]);
      dst_type.set_scale(parse_node.int16_values_[OB_NODE_CAST_N_SCALE_IDX]);
    }
    LOG_DEBUG("get_cast_type", K(dst_type), K(param_type2));
  }
  return ret;
}

int ObExprCast::get_subquery_iter(const sql::ObExpr &expr,
                                  sql::ObEvalCtx &ctx,
                                  ObExpr **&subquery_row,
                                  ObEvalCtx *&subquery_ctx,
                                  ObSubQueryIterator *&iter)
{
  int ret = OB_SUCCESS;
  iter = NULL;
  subquery_row = NULL;
  subquery_ctx = NULL;
  sql::ObDatum *subquery_datum = NULL;
  const ObExprCast::CastMultisetExtraInfo *info =
    static_cast<const ObExprCast::CastMultisetExtraInfo *>(expr.extra_info_);
  if (OB_UNLIKELY(2 != expr.arg_cnt_) || OB_ISNULL(info)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("unexpected param", K(ret));
  } else if (OB_FAIL(expr.args_[0]->eval(ctx, subquery_datum))){
    LOG_WARN("expr evaluate failed", K(ret));
  } else if (OB_ISNULL(subquery_datum) || subquery_datum->is_null()) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("NULL subquery ref info returned", K(ret));
  } else if (OB_FAIL(ObExprSubQueryRef::get_subquery_iter(
                ctx, ObExprSubQueryRef::Extra::get_info(subquery_datum->get_int()), iter))) {
    LOG_WARN("get subquery iterator failed", K(ret));
  } else if (OB_ISNULL(iter)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("NULL subquery iterator", K(ret));
  } else if (OB_FAIL(iter->rewind())) {
    LOG_WARN("start iterate failed", K(ret));
  } else if (OB_ISNULL(subquery_row = &const_cast<ExprFixedArray &>(iter->get_output()).at(0))){
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("null row", K(ret));
  } else if (OB_ISNULL(subquery_ctx = &iter->get_eval_ctx())) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("null ctx", K(ret));
  } else if (OB_UNLIKELY(iter->get_output().count() != 1)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("unexpected output column count", K(ret), K(iter->get_output().count()));
  } else if (ObNullType != iter->get_output().at(0)->datum_meta_.type_
        && iter->get_output().at(0)->datum_meta_.type_ != info->elem_type_.get_obj_type()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("check type failed", K(ret), K(expr), KPC(iter->get_output().at(0)), K(info->elem_type_));
  }
  return ret;
}

int ObExprCast::construct_collection(const sql::ObExpr &expr,
                                     ObEvalCtx &ctx,
                                     sql::ObDatum &res_datum,
                                     ObExpr **subquery_row,
                                     ObEvalCtx *subquery_ctx,
                                     ObSubQueryIterator *subquery_iter)
{
  int ret = OB_SUCCESS;
  ret = OB_NOT_SUPPORTED;
  LOG_WARN("not support", K(ret));
  return ret;
}

int ObExprCast::fill_element(const sql::ObExpr &expr,
                             ObEvalCtx &ctx,
                             sql::ObDatum &res_datum,
                             pl::ObPLCollection *coll,
                             pl::ObPLINS *ns,
                             const pl::ObCollectionType *collection_type,
                             ObExpr **subquery_row,
                             ObEvalCtx *subquery_ctx,
                             ObSubQueryIterator *subquery_iter)
{
  int ret = OB_SUCCESS;
  ret = OB_NOT_SUPPORTED;
  LOG_WARN("not support", K(ret));
  return ret;
}

int ObExprCast::eval_cast_multiset(const sql::ObExpr &expr,
                                   sql::ObEvalCtx &ctx,
                                   sql::ObDatum &res_datum)
{
  int ret = OB_SUCCESS;
  ret = OB_NOT_SUPPORTED;
  LOG_USER_ERROR(OB_NOT_SUPPORTED, "eval cast multiset");
  return ret;
}

int ObExprCast::cg_cast_multiset(ObExprCGCtx &op_cg_ctx,
                                 const ObRawExpr &raw_expr,
                                 ObExpr &rt_expr) const
{
  int ret = OB_SUCCESS;
  ret = OB_NOT_SUPPORTED;
  LOG_USER_ERROR(OB_NOT_SUPPORTED, "cast multiset");
  return ret;
}

OB_SERIALIZE_MEMBER(ObExprCast::CastMultisetExtraInfo,
                    pl_type_, not_null_, elem_type_, capacity_, udt_id_);

int ObExprCast::CastMultisetExtraInfo::deep_copy(common::ObIAllocator &allocator,
                                                    const ObExprOperatorType type,
                                                    ObIExprExtraInfo *&copied_info) const
{
  int ret = OB_SUCCESS;
  OZ(ObExprExtraInfoFactory::alloc(allocator, type, copied_info));
  CastMultisetExtraInfo &other = *static_cast<CastMultisetExtraInfo *>(copied_info);
  if (OB_SUCC(ret)) {
    other = *this;
  }
  return ret;
}


int ObExprCast::cg_expr(ObExprCGCtx &op_cg_ctx,
                        const ObRawExpr &raw_expr,
                        ObExpr &rt_expr) const
{
  int ret = OB_SUCCESS;
  OB_ASSERT(2 == rt_expr.arg_cnt_);
  OB_ASSERT(NULL != rt_expr.args_);
  OB_ASSERT(NULL != rt_expr.args_[0]);
  OB_ASSERT(NULL != rt_expr.args_[1]);
  ObObjType in_type = rt_expr.args_[0]->datum_meta_.type_;
  ObCollationType in_cs_type = rt_expr.args_[0]->datum_meta_.cs_type_;
  ObObjType out_type = rt_expr.datum_meta_.type_;
  ObCollationType out_cs_type = rt_expr.datum_meta_.cs_type_;

  if (OB_UNLIKELY(ObMaxType == in_type || ObMaxType == out_type) ||
      OB_ISNULL(op_cg_ctx.allocator_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("in_type or out_type or allocator is invalid", K(ret),
             K(in_type), K(out_type), KP(op_cg_ctx.allocator_));
  } else {
    // setup cast mode for explicit cast.
    // 隐式cast的cast mode在创建cast expr时已经被设置好了，直接从raw_expr.get_extra()里拿
    ObCastMode cast_mode = raw_expr.get_extra();
    if (cast_mode & CM_ZERO_FILL) {
      // 将zerofill信息放在scale里面
      const ObRawExpr *src_raw_expr = NULL;
      CK(OB_NOT_NULL(src_raw_expr = raw_expr.get_param_expr(0)));
      if (OB_SUCC(ret)) {
        const ObExprResType &src_res_type = src_raw_expr->get_result_type();
        if (OB_UNLIKELY(UINT_MAX8 < src_res_type.get_length())) {
          ret = OB_ERR_UNEXPECTED;
          LOG_WARN("unexpected zerofill length", K(ret), K(src_res_type.get_length()));
        } else if (ob_is_string_or_lob_type(out_type)) {
          // The zerofill information will only be used when cast to string/lob type.
          // for these types, scale is unused, so the previous design is to save child length
          // to the scale of the rt_expr.
          rt_expr.datum_meta_.scale_ = static_cast<int8_t>(src_res_type.get_length());
        }
      }
    }
    rt_expr.is_called_in_sql_ = is_called_in_sql();
    if (OB_SUCC(ret)) {
      const ObRawExpr *src_raw_expr = raw_expr.get_param_expr(0);
      if (OB_ISNULL(src_raw_expr)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("unexpected null", K(ret));
      } else if (src_raw_expr->is_multiset_expr()) {
        if (OB_FAIL(cg_cast_multiset(op_cg_ctx, raw_expr, rt_expr))) {
          LOG_WARN("failed to cg cast multiset", K(ret));
        }
      } else {
        if (OB_FAIL(ObDatumCast::choose_cast_function(in_type, in_cs_type,
                    out_type, out_cs_type, cast_mode, *(op_cg_ctx.allocator_), rt_expr))) {
          LOG_WARN("choose_cast_func failed", K(ret));
        }
      }
    }
    if (OB_SUCC(ret)) {
      rt_expr.extra_ = cast_mode;
    }
  }
  return ret;
}

int ObExprCast::do_implicit_cast(ObExprCtx &expr_ctx,
                                 const ObCastMode &cast_mode,
                                 const ObObjType &dst_type,
                                 const ObObj &src_obj,
                                 ObObj &dst_obj) const
{
  int ret = OB_SUCCESS;

  EXPR_SET_CAST_CTX_MODE(expr_ctx);
  const ObDataTypeCastParams dtc_params =
        ObBasicSessionInfo::create_dtc_params((expr_ctx).my_session_);
  ObCastCtx cast_ctx((expr_ctx).calc_buf_,
                      &dtc_params,
                      (expr_ctx).phy_plan_ctx_->get_cur_time().get_datetime(),
                      (expr_ctx).cast_mode_ | (cast_mode),
                      result_type_.get_collation_type());
  const ObObj *obj_ptr = NULL;
  ObObj tmp_obj;
  if(OB_FAIL(ret)) {
  } else if (OB_FAIL(ObObjCaster::to_type(dst_type, cast_ctx, src_obj, tmp_obj, obj_ptr))) {
    LOG_WARN("cast failed", K(ret), K(src_obj), K(dst_type));
  } else if (OB_ISNULL(obj_ptr)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("obj ptr is NULL", K(ret));
  } else {
    dst_obj = *obj_ptr;
  }
  LOG_DEBUG("do_implicit_cast done", K(ret), K(dst_obj), K(src_obj), K(dst_type), K(cast_mode));
  return ret;
}

OB_DEF_SERIALIZE(ObExprCast)
{
  int ret = OB_SUCCESS;
  ret = ObExprOperator::serialize_(buf, buf_len, pos);
  return ret;
}

OB_DEF_DESERIALIZE(ObExprCast)
{
  int ret = OB_SUCCESS;
  ret = ObExprOperator::deserialize_(buf, data_len, pos);
  return ret;
}

OB_DEF_SERIALIZE_SIZE(ObExprCast)
{
  int64_t len = 0;
  len = ObExprOperator::get_serialize_size_();
  return len;
}

}
}

