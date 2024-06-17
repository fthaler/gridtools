/*
 * GridTools
 *
 * Copyright (c) 2014-2023, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <utility>

#include "../../common/cuda_util.hpp"
#include "../../common/hymap.hpp"
#include "../../meta.hpp"
#include "../../sid/allocator.hpp"
#include "../../sid/concept.hpp"
#include "../../sid/contiguous.hpp"
#include "../../sid/multi_shift.hpp"
#include "../../sid/unknown_kind.hpp"
#include "./common.hpp"

namespace gridtools::fn::backend {
    namespace gpu_impl_ {
        /*
         * ThreadBlockSizes must be a meta map, mapping dimensions to integral constant block sizes.
         *
         * For example, meta::list<meta::list<dim::i, integral_constant<int, 32>>,
         *                         meta::list<dim::j, integral_constant<int, 8>>,
         *                         meta::list<dim::k, integral_constant<int, 1>>>;
         * When using a cartesian grid.
         */
        template <class ThreadBlockSizes,
            class LoopBlockSizes = meta::transpose<meta::list<meta::first<meta::transpose<ThreadBlockSizes>>,
                meta::repeat<meta::length<ThreadBlockSizes>, meta::list<integral_constant<int, 1>>>>>>
        struct gpu {
            using thread_block_sizes_t = ThreadBlockSizes;
            using loop_block_sizes_t = LoopBlockSizes;
            cudaStream_t stream = 0;
        };

        template <class BlockSizes, class Dims, int I>
        using block_size_at_dim = meta::second<meta::mp_find<BlockSizes, meta::at_c<Dims, I>>>;

        template <class ThreadBlockSizes, class LoopBlockSizes, class Sizes>
        GT_FUNCTION_DEVICE auto global_thread_index(Sizes const &sizes) {
            using all_keys_t = get_keys<Sizes>;
            using ndims_t = meta::length<all_keys_t>;
            using keys_t = meta::rename<hymap::keys, meta::take_c<std::min(3, (int)ndims_t::value), all_keys_t>>;
            if constexpr (ndims_t::value == 0) {
                return std::make_tuple(hymap::keys<>::values<>(), hymap::keys<>::values<>());
            } else if constexpr (ndims_t::value == 1) {
                using thread_block_dim_x = block_size_at_dim<ThreadBlockSizes, keys_t, 0>;
                using loop_block_dim_x = block_size_at_dim<LoopBlockSizes, keys_t, 0>;
                using values_t = typename keys_t::template values<int>;
                int thread_idx_x = blockIdx.x * (thread_block_dim_x::value * loop_block_dim_x::value) +
                                   threadIdx.x * loop_block_dim_x::value;
                int block_size_x = tuple_util::get<0>(sizes) - thread_idx_x;
                if (block_size_x > loop_block_dim_x::value)
                    block_size_x = loop_block_dim_x::value;
                if (block_size_x < 0)
                    block_size_x = 0;
                return std::make_tuple(values_t(thread_idx_x), values_t(block_size_x));
            } else if constexpr (ndims_t::value == 2) {
                using thread_block_dim_x = block_size_at_dim<ThreadBlockSizes, keys_t, 0>;
                using thread_block_dim_y = block_size_at_dim<ThreadBlockSizes, keys_t, 1>;
                using loop_block_dim_x = block_size_at_dim<LoopBlockSizes, keys_t, 0>;
                using loop_block_dim_y = block_size_at_dim<LoopBlockSizes, keys_t, 1>;
                using values_t = typename keys_t::template values<int, int>;
                int thread_idx_x = blockIdx.x * (thread_block_dim_x::value * loop_block_dim_x::value) +
                                   threadIdx.x * loop_block_dim_x::value;
                int thread_idx_y = blockIdx.y * (thread_block_dim_y::value * loop_block_dim_y::value) +
                                   threadIdx.y * loop_block_dim_y::value;
                int block_size_x = tuple_util::get<0>(sizes) - thread_idx_x;
                int block_size_y = tuple_util::get<1>(sizes) - thread_idx_y;
                if (block_size_x > loop_block_dim_x::value)
                    block_size_x = loop_block_dim_x::value;
                if (block_size_x < 0)
                    block_size_x = 0;
                if (block_size_y > loop_block_dim_y::value)
                    block_size_y = loop_block_dim_y::value;
                if (block_size_y < 0)
                    block_size_y = 0;
                return std::make_tuple(values_t(thread_idx_x, thread_idx_y), values_t(block_size_x, block_size_y));
            } else {
                using thread_block_dim_x = block_size_at_dim<ThreadBlockSizes, keys_t, 0>;
                using thread_block_dim_y = block_size_at_dim<ThreadBlockSizes, keys_t, 1>;
                using thread_block_dim_z = block_size_at_dim<ThreadBlockSizes, keys_t, 2>;
                using loop_block_dim_x = block_size_at_dim<LoopBlockSizes, keys_t, 0>;
                using loop_block_dim_y = block_size_at_dim<LoopBlockSizes, keys_t, 1>;
                using loop_block_dim_z = block_size_at_dim<LoopBlockSizes, keys_t, 2>;
                using values_t = typename keys_t::template values<int, int, int>;
                int thread_idx_x = blockIdx.x * (thread_block_dim_x::value * loop_block_dim_x::value) +
                                   threadIdx.x * loop_block_dim_x::value;
                int thread_idx_y = blockIdx.y * (thread_block_dim_y::value * loop_block_dim_y::value) +
                                   threadIdx.y * loop_block_dim_y::value;
                int thread_idx_z = blockIdx.z * (thread_block_dim_z::value * loop_block_dim_z::value) +
                                   threadIdx.z * loop_block_dim_z::value;
                int block_size_x = tuple_util::get<0>(sizes) - thread_idx_x;
                int block_size_y = tuple_util::get<1>(sizes) - thread_idx_y;
                int block_size_z = tuple_util::get<2>(sizes) - thread_idx_z;
                if (block_size_x > loop_block_dim_x::value)
                    block_size_x = loop_block_dim_x::value;
                if (block_size_x < 0)
                    block_size_x = 0;
                if (block_size_y > loop_block_dim_y::value)
                    block_size_y = loop_block_dim_y::value;
                if (block_size_y < 0)
                    block_size_y = 0;
                if (block_size_z > loop_block_dim_z::value)
                    block_size_z = loop_block_dim_z::value;
                if (block_size_z < 0)
                    block_size_z = 0;
                return std::make_tuple(values_t(thread_idx_x, thread_idx_y, thread_idx_z),
                    values_t(block_size_x, block_size_y, block_size_z));
            }
            // disable incorrect warning "missing return statement at end of non-void function"
            GT_NVCC_DIAG_PUSH_SUPPRESS(940)
        }
        GT_NVCC_DIAG_POP_SUPPRESS(940)

        template <class Key>
        struct at_generator_f {
            template <class Value>
            GT_FUNCTION_DEVICE decltype(auto) operator()(Value &&value) const {
                return device::at_key<Key>(std::forward<Value>(value));
            }
        };

        template <class Index, class Sizes>
        GT_FUNCTION_DEVICE bool in_domain(Index const &index, Sizes const &sizes) {
            using sizes_t = meta::rename<tuple, Index>;
            using generators_t = meta::transform<at_generator_f, get_keys<Index>>;
            auto indexed_sizes = tuple_util::device::generate<generators_t, sizes_t>(sizes);
            return tuple_util::device::all_of(std::less(), index, indexed_sizes);
        }

        template <class ThreadBlockSizes,
            class LoopBlockSizes,
            class Sizes,
            class PtrHolder,
            class Strides,
            class Fun,
            class NDims = tuple_util::size<Sizes>,
            class SizeKeys = get_keys<Sizes>>
        __global__ void kernel(Sizes sizes, PtrHolder ptr_holder, Strides strides, Fun fun) {
            auto const [thread_idx, block_size] = global_thread_index<ThreadBlockSizes, LoopBlockSizes>(sizes);
            if (!in_domain(thread_idx, sizes))
                return;
            auto ptr = ptr_holder();
            sid::multi_shift(ptr, strides, thread_idx);
            if constexpr (NDims::value <= 3) {
                common::make_loops(block_size)(std::move(fun))(ptr, strides);
            } else {
                auto inner_sizes = tuple_util::device::convert_to<meta::drop_front_c<3, SizeKeys>::values>(
                    tuple_util::device::drop_front<3>(tuple_util::device::convert_to<std::tuple>(sizes)));
                auto loop_sizes = hymap::concat(block_size, inner_sizes);
                common::make_loops(loop_sizes)(std::move(fun))(ptr, strides);
            }
        }

        template <class ThreadBlockSizes, class LoopBlockSizes, class Sizes>
        std::tuple<dim3, dim3> blocks_and_threads(Sizes const &sizes) {
            using keys_t = get_keys<Sizes>;
            using ndims_t = meta::length<keys_t>;
            dim3 blocks(1, 1, 1);
            dim3 threads(1, 1, 1);
            if constexpr (ndims_t::value >= 1) {
                threads.x = block_size_at_dim<ThreadBlockSizes, keys_t, 0>();
                constexpr int block_dim_x = block_size_at_dim<ThreadBlockSizes, keys_t, 0>::value *
                                            block_size_at_dim<LoopBlockSizes, keys_t, 0>::value;
                blocks.x = (tuple_util::get<0>(sizes) + block_dim_x - 1) / block_dim_x;
            }
            if constexpr (ndims_t::value >= 2) {
                threads.y = block_size_at_dim<ThreadBlockSizes, keys_t, 1>();
                constexpr int block_dim_y = block_size_at_dim<ThreadBlockSizes, keys_t, 1>::value *
                                            block_size_at_dim<LoopBlockSizes, keys_t, 1>::value;
                blocks.y = (tuple_util::get<1>(sizes) + block_dim_y - 1) / block_dim_y;
            }
            if constexpr (ndims_t::value >= 3) {
                threads.z = block_size_at_dim<ThreadBlockSizes, keys_t, 2>();
                constexpr int block_dim_z = block_size_at_dim<ThreadBlockSizes, keys_t, 2>::value *
                                            block_size_at_dim<LoopBlockSizes, keys_t, 2>::value;
                blocks.z = (tuple_util::get<2>(sizes) + block_dim_z - 1) / block_dim_z;
            }
            return {blocks, threads};
        }

        template <class StencilStage, class MakeIterator>
        struct stencil_fun_f {
            MakeIterator m_make_iterator;

            template <class Ptr, class Strides>
            GT_FUNCTION_DEVICE void operator()(Ptr &ptr, Strides const &strides) const {
                StencilStage()(m_make_iterator(), ptr, strides);
            }
        };

        template <class Sizes>
        bool is_domain_empty(const Sizes &sizes) {
            return tuple_util::host::apply([](auto... sizes) { return ((sizes == 0) || ...); }, sizes);
        }

        template <class ThreadBlockSizes,
            class LoopBlockSizes,
            class Sizes,
            class StencilStage,
            class MakeIterator,
            class Composite>
        void apply_stencil_stage(gpu<ThreadBlockSizes, LoopBlockSizes> const &g,
            Sizes const &sizes,
            StencilStage,
            MakeIterator make_iterator,
            Composite &&composite) {

            if (is_domain_empty(sizes)) {
                return;
            }

            auto ptr_holder = sid::get_origin(std::forward<Composite>(composite));
            auto strides = sid::get_strides(std::forward<Composite>(composite));

            auto [blocks, threads] = blocks_and_threads<ThreadBlockSizes, LoopBlockSizes>(sizes);
            assert(threads.x > 0 && threads.y > 0 && threads.z > 0);
            cuda_util::launch(blocks,
                threads,
                0,
                g.stream,
                kernel<ThreadBlockSizes,
                    LoopBlockSizes,
                    Sizes,
                    decltype(ptr_holder),
                    decltype(strides),
                    stencil_fun_f<StencilStage, MakeIterator>>,
                sizes,
                ptr_holder,
                strides,
                stencil_fun_f<StencilStage, MakeIterator>{std::move(make_iterator)});
        }

        template <class ColumnStage, class MakeIterator, class Seed>
        struct column_fun_f {
            MakeIterator m_make_iterator;
            Seed m_seed;
            int m_v_size;

            template <class Ptr, class Strides>
            GT_FUNCTION_DEVICE void operator()(Ptr ptr, Strides const &strides) const {
                ColumnStage()(m_seed, m_v_size, m_make_iterator(), std::move(ptr), strides);
            }
        };

        template <class ThreadBlockSizes,
            class LoopBlockSizes,
            class Sizes,
            class ColumnStage,
            class MakeIterator,
            class Composite,
            class Vertical,
            class Seed>
        void apply_column_stage(gpu<ThreadBlockSizes, LoopBlockSizes> const &g,
            Sizes const &sizes,
            ColumnStage,
            MakeIterator make_iterator,
            Composite &&composite,
            Vertical,
            Seed seed) {

            if (is_domain_empty(sizes)) {
                return;
            }

            auto ptr_holder = sid::get_origin(std::forward<Composite>(composite));
            auto strides = sid::get_strides(std::forward<Composite>(composite));
            auto h_sizes = hymap::canonicalize_and_remove_key<Vertical>(sizes);
            int v_size = at_key<Vertical>(sizes);

            auto [blocks, threads] = blocks_and_threads<ThreadBlockSizes, LoopBlockSizes>(h_sizes);
            assert(threads.x > 0 && threads.y > 0 && threads.z > 0);
            cuda_util::launch(blocks,
                threads,
                0,
                g.stream,
                kernel<ThreadBlockSizes,
                    LoopBlockSizes,
                    decltype(h_sizes),
                    decltype(ptr_holder),
                    decltype(strides),
                    column_fun_f<ColumnStage, MakeIterator, Seed>>,
                h_sizes,
                ptr_holder,
                strides,
                column_fun_f<ColumnStage, MakeIterator, Seed>{std::move(make_iterator), std::move(seed), v_size});
        }

        template <class ThreadBlockSizes, class LoopBlockSizes>
        auto tmp_allocator(gpu<ThreadBlockSizes, LoopBlockSizes> be) {
            return std::make_tuple(be, sid::device::cached_allocator(&cuda_util::cuda_malloc<char[]>));
        }

        template <class ThreadBlockSizes, class LoopBlockSizes, class Allocator, class Sizes, class T>
        auto allocate_global_tmp(
            std::tuple<gpu<ThreadBlockSizes, LoopBlockSizes>, Allocator> &alloc, Sizes const &sizes, data_type<T>) {
            return sid::make_contiguous<T, int_t, sid::unknown_kind>(std::get<1>(alloc), sizes);
        }
    } // namespace gpu_impl_

    using gpu_impl_::gpu;

    using gpu_impl_::apply_column_stage;
    using gpu_impl_::apply_stencil_stage;

    using gpu_impl_::allocate_global_tmp;
    using gpu_impl_::tmp_allocator;
} // namespace gridtools::fn::backend
