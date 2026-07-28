/**
 * XPU - samples/mnist_train/main.cpp
 *
 * MNIST-like digit classification demo using a small CNN.
 *
 * Since downloading the real MNIST dataset requires network access,
 * this demo uses SYNTHETIC digits: simple 8x8 patterns that look
 * like 0s and 1s. The architecture is real though - a 1-layer CNN
 * that learns to classify them with >95% accuracy.
 *
 * Architecture:
 *   Input (1, 1, 8, 8)  →  Conv2d(1->4, 3x3) + ReLU  →  (1, 4, 6, 6)
 *                        →  Flatten (144)
 *                        →  Dense(144 -> 2) + Softmax
 *                        →  Output (2 classes: digit 0 or digit 1)
 *
 * Build: make build/xpu_mnist_train
 * Run  : LD_LIBRARY_PATH=build ./build/xpu_mnist_train
 */

#include "xpu/xpu.h"
#include "xpu/xpu_tensor.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>

/* Synthetic dataset: 8x8 binary images.
 * Class 0 = "zero" pattern (ring shape)
 * Class 1 = "one" pattern (vertical line) */
#define IMG_SIZE 8
#define N_CLASSES 2
#define N_SAMPLES 40  /* 20 per class */

/* Generate a "zero" pattern with some noise */
static void gen_zero(float* img, uint32_t seed) {
    /* Ring shape: filled circle with hole in middle */
    uint32_t state = seed;
    auto rnd = [&]() { state = state * 1103515245u + 12345u; return (state >> 8) / 16777216.0f; };
    for (int y = 0; y < IMG_SIZE; ++y) {
        for (int x = 0; x < IMG_SIZE; ++x) {
            float dx = x - 3.5f, dy = y - 3.5f;
            float dist = std::sqrt(dx*dx + dy*dy);
            /* Ring: outer radius 3, inner radius 1.5 */
            float v = (dist < 3.0f && dist > 1.5f) ? 1.0f : 0.0f;
            /* Add some noise */
            v += (rnd() - 0.5f) * 0.2f;
            img[y * IMG_SIZE + x] = v > 0 ? v : 0;
        }
    }
}

/* Generate a "one" pattern with some noise */
static void gen_one(float* img, uint32_t seed) {
    uint32_t state = seed;
    auto rnd = [&]() { state = state * 1103515245u + 12345u; return (state >> 8) / 16777216.0f; };
    for (int y = 0; y < IMG_SIZE; ++y) {
        for (int x = 0; x < IMG_SIZE; ++x) {
            /* Vertical line at x=3..4 */
            float v = (x >= 3 && x <= 4) ? 1.0f : 0.0f;
            /* Add a small diagonal at top (serif) */
            if (y <= 1 && x == 2) v = 0.7f;
            if (y <= 1 && x == 5) v = 0.7f;
            v += (rnd() - 0.5f) * 0.2f;
            img[y * IMG_SIZE + x] = v > 0 ? v : 0;
        }
    }
}

int main() {
    std::printf("=== XPU MNIST-like Training Demo ===\n");
    std::printf("XPU version   : %s\n", xpuGetVersionString());
    std::printf("CPU detected  : %s\n", xpu_math_arch_name(xpu_math_detect_cpu_arch()));
    std::printf("Problem       : classify synthetic digits (0 vs 1)\n");
    std::printf("Architecture  : Conv2d(1->4, 3x3) + ReLU -> Dense(144->2) + Softmax\n");
    std::printf("Dataset       : %d samples (%d per class)\n\n", N_SAMPLES, N_SAMPLES / 2);

    /* Generate dataset */
    XpuTensor X = xpu_tensor_create_4d(N_SAMPLES, 1, IMG_SIZE, IMG_SIZE);
    XpuTensor Y = xpu_tensor_create_2d(N_SAMPLES, N_CLASSES);
    xpu_tensor_fill_zero(Y);

    for (uint32_t i = 0; i < N_SAMPLES; ++i) {
        float* img = X->data + (size_t)i * IMG_SIZE * IMG_SIZE;
        if (i < N_SAMPLES / 2) {
            gen_zero(img, 100 + i);
            Y->data[i * N_CLASSES + 0] = 1.0f;  /* class 0 */
        } else {
            gen_one(img, 200 + i);
            Y->data[i * N_CLASSES + 1] = 1.0f;  /* class 1 */
        }
    }

    /* Print sample images as ASCII art */
    std::printf("Sample 'zero' (class 0):\n");
    for (int y = 0; y < IMG_SIZE; ++y) {
        std::printf("  ");
        for (int x = 0; x < IMG_SIZE; ++x) {
            float v = X->data[y * IMG_SIZE + x];
            std::printf("%c", v > 0.5f ? '#' : v > 0.2f ? '.' : ' ');
        }
        std::printf("\n");
    }
    std::printf("\nSample 'one' (class 1):\n");
    for (int y = 0; y < IMG_SIZE; ++y) {
        std::printf("  ");
        for (int x = 0; x < IMG_SIZE; ++x) {
            float v = X->data[(size_t)(N_SAMPLES/2) * IMG_SIZE * IMG_SIZE + y * IMG_SIZE + x];
            std::printf("%c", v > 0.5f ? '#' : v > 0.2f ? '.' : ' ');
        }
        std::printf("\n");
    }
    std::printf("\n");

    /* ----- Model parameters ----- */
    /* Conv layer: 4 filters of size 1x3x3 = 9 params each, total 36 + 4 bias */
    const uint32_t COUT = 4, KH = 3, KW = 3;
    XpuTensor conv_w = xpu_tensor_create_4d(COUT, 1, KH, KW);
    xpu_tensor_init_glorot(conv_w, 1 * KH * KW, COUT * KH * KW, 42);
    XpuTensor conv_b = xpu_tensor_create_1d(COUT);
    xpu_tensor_fill_zero(conv_b);

    /* After conv: (N, 4, 6, 6) = 144 features per sample */
    const uint32_t FLAT = COUT * (IMG_SIZE - KH + 1) * (IMG_SIZE - KW + 1);  /* 4*6*6=144 */
    XpuTensor fc_w = xpu_tensor_create_2d(FLAT, N_CLASSES);
    xpu_tensor_init_glorot(fc_w, FLAT, N_CLASSES, 43);
    XpuTensor fc_b = xpu_tensor_create_1d(N_CLASSES);
    xpu_tensor_fill_zero(fc_b);

    /* Adam state for each param */
    XpuAdamState* adam_cw = xpu_optim_adam_init(conv_w);
    XpuAdamState* adam_cb = xpu_optim_adam_init(conv_b);
    XpuAdamState* adam_fw = xpu_optim_adam_init(fc_w);
    XpuAdamState* adam_fb = xpu_optim_adam_init(fc_b);

    /* Forward-pass buffers */
    XpuTensor conv_out = xpu_tensor_create_4d(N_SAMPLES, COUT, IMG_SIZE - KH + 1, IMG_SIZE - KW + 1);
    XpuTensor flat     = xpu_tensor_create_2d(N_SAMPLES, FLAT);
    XpuTensor logits   = xpu_tensor_create_2d(N_SAMPLES, N_CLASSES);
    XpuTensor probs    = xpu_tensor_create_2d(N_SAMPLES, N_CLASSES);

    /* Gradient buffers */
    XpuTensor dlogits = xpu_tensor_create_2d(N_SAMPLES, N_CLASSES);
    XpuTensor dfc_b   = xpu_tensor_create_1d(N_CLASSES);
    XpuTensor dfc_w   = xpu_tensor_create_2d(FLAT, N_CLASSES);
    XpuTensor dflat   = xpu_tensor_create_2d(N_SAMPLES, FLAT);
    XpuTensor dconv_out = xpu_tensor_create_4d(N_SAMPLES, COUT, IMG_SIZE - KH + 1, IMG_SIZE - KW + 1);
    XpuTensor dconv_b = xpu_tensor_create_1d(COUT);
    XpuTensor dconv_w = xpu_tensor_create_4d(COUT, 1, KH, KW);

    /* Transpose flat for fc_w gradient: dflat^T * dlogits / actually dfc_w = flat^T * dlogits */
    XpuTensor flat_T  = xpu_tensor_create_2d(FLAT, N_SAMPLES);
    XpuTensor dflat_T_logits = xpu_tensor_create_2d(FLAT, N_CLASSES);

    /* Training loop */
    const uint32_t EPOCHS = 200;
    const float LR = 0.01f;
    std::printf("Training %u epochs (lr=%.3f)...\n\n", EPOCHS, LR);
    std::printf("epoch   loss   accuracy\n");

    for (uint32_t epoch = 0; epoch < EPOCHS; ++epoch) {
        /* ===== FORWARD ===== */
        xpu_tensor_conv2d(X, conv_w, conv_b, conv_out);
        xpu_tensor_relu_inplace(conv_out);

        /* Flatten conv_out -> flat (N, 144) */
        std::memcpy(flat->data, conv_out->data, flat->size * sizeof(float));

        /* Dense: logits = flat * fc_w + fc_b */
        xpu_tensor_matmul_add_bias(flat, fc_w, fc_b, logits);

        /* Softmax */
        xpu_tensor_softmax(logits, probs);

        /* ===== LOSS (cross-entropy) ===== */
        /* loss = -mean(sum(Y * log(probs))) */
        float loss = 0;
        for (uint32_t i = 0; i < N_SAMPLES; ++i) {
            for (uint32_t c = 0; c < N_CLASSES; ++c) {
                float p = probs->data[i * N_CLASSES + c];
                if (p < 1e-7f) p = 1e-7f;
                loss -= Y->data[i * N_CLASSES + c] * std::log(p);
            }
        }
        loss /= N_SAMPLES;

        /* ===== ACCURACY ===== */
        int correct = 0;
        for (uint32_t i = 0; i < N_SAMPLES; ++i) {
            float p0 = probs->data[i * N_CLASSES + 0];
            float p1 = probs->data[i * N_CLASSES + 1];
            int pred = p1 > p0 ? 1 : 0;
            int actual = Y->data[i * N_CLASSES + 1] > 0.5f ? 1 : 0;
            if (pred == actual) ++correct;
        }
        float acc = 100.0f * correct / N_SAMPLES;

        if (epoch % 20 == 0 || epoch == EPOCHS - 1) {
            std::printf("%5u %7.4f  %5.1f%%\n", epoch, loss, acc);
        }

        /* ===== BACKWARD ===== */
        /* dlogits = (probs - Y) / N  (softmax + cross-entropy simplified gradient) */
        float inv_n = 1.0f / N_SAMPLES;
        for (uint32_t i = 0; i < N_SAMPLES; ++i) {
            for (uint32_t c = 0; c < N_CLASSES; ++c) {
                dlogits->data[i * N_CLASSES + c] =
                    inv_n * (probs->data[i * N_CLASSES + c] - Y->data[i * N_CLASSES + c]);
            }
        }

        /* dfc_b = sum(dlogits, axis=0) */
        xpu_tensor_sum_axis(dlogits, 0, dfc_b);

        /* dfc_w = flat^T * dlogits  (FLAT x N_CLASSES) */
        xpu_tensor_transpose_2d(flat, flat_T);
        xpu_tensor_matmul(flat_T, dlogits, dfc_w);

        /* dflat = dlogits * fc_w^T  (N x FLAT) - need fc_w transposed */
        XpuTensor fc_w_T = xpu_tensor_create_2d(N_CLASSES, FLAT);
        xpu_tensor_transpose_2d(fc_w, fc_w_T);
        xpu_tensor_matmul(dlogits, fc_w_T, dflat);
        xpu_tensor_destroy(fc_w_T);

        /* Backprop through ReLU */
        for (size_t i = 0; i < conv_out->size; ++i) {
            if (conv_out->data[i] <= 0) dflat->data[i] = 0;
        }

        /* dconv_out = dflat (reshape) - already same memory layout */
        std::memcpy(dconv_out->data, dflat->data, dconv_out->size * sizeof(float));

        /* Conv gradient: for simplicity, compute dconv_b and dconv_w with scalar loops */
        /* dconv_b[co] = sum over (n, oh, ow) of dconv_out */
        xpu_tensor_fill_zero(dconv_b);
        for (uint32_t co = 0; co < COUT; ++co) {
            float s = 0;
            for (uint32_t n = 0; n < N_SAMPLES; ++n) {
                for (uint32_t i = 0; i < (IMG_SIZE - KH + 1) * (IMG_SIZE - KW + 1); ++i) {
                    size_t idx = (((size_t)n * COUT + co) * (IMG_SIZE - KH + 1) * (IMG_SIZE - KW + 1)) + i;
                    s += dconv_out->data[idx];
                }
            }
            dconv_b->data[co] = s;
        }

        /* dconv_w[co, ci, kh, kw] = sum over (n, oh, ow) of
         *                           dconv_out[n, co, oh, ow] * X[n, ci, oh+kh, ow+kw] */
        xpu_tensor_fill_zero(dconv_w);
        for (uint32_t co = 0; co < COUT; ++co) {
            for (uint32_t ci = 0; ci < 1; ++ci) {
                for (uint32_t kh = 0; kh < KH; ++kh) {
                    for (uint32_t kw = 0; kw < KW; ++kw) {
                        float s = 0;
                        for (uint32_t n = 0; n < N_SAMPLES; ++n) {
                            for (uint32_t oh = 0; oh < IMG_SIZE - KH + 1; ++oh) {
                                for (uint32_t ow = 0; ow < IMG_SIZE - KW + 1; ++ow) {
                                    size_t do_idx = (((size_t)n * COUT + co) * (IMG_SIZE - KH + 1) + oh) * (IMG_SIZE - KW + 1) + ow;
                                    size_t in_idx = (((size_t)n * 1 + ci) * IMG_SIZE + (oh + kh)) * IMG_SIZE + (ow + kw);
                                    s += dconv_out->data[do_idx] * X->data[in_idx];
                                }
                            }
                        }
                        size_t dw_idx = (((size_t)co * 1 + ci) * KH + kh) * KW + kw;
                        dconv_w->data[dw_idx] = s;
                    }
                }
            }
        }

        /* ===== OPTIMIZER STEP ===== */
        xpu_optim_adam_step(adam_cw, conv_w, dconv_w, LR);
        xpu_optim_adam_step(adam_cb, conv_b, dconv_b, LR);
        xpu_optim_adam_step(adam_fw, fc_w, dfc_w, LR);
        xpu_optim_adam_step(adam_fb, fc_b, dfc_b, LR);
    }

    /* ===== FINAL EVALUATION ===== */
    std::printf("\n=== Final Evaluation ===\n");
    /* Forward pass */
    xpu_tensor_conv2d(X, conv_w, conv_b, conv_out);
    xpu_tensor_relu_inplace(conv_out);
    std::memcpy(flat->data, conv_out->data, flat->size * sizeof(float));
    xpu_tensor_matmul_add_bias(flat, fc_w, fc_b, logits);
    xpu_tensor_softmax(logits, probs);

    int correct = 0;
    for (uint32_t i = 0; i < N_SAMPLES; ++i) {
        float p0 = probs->data[i * N_CLASSES + 0];
        float p1 = probs->data[i * N_CLASSES + 1];
        int pred = p1 > p0 ? 1 : 0;
        int actual = Y->data[i * N_CLASSES + 1] > 0.5f ? 1 : 0;
        if (pred == actual) ++correct;
    }
    float final_acc = 100.0f * correct / N_SAMPLES;
    std::printf("Final accuracy: %.1f%% (%d/%d correct)\n", final_acc, correct, N_SAMPLES);
    std::printf("\nSample predictions:\n");
    for (uint32_t i = 0; i < 8; ++i) {
        float p0 = probs->data[i * N_CLASSES + 0];
        float p1 = probs->data[i * N_CLASSES + 1];
        int pred = p1 > p0 ? 1 : 0;
        int actual = Y->data[i * N_CLASSES + 1] > 0.5f ? 1 : 0;
        std::printf("  sample %2d: pred=%d (p0=%.2f, p1=%.2f), actual=%d %s\n",
                      i, pred, p0, p1, actual, pred == actual ? "✓" : "✗");
    }

    if (final_acc >= 95.0f) {
        std::printf("\n🎉 SUCCESS! CNN learned to classify digits with %.1f%% accuracy!\n", final_acc);
        std::printf("This proves XPU can train CNNs with:\n");
        std::printf("  - 2D convolution layer (real conv2d implementation)\n");
        std::printf("  - ReLU activation\n");
        std::printf("  - Fully connected layer\n");
        std::printf("  - Softmax + cross-entropy loss\n");
        std::printf("  - Full backprop through all layers\n");
    }

    /* Cleanup */
    xpu_optim_adam_destroy(adam_cw);
    xpu_optim_adam_destroy(adam_cb);
    xpu_optim_adam_destroy(adam_fw);
    xpu_optim_adam_destroy(adam_fb);
    xpu_tensor_destroy(X); xpu_tensor_destroy(Y);
    xpu_tensor_destroy(conv_w); xpu_tensor_destroy(conv_b);
    xpu_tensor_destroy(fc_w); xpu_tensor_destroy(fc_b);
    xpu_tensor_destroy(conv_out); xpu_tensor_destroy(flat);
    xpu_tensor_destroy(logits); xpu_tensor_destroy(probs);
    xpu_tensor_destroy(dlogits); xpu_tensor_destroy(dfc_b);
    xpu_tensor_destroy(dfc_w); xpu_tensor_destroy(dflat);
    xpu_tensor_destroy(dconv_out); xpu_tensor_destroy(dconv_b);
    xpu_tensor_destroy(dconv_w);
    xpu_tensor_destroy(flat_T); xpu_tensor_destroy(dflat_T_logits);

    return 0;
}
