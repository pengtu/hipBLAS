/* ************************************************************************
 * Copyright (C) 2016-2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ************************************************************************ */

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <vector>

#include "testing_common.hpp"

/* ============================================================================================ */

using hipblasSbmvStridedBatchedModel = ArgumentModel<e_M,
                                                     e_K,
                                                     e_alpha,
                                                     e_lda,
                                                     e_incx,
                                                     e_beta,
                                                     e_incy,
                                                     e_stride_scale,
                                                     e_batch_count>;

inline void testname_sbmv_strided_batched(const Arguments& arg, std::string& name)
{
    hipblasSbmvStridedBatchedModel{}.test_name(arg, name);
}

template <typename T>
inline hipblasStatus_t testing_sbmv_strided_batched(const Arguments& arg)
{
    bool FORTRAN = arg.fortran;
    auto hipblasSbmvStridedBatchedFn
        = FORTRAN ? hipblasSbmvStridedBatched<T, true> : hipblasSbmvStridedBatched<T, false>;

    int    M            = arg.M;
    int    K            = arg.K;
    int    lda          = arg.lda;
    int    incx         = arg.incx;
    int    incy         = arg.incy;
    double stride_scale = arg.stride_scale;
    int    batch_count  = arg.batch_count;

    int           abs_incx = incx >= 0 ? incx : -incx;
    int           abs_incy = incy >= 0 ? incy : -incy;
    hipblasStride stride_A = size_t(lda) * M * stride_scale;
    hipblasStride stride_x = size_t(M) * abs_incx * stride_scale;
    hipblasStride stride_y = size_t(M) * abs_incy * stride_scale;

    size_t A_size = stride_A * batch_count;
    size_t X_size = stride_x * batch_count;
    size_t Y_size = stride_y * batch_count;

    hipblasFillMode_t uplo = char2hipblas_fill(arg.uplo);

    hipblasLocalHandle handle(arg);

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    bool invalid_size
        = M < 0 || K < 0 || lda < K + 1 || lda < 1 || !incx || !incy || batch_count < 0;
    if(invalid_size || !M || !batch_count)
    {
        hipblasStatus_t actual = hipblasSbmvStridedBatchedFn(handle,
                                                             uplo,
                                                             M,
                                                             K,
                                                             nullptr,
                                                             nullptr,
                                                             lda,
                                                             stride_A,
                                                             nullptr,
                                                             incx,
                                                             stride_x,
                                                             nullptr,
                                                             nullptr,
                                                             incy,
                                                             stride_y,
                                                             batch_count);
        EXPECT_HIPBLAS_STATUS(
            actual, (invalid_size ? HIPBLAS_STATUS_INVALID_VALUE : HIPBLAS_STATUS_SUCCESS));
        return actual;
    }

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    host_vector<T> hA(A_size);
    host_vector<T> hx(X_size);
    host_vector<T> hy(Y_size);
    host_vector<T> hy_cpu(Y_size);
    host_vector<T> hy_host(Y_size);
    host_vector<T> hy_device(Y_size);

    device_vector<T> dA(A_size);
    device_vector<T> dx(X_size);
    device_vector<T> dy(Y_size);
    device_vector<T> d_alpha(1);
    device_vector<T> d_beta(1);

    T h_alpha = arg.get_alpha<T>();
    T h_beta  = arg.get_beta<T>();

    double gpu_time_used, hipblas_error_host, hipblas_error_device;

    // Initial Data on CPU
    hipblas_init_matrix(
        hA, arg, M, M, lda, stride_A, batch_count, hipblas_client_alpha_sets_nan, true);
    hipblas_init_vector(hx, arg, M, abs_incx, stride_x, batch_count, hipblas_client_alpha_sets_nan);
    hipblas_init_vector(hy, arg, M, abs_incy, stride_y, batch_count, hipblas_client_beta_sets_nan);

    hy_cpu = hy;

    // copy data from CPU to device
    CHECK_HIP_ERROR(hipMemcpy(dA, hA.data(), sizeof(T) * A_size, hipMemcpyHostToDevice));
    CHECK_HIP_ERROR(hipMemcpy(dx, hx.data(), sizeof(T) * X_size, hipMemcpyHostToDevice));
    CHECK_HIP_ERROR(hipMemcpy(dy, hy.data(), sizeof(T) * Y_size, hipMemcpyHostToDevice));
    CHECK_HIP_ERROR(hipMemcpy(d_alpha, &h_alpha, sizeof(T), hipMemcpyHostToDevice));
    CHECK_HIP_ERROR(hipMemcpy(d_beta, &h_beta, sizeof(T), hipMemcpyHostToDevice));

    if(arg.unit_check || arg.norm_check)
    {
        /* =====================================================================
            HIPBLAS
        =================================================================== */
        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST));
        CHECK_HIPBLAS_ERROR(hipblasSbmvStridedBatchedFn(handle,
                                                        uplo,
                                                        M,
                                                        K,
                                                        &h_alpha,
                                                        dA,
                                                        lda,
                                                        stride_A,
                                                        dx,
                                                        incx,
                                                        stride_x,
                                                        &h_beta,
                                                        dy,
                                                        incy,
                                                        stride_y,
                                                        batch_count));

        CHECK_HIP_ERROR(hipMemcpy(hy_host.data(), dy, sizeof(T) * Y_size, hipMemcpyDeviceToHost));
        CHECK_HIP_ERROR(hipMemcpy(dy, hy.data(), sizeof(T) * Y_size, hipMemcpyHostToDevice));

        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));
        CHECK_HIPBLAS_ERROR(hipblasSbmvStridedBatchedFn(handle,
                                                        uplo,
                                                        M,
                                                        K,
                                                        d_alpha,
                                                        dA,
                                                        lda,
                                                        stride_A,
                                                        dx,
                                                        incx,
                                                        stride_x,
                                                        d_beta,
                                                        dy,
                                                        incy,
                                                        stride_y,
                                                        batch_count));

        CHECK_HIP_ERROR(hipMemcpy(hy_device.data(), dy, sizeof(T) * Y_size, hipMemcpyDeviceToHost));

        /* =====================================================================
           CPU BLAS
        =================================================================== */
        for(int b = 0; b < batch_count; b++)
        {
            cblas_sbmv<T>(uplo,
                          M,
                          K,
                          h_alpha,
                          hA.data() + b * stride_A,
                          lda,
                          hx.data() + b * stride_x,
                          incx,
                          h_beta,
                          hy_cpu.data() + b * stride_y,
                          incy);
        }

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(arg.unit_check)
        {
            unit_check_general<T>(1, M, batch_count, abs_incy, stride_y, hy_cpu, hy_host);
            unit_check_general<T>(1, M, batch_count, abs_incy, stride_y, hy_cpu, hy_device);
        }
        if(arg.norm_check)
        {
            hipblas_error_host = norm_check_general<T>(
                'F', 1, M, abs_incy, stride_y, hy_cpu, hy_host, batch_count);
            hipblas_error_device = norm_check_general<T>(
                'F', 1, M, abs_incy, stride_y, hy_cpu, hy_device, batch_count);
        }
    }

    if(arg.timing)
    {
        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));
        CHECK_HIP_ERROR(hipMemcpy(dy, hy.data(), sizeof(T) * Y_size, hipMemcpyHostToDevice));
        hipStream_t stream;
        CHECK_HIPBLAS_ERROR(hipblasGetStream(handle, &stream));

        int runs = arg.cold_iters + arg.iters;
        for(int iter = 0; iter < runs; iter++)
        {
            if(iter == arg.cold_iters)
            {
                gpu_time_used = get_time_us_sync(stream);
            }
            CHECK_HIPBLAS_ERROR(hipblasSbmvStridedBatchedFn(handle,
                                                            uplo,
                                                            M,
                                                            K,
                                                            d_alpha,
                                                            dA,
                                                            lda,
                                                            stride_A,
                                                            dx,
                                                            incx,
                                                            stride_x,
                                                            d_beta,
                                                            dy,
                                                            incy,
                                                            stride_y,
                                                            batch_count));
        }

        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        hipblasSbmvStridedBatchedModel{}.log_args<T>(std::cout,
                                                     arg,
                                                     gpu_time_used,
                                                     sbmv_gflop_count<T>(M, K),
                                                     sbmv_gbyte_count<T>(M, K),
                                                     hipblas_error_host,
                                                     hipblas_error_device);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
