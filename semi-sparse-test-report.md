# Semi-Sparse Abstract Execution — Test Report

**Branch:** `vk/5d0b-sparse`
**Date:** 2026-03-16
**Commit:** `c00db09d`

## Summary

| Mode | Pass | Fail | Total |
|------|------|------|-------|
| Dense (baseline) | 317 | 25 | 342 |
| Semi-sparse | 317 | 25 | 342 |

**Semi-sparse regressions: 0** — identical failure set as dense mode.

---

## Failing Test Cases (25 total, all pre-existing in dense mode)

### Category 1: Integer Overflow Tests (18 failures)

These tests involve the CWE121 Stack-Based Buffer Overflow test suite. They fail because the abstract execution engine does not model integer overflow wrapping precisely, causing the overflow-related buffer access checks to produce incorrect results.

| # | Test Name | Root Cause |
|---|-----------|------------|
| 1 | `CWE121_Stack_Based_Buffer_Overflow__CWE129_fgets_01` | Integer overflow imprecision in `fgets` return modeling |
| 2 | `CWE121_Stack_Based_Buffer_Overflow__CWE129_listen_socket_01` | Integer overflow imprecision in socket value modeling |
| 3 | `CWE121_Stack_Based_Buffer_Overflow__CWE131_loop_01` | Overflow in loop-computed buffer size |
| 4 | `CWE121_Stack_Based_Buffer_Overflow__CWE131_memcpy_01` | Overflow in memcpy size computation |
| 5 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_char_alloca_loop_01` | Overflow in alloca + loop interaction |
| 6 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_char_alloca_memcpy_01` | Overflow in alloca + memcpy size |
| 7 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_char_declare_loop_01` | Overflow in declared buffer + loop |
| 8 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_char_declare_memcpy_01` | Overflow in declared buffer + memcpy |
| 9 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_int64_t_alloca_loop_01` | Overflow in int64 alloca + loop |
| 10 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_int64_t_alloca_memcpy_01` | Overflow in int64 alloca + memcpy |
| 11 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_int64_t_declare_loop_01` | Overflow in int64 declare + loop |
| 12 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_int64_t_declare_memcpy_01` | Overflow in int64 declare + memcpy |
| 13 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_int_alloca_loop_01` | Overflow in int alloca + loop |
| 14 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_int_alloca_memcpy_01` | Overflow in int alloca + memcpy |
| 15 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_int_declare_loop_01` | Overflow in int declare + loop |
| 16 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_int_declare_memcpy_01` | Overflow in int declare + memcpy |
| 17 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_struct_alloca_loop_01` | Overflow in struct alloca + loop |
| 18 | `CWE121_Stack_Based_Buffer_Overflow__CWE805_struct_alloca_memcpy_01` | Overflow in struct alloca + memcpy |

**Analysis:** These are all buffer overflow detection tests from the Juliet test suite. The AE engine's interval domain does not precisely model integer overflow wrapping semantics (e.g., when a 32-bit int overflows from `INT_MAX+1` to `INT_MIN`). This causes buffer size/index computations to be imprecise, leading to missed or false buffer overflow reports. This is a fundamental limitation of the interval abstract domain, not a semi-sparse issue.

### Category 2: Recursion Tests with Widen-Narrow (7 failures)

These tests involve recursive functions where the widen-narrow fixpoint iteration does not converge to precise enough results. They fail identically in both dense and semi-sparse modes.

| # | Test Name | Root Cause |
|---|-----------|------------|
| 19 | `recursive_addition_0` | Addition recursion: widening over-approximates sum |
| 20 | `recursive_addition_1` | Addition recursion variant |
| 21 | `recursive_addition_2` | Addition recursion variant |
| 22 | `recursive_addition_3` | Addition recursion variant |
| 23 | `recursive_addition_4` | Addition recursion variant |
| 24 | `recursive_sum_0` | Sum recursion: widening loses precision |
| 25 | `recursive_sum_1` | Sum recursion variant |

**Analysis:** These recursive function tests involve accumulating sums (e.g., `f(n) = n + f(n-1)`). The widening operator necessarily over-approximates the accumulated value to ensure termination, and narrowing cannot fully recover precision for these patterns. For example, `sum(5)` should yield `15`, but widening produces `[0, +inf]` and narrowing can only narrow to `[0, +inf]` since each recursive call adds a non-negative value. This is an inherent limitation of the widen-narrow approach for non-monotone recursive computations.

---

## Semi-Sparse Design Summary

### Architecture
- **ValVar** (top-level SSA variables): stored in flow-insensitive `globalState`, pulled on-demand via `buildSparseState()`, flushed back via `flushToGlobalState()`
- **ObjVar** (address-taken/heap objects): dense propagation via `abstractTrace` (unchanged from dense mode)

### Key Design Decisions
1. **Branch refinement preservation**: After `isBranchFeasible` refines ValVars via `meet_with`, do selective clear — keep only ValVars that differ from `globalState`. `buildSparseState` skips ValVars already present (from branch refinement).
2. **CallCFGEdge formal params**: Keep only formal params (from `CallPE`) on `CallCFGEdge` so they are correctly joined across call sites. Other ValVars cleared.
3. **ExtAPI call nodes**: Pull ALL ValVars from `globalState` (not just collected operands) because ExtAPI handlers access arbitrary ValVars via indirect IR graph walks (e.g., `set_value` resolves pointer chains).
4. **Widen/narrow flush**: After `widening()` or `narrowing()` modifies `abstractTrace[cycle_head]`, flush to `globalState` so subsequent cycle body nodes see updated values.
5. **`flushToGlobalState` uses overwrite** (not `join_with`): `join_with` breaks narrowing in recursive functions (`join(widened, narrowed) = widened`). Cross-call-site join happens naturally via `CallCFGEdge` formal param merge.
