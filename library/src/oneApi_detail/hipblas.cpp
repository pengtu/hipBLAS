//#include <hip/hip_runtime.h>
#include "deps/onemkl.h"
#include <algorithm>

#include <functional>
#include <hip/hip_interop.h>
#include <hipblas.h>
#include <exceptions.hpp>
//#include <math.h>

#include "sycl_w.h"

// local functions
static hipblasStatus_t updateSyclHandlesToCrrStream(hipStream_t stream, syclblasHandle_t handle)
{
    // Obtain the handles to the LZ handlers.
    unsigned long lzHandles[4];
    int           nHandles = 4;
    hipGetBackendNativeHandles((uintptr_t)stream, lzHandles, &nHandles);
    //Fix-Me : Should Sycl know hipStream_t??
    syclblas_set_stream(handle, lzHandles, nHandles, stream);
    return HIPBLAS_STATUS_SUCCESS;
}

// hipblas APIs
hipblasStatus_t hipblasCreate(hipblasHandle_t* handle)
try
{
    // create syclBlas
    syclblas_create((syclblasHandle_t*)handle);

    hipStream_t nullStream = NULL; // default or null stream
    // set stream to default NULL stream
    auto status = updateSyclHandlesToCrrStream(nullStream, (syclblasHandle_t)*handle);
    return status;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDestroy(hipblasHandle_t handle)
try
{
    return syclblas_destroy((syclblasHandle_t)handle);
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasSetStream(hipblasHandle_t handle, hipStream_t stream)
try
{
    return updateSyclHandlesToCrrStream(stream, (syclblasHandle_t)handle);
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasGetStream(hipblasHandle_t handle, hipStream_t* pStream)
try
{
    if(handle == nullptr)
    {
        return HIPBLAS_STATUS_NOT_INITIALIZED;
    }
    return syclblas_get_hipstream((syclblasHandle_t)handle, pStream);
}
catch(...)
{
    return exception_to_hipblas_status();
}

// atomics mode - cannot find corresponding atomics mode in oneMKL, default to ALLOWED
hipblasStatus_t hipblasGetAtomicsMode(hipblasHandle_t handle, hipblasAtomicsMode_t* atomics_mode)
try
{
    *atomics_mode = HIPBLAS_ATOMICS_ALLOWED;
    return HIPBLAS_STATUS_SUCCESS;
 }
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasSetAtomicsMode(hipblasHandle_t handle, hipblasAtomicsMode_t atomics_mode)
try
{
    // No op
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasGetInt8Datatype(hipblasHandle_t handle, hipblasInt8Datatype_t * int8Type)
try
{
    *int8Type = HIPBLAS_INT8_DATATYPE_INT8;
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasSetInt8Datatype(hipblasHandle_t handle, hipblasInt8Datatype_t int8Type)
try
{
    // No op
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

bool isDevicePointer(const void* ptr) {
    hipPointerAttribute_t attribs;
    hipError_t hip_status = hipPointerGetAttributes(&attribs, ptr);
    bool is_result_dev_ptr = true;
    if (attribs.memoryType != hipMemoryTypeDevice) {
        is_result_dev_ptr = false;
    }
    return is_result_dev_ptr;
}

onemklTranspose convert(hipblasOperation_t val) {
    switch(val) {
        case HIPBLAS_OP_T:
            return ONEMKL_TRANSPOSE_TRANS;
        case HIPBLAS_OP_C:
            return ONEMLK_TRANSPOSE_CONJTRANS;
        case HIPBLAS_OP_N:
        default:
            return ONEMKL_TRANSPOSE_NONTRANS;
    }
}

onemklUplo convert(hipblasFillMode_t val) {
    switch(val) {
        case HIPBLAS_FILL_MODE_UPPER:
            return ONEMKL_UPLO_UPPER;
        case HIPBLAS_FILL_MODE_LOWER:
            return ONEMKL_UPLO_LOWER;
    }
}

onemklDiag convert(hipblasDiagType_t val) {
    switch(val) {
        case HIPBLAS_DIAG_NON_UNIT:
            return ONEMKL_DIAG_NONUNIT;
        case HIPBLAS_DIAG_UNIT:
            return ONEMKL_DIAG_UNIT;
    }
}
// ----------------------------- hipBlas APIs ------------------------------------

// Level-1 : amax (supported datatypes : float, double, complex float, complex double)
// Generic amax which can handle batched/stride/non-batched
hipblasStatus_t hipblasIsamax(hipblasHandle_t handle, int n, const float* x, int incx, int* result)
try
{
    bool is_result_dev_ptr = isDevicePointer(result);
    // Warning: result is a int* where as amax takes int64_t*
    int64_t *dev_results = (int64_t*)result;
    hipError_t hip_status;
    if (!is_result_dev_ptr)
        hip_status = hipMalloc(&dev_results, sizeof(int64_t));

    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklSamax(sycl_queue, n, x, incx, dev_results);

    syclblas_queue_wait(sycl_queue); // wait until task is completed

    if (!is_result_dev_ptr) {
        int64_t results_host_memory = 0;
        hip_status = hipMemcpy(&results_host_memory, dev_results, sizeof(int64_t), hipMemcpyDefault);

        //Fix_Me : Chance of data corruption
        *result = (int)results_host_memory;

        hip_status = hipFree(&dev_results);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch (...) {
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasIdamax(hipblasHandle_t handle, int n, const double* x, int incx, int* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    // Warning: result is a int* where as amax takes int64_t*
    int64_t *dev_results = (int64_t*)result;

    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_results, sizeof(int64_t));
    }

    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklDamax(sycl_queue, n, x, incx, dev_results);
    syclblas_queue_wait(sycl_queue); // wait until task is completed

    if (!is_result_dev_ptr) {
        int64_t results_host_memory = 0;
        hip_status = hipMemcpy(&results_host_memory, dev_results, sizeof(int64_t), hipMemcpyDefault);

        //Fix_Me : Chance of data corruption
        *result = (int)results_host_memory;

        hip_status = hipFree(&dev_results);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasIcamax(hipblasHandle_t handle, int n, const hipblasComplex* x, int incx, int* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    // Warning: result is a int* where as amax takes int64_t*
    int64_t *dev_results = (int64_t*)result;

    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_results, sizeof(int64_t));
    }

    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklCamax(sycl_queue, n, (const float _Complex*)x, incx, dev_results);

    syclblas_queue_wait(sycl_queue); // wait until task is completed

    if (!is_result_dev_ptr) {
        int64_t results_host_memory = 0;
        hip_status = hipMemcpy(&results_host_memory, dev_results, sizeof(int64_t), hipMemcpyDefault);

        //Fix_Me : Chance of data corruption
        *result = (int)results_host_memory;

        hip_status = hipFree(&dev_results);
    }
    return HIPBLAS_STATUS_SUCCESS;
}
catch (...) {
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasIzamax(hipblasHandle_t handle, int n, const hipblasDoubleComplex* x, int incx, int* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    // Warning: result is a int* where as amax takes int64_t*
    int64_t *dev_results = (int64_t*)result;

    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_results, sizeof(int64_t));
    }

    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklZamax(sycl_queue, n, (const double _Complex*)x, incx, dev_results);
    syclblas_queue_wait(sycl_queue); // wait until task is completed

    if (!is_result_dev_ptr) {
        int64_t results_host_memory = 0;
        hip_status = hipMemcpy(&results_host_memory, dev_results, sizeof(int64_t), hipMemcpyDefault);

        //Fix_Me : Chance of data corruption
        *result = (int)results_host_memory;

        hip_status = hipFree(&dev_results);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch (...) {
    return exception_to_hipblas_status();
}

// Level-1 : amin (supported datatypes : float, double, complex float, complex double)
// Generic amin which can handle batched/stride/non-batched
hipblasStatus_t hipblasIsamin(hipblasHandle_t handle, int n, const float* x, int incx, int* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    // Warning: result is a int* where as amin takes int64_t*
    int64_t *dev_results = (int64_t*)result;

    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_results, sizeof(int64_t));
    }

    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklSamin(sycl_queue, n, x, incx, dev_results);
    syclblas_queue_wait(sycl_queue); // wait until task is completed

    if (!is_result_dev_ptr) {
        int64_t results_host_memory = 0;
        hip_status = hipMemcpy(&results_host_memory, dev_results, sizeof(int64_t), hipMemcpyDefault);

        //Fix_Me : Chance of data corruption
        *result = (int)results_host_memory;

        hip_status = hipFree(&dev_results);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasIdamin(hipblasHandle_t handle, int n, const double* x, int incx, int* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    // Warning: result is a int* where as amin takes int64_t*
    int64_t *dev_results = (int64_t*)result;

    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_results, sizeof(int64_t));
    }

    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklDamin(sycl_queue, n, x, incx, dev_results);
    syclblas_queue_wait(sycl_queue); // wait until task is completed

    if (!is_result_dev_ptr) {
        int64_t results_host_memory = 0;
        hip_status = hipMemcpy(&results_host_memory, dev_results, sizeof(int64_t), hipMemcpyDefault);

        //Fix_Me : Chance of data corruption
        *result = (int)results_host_memory;

        hip_status = hipFree(&dev_results);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasIcamin(hipblasHandle_t handle, int n, const hipblasComplex* x, int incx, int* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    // Warning: result is a int* where as amin takes int64_t*
    int64_t *dev_results = (int64_t*)result;

    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_results, sizeof(int64_t));
    }

    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklCamin(sycl_queue, n, (const float _Complex*)x, incx, dev_results);

    syclblas_queue_wait(sycl_queue); // wait until task is completed

    if (!is_result_dev_ptr) {
        int64_t results_host_memory = 0;
        hip_status = hipMemcpy(&results_host_memory, dev_results, sizeof(int64_t), hipMemcpyDefault);

        //Fix_Me : Chance of data corruption
        *result = (int)results_host_memory;

        hip_status = hipFree(&dev_results);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasIzamin(hipblasHandle_t handle, int n, const hipblasDoubleComplex* x, int incx, int* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    // Warning: result is a int* where as amin takes int64_t*
    int64_t *dev_results = (int64_t*)result;

    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_results, sizeof(int64_t));
    }

    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklZamin(sycl_queue, n, (const double _Complex*)x, incx, dev_results);

    syclblas_queue_wait(sycl_queue); // wait until task is completed

    if (!is_result_dev_ptr) {
        int64_t results_host_memory = 0;
        hip_status = hipMemcpy(&results_host_memory, dev_results, sizeof(int64_t), hipMemcpyDefault);

        //Fix_Me : Chance of data corruption
        *result = (int)results_host_memory;

        hip_status = hipFree(&dev_results);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-1 : asum (supported datatypes : float, double, complex float, complex double)
// Generic asum which can handle batched/stride/non-batched
hipblasStatus_t hipblasSasum(hipblasHandle_t handle, int n, const float* x, int incx, float* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    // 'result' can be device or host memory but oneMKL needs device memory
    float* dev_result = result;
    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_result, sizeof(float));
    }

    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklSasum(sycl_queue, n, x, incx, dev_result);
    syclblas_queue_wait(sycl_queue);

    if (!is_result_dev_ptr) {
        hip_status = hipMemcpy(result, dev_result, sizeof(float), hipMemcpyDefault);
        hip_status = hipFree(dev_result);
    }
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t
    hipblasDasum(hipblasHandle_t handle, int n, const double* x, int incx, double* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    // 'result' can be device or host memory but oneMKL needs device memory
    double* dev_result = result;
    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_result, sizeof(double));
    }

    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklDasum(sycl_queue, n, x, incx, dev_result);
    syclblas_queue_wait(sycl_queue);

    if (!is_result_dev_ptr) {
        hip_status = hipMemcpy(result, dev_result, sizeof(double), hipMemcpyDefault);
        hip_status = hipFree(dev_result);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasScasum(hipblasHandle_t handle, int n, const hipblasComplex* x, int incx, float* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    // 'result' can be device or host memory but oneMKL needs device memory
    float* dev_result = result;
    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_result, sizeof(float));
    }

    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklCasum(sycl_queue, n, (const float _Complex*)x, incx, dev_result);
    syclblas_queue_wait(sycl_queue);

    if (!is_result_dev_ptr) {
        hip_status = hipMemcpy(result, dev_result, sizeof(float), hipMemcpyDefault);
        hip_status = hipFree(dev_result);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDzasum(hipblasHandle_t handle, int n, const hipblasDoubleComplex* x, int incx, double* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    // 'result' can be device or host memory but oneMKL needs device memory
    double* dev_result = result;
    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_result, sizeof(double));
    }

    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklZasum(sycl_queue, n, (const double _Complex*)x, incx, dev_result);
    syclblas_queue_wait(sycl_queue);

    if (!is_result_dev_ptr) {
        hip_status = hipMemcpy(result, dev_result, sizeof(double), hipMemcpyDefault);
        hip_status = hipFree(dev_result);
    }
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-1 : axpy (supported datatypes : float, double, complex float, complex double)
// Generic axpy which can handle batched/stride/non-batched
hipblasStatus_t hipblasSaxpy(hipblasHandle_t handle, int n, const float* alpha,
                             const float* x, int incx, float* y, int incy)
try
{
    bool is_result_dev_ptr = isDevicePointer(alpha);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // 'alpha' can be device or host memory hence need to be copied before access
    float host_alpha_ptr = 0;
    if (is_result_dev_ptr) {
        auto hipStatus = hipMemcpy(&host_alpha_ptr, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        host_alpha_ptr = *alpha;
    }

    onemklSaxpy(sycl_queue, n, host_alpha_ptr, x, incx, y, incy);
    syclblas_queue_wait(sycl_queue);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDaxpy(hipblasHandle_t handle, int n, const double* alpha,
                             const double* x, int incx, double* y, int incy)
try
{
    bool is_result_dev_ptr = isDevicePointer(alpha);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // 'alpha' can be device or host memory hence need to be copied before access
    double host_alpha_ptr = 0;
    if (is_result_dev_ptr) {
        auto hipStatus = hipMemcpy(&host_alpha_ptr, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        host_alpha_ptr = *alpha;
    }

    onemklDaxpy(sycl_queue, n, host_alpha_ptr, x, incx, y, incy);

    syclblas_queue_wait(sycl_queue);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCaxpy(hipblasHandle_t handle, int n, const hipblasComplex* alpha,
                             const hipblasComplex* x, int incx, hipblasComplex* y, int incy)
try
{
    bool is_result_dev_ptr = isDevicePointer(alpha);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // 'alpha' can be device or host memory hence need to be copied before access
    float _Complex host_alpha_ptr = 0;
    if (is_result_dev_ptr) {
        auto hipStatus = hipMemcpy(&host_alpha_ptr, alpha, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        host_alpha_ptr = *((const float _Complex*)alpha);
    }
    onemklCaxpy(sycl_queue, n, host_alpha_ptr, (const float _Complex*)x, incx, (float _Complex*)y, incy);

    syclblas_queue_wait(sycl_queue);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZaxpy(hipblasHandle_t handle, int n, const hipblasDoubleComplex* alpha,
                             const hipblasDoubleComplex* x, int incx, hipblasDoubleComplex* y, int incy)
try
{
    bool is_result_dev_ptr = isDevicePointer(alpha);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // 'alpha' can be device or host memory hence need to be copied before access
    double _Complex host_alpha_ptr = 0;
    if (is_result_dev_ptr) {
        auto hipStatus = hipMemcpy(&host_alpha_ptr, alpha, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        host_alpha_ptr = *((const double _Complex*)alpha);
    }

    onemklZaxpy(sycl_queue, n, host_alpha_ptr, (const double _Complex*)x, incx, (double _Complex*)y, incy);

    syclblas_queue_wait(sycl_queue);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-1 : scal (supported datatypes : float, double, complex float, complex double)
// Generic scal which can handle batched/stride/non-batched
hipblasStatus_t
    hipblasSscal(hipblasHandle_t handle, int n, const float *alpha, float *x, int incx)
try
{
    bool is_dev_ptr = isDevicePointer(alpha);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    float host_alpha = 1.0;
    if (is_dev_ptr) {
        auto hipStatus = hipMemcpy(&host_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        host_alpha = *alpha;
    }

    onemklSscal(sycl_queue, n, host_alpha, x, incx);
    syclblas_queue_wait(sycl_queue);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t
    hipblasDscal(hipblasHandle_t handle, int n, const double *alpha, double *x, int incx)
try
{
    bool is_dev_ptr = isDevicePointer(alpha);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    double host_alpha = 1.0;
    if (is_dev_ptr) {
        auto hipStatus = hipMemcpy(&host_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        host_alpha = *alpha;
    }

    onemklDscal(sycl_queue, n, host_alpha, x, incx);
    syclblas_queue_wait(sycl_queue);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t
    hipblasCscal(hipblasHandle_t handle, int n, const hipblasComplex *alpha, hipblasComplex *x, int incx)
try
{
    bool is_dev_ptr = isDevicePointer(alpha);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    float _Complex host_alpha = 1.0;
    if (is_dev_ptr) {
        auto hipStatus = hipMemcpy(&host_alpha, alpha, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        host_alpha = *((float _Complex*)alpha);
    }

    onemklCscal(sycl_queue, n, host_alpha, (float _Complex*)x, incx);
    syclblas_queue_wait(sycl_queue);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t
    hipblasCsscal(hipblasHandle_t handle, int n, const float *alpha, hipblasComplex *x, int incx)
try
{
    bool is_dev_ptr = isDevicePointer(alpha);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    float host_alpha = 1.0;
    if (is_dev_ptr) {
        auto hipStatus = hipMemcpy(&host_alpha, alpha, sizeof(float ), hipMemcpyDefault);
    } else {
        host_alpha = *alpha;
    }
    onemklCsscal(sycl_queue, n, host_alpha, (float _Complex*)x, incx);
    syclblas_queue_wait(sycl_queue);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t
    hipblasZscal(hipblasHandle_t handle, int n, const hipblasDoubleComplex *alpha, hipblasDoubleComplex *x, int incx)
try
{
    bool is_dev_ptr = isDevicePointer(alpha);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    double _Complex host_alpha = 1.0;
    if (is_dev_ptr) {
        auto hipStatus = hipMemcpy(&host_alpha, alpha, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        host_alpha = *((double _Complex*)alpha);
    }
    onemklZscal(sycl_queue, n, host_alpha, (double _Complex*)x, incx);
    syclblas_queue_wait(sycl_queue);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t
    hipblasZdscal(hipblasHandle_t handle, int n, const double *alpha, hipblasDoubleComplex *x, int incx)
try
{
    bool is_dev_ptr = isDevicePointer(alpha);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    double host_alpha = 1.0;
    if (is_dev_ptr) {
        auto hipStatus = hipMemcpy(&host_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        host_alpha = *alpha;
    }
    onemklZdscal(sycl_queue, n, host_alpha, (double _Complex*)x, incx);
    syclblas_queue_wait(sycl_queue);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-1 : nrm2 (supported datatypes : float, double, complex float, complex double)
// Generic nrm2 which can handle batched/stride/non-batched
hipblasStatus_t
    hipblasSnrm2(hipblasHandle_t handle, int n, const float* x, int incx, float* result)
try
{
    hipError_t status;
    bool is_result_dev_ptr = isDevicePointer(result);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    float* dev_result = result;
    if (!is_result_dev_ptr) {
        status = hipMalloc(&dev_result, sizeof(float));
    }
    onemklSnrm2(sycl_queue, n, x, incx, dev_result);
    syclblas_queue_wait(sycl_queue);

    if (!is_result_dev_ptr) {
        status = hipMemcpy(result, dev_result, sizeof(float), hipMemcpyDefault);
        status = hipFree(dev_result);
    }
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t
    hipblasDnrm2(hipblasHandle_t handle, int n, const double* x, int incx, double* result)
try
{
    hipError_t status;
    bool is_result_dev_ptr = isDevicePointer(result);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    double* dev_result = result;
    if (!is_result_dev_ptr) {
        status = hipMalloc(&dev_result, sizeof(double));
    }
    onemklDnrm2(sycl_queue, n, x, incx, dev_result);
    syclblas_queue_wait(sycl_queue);

    if (!is_result_dev_ptr) {
        status = hipMemcpy(result, dev_result, sizeof(double), hipMemcpyDefault);
        status = hipFree(dev_result);
    }
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t
    hipblasScnrm2(hipblasHandle_t handle, int n, const hipblasComplex* x, int incx, float* result)
try
{
    hipError_t status;
    bool is_result_dev_ptr = isDevicePointer(result);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    float* dev_result = result;
    if (!is_result_dev_ptr) {
        status = hipMalloc(&dev_result, sizeof(float));
    }
    onemklCnrm2(sycl_queue, n, (const float _Complex*)x, incx, dev_result);
    syclblas_queue_wait(sycl_queue);
    if (!is_result_dev_ptr) {
        status = hipMemcpy(result, dev_result, sizeof(float), hipMemcpyDefault);
        status = hipFree(dev_result);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t
    hipblasDznrm2(hipblasHandle_t handle, int n, const hipblasDoubleComplex* x, int incx, double* result)
try
{
    hipError_t status;
    bool is_result_dev_ptr = isDevicePointer(result);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    double* dev_result = result;
    if (!is_result_dev_ptr) {
        status = hipMalloc(&dev_result, sizeof(double));
    }
    onemklZnrm2(sycl_queue, n, (const double _Complex*)x, incx, dev_result);
    syclblas_queue_wait(sycl_queue);
    if (!is_result_dev_ptr) {
        status = hipMemcpy(result, dev_result, sizeof(double), hipMemcpyDefault);
        status = hipFree(dev_result);
    }
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-1 : copy (supported datatypes : float, double, complex float, complex double)
// Generic copy which can handle batched/stride/non-batched
hipblasStatus_t
    hipblasScopy(hipblasHandle_t handle, int n, const float* x, int incx, float* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklScopy(sycl_queue, n, x, incx, y, incy);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t
    hipblasDcopy(hipblasHandle_t handle, int n, const double* x, int incx, double* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklDcopy(sycl_queue, n, x, incx, y, incy);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t
    hipblasCcopy(hipblasHandle_t handle, int n, const hipblasComplex* x, int incx, hipblasComplex* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklCcopy(sycl_queue, n, (const float _Complex*)x, incx, (float _Complex*)y, incy);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t
    hipblasZcopy(hipblasHandle_t handle, int n, const hipblasDoubleComplex* x, int incx, hipblasDoubleComplex* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklZcopy(sycl_queue, n, (const double _Complex*)x, incx, (double _Complex*)y, incy);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-1 : dot (supported datatypes : float, double, complex float, complex double)
// Generic dot which can handle batched/stride/non-batched
hipblasStatus_t hipblasSdot(hipblasHandle_t handle, int n, const float* x, int incx, const float* y, int incy, float* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    float* dev_result = result;
    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_result, sizeof(float));
    }
    onemklSdot(sycl_queue, n, x, incx, y, incy, dev_result);
    syclblas_queue_wait(sycl_queue);

    if (!is_result_dev_ptr) {
        hip_status = hipMemcpy(result, dev_result, sizeof(float), hipMemcpyDefault);
        hip_status = hipFree(dev_result);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDdot(hipblasHandle_t handle, int n, const double* x, int incx, const double* y, int incy, double* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    double* dev_result = result;
    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_result, sizeof(double));
    }
    onemklDdot(sycl_queue, n, x, incx, y, incy, dev_result);
    syclblas_queue_wait(sycl_queue);

    if (!is_result_dev_ptr) {
        hip_status = hipMemcpy(result, dev_result, sizeof(double), hipMemcpyDefault);
        hip_status = hipFree(dev_result);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCdotc(hipblasHandle_t handle, int n, const hipblasComplex* x,
                             int incx, const hipblasComplex* y, int incy, hipblasComplex* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    float _Complex* dev_result = (float _Complex*)result;
    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_result, sizeof(float _Complex));
    }
    onemklCdotc(sycl_queue, n, (const float _Complex*)x, incx, (const float _Complex*)y, incy, dev_result);
    syclblas_queue_wait(sycl_queue);

    if (!is_result_dev_ptr) {
        hip_status = hipMemcpy(result, dev_result, sizeof(float _Complex), hipMemcpyDefault);
        hip_status = hipFree(dev_result);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCdotu(hipblasHandle_t handle, int n, const hipblasComplex* x,
                             int incx, const hipblasComplex* y, int incy, hipblasComplex* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    float _Complex* dev_result = (float _Complex*)result;
    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_result, sizeof(float _Complex));
    }
    onemklCdotc(sycl_queue, n, (const float _Complex*)x, incx, (const float _Complex*)y, incy, dev_result);
    syclblas_queue_wait(sycl_queue);

    if (!is_result_dev_ptr) {
        hip_status = hipMemcpy(result, dev_result, sizeof(float _Complex), hipMemcpyDefault);
        hip_status = hipFree(dev_result);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZdotc(hipblasHandle_t handle, int n, const hipblasDoubleComplex* x,
                             int incx, const hipblasDoubleComplex* y, int incy, hipblasDoubleComplex* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    double _Complex* dev_result = (double _Complex*)result;
    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_result, sizeof(double _Complex));
    }
    onemklZdotc(sycl_queue, n, (const double _Complex*)x, incx, (const double _Complex*)y, incy, dev_result);
    syclblas_queue_wait(sycl_queue);

    if (!is_result_dev_ptr) {
        hip_status = hipMemcpy(result, dev_result, sizeof(double _Complex), hipMemcpyDefault);
        hip_status = hipFree(dev_result);
    }
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZdotu(hipblasHandle_t handle, int n, const hipblasDoubleComplex* x,
                             int incx, const hipblasDoubleComplex* y, int incy, hipblasDoubleComplex* result)
try
{
    hipError_t hip_status;
    bool is_result_dev_ptr = isDevicePointer(result);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    double _Complex* dev_result = (double _Complex*)result;
    if (!is_result_dev_ptr) {
        hip_status = hipMalloc(&dev_result, sizeof(double _Complex));
    }
    onemklZdotc(sycl_queue, n, (const double _Complex*)x, incx, (const double _Complex*)y, incy, dev_result);
    syclblas_queue_wait(sycl_queue);

    if (!is_result_dev_ptr) {
        hip_status = hipMemcpy(result, dev_result, sizeof(double _Complex), hipMemcpyDefault);
        hip_status = hipFree(dev_result);
    }
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-1 : swap (supported datatypes : float, double, complex float, complex double)
// Generic swap which can handle batched/stride/non-batched
hipblasStatus_t hipblasSswap(hipblasHandle_t handle, int n, float* x, int incx, float* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklSswap(sycl_queue, n, x, incx, y, incy);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDswap(hipblasHandle_t handle, int n, double* x, int incx, double* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklDswap(sycl_queue, n, x, incx, y, incy);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCswap(hipblasHandle_t handle, int n, hipblasComplex* x, int incx,
                             hipblasComplex* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklCswap(sycl_queue, n, (float _Complex*)x, incx, (float _Complex*)y, incy);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZswap(hipblasHandle_t handle, int n, hipblasDoubleComplex* x, int incx,
                             hipblasDoubleComplex* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklZswap(sycl_queue, n, (double _Complex*)x, incx, (double _Complex*)y, incy);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-1 : rot (supported datatypes : float, double, complex float, complex double)
// Generic rot which can handle batched/stride/non-batched
hipblasStatus_t hipblasSrot(hipblasHandle_t handle,int n, float* x,int incx,
                                           float* y, int incy,const float* c, const float* s)
try
{
    hipError_t hip_status;
    bool is_c_dev_ptr = isDevicePointer(c);
    bool is_s_dev_ptr = isDevicePointer(s);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    float h_c, h_s;
    if (is_c_dev_ptr) {
        hip_status = hipMemcpy(&h_c, c, sizeof(float), hipMemcpyDefault);
    } else {
        h_c = *c;
    }
    if (is_s_dev_ptr) {
        hip_status = hipMemcpy(&h_s, s, sizeof(float), hipMemcpyDefault);
    } else {
        h_s = *s;
    }

    onemklSrot(sycl_queue, n, x, incx, y, incy, h_c, h_s);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDrot(hipblasHandle_t handle,int n, double* x,int incx,
                                           double* y, int incy,const double* c, const double* s)
try
{
    hipError_t hip_status;
    bool is_c_dev_ptr = isDevicePointer(c);
    bool is_s_dev_ptr = isDevicePointer(s);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    double h_c, h_s;
    if (is_c_dev_ptr) {
        hip_status = hipMemcpy(&h_c, c, sizeof(double), hipMemcpyDefault);
    } else {
        h_c = *c;
    }
    if (is_s_dev_ptr) {
        hip_status = hipMemcpy(&h_s, s, sizeof(double), hipMemcpyDefault);
    } else {
        h_s = *s;
    }

    onemklDrot(sycl_queue, n, x, incx, y, incy, h_c, h_s);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCrot(hipblasHandle_t handle,int n, hipblasComplex* x,int incx,
                                           hipblasComplex* y, int incy,const float* c,
                                           const hipblasComplex* s)
try
{
    hipError_t hip_status;
    bool is_c_dev_ptr = isDevicePointer(c);
    bool is_s_dev_ptr = isDevicePointer(s);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    float h_c;
    if (is_c_dev_ptr) {
        hip_status = hipMemcpy(&h_c, c, sizeof(float), hipMemcpyDefault);
    } else {
        h_c = *c;
    }
    float _Complex h_s;
    if (is_s_dev_ptr) {
        hip_status = hipMemcpy(&h_s, s, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_s = *((float _Complex*)s);
    }

    onemklCrot(sycl_queue, n, (float _Complex*)x, incx, (float _Complex*)y, incy, h_c, h_s);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCsrot(hipblasHandle_t handle,int n, hipblasComplex* x,int incx,
                                           hipblasComplex* y, int incy,const float* c,
                                           const float* s)
try
{
    hipError_t hip_status;
    bool is_c_dev_ptr = isDevicePointer(c);
    bool is_s_dev_ptr = isDevicePointer(s);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    float h_c, h_s;
    if (is_c_dev_ptr) {
        hip_status = hipMemcpy(&h_c, c, sizeof(float), hipMemcpyDefault);
    } else {
        h_c = *c;
    }
    if (is_s_dev_ptr) {
        hip_status = hipMemcpy(&h_s, s, sizeof(float), hipMemcpyDefault);
    } else {
        h_s = *s;
    }
    // Fix-me : assuming c and s are host readable memory else we need to copy it to host memory before read
    onemklCsrot(sycl_queue, n, (float _Complex*)x, incx, (float _Complex*)y, incy, h_c, h_s);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZrot(hipblasHandle_t handle,int n, hipblasDoubleComplex* x,int incx,
                                           hipblasDoubleComplex* y, int incy,const double* c,
                                           const hipblasDoubleComplex* s)
try
{
    hipError_t hip_status;
    bool is_c_dev_ptr = isDevicePointer(c);
    bool is_s_dev_ptr = isDevicePointer(s);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    double h_c;
    if (is_c_dev_ptr) {
        hip_status = hipMemcpy(&h_c, c, sizeof(double), hipMemcpyDefault);
    } else {
        h_c = *c;
    }
    double _Complex h_s;
    if (is_s_dev_ptr) {
        hip_status = hipMemcpy(&h_s, s, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_s = *((double _Complex*)s);
    }
    onemklZrot(sycl_queue, n, (double _Complex*)x, incx, (double _Complex*)y, incy, h_c, h_s);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZdrot(hipblasHandle_t handle,int n, hipblasDoubleComplex* x,int incx,
                                           hipblasDoubleComplex* y, int incy, const double* c,
                                           const double* s)
try
{
    hipError_t hip_status;
    bool is_c_dev_ptr = isDevicePointer(c);
    bool is_s_dev_ptr = isDevicePointer(s);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    double h_c, h_s;
    if (is_c_dev_ptr) {
        hip_status = hipMemcpy(&h_c, c, sizeof(double), hipMemcpyDefault);
    } else {
        h_c = *c;
    }
    if (is_s_dev_ptr) {
        hip_status = hipMemcpy(&h_s, s, sizeof(double), hipMemcpyDefault);
    } else {
        h_s = *s;
    }
    onemklZdrot(sycl_queue, n, (double _Complex*)x, incx, (double _Complex*)y, incy, h_c, h_s);
    syclblas_queue_wait(sycl_queue);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-1 : rotg (supported datatypes : float, double, complex float, complex double)
// Generic rotg which can handle batched/stride/non-batched
hipblasStatus_t hipblasSrotg(hipblasHandle_t handle, float* a, float* b, float* c, float* s)
try
{
    bool is_a_dev_ptr = isDevicePointer(a);
    bool is_b_dev_ptr = isDevicePointer(b);
    bool is_c_dev_ptr = isDevicePointer(c);
    bool is_s_dev_ptr = isDevicePointer(s);
    // FixMe: oneAPI supports only device pointers
    if (!is_a_dev_ptr || !is_b_dev_ptr || !is_c_dev_ptr || !is_s_dev_ptr) {
        return HIPBLAS_STATUS_NOT_SUPPORTED;
    }
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklSrotg(sycl_queue, a, b, c, s);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDrotg(hipblasHandle_t handle, double* a, double* b, double* c, double* s)
try
{
    bool is_a_dev_ptr = isDevicePointer(a);
    bool is_b_dev_ptr = isDevicePointer(b);
    bool is_c_dev_ptr = isDevicePointer(c);
    bool is_s_dev_ptr = isDevicePointer(s);
    // FixMe: oneAPI supports only device pointers
    if (!is_a_dev_ptr || !is_b_dev_ptr || !is_c_dev_ptr || !is_s_dev_ptr) {
        return HIPBLAS_STATUS_NOT_SUPPORTED;
    }
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklDrotg(sycl_queue, a, b, c, s);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCrotg(hipblasHandle_t handle, hipblasComplex* a, hipblasComplex* b, float* c, hipblasComplex* s)
try
{
    bool is_a_dev_ptr = isDevicePointer(a);
    bool is_b_dev_ptr = isDevicePointer(b);
    bool is_c_dev_ptr = isDevicePointer(c);
    bool is_s_dev_ptr = isDevicePointer(s);
    // FixMe: oneAPI supports only device pointers
    if (!is_a_dev_ptr || !is_b_dev_ptr || !is_c_dev_ptr || !is_s_dev_ptr) {
        return HIPBLAS_STATUS_NOT_SUPPORTED;
    }
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklCrotg(sycl_queue, (float _Complex*)a, (float _Complex*)b, c, (float _Complex*)s);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZrotg(hipblasHandle_t handle, hipblasDoubleComplex* a,
                             hipblasDoubleComplex* b, double* c, hipblasDoubleComplex* s)
try
{
    bool is_a_dev_ptr = isDevicePointer(a);
    bool is_b_dev_ptr = isDevicePointer(b);
    bool is_c_dev_ptr = isDevicePointer(c);
    bool is_s_dev_ptr = isDevicePointer(s);
    // FixMe: oneAPI supports only device pointers
    if (!is_a_dev_ptr || !is_b_dev_ptr || !is_c_dev_ptr || !is_s_dev_ptr) {
        return HIPBLAS_STATUS_NOT_SUPPORTED;
    }
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklZrotg(sycl_queue, (double _Complex*)a, (double _Complex*)b, c, (double _Complex*)s);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-1 : rotm (supported datatypes : float, double)
// Generic rotm which can handle batched/stride/non-batched
hipblasStatus_t hipblasSrotm(hipblasHandle_t handle, int n, float* x, int incx, float* y, int incy, const float* param)
try
{
    hipError_t hipStatus;
    bool is_param_dev_ptr = isDevicePointer(param);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    float* dev_param = (float*) param;
    if (!is_param_dev_ptr) {
        hipStatus = hipMalloc(&dev_param, sizeof(float)*5);
        hipStatus = hipMemcpy(dev_param, param, sizeof(float)*5, hipMemcpyHostToDevice);
    }

    onemklSrotm(sycl_queue, n, x, incx, y, incy, dev_param);

    if (!is_param_dev_ptr) {
        syclblas_queue_wait(sycl_queue);
        hipStatus = hipFree(dev_param);
    }
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDrotm(hipblasHandle_t handle, int n, double* x, int incx, double* y, int incy, const double* param)
try
{
    hipError_t hipStatus;
    bool is_param_dev_ptr = isDevicePointer(param);
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    double* dev_param = (double*)param;
    if (!is_param_dev_ptr) {
        hipStatus = hipMalloc(&dev_param, sizeof(double)*5);
        hipStatus = hipMemcpy(dev_param, param, sizeof(double)*5, hipMemcpyHostToDevice);
    }

    onemklDrotm(sycl_queue, n, x, incx, y, incy, dev_param);

    if (!is_param_dev_ptr) {
        syclblas_queue_wait(sycl_queue);
        hipStatus = hipFree(dev_param);
    }
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

/*
 * ===========================================================================
 *    level 2 BLAS
 * ===========================================================================
 */

// Level-2 : gbmv(supported datatypes : float, double, float complex and doule complex)
hipblasStatus_t hipblasSgbmv(hipblasHandle_t handle, hipblasOperation_t trans,
                              int m, int n, int kl, int ku, const float* alpha,
                              const float* AP, int lda, const float* x, int incx,
                              const float* beta, float* y, int incy)
try{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    // Need to check as alpha and beta can be host/device pointer
    float h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        h_alpha = *((float*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(float), hipMemcpyDefault);
    } else {
        h_beta = *((float*)beta);
    }
    onemklSgbmv(sycl_queue, convert(trans), m, n, kl, ku, h_alpha, AP, lda, x, incx, h_beta, y, incy);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDgbmv(hipblasHandle_t handle, hipblasOperation_t trans,
                              int m, int n, int kl, int ku, const double* alpha,
                              const double* AP, int lda, const double* x, int incx,
                              const double* beta, double* y, int incy)
try{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    double h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        h_alpha = *((double*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(double), hipMemcpyDefault);
    } else {
        h_beta = *((double*)beta);
    }
    onemklDgbmv(sycl_queue, convert(trans), m, n, kl, ku, h_alpha, AP, lda, x, incx, h_beta, y, incy);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCgbmv(hipblasHandle_t handle, hipblasOperation_t trans,
                              int m, int n, int kl, int ku, const hipblasComplex* alpha,
                              const hipblasComplex* AP, int lda, const hipblasComplex* x, int incx,
                              const hipblasComplex* beta, hipblasComplex* y, int incy)
try{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    float _Complex h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((float _Complex*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_beta = *((float _Complex*)beta);
    }
    onemklCgbmv(sycl_queue, convert(trans), m, n, kl, ku, h_alpha,
                (const float _Complex *)AP, lda, (const float _Complex *)x, incx,
                 h_beta, (float _Complex *)y, incy);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZgbmv(hipblasHandle_t handle, hipblasOperation_t trans,
                              int m, int n, int kl, int ku, const hipblasDoubleComplex* alpha,
                              const hipblasDoubleComplex* AP, int lda, const hipblasDoubleComplex* x, int incx,
                              const hipblasDoubleComplex* beta, hipblasDoubleComplex* y, int incy)
try{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    double _Complex h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((double _Complex*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_beta = *((double _Complex*)beta);
    }
    onemklZgbmv(sycl_queue, convert(trans), m, n, kl, ku, h_alpha,
                (const double _Complex *)AP, lda, (const double _Complex *)x, incx,
                 h_beta, (double _Complex *)y, incy);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : gemv(supported datatypes : float, double, float complex and doule complex)
hipblasStatus_t hipblasSgemv(hipblasHandle_t handle, hipblasOperation_t trans, int m, int n,
                             const float* alpha, const float* AP, int lda, const float* x, int incx,
                             const float* beta, float* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    float h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        h_alpha = *((float*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(float), hipMemcpyDefault);
    } else {
        h_beta = *((float*)beta);
    }
    onemklSgemv(sycl_queue, convert(trans), m, n, h_alpha, AP, lda, x, incx, h_beta, y, incy);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDgemv(hipblasHandle_t handle, hipblasOperation_t trans, int m, int n,
                             const double* alpha, const double* AP, int lda, const double* x, int incx,
                             const double* beta, double* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    double h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        h_alpha = *((double*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(double), hipMemcpyDefault);
    } else {
        h_beta = *((double*)beta);
    }
    onemklDgemv(sycl_queue, convert(trans), m, n, h_alpha, AP, lda, x, incx, h_beta, y, incy);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCgemv(hipblasHandle_t handle, hipblasOperation_t trans,
                              int m, int n, const hipblasComplex* alpha,
                              const hipblasComplex* AP, int lda, const hipblasComplex* x, int incx,
                              const hipblasComplex* beta, hipblasComplex* y, int incy)
try{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    float _Complex h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((float _Complex*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_beta = *((float _Complex*)beta);
    }
    onemklCgemv(sycl_queue, convert(trans), m, n, h_alpha,
                (const float _Complex *)AP, lda, (const float _Complex *)x, incx,
                 h_beta, (float _Complex *)y, incy);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZgemv(hipblasHandle_t handle, hipblasOperation_t trans,
                              int m, int n, const hipblasDoubleComplex* alpha,
                              const hipblasDoubleComplex* AP, int lda, const hipblasDoubleComplex* x, int incx,
                              const hipblasDoubleComplex* beta, hipblasDoubleComplex* y, int incy)
try{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    double _Complex h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((double _Complex*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_beta = *((double _Complex*)beta);
    }
    onemklZgemv(sycl_queue, convert(trans), m, n, h_alpha,
                (const double _Complex *)AP, lda, (const double _Complex *)x, incx,
                 h_beta, (double _Complex *)y, incy);

    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : ger(supported datatypes : float, double, float complex and doule complex)
hipblasStatus_t hipblasSger(hipblasHandle_t handle, int m, int n, const float* alpha,
                            const float* x, int incx, const float* y, int incy,
                            float* AP, int lda)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    float h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        h_alpha = *((float*)alpha);
    }
    onemklSger(sycl_queue, m, n, h_alpha, x, incx, y, incy, AP, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDger(hipblasHandle_t handle, int m, int n, const double* alpha,
                            const double* x, int incx, const double* y, int incy,
                            double* AP, int lda)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    double h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        h_alpha = *((double*)alpha);
    }
    onemklDger(sycl_queue, m, n, h_alpha, x, incx, y, incy, AP, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCgerc(hipblasHandle_t handle, int m, int n, const hipblasComplex* alpha,
                            const hipblasComplex* x, int incx, const hipblasComplex* y, int incy,
                            hipblasComplex* AP, int lda)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    float _Complex h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((float _Complex*)alpha);
    }
    onemklCgerc(sycl_queue, m, n, h_alpha, (const float _Complex*)x, incx, (const float _Complex*)y, incy,
                (float _Complex*)AP, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCgeru(hipblasHandle_t handle, int m, int n, const hipblasComplex* alpha,
                            const hipblasComplex* x, int incx, const hipblasComplex* y, int incy,
                            hipblasComplex* AP, int lda)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    float _Complex h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((float _Complex*)alpha);
    }
    onemklCgeru(sycl_queue, m, n, h_alpha, (const float _Complex*)x, incx, (const float _Complex*)y, incy,
                (float _Complex*)AP, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZgerc(hipblasHandle_t handle, int m, int n, const hipblasDoubleComplex* alpha,
                            const hipblasDoubleComplex* x, int incx, const hipblasDoubleComplex* y, int incy,
                            hipblasDoubleComplex* AP, int lda)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    double _Complex h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((double _Complex*)alpha);
    }
    onemklZgerc(sycl_queue, m, n, h_alpha, (const double _Complex*)x, incx, (const double _Complex*)y, incy,
                (double _Complex*)AP, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZgeru(hipblasHandle_t handle, int m, int n, const hipblasDoubleComplex* alpha,
                            const hipblasDoubleComplex* x, int incx, const hipblasDoubleComplex* y, int incy,
                            hipblasDoubleComplex* AP, int lda)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    double _Complex h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((double _Complex*)alpha);
    }
    onemklZgeru(sycl_queue, m, n, h_alpha, (const double _Complex*)x, incx, (const double _Complex*)y, incy,
                (double _Complex*)AP, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : hbmv(supported datatypes : float complex and doule complex)
hipblasStatus_t hipblasChbmv(hipblasHandle_t handle, hipblasFillMode_t uplo,
                            int n, int k, const hipblasComplex* alpha,
                            const hipblasComplex* AP, int lda,
                            const hipblasComplex* x, int incx,
                             const hipblasComplex* beta, hipblasComplex* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    float _Complex h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((float _Complex*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_beta = *((float _Complex*)beta);
    }
    onemklChbmv(sycl_queue, convert(uplo), n,k, h_alpha, (const float _Complex*)AP, lda, (const float _Complex*)x, incx, 
                h_beta, (float _Complex*)y, incy);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZhbmv(hipblasHandle_t handle, hipblasFillMode_t uplo,
                            int n, int k, const hipblasDoubleComplex* alpha,
                            const hipblasDoubleComplex* AP, int lda,
                            const hipblasDoubleComplex* x, int incx,
                             const hipblasDoubleComplex* beta, hipblasDoubleComplex* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    double _Complex h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((double _Complex*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_beta = *((double _Complex*)beta);
    }
    onemklZhbmv(sycl_queue, convert(uplo), n,k, h_alpha, (const double _Complex*)AP, lda, (const double _Complex*)x, incx, 
                h_beta, (double _Complex*)y, incy);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : hemv(supported datatypes : float complex and doule complex)
hipblasStatus_t hipblasChemv(hipblasHandle_t handle, hipblasFillMode_t uplo, int n,
                            const hipblasComplex* alpha, const hipblasComplex* AP,
                            int lda, const hipblasComplex* x, int incx,
                            const hipblasComplex* beta, hipblasComplex* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    float _Complex h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((float _Complex*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_beta = *((float _Complex*)beta);
    }
    onemklChemv(sycl_queue, convert(uplo), n, h_alpha, (const float _Complex*)AP, lda,
                (const float _Complex*)x, incx, h_beta, (float _Complex*)y, incy);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZhemv(hipblasHandle_t handle, hipblasFillMode_t uplo, int n,
                            const hipblasDoubleComplex* alpha, const hipblasDoubleComplex* AP,
                            int lda, const hipblasDoubleComplex* x, int incx,
                            const hipblasDoubleComplex* beta, hipblasDoubleComplex* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    double _Complex h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((double _Complex*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_beta = *((double _Complex*)beta);
    }
    onemklZhemv(sycl_queue, convert(uplo), n, h_alpha, (const double _Complex*)AP, lda,
                (const double _Complex*)x, incx, h_beta, (double _Complex*)y, incy); 
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : her(supported datatypes : float complex and doule complex)
hipblasStatus_t hipblasCher(hipblasHandle_t handle, hipblasFillMode_t uplo, int n,
                            const float* alpha, const hipblasComplex* x, int incx,
                            hipblasComplex* AP, int lda)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    float h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        h_alpha = *((float*)alpha);
    }
    onemklCher(sycl_queue, convert(uplo), n, h_alpha, (const float _Complex*)x, incx, (float _Complex*)AP, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZher(hipblasHandle_t handle, hipblasFillMode_t uplo, int n,
                            const double* alpha, const hipblasDoubleComplex* x, int incx,
                            hipblasDoubleComplex* AP, int lda)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    double h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        h_alpha = *((double*)alpha);
    }
    onemklZher(sycl_queue, convert(uplo), n, h_alpha, (const double _Complex*)x, incx, (double _Complex*)AP, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : her2(supported datatypes : float complex and doule complex)
hipblasStatus_t hipblasCher2(hipblasHandle_t handle, hipblasFillMode_t uplo, int n,
                            const hipblasComplex* alpha, const hipblasComplex* x, int incx,
                            const hipblasComplex* y, int incy, hipblasComplex* AP, int lda)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    float _Complex h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((float _Complex*)alpha);
    }
    onemklCher2(sycl_queue, convert(uplo), n, h_alpha, (const float _Complex*)x, incx,
                (const float _Complex*)y, incy, (float _Complex*)AP, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZher2(hipblasHandle_t handle, hipblasFillMode_t uplo, int n,
                            const hipblasDoubleComplex* alpha, const hipblasDoubleComplex* x, int incx,
                            const hipblasDoubleComplex* y, int incy, hipblasDoubleComplex* AP, int lda)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    double _Complex h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((double _Complex*)alpha);
    }
    onemklZher2(sycl_queue, convert(uplo), n, h_alpha, (const double _Complex*)x, incx,
                (const double _Complex*)y, incy, (double _Complex*)AP, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : hpmv(supported datatypes : float complex and doule complex)
hipblasStatus_t hipblasChpmv(hipblasHandle_t handle, hipblasFillMode_t uplo, int n,
                            const hipblasComplex* alpha, const hipblasComplex* AP,
                            const hipblasComplex* x, int incx, const hipblasComplex* beta,
                            hipblasComplex* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    float _Complex h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((float _Complex*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_beta = *((float _Complex*)beta);
    }
    onemklChpmv(sycl_queue, convert(uplo), n, h_alpha, (const float _Complex*)AP, (const float _Complex*)x, incx,
                h_beta, (float _Complex*)y, incy);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZhpmv(hipblasHandle_t handle, hipblasFillMode_t uplo, int n,
                            const hipblasDoubleComplex* alpha, const hipblasDoubleComplex* AP,
                            const hipblasDoubleComplex* x, int incx, const hipblasDoubleComplex* beta,
                            hipblasDoubleComplex* y, int incy)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    double _Complex h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((double _Complex*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_beta = *((double _Complex*)beta);
    }
    onemklZhpmv(sycl_queue, convert(uplo), n, h_alpha, (const double _Complex*)AP, (const double _Complex*)x, incx,
                h_beta, (double _Complex*)y, incy);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : hpr(supported datatypes : float complex and doule complex)
hipblasStatus_t hipblasChpr(hipblasHandle_t       handle,
                            hipblasFillMode_t     uplo,
                            int                   n,
                            const float*          alpha,
                            const hipblasComplex* x,
                            int                   incx,
                            hipblasComplex*       AP)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    float h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        h_alpha = *((float*)alpha);
    }
    onemklChpr(sycl_queue, convert(uplo), n, h_alpha, (const float _Complex*)x, incx, (float _Complex*)AP);
	return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZhpr(hipblasHandle_t             handle,
                            hipblasFillMode_t           uplo,
                            int                         n,
                            const double*               alpha,
                            const hipblasDoubleComplex* x,
                            int                         incx,
                            hipblasDoubleComplex*       AP)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    double h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        h_alpha = *((double*)alpha);
    }
    onemklZhpr(sycl_queue, convert(uplo), n, h_alpha, (const double _Complex*)x, incx, (double _Complex*)AP);
	return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// hpr_batched
hipblasStatus_t hipblasChprBatched(hipblasHandle_t             handle,
                                   hipblasFillMode_t           uplo,
                                   int                         n,
                                   const float*                alpha,
                                   const hipblasComplex* const x[],
                                   int                         incx,
                                   hipblasComplex* const       AP[],
                                   int                         batchCount)
try
{
	return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZhprBatched(hipblasHandle_t                   handle,
                                   hipblasFillMode_t                 uplo,
                                   int                               n,
                                   const double*                     alpha,
                                   const hipblasDoubleComplex* const x[],
                                   int                               incx,
                                   hipblasDoubleComplex* const       AP[],
                                   int                               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// hpr_strided_batched
hipblasStatus_t hipblasChprStridedBatched(hipblasHandle_t       handle,
                                          hipblasFillMode_t     uplo,
                                          int                   n,
                                          const float*          alpha,
                                          const hipblasComplex* x,
                                          int                   incx,
                                          hipblasStride         stridex,
                                          hipblasComplex*       AP,
                                          hipblasStride         strideAP,
                                          int                   batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZhprStridedBatched(hipblasHandle_t             handle,
                                          hipblasFillMode_t           uplo,
                                          int                         n,
                                          const double*               alpha,
                                          const hipblasDoubleComplex* x,
                                          int                         incx,
                                          hipblasStride               stridex,
                                          hipblasDoubleComplex*       AP,
                                          hipblasStride               strideAP,
                                          int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : hpr2(supported datatypes : float complex and doule complex)
hipblasStatus_t hipblasChpr2(hipblasHandle_t       handle,
                             hipblasFillMode_t     uplo,
                             int                   n,
                             const hipblasComplex* alpha,
                             const hipblasComplex* x,
                             int                   incx,
                             const hipblasComplex* y,
                             int                   incy,
                             hipblasComplex*       AP)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    float _Complex h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((float _Complex*)alpha);
    }
    onemklChpr2(sycl_queue, convert(uplo), n, h_alpha, (const float _Complex*)x, incx,
                            (const float _Complex*)y, incy, (float _Complex*)AP);
	return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZhpr2(hipblasHandle_t             handle,
                             hipblasFillMode_t           uplo,
                             int                         n,
                             const hipblasDoubleComplex* alpha,
                             const hipblasDoubleComplex* x,
                             int                         incx,
                             const hipblasDoubleComplex* y,
                             int                         incy,
                             hipblasDoubleComplex*       AP)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    double _Complex h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double _Complex), hipMemcpyDefault);
    } else {
        h_alpha = *((double _Complex*)alpha);
    }
    onemklZhpr2(sycl_queue, convert(uplo), n, h_alpha, (const double _Complex*)x, incx,
                            (const double _Complex*)y, incy, (double _Complex*)AP);
	return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// hpr2_batched
hipblasStatus_t hipblasChpr2Batched(hipblasHandle_t             handle,
                                    hipblasFillMode_t           uplo,
                                    int                         n,
                                    const hipblasComplex*       alpha,
                                    const hipblasComplex* const x[],
                                    int                         incx,
                                    const hipblasComplex* const y[],
                                    int                         incy,
                                    hipblasComplex* const       AP[],
                                    int                         batchCount)
try
{
	return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZhpr2Batched(hipblasHandle_t                   handle,
                                    hipblasFillMode_t                 uplo,
                                    int                               n,
                                    const hipblasDoubleComplex*       alpha,
                                    const hipblasDoubleComplex* const x[],
                                    int                               incx,
                                    const hipblasDoubleComplex* const y[],
                                    int                               incy,
                                    hipblasDoubleComplex* const       AP[],
                                    int                               batchCount)
try
{
	return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// hpr2_strided_batched
hipblasStatus_t hipblasChpr2StridedBatched(hipblasHandle_t       handle,
                                           hipblasFillMode_t     uplo,
                                           int                   n,
                                           const hipblasComplex* alpha,
                                           const hipblasComplex* x,
                                           int                   incx,
                                           hipblasStride         stridex,
                                           const hipblasComplex* y,
                                           int                   incy,
                                           hipblasStride         stridey,
                                           hipblasComplex*       AP,
                                           hipblasStride         strideAP,
                                           int                   batchCount)
try
{
	return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZhpr2StridedBatched(hipblasHandle_t             handle,
                                           hipblasFillMode_t           uplo,
                                           int                         n,
                                           const hipblasDoubleComplex* alpha,
                                           const hipblasDoubleComplex* x,
                                           int                         incx,
                                           hipblasStride               stridex,
                                           const hipblasDoubleComplex* y,
                                           int                         incy,
                                           hipblasStride               stridey,
                                           hipblasDoubleComplex*       AP,
                                           hipblasStride               strideAP,
                                           int                         batchCount)
try
{
	return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : sbmv(supported datatypes : float and doule )
hipblasStatus_t hipblasSsbmv(hipblasHandle_t   handle,
                             hipblasFillMode_t uplo,
                             int               n,
                             int               k,
                             const float*      alpha,
                             const float*      A,
                             int               lda,
                             const float*      x,
                             int               incx,
                             const float*      beta,
                             float*            y,
                             int               incy)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    float h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        h_alpha = *((float*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(float), hipMemcpyDefault);
    } else {
        h_beta = *((float*)beta);
    }
    onemklSsbmv(sycl_queue, convert(uplo), n, k, h_alpha, A, lda, x, incx, h_beta, y, incy);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsbmv(hipblasHandle_t   handle,
                             hipblasFillMode_t uplo,
                             int               n,
                             int               k,
                             const double*     alpha,
                             const double*     A,
                             int               lda,
                             const double*     x,
                             int               incx,
                             const double*     beta,
                             double*           y,
                             int               incy)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    double h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        h_alpha = *((double*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(double), hipMemcpyDefault);
    } else {
        h_beta = *((double*)beta);
    }
    onemklDsbmv(sycl_queue, convert(uplo), n, k, h_alpha, A, lda, x, incx, h_beta, y, incy);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// sbmv_batched
hipblasStatus_t hipblasSsbmvBatched(hipblasHandle_t    handle,
                                    hipblasFillMode_t  uplo,
                                    int                n,
                                    int                k,
                                    const float*       alpha,
                                    const float* const A[],
                                    int                lda,
                                    const float* const x[],
                                    int                incx,
                                    const float*       beta,
                                    float*             y[],
                                    int                incy,
                                    int                batchCount)
try
{
	return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsbmvBatched(hipblasHandle_t     handle,
                                    hipblasFillMode_t   uplo,
                                    int                 n,
                                    int                 k,
                                    const double*       alpha,
                                    const double* const A[],
                                    int                 lda,
                                    const double* const x[],
                                    int                 incx,
                                    const double*       beta,
                                    double*             y[],
                                    int                 incy,
                                    int                 batchCount)
try
{
	return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// sbmv_strided_batched
hipblasStatus_t hipblasSsbmvStridedBatched(hipblasHandle_t   handle,
                                           hipblasFillMode_t uplo,
                                           int               n,
                                           int               k,
                                           const float*      alpha,
                                           const float*      A,
                                           int               lda,
                                           hipblasStride     strideA,
                                           const float*      x,
                                           int               incx,
                                           hipblasStride     stridex,
                                           const float*      beta,
                                           float*            y,
                                           int               incy,
                                           hipblasStride     stridey,
                                           int               batchCount)
try
{
	return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsbmvStridedBatched(hipblasHandle_t   handle,
                                           hipblasFillMode_t uplo,
                                           int               n,
                                           int               k,
                                           const double*     alpha,
                                           const double*     A,
                                           int               lda,
                                           hipblasStride     strideA,
                                           const double*     x,
                                           int               incx,
                                           hipblasStride     stridex,
                                           const double*     beta,
                                           double*           y,
                                           int               incy,
                                           hipblasStride     stridey,
                                           int               batchCount)
try
{
	return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : spmv(supported datatypes : float and doule )
hipblasStatus_t hipblasSspmv(hipblasHandle_t   handle,
                             hipblasFillMode_t uplo,
                             int               n,
                             const float*      alpha,
                             const float*      AP,
                             const float*      x,
                             int               incx,
                             const float*      beta,
                             float*            y,
                             int               incy)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    float h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        h_alpha = *((float*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(float), hipMemcpyDefault);
    } else {
        h_beta = *((float*)beta);
    }
    onemklSspmv(sycl_queue, convert(uplo), n, h_alpha, AP, x, incx, h_beta, y, incy);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDspmv(hipblasHandle_t   handle,
                             hipblasFillMode_t uplo,
                             int               n,
                             const double*     alpha,
                             const double*     AP,
                             const double*     x,
                             int               incx,
                             const double*     beta,
                             double*           y,
                             int               incy)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    double h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        h_alpha = *((double*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(double), hipMemcpyDefault);
    } else {
        h_beta = *((double*)beta);
    }
    onemklDspmv(sycl_queue, convert(uplo), n, h_alpha, AP, x, incx, h_beta, y, incy);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// spmv_batched
hipblasStatus_t hipblasSspmvBatched(hipblasHandle_t    handle,
                                    hipblasFillMode_t  uplo,
                                    int                n,
                                    const float*       alpha,
                                    const float* const AP[],
                                    const float* const x[],
                                    int                incx,
                                    const float*       beta,
                                    float*             y[],
                                    int                incy,
                                    int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDspmvBatched(hipblasHandle_t     handle,
                                    hipblasFillMode_t   uplo,
                                    int                 n,
                                    const double*       alpha,
                                    const double* const AP[],
                                    const double* const x[],
                                    int                 incx,
                                    const double*       beta,
                                    double*             y[],
                                    int                 incy,
                                    int                 batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// spmv_strided_batched
hipblasStatus_t hipblasSspmvStridedBatched(hipblasHandle_t   handle,
                                           hipblasFillMode_t uplo,
                                           int               n,
                                           const float*      alpha,
                                           const float*      AP,
                                           hipblasStride     strideAP,
                                           const float*      x,
                                           int               incx,
                                           hipblasStride     stridex,
                                           const float*      beta,
                                           float*            y,
                                           int               incy,
                                           hipblasStride     stridey,
                                           int               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDspmvStridedBatched(hipblasHandle_t   handle,
                                           hipblasFillMode_t uplo,
                                           int               n,
                                           const double*     alpha,
                                           const double*     AP,
                                           hipblasStride     strideAP,
                                           const double*     x,
                                           int               incx,
                                           hipblasStride     stridex,
                                           const double*     beta,
                                           double*           y,
                                           int               incy,
                                           hipblasStride     stridey,
                                           int               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : spr(supported datatypes : float, double, float complex and doule complex )
hipblasStatus_t hipblasSspr(hipblasHandle_t   handle,
                            hipblasFillMode_t uplo,
                            int               n,
                            const float*      alpha,
                            const float*      x,
                            int               incx,
                            float*            AP)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    float h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        h_alpha = *((float*)alpha);
    }
    onemklSspr(sycl_queue, convert(uplo), n, h_alpha, x, incx, AP);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDspr(hipblasHandle_t   handle,
                            hipblasFillMode_t uplo,
                            int               n,
                            const double*     alpha,
                            const double*     x,
                            int               incx,
                            double*           AP)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    double h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        h_alpha = *((double*)alpha);
    }
    onemklDspr(sycl_queue, convert(uplo), n, h_alpha, x, incx, AP);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCspr(hipblasHandle_t       handle,
                            hipblasFillMode_t     uplo,
                            int                   n,
                            const hipblasComplex* alpha,
                            const hipblasComplex* x,
                            int                   incx,
                            hipblasComplex*       AP)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZspr(hipblasHandle_t             handle,
                            hipblasFillMode_t           uplo,
                            int                         n,
                            const hipblasDoubleComplex* alpha,
                            const hipblasDoubleComplex* x,
                            int                         incx,
                            hipblasDoubleComplex*       AP)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// spr_batched
hipblasStatus_t hipblasSsprBatched(hipblasHandle_t    handle,
                                   hipblasFillMode_t  uplo,
                                   int                n,
                                   const float*       alpha,
                                   const float* const x[],
                                   int                incx,
                                   float* const       AP[],
                                   int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsprBatched(hipblasHandle_t     handle,
                                   hipblasFillMode_t   uplo,
                                   int                 n,
                                   const double*       alpha,
                                   const double* const x[],
                                   int                 incx,
                                   double* const       AP[],
                                   int                 batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCsprBatched(hipblasHandle_t             handle,
                                   hipblasFillMode_t           uplo,
                                   int                         n,
                                   const hipblasComplex*       alpha,
                                   const hipblasComplex* const x[],
                                   int                         incx,
                                   hipblasComplex* const       AP[],
                                   int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZsprBatched(hipblasHandle_t                   handle,
                                   hipblasFillMode_t                 uplo,
                                   int                               n,
                                   const hipblasDoubleComplex*       alpha,
                                   const hipblasDoubleComplex* const x[],
                                   int                               incx,
                                   hipblasDoubleComplex* const       AP[],
                                   int                               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// spr_strided_batched
hipblasStatus_t hipblasSsprStridedBatched(hipblasHandle_t   handle,
                                          hipblasFillMode_t uplo,
                                          int               n,
                                          const float*      alpha,
                                          const float*      x,
                                          int               incx,
                                          hipblasStride     stridex,
                                          float*            AP,
                                          hipblasStride     strideAP,
                                          int               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsprStridedBatched(hipblasHandle_t   handle,
                                          hipblasFillMode_t uplo,
                                          int               n,
                                          const double*     alpha,
                                          const double*     x,
                                          int               incx,
                                          hipblasStride     stridex,
                                          double*           AP,
                                          hipblasStride     strideAP,
                                          int               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCsprStridedBatched(hipblasHandle_t       handle,
                                          hipblasFillMode_t     uplo,
                                          int                   n,
                                          const hipblasComplex* alpha,
                                          const hipblasComplex* x,
                                          int                   incx,
                                          hipblasStride         stridex,
                                          hipblasComplex*       AP,
                                          hipblasStride         strideAP,
                                          int                   batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZsprStridedBatched(hipblasHandle_t             handle,
                                          hipblasFillMode_t           uplo,
                                          int                         n,
                                          const hipblasDoubleComplex* alpha,
                                          const hipblasDoubleComplex* x,
                                          int                         incx,
                                          hipblasStride               stridex,
                                          hipblasDoubleComplex*       AP,
                                          hipblasStride               strideAP,
                                          int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : spr2(supported datatypes : float and double )
hipblasStatus_t hipblasSspr2(hipblasHandle_t   handle,
                             hipblasFillMode_t uplo,
                             int               n,
                             const float*      alpha,
                             const float*      x,
                             int               incx,
                             const float*      y,
                             int               incy,
                             float*            AP)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    float h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        h_alpha = *((float*)alpha);
    }
    onemklSspr2(sycl_queue, convert(uplo), n, h_alpha, x, incx, y, incy, AP);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDspr2(hipblasHandle_t   handle,
                             hipblasFillMode_t uplo,
                             int               n,
                             const double*     alpha,
                             const double*     x,
                             int               incx,
                             const double*     y,
                             int               incy,
                             double*           AP)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    double h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        h_alpha = *((double*)alpha);
    }
    onemklDspr2(sycl_queue, convert(uplo), n, h_alpha, x, incx, y, incy, AP);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// spr2_batched
hipblasStatus_t hipblasSspr2Batched(hipblasHandle_t    handle,
                                    hipblasFillMode_t  uplo,
                                    int                n,
                                    const float*       alpha,
                                    const float* const x[],
                                    int                incx,
                                    const float* const y[],
                                    int                incy,
                                    float* const       AP[],
                                    int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDspr2Batched(hipblasHandle_t     handle,
                                    hipblasFillMode_t   uplo,
                                    int                 n,
                                    const double*       alpha,
                                    const double* const x[],
                                    int                 incx,
                                    const double* const y[],
                                    int                 incy,
                                    double* const       AP[],
                                    int                 batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// spr2_strided_batched
hipblasStatus_t hipblasSspr2StridedBatched(hipblasHandle_t   handle,
                                           hipblasFillMode_t uplo,
                                           int               n,
                                           const float*      alpha,
                                           const float*      x,
                                           int               incx,
                                           hipblasStride     stridex,
                                           const float*      y,
                                           int               incy,
                                           hipblasStride     stridey,
                                           float*            AP,
                                           hipblasStride     strideAP,
                                           int               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDspr2StridedBatched(hipblasHandle_t   handle,
                                           hipblasFillMode_t uplo,
                                           int               n,
                                           const double*     alpha,
                                           const double*     x,
                                           int               incx,
                                           hipblasStride     stridex,
                                           const double*     y,
                                           int               incy,
                                           hipblasStride     stridey,
                                           double*           AP,
                                           hipblasStride     strideAP,
                                           int               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : symv(supported datatypes : float and double )
hipblasStatus_t hipblasSsymv(hipblasHandle_t   handle,
                             hipblasFillMode_t uplo,
                             int               n,
                             const float*      alpha,
                             const float*      A,
                             int               lda,
                             const float*      x,
                             int               incx,
                             const float*      beta,
                             float*            y,
                             int               incy)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    float h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        h_alpha = *((float*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(float), hipMemcpyDefault);
    } else {
        h_beta = *((float*)beta);
    }
    onemklSsymv(sycl_queue, convert(uplo), n, h_alpha, A, lda, x, incx, h_beta, y, incy);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsymv(hipblasHandle_t   handle,
                             hipblasFillMode_t uplo,
                             int               n,
                             const double*     alpha,
                             const double*     A,
                             int               lda,
                             const double*     x,
                             int               incx,
                             const double*     beta,
                             double*           y,
                             int               incy)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);
    auto is_beta_dev_ptr = isDevicePointer(beta);

    double h_alpha, h_beta;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        h_alpha = *((double*)alpha);
    }
    if (is_beta_dev_ptr) {
        hipMemcpy(&h_beta, beta, sizeof(double), hipMemcpyDefault);
    } else {
        h_beta = *((double*)beta);
    }
    onemklDsymv(sycl_queue, convert(uplo), n, h_alpha, A, lda, x, incx, h_beta, y, incy);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCsymv(hipblasHandle_t       handle,
                             hipblasFillMode_t     uplo,
                             int                   n,
                             const hipblasComplex* alpha,
                             const hipblasComplex* A,
                             int                   lda,
                             const hipblasComplex* x,
                             int                   incx,
                             const hipblasComplex* beta,
                             hipblasComplex*       y,
                             int                   incy)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZsymv(hipblasHandle_t             handle,
                             hipblasFillMode_t           uplo,
                             int                         n,
                             const hipblasDoubleComplex* alpha,
                             const hipblasDoubleComplex* A,
                             int                         lda,
                             const hipblasDoubleComplex* x,
                             int                         incx,
                             const hipblasDoubleComplex* beta,
                             hipblasDoubleComplex*       y,
                             int                         incy)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// symv_batched
hipblasStatus_t hipblasSsymvBatched(hipblasHandle_t    handle,
                                    hipblasFillMode_t  uplo,
                                    int                n,
                                    const float*       alpha,
                                    const float* const A[],
                                    int                lda,
                                    const float* const x[],
                                    int                incx,
                                    const float*       beta,
                                    float*             y[],
                                    int                incy,
                                    int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsymvBatched(hipblasHandle_t     handle,
                                    hipblasFillMode_t   uplo,
                                    int                 n,
                                    const double*       alpha,
                                    const double* const A[],
                                    int                 lda,
                                    const double* const x[],
                                    int                 incx,
                                    const double*       beta,
                                    double*             y[],
                                    int                 incy,
                                    int                 batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCsymvBatched(hipblasHandle_t             handle,
                                    hipblasFillMode_t           uplo,
                                    int                         n,
                                    const hipblasComplex*       alpha,
                                    const hipblasComplex* const A[],
                                    int                         lda,
                                    const hipblasComplex* const x[],
                                    int                         incx,
                                    const hipblasComplex*       beta,
                                    hipblasComplex*             y[],
                                    int                         incy,
                                    int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZsymvBatched(hipblasHandle_t                   handle,
                                    hipblasFillMode_t                 uplo,
                                    int                               n,
                                    const hipblasDoubleComplex*       alpha,
                                    const hipblasDoubleComplex* const A[],
                                    int                               lda,
                                    const hipblasDoubleComplex* const x[],
                                    int                               incx,
                                    const hipblasDoubleComplex*       beta,
                                    hipblasDoubleComplex*             y[],
                                    int                               incy,
                                    int                               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// symv_strided_batched
hipblasStatus_t hipblasSsymvStridedBatched(hipblasHandle_t   handle,
                                           hipblasFillMode_t uplo,
                                           int               n,
                                           const float*      alpha,
                                           const float*      A,
                                           int               lda,
                                           hipblasStride     strideA,
                                           const float*      x,
                                           int               incx,
                                           hipblasStride     stridex,
                                           const float*      beta,
                                           float*            y,
                                           int               incy,
                                           hipblasStride     stridey,
                                           int               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsymvStridedBatched(hipblasHandle_t   handle,
                                           hipblasFillMode_t uplo,
                                           int               n,
                                           const double*     alpha,
                                           const double*     A,
                                           int               lda,
                                           hipblasStride     strideA,
                                           const double*     x,
                                           int               incx,
                                           hipblasStride     stridex,
                                           const double*     beta,
                                           double*           y,
                                           int               incy,
                                           hipblasStride     stridey,
                                           int               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCsymvStridedBatched(hipblasHandle_t       handle,
                                           hipblasFillMode_t     uplo,
                                           int                   n,
                                           const hipblasComplex* alpha,
                                           const hipblasComplex* A,
                                           int                   lda,
                                           hipblasStride         strideA,
                                           const hipblasComplex* x,
                                           int                   incx,
                                           hipblasStride         stridex,
                                           const hipblasComplex* beta,
                                           hipblasComplex*       y,
                                           int                   incy,
                                           hipblasStride         stridey,
                                           int                   batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZsymvStridedBatched(hipblasHandle_t             handle,
                                           hipblasFillMode_t           uplo,
                                           int                         n,
                                           const hipblasDoubleComplex* alpha,
                                           const hipblasDoubleComplex* A,
                                           int                         lda,
                                           hipblasStride               strideA,
                                           const hipblasDoubleComplex* x,
                                           int                         incx,
                                           hipblasStride               stridex,
                                           const hipblasDoubleComplex* beta,
                                           hipblasDoubleComplex*       y,
                                           int                         incy,
                                           hipblasStride               stridey,
                                           int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : syr(supported datatypes : float and double )
hipblasStatus_t hipblasSsyr(hipblasHandle_t   handle,
                            hipblasFillMode_t uplo,
                            int               n,
                            const float*      alpha,
                            const float*      x,
                            int               incx,
                            float*            A,
                            int               lda)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    float h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        h_alpha = *((float*)alpha);
    }
    onemklSsyr(sycl_queue, convert(uplo), n, h_alpha, x, incx, A, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsyr(hipblasHandle_t   handle,
                            hipblasFillMode_t uplo,
                            int               n,
                            const double*     alpha,
                            const double*     x,
                            int               incx,
                            double*           A,
                            int               lda)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    double h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        h_alpha = *((double*)alpha);
    }
    onemklDsyr(sycl_queue, convert(uplo), n, h_alpha, x, incx, A, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCsyr(hipblasHandle_t       handle,
                            hipblasFillMode_t     uplo,
                            int                   n,
                            const hipblasComplex* alpha,
                            const hipblasComplex* x,
                            int                   incx,
                            hipblasComplex*       A,
                            int                   lda)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZsyr(hipblasHandle_t             handle,
                            hipblasFillMode_t           uplo,
                            int                         n,
                            const hipblasDoubleComplex* alpha,
                            const hipblasDoubleComplex* x,
                            int                         incx,
                            hipblasDoubleComplex*       A,
                            int                         lda)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// syr_batched
hipblasStatus_t hipblasSsyrBatched(hipblasHandle_t    handle,
                                   hipblasFillMode_t  uplo,
                                   int                n,
                                   const float*       alpha,
                                   const float* const x[],
                                   int                incx,
                                   float* const       A[],
                                   int                lda,
                                   int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsyrBatched(hipblasHandle_t     handle,
                                   hipblasFillMode_t   uplo,
                                   int                 n,
                                   const double*       alpha,
                                   const double* const x[],
                                   int                 incx,
                                   double* const       A[],
                                   int                 lda,
                                   int                 batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCsyrBatched(hipblasHandle_t             handle,
                                   hipblasFillMode_t           uplo,
                                   int                         n,
                                   const hipblasComplex*       alpha,
                                   const hipblasComplex* const x[],
                                   int                         incx,
                                   hipblasComplex* const       A[],
                                   int                         lda,
                                   int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZsyrBatched(hipblasHandle_t                   handle,
                                   hipblasFillMode_t                 uplo,
                                   int                               n,
                                   const hipblasDoubleComplex*       alpha,
                                   const hipblasDoubleComplex* const x[],
                                   int                               incx,
                                   hipblasDoubleComplex* const       A[],
                                   int                               lda,
                                   int                               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// syr_strided_batched
hipblasStatus_t hipblasSsyrStridedBatched(hipblasHandle_t   handle,
                                          hipblasFillMode_t uplo,
                                          int               n,
                                          const float*      alpha,
                                          const float*      x,
                                          int               incx,
                                          hipblasStride     stridex,
                                          float*            A,
                                          int               lda,
                                          hipblasStride     strideA,
                                          int               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsyrStridedBatched(hipblasHandle_t   handle,
                                          hipblasFillMode_t uplo,
                                          int               n,
                                          const double*     alpha,
                                          const double*     x,
                                          int               incx,
                                          hipblasStride     stridex,
                                          double*           A,
                                          int               lda,
                                          hipblasStride     strideA,
                                          int               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCsyrStridedBatched(hipblasHandle_t       handle,
                                          hipblasFillMode_t     uplo,
                                          int                   n,
                                          const hipblasComplex* alpha,
                                          const hipblasComplex* x,
                                          int                   incx,
                                          hipblasStride         stridex,
                                          hipblasComplex*       A,
                                          int                   lda,
                                          hipblasStride         strideA,
                                          int                   batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZsyrStridedBatched(hipblasHandle_t             handle,
                                          hipblasFillMode_t           uplo,
                                          int                         n,
                                          const hipblasDoubleComplex* alpha,
                                          const hipblasDoubleComplex* x,
                                          int                         incx,
                                          hipblasStride               stridex,
                                          hipblasDoubleComplex*       A,
                                          int                         lda,
                                          hipblasStride               strideA,
                                          int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : syr2(supported datatypes : float and double )
hipblasStatus_t hipblasSsyr2(hipblasHandle_t   handle,
                             hipblasFillMode_t uplo,
                             int               n,
                             const float*      alpha,
                             const float*      x,
                             int               incx,
                             const float*      y,
                             int               incy,
                             float*            A,
                             int               lda)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    float h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(float), hipMemcpyDefault);
    } else {
        h_alpha = *((float*)alpha);
    }
    onemklSsyr2(sycl_queue, convert(uplo), n, h_alpha, x, incx, y, incy, A, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsyr2(hipblasHandle_t   handle,
                             hipblasFillMode_t uplo,
                             int               n,
                             const double*     alpha,
                             const double*     x,
                             int               incx,
                             const double*     y,
                             int               incy,
                             double*           A,
                             int               lda)
try
{
    // As per spec alpha can be device/host memory. In case of device memory *alpha will crash
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    auto is_alpha_dev_ptr = isDevicePointer(alpha);

    double h_alpha;
    if (is_alpha_dev_ptr) {
        hipMemcpy(&h_alpha, alpha, sizeof(double), hipMemcpyDefault);
    } else {
        h_alpha = *((double*)alpha);
    }
    onemklDsyr2(sycl_queue, convert(uplo), n, h_alpha, x, incx, y, incy, A, lda);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCsyr2(hipblasHandle_t       handle,
                             hipblasFillMode_t     uplo,
                             int                   n,
                             const hipblasComplex* alpha,
                             const hipblasComplex* x,
                             int                   incx,
                             const hipblasComplex* y,
                             int                   incy,
                             hipblasComplex*       A,
                             int                   lda)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZsyr2(hipblasHandle_t             handle,
                             hipblasFillMode_t           uplo,
                             int                         n,
                             const hipblasDoubleComplex* alpha,
                             const hipblasDoubleComplex* x,
                             int                         incx,
                             const hipblasDoubleComplex* y,
                             int                         incy,
                             hipblasDoubleComplex*       A,
                             int                         lda)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// syr2_batched
hipblasStatus_t hipblasSsyr2Batched(hipblasHandle_t    handle,
                                    hipblasFillMode_t  uplo,
                                    int                n,
                                    const float*       alpha,
                                    const float* const x[],
                                    int                incx,
                                    const float* const y[],
                                    int                incy,
                                    float* const       A[],
                                    int                lda,
                                    int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsyr2Batched(hipblasHandle_t     handle,
                                    hipblasFillMode_t   uplo,
                                    int                 n,
                                    const double*       alpha,
                                    const double* const x[],
                                    int                 incx,
                                    const double* const y[],
                                    int                 incy,
                                    double* const       A[],
                                    int                 lda,
                                    int                 batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCsyr2Batched(hipblasHandle_t             handle,
                                    hipblasFillMode_t           uplo,
                                    int                         n,
                                    const hipblasComplex*       alpha,
                                    const hipblasComplex* const x[],
                                    int                         incx,
                                    const hipblasComplex* const y[],
                                    int                         incy,
                                    hipblasComplex* const       A[],
                                    int                         lda,
                                    int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZsyr2Batched(hipblasHandle_t                   handle,
                                    hipblasFillMode_t                 uplo,
                                    int                               n,
                                    const hipblasDoubleComplex*       alpha,
                                    const hipblasDoubleComplex* const x[],
                                    int                               incx,
                                    const hipblasDoubleComplex* const y[],
                                    int                               incy,
                                    hipblasDoubleComplex* const       A[],
                                    int                               lda,
                                    int                               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// syr2_strided_batched
hipblasStatus_t hipblasSsyr2StridedBatched(hipblasHandle_t   handle,
                                           hipblasFillMode_t uplo,
                                           int               n,
                                           const float*      alpha,
                                           const float*      x,
                                           int               incx,
                                           hipblasStride     stridex,
                                           const float*      y,
                                           int               incy,
                                           hipblasStride     stridey,
                                           float*            A,
                                           int               lda,
                                           hipblasStride     strideA,
                                           int               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDsyr2StridedBatched(hipblasHandle_t   handle,
                                           hipblasFillMode_t uplo,
                                           int               n,
                                           const double*     alpha,
                                           const double*     x,
                                           int               incx,
                                           hipblasStride     stridex,
                                           const double*     y,
                                           int               incy,
                                           hipblasStride     stridey,
                                           double*           A,
                                           int               lda,
                                           hipblasStride     strideA,
                                           int               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCsyr2StridedBatched(hipblasHandle_t       handle,
                                           hipblasFillMode_t     uplo,
                                           int                   n,
                                           const hipblasComplex* alpha,
                                           const hipblasComplex* x,
                                           int                   incx,
                                           hipblasStride         stridex,
                                           const hipblasComplex* y,
                                           int                   incy,
                                           hipblasStride         stridey,
                                           hipblasComplex*       A,
                                           int                   lda,
                                           hipblasStride         strideA,
                                           int                   batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZsyr2StridedBatched(hipblasHandle_t             handle,
                                           hipblasFillMode_t           uplo,
                                           int                         n,
                                           const hipblasDoubleComplex* alpha,
                                           const hipblasDoubleComplex* x,
                                           int                         incx,
                                           hipblasStride               stridex,
                                           const hipblasDoubleComplex* y,
                                           int                         incy,
                                           hipblasStride               stridey,
                                           hipblasDoubleComplex*       A,
                                           int                         lda,
                                           hipblasStride               strideA,
                                           int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : tbmv(supported datatypes : float , double , float complex and double complex )
hipblasStatus_t hipblasStbmv(hipblasHandle_t    handle,
                             hipblasFillMode_t  uplo,
                             hipblasOperation_t transA,
                             hipblasDiagType_t  diag,
                             int                m,
                             int                k,
                             const float*       A,
                             int                lda,
                             float*             x,
                             int                incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklStbmv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, k, A, lda, x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtbmv(hipblasHandle_t    handle,
                             hipblasFillMode_t  uplo,
                             hipblasOperation_t transA,
                             hipblasDiagType_t  diag,
                             int                m,
                             int                k,
                             const double*      A,
                             int                lda,
                             double*            x,
                             int                incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklDtbmv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, k, A, lda, x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtbmv(hipblasHandle_t       handle,
                             hipblasFillMode_t     uplo,
                             hipblasOperation_t    transA,
                             hipblasDiagType_t     diag,
                             int                   m,
                             int                   k,
                             const hipblasComplex* A,
                             int                   lda,
                             hipblasComplex*       x,
                             int                   incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklCtbmv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, k, 
                            (const float _Complex*)A, lda, (float _Complex*)x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtbmv(hipblasHandle_t             handle,
                             hipblasFillMode_t           uplo,
                             hipblasOperation_t          transA,
                             hipblasDiagType_t           diag,
                             int                         m,
                             int                         k,
                             const hipblasDoubleComplex* A,
                             int                         lda,
                             hipblasDoubleComplex*       x,
                             int                         incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklZtbmv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, k, 
                            (const double _Complex*)A, lda, (double _Complex*)x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// tbmv_batched
hipblasStatus_t hipblasStbmvBatched(hipblasHandle_t    handle,
                                    hipblasFillMode_t  uplo,
                                    hipblasOperation_t transA,
                                    hipblasDiagType_t  diag,
                                    int                m,
                                    int                k,
                                    const float* const A[],
                                    int                lda,
                                    float* const       x[],
                                    int                incx,
                                    int                batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtbmvBatched(hipblasHandle_t     handle,
                                    hipblasFillMode_t   uplo,
                                    hipblasOperation_t  transA,
                                    hipblasDiagType_t   diag,
                                    int                 m,
                                    int                 k,
                                    const double* const A[],
                                    int                 lda,
                                    double* const       x[],
                                    int                 incx,
                                    int                 batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtbmvBatched(hipblasHandle_t             handle,
                                    hipblasFillMode_t           uplo,
                                    hipblasOperation_t          transA,
                                    hipblasDiagType_t           diag,
                                    int                         m,
                                    int                         k,
                                    const hipblasComplex* const A[],
                                    int                         lda,
                                    hipblasComplex* const       x[],
                                    int                         incx,
                                    int                         batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtbmvBatched(hipblasHandle_t                   handle,
                                    hipblasFillMode_t                 uplo,
                                    hipblasOperation_t                transA,
                                    hipblasDiagType_t                 diag,
                                    int                               m,
                                    int                               k,
                                    const hipblasDoubleComplex* const A[],
                                    int                               lda,
                                    hipblasDoubleComplex* const       x[],
                                    int                               incx,
                                    int                               batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// tbmv_strided_batched
hipblasStatus_t hipblasStbmvStridedBatched(hipblasHandle_t    handle,
                                           hipblasFillMode_t  uplo,
                                           hipblasOperation_t transA,
                                           hipblasDiagType_t  diag,
                                           int                m,
                                           int                k,
                                           const float*       A,
                                           int                lda,
                                           hipblasStride      stride_a,
                                           float*             x,
                                           int                incx,
                                           hipblasStride      stride_x,
                                           int                batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtbmvStridedBatched(hipblasHandle_t    handle,
                                           hipblasFillMode_t  uplo,
                                           hipblasOperation_t transA,
                                           hipblasDiagType_t  diag,
                                           int                m,
                                           int                k,
                                           const double*      A,
                                           int                lda,
                                           hipblasStride      stride_a,
                                           double*            x,
                                           int                incx,
                                           hipblasStride      stride_x,
                                           int                batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtbmvStridedBatched(hipblasHandle_t       handle,
                                           hipblasFillMode_t     uplo,
                                           hipblasOperation_t    transA,
                                           hipblasDiagType_t     diag,
                                           int                   m,
                                           int                   k,
                                           const hipblasComplex* A,
                                           int                   lda,
                                           hipblasStride         stride_a,
                                           hipblasComplex*       x,
                                           int                   incx,
                                           hipblasStride         stride_x,
                                           int                   batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtbmvStridedBatched(hipblasHandle_t             handle,
                                           hipblasFillMode_t           uplo,
                                           hipblasOperation_t          transA,
                                           hipblasDiagType_t           diag,
                                           int                         m,
                                           int                         k,
                                           const hipblasDoubleComplex* A,
                                           int                         lda,
                                           hipblasStride               stride_a,
                                           hipblasDoubleComplex*       x,
                                           int                         incx,
                                           hipblasStride               stride_x,
                                           int                         batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : tbsv(supported datatypes : float , double , float complex and double complex )
hipblasStatus_t hipblasStbsv(hipblasHandle_t    handle,
                             hipblasFillMode_t  uplo,
                             hipblasOperation_t transA,
                             hipblasDiagType_t  diag,
                             int                n,
                             int                k,
                             const float*       A,
                             int                lda,
                             float*             x,
                             int                incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklStbsv(sycl_queue, convert(uplo), convert(transA), convert(diag), n, k, A, lda, x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtbsv(hipblasHandle_t    handle,
                             hipblasFillMode_t  uplo,
                             hipblasOperation_t transA,
                             hipblasDiagType_t  diag,
                             int                n,
                             int                k,
                             const double*      A,
                             int                lda,
                             double*            x,
                             int                incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklDtbsv(sycl_queue, convert(uplo), convert(transA), convert(diag), n, k, A, lda, x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtbsv(hipblasHandle_t       handle,
                             hipblasFillMode_t     uplo,
                             hipblasOperation_t    transA,
                             hipblasDiagType_t     diag,
                             int                   n,
                             int                   k,
                             const hipblasComplex* A,
                             int                   lda,
                             hipblasComplex*       x,
                             int                   incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklCtbsv(sycl_queue, convert(uplo), convert(transA), convert(diag), n, k, (const float _Complex*)A, lda, (float _Complex*)x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtbsv(hipblasHandle_t             handle,
                             hipblasFillMode_t           uplo,
                             hipblasOperation_t          transA,
                             hipblasDiagType_t           diag,
                             int                         n,
                             int                         k,
                             const hipblasDoubleComplex* A,
                             int                         lda,
                             hipblasDoubleComplex*       x,
                             int                         incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklZtbsv(sycl_queue, convert(uplo), convert(transA), convert(diag), n, k, (const double _Complex*)A, lda, (double _Complex*)x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// tbsv_batched
hipblasStatus_t hipblasStbsvBatched(hipblasHandle_t    handle,
                                    hipblasFillMode_t  uplo,
                                    hipblasOperation_t transA,
                                    hipblasDiagType_t  diag,
                                    int                n,
                                    int                k,
                                    const float* const A[],
                                    int                lda,
                                    float* const       x[],
                                    int                incx,
                                    int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtbsvBatched(hipblasHandle_t     handle,
                                    hipblasFillMode_t   uplo,
                                    hipblasOperation_t  transA,
                                    hipblasDiagType_t   diag,
                                    int                 n,
                                    int                 k,
                                    const double* const A[],
                                    int                 lda,
                                    double* const       x[],
                                    int                 incx,
                                    int                 batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtbsvBatched(hipblasHandle_t             handle,
                                    hipblasFillMode_t           uplo,
                                    hipblasOperation_t          transA,
                                    hipblasDiagType_t           diag,
                                    int                         n,
                                    int                         k,
                                    const hipblasComplex* const A[],
                                    int                         lda,
                                    hipblasComplex* const       x[],
                                    int                         incx,
                                    int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtbsvBatched(hipblasHandle_t                   handle,
                                    hipblasFillMode_t                 uplo,
                                    hipblasOperation_t                transA,
                                    hipblasDiagType_t                 diag,
                                    int                               n,
                                    int                               k,
                                    const hipblasDoubleComplex* const A[],
                                    int                               lda,
                                    hipblasDoubleComplex* const       x[],
                                    int                               incx,
                                    int                               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// tbsv_strided_batched
hipblasStatus_t hipblasStbsvStridedBatched(hipblasHandle_t    handle,
                                           hipblasFillMode_t  uplo,
                                           hipblasOperation_t transA,
                                           hipblasDiagType_t  diag,
                                           int                n,
                                           int                k,
                                           const float*       A,
                                           int                lda,
                                           hipblasStride      strideA,
                                           float*             x,
                                           int                incx,
                                           hipblasStride      stridex,
                                           int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtbsvStridedBatched(hipblasHandle_t    handle,
                                           hipblasFillMode_t  uplo,
                                           hipblasOperation_t transA,
                                           hipblasDiagType_t  diag,
                                           int                n,
                                           int                k,
                                           const double*      A,
                                           int                lda,
                                           hipblasStride      strideA,
                                           double*            x,
                                           int                incx,
                                           hipblasStride      stridex,
                                           int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtbsvStridedBatched(hipblasHandle_t       handle,
                                           hipblasFillMode_t     uplo,
                                           hipblasOperation_t    transA,
                                           hipblasDiagType_t     diag,
                                           int                   n,
                                           int                   k,
                                           const hipblasComplex* A,
                                           int                   lda,
                                           hipblasStride         strideA,
                                           hipblasComplex*       x,
                                           int                   incx,
                                           hipblasStride         stridex,
                                           int                   batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtbsvStridedBatched(hipblasHandle_t             handle,
                                           hipblasFillMode_t           uplo,
                                           hipblasOperation_t          transA,
                                           hipblasDiagType_t           diag,
                                           int                         n,
                                           int                         k,
                                           const hipblasDoubleComplex* A,
                                           int                         lda,
                                           hipblasStride               strideA,
                                           hipblasDoubleComplex*       x,
                                           int                         incx,
                                           hipblasStride               stridex,
                                           int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : tpmv(supported datatypes : float , double , float complex and double complex )
hipblasStatus_t hipblasStpmv(hipblasHandle_t    handle,
                             hipblasFillMode_t  uplo,
                             hipblasOperation_t transA,
                             hipblasDiagType_t  diag,
                             int                m,
                             const float*       AP,
                             float*             x,
                             int                incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklStpmv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, AP, x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtpmv(hipblasHandle_t    handle,
                             hipblasFillMode_t  uplo,
                             hipblasOperation_t transA,
                             hipblasDiagType_t  diag,
                             int                m,
                             const double*      AP,
                             double*            x,
                             int                incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklDtpmv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, AP, x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtpmv(hipblasHandle_t       handle,
                             hipblasFillMode_t     uplo,
                             hipblasOperation_t    transA,
                             hipblasDiagType_t     diag,
                             int                   m,
                             const hipblasComplex* AP,
                             hipblasComplex*       x,
                             int                   incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklCtpmv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, 
                (const float _Complex*)AP, (float _Complex*)x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtpmv(hipblasHandle_t             handle,
                             hipblasFillMode_t           uplo,
                             hipblasOperation_t          transA,
                             hipblasDiagType_t           diag,
                             int                         m,
                             const hipblasDoubleComplex* AP,
                             hipblasDoubleComplex*       x,
                             int                         incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklZtpmv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, 
                (const double _Complex*)AP, (double _Complex*)x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// tpmv_batched
hipblasStatus_t hipblasStpmvBatched(hipblasHandle_t    handle,
                                    hipblasFillMode_t  uplo,
                                    hipblasOperation_t transA,
                                    hipblasDiagType_t  diag,
                                    int                m,
                                    const float* const AP[],
                                    float* const       x[],
                                    int                incx,
                                    int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtpmvBatched(hipblasHandle_t     handle,
                                    hipblasFillMode_t   uplo,
                                    hipblasOperation_t  transA,
                                    hipblasDiagType_t   diag,
                                    int                 m,
                                    const double* const AP[],
                                    double* const       x[],
                                    int                 incx,
                                    int                 batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtpmvBatched(hipblasHandle_t             handle,
                                    hipblasFillMode_t           uplo,
                                    hipblasOperation_t          transA,
                                    hipblasDiagType_t           diag,
                                    int                         m,
                                    const hipblasComplex* const AP[],
                                    hipblasComplex* const       x[],
                                    int                         incx,
                                    int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtpmvBatched(hipblasHandle_t                   handle,
                                    hipblasFillMode_t                 uplo,
                                    hipblasOperation_t                transA,
                                    hipblasDiagType_t                 diag,
                                    int                               m,
                                    const hipblasDoubleComplex* const AP[],
                                    hipblasDoubleComplex* const       x[],
                                    int                               incx,
                                    int                               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// tpmv_strided_batched
hipblasStatus_t hipblasStpmvStridedBatched(hipblasHandle_t    handle,
                                           hipblasFillMode_t  uplo,
                                           hipblasOperation_t transA,
                                           hipblasDiagType_t  diag,
                                           int                m,
                                           const float*       AP,
                                           hipblasStride      strideAP,
                                           float*             x,
                                           int                incx,
                                           hipblasStride      stridex,
                                           int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtpmvStridedBatched(hipblasHandle_t    handle,
                                           hipblasFillMode_t  uplo,
                                           hipblasOperation_t transA,
                                           hipblasDiagType_t  diag,
                                           int                m,
                                           const double*      AP,
                                           hipblasStride      strideAP,
                                           double*            x,
                                           int                incx,
                                           hipblasStride      stridex,
                                           int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtpmvStridedBatched(hipblasHandle_t       handle,
                                           hipblasFillMode_t     uplo,
                                           hipblasOperation_t    transA,
                                           hipblasDiagType_t     diag,
                                           int                   m,
                                           const hipblasComplex* AP,
                                           hipblasStride         strideAP,
                                           hipblasComplex*       x,
                                           int                   incx,
                                           hipblasStride         stridex,
                                           int                   batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtpmvStridedBatched(hipblasHandle_t             handle,
                                           hipblasFillMode_t           uplo,
                                           hipblasOperation_t          transA,
                                           hipblasDiagType_t           diag,
                                           int                         m,
                                           const hipblasDoubleComplex* AP,
                                           hipblasStride               strideAP,
                                           hipblasDoubleComplex*       x,
                                           int                         incx,
                                           hipblasStride               stridex,
                                           int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : tpsv(supported datatypes : float , double , float complex and double complex )
hipblasStatus_t hipblasStpsv(hipblasHandle_t    handle,
                             hipblasFillMode_t  uplo,
                             hipblasOperation_t transA,
                             hipblasDiagType_t  diag,
                             int                m,
                             const float*       AP,
                             float*             x,
                             int                incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklStpsv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, AP, x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtpsv(hipblasHandle_t    handle,
                             hipblasFillMode_t  uplo,
                             hipblasOperation_t transA,
                             hipblasDiagType_t  diag,
                             int                m,
                             const double*      AP,
                             double*            x,
                             int                incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklDtpsv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, AP, x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtpsv(hipblasHandle_t       handle,
                             hipblasFillMode_t     uplo,
                             hipblasOperation_t    transA,
                             hipblasDiagType_t     diag,
                             int                   m,
                             const hipblasComplex* AP,
                             hipblasComplex*       x,
                             int                   incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklCtpsv(sycl_queue, convert(uplo), convert(transA), convert(diag), m,
                (const float _Complex*) AP, (float _Complex*)x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtpsv(hipblasHandle_t             handle,
                             hipblasFillMode_t           uplo,
                             hipblasOperation_t          transA,
                             hipblasDiagType_t           diag,
                             int                         m,
                             const hipblasDoubleComplex* AP,
                             hipblasDoubleComplex*       x,
                             int                         incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklZtpsv(sycl_queue, convert(uplo), convert(transA), convert(diag), m,
                (const double _Complex*) AP, (double _Complex*)x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// tpsv_batched
hipblasStatus_t hipblasStpsvBatched(hipblasHandle_t    handle,
                                    hipblasFillMode_t  uplo,
                                    hipblasOperation_t transA,
                                    hipblasDiagType_t  diag,
                                    int                m,
                                    const float* const AP[],
                                    float* const       x[],
                                    int                incx,
                                    int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtpsvBatched(hipblasHandle_t     handle,
                                    hipblasFillMode_t   uplo,
                                    hipblasOperation_t  transA,
                                    hipblasDiagType_t   diag,
                                    int                 m,
                                    const double* const AP[],
                                    double* const       x[],
                                    int                 incx,
                                    int                 batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtpsvBatched(hipblasHandle_t             handle,
                                    hipblasFillMode_t           uplo,
                                    hipblasOperation_t          transA,
                                    hipblasDiagType_t           diag,
                                    int                         m,
                                    const hipblasComplex* const AP[],
                                    hipblasComplex* const       x[],
                                    int                         incx,
                                    int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtpsvBatched(hipblasHandle_t                   handle,
                                    hipblasFillMode_t                 uplo,
                                    hipblasOperation_t                transA,
                                    hipblasDiagType_t                 diag,
                                    int                               m,
                                    const hipblasDoubleComplex* const AP[],
                                    hipblasDoubleComplex* const       x[],
                                    int                               incx,
                                    int                               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// tpsv_strided_batched
hipblasStatus_t hipblasStpsvStridedBatched(hipblasHandle_t    handle,
                                           hipblasFillMode_t  uplo,
                                           hipblasOperation_t transA,
                                           hipblasDiagType_t  diag,
                                           int                m,
                                           const float*       AP,
                                           hipblasStride      strideAP,
                                           float*             x,
                                           int                incx,
                                           hipblasStride      stridex,
                                           int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtpsvStridedBatched(hipblasHandle_t    handle,
                                           hipblasFillMode_t  uplo,
                                           hipblasOperation_t transA,
                                           hipblasDiagType_t  diag,
                                           int                m,
                                           const double*      AP,
                                           hipblasStride      strideAP,
                                           double*            x,
                                           int                incx,
                                           hipblasStride      stridex,
                                           int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtpsvStridedBatched(hipblasHandle_t       handle,
                                           hipblasFillMode_t     uplo,
                                           hipblasOperation_t    transA,
                                           hipblasDiagType_t     diag,
                                           int                   m,
                                           const hipblasComplex* AP,
                                           hipblasStride         strideAP,
                                           hipblasComplex*       x,
                                           int                   incx,
                                           hipblasStride         stridex,
                                           int                   batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtpsvStridedBatched(hipblasHandle_t             handle,
                                           hipblasFillMode_t           uplo,
                                           hipblasOperation_t          transA,
                                           hipblasDiagType_t           diag,
                                           int                         m,
                                           const hipblasDoubleComplex* AP,
                                           hipblasStride               strideAP,
                                           hipblasDoubleComplex*       x,
                                           int                         incx,
                                           hipblasStride               stridex,
                                           int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : trmv(supported datatypes : float , double , float complex and double complex )
hipblasStatus_t hipblasStrmv(hipblasHandle_t    handle,
                             hipblasFillMode_t  uplo,
                             hipblasOperation_t transA,
                             hipblasDiagType_t  diag,
                             int                m,
                             const float*       A,
                             int                lda,
                             float*             x,
                             int                incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklStrmv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, A, lda, x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtrmv(hipblasHandle_t    handle,
                             hipblasFillMode_t  uplo,
                             hipblasOperation_t transA,
                             hipblasDiagType_t  diag,
                             int                m,
                             const double*      A,
                             int                lda,
                             double*            x,
                             int                incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklDtrmv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, A, lda, x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtrmv(hipblasHandle_t       handle,
                             hipblasFillMode_t     uplo,
                             hipblasOperation_t    transA,
                             hipblasDiagType_t     diag,
                             int                   m,
                             const hipblasComplex* A,
                             int                   lda,
                             hipblasComplex*       x,
                             int                   incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklCtrmv(sycl_queue, convert(uplo), convert(transA), convert(diag), m,
                (const float _Complex*)A, lda, (float _Complex*)x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtrmv(hipblasHandle_t             handle,
                             hipblasFillMode_t           uplo,
                             hipblasOperation_t          transA,
                             hipblasDiagType_t           diag,
                             int                         m,
                             const hipblasDoubleComplex* A,
                             int                         lda,
                             hipblasDoubleComplex*       x,
                             int                         incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklZtrmv(sycl_queue, convert(uplo), convert(transA), convert(diag), m,
                (const double _Complex*)A, lda, (double _Complex*)x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// trmv_batched
hipblasStatus_t hipblasStrmvBatched(hipblasHandle_t    handle,
                                    hipblasFillMode_t  uplo,
                                    hipblasOperation_t transA,
                                    hipblasDiagType_t  diag,
                                    int                m,
                                    const float* const A[],
                                    int                lda,
                                    float* const       x[],
                                    int                incx,
                                    int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtrmvBatched(hipblasHandle_t     handle,
                                    hipblasFillMode_t   uplo,
                                    hipblasOperation_t  transA,
                                    hipblasDiagType_t   diag,
                                    int                 m,
                                    const double* const A[],
                                    int                 lda,
                                    double* const       x[],
                                    int                 incx,
                                    int                 batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtrmvBatched(hipblasHandle_t             handle,
                                    hipblasFillMode_t           uplo,
                                    hipblasOperation_t          transA,
                                    hipblasDiagType_t           diag,
                                    int                         m,
                                    const hipblasComplex* const A[],
                                    int                         lda,
                                    hipblasComplex* const       x[],
                                    int                         incx,
                                    int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtrmvBatched(hipblasHandle_t                   handle,
                                    hipblasFillMode_t                 uplo,
                                    hipblasOperation_t                transA,
                                    hipblasDiagType_t                 diag,
                                    int                               m,
                                    const hipblasDoubleComplex* const A[],
                                    int                               lda,
                                    hipblasDoubleComplex* const       x[],
                                    int                               incx,
                                    int                               batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// trmv_strided_batched
hipblasStatus_t hipblasStrmvStridedBatched(hipblasHandle_t    handle,
                                           hipblasFillMode_t  uplo,
                                           hipblasOperation_t transA,
                                           hipblasDiagType_t  diag,
                                           int                m,
                                           const float*       A,
                                           int                lda,
                                           hipblasStride      stride_a,
                                           float*             x,
                                           int                incx,
                                           hipblasStride      stridex,
                                           int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtrmvStridedBatched(hipblasHandle_t    handle,
                                           hipblasFillMode_t  uplo,
                                           hipblasOperation_t transA,
                                           hipblasDiagType_t  diag,
                                           int                m,
                                           const double*      A,
                                           int                lda,
                                           hipblasStride      stride_a,
                                           double*            x,
                                           int                incx,
                                           hipblasStride      stridex,
                                           int                batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtrmvStridedBatched(hipblasHandle_t       handle,
                                           hipblasFillMode_t     uplo,
                                           hipblasOperation_t    transA,
                                           hipblasDiagType_t     diag,
                                           int                   m,
                                           const hipblasComplex* A,
                                           int                   lda,
                                           hipblasStride         stride_a,
                                           hipblasComplex*       x,
                                           int                   incx,
                                           hipblasStride         stridex,
                                           int                   batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtrmvStridedBatched(hipblasHandle_t             handle,
                                           hipblasFillMode_t           uplo,
                                           hipblasOperation_t          transA,
                                           hipblasDiagType_t           diag,
                                           int                         m,
                                           const hipblasDoubleComplex* A,
                                           int                         lda,
                                           hipblasStride               stride_a,
                                           hipblasDoubleComplex*       x,
                                           int                         incx,
                                           hipblasStride               stridex,
                                           int                         batchCount)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// Level-2 : trsv(supported datatypes : float , double , float complex and double complex )
hipblasStatus_t hipblasStrsv(hipblasHandle_t    handle,
                             hipblasFillMode_t  uplo,
                             hipblasOperation_t transA,
                             hipblasDiagType_t  diag,
                             int                m,
                             const float*       A,
                             int                lda,
                             float*             x,
                             int                incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklStrsv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, A, lda, x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtrsv(hipblasHandle_t    handle,
                             hipblasFillMode_t  uplo,
                             hipblasOperation_t transA,
                             hipblasDiagType_t  diag,
                             int                m,
                             const double*      A,
                             int                lda,
                             double*            x,
                             int                incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklDtrsv(sycl_queue, convert(uplo), convert(transA), convert(diag), m, A, lda, x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtrsv(hipblasHandle_t       handle,
                             hipblasFillMode_t     uplo,
                             hipblasOperation_t    transA,
                             hipblasDiagType_t     diag,
                             int                   m,
                             const hipblasComplex* A,
                             int                   lda,
                             hipblasComplex*       x,
                             int                   incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklCtrsv(sycl_queue, convert(uplo), convert(transA), convert(diag), m,
                (const float _Complex*)A, lda, (float _Complex*)x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtrsv(hipblasHandle_t             handle,
                             hipblasFillMode_t           uplo,
                             hipblasOperation_t          transA,
                             hipblasDiagType_t           diag,
                             int                         m,
                             const hipblasDoubleComplex* A,
                             int                         lda,
                             hipblasDoubleComplex*       x,
                             int                         incx)
try
{
    auto sycl_queue = syclblas_get_sycl_queue((syclblasHandle_t)handle);
    onemklZtrsv(sycl_queue, convert(uplo), convert(transA), convert(diag), m,
                (const double _Complex*)A, lda, (double _Complex*)x, incx);
    return HIPBLAS_STATUS_SUCCESS;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// trsv_batched
hipblasStatus_t hipblasStrsvBatched(hipblasHandle_t    handle,
                                    hipblasFillMode_t  uplo,
                                    hipblasOperation_t transA,
                                    hipblasDiagType_t  diag,
                                    int                m,
                                    const float* const A[],
                                    int                lda,
                                    float* const       x[],
                                    int                incx,
                                    int                batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtrsvBatched(hipblasHandle_t     handle,
                                    hipblasFillMode_t   uplo,
                                    hipblasOperation_t  transA,
                                    hipblasDiagType_t   diag,
                                    int                 m,
                                    const double* const A[],
                                    int                 lda,
                                    double* const       x[],
                                    int                 incx,
                                    int                 batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtrsvBatched(hipblasHandle_t             handle,
                                    hipblasFillMode_t           uplo,
                                    hipblasOperation_t          transA,
                                    hipblasDiagType_t           diag,
                                    int                         m,
                                    const hipblasComplex* const A[],
                                    int                         lda,
                                    hipblasComplex* const       x[],
                                    int                         incx,
                                    int                         batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtrsvBatched(hipblasHandle_t                   handle,
                                    hipblasFillMode_t                 uplo,
                                    hipblasOperation_t                transA,
                                    hipblasDiagType_t                 diag,
                                    int                               m,
                                    const hipblasDoubleComplex* const A[],
                                    int                               lda,
                                    hipblasDoubleComplex* const       x[],
                                    int                               incx,
                                    int                               batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

// trsv_strided_batched
hipblasStatus_t hipblasStrsvStridedBatched(hipblasHandle_t    handle,
                                           hipblasFillMode_t  uplo,
                                           hipblasOperation_t transA,
                                           hipblasDiagType_t  diag,
                                           int                m,
                                           const float*       A,
                                           int                lda,
                                           hipblasStride      strideA,
                                           float*             x,
                                           int                incx,
                                           hipblasStride      stridex,
                                           int                batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasDtrsvStridedBatched(hipblasHandle_t    handle,
                                           hipblasFillMode_t  uplo,
                                           hipblasOperation_t transA,
                                           hipblasDiagType_t  diag,
                                           int                m,
                                           const double*      A,
                                           int                lda,
                                           hipblasStride      strideA,
                                           double*            x,
                                           int                incx,
                                           hipblasStride      stridex,
                                           int                batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasCtrsvStridedBatched(hipblasHandle_t       handle,
                                           hipblasFillMode_t     uplo,
                                           hipblasOperation_t    transA,
                                           hipblasDiagType_t     diag,
                                           int                   m,
                                           const hipblasComplex* A,
                                           int                   lda,
                                           hipblasStride         strideA,
                                           hipblasComplex*       x,
                                           int                   incx,
                                           hipblasStride         stridex,
                                           int                   batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}

hipblasStatus_t hipblasZtrsvStridedBatched(hipblasHandle_t             handle,
                                           hipblasFillMode_t           uplo,
                                           hipblasOperation_t          transA,
                                           hipblasDiagType_t           diag,
                                           int                         m,
                                           const hipblasDoubleComplex* A,
                                           int                         lda,
                                           hipblasStride               strideA,
                                           hipblasDoubleComplex*       x,
                                           int                         incx,
                                           hipblasStride               stridex,
                                           int                         batch_count)
try
{
    return HIPBLAS_STATUS_NOT_SUPPORTED;
}
catch(...)
{
    return exception_to_hipblas_status();
}