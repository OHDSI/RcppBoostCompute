
#include <vector>
#include <algorithm>

#define BOOST_COMPUTE_DEBUG_KERNEL_COMPILATION
#define MAS_DEBUG

#define CL_API_SUFFIX__VERSION_1_0

#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/algorithm/transform_reduce.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/functional/math.hpp>
#include <boost/compute/core.hpp>

#include <Rcpp.h>

using namespace Rcpp;

//class BoostComputeEnvironment {
//public:
//    namespace compute = boost::compute;
//    //using namespace compute;
//
//    BoostComputeEnvironment() { }
//
//    compute::device& getDevice() { return device; }
//    const compute::device& getDevice() const { return device; }
//
//    compute::context getContext() { return context; }
//    const compute::context getContext() const { return context; }
//
//    compute::command_queue& getQueue() { return queue; }
//
//private:
//    compute::device& device;
//    compute::context& context;
//    compute::command_queue& queue;
//};

// [[Rcpp::export]]
void compute_hello_world() {

    namespace compute = boost::compute;

    compute::device device = compute::system::default_device();
    Rcpp::Rcout << "hello from " << device.name() << std::endl;
    Rcpp::Rcout << std::endl;
    for (const auto& device : compute::system::devices()) {
        Rcpp::Rcout << device.name() << std::endl;
    }
    Rcpp::Rcout << std::endl;
    for(const auto &platform : boost::compute::system::platforms()) {
         Rcpp::Rcout << platform.name() << std::endl;
     }
}


// [[Rcpp::export]]
List getBoostComputeEnvironment() {

    namespace compute = boost::compute;

    // get default device and setup context
    compute::device* device = new compute::device(compute::system::default_device());
    compute::context* context = new compute::context(*device);
    compute::command_queue* queue = new compute::command_queue(*context, *device);

    List result = List::create(
        Named("device") = XPtr<compute::device>(device),
        Named("context") = XPtr<compute::context>(context),
        Named("queue") = XPtr<compute::command_queue>(queue));
    // TODO Set S3 class
    return result;
}

//namespace Rcpp {
//
//    template <> BoostComputeEnvironment as(SEXP x) {
////    S4 d_x(x);
////    if (!d_x.is("dgCMatrix")) {
////        stop("Need S4 class dgCMatrix for a sparse matrix");
////    }
//
//	    return BoostComputeEnvironment();
//    }
//}

// [[Rcpp::export]]
List getProgramCache(SEXP sexpContext) {
    namespace compute = boost::compute;

    XPtr<compute::context> pContext(sexpContext);
    auto cache = compute::detail::get_program_cache(*pContext);
//    if (cache->size() == 0) {
//        return
//    }
    List result(cache->size());

    size_t i = 0;
    for (auto key : cache->get_keys()) {
        result[i] = cache->get(key).source();
        ++i;
    }

//    List result = List::create(
//        Named("size") = cache->size()
//    );
    return result;
}

template <typename InType, typename OutType>
std::vector<OutType>* makeHostVector(const std::vector<InType>& in) {

    std::vector<OutType>* pTmp = new std::vector<OutType>();

    (*pTmp).resize(in.size());
    std::copy(in.begin(), in.end(), (*pTmp).begin());

    return pTmp;
}

template <typename InType, typename OutType>
boost::compute::vector<OutType>* makeDeviceVector(const std::vector<InType>& in,
        boost::compute::context& context,
        boost::compute::command_queue& queue) {
    namespace compute = boost::compute;

    std::vector<OutType> tmp;
    tmp.resize(in.size());
    std::copy(in.begin(), in.end(), tmp.begin());

    compute::vector<OutType>* pDeviceVector = new compute::vector<OutType>(
        in.size(), context);
    compute::copy(
        tmp.begin(), tmp.end(), (*pDeviceVector).begin(), queue);
    return pDeviceVector;
}

typedef float DeviceType;

// [[Rcpp::export]]
SEXP copyToDevice(SEXP sexpContext, SEXP sexpQueue, const std::vector<int>& rVector) {

    namespace compute = boost::compute;

    XPtr<compute::context> pContext(sexpContext);
    XPtr<compute::command_queue> pQueue(sexpQueue);

    auto pDeviceVector = makeDeviceVector<int, DeviceType>(rVector, *pContext, *pQueue);

    return XPtr<compute::vector<DeviceType>>(pDeviceVector);
}

// [[Rcpp::export]]
SEXP copyToHost(const std::vector<int>& rVector) {

    auto pHostVector = makeHostVector<int, DeviceType>(rVector);
    return XPtr<std::vector<DeviceType>>(pHostVector);
}

// [[Rcpp::export]]
double simpleTransformationReduction(SEXP sexpContext, SEXP sexpQueue, SEXP sexpDeviceVector) {

    namespace compute = boost::compute;

    XPtr<compute::context> pContext(sexpContext);
    XPtr<compute::command_queue> pQueue(sexpQueue);
    XPtr<compute::vector<DeviceType> > pDeviceVector(sexpDeviceVector);

    BOOST_COMPUTE_FUNCTION(float, handRolledExp, (const float x), {
        return exp(x);
    });


    DeviceType result;
    // calculate the square-root of each element in-place
    compute::transform_reduce(
//    compute::reduce(
        (*pDeviceVector).begin(),
        (*pDeviceVector).end(),
        &result,
        //compute::exp<DeviceType>(),
        handRolledExp,
        compute::plus<DeviceType>(),
        (*pQueue)
    );

    return static_cast<double>(result);
}



// [[Rcpp::export]]
double simpleTransformationReductionWithCopy(SEXP sexpContext, SEXP sexpQueue,
         const std::vector<int>& rVector) {

    namespace compute = boost::compute;

    XPtr<compute::context> pContext(sexpContext);
    XPtr<compute::command_queue> pQueue(sexpQueue);

    // create a vector on the device
    compute::vector<DeviceType> deviceVector(rVector.size(), (*pContext));

    // transfer data from the host to the device
    compute::copy(
        rVector.begin(), rVector.end(), deviceVector.begin(), (*pQueue)
    );

    DeviceType result;
    // calculate the square-root of each element in-place
    compute::transform_reduce(
        deviceVector.begin(),
        deviceVector.end(),
        &result,
        compute::abs<DeviceType>(),
        compute::plus<DeviceType>(),
        (*pQueue)
    );

    return result;
}

// [[Rcpp::export]]
double simpleTransformationReductionNoParallel(SEXP sexpHostVector) {

    XPtr<std::vector<DeviceType> > pHostVector(sexpHostVector);

    double result = 0.0;
    // calculate the square-root of each element in-place
    const auto end = (*pHostVector).end();
    for (auto it = (*pHostVector).begin(); it != end; ++it) {
        result += exp(*it);
    }
    return result;
}
