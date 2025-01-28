// random.h
#ifndef RANDOM_H
#define RANDOM_H

#include <stdint.h>
#include <time.h>
#include <windows.h>
#include <math.h>


// ====================== 随机数生成核心 ======================
typedef struct {
    uint64_t state[2];  // 内部状态（128位）
} xorshift128p_state;

// 初始化种子（使用系统时间 + 内存地址哈希）
static void xorshift128p_seed(xorshift128p_state *state) {
    // 获取纳秒级时间（兼容不同平台）
    uint64_t seed = (uint64_t)time(NULL);
    #ifdef _WIN32
        LARGE_INTEGER freq, counter;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&counter);
        seed ^= (counter.QuadPart * 11400714819323198485ULL) >> 32;
    #else
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        seed ^= (ts.tv_nsec * 11400714819323198485ULL) >> 32;
    #endif

    // 混合内存地址哈希（增加熵）
    seed ^= (uint64_t)((uintptr_t)&seed) * 0x9E3779B97F4A7C15;

    // 初始化状态
    state->state[0] = seed;
    state->state[1] = seed ^ 0x6A09E667F3BCC909;
}

// 生成 64 位随机数（核心算法）
static uint64_t xorshift128p_next(xorshift128p_state *state) {
    uint64_t s1 = state->state[0];
    const uint64_t s0 = state->state[1];
    state->state[0] = s0;
    s1 ^= s1 << 23;
    state->state[1] = s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26);
    return state->state[1] + s0;
}

// ====================== 用户接口 ======================
// 生成 [0,1) 范围内的均匀分布浮点数
float random_float() {
    static xorshift128p_state state;
    static int seeded = 0;
    if (!seeded) {
        xorshift128p_seed(&state);
        seeded = 1;
    }
    return (xorshift128p_next(&state) >> 11) * (1.0 / (UINT64_C(1) << 53));
}

// 生成正态分布随机数（Box-Muller 变换）
float random_normal(float mean, float stddev) {
    static float n2 = 0.0f;
    static int n2_cached = 0;
    if (!n2_cached) {
        float x, y, r;
        do {
            x = 2.0f * random_float() - 1.0f;
            y = 2.0f * random_float() - 1.0f;
            r = x*x + y*y;
        } while (r >= 1.0f || r == 0.0f);
        float d = sqrtf(-2.0f * logf(r) / r);
        n2 = y * d;
        n2_cached = 1;
        return x * d * stddev + mean;
    } else {
        n2_cached = 0;
        return n2 * stddev + mean;
    }
}

// 生成 [min, max] 的整数（快速通用版）
static int random_int(int min, int max) {
    if (min > max) { 
        int temp = min;
        min = max;
        max = temp;
    }
    int range = max - min + 1;
    return (int)(random_float() * range) + min;
}

// 生成 [min, max] 的整数（高精度版，适用于大范围）
static int random_int_high_precision(int min, int max) {
    static xorshift128p_state state;
    static int seeded = 0;
    if (!seeded) {
        xorshift128p_seed(&state);
        seeded = 1;
    }

    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    uint64_t range = (uint64_t)max - min + 1;
    if (range == 0) return min; // 处理全范围溢出
    
    uint64_t limit = UINT64_MAX - (UINT64_MAX % range);
    uint64_t r;
    do {
        r = xorshift128p_next(&state);
    } while (r >= limit);
    return (int)(r % range) + min;
}

#endif // RANDOM_H