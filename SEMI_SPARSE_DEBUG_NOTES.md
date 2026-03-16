# Semi-Sparse Debug Notes (2026-03-16)

## Current Status
- Branch fix (collectNeededVars + BranchStmt): 151 failures -> 106 failures
- Dense mode: 317/342 pass (25 pre-existing failures)
- Semi-sparse mode: 236/342 pass (106 failures, 其中约40个是 dense 也失败的)
- 真正的 semi-sparse regression: ~66 个

## Failure Categories

| Category | Count | Description |
|----------|-------|-------------|
| svf_assert cannot be verified | 51 | 精度丢失，interval 变成 top |
| has not been checked | 8 | 代码不可达（switch 分支被剪掉） |
| inVarToValTable crash | 3 | ExtAPI print() 参数不在状态中 |
| bottom interval crash | 2 | 访问 bottom interval 的 lb/ub |
| out_of_range crash | 1 | unordered_map::at 找不到 key |
| LLVM intrinsic not in rules | 34 | 预计是 pre-existing failure |
| handleStubFunctions crash | 6 | 预计部分是 pre-existing |
| svf_assert_eq fail | 1 | 值不匹配 |

## 已确认的 Bug #1: BranchStmt 条件变量缺失 (已修复)

BranchStmt 所在的 node 经过 merge + clearVarMap 后，abstractTrace[node] 只有 ObjVar。
原来的 collectNeededVars 没有收集 BranchStmt 的条件变量。
后续 successor 的 mergeStatesFromPredecessors 做 isBranchFeasible 时，
tmpEs = abstractTrace[pred] 里没有 CmpStmt result -> bottom -> 分支永远 infeasible。

修复: 在 collectNeededVars 中加入 BranchStmt 处理，收集 condVar + CmpStmt operands。

## 当前困惑: 精度丢失的根因

### 现象
INTERVAL_test_2: `%b = load i32, ptr %b_ptr` 的值变成 `[-2147483647, 2147483648]` (top)，
但 dense 模式下应该是精确值。

### 分析过的路径

1. **load/store 应该没问题**: ObjVar 是 dense 的，loadValue 走 addr->obj 路径，
   storeValue 同理。buildSparseState 拉了 pointer 的 ValVar (有 address set)。

2. **ConstIntValVar 的问题**:
   - ConstIntValVar 的 `getICFGNode()` 返回的是**使用它的 node**，不是 globalNode
   - ConstIntValVar 的值**从来没有被显式写入 abstractTrace**
   - buildSparseState 去 abstractTrace[defSite] 找 ConstIntValVar，找不到
   - 但 dense 模式下 updateStateOnCmp 也是对 missing operand 设 top，所以 dense 也不依赖 const 在 abstractTrace 里

3. **还没想通的地方**:
   - dense 模式下，所有 ValVar 都通过 edge state 传播，所以在 predecessors 处理过的 ValVar 值自然传到了当前 node
   - semi-sparse 模式下，buildSparseState 从 def-site pull，handleSVFStatement 写入当前 node
   - 问题: 如果一个 ValVar 在 def-site 被写入了 abstractTrace[defSite]，后续 node 的 buildSparseState 可以读到它
   - 但如果 **def-site 的 abstractTrace 在 merge 时被 clearVarMap 了呢**？
     - 不会！clearVarMap 只在 merge 阶段清理，之后 buildSparseState + handleSVFStatement 写入的值不会被清

4. **真正可能的问题**:
   - handleSVFStatement 写入了 LHS ValVar（如 load 的 LHS, binary 的 res, cmp 的 res）
   - 这些值留在 abstractTrace[defSite] 中
   - buildSparseState 在 downstream node 成功读取它们
   - 但是...有没有什么 statement 的处理依赖于 **不是** buildSparseState 拉的 ValVar？
   - 比如 `isBranchFeasible` 修改了 `as` (传入的 tmpEs)，做了 branch refinement
   - **branch refinement** 在 dense 模式下会收窄 op0/op1 的 interval，
     然后这个收窄后的状态被传递给 successor
   - 在 semi-sparse 模式下，clearVarMap 把这些收窄后的 ValVar 扔掉了！
     successor 的 buildSparseState 重新从 def-site 拉，拿到的是未收窄的原始值

### 假设: Branch Refinement 丢失

这可能是最大的精度丢失来源：

Dense: `if (x < 10)` 的 true branch 里，x 的 interval 被收窄到 `[-inf, 9]`
       这个收窄后的值通过 edge state 传播到 successor

Semi-sparse: clearVarMap 扔掉了收窄后的 x，buildSparseState 从 x 的 def-site 重新拉，
             拿到的是未收窄的原始 interval

## 可能的修复方向

如果 branch refinement 丢失是主因，有几个选择：

1. **不清理被 refinement 修改过的 ValVar**: 但这需要知道哪些被修改了
2. **在 merge 阶段保留 branch refined state**: isBranchFeasible 返回 refined state，
   把 refined ValVars 也加入 workList 的 state 中（不 clearVarMap 这些？）
3. **在 successor 的 buildSparseState 中也做 branch refinement**: 重新从 def-site 拉值，
   然后对 conditional edge 的条件重新 refine

方向 3 可能最干净，但需要在 buildSparseState 知道当前 node 是通过哪条 conditional edge 到达的。

方向 1/2 更直接: 在 mergeStatesFromPredecessors 里，isBranchFeasible 修改的 tmpEs 中
被收窄的 ValVar 不要 clearVarMap，直接保留到 merged state 中。
但这会增加 state size，而且多条 incoming edge 的 ValVar 需要 join。

## 文件位置
- 源码: `/var/tmp/vibe-kanban/worktrees/5d0b-sparse/SVF/svf/lib/AE/Svfexe/AbstractInterpretation.cpp`
- 头文件: `/var/tmp/vibe-kanban/worktrees/5d0b-sparse/SVF/svf/include/AE/Svfexe/AbstractInterpretation.h`
- Build dir: `/tmp/svf-semisparse-build/Release-build`
- 测试脚本: `/tmp/svf-semisparse-test.py`
- Dense 测试脚本: `/tmp/svf-dense-test.py`
