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

using hipblasTrmmBatchedModel = ArgumentModel<e_side,
                                              e_uplo,
                                              e_transA,
                                              e_diag,
                                              e_M,
                                              e_N,
                                              e_alpha,
                                              e_lda,
                                              e_ldb,
                                              e_batch_count>;

inline void testname_trmm_batched(const Arguments& arg, std::string& name)
{
    hipblasTrmmBatchedModel{}.test_name(arg, name);
}

template <typename T>
inline hipblasStatus_t testing_trmm_batched(const Arguments& arg)
{
    bool FORTRAN = arg.fortran;
    auto hipblasTrmmBatchedFn
        = FORTRAN ? hipblasTrmmBatched<T, true> : hipblasTrmmBatched<T, false>;

    hipblasSideMode_t  side        = char2hipblas_side(arg.side);
    hipblasFillMode_t  uplo        = char2hipblas_fill(arg.uplo);
    hipblasOperation_t transA      = char2hipblas_operation(arg.transA);
    hipblasDiagType_t  diag        = char2hipblas_diagonal(arg.diag);
    int                M           = arg.M;
    int                N           = arg.N;
    int                lda         = arg.lda;
    int                ldb         = arg.ldb;
    int                batch_count = arg.batch_count;

    T h_alpha = arg.get_alpha<T>();

    hipblasStatus_t status = HIPBLAS_STATUS_SUCCESS;

    int    K      = (side == HIPBLAS_SIDE_LEFT ? M : N);
    size_t A_size = size_t(lda) * K;
    size_t B_size = size_t(ldb) * N;

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    if(M < 0 || N < 0 || lda < K || ldb < M || batch_count < 0)
    {
        return HIPBLAS_STATUS_INVALID_VALUE;
    }
    else if(batch_count == 0)
    {
        return HIPBLAS_STATUS_SUCCESS;
    }

    double gpu_time_used, hipblas_error_host, hipblas_error_device;

    hipblasLocalHandle handle(arg);

    // host arrays
    host_batch_vector<T> hA(A_size, 1, batch_count);
    host_batch_vector<T> hB_host(B_size, 1, batch_count);
    host_batch_vector<T> hB_device(B_size, 1, batch_count);
    host_batch_vector<T> hB_gold(B_size, 1, batch_count);

    // device arrays
    device_batch_vector<T> dA(A_size, 1, batch_count);
    device_batch_vector<T> dB(B_size, 1, batch_count);
    device_vector<T>       d_alpha(1);

    CHECK_HIP_ERROR(dA.memcheck());
    CHECK_HIP_ERROR(dB.memcheck());

    hipblas_init_vector(hA, arg, hipblas_client_alpha_sets_nan, true);
    hipblas_init_vector(hB_host, arg, hipblas_client_alpha_sets_nan, false, true);
    hB_device.copy_from(hB_host);
    hB_gold.copy_from(hB_host);

    CHECK_HIP_ERROR(dA.transfer_from(hA));
    CHECK_HIP_ERROR(dB.transfer_from(hB_host));
    CHECK_HIP_ERROR(hipMemcpy(d_alpha, &h_alpha, sizeof(T), hipMemcpyHostToDevice));

    if(arg.unit_check || arg.norm_check)
    {
        /* =====================================================================
            HIPBLAS
        =================================================================== */
        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST));
        CHECK_HIPBLAS_ERROR(hipblasTrmmBatchedFn(handle,
                                                 side,
                                                 uplo,
                                                 transA,
                                                 diag,
                                                 M,
                                                 N,
                                                 &h_alpha,
                                                 dA.ptr_on_device(),
                                                 lda,
                                                 dB.ptr_on_device(),
                                                 ldb,
                                                 batch_count));

        CHECK_HIP_ERROR(hB_host.transfer_from(dB));
        CHECK_HIP_ERROR(dB.transfer_from(hB_device));

        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));
        CHECK_HIPBLAS_ERROR(hipblasTrmmBatchedFn(handle,
                                                 side,
                                                 uplo,
                                                 transA,
                                                 diag,
                                                 M,
                                                 N,
                                                 d_alpha,
                                                 dA.ptr_on_device(),
                                                 lda,
                                                 dB.ptr_on_device(),
                                                 ldb,
                                                 batch_count));

        CHECK_HIP_ERROR(hB_device.transfer_from(dB));

        /* =====================================================================
           CPU BLAS
        =================================================================== */
        for(int b = 0; b < batch_count; b++)
        {
            cblas_trmm<T>(side, uplo, transA, diag, M, N, h_alpha, hA[b], lda, hB_gold[b], ldb);
        }

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(arg.unit_check)
        {
            unit_check_general<T>(M, N, batch_count, ldb, hB_gold, hB_host);
            unit_check_general<T>(M, N, batch_count, ldb, hB_gold, hB_device);
        }
        if(arg.norm_check)
        {
            hipblas_error_host
                = norm_check_general<T>('F', M, N, ldb, hB_gold, hB_host, batch_count);
            hipblas_error_device
                = norm_check_general<T>('F', M, N, ldb, hB_gold, hB_device, batch_count);
        }
    }

    if(arg.timing)
    {
        hipStream_t stream;
        CHECK_HIPBLAS_ERROR(hipblasGetStream(handle, &stream));
        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));

        int runs = arg.cold_iters + arg.iters;
        for(int iter = 0; iter < runs; iter++)
        {
            if(iter == arg.cold_iters)
                gpu_time_used = get_time_us_sync(stream);

            CHECK_HIPBLAS_ERROR(hipblasTrmmBatchedFn(handle,
                                                     side,
                                                     uplo,
                                                     transA,
                                                     diag,
                                                     M,
                                                     N,
                                                     d_alpha,
                                                     dA.ptr_on_device(),
                                                     lda,
                                                     dB.ptr_on_device(),
                                                     ldb,
                                                     batch_count));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        hipblasTrmmBatchedModel{}.log_args<T>(std::cout,
                                              arg,
                                              gpu_time_used,
                                              trmm_gflop_count<T>(M, N, K),
                                              trmm_gbyte_count<T>(M, N, K),
                                              hipblas_error_host,
                                              hipblas_error_device);
    }

    return HIPBLAS_STATUS_SUCCESS;
}
