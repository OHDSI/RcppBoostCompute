//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://kylelutz.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_ALGORITHM_MERGE_HPP
#define BOOST_COMPUTE_ALGORITHM_MERGE_HPP

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/detail/serial_merge.hpp>

namespace boost {
namespace compute {

/// Merges the sorted values in the range [\p first1, \p last1) with
/// the sorted values in the range [\p first2, last2) and stores the
/// result in the range beginning at \p result.
template<class InputIterator1, class InputIterator2, class OutputIterator>
inline OutputIterator merge(InputIterator1 first1,
                            InputIterator1 last1,
                            InputIterator2 first2,
                            InputIterator2 last2,
                            OutputIterator result,
                            command_queue &queue = system::default_queue())
{
    typedef typename std::iterator_traits<InputIterator1>::value_type T1;

    return merge(first1, last1, first2, last2, result, less<T1>(), queue);
}

/// Merges the sorted values in the range [\p first1, \p last1) with
/// the sorted values in the range [\p first2, last2) and stores the
/// result in the range beginning at \p result. Values are compared
/// using the \p comp function.
template<class InputIterator1,
         class InputIterator2,
         class OutputIterator,
         class Compare>
inline OutputIterator merge(InputIterator1 first1,
                            InputIterator1 last1,
                            InputIterator2 first2,
                            InputIterator2 last2,
                            OutputIterator result,
                            Compare comp,
                            command_queue &queue = system::default_queue())
{
    size_t size1 = detail::iterator_range_size(first1, last1);
    size_t size2 = detail::iterator_range_size(first2, last2);

    // handle trivial cases
    if(size1 == 0 && size2 == 0){
        return result;
    }
    else if(size1 == 0){
        return ::boost::compute::copy(first2, last2, result, queue);
    }
    else if(size2 == 0){
        return ::boost::compute::copy(first1, last1, result, queue);
    }

    return detail::serial_merge(
        first1, last1, first2, last2, result, comp, queue
    );
}

} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_ALGORITHM_MERGE_HPP
