# Copyright 2021 NVIDIA Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

from __future__ import division

import legate.numpy as lg

n = 20
shape = (n, n + 1, n + 2)


def test():
    for ndim in range(2, 4):
        print(f"Testing {ndim}D")
        local_shape = shape[:ndim]
        x = lg.random.random(local_shape)
        a = x.__array__()

        for dim in range(1, ndim):
            y = lg.random.random(local_shape[-dim:])
            b = y.__array__()
            print(f"  {a.shape} x {b.shape}")
            assert lg.array_equal(x + y, a + b)

        for dim in range(ndim):
            rhs_shape = list(local_shape)
            rhs_shape[dim] = 1
            y = lg.random.random(tuple(rhs_shape))
            b = y.__array__()
            print(f"  {a.shape} x {b.shape}")
            assert lg.array_equal(x + y, a + b)

        if ndim == 2:
            continue
        for dim in range(ndim):
            rhs_shape = [1] * ndim
            rhs_shape[dim] = local_shape[dim]
            y = lg.random.random(tuple(rhs_shape))
            b = y.__array__()
            print(f"  {a.shape} x {b.shape}")
            assert lg.array_equal(x + y, a + b)


if __name__ == "__main__":
    test()
