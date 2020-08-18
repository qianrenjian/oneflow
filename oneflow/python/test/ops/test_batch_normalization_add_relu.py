"""
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
import os
from collections import OrderedDict

import numpy as np
import oneflow as flow
import tensorflow as tf
import test_global_storage
from test_util import Args, GenArgDict, type_name_to_flow_type, type_name_to_np_type
import oneflow.typing as oft
import unittest


@unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
def test_train_consistent(test_case):
    flow.config.enable_debug_mode(True)
    func_config = flow.FunctionConfig()
    func_config.default_logical_view(flow.scope.consistent_view())
    func_config.default_data_type(flow.float32)

    @flow.global_function(type="train", function_config=func_config)
    def Foo(
        x: oft.Numpy.Placeholder((2, 8, 32, 32)),
        add_in: oft.Numpy.Placeholder((2, 8, 32, 32)),
    ):
        y = flow.layers.batch_normalization_add_relu(x, add_in, axis=1)
        flow.optimizer.SGD(
            flow.optimizer.PiecewiseConstantScheduler([], [0.001]), momentum=0
        ).minimize(flow.math.reduce_sum(y))

    Foo(
        np.ones((2, 8, 32, 32), dtype=np.float32),
        np.ones((2, 8, 32, 32), dtype=np.float32),
    )


@unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
def test_no_watch_scope_consistent(test_case):
    func_config = flow.FunctionConfig()
    func_config.default_logical_view(flow.scope.consistent_view())
    func_config.default_data_type(flow.float32)

    @flow.global_function(function_config=func_config)
    def Foo(
        x: oft.Numpy.Placeholder((2, 8, 32, 32)),
        add_in: oft.Numpy.Placeholder((2, 8, 32, 32)),
    ):
        return flow.layers.batch_normalization_add_relu(x, add_in)

    Foo(
        np.ones((2, 8, 32, 32), dtype=np.float32),
        np.ones((2, 8, 32, 32), dtype=np.float32),
    )


def test_layer_batchnormaddrelu(test_case):
    arg_dict = OrderedDict()
    arg_dict["device_type"] = ["gpu"]
    arg_dict["data_type"] = ["float32"]
    arg_dict["input_shape"] = [(1, 4, 1, 2)]
    arg_dict["op_args"] = [
        Args([1]),
        Args([2]),
        Args([1, 0.95, 0.0001]),
        Args([1, 0.99, 0.001, False]),
        Args([1, 0.99, 0.001, False, False]),
        Args([]),
        Args([1, 0.95, 0.1]),
    ]
    for arg in GenArgDict(arg_dict):
        CompareBnAddReluWithTensorFlow(**arg)


def CompareBnAddReluWithTensorFlow(
    device_type,
    input_shape,
    data_type,
    op_args=None,
    input_minval=-10,
    input_maxval=10,
    y_rtol=1e-5,
    y_atol=1e-5,
    x_diff_rtol=1e-5,
    x_diff_atol=1e-5,
    training=True,
    trainable=True,
):
    assert device_type in ["gpu", "cpu"]
    # tf bn doesn't support double
    assert data_type in ["float32"]
    if op_args is None:
        flow_args, tf_args = [], []
    else:
        flow_args, tf_args = op_args.flow_args, op_args.tf_args

    x = np.random.uniform(low=input_minval, high=input_maxval, size=input_shape)
    if device_type == "cpu":
        y_rtol *= 100
        y_atol *= 100
        x_diff_rtol *= 100
        x_diff_atol *= 100
    if trainable:
        of_y, of_x_diff = RunOneflowLayerBnAddRelu(
            device_type, x, data_type, flow_args, training=training, trainable=trainable
        )
        tf_y, tf_x_diff = RunTensorFlowBnAddRelu(
            x, tf_args, training=training, trainable=trainable
        )
        assert np.allclose(of_y, tf_y, rtol=y_rtol, atol=y_atol), of_y - tf_y
        assert np.allclose(of_x_diff, tf_x_diff, rtol=x_diff_rtol, atol=x_diff_atol)
    else:
        of_y = RunOneflowLayerBnAddRelu(
            device_type, x, data_type, flow_args, training=training, trainable=trainable
        )
        tf_y = RunTensorFlowBnAddRelu(
            x, tf_args, training=training, trainable=trainable
        )
        assert np.allclose(of_y, tf_y, rtol=y_rtol, atol=y_atol)


def RunTensorFlowBnAddRelu(x, tf_args, training, trainable):
    x = x.astype(np.float32)
    # TensorFlow
    with tf.GradientTape(persistent=True) as tape:
        x = tf.Variable(x)
        tf_op = tf.keras.layers.BatchNormalization(*tf_args, trainable=trainable)

        y = tf_op(x, training=training)
        y = tf.nn.relu(tf.math.add(y, x))
    if trainable:
        x_diff = tape.gradient(y, x)
        return y.numpy(), x_diff.numpy()
    else:
        return y.numpy()


def RunOneflowLayerBnAddRelu(
    device_type, x, data_type, flow_args, training=True, trainable=True
):
    flow.clear_default_session()
    func_config = flow.FunctionConfig()
    func_config.default_logical_view(flow.scope.consistent_view())
    if data_type == "float16":
        func_config.enable_auto_mixed_precision(True)
        dtype = flow.float
        np_dtype = np.float32
    else:
        dtype = type_name_to_flow_type[data_type]
        np_dtype = type_name_to_np_type[data_type]
    x = x.astype(np_dtype)

    func_config.default_data_type(dtype)
    if trainable:
        func_config_type = "train"
    else:
        func_config_type = "predict"

    @flow.global_function(type=func_config_type, function_config=func_config)
    def FlowJob(x_full_precision: oft.Numpy.Placeholder(x.shape, dtype=dtype)):
        with flow.scope.placement(device_type, "0:0"):
            x_full_precision += flow.get_variable(
                name="v1", shape=(1,), dtype=dtype, initializer=flow.zeros_initializer()
            )
            if data_type == "float16":
                x = flow.cast(x_full_precision, flow.float16)
            else:
                x = x_full_precision
            y = flow.layers.batch_normalization_add_relu(
                x, x, *flow_args, trainable=trainable, training=training
            )
            y = flow.cast(y, flow.float)
            if trainable:
                flow.optimizer.SGD(
                    flow.optimizer.PiecewiseConstantScheduler([], [0.001]), momentum=0
                ).minimize(y)

                flow.watch_diff(x_full_precision, test_global_storage.Setter("x_diff"))

            return y

    check_point = flow.train.CheckPoint()
    check_point.init()
    y = FlowJob(x).get().numpy()
    if trainable:
        x_diff = test_global_storage.Get("x_diff")
        return y, x_diff
    else:
        return y