//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://kylelutz.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_ALGORITHM_DETAIL_RADIX_SORT_HPP
#define BOOST_COMPUTE_ALGORITHM_DETAIL_RADIX_SORT_HPP

#include <iterator>

#include <boost/assert.hpp>
#include <boost/type_traits/is_signed.hpp>
#include <boost/type_traits/is_floating_point.hpp>

#include <boost/compute/kernel.hpp>
#include <boost/compute/program.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/exclusive_scan.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/type_traits/type_name.hpp>
#include <boost/compute/detail/iterator_range_size.hpp>
#include <boost/compute/detail/program_cache.hpp>

namespace boost {
namespace compute {
namespace detail {

template<size_t N>
struct radix_sort_value_type
{
};

template<>
struct radix_sort_value_type<1>
{
    typedef uchar_ type;
};

template<>
struct radix_sort_value_type<2>
{
    typedef ushort_ type;
};

template<>
struct radix_sort_value_type<4>
{
    typedef uint_ type;
};

template<>
struct radix_sort_value_type<8>
{
    typedef ulong_ type;
};

const char radix_sort_source[] =
"#define K2_BITS (1 << K_BITS)\n"
"#define RADIX_MASK ((((T)(1)) << K_BITS) - 1)\n"
"#define SIGN_BIT ((sizeof(T) * CHAR_BIT) - 1)\n"

"inline uint radix(const T x, const uint low_bit)\n"
"{\n"
"#if defined(IS_FLOATING_POINT)\n"
"    const T mask = -(x >> SIGN_BIT) | (((T)(1)) << SIGN_BIT);\n"
"    return ((x ^ mask) >> low_bit) & RADIX_MASK;\n"
"#elif defined(IS_SIGNED)\n"
"    return ((x ^ (((T)(1)) << SIGN_BIT)) >> low_bit) & RADIX_MASK;\n"
"#else\n"
"    return (x >> low_bit) & RADIX_MASK;\n"
"#endif\n"
"}\n"

"__kernel void count(__global const T *input,\n"
"                    const uint input_size,\n"
"                    __global uint *global_counts,\n"
"                    __global uint *global_offsets,\n"
"                    __local uint *local_counts,\n"
"                    const uint low_bit)\n"
"{\n"
     // work-item parameters
"    const uint gid = get_global_id(0);\n"
"    const uint lid = get_local_id(0);\n"

     // zero local counts
"    if(lid < K2_BITS){\n"
"        local_counts[lid] = 0;\n"
"    }\n"
"    barrier(CLK_LOCAL_MEM_FENCE);\n"

     // reduce local counts
"    if(gid < input_size){\n"
"        T value = input[gid];\n"
"        uint bucket = radix(value, low_bit);\n"
"        atomic_inc(local_counts + bucket);\n"
"    }\n"
"    barrier(CLK_LOCAL_MEM_FENCE);\n"

     // write block-relative offsets
"    if(lid < K2_BITS){\n"
"        global_counts[K2_BITS*get_group_id(0) + lid] = local_counts[lid];\n"

         // write global offsets
"        if(get_group_id(0) == (get_num_groups(0) - 1)){\n"
"            global_offsets[lid] = local_counts[lid];\n"
"        }\n"
"    }\n"
"}\n"

"__kernel void scan(__global const uint *block_offsets,\n"
"                   __global uint *global_offsets,\n"
"                   const uint block_count)\n"
"{\n"
"    __global const uint *last_block_offsets =\n"
"        block_offsets + K2_BITS * (block_count - 1);\n"

     // calculate and scan global_offsets
"    uint sum = 0;\n"
"    for(uint i = 0; i < K2_BITS; i++){\n"
"        uint x = global_offsets[i] + last_block_offsets[i];\n"
"        global_offsets[i] = sum;\n"
"        sum += x;\n"
"    }\n"
"}\n"

"__kernel void scatter(__global const T *input,\n"
"                      const uint input_size,\n"
"                      const uint low_bit,\n"
"                      __global const uint *counts,\n"
"                      __global const uint *global_offsets,\n"
"#ifndef SORT_BY_KEY\n"
"                      __global T *output)\n"
"#else\n"
"                      __global T *keys_output,\n"
"                      __global T2 *values_input,\n"
"                      __global T2 *values_output)\n"
"#endif\n"
"{\n"
     // work-item parameters
"    const uint gid = get_global_id(0);\n"
"    const uint lid = get_local_id(0);\n"

     // copy input to local memory
"    T value;\n"
"    uint bucket;\n"
"    __local uint local_input[BLOCK_SIZE];\n"
"    if(gid < input_size){\n"
"        value = input[gid];\n"
"        bucket = radix(value, low_bit);\n"
"        local_input[lid] = bucket;\n"
"    }\n"

     // copy block counts to local memory
"    __local uint local_counts[(1 << K_BITS)];\n"
"    if(lid < K2_BITS){\n"
"        local_counts[lid] = counts[get_group_id(0) * K2_BITS + lid];\n"
"    }\n"

     // wait until local memory is ready
"    barrier(CLK_LOCAL_MEM_FENCE);\n"

"    if(gid >= input_size){\n"
"        return;\n"
"    }\n"

     // get global offset
"    uint offset = global_offsets[bucket] + local_counts[bucket];\n"

     // calculate local offset
"    uint local_offset = 0;\n"
"    for(uint i = 0; i < lid; i++){\n"
"        if(local_input[i] == bucket)\n"
"            local_offset++;\n"
"    }\n"

"#ifndef SORT_BY_KEY\n"
     // write value to output
"    output[offset + local_offset] = value;\n"
"#else\n"
     // write key and value if doing sort_by_key
"    keys_output[offset + local_offset] = value;\n"
"    values_output[offset + local_offset] = values_input[gid];\n"
"#endif\n"
"}\n";

template<class T, class T2>
inline void radix_sort_impl(const buffer_iterator<T> first,
                            const buffer_iterator<T> last,
                            const buffer_iterator<T2> values_first,
                            command_queue &queue)
{
    typedef T value_type;
    typedef typename radix_sort_value_type<sizeof(T)>::type sort_type;

    const context &context = queue.get_context();

    boost::shared_ptr<program_cache> cache =
        detail::get_program_cache(context);

    size_t count = detail::iterator_range_size(first, last);

    // sort parameters
    const uint_ k = 4;
    const uint_ k2 = 1 << k;
    const uint_ block_size = 128;

    uint_ block_count = static_cast<uint_>(count / block_size);
    if(block_count * block_size != count){
        block_count++;
    }

    // if we have a valid values iterator then we are doing a
    // sort by key and have to set up the values buffer
    bool sort_by_key = (values_first.get_buffer().get() != 0);

    // load (or create) radix sort program
    std::string cache_key =
        std::string("radix_sort_") + type_name<value_type>();

    if(sort_by_key){
        cache_key += std::string("_with_") + type_name<T2>();
    }

    program radix_sort_program = cache->get(cache_key);

    if(!radix_sort_program.get()){
        std::stringstream options;
        options << "-DK_BITS=" << k;
        options << " -DT=" << type_name<sort_type>();
        options << " -DBLOCK_SIZE=" << block_size;

        if(boost::is_floating_point<value_type>::value){
            options << " -DIS_FLOATING_POINT";
        }

        if(boost::is_signed<value_type>::value){
            options << " -DIS_SIGNED";
        }

        if(sort_by_key){
            options << " -DSORT_BY_KEY";
            options << " -DT2=" << type_name<T2>();
        }

        radix_sort_program =
            program::build_with_source(radix_sort_source, context, options.str());

        cache->insert(cache_key, radix_sort_program);
    }

    kernel count_kernel(radix_sort_program, "count");
    kernel scan_kernel(radix_sort_program, "scan");
    kernel scatter_kernel(radix_sort_program, "scatter");

    // setup temporary buffers
    vector<value_type> output(count, context);
    vector<T2> values_output(sort_by_key ? count : 0, context);
    vector<uint_> offsets(k2, context);
    vector<uint_> counts(block_count * k2, context);

    const buffer *input_buffer = &first.get_buffer();
    const buffer *output_buffer = &output.get_buffer();
    const buffer *values_input_buffer = &values_first.get_buffer();
    const buffer *values_output_buffer = &values_output.get_buffer();

    for(uint_ i = 0; i < sizeof(sort_type) * CHAR_BIT / k; i++){
        // write counts
        count_kernel.set_arg(0, *input_buffer);
        count_kernel.set_arg(1, static_cast<uint_>(count));
        count_kernel.set_arg(2, counts);
        count_kernel.set_arg(3, offsets);
        count_kernel.set_arg(4, block_size * sizeof(uint_), 0);
        count_kernel.set_arg(5, i * k);
        queue.enqueue_1d_range_kernel(count_kernel,
                                      0,
                                      block_count * block_size,
                                      block_size);

        // scan counts
        if(k == 1){
            typedef uint2_ counter_type;
            ::boost::compute::exclusive_scan(
                make_buffer_iterator<counter_type>(counts.get_buffer(), 0),
                make_buffer_iterator<counter_type>(counts.get_buffer(), counts.size() / 2),
                make_buffer_iterator<counter_type>(counts.get_buffer()),
                queue
            );
        }
        else if(k == 2){
            typedef uint4_ counter_type;
            ::boost::compute::exclusive_scan(
                make_buffer_iterator<counter_type>(counts.get_buffer(), 0),
                make_buffer_iterator<counter_type>(counts.get_buffer(), counts.size() / 4),
                make_buffer_iterator<counter_type>(counts.get_buffer()),
                queue
            );
        }
        else if(k == 4){
            typedef uint16_ counter_type;
            ::boost::compute::exclusive_scan(
                make_buffer_iterator<counter_type>(counts.get_buffer(), 0),
                make_buffer_iterator<counter_type>(counts.get_buffer(), counts.size() / 16),
                make_buffer_iterator<counter_type>(counts.get_buffer()),
                queue
            );
        }
        else {
            BOOST_ASSERT(false && "unknown k");
            break;
        }

        // scan global offsets
        scan_kernel.set_arg(0, counts);
        scan_kernel.set_arg(1, offsets);
        scan_kernel.set_arg(2, block_count);
        queue.enqueue_task(scan_kernel);

        // scatter values
        scatter_kernel.set_arg(0, *input_buffer);
        scatter_kernel.set_arg(1, static_cast<uint_>(count));
        scatter_kernel.set_arg(2, i * k);
        scatter_kernel.set_arg(3, counts);
        scatter_kernel.set_arg(4, offsets);
        scatter_kernel.set_arg(5, *output_buffer);
        if(sort_by_key){
            scatter_kernel.set_arg(6, *values_input_buffer);
            scatter_kernel.set_arg(7, *values_output_buffer);
        }
        queue.enqueue_1d_range_kernel(scatter_kernel,
                                      0,
                                      block_count * block_size,
                                      block_size);

        // swap buffers
        std::swap(input_buffer, output_buffer);
        std::swap(values_input_buffer, values_output_buffer);
    }
}

template<class Iterator>
inline void radix_sort(Iterator first,
                       Iterator last,
                       command_queue &queue)
{
    radix_sort_impl(first, last, buffer_iterator<int>(), queue);
}

template<class KeyIterator, class ValueIterator>
inline void radix_sort_by_key(KeyIterator keys_first,
                              KeyIterator keys_last,
                              ValueIterator values_first,
                              command_queue &queue)
{
    radix_sort_impl(keys_first, keys_last, values_first, queue);
}

} // end detail namespace
} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_ALGORITHM_DETAIL_RADIX_SORT_HPP
