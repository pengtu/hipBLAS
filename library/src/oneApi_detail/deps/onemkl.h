#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct syclDevice_st *syclDevice_t;
typedef struct syclPlatform_st *syclPlatform_t;
typedef struct syclContext_st *syclContext_t;
typedef struct syclQueue_st *syclQueue_t;
typedef struct syclEvent_st *syclEvent_t;

typedef enum {
    ONEMKL_TRANSPOSE_NONTRANS,
    ONEMKL_TRANSPOSE_TRANS,
    ONEMLK_TRANSPOSE_CONJTRANS
} onemklTranspose;

typedef enum {
    ONEMKL_UPLO_UPPER,
    ONEMKL_UPLO_LOWER
} onemklUplo;

typedef enum {
    ONEMKL_DIAG_NONUNIT,
    ONEMKL_DIAG_UNIT
 } onemklDiag;

// XXX: how to expose half in C?
// int onemklHgemm(syclQueue_t device_queue, onemklTranspose transA,
//                onemklTranspose transB, int64_t m, int64_t n, int64_t k,
//                half alpha, const half *A, int64_t lda, const half *B,
//                int64_t ldb, half beta, half *C, int64_t ldc);
int onemklSgemm(syclQueue_t device_queue, onemklTranspose transA,
                onemklTranspose transB, int64_t m, int64_t n, int64_t k,
                float alpha, const float *A, int64_t lda, const float *B,
                int64_t ldb, float beta, float *C, int64_t ldc);
int onemklDgemm(syclQueue_t device_queue, onemklTranspose transA,
                onemklTranspose transB, int64_t m, int64_t n, int64_t k,
                double alpha, const double *A, int64_t lda, const double *B,
                int64_t ldb, double beta, double *C, int64_t ldc);
int onemklCgemm(syclQueue_t device_queue, onemklTranspose transA,
                onemklTranspose transB, int64_t m, int64_t n, int64_t k,
                float _Complex alpha, const float _Complex *A, int64_t lda,
                const float _Complex *B, int64_t ldb, float _Complex beta,
                float _Complex *C, int64_t ldc);
int onemklZgemm(syclQueue_t device_queue, onemklTranspose transA,
                onemklTranspose transB, int64_t m, int64_t n, int64_t k,
                double _Complex alpha, const double _Complex *A, int64_t lda,
                const double _Complex *B, int64_t ldb, double _Complex beta,
                double _Complex *C, int64_t ldc);

void onemklSasum(syclQueue_t device_queue, int64_t n,
                const float *x, int64_t incx, float *result);
void onemklDasum(syclQueue_t device_queue, int64_t n,
                const double *x, int64_t incx, double *result);
void onemklCasum(syclQueue_t device_queue, int64_t n,
                const float _Complex *x, int64_t incx, float *result);
void onemklZasum(syclQueue_t device_queue, int64_t n,
                const double _Complex *x, int64_t incx, double *result);

void onemklSaxpy(syclQueue_t device_queue, int64_t n, float alpha, const float *x,
                int64_t incx, float *y, int64_t incy);
void onemklDaxpy(syclQueue_t device_queue, int64_t n, double alpha, const double *x,
                int64_t incx, double *y, int64_t incy);
void onemklCaxpy(syclQueue_t device_queue, int64_t n, float _Complex alpha,
                const float _Complex *x, int64_t incx, float _Complex *y, int64_t incy);
void onemklZaxpy(syclQueue_t device_queue, int64_t n, double _Complex alpha,
                const double _Complex *x, int64_t incx, double _Complex *y, int64_t incy);

// Level-1: scal oneMKL
void onemklDscal(syclQueue_t device_queue, int64_t n, double alpha,
                double *x, int64_t incx);
void onemklSscal(syclQueue_t device_queue, int64_t n, float alpha,
                float *x, int64_t incx);
void onemklCscal(syclQueue_t device_queue, int64_t n, float _Complex alpha,
                float _Complex *x, int64_t incx);
void onemklCsscal(syclQueue_t device_queue, int64_t n, float alpha,
                float _Complex *x, int64_t incx);
void onemklZscal(syclQueue_t device_queue, int64_t n, double _Complex alpha,
                double _Complex *x, int64_t incx);
void onemklZdscal(syclQueue_t device_queue, int64_t n, double alpha,
                double _Complex *x, int64_t incx);

// Supported Level-1: Nrm2
void onemklDnrm2(syclQueue_t device_queue, int64_t n, const double *x,
                 int64_t incx, double *result);
void onemklSnrm2(syclQueue_t device_queue, int64_t n, const float *x,
                 int64_t incx, float *result);
void onemklCnrm2(syclQueue_t device_queue, int64_t n, const float _Complex *x,
                 int64_t incx, float *result);
void onemklZnrm2(syclQueue_t device_queue, int64_t n, const double _Complex *x,
                 int64_t incx, double *result);

void onemklSdot(syclQueue_t device_queue, int64_t n, const float *x,
                int64_t incx, const float *y, int64_t incy, float *result);
void onemklDdot(syclQueue_t device_queue, int64_t n, const double *x,
                int64_t incx, const double *y, int64_t incy, double *result);
void onemklCdotc(syclQueue_t device_queue, int64_t n, const float _Complex *x,
                int64_t incx, const float _Complex *y, int64_t incy,
                float _Complex *result);
void onemklZdotc(syclQueue_t device_queue, int64_t n, const double _Complex *x,
                int64_t incx, const double _Complex *y, int64_t incy,
                double _Complex *result);
void onemklCdotu(syclQueue_t device_queue, int64_t n, const float _Complex *x,
                int64_t incx, const float _Complex *y, int64_t incy,
                float _Complex *result);
void onemklZdotu(syclQueue_t device_queue, int64_t n, const double _Complex *x,
                int64_t incx, const double _Complex *y, int64_t incy,
                double _Complex *result);

void onemklDcopy(syclQueue_t device_queue, int64_t n, const double *x,
                 int64_t incx, double *y, int64_t incy);
void onemklScopy(syclQueue_t device_queue, int64_t n, const float *x,
                 int64_t incx, float *y, int64_t incy);
void onemklZcopy(syclQueue_t device_queue, int64_t n, const double _Complex *x,
                 int64_t incx, double _Complex *y, int64_t incy);
void onemklCcopy(syclQueue_t device_queue, int64_t n, const float _Complex *x,
                 int64_t incx, float _Complex *y, int64_t incy);

void onemklDamax(syclQueue_t device_queue, int64_t n, const double *x, int64_t incx,
                 int64_t *result);
void onemklSamax(syclQueue_t device_queue, int64_t n, const float  *x, int64_t incx,
                 int64_t *result);
void onemklZamax(syclQueue_t device_queue, int64_t n, const double _Complex *x, int64_t incx,
                 int64_t *result);
void onemklCamax(syclQueue_t device_queue, int64_t n, const float _Complex *x, int64_t incx,
                 int64_t *result);

void onemklDamin(syclQueue_t device_queue, int64_t n, const double *x, int64_t incx,
                 int64_t *result);
void onemklSamin(syclQueue_t device_queue, int64_t n, const float  *x, int64_t incx,
                 int64_t *result);
void onemklZamin(syclQueue_t device_queue, int64_t n, const double _Complex *x, int64_t incx,
                 int64_t *result);
void onemklCamin(syclQueue_t device_queue, int64_t n, const float _Complex *x, int64_t incx,
                 int64_t *result);

void onemklSswap(syclQueue_t device_queue, int64_t n, float *x, int64_t incx,
                float *y, int64_t incy);
void onemklDswap(syclQueue_t device_queue, int64_t n, double *x, int64_t incx,
                double *y, int64_t incy);
void onemklCswap(syclQueue_t device_queue, int64_t n, float _Complex *x, int64_t incx,
                float _Complex *y, int64_t incy);
void onemklZswap(syclQueue_t device_queue, int64_t n, double _Complex *x, int64_t incx,
                double _Complex *y, int64_t incy);

void onemklSrot(syclQueue_t device_queue, int n, float* x, int incx, float* y, int incy,
                const float c, const float s);
void onemklDrot(syclQueue_t device_queue, int n, double* x, int incx, double* y, int incy,
                const double c, const double s);
void onemklCrot(syclQueue_t device_queue, int n, float _Complex* x, int incx, float _Complex* y, int incy,
                const float c, const float _Complex s);
void onemklCsrot(syclQueue_t device_queue, int n, float _Complex* x, int incx, float _Complex* y, int incy,
                const float c, const float s);
void onemklZrot(syclQueue_t device_queue, int n, double _Complex* x, int incx, double _Complex* y, int incy,
                const double c, const double _Complex s);
void onemklZdrot(syclQueue_t device_queue, int n, double _Complex* x, int incx, double _Complex* y, int incy,
                const double c, const double s);

void onemklSrotg(syclQueue_t device_queue, float* a, float* b, float* c, float* s);
void onemklDrotg(syclQueue_t device_queue, double* a, double* b, double* c, double* s);
void onemklCrotg(syclQueue_t device_queue, float _Complex* a, float _Complex* b, float* c, float _Complex* s);
void onemklZrotg(syclQueue_t device_queue, double _Complex* a, double _Complex* b, double* c, double _Complex* s);

void onemklSrotm(syclQueue_t device_queue, int64_t n, float *x, int64_t incx,
                float *y, int64_t incy, float* param);
void onemklDrotm(syclQueue_t device_queue, int64_t n, double *x, int64_t incx,
                double *y, int64_t incy, double* param);

// Level-2
void onemklSgbmv(syclQueue_t device_queue, onemklTranspose trans, int64_t m,
                int64_t n, int64_t kl, int64_t ku, float alpha, const float *a,
                int64_t lda, const float *x, int64_t incx, float beta, float *y,
                int64_t incy);
void onemklDgbmv(syclQueue_t device_queue, onemklTranspose trans, int64_t m,
                int64_t n, int64_t kl, int64_t ku, double alpha, const double *a,
                int64_t lda, const double *x, int64_t incx, double beta, double *y,
                int64_t incy);
void onemklCgbmv(syclQueue_t device_queue, onemklTranspose trans, int64_t m,
                int64_t n, int64_t kl, int64_t ku, float _Complex alpha, const float
                _Complex *a, int64_t lda, const float _Complex *x, int64_t incx,
                float _Complex beta, float _Complex *y, int64_t incy);
void onemklZgbmv(syclQueue_t device_queue, onemklTranspose trans, int64_t m,
                int64_t n, int64_t kl, int64_t ku, double _Complex alpha,
                const double _Complex *a, int64_t lda, const double _Complex *x,
                int64_t incx, double _Complex beta, double _Complex *y, int64_t incy);

void onemklSgemv(syclQueue_t device_queue, onemklTranspose trans, int64_t m,
                 int64_t n, float alpha, const float *a, int64_t lda,
                 const float *x, int64_t incx, float beta, float *y, int64_t incy);
void onemklDgemv(syclQueue_t device_queue, onemklTranspose trans, int64_t m,
                 int64_t n, double alpha, const double *a, int64_t lda,
                 const double *x, int64_t incx, double beta, double *y, int64_t incy);
void onemklCgemv(syclQueue_t device_queue, onemklTranspose trans, int64_t m,
                 int64_t n, float _Complex alpha, const float _Complex *a, int64_t lda,
                 const float _Complex *x, int64_t incx, float _Complex beta,
                 float _Complex *y, int64_t incy);
void onemklZgemv(syclQueue_t device_queue, onemklTranspose trans, int64_t m,
                 int64_t n, double _Complex alpha, const double _Complex *a, int64_t lda,
                 const double _Complex *x, int64_t incx, double _Complex beta,
                 double _Complex *y, int64_t incy);

void onemklSger(syclQueue_t device_queue, int64_t m, int64_t n, float alpha,
                const float *x, int64_t incx, const float *y, int64_t incy,
                float *a, int64_t lda);
void onemklDger(syclQueue_t device_queue, int64_t m, int64_t n, double alpha,
                const double *x, int64_t incx, const double *y, int64_t incy,
                double *a, int64_t lda);
void onemklCgerc(syclQueue_t device_queue, int64_t m, int64_t n, float _Complex alpha,
                const float _Complex *x, int64_t incx, const float _Complex *y, int64_t incy,
                float _Complex *a, int64_t lda);
void onemklCgeru(syclQueue_t device_queue, int64_t m, int64_t n, float _Complex alpha,
                const float _Complex *x, int64_t incx, const float _Complex *y, int64_t incy,
                float _Complex *a, int64_t lda);
void onemklZgerc(syclQueue_t device_queue, int64_t m, int64_t n, double _Complex alpha,
                const double _Complex *x, int64_t incx, const double _Complex *y, int64_t incy,
                double _Complex *a, int64_t lda);
void onemklZgeru(syclQueue_t device_queue, int64_t m, int64_t n, double _Complex alpha,
                const double _Complex *x, int64_t incx, const double _Complex *y, int64_t incy,
                double _Complex *a, int64_t lda);

void onemklChbmv(syclQueue_t device_queue, onemklUplo uplo, int64_t n,
                int64_t k, float _Complex alpha, const float _Complex *a,
                int64_t lda, const float _Complex *x, int64_t incx, float _Complex beta,
                float _Complex *y, int64_t incy);
void onemklZhbmv(syclQueue_t device_queue, onemklUplo uplo, int64_t n,
                int64_t k, double _Complex alpha, const double _Complex *a,
                int64_t lda, const double _Complex *x, int64_t incx, double _Complex beta,
                double _Complex *y, int64_t incy);

void onemklChemv(syclQueue_t device_queue, onemklUplo uplo, int64_t n,
                float _Complex alpha, const float _Complex *a, int64_t lda,
                const float _Complex *x, int64_t incx, float _Complex beta,
                float _Complex *y, int64_t incy);
void onemklZhemv(syclQueue_t device_queue, onemklUplo uplo, int64_t n,
                double _Complex alpha, const double _Complex *a, int64_t lda,
                const double _Complex *x, int64_t incx, double _Complex beta,
                double _Complex *y, int64_t incy);

void onemklCher(syclQueue_t device_queue, onemklUplo uplo, int64_t n, float alpha,
                const float _Complex *x, int64_t incx, float _Complex *a,
                int64_t lda);
void onemklZher(syclQueue_t device_queue, onemklUplo uplo, int64_t n, double alpha,
                const double _Complex *x, int64_t incx, double _Complex *a,
                int64_t lda);

void onemklCher2(syclQueue_t device_queue, onemklUplo uplo, int64_t n, float _Complex alpha,
                const float _Complex *x, int64_t incx, const float _Complex *y, int64_t incy,
                float _Complex *a, int64_t lda);
void onemklZher2(syclQueue_t device_queue, onemklUplo uplo, int64_t n, double _Complex alpha,
                const double _Complex *x, int64_t incx, const double _Complex *y, int64_t incy,
                double _Complex *a, int64_t lda);

void onemklChpmv(syclQueue_t device_queue, onemklUplo uplo, int64_t n, float _Complex alpha,
                const float _Complex *a, const float _Complex *x, int64_t incx,
                float _Complex beta, float _Complex *y, int64_t incy);
void onemklZhpmv(syclQueue_t device_queue, onemklUplo uplo, int64_t n, double _Complex alpha,
                const double _Complex *a, const double _Complex *x, int64_t incx,
                double _Complex beta, double _Complex *y, int64_t incy);

void onemklChpr(syclQueue_t device_queue, onemklUplo uplo, int64_t n, float alpha,
                const float _Complex *x, int64_t incx, float _Complex *a);
void onemklZhpr(syclQueue_t device_queue, onemklUplo uplo, int64_t n, double alpha,
                const double _Complex *x, int64_t incx, double _Complex *a);

void onemklChpr2(syclQueue_t device_queue, onemklUplo uplo, int64_t n, float _Complex alpha,
                const float _Complex *x, int64_t incx, const float _Complex *y, int64_t incy, float _Complex *a);
void onemklZhpr2(syclQueue_t device_queue, onemklUplo uplo, int64_t n, double _Complex alpha,
                const double _Complex *x, int64_t incx, const double _Complex *y, int64_t incy, double _Complex *a);

void onemklSsbmv(syclQueue_t device_queue, onemklUplo uplo, int64_t n, int64_t k,
                 float alpha, const float *a, int64_t lda, const float *x,
                 int64_t incx, float beta, float *y, int64_t incy);
void onemklDsbmv(syclQueue_t device_queue, onemklUplo uplo, int64_t n, int64_t k,
                 double alpha, const double *a, int64_t lda, const double *x,
                 int64_t incx, double beta, double *y, int64_t incy);

void onemklSspmv(syclQueue_t device_queue, onemklUplo uplo, int64_t n,
                 float alpha, const float *a, const float *x,
                 int64_t incx, float beta, float *y, int64_t incy);
void onemklDspmv(syclQueue_t device_queue, onemklUplo uplo, int64_t n,
                 double alpha, const double *a, const double *x,
                 int64_t incx, double beta, double *y, int64_t incy);

void onemklSspr(syclQueue_t device_queue, onemklUplo uplo, int64_t n,
                 float alpha, const float *x, int64_t incx, float *a);
void onemklDspr(syclQueue_t device_queue, onemklUplo uplo, int64_t n,
                 double alpha, const double *x, int64_t incx, double *a);

void onemklSspr2(syclQueue_t device_queue, onemklUplo uplo, int64_t n,
                 float alpha, const float *x, int64_t incx,
                 const float *y, int64_t incy, float *a);
void onemklDspr2(syclQueue_t device_queue, onemklUplo uplo, int64_t n,
                 double alpha, const double *x, int64_t incx,
                 const double *y, int64_t incy, double *a);

void onemklSsymv(syclQueue_t device_queue, onemklUplo uplo, int64_t n, float alpha,
                 const float *a, int64_t lda, const float *x, int64_t incx, float beta,
                 float *y, int64_t incy);
void onemklDsymv(syclQueue_t device_queue, onemklUplo uplo, int64_t n,
                 double alpha, const double *a, int64_t lda, const double *x,
                 int64_t incx, double beta, double *y, int64_t incy);

void onemklSsyr(syclQueue_t device_queue, onemklUplo uplo, int64_t n, float alpha,
                           const float *x, int64_t incx, float *a, int64_t lda);
void onemklDsyr(syclQueue_t device_queue, onemklUplo uplo, int64_t n, double alpha,
                           const double *x, int64_t incx, double *a, int64_t lda);

void onemklSsyr2(syclQueue_t device_queue, onemklUplo uplo, int64_t n, float alpha,
                           const float *x, int64_t incx, const float *y, int64_t incy, float *a, int64_t lda);
void onemklDsyr2(syclQueue_t device_queue, onemklUplo uplo, int64_t n, double alpha,
                           const double *x, int64_t incx, const double *y, int64_t incy, double *a, int64_t lda);

void onemklStbmv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t n,
                int64_t k, const float *a, int64_t lda, float *x, int64_t incx);

void onemklDtbmv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t n,
                int64_t k, const double *a, int64_t lda, double *x, int64_t incx);

void onemklCtbmv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t n,
                int64_t k, const float _Complex *a, int64_t lda, float _Complex *x,
                int64_t incx);

void onemklZtbmv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t n,
                int64_t k, const double _Complex *a, int64_t lda, double _Complex *x,
                int64_t incx);

void onemklStbsv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t n,
                int64_t k, const float *a, int64_t lda, float *x, int64_t incx);

void onemklDtbsv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t n,
                int64_t k, const double *a, int64_t lda, double *x, int64_t incx);

void onemklCtbsv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t n,
                int64_t k, const float _Complex *a, int64_t lda, float _Complex *x,
                int64_t incx);

void onemklZtbsv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t n,
                int64_t k, const double _Complex *a, int64_t lda, double _Complex *x,
                int64_t incx);

void onemklStpmv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t n,
                const float *a, float *x, int64_t incx);

void onemklDtpmv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t n,
                const double *a, double *x, int64_t incx);

void onemklCtpmv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t n,
                const float _Complex *a, float _Complex *x, int64_t incx);

void onemklZtpmv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t n,
                const double _Complex *a, double _Complex *x, int64_t incx);

void onemklStpsv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t m,
                const float *a, float *x, int64_t incx);

void onemklDtpsv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t m,
                const double *a, double *x, int64_t incx);

void onemklCtpsv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t m,
                const float _Complex *a, float _Complex *x, int64_t incx);

void onemklZtpsv(syclQueue_t device_queue, onemklUplo uplo,
                onemklTranspose trans, onemklDiag diag, int64_t m,
                const double _Complex *a, double _Complex *x, int64_t incx);

void onemklStrmv(syclQueue_t device_queue, onemklUplo uplo, onemklTranspose trans,
                onemklDiag diag, int64_t n, const float *a, int64_t lda, float *x,
                int64_t incx);

void onemklDtrmv(syclQueue_t device_queue, onemklUplo uplo, onemklTranspose trans,
                onemklDiag diag, int64_t n, const double *a, int64_t lda, double *x,
                int64_t incx);

void onemklCtrmv(syclQueue_t device_queue, onemklUplo uplo, onemklTranspose trans,
                onemklDiag diag, int64_t n, const float _Complex *a, int64_t lda, float _Complex *x,
                int64_t incx);

void onemklZtrmv(syclQueue_t device_queue, onemklUplo uplo, onemklTranspose trans,
                onemklDiag diag, int64_t n, const double _Complex *a, int64_t lda, double _Complex *x,
                int64_t incx);

// trsv
void onemklStrsv(syclQueue_t device_queue, onemklUplo uplo, onemklTranspose trans,
                onemklDiag diag, int64_t n, const float *a, int64_t lda, float *x,
                int64_t incx);

void onemklDtrsv(syclQueue_t device_queue, onemklUplo uplo, onemklTranspose trans,
                onemklDiag diag, int64_t n, const double *a, int64_t lda, double *x,
                int64_t incx);

void onemklCtrsv(syclQueue_t device_queue, onemklUplo uplo, onemklTranspose trans,
                onemklDiag diag, int64_t n, const float _Complex *a, int64_t lda, float _Complex *x,
                int64_t incx);

void onemklZtrsv(syclQueue_t device_queue, onemklUplo uplo, onemklTranspose trans,
                onemklDiag diag, int64_t n, const double _Complex *a, int64_t lda, double _Complex *x,
                int64_t incx);
void onemklDestroy();
#ifdef __cplusplus
}
#endif
