#pragma once
#include <cstdint>
struct alignas(16) AbdBatchedPoint { float p1, p2, mu, sigma, sec_mu, sec_sigma, thd, pad; };
static const AbdBatchedPoint test_table[16] = {};
