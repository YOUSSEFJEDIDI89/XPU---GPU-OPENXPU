/**
 * XPU - tests/test_tensor.cpp
 *
 * Unit tests for the Tensor API.
 */

#include "xpu/xpu_tensor.h"
#include <cstdio>
#include <cmath>
#include <cstring>

static int g_failures = 0;

static void check(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_failures;
    } else {
        std::printf("ok   : %s\n", msg);
    }
}

static bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}

int main() {
    std::printf("=== XPU Tensor Tests ===\n");

    /* Create + size */
    XpuTensor t = xpu_tensor_create_2d(3, 4);
    check(t != nullptr, "tensor_create_2d");
    check(xpu_tensor_ndim(t) == 2, "tensor ndim");
    check(xpu_tensor_dim(t, 0) == 3 && xpu_tensor_dim(t, 1) == 4, "tensor shape");
    check(xpu_tensor_size(t) == 12, "tensor size");

    /* Fill constant + sum */
    xpu_tensor_fill_constant(t, 2.0f);
    check(approx(xpu_tensor_sum(t), 24.0f), "fill_constant + sum");
    check(approx(xpu_tensor_mean(t), 2.0f), "mean");

    /* Element-wise: relu */
    xpu_tensor_fill_constant(t, -1.5f);
    XpuTensor r = xpu_tensor_create_2d(3, 4);
    xpu_tensor_relu(t, r);
    check(approx(xpu_tensor_sum(r), 0.0f), "relu of negatives = 0");

    /* Set + get */
    uint32_t idx[2] = {1, 2};
    xpu_tensor_set(t, idx, 7.5f);
    check(approx(xpu_tensor_get(t, idx), 7.5f), "set/get");

    /* Matmul: 2x3 * 3x2 = 2x2 */
    XpuTensor A = xpu_tensor_create_2d(2, 3);
    XpuTensor B = xpu_tensor_create_2d(3, 2);
    XpuTensor C = xpu_tensor_create_2d(2, 2);
    /* A = [[1,2,3],[4,5,6]] */
    A->data[0]=1; A->data[1]=2; A->data[2]=3;
    A->data[3]=4; A->data[4]=5; A->data[5]=6;
    /* B = [[7,8],[9,10],[11,12]] */
    B->data[0]=7;  B->data[1]=8;
    B->data[2]=9;  B->data[3]=10;
    B->data[4]=11; B->data[5]=12;
    /* Expected C = [[1*7+2*9+3*11, 1*8+2*10+3*12],
     *               [4*7+5*9+6*11, 4*8+5*10+6*12]]
     *           = [[58, 64], [139, 154]] */
    xpu_tensor_matmul(A, B, C);
    check(approx(C->data[0], 58) && approx(C->data[1], 64) &&
          approx(C->data[2], 139) && approx(C->data[3], 154),
          "matmul 2x3 * 3x2");

    /* Transpose 2D */
    XpuTensor At = xpu_tensor_create_2d(3, 2);
    xpu_tensor_transpose_2d(A, At);
    check(approx(At->data[0], 1) && approx(At->data[1], 4) &&
          approx(At->data[2], 2) && approx(At->data[3], 5),
          "transpose_2d");

    /* Sigmoid */
    XpuTensor s = xpu_tensor_create_1d(3);
    XpuTensor so = xpu_tensor_create_1d(3);
    s->data[0] = 0;     /* sigmoid(0) = 0.5 */
    s->data[1] = 100;   /* sigmoid(100) ≈ 1 */
    s->data[2] = -100;  /* sigmoid(-100) ≈ 0 */
    xpu_tensor_sigmoid(s, so);
    check(approx(so->data[0], 0.5f) &&
          approx(so->data[1], 1.0f, 1e-3f) &&
          approx(so->data[2], 0.0f, 1e-3f),
          "sigmoid");

    /* MSE loss */
    XpuTensor p = xpu_tensor_create_1d(3);
    XpuTensor y = xpu_tensor_create_1d(3);
    p->data[0]=1; p->data[1]=2; p->data[2]=3;
    y->data[0]=1; y->data[1]=2; y->data[2]=4;
    /* MSE = ((0)^2 + (0)^2 + (-1)^2) / 3 = 0.333 */
    check(approx(xpu_loss_mse(p, y), 0.3333f), "MSE loss");

    /* SGD optimizer */
    XpuTensor w = xpu_tensor_create_1d(2);
    XpuTensor g = xpu_tensor_create_1d(2);
    w->data[0]=1; w->data[1]=2;
    g->data[0]=0.1f; g->data[1]=0.2f;
    /* After SGD with lr=0.5: w -= 0.5 * g = [0.95, 1.9] */
    xpu_optim_sgd(w, g, 0.5f);
    check(approx(w->data[0], 0.95f) && approx(w->data[1], 1.9f), "SGD step");

    /* Adam - just verify it doesn't crash */
    XpuTensor w2 = xpu_tensor_create_1d(4);
    xpu_tensor_fill_constant(w2, 0.5f);
    XpuTensor g2 = xpu_tensor_create_1d(4);
    xpu_tensor_fill_constant(g2, 0.1f);
    XpuAdamState* adam = xpu_optim_adam_init(w2);
    xpu_optim_adam_step(adam, w2, g2, 0.01f);
    check(true, "Adam step (no crash)");
    xpu_optim_adam_destroy(adam);

    /* Cleanup */
    xpu_tensor_destroy(t); xpu_tensor_destroy(r);
    xpu_tensor_destroy(A); xpu_tensor_destroy(B); xpu_tensor_destroy(C);
    xpu_tensor_destroy(At);
    xpu_tensor_destroy(s); xpu_tensor_destroy(so);
    xpu_tensor_destroy(p); xpu_tensor_destroy(y);
    xpu_tensor_destroy(w); xpu_tensor_destroy(g);
    xpu_tensor_destroy(w2); xpu_tensor_destroy(g2);

    if (g_failures) {
        std::fprintf(stderr, "\n%d test(s) failed\n", g_failures);
        return 1;
    }
    std::printf("\nAll tensor tests passed!\n");
    return 0;
}
