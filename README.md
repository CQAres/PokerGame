# PokerGame

多进程德州扑克项目的基础工程。当前已完成 Windows/Linux 可构建的 CMake 骨架、`poker_common` 基础层，以及 `poker_core` 的牌、牌堆、可复现随机数、5–7 张牌型评估器和完整下注状态机。底池结算、网络进程和 Agent 服务将在后续里程碑加入。

完整设计见 [德州扑克多进程服务器与LLM_Poker_Agent_开发方案_v2.md](德州扑克多进程服务器与LLM_Poker_Agent_开发方案_v2.md)。

## 当前范围

- C++17、CMake 3.22+、Ninja。
- Windows 使用 MSYS2/MinGW-w64；Linux 使用发行版工具链。
- `poker_common`：
  - `result.h`：`Status` / `Result<T>` / `Error`。
  - `json.h/.cpp`：JSON 解析、序列化、文件读写。
  - `config.h/.cpp`：点路径读取配置，支持对象和数组下标。
  - `logging.h/.cpp`：spdlog 控制台/文件日志初始化。
  - `time.h/.cpp`：Unix 毫秒和 ISO-8601 UTC 时间。
  - `ids.h/.cpp`：进程内单调 ID 和带前缀的字符串 ID。
- `poker_core`：
  - `card.h/.cpp`：`Card`、`Suit`、解析/格式化、0–51 索引。
  - `deck.h/.cpp`：52 张牌堆、Fisher-Yates 洗牌、可复现发牌。
  - `rng.h/.cpp`：`mt19937_64` + 拒绝采样，跨平台确定性随机。
  - `hand_rank.h/.cpp`：十类牌型、tiebreak 和比较。
  - `hand_evaluator.h/.cpp`：5–7 张牌取最佳五张，校验重复牌/非法牌。
  - `action.h/.cpp`：行动类型、行动、候选行动和合法行动集合。
  - `player.h/.cpp`、`table_config.h/.cpp`：玩家状态和牌桌配置校验。
  - `game_state.h/.cpp`：街道、玩家下注状态、底池、按钮/盲注/行动座位。
  - `action_validator.h/.cpp`：跟注额、最小加注、候选生成、自定义金额校验。
  - `game_engine.h/.cpp`：按钮移动、盲注、发牌、下注轮、街道推进、全押 Runout、摊牌标记。
- 规则约定见 [docs/rules.md](docs/rules.md)。
- GoogleTest + CTest：`poker_common_tests`、`poker_core_tests`。

当前测试数：46（common 12 + core 34）。

## Windows 构建

前置条件：MSYS2 安装在 `C:\msys64`，并已安装：

```text
mingw-w64-x86_64-gcc
mingw-w64-x86_64-cmake
mingw-w64-x86_64-ninja
mingw-w64-x86_64-nlohmann-json
mingw-w64-x86_64-spdlog
mingw-w64-x86_64-gtest
git
```

```powershell
.\scripts\dev-shell.ps1
cmake --preset windows-msys2-debug
cmake --build --preset windows-msys2-debug
ctest --preset windows-msys2-debug --output-on-failure
```

运行构建产物时，`PATH` 必须包含 `C:\msys64\mingw64\bin`，否则可能缺少 MinGW 运行时 DLL。

## Linux 构建

```bash
sudo apt install build-essential cmake ninja-build \
  nlohmann-json3-dev libspdlog-dev libgtest-dev

cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
```

## 目录

```text
src/common/      共享基础层
src/poker_core/  牌、牌堆、RNG、牌型评估、下注状态机
tests/common/    poker_common 单元测试
tests/core/      poker_core 单元测试
docs/            规则与设计补充
scripts/         开发环境脚本
```

下一步：实现 `PotManager` / `Settlement`，包括边池分层、未跟注退回、赢家判定和奇数筹码分配。
