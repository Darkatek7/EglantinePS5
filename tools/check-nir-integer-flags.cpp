// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the production NIR flag emitter through Mesa's real constant-folding pass.
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>

#include "nir_integer_flags.h"

static unsigned cases;

static void Check(uint32_t lhs, uint32_t rhs, unsigned operation = 0, unsigned offset = 0,
                  unsigned count = 32) {
    static const nir_shader_compiler_options options{};
    nir_builder b = nir_builder_init_simple_shader(MESA_SHADER_COMPUTE, &options, "integer_flags");
    nir_def* a = nir_imm_int(&b, lhs);
    nir_def* c = nir_imm_int(&b, rhs);
    nir_def* result{};
    uint32_t word{};
    switch (operation) {
    case 0: result = nir_iadd(&b, a, c); word = lhs + rhs; break;
    case 1: result = nir_iand(&b, a, c); word = lhs & rhs; break;
    case 2: result = nir_ior(&b, a, c); word = lhs | rhs; break;
    case 3: result = nir_ixor(&b, a, c); word = lhs ^ rhs; break;
    case 4:
    case 5: {
        nir_def* off = nir_imm_int(&b, offset);
        nir_def* width = nir_imm_int(&b, count);
        result = operation == 4 ? nir_ubitfield_extract(&b, a, off, width)
                                : nir_ibitfield_extract(&b, a, off, width);
        const uint32_t mask = uint32_t((uint64_t(1) << count) - 1);
        word = count == 0 ? 0 : (lhs >> offset) & mask;
        if (operation == 5 && count != 0 && (word & (uint32_t(1) << (count - 1)))) {
            word |= ~mask;
        }
        break;
    }
    default: std::abort();
    }
    constexpr std::array flags{Eden::Nir::IntegerFlag::Zero, Eden::Nir::IntegerFlag::Sign,
                               Eden::Nir::IntegerFlag::Carry, Eden::Nir::IntegerFlag::Overflow};
    std::array<nir_intrinsic_instr*, 4> stores{};
    const unsigned flag_count = operation == 0 ? 4 : 2;
    for (unsigned i = 0; i < flag_count; ++i) {
        nir_def* flag = Eden::Nir::EmitIntegerFlag(&b, flags[i], result, a, c);
        if (flag->bit_size != 1 || flag->num_components != 1) std::abort();
        stores[i] = _nir_build_store_ssbo(
            &b, nir_b2i32(&b, flag), nir_imm_int(&b, 0), nir_imm_int(&b, i * 4),
            _nir_store_ssbo_indices{0, 1, static_cast<gl_access_qualifier>(0), 4, 0, 0});
    }
    while (nir_opt_constant_folding(b.shader)) {}
    const uint64_t wide = uint64_t(lhs) + rhs;
    const int64_t signed_wide = int64_t(std::bit_cast<int32_t>(lhs)) +
                                int64_t(std::bit_cast<int32_t>(rhs));
    const std::array expected{word == 0, (word & 0x80000000U) != 0,
                              wide > UINT32_MAX,
                              signed_wide < INT32_MIN || signed_wide > INT32_MAX};
    for (unsigned i = 0; i < flag_count; ++i) {
        if (!nir_src_is_const(stores[i]->src[0]) ||
            nir_src_as_uint(stores[i]->src[0]) != unsigned(expected[i])) {
            std::fprintf(stderr, "flag %u mismatch for %08x + %08x\n", i, lhs, rhs);
            std::abort();
        }
    }
    ++cases;
    ralloc_free(b.shader);
}

int main() {
    glsl_type_singleton_init_or_ref();
    constexpr std::array<uint32_t, 12> edges{0, 1, 2, 0x7ffffffe, 0x7fffffff, 0x80000000,
                                             0x80000001, 0xfffffffe, 0xffffffff,
                                             0x55555555, 0xaaaaaaaa, 0x40000000};
    for (const auto a : edges) for (const auto b : edges) {
        for (unsigned op = 0; op < 4; ++op) Check(a, b, op);
    }
    for (const auto a : edges) {
        for (unsigned offset = 0; offset <= 32; ++offset) {
            for (unsigned count = 0; count <= 32 - offset; ++count) {
                Check(a, 0, 4, offset, count);
                Check(a, 0, 5, offset, count);
            }
        }
    }
    std::mt19937 random(0x4e4952);
    for (unsigned i = 0; i < 4096; ++i) {
        for (unsigned op = 0; op < 4; ++op) Check(random(), random(), op);
    }
    glsl_type_singleton_decref();
    std::printf("NIR integer flags: %u cases, real Mesa folding, zero/sign/carry/overflow PASS\n", cases);
}
