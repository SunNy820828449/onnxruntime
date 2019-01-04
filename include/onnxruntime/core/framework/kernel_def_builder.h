// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <limits.h>

#include "core/common/common.h"
#include "core/graph/basic_types.h"
#include "core/framework/data_types.h"
#include "core/framework/allocator.h"

namespace onnxruntime {
class KernelDefBuilder;

typedef std::map<size_t, OrtMemType> MemTypeMap;

// note that input/output might be on CPU implicitly when the node is from CPU execution provider
inline bool MemTypeOnCpuExplicitly(OrtMemType mem_type) {
  return mem_type == OrtMemTypeCPUInput || mem_type == OrtMemTypeCPUOutput;
}

class KernelDef {
 public:
  explicit KernelDef() : default_inputs_mem_type_(OrtMemTypeDefault), default_outputs_mem_type_(OrtMemTypeDefault) {
  }

  const std::string& OpName() const {
    return op_name_;
  }

  const std::string& Domain() const {
    return op_domain_;
  }

  void SinceVersion(/*out*/ int* start, /*out*/ int* end) const {
    *start = op_since_version_start_;
    *end = op_since_version_end_;
  }

  onnxruntime::ProviderType Provider() const {
    return provider_type_;
  }

  const std::unordered_map<std::string, std::vector<MLDataType>>& TypeConstraints() const {
    return type_constraints_;
  }

  const std::vector<std::pair<int, int>>& MayInplace() const {
    return inplace_map_;
  }

  const std::vector<std::pair<int, int>>& Alias() const {
    return alias_map_;
  }

  OrtMemType InputMemoryType(size_t input_index) const {
    auto it = input_memory_type_args_.find(input_index);
    if (it == input_memory_type_args_.end())
      return default_inputs_mem_type_;
    else
      return it->second;
  }

  OrtMemType OutputMemoryType(size_t output_index) const {
    auto it = output_memory_type_args_.find(output_index);
    if (it == output_memory_type_args_.end())
      return default_outputs_mem_type_;
    else
      return it->second;
  }

  int ExecQueueId() const {
    return exec_queue_id_;
  }

  bool IsConflict(const KernelDef& other) const;

 private:
  friend class KernelDefBuilder;

  // The operator name supported by <*this> kernel..
  std::string op_name_;

  // The operator since_version range supported by <*this> kernel.
  // A kernel could support an operator definition between <op_since_version_start>
  // and <op_since_version_end> (inclusive).
  int op_since_version_start_ = 1;
  int op_since_version_end_ = INT_MAX;

  // The operator domain supported by <*this> kernel.
  // Default to 'onnxruntime::kOnnxDomain'.
  // Please note the behavior of std::string("") and std::string() are different
  std::string op_domain_;

  // The type of the execution provider.
  std::string provider_type_;

  // The supported data types for inputs/outputs.
  // Key is input/output name defined in op schema, Value are supported types.
  std::unordered_map<std::string, std::vector<MLDataType>> type_constraints_;

  // An element <i, j> means that output j reuses the memory of input i.
  std::vector<std::pair<int, int>> inplace_map_;

  // An element <i, j> means that output j is an alias of input i.
  std::vector<std::pair<int, int>> alias_map_;

  // The memory types of inputs/outputs of this kernel
  MemTypeMap input_memory_type_args_;
  MemTypeMap output_memory_type_args_;

  // execution command queue id, 0 for default queue in execution provider
  int exec_queue_id_ = 0;
  // Default memory type for all inputs
  OrtMemType default_inputs_mem_type_;
  // Default memory type for all outputs
  OrtMemType default_outputs_mem_type_;
};

class KernelDefBuilder {
 public:
  explicit KernelDefBuilder()
      : kernel_def_(new KernelDef()) {}

  KernelDefBuilder& SetName(const std::string& op_name);
  KernelDefBuilder& SetName(const char* op_name);

  KernelDefBuilder& SetDomain(const std::string& domain);
  KernelDefBuilder& SetDomain(const char* domain);

  /**
     This kernel supports operator definition since <since_version> (to latest).
  */
  KernelDefBuilder& SinceVersion(int since_version) {
    kernel_def_->op_since_version_start_ = since_version;
    return *this;
  }

  /**
     The start and end version should be set accordingly per version range for
     each domain registered in OpSchemaRegistry::DomainToVersionRange in
     \onnxruntime\onnxruntime\core\graph\op.h as below.
     Key: domain. Value: <lowest version, highest version> pair.
     std::unordered_map<std::string, std::pair<int, int>> map_;
  */
  KernelDefBuilder& SinceVersion(int since_version_start, int since_version_end) {
    kernel_def_->op_since_version_start_ = since_version_start;
    kernel_def_->op_since_version_end_ = since_version_end;
    return *this;
  }

  /**
     The execution provider type of the kernel.
  */
  KernelDefBuilder& Provider(onnxruntime::ProviderType provider_type);
  KernelDefBuilder& Provider(const char* provider_type);

  /**
     Specify the set of types that this kernel supports. A further restriction
     of the set of types specified in the op schema.
     The arg name could be either op formal parameter name, say "X", or type
     argument name specified in op schema, say "T".
  */
  KernelDefBuilder& TypeConstraint(const std::string& arg_name,
                                   const std::vector<MLDataType>& supported_types);
  KernelDefBuilder& TypeConstraint(const char* arg_name,
                                   const std::vector<MLDataType>& supported_types);

  /**
     Like TypeConstraint but supports just a single type.
  */
  KernelDefBuilder& TypeConstraint(const std::string& arg_name, MLDataType supported_type);
  KernelDefBuilder& TypeConstraint(const char* arg_name, MLDataType supported_type);

  /**
     Inplace mapping from inputs to outputs allowed.
     It means that uplayer runtime could do memory in-place optimization
     as it will not impact the correctness of this kernel.
  */
  KernelDefBuilder& MayInplace(const std::vector<std::pair<int, int>>& inplaces);
  KernelDefBuilder& MayInplace(int input_index, int output_index);

  /**
     Alias mapping from inputs to outputs. Different from Inplace that the
     content of the tensor is not changed. This is to take care of operators
     such as Identity and Reshape.
  */
  KernelDefBuilder& Alias(const std::vector<std::pair<int, int>>& aliases);
  KernelDefBuilder& Alias(int input_index, int output_index);

  /**
     Specify that this kernel requires an input arg
     in certain memory type (instead of the default, device memory).
  */
  template <OrtMemType T>
  KernelDefBuilder& InputMemoryType(int input_index) {
    kernel_def_->input_memory_type_args_.insert(std::make_pair(input_index, T));
    return *this;
  }

  /**
     Specify that this kernel provides an output arg
     in certain memory type (instead of the default, device memory).
  */
  template <OrtMemType T>
  KernelDefBuilder& OutputMemoryType(int output_index) {
    kernel_def_->output_memory_type_args_.insert(std::make_pair(output_index, T));
    return *this;
  }

  /**
     Specify that this kernel runs on which execution queue in the provider
  */
  KernelDefBuilder& ExecQueueId(int queue_id) {
    kernel_def_->exec_queue_id_ = queue_id;
    return *this;
  }

  /**
  Specify the default inputs memory type, if not specified, it is DefaultMemory
  */
  KernelDefBuilder& SetDefaultInputsMemoryType(OrtMemType mem_type) {
    kernel_def_->default_inputs_mem_type_ = mem_type;
    return *this;
  }

  /**
  Specify the default outputs memory type, if not specified, it is DefaultMemory
  */
  KernelDefBuilder& SetDefaultOutputMemoryType(OrtMemType mem_type) {
    kernel_def_->default_outputs_mem_type_ = mem_type;
    return *this;
  }

  /**
     Return the kernel definition, passing ownership of the KernelDef to the caller
  */
  std::unique_ptr<KernelDef> Build() {
    return std::move(kernel_def_);
  }

 private:
  // we own the KernelDef until Build() is called.
  std::unique_ptr<KernelDef> kernel_def_;
};

}  // namespace onnxruntime
