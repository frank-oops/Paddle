/* Copyright (c) 2023 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/fluid/distributed/auto_parallel/spmd_rules/common.h"

namespace paddle {
namespace distributed {
namespace auto_parallel {

std::vector<DistTensorSpec> SPMDRuleBase::InferForward(
    const std::vector<DistTensorSpec>& input_specs,
    const paddle::framework::AttributeMap& attrs) {
  PADDLE_THROW(
      phi::errors::Unimplemented("InferForward should be called from a "
                                 "derived class of SPMDRuleBase !"));
}

std::vector<DistTensorSpec> SPMDRuleBase::InferBackward(
    const std::vector<DistTensorSpec>& output_specs,
    const paddle::framework::AttributeMap& attrs) {
  PADDLE_THROW(
      phi::errors::Unimplemented("InferBackward should be called from a "
                                 "derived class of SPMDRuleBase !"));
}

std::unordered_map<std::string, int64_t> ShardingMergeForTensors(
    const std::vector<std::pair<const std::string, const std::vector<int64_t>>>&
        tensor_notation_to_dim_pairs) {
  std::unordered_map<std::string, int64_t> axis_to_dim_map;
  std::unordered_map<int64_t, std::string> dim_to_axis_map;
  int64_t merge_dim;

  for (auto& pair : tensor_notation_to_dim_pairs) {
    for (int i = 0; i < pair.second.size(); i++) {
      auto tensor_axis = pair.first.substr(i, 1);
      auto mesh_dim = pair.second[i];

      if (axis_to_dim_map.count(tensor_axis) == 0) {
        merge_dim = mesh_dim;
      } else {
        merge_dim = ShardingMergeForAxis(
            tensor_axis, mesh_dim, axis_to_dim_map[tensor_axis]);
      }
      axis_to_dim_map.insert({tensor_axis, merge_dim});

      if (dim_to_axis_map.count(merge_dim) == 0) {
        dim_to_axis_map.insert({merge_dim, tensor_axis});
      } else {
        dim_to_axis_map[merge_dim] += tensor_axis;
      }
    }
  }

  // Resolute "mesh_dim shard by more than one axis" confict.
  // Now we just naive pick the first axis naively.
  // (TODO) use local cost model to pick the axis with lowest cost(in concern of
  // memory or communication or computation).
  for (auto& it : dim_to_axis_map) {
    if (it.second.size() > 1) {
      VLOG(4) << "Sharding Conflict: Mesh_Dim [" << it.first
              << "] are Sharding Multiple Tensor Axis: [" << it.second
              << "]. The Axis: [" << it.second[0] << "] is Picked.";
      for (int i = 1; i < it.second.size(); i++) {
        axis_to_dim_map[it.second.substr(i, 1)] = -1;
      }
    }
  }

  return axis_to_dim_map;
}

// Rule1: A repicated dimension could be merged by any sharded dimension.
// Rule2: A tensor axis could at most be sharded by one mesh dimension.
// (TODO trigger heuristics cost model and reshard to handle axis sharded by
// multiple dimension case.)
int64_t ShardingMergeForAxis(const std::string axis,
                             const int64_t mesh_dim1,
                             const int64_t mesh_dim2) {
  if (mesh_dim1 != mesh_dim2) {
    if (mesh_dim1 == -1) {
      return mesh_dim2;
    } else if (mesh_dim2 == -1) {
      return mesh_dim1;
    } else {
      // (TODO) local cost model here.
      PADDLE_THROW(
          phi::errors::Unimplemented("Tensor Axis[%s] is Sharded by two "
                                     "different mesh dimension [%d] and [%d].",
                                     axis,
                                     mesh_dim1,
                                     mesh_dim2));
    }

  } else {
    return mesh_dim1;
  }
}

TensorDistAttr CopyTensorDistAttrForOutput(
    const TensorDistAttr& src_dist_attr) {
  TensorDistAttr new_dist_attr = TensorDistAttr();
  new_dist_attr.set_process_mesh(src_dist_attr.process_mesh());
  new_dist_attr.set_batch_dim(src_dist_attr.batch_dim());
  new_dist_attr.set_dynamic_dims(src_dist_attr.dynamic_dims());
  new_dist_attr.set_annotated(false);
  return new_dist_attr;
}

std::vector<int64_t> ResoluteOutputPartialDimension(
    const std::unordered_map<std::string, int64_t>& in_axis_to_dim_map,
    const std::string& out_axis) {
  std::vector<int64_t> partial_on_dims;

  for (auto& it : in_axis_to_dim_map) {
    if (out_axis.find(it.first) != std::string::npos) {
      if (it.second > -1) {
        partial_on_dims.push_back(it.second);
      }
    }
  }
  return partial_on_dims;
}

}  // namespace auto_parallel
}  // namespace distributed
}  // namespace paddle
