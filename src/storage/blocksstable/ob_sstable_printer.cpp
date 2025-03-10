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

#include "ob_sstable_printer.h"
#include "storage/tx/ob_tx_data_define.h"
#include "storage/tx_table/ob_tx_table_iterator.h"

namespace oceanbase
{
using namespace storage;
using namespace common;
namespace blocksstable
{
#define FPRINTF(args...) fprintf(stderr, ##args)
#define P_BAR() FPRINTF("|")
#define P_DASH() FPRINTF("------------------------------")
#define P_END_DASH() FPRINTF("--------------------------------------------------------------------------------")
#define P_NAME(key) FPRINTF("%s", (key))
#define P_NAME_FMT(key) FPRINTF("%30s", (key))
#define P_VALUE_STR_B(key) FPRINTF("[%s]", (key))
#define P_VALUE_INT_B(key) FPRINTF("[%d]", (key))
#define P_VALUE_BINT_B(key) FPRINTF("[%ld]", (key))
#define P_VALUE_STR(key) FPRINTF("%s", (key))
#define P_VALUE_INT(key) FPRINTF("%d", (key))
#define P_VALUE_BINT(key) FPRINTF("%ld", (key))
#define P_NAME_BRACE(key) FPRINTF("{%s}", (key))
#define P_COLOR(color) FPRINTF(color)
#define P_LB() FPRINTF("[")
#define P_RB() FPRINTF("]")
#define P_LBRACE() FPRINTF("{")
#define P_RBRACE() FPRINTF("}")
#define P_COLON() FPRINTF(":")
#define P_COMMA() FPRINTF(",")
#define P_TAB() FPRINTF("\t")
#define P_LINE_NAME(key) FPRINTF("%30s", (key))
#define P_LINE_VALUE(key, type) P_LINE_VALUE_##type((key))
#define P_LINE_VALUE_INT(key) FPRINTF("%-30d", (key))
#define P_LINE_VALUE_BINT(key) FPRINTF("%-30ld", (key))
#define P_LINE_VALUE_STR(key) FPRINTF("%-30s", (key))
#define P_END() FPRINTF("\n")

#define P_TAB_LEVEL(level) \
  do { \
    for (int64_t i = 1; i < (level); ++i) { \
      P_TAB(); \
      if (i != level - 1) P_BAR(); \
    } \
  } while (0)

#define P_DUMP_LINE(type) \
  do { \
    P_TAB_LEVEL(level); \
    P_BAR(); \
    P_LINE_NAME(name); \
    P_BAR(); \
    P_LINE_VALUE(value, type); \
    P_END(); \
  } while (0)

#define P_DUMP_LINE_COLOR(type) \
  do { \
    P_COLOR(LIGHT_GREEN); \
    P_TAB_LEVEL(level); \
    P_BAR(); \
    P_COLOR(CYAN); \
    P_LINE_NAME(name); \
    P_BAR(); \
    P_COLOR(NONE_COLOR); \
    P_LINE_VALUE(value, type); \
    P_END(); \
  } while (0)

// mapping to ObObjType
static const char * OB_OBJ_TYPE_NAMES[ObMaxType] = {
    "ObNullType", "ObTinyIntType", "ObSmallIntType",
    "ObMediumIntType", "ObInt32Type", "ObIntType",
    "ObUTinyIntType", "ObUSmallIntType", "ObUMediumIntType",
    "ObUInt32Type", "ObUInt64Type", "ObFloatType",
    "ObDoubleType", "ObUFloatType", "ObUDoubleType",
    "ObNumberType", "ObUNumberType", "ObDateTimeType",
    "ObTimestampType", "ObDateType", "ObTimeType", "ObYearType",
    "ObVarcharType", "ObCharType", "ObHexStringType",
    "ObExtendType", "ObUnknowType", "ObTinyTextType",
    "ObTextType", "ObMediumTextType", "ObLongTextType", "ObBitType",
    "ObEnumType", "ObSetType", "ObEnumInnerType", "ObSetInnerType",
    "ObTimestampTZType", "ObTimestampLTZType", "ObTimestampNanoType",
    "ObRawType", "ObIntervalYMType", "ObIntervalDSType", "ObNumberFloatType",
    "ObNVarchar2Type", "ObNCharType", "ObURowIDType", "ObLobType", "ObJsonType", "ObGeometryType"
};

void ObSSTablePrinter::print_title(const char *title, const int64_t level)
{
  if (isatty(fileno(stderr)) > 0) {
    P_COLOR(LIGHT_GREEN);
  }
  P_TAB_LEVEL(level);
  P_DASH();
  P_NAME_BRACE(title);
  P_DASH();
  P_END();
}

void ObSSTablePrinter::print_end_line(const int64_t level)
{
  if (isatty(fileno(stderr)) > 0) {
    P_COLOR(LIGHT_GREEN);
  }
  P_TAB_LEVEL(level);
  P_END_DASH();
  P_END();
  if (isatty(fileno(stderr)) > 0) {
    P_COLOR(NONE_COLOR);
  }
}

void ObSSTablePrinter::print_title(const char *title, const int64_t value, const int64_t level)
{
  if (isatty(fileno(stderr)) > 0) {
    P_COLOR(LIGHT_GREEN);
  }
  P_TAB_LEVEL(level);
  P_DASH();
  P_LBRACE();
  P_NAME(title);
  P_VALUE_BINT_B(value);
  P_RBRACE();
  P_DASH();
  P_END();
  if (isatty(fileno(stderr)) > 0) {
    P_COLOR(NONE_COLOR);
  }
}

void ObSSTablePrinter::print_line(const char *name, const uint32_t value, const int64_t level)
{
  if (isatty(fileno(stderr)) > 0) {
    P_DUMP_LINE_COLOR(INT);
  } else {
    P_DUMP_LINE(INT);
  }
}

void ObSSTablePrinter::print_line(const char *name, const uint64_t value, const int64_t level)
{
  if (isatty(fileno(stderr)) > 0) {
    P_DUMP_LINE_COLOR(BINT);
  } else {
    P_DUMP_LINE(BINT);
  }
}

void ObSSTablePrinter::print_line(const char *name, const int32_t value, const int64_t level)
{
  if (isatty(fileno(stderr)) > 0) {
    P_DUMP_LINE_COLOR(INT);
  } else {
    P_DUMP_LINE(INT);
  }
}

void ObSSTablePrinter::print_line(const char *name, const int64_t value, const int64_t level)
{
  if (isatty(fileno(stderr)) > 0) {
    P_DUMP_LINE_COLOR(BINT);
  } else {
    P_DUMP_LINE(BINT);
  }
}

void ObSSTablePrinter::print_line(const char *name, const char *value, const int64_t level)
{
  if (isatty(fileno(stderr)) > 0) {
    P_DUMP_LINE_COLOR(STR);
  } else {
    P_DUMP_LINE(STR);
  }
}

void ObSSTablePrinter::print_cols_info_start(const char *n1, const char *n2, const char *n3, const char *n4, const char *n5)
{
  if (isatty(fileno(stderr)) > 0) {
    P_COLOR(LIGHT_GREEN);
  }
  FPRINTF("--------{%-15s %15s %15s %15s %15s}----------\n", n1, n2, n3, n4, n5);
}

void ObSSTablePrinter::print_cols_info_line(const int32_t &v1, const common::ObObjType v2, const common::ObOrderType v3, const int64_t &v4, const int64_t & v5)
{
  const char *v3_cstr = ObOrderType::ASC == v3 ? "ASC" : "DESC";
  if (isatty(fileno(stderr)) > 0) {
    P_COLOR(LIGHT_GREEN);
    P_BAR();
    P_COLOR(NONE_COLOR);
    FPRINTF("\t[%-15d %15s %15s %15ld %15ld]\n", v1, OB_OBJ_TYPE_NAMES[v2], v3_cstr, v4, v5);
  } else {
    P_BAR();
    FPRINTF("\t[%-15d %15s %15s %15ld %15ld]\n", v1, OB_OBJ_TYPE_NAMES[v2], v3_cstr, v4, v5);
  }
}

void ObSSTablePrinter::print_row_title(const ObDatumRow *row, const int64_t row_index)
{
  char dml_flag[16];
  char mvcc_flag[16];
  row->mvcc_row_flag_.format_str(mvcc_flag, 16);
  row->row_flag_.format_str(dml_flag, 16);
  if (isatty(fileno(stderr)) > 0) {
    P_COLOR(LIGHT_GREEN);
    P_BAR();
    P_COLOR(CYAN);
    P_NAME("ROW");
    P_VALUE_BINT_B(row_index);
    P_COLON();
    P_NAME("trans_id=");
    P_VALUE_STR_B(to_cstring(row->trans_id_));
    P_COMMA();
    P_NAME("dml_flag=");
    P_VALUE_STR_B(dml_flag);
    P_COMMA();
    P_NAME("mvcc_flag=");
    P_VALUE_STR_B(mvcc_flag);
    P_BAR();
    P_COLOR(NONE_COLOR);
  } else {
    P_BAR();
    P_NAME("ROW");
    P_VALUE_BINT_B(row_index);
    P_COLON();
    P_NAME("trans_id=");
    P_VALUE_STR_B(to_cstring(row->trans_id_));
    P_COMMA();
    P_NAME("dml_flag=");
    P_VALUE_STR_B(dml_flag);
    P_COMMA();
    P_NAME("mvcc_flag=");
    P_VALUE_STR_B(mvcc_flag);
    P_BAR();
  }
}

void ObSSTablePrinter::print_cell(const ObObj &cell)
{
  P_VALUE_STR_B(to_cstring(cell));
}

void ObSSTablePrinter::print_common_header(const ObMacroBlockCommonHeader *common_header)
{
  print_title("Common Header");
  print_line("header_size", common_header->get_header_size());
  print_line("version", common_header->get_version());
  print_line("magic", common_header->get_magic());
  print_line("attr", common_header->get_attr());
  print_line("payload_size", common_header->get_payload_size());
  print_line("payload_checksum", common_header->get_payload_checksum());
  print_end_line();
}

void ObSSTablePrinter::print_macro_block_header(const ObSSTableMacroBlockHeader *sstable_header)
{
  print_title("SSTable Macro Block Header");
  print_line("header_size", sstable_header->fixed_header_.header_size_);
  print_line("version", sstable_header->fixed_header_.version_);
  print_line("magic", sstable_header->fixed_header_.magic_);
  print_line("tablet_id", sstable_header->fixed_header_.tablet_id_);
  print_line("logical_version", sstable_header->fixed_header_.logical_version_);
  print_line("data_seq", sstable_header->fixed_header_.data_seq_);
  print_line("column_count", sstable_header->fixed_header_.column_count_);
  print_line("rowkey_column_count", sstable_header->fixed_header_.rowkey_column_count_);
  print_line("row_store_type", sstable_header->fixed_header_.row_store_type_);
  print_line("row_count", sstable_header->fixed_header_.row_count_);
  print_line("occupy_size", sstable_header->fixed_header_.occupy_size_);
  print_line("micro_block_count", sstable_header->fixed_header_.micro_block_count_);
  print_line("micro_block_data_offset", sstable_header->fixed_header_.micro_block_data_offset_);
  print_line("data_checksum", sstable_header->fixed_header_.data_checksum_);
  print_line("compressor_type", sstable_header->fixed_header_.compressor_type_);
  print_line("master_key_id", sstable_header->fixed_header_.master_key_id_);
  print_end_line();
}

void ObSSTablePrinter::print_macro_block_header(const ObBloomFilterMacroBlockHeader *bf_macro_header)
{
  print_title("SSTable Bloomfilter Macro Block Header");
  print_line("header_size", bf_macro_header->header_size_);
  print_line("version", bf_macro_header->version_);
  print_line("magic", bf_macro_header->magic_);
  print_line("attr", bf_macro_header->attr_);
  print_line("tablet_id", bf_macro_header->tablet_id_);
  print_line("data_version", bf_macro_header->snapshot_version_);
  print_line("rowkey_column_count", bf_macro_header->rowkey_column_count_);
  print_line("row_count", bf_macro_header->row_count_);
  print_line("occupy_size", bf_macro_header->occupy_size_);
  print_line("micro_block_count", bf_macro_header->micro_block_count_);
  print_line("micro_block_data_offset", bf_macro_header->micro_block_data_offset_);
  print_line("micro_block_data_size", bf_macro_header->micro_block_data_size_);
  print_line("data_checksum", bf_macro_header->data_checksum_);
  print_line("compressor_type", bf_macro_header->compressor_type_);
  print_end_line();
}

void ObSSTablePrinter::print_macro_block_header(const storage::ObLinkedMacroBlockHeader *linked_macro_header)
{
  print_title("Linked Macro Block Header");
  print_line("version", linked_macro_header->version_);
  print_line("magic", linked_macro_header->magic_);
  print_line("item_count", linked_macro_header->item_count_);
  print_line("fragment_offset", linked_macro_header->fragment_offset_);
  print_line("previous_macro_block_index", linked_macro_header->previous_macro_block_id_.block_index());
  print_line("previous_macro_block_seq", linked_macro_header->previous_macro_block_id_.write_seq());
  print_end_line();
}

void ObSSTablePrinter::print_index_row_header(const ObIndexBlockRowHeader *idx_row_header)
{
  if (isatty(fileno(stderr)) > 0) {
    P_COLOR(LIGHT_GREEN);
    P_BAR();
    P_COLOR(CYAN);
    P_NAME("Index Block Row Header");
    P_BAR();
    P_COLOR(NONE_COLOR);
    P_VALUE_STR_B(to_cstring(*idx_row_header));
    P_END();
  } else {
    P_BAR();
    P_NAME("Index Block Row Header");
    P_BAR();
    P_VALUE_STR_B(to_cstring(*idx_row_header));
    P_END();
  }
}

void ObSSTablePrinter::print_index_minor_meta(const ObIndexBlockRowMinorMetaInfo *minor_meta)
{
  if (isatty(fileno(stderr)) > 0) {
    P_COLOR(LIGHT_GREEN);
    P_BAR();
    P_COLOR(CYAN);
    P_NAME("Index Block Minor Meta Info");
    P_BAR();
    P_COLOR(NONE_COLOR);
    P_VALUE_STR_B(to_cstring(*minor_meta));
    P_END();
  } else {
    P_BAR();
    P_NAME("Index Block Minor Meta Info");
    P_BAR();
    P_VALUE_STR_B(to_cstring(*minor_meta));
    P_END();
  }
}

void ObSSTablePrinter::print_micro_header(const ObMicroBlockHeader *micro_block_header)
{
  print_title("Micro Header");
  print_line("header_size", micro_block_header->header_size_);
  print_line("version", micro_block_header->version_);
  print_line("magic", micro_block_header->magic_);
  print_line("column_count", micro_block_header->column_count_);
  print_line("rowkey_column_count", micro_block_header->rowkey_column_count_);
  print_line("row_count", micro_block_header->row_count_);
  print_line("row_store_type", micro_block_header->row_store_type_);
  print_line("opt", micro_block_header->opt_);
  print_line("row_offset", micro_block_header->row_offset_);
  print_line("data_length", micro_block_header->data_length_);
  print_line("data_zlength", micro_block_header->data_zlength_);
  print_line("data_checksum", micro_block_header->data_checksum_);
  if (micro_block_header->has_column_checksum_) {
    for (int64_t i = 0; i < micro_block_header->column_count_; ++i) {
      if (isatty(fileno(stderr)) > 0) {
        P_COLOR(LIGHT_GREEN);
        P_BAR();
        P_COLOR(NONE_COLOR);
        FPRINTF("column_chksum[%15ld]|%-30ld\n", i, micro_block_header->column_checksums_[i]);
      } else {
        P_BAR();
        FPRINTF("column_chksum[%15ld]|%-30ld\n", i, micro_block_header->column_checksums_[i]);
      }
    }
  }
  print_end_line();
}

void ObSSTablePrinter::print_encoding_micro_header(const blocksstable::ObMicroBlockHeader *micro_header)
{
  print_title("Encoding Micro Header");
  print_line("header_size", micro_header->header_size_);
  print_line("version", micro_header->version_);
  print_line("magic", micro_header->magic_);
  print_line("column_count", micro_header->column_count_);
  print_line("rowkey_column_count", micro_header->rowkey_column_count_);
  print_line("row_count", micro_header->row_count_);
  print_line("row_store_type", micro_header->row_store_type_);
  print_line("row_index_byte", micro_header->row_index_byte_);
  print_line("var_column_count", micro_header->var_column_count_);
  print_line("row_data_offset", micro_header->row_data_offset_);
  if (micro_header->has_column_checksum_) {
    for (int64_t i = 0; i < micro_header->column_count_; ++i) {
      if (isatty(fileno(stderr)) > 0) {
        P_COLOR(LIGHT_GREEN);
        P_BAR();
        P_COLOR(NONE_COLOR);
        FPRINTF("column_chksum[%15ld]|%-30ld\n", i, micro_header->column_checksums_[i]);
      } else {
        P_BAR();
        FPRINTF("column_chksum[%15ld]|%-30ld\n", i, micro_header->column_checksums_[i]);
      }
    }
  }
  print_end_line();
}

void ObSSTablePrinter::print_bloom_filter_micro_header(const ObBloomFilterMicroBlockHeader *micro_block_header)
{
  print_title("Bloom Filter Micro Header");
  print_line("header_size", micro_block_header->header_size_);
  print_line("version", micro_block_header->version_);
  print_line("magic", micro_block_header->magic_);
  print_line("rowkey_column_count", micro_block_header->rowkey_column_count_);
  print_line("row_count", micro_block_header->row_count_);
  print_line("reserved", micro_block_header->reserved_);
  print_end_line();
}

void ObSSTablePrinter::print_bloom_filter_micro_block(const char* micro_block_buf, const int64_t micro_block_size)
{
  print_title("Bloom Filter Micro Data");
  print_line("block_size", micro_block_size);
  P_VALUE_STR_B(micro_block_buf);
  P_END();
}

void ObSSTablePrinter::print_store_row(
    const ObDatumRow *row,
    const ObObjMeta *obj_metas,
    const int64_t rowkey_column_cnt,
    const bool is_index_block,
    const bool is_trans_sstable)
{
  int ret = OB_SUCCESS;
  if (is_trans_sstable) {
    transaction::ObTransID tx_id = row->storage_datums_[TX_DATA_ID_COLUMN].get_int();
    // TODO : @gengli There may be multiple rows belong to one tx data. Handle this situation
    const ObString str
      = row
          ->storage_datums_[TX_DATA_VAL_COLUMN
                            + ObMultiVersionRowkeyHelpper::get_extra_rowkey_col_cnt()]
          .get_string();
    int64_t pos = 0;

    if (OB_LIKELY(tx_id.get_id() != INT64_MAX)) {
      ObMemAttr mem_attr;
      mem_attr.label_ = "TX_DATA_TABLE";
      void *p = op_alloc(ObSliceAlloc);
      auto slice_allocator = new (p) ObSliceAlloc(storage::TX_DATA_SLICE_SIZE, mem_attr);

      ObTxData tx_data;
      tx_data.tx_id_ = tx_id;
      if (OB_FAIL(tx_data.deserialize(str.ptr(), str.length(), pos, *slice_allocator))) {
        STORAGE_LOG(WARN, "deserialize tx data failed", KR(ret), K(str));
        hex_dump(str.ptr(), str.length(), true, OB_LOG_LEVEL_WARN);
      } else {
        ObTxData::print_to_stderr(tx_data);
      }
    } else {
      // pre-process data for upper trans version calculation
      void *p = op_alloc(ObCommitVersionsArray);
      ObCommitVersionsArray *commit_versions = new (p) ObCommitVersionsArray();

      if (OB_FAIL(commit_versions->deserialize(str.ptr(), str.length(), pos))) {
        STORAGE_LOG(WARN, "deserialize commit versions failed", KR(ret), K(str));
        hex_dump(str.ptr(), str.length(), true, OB_LOG_LEVEL_WARN);
      } else {
        ObCommitVersionsArray::print_to_stderr(*commit_versions);
      }
    }
  } else {
    ObObj obj;
    for (int64_t i = 0; i < row->get_column_count(); ++i) {
      ObObjMeta column_meta = obj_metas[i];
      if (is_index_block && i == rowkey_column_cnt) {
        column_meta.set_varbinary();
      }
      if (OB_FAIL(row->storage_datums_[i].to_obj_enhance(obj, column_meta))) {
        STORAGE_LOG(WARN, "Fail to transform storage datum to obj", K(ret), K(i), K(column_meta),
                    KPC(row));
      } else {
        print_cell(obj);
      }
    }
  }
  P_END();
}

void ObSSTablePrinter::print_store_row_hex(const ObDatumRow *row, const ObObjMeta *obj_metas, const int64_t buf_size, char *hex_print_buf)
{

  int ret = OB_SUCCESS;
  if (OB_NOT_NULL(hex_print_buf)) {
    ObObj obj;
    for (int64_t i = 0; i < row->get_column_count(); ++i) {
      int64_t pos = 0;
      if (OB_FAIL(row->storage_datums_[i].to_obj_enhance(obj, obj_metas[i]))) {
        STORAGE_LOG(WARN, "Fail to transform storage datum to obj", K(ret), K(i), K(obj_metas[i]), KPC(row));
      } else if (OB_FAIL(obj.print_smart(hex_print_buf, buf_size, pos))) {
        STORAGE_LOG(WARN, "Failed to print obj to hex buf", K(ret), K(i), K(obj));
      } else {
        P_VALUE_STR_B(hex_print_buf);
      }
    }
    P_END();
  }
}

void ObSSTablePrinter::print_encoding_column_header(const ObColumnHeader *col_header, const int64_t col_id)
{
  print_title("Encoding Column Header", col_id, 1);
  print_line("type", col_header->type_);
  print_line("attribute", col_header->attr_);
  print_line("is fix length", col_header->is_fix_length());
  print_line("has extend value", col_header->has_extend_value());
  print_line("is bit packing", col_header->is_bit_packing());
  print_line("is last var field", col_header->is_last_var_field());
  print_line("extend value index", col_header->extend_value_index_);
  print_line("store object type", col_header->obj_type_);
  print_line("offset", col_header->offset_);
  print_line("length", col_header->length_);
  print_end_line();
}

void ObSSTablePrinter::print_hex_micro_block_header(const ObMicroBlockData &block_data)
{
  print_title("micro_block_hex_data");
  P_NAME("block_size");
  P_COLON();
  P_VALUE_BINT(block_data.get_buf_size());
  P_COMMA();
  P_NAME("extra_block_size");
  P_COLON();
  P_VALUE_BINT(block_data.get_extra_size());

}

void ObSSTablePrinter::print_hex_micro_block(const ObMicroBlockData &block_data, char *hex_print_buf, const int64_t buf_size)
{
  print_hex_micro_block_header(block_data);
  P_COMMA();
  P_NAME("block_data");
  P_COLON();
  P_LBRACE();
  if (hex_print_buf != nullptr) {
    to_hex_cstr(block_data.buf_, block_data.get_buf_size(), hex_print_buf, buf_size);
    P_VALUE_STR_B(hex_print_buf);
  } else {
    P_VALUE_STR_B(block_data.get_buf());
  }
  P_RBRACE();
  P_END();

}

}
}
