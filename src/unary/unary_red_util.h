/* Copyright 2021 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#pragma once

#include "numpy.h"
#include "deserializer.h"
#include "dispatch.h"
#include "scalar.h"

namespace legate {
namespace numpy {

enum class UnaryRedCode : int {
  MAX  = 1,
  MIN  = 2,
  PROD = 3,
  SUM  = 4,
};

void deserialize(Deserializer &ctx, UnaryRedCode &code);

template <typename Functor, typename... Fnargs>
constexpr decltype(auto) op_dispatch(UnaryRedCode op_code, Functor f, Fnargs &&... args)
{
  switch (op_code) {
    case UnaryRedCode::MAX:
      return f.template operator()<UnaryRedCode::MAX>(std::forward<Fnargs>(args)...);
    case UnaryRedCode::MIN:
      return f.template operator()<UnaryRedCode::MIN>(std::forward<Fnargs>(args)...);
    case UnaryRedCode::PROD:
      return f.template operator()<UnaryRedCode::PROD>(std::forward<Fnargs>(args)...);
    case UnaryRedCode::SUM:
      return f.template operator()<UnaryRedCode::SUM>(std::forward<Fnargs>(args)...);
  }
  assert(false);
  return f.template operator()<UnaryRedCode::MAX>(std::forward<Fnargs>(args)...);
}

template <UnaryRedCode OP_CODE, LegateTypeCode TYPE_CODE>
struct UnaryRedOp {
  static constexpr bool valid = false;
};

template <LegateTypeCode TYPE_CODE>
struct UnaryRedOp<UnaryRedCode::MAX, TYPE_CODE> {
  static constexpr bool valid = true;

  using VAL = legate_type_of<TYPE_CODE>;
  using OP  = Legion::MaxReduction<VAL>;

  static const VAL identity;

  template <bool EXCLUSIVE>
  __CUDA_HD__ static void fold(VAL &rhs1, VAL rhs2)
  {
    OP::template fold<EXCLUSIVE>(rhs1, rhs2);
  }
};

template <>
struct UnaryRedOp<UnaryRedCode::MAX, LegateTypeCode::COMPLEX128_LT> {
  static constexpr bool valid = false;
};

template <LegateTypeCode TYPE_CODE>
struct UnaryRedOp<UnaryRedCode::MIN, TYPE_CODE> {
  static constexpr bool valid = true;

  using VAL = legate_type_of<TYPE_CODE>;
  using OP  = Legion::MinReduction<VAL>;

  static const VAL identity;

  template <bool EXCLUSIVE>
  __CUDA_HD__ static void fold(VAL &rhs1, VAL rhs2)
  {
    OP::template fold<EXCLUSIVE>(rhs1, rhs2);
  }
};

template <>
struct UnaryRedOp<UnaryRedCode::MIN, LegateTypeCode::COMPLEX128_LT> {
  static constexpr bool valid = false;
};

template <LegateTypeCode TYPE_CODE>
struct UnaryRedOp<UnaryRedCode::PROD, TYPE_CODE> {
  static constexpr bool valid = true;

  using VAL = legate_type_of<TYPE_CODE>;
  using OP  = Legion::ProdReduction<VAL>;

  static const VAL identity;

  template <bool EXCLUSIVE>
  __CUDA_HD__ static void fold(VAL &rhs1, VAL rhs2)
  {
    OP::template fold<EXCLUSIVE>(rhs1, rhs2);
  }
};

template <>
struct UnaryRedOp<UnaryRedCode::PROD, LegateTypeCode::COMPLEX128_LT> {
  static constexpr bool valid = false;
};

template <LegateTypeCode TYPE_CODE>
struct UnaryRedOp<UnaryRedCode::SUM, TYPE_CODE> {
  static constexpr bool valid = true;

  using VAL = legate_type_of<TYPE_CODE>;
  using OP  = Legion::SumReduction<VAL>;

  static const VAL identity;

  template <bool EXCLUSIVE>
  __CUDA_HD__ static void fold(VAL &rhs1, VAL rhs2)
  {
    OP::template fold<EXCLUSIVE>(rhs1, rhs2);
  }
};

template <UnaryRedCode OP_CODE>
struct UntypedScalarRedOp {
  using LHS = UntypedScalar;
  using RHS = UntypedScalar;

  static const UntypedScalar identity;

  template <bool EXCLUSIVE>
  struct apply_fn {
    template <LegateTypeCode TYPE_CODE,
              std::enable_if_t<UnaryRedOp<OP_CODE, TYPE_CODE>::valid> * = nullptr>
    void operator()(void *lhs, void *rhs)
    {
      using VAL = legate_type_of<TYPE_CODE>;
      UnaryRedOp<OP_CODE, TYPE_CODE>::OP::template apply<EXCLUSIVE>(*static_cast<VAL *>(lhs),
                                                                    *static_cast<VAL *>(rhs));
    }

    template <LegateTypeCode TYPE_CODE,
              std::enable_if_t<!UnaryRedOp<OP_CODE, TYPE_CODE>::valid> * = nullptr>
    void operator()(void *lhs, void *rhs)
    {
      assert(false);
    }
  };

  template <bool EXCLUSIVE>
  struct fold_fn {
    template <LegateTypeCode TYPE_CODE,
              std::enable_if_t<UnaryRedOp<OP_CODE, TYPE_CODE>::valid> * = nullptr>
    void operator()(void *lhs, void *rhs)
    {
      using VAL = legate_type_of<TYPE_CODE>;
      UnaryRedOp<OP_CODE, TYPE_CODE>::OP::template fold<EXCLUSIVE>(*static_cast<VAL *>(lhs),
                                                                   *static_cast<VAL *>(rhs));
    }

    template <LegateTypeCode TYPE_CODE,
              std::enable_if_t<!UnaryRedOp<OP_CODE, TYPE_CODE>::valid> * = nullptr>
    void operator()(void *lhs, void *rhs)
    {
      assert(false);
    }
  };

  template <typename Fn>
  struct dispatch_fn {
    void operator()(LegateTypeCode code, void *lhs, void *rhs)
    {
      type_dispatch(code, Fn{}, lhs, rhs);
    }
  };

  template <bool EXCLUSIVE>
  static void apply(LHS &lhs, RHS rhs)
  {
    assert(EXCLUSIVE);
    if (LegateTypeCode::MAX_TYPE_NUMBER == lhs.code())
      lhs = rhs;
    else
      type_dispatch(lhs.code(), apply_fn<EXCLUSIVE>{}, lhs.ptr(), rhs.ptr());
  }

  template <bool EXCLUSIVE>
  static void fold(RHS &rhs1, RHS rhs2)
  {
    assert(EXCLUSIVE);
    if (LegateTypeCode::MAX_TYPE_NUMBER == rhs1.code())
      rhs1 = rhs2;
    else
      type_dispatch(rhs1.code(), fold_fn<EXCLUSIVE>{}, rhs1.ptr(), rhs2.ptr());
  }
};

}  // namespace numpy
}  // namespace legate