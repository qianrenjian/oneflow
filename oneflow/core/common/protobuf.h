#ifndef ONEFLOW_CORE_COMMON_PROTOBUF_H_
#define ONEFLOW_CORE_COMMON_PROTOBUF_H_

#ifdef _MSC_VER
#include <io.h>
#endif
#include <google/protobuf/descriptor.h>
#include <google/protobuf/map.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/message_differencer.h>
#include "oneflow/core/common/util.h"
#include "oneflow/core/common/preprocessor.h"
#include "oneflow/core/operator/op_conf.pb.h"
#include "oneflow/core/persistence/persistent_out_stream.h"

namespace oneflow {

using PbMessage = google::protobuf::Message;
template<typename T>
using PbRf = google::protobuf::RepeatedField<T>;
template<typename T>
using PbRpf = google::protobuf::RepeatedPtrField<T>;
template<typename T1, typename T2>
using PbMapPair = google::protobuf::MapPair<T1, T2>;
template<typename K, typename V>
using PbMap = google::protobuf::Map<K, V>;
using PbFd = google::protobuf::FieldDescriptor;
using PbMd = google::protobuf::util::MessageDifferencer;

#define PROTOBUF_BASIC_DATA_TYPE_SEQ        \
  OF_PP_MAKE_TUPLE_SEQ(std::string, String) \
  OF_PP_MAKE_TUPLE_SEQ(int32_t, Int32)      \
  OF_PP_MAKE_TUPLE_SEQ(uint32_t, UInt32)    \
  OF_PP_MAKE_TUPLE_SEQ(int64_t, Int64)      \
  OF_PP_MAKE_TUPLE_SEQ(uint64_t, UInt64)    \
  OF_PP_MAKE_TUPLE_SEQ(float, Float)        \
  OF_PP_MAKE_TUPLE_SEQ(int16_t, EnumValue)  \
  OF_PP_MAKE_TUPLE_SEQ(bool, Bool)

#define PROTOBUF_GET_FIELDDESC(msg, field_name)                            \
  auto d = const_cast<google::protobuf::Descriptor*>(msg.GetDescriptor()); \
  auto fd = const_cast<PbFd*>(d->FindFieldByName(field_name));

#define PROTOBUF_REFLECTION(msg, field_name) \
  PROTOBUF_GET_FIELDDESC(msg, field_name)    \
  CHECK_NOTNULL(fd);                         \
  auto r = const_cast<google::protobuf::Reflection*>(msg.GetReflection());

// Prototxt <-> File
bool TryParseProtoFromTextFile(const std::string& file_path, PbMessage* proto);
void ParseProtoFromTextFile(const std::string& file_path, PbMessage* proto);
void PrintProtoToTextFile(const PbMessage& proto, const std::string& file_path);

// Does PbMessage have the field_name
bool HasFieldInPbMessage(const PbMessage&, const std::string& field_name);

// Get From PbMessage

const PbFd* GetPbFdFromPbMessage(const PbMessage&, const std::string& field_name);

template<typename T>
T GetValFromPbMessage(const PbMessage&, const std::string& field_name);

int32_t GetEnumFromPbMessage(const PbMessage&, const std::string& field_name);

template<typename T>
const PbRf<T>& GetPbRfFromPbMessage(const PbMessage& msg, const std::string& field_name) {
  PROTOBUF_REFLECTION(msg, field_name);
  return r->GetRepeatedField<T>(msg, fd);
}

template<typename T>
const PbRpf<T>& GetPbRpfFromPbMessage(const PbMessage& msg, const std::string& field_name) {
  PROTOBUF_REFLECTION(msg, field_name);
  return r->GetRepeatedPtrField<T>(msg, fd);
}

template<typename T>
PbRpf<T>* MutPbRpfFromPbMessage(PbMessage* msg, const std::string& field_name) {
  PROTOBUF_REFLECTION((*msg), field_name);
  return r->MutableRepeatedPtrField<T>(msg, fd);
}

// Set In PbMessage

template<typename T>
void SetValInPbMessage(PbMessage* msg, const std::string& field_name, const T& val);

PbMessage* MutableMessageInPbMessage(PbMessage*, const std::string& field_name);
PbMessage* MutableMessageInPbMessage(PbMessage*, int field_index);

// Add In PbMessage RepeatedField

template<typename T>
void AddValInPbRf(PbMessage*, const std::string& field_name, const T& val);

// PbRf <-> std::vector

template<typename T>
inline std::vector<T> PbRf2StdVec(const PbRf<T>& rf) {
  return std::vector<T>(rf.begin(), rf.end());
}

template<typename T>
inline PbRf<T> StdVec2PbRf(const std::vector<T>& vec) {
  return PbRf<T>(vec.begin(), vec.end());
}

// PbRpf <-> std::vector
template<typename T>
inline std::vector<T> PbRpf2StdVec(const PbRpf<T>& rpf) {
  return std::vector<T>(rpf.begin(), rpf.end());
}

template<typename T>
inline PbRpf<T> StdVec2PbRpf(const std::vector<T>& vec) {
  using RetType = PbRpf<T>;
  return RetType(vec.begin(), vec.end());
}

// ProtoMap <-> HashMap
template<typename K, typename V>
HashMap<K, V> PbMap2HashMap(const google::protobuf::Map<K, V>& pb_map) {
  return HashMap<K, V>(pb_map.begin(), pb_map.end());
}

template<typename K, typename V>
google::protobuf::Map<K, V> HashMap2PbMap(const HashMap<K, V>& hash_map) {
  using RetType = google::protobuf::Map<K, V>;
  return RetType(hash_map.begin(), hash_map.end());
}

// Hack Oneof Getter

template<typename T = PbMessage>
const T* GetMsgPtrFromPbMessage(const PbMessage& msg, const std::string& field_name) {
  PROTOBUF_REFLECTION(msg, field_name);
  if (r->HasField(msg, fd)) {
    return static_cast<const T*>(&(GetValFromPbMessage<const PbMessage&>(msg, field_name)));
  } else {
    return nullptr;
  }
}

// If value exists in RepeatedField
template<typename T>
bool IsInRepeatedField(const PbRf<T>& repeated_field, const T& value) {
  return std::find(repeated_field.cbegin(), repeated_field.cend(), value) != repeated_field.cend();
}

// LBI compare operator

inline bool operator<(const LogicalBlobId& lhs, const LogicalBlobId& rhs) {
  if (lhs.op_name() != rhs.op_name()) { return lhs.op_name() < rhs.op_name(); }
  if (lhs.blob_name() != rhs.blob_name()) { return lhs.blob_name() < rhs.blob_name(); }
  if (lhs.clone_id() != rhs.clone_id()) { return lhs.clone_id() < rhs.clone_id(); }
  if (lhs.is_packed_id() != rhs.is_packed_id()) { return lhs.is_packed_id() < rhs.is_packed_id(); }
  return false;
}

inline bool operator==(const LogicalBlobId& lhs, const LogicalBlobId& rhs) {
  PbMd message_diff;
  return message_diff.Equivalent(lhs, rhs);
}

// Persistent

PersistentOutStream& operator<<(PersistentOutStream&, const PbMessage&);

}  // namespace oneflow

namespace std {

template<>
struct hash<oneflow::LogicalBlobId> {
  size_t operator()(const oneflow::LogicalBlobId& lbi) const {
    return std::hash<std::string>()(lbi.op_name() + lbi.blob_name() + std::to_string(lbi.clone_id())
                                    + std::to_string(lbi.is_packed_id()));
  }
};

}  // namespace std

#endif  // ONEFLOW_CORE_COMMON_PROTOBUF_H_
