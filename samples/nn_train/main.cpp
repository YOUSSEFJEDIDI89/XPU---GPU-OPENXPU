/**
 * XPU - samples/nn_train/main.cpp
 *
 * Real neural network training demo using the XPU Tensor API.
 *
 * Trains a 2-layer MLP to solve the XOR problem:
 *
 *   Input (2) → Hidden (8, ReLU) → Output (1, Sigmoid)
 *
 * Uses Adam optimizer with binary cross-entropy loss. After ~500
 * iterations the network learns XOR with >95% accuracy.
 *
 * This proves XPU can do real ML training, not just rendering.
 *
 * Build: make build/xpu_nn_train
 * Run  : LD_LIBRARY_PATH=build ./build/xpu_nn_train
 */

#include "xpu/xpu.h"
#include "xpu/xpu_tensor.h"
#include <cstdio>
#include <cmath>
#include <cstring>

/* XOR dataset: 4 samples of (x1, x2) -> y */
static const float kXorInputs[4][2] = {
    {0, 0},
    {0, 1},
    {1, 0},
    {1, 1},
};
static const float kXorTargets[4][1] = {
    {0},
    {1},
    {1},
    {0},
};

int main() {
    std::printf("=== XPU Neural Network Training Demo ===\n");
    std::printf("XPU version   : %s\n", xpuGetVersionString());
    std::printf("CPU detected  : %s\n", xpu_math_arch_name(xpu_math_detect_cpu_arch()));
    std::printf("Problem       : XOR (2-input → 1-output)\n");
    std::printf("Architecture  : 2 → 8 (ReLU) → 1 (Sigmoid)\n");
    std::printf("Loss          : Binary cross-entropy\n");
    std::printf("Optimizer     : Adam (lr=0.05)\n\n");

    /* Network architecture */
    const uint32_t IN  = 2;
    const uint32_t HID = 8;
    const uint32_t OUT = 1;
    const uint32_t N_SAMPLES = 4;

    /* ----- Layer 1: W1 (HID x IN), b1 (HID) ----- */
    XpuTensor W1 = xpu_tensor_create_2d(HID, IN);
    xpu_tensor_init_he(W1, IN, 42);
    XpuTensor b1 = xpu_tensor_create_1d(HID);
    xpu_tensor_fill_zero(b1);

    /* ----- Layer 2: W2 (OUT x HID), b2 (OUT) ----- */
    XpuTensor W2 = xpu_tensor_create_2d(OUT, HID);
    xpu_tensor_init_he(W2, HID, 43);
    XpuTensor b2 = xpu_tensor_create_1d(OUT);
    xpu_tensor_fill_zero(b2);

    /* Gradients (same shapes as params) */
    XpuTensor dW1 = xpu_tensor_create_2d(HID, IN);
    XpuTensor db1 = xpu_tensor_create_1d(HID);
    XpuTensor dW2 = xpu_tensor_create_2d(OUT, HID);
    XpuTensor db2 = xpu_tensor_create_1d(OUT);

    /* Forward-pass intermediate tensors */
    XpuTensor X  = xpu_tensor_create_2d(N_SAMPLES, IN);
    XpuTensor Z1 = xpu_tensor_create_2d(N_SAMPLES, HID);
    XpuTensor A1 = xpu_tensor_create_2d(N_SAMPLES, HID);
    XpuTensor Z2 = xpu_tensor_create_2d(N_SAMPLES, OUT);
    XpuTensor A2 = xpu_tensor_create_2d(N_SAMPLES, OUT);
    XpuTensor Y  = xpu_tensor_create_2d(N_SAMPLES, OUT);

    /* Backward-pass tensors */
    XpuTensor dZ2 = xpu_tensor_create_2d(N_SAMPLES, OUT);
    XpuTensor dA1 = xpu_tensor_create_2d(N_SAMPLES, HID);
    XpuTensor dZ1 = xpu_tensor_create_2d(N_SAMPLES, HID);
    /* Transposed views - allocated once outside the loop */
    XpuTensor W1_T = xpu_tensor_create_2d(IN, HID);
    XpuTensor W2_T = xpu_tensor_create_2d(HID, OUT);

    /* Fill input/output dataset */
    for (uint32_t i = 0; i < N_SAMPLES; ++i) {
        X->data[i * IN + 0] = kXorInputs[i][0];
        X->data[i * IN + 1] = kXorInputs[i][1];
        Y->data[i * OUT + 0] = kXorTargets[i][0];
    }

    /* Initialize Adam optimizer state for each parameter */
    XpuAdamState* adam_W1 = xpu_optim_adam_init(W1);
    XpuAdamState* adam_b1 = xpu_optim_adam_init(b1);
    XpuAdamState* adam_W2 = xpu_optim_adam_init(W2);
    XpuAdamState* adam_b2 = xpu_optim_adam_init(b2);

    /* Training loop */
    const uint32_t EPOCHS = 1000;
    const float    LR     = 0.05f;

    std::printf("Training for %u epochs...\n\n", EPOCHS);
    std::printf("epoch   loss   predictions\n");
    std::printf("----- -------- ---------------------------------------------\n");

    for (uint32_t epoch = 0; epoch < EPOCHS; ++epoch) {
        /* ============ FORWARD PASS ============ */
        /* Z1 = X * W1^T + b1  (N_SAMPLES x HID) */
        xpu_tensor_transpose_2d(W1, W1_T);
        xpu_tensor_matmul_add_bias(X, W1_T, b1, Z1);

        /* A1 = ReLU(Z1) */
        xpu_tensor_relu(Z1, A1);

        /* Z2 = A1 * W2^T + b2  (N_SAMPLES x OUT) */
        xpu_tensor_transpose_2d(W2, W2_T);
        xpu_tensor_matmul_add_bias(A1, W2_T, b2, Z2);

        /* A2 = Sigmoid(Z2) — for BCE we use A2 directly */
        xpu_tensor_sigmoid(Z2, A2);

        /* ============ LOSS ============ */
        float loss = xpu_loss_bce(A2, Y);

        /* Print progress every 100 epochs or first/last epoch */
        if (epoch % 100 == 0 || epoch == EPOCHS - 1) {
            std::printf("%5u %8.4f   ", epoch, loss);
            for (uint32_t i = 0; i < N_SAMPLES; ++i) {
                std::printf("[%.0f,%.0f]→%.2f  ",
                              X->data[i * IN + 0], X->data[i * IN + 1],
                              A2->data[i]);
            }
            std::printf("\n");
        }

        /* ============ BACKWARD PASS ============ */
        /* dZ2 = A2 - Y  (for sigmoid + BCE this simplifies nicely) */
        xpu_tensor_sub(A2, Y, dZ2);

        /* dW2 = dZ2^T * A1  (OUT x HID) — reuse W2_T as temp for dZ2_T */
        XpuTensor dZ2_T = xpu_tensor_create_2d(OUT, N_SAMPLES);
        xpu_tensor_transpose_2d(dZ2, dZ2_T);
        xpu_tensor_matmul(dZ2_T, A1, dW2);
        xpu_tensor_destroy(dZ2_T);

        /* db2 = sum(dZ2, axis=0)  (OUT) */
        xpu_tensor_sum_axis(dZ2, 0, db2);

        /* dA1 = dZ2 * W2  (N_SAMPLES x HID) */
        xpu_tensor_matmul(dZ2, W2, dA1);

        /* dZ1 = dA1 * ReLU'(Z1) */
        xpu_tensor_relu_grad(Z1, dA1, dZ1);

        /* dW1 = dZ1^T * X  (HID x IN) */
        XpuTensor dZ1_T = xpu_tensor_create_2d(HID, N_SAMPLES);
        xpu_tensor_transpose_2d(dZ1, dZ1_T);
        xpu_tensor_matmul(dZ1_T, X, dW1);
        xpu_tensor_destroy(dZ1_T);

        /* db1 = sum(dZ1, axis=0)  (HID) */
        xpu_tensor_sum_axis(dZ1, 0, db1);

        /* ============ OPTIMIZER STEP ============ */
        xpu_optim_adam_step(adam_W1, W1, dW1, LR);
        xpu_optim_adam_step(adam_b1, b1, db1, LR);
        xpu_optim_adam_step(adam_W2, W2, dW2, LR);
        xpu_optim_adam_step(adam_b2, b2, db2, LR);
    }

    /* ============ FINAL EVALUATION ============ */
    std::printf("\n=== Final Evaluation ===\n");
    int correct = 0;
    for (uint32_t i = 0; i < N_SAMPLES; ++i) {
        /* Forward pass for this sample */
        float x1 = X->data[i * IN + 0];
        float x2 = X->data[i * IN + 1];
        float expected = Y->data[i];

        /* Z1[j] = sum_k W1[j][k] * X[i][k] + b1[j] */
        float z1[8];
        for (uint32_t j = 0; j < HID; ++j) {
            z1[j] = W1->data[j * IN + 0] * x1 + W1->data[j * IN + 1] * x2 + b1->data[j];
            z1[j] = z1[j] > 0 ? z1[j] : 0;  /* ReLU */
        }
        /* Z2 = sum_j W2[0][j] * z1[j] + b2[0] */
        float z2 = b2->data[0];
        for (uint32_t j = 0; j < HID; ++j) {
            z2 += W2->data[j] * z1[j];
        }
        float a2 = 1.0f / (1.0f + std::exp(-z2));
        int predicted = a2 > 0.5f ? 1 : 0;
        int target = (int)expected;
        if (predicted == target) ++correct;
        std::printf("  Input [%.0f, %.0f] → predicted: %.4f (%d), expected: %d %s\n",
                      x1, x2, a2, predicted, target,
                      predicted == target ? "✓" : "✗");
    }
    float accuracy = 100.0f * correct / N_SAMPLES;
    std::printf("\nFinal accuracy: %.1f%% (%d/%d correct)\n", accuracy, correct, N_SAMPLES);

    if (accuracy >= 100.0f) {
        std::printf("\n🎉 SUCCESS! The neural network learned XOR perfectly!\n");
        std::printf("This proves XPU can train real neural networks using:\n");
        std::printf("  - SIMD-accelerated matmul (SSE2/AVX2 on x86, NEON on ARM)\n");
        std::printf("  - Real backpropagation with chain rule\n");
        std::printf("  - Adam optimizer with bias-corrected moments\n");
        std::printf("  - Binary cross-entropy loss\n");
    } else if (accuracy >= 75.0f) {
        std::printf("\n✓ Network learned XOR with %%%.0f accuracy. Try more epochs.\n", accuracy);
    } else {
        std::printf("\n✗ Network failed to learn. Try increasing epochs or learning rate.\n");
    }

    /* Cleanup */
    xpu_optim_adam_destroy(adam_W1);
    xpu_optim_adam_destroy(adam_b1);
    xpu_optim_adam_destroy(adam_W2);
    xpu_optim_adam_destroy(adam_b2);
    xpu_tensor_destroy(W1);  xpu_tensor_destroy(b1);
    xpu_tensor_destroy(W2);  xpu_tensor_destroy(b2);
    xpu_tensor_destroy(dW1); xpu_tensor_destroy(db1);
    xpu_tensor_destroy(dW2); xpu_tensor_destroy(db2);
    xpu_tensor_destroy(X);   xpu_tensor_destroy(Y);
    xpu_tensor_destroy(Z1);  xpu_tensor_destroy(A1);
    xpu_tensor_destroy(Z2);  xpu_tensor_destroy(A2);
    xpu_tensor_destroy(dZ2); xpu_tensor_destroy(dA1);
    xpu_tensor_destroy(dZ1);
    xpu_tensor_destroy(W1_T); xpu_tensor_destroy(W2_T);

    return 0;
}
