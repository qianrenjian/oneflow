#ifndef ONEFLOW_CORE_OPERATOR_COPY_HD_OP_H_
#define ONEFLOW_CORE_OPERATOR_COPY_HD_OP_H_

#include "oneflow/core/operator/operator.h"

namespace oneflow {

class CopyHdOp final : public Operator {
 public:
  OF_DISALLOW_COPY_AND_MOVE(CopyHdOp);
  CopyHdOp() = default;
  ~CopyHdOp() = default;

  void InitFromOpConf() override;
  const PbMessage& GetCustomizedConf() const override;
  bool CanInferInDiffDynamicShapeWithoutIn() const override { return true; }

 private:
  bool IsInputBlobAllowedModelSplit(const std::string& ibn) const override { UNIMPLEMENTED(); }

  LogicalBlobId ibn2lbi(const std::string& input_bn) const override { return GenPackedLbi(); }
  LogicalBlobId obn2lbi(const std::string& output_bn) const override { return GenPackedLbi(); }
};

}  // namespace oneflow

#endif  // ONEFLOW_CORE_OPERATOR_COPY_HD_OP_H_
