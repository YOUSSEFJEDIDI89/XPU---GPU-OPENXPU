/**
 * XPU - xpu_tensor.h
 *
 * Tensor operations API for machine learning on XPU.
 *
 * This API provides:
 *   - N-dimensional tensors (up to 4D: NCHW)
 *   - Matrix multiplication (CPU SIMD + future Vulkan compute)
 *   - Element-wise ops: ReLU, sigmoid, tanh, softmax
 *   - 2D convolution (for CNNs)
 *   - Reductions: sum, mean, max
 *   - Loss functions: MSE, cross-entropy
 *   - Optimizers: SGD, Adam
 *
 * All operations work on the CPU using NEON (ARM) or SSE/AVX (x86),
 * with a planned Vulkan compute backend for true GPU acceleration
 * (no root required — Vulkan is a public Android API).
 *
 * Designed to be small, embeddable, and dependency-free.
 */

#ifndef XPU_TENSOR_H
#define XPU_TENSOR_H

#include "xpu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Tensor handle                                                      */
/* ------------------------------------------------------------------ */
#define XPU_TENSOR_MAX_DIMS 4

typedef enum XpuTensorDevice {
    XPU_TENSOR_CPU = 0,           /* system RAM (always available) */
    XPU_TENSOR_GPU_COMPUTE = 1,   /* Vulkan compute shader (when available) */
} XpuTensorDevice;

typedef struct XpuTensor_T {
    uint32_t shape[XPU_TENSOR_MAX_DIMS];   /* NCHW (or fewer dims) */
    uint32_t ndim;
    size_t   size;                         /* total element count */
    float*   data;                         /* contiguous, 16-byte aligned */
    XpuTensorDevice device;
    int      owns_data;                    /* 1 = we malloc'd it */
} XpuTensor_T;

typedef XpuTensor_T* XpuTensor;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                          */
/* ------------------------------------------------------------------ */

XPU_API XpuTensor xpu_tensor_create(const uint32_t* shape, uint32_t ndim);
XPU_API XpuTensor xpu_tensor_create_1d(uint32_t n);
XPU_API XpuTensor xpu_tensor_create_2d(uint32_t rows, uint32_t cols);
XPU_API XpuTensor xpu_tensor_create_3d(uint32_t d0, uint32_t d1, uint32_t d2);
XPU_API XpuTensor xpu_tensor_create_4d(uint32_t n, uint32_t c, uint32_t h, uint32_t w);
XPU_API void      xpu_tensor_destroy(XpuTensor t);

XPU_API XpuTensor xpu_tensor_clone(const XpuTensor src);

/* ------------------------------------------------------------------ */
/* Initialization                                                     */
/* ------------------------------------------------------------------ */

XPU_API void xpu_tensor_fill_zero(XpuTensor t);
XPU_API void xpu_tensor_fill_constant(XpuTensor t, float value);
XPU_API void xpu_tensor_fill_random_normal(XpuTensor t, float mean, float stddev, uint32_t seed);
XPU_API void xpu_tensor_fill_random_uniform(XpuTensor t, float min, float max, uint32_t seed);

/* Glorot/Xavier initialization - good default for tanh/sigmoid */
XPU_API void xpu_tensor_init_glorot(XpuTensor t, uint32_t fan_in, uint32_t fan_out, uint32_t seed);
/* He initialization - good default for ReLU */
XPU_API void xpu_tensor_init_he(XpuTensor t, uint32_t fan_in, uint32_t seed);

/* ------------------------------------------------------------------ */
/* Basic accessors                                                    */
/* ------------------------------------------------------------------ */

XPU_API size_t   xpu_tensor_size(const XpuTensor t);
XPU_API uint32_t xpu_tensor_ndim(const XpuTensor t);
XPU_API uint32_t xpu_tensor_dim(const XpuTensor t, uint32_t axis);
XPU_API float*   xpu_tensor_data(XpuTensor t);
XPU_API const float* xpu_tensor_data_const(const XpuTensor t);

XPU_API void     xpu_tensor_set(XpuTensor t, const uint32_t* indices, float value);
XPU_API float    xpu_tensor_get(const XpuTensor t, const uint32_t* indices);

XPU_API void     xpu_tensor_print(const XpuTensor t, const char* name);

/* ------------------------------------------------------------------ */
/* Element-wise operations (output to a new tensor or in-place)       */
/* ------------------------------------------------------------------ */

XPU_API void xpu_tensor_relu(const XpuTensor x, XpuTensor out);
XPU_API void xpu_tensor_relu_inplace(XpuTensor x);

XPU_API void xpu_tensor_sigmoid(const XpuTensor x, XpuTensor out);
XPU_API void xpu_tensor_tanh(const XpuTensor x, XpuTensor out);
XPU_API void xpu_tensor_softmax(const XpuTensor x, XpuTensor out);

/* Derivatives (used in backprop) */
XPU_API void xpu_tensor_relu_grad(const XpuTensor x, const XpuTensor grad, XpuTensor out);
XPU_API void xpu_tensor_sigmoid_grad_from_output(const XpuTensor out, const XpuTensor grad, XpuTensor dx);

/* ------------------------------------------------------------------ */
/* Matrix operations                                                  */
/* ------------------------------------------------------------------ */

/* C = A * B  (A: M×K, B: K×N, C: M×N) — uses SIMD */
XPU_API void xpu_tensor_matmul(const XpuTensor A, const XpuTensor B, XpuTensor C);

/* C = A * B + bias — for dense layers */
XPU_API void xpu_tensor_matmul_add_bias(const XpuTensor A, const XpuTensor B,
                                          const XpuTensor bias, XpuTensor C);

/* Transpose a 2D tensor */
XPU_API void xpu_tensor_transpose_2d(const XpuTensor src, XpuTensor dst);

/* 2D convolution: out = conv2d(input, kernel) + bias
 * input:  (N, C_in, H, W)
 * kernel: (C_out, C_in, KH, KW)
 * bias:   (C_out,) or NULL
 * output: (N, C_out, OH, OW) where OH = H - KH + 1, OW = W - KW + 1
 * (valid padding, no stride) */
XPU_API void xpu_tensor_conv2d(const XpuTensor input,
                                 const XpuTensor kernel,
                                 const XpuTensor bias,
                                 XpuTensor output);

/* Element-wise add: out = a + b */
XPU_API void xpu_tensor_add(const XpuTensor a, const XpuTensor b, XpuTensor out);
XPU_API void xpu_tensor_sub(const XpuTensor a, const XpuTensor b, XpuTensor out);
XPU_API void xpu_tensor_mul(const XpuTensor a, const XpuTensor b, XpuTensor out);

/* Scale: out = a * scalar */
XPU_API void xpu_tensor_scale(const XpuTensor a, float scalar, XpuTensor out);

/* ------------------------------------------------------------------ */
/* Reductions                                                         */
/* ------------------------------------------------------------------ */

XPU_API float xpu_tensor_sum(const XpuTensor t);
XPU_API float xpu_tensor_mean(const XpuTensor t);
XPU_API float xpu_tensor_max(const XpuTensor t);

/* Sum along an axis (e.g., for batch reduction in loss) */
XPU_API void xpu_tensor_sum_axis(const XpuTensor t, uint32_t axis, XpuTensor out);

/* ------------------------------------------------------------------ */
/* Loss functions                                                     */
/* ------------------------------------------------------------------ */

/* MSE loss: L = mean((pred - target)^2) */
XPU_API float xpu_loss_mse(const XpuTensor pred, const XpuTensor target);

/* MSE gradient: dL/d_pred = 2 * (pred - target) / N */
XPU_API void xpu_loss_mse_grad(const XpuTensor pred, const XpuTensor target, XpuTensor grad);

/* Binary cross-entropy: L = -mean(target * log(pred) + (1-target) * log(1-pred)) */
XPU_API float xpu_loss_bce(const XpuTensor pred, const XpuTensor target);

/* BCE gradient: dL/d_pred = (pred - target) / (pred * (1 - pred) * N) */
XPU_API void xpu_loss_bce_grad(const XpuTensor pred, const XpuTensor target, XpuTensor grad);

/* ------------------------------------------------------------------ */
/* Optimizers                                                         */
/* ------------------------------------------------------------------ */

/* SGD: param -= lr * grad */
XPU_API void xpu_optim_sgd(XpuTensor param, const XpuTensor grad, float lr);

/* SGD with momentum: v = momentum * v + grad; param -= lr * v */
typedef struct XpuSgdState {
    XpuTensor velocity;
    float     momentum;
} XpuSgdState;

XPU_API XpuSgdState* xpu_optim_sgd_init(const XpuTensor param, float momentum);
XPU_API void         xpu_optim_sgd_step(XpuSgdState* state, XpuTensor param,
                                          const XpuTensor grad, float lr);
XPU_API void         xpu_optim_sgd_destroy(XpuSgdState* state);

/* Adam optimizer (Kingma & Ba 2014) */
typedef struct XpuAdamState {
    XpuTensor m;           /* first moment */
    XpuTensor v;           /* second moment */
    uint32_t  t;           /* timestep */
    float     beta1;
    float     beta2;
    float     eps;
} XpuAdamState;

XPU_API XpuAdamState* xpu_optim_adam_init(const XpuTensor param);
XPU_API void          xpu_optim_adam_step(XpuAdamState* state, XpuTensor param,
                                            const XpuTensor grad, float lr);
XPU_API void          xpu_optim_adam_destroy(XpuAdamState* state);

#ifdef __cplusplus
}
#endif

#endif /* XPU_TENSOR_H */
