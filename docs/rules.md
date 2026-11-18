# 牌局规则（P0 实现约定）

本文记录 `poker_core` 当前采用的确定性规则，重点是短全押、补足下注和加注权。实现与测试以本文和 `GameEngine` 为准。

## 座位、按钮与盲注

- P0 固定 6 座，实际活跃玩家可以少于 6 人。
- 第一手按钮从 `first_button_seat` 开始，顺时针找第一个活跃座位。
- 后续每手按钮移到上一个按钮顺时针的下一个活跃座位。
- 活跃：已入座、未坐出、筹码大于 0。
- 3 人及以上：小盲是按钮顺时针下一个活跃座位，大盲是小盲顺时针下一个活跃座位。
- Heads-up：按钮同时是小盲，另一个玩家是大盲。
- 盲注投入 `min(stack, blind)`；投入后筹码为 0 时标记全押。
- 大盲不足额时，`current_bet` 仍为完整大盲。其他玩家按完整大盲跟注，多出的未跟注部分在结算时退回。
- 每手独立手牌种子，洗牌与发牌顺序可复现。
- 发牌从按钮顺时针下一个活跃座位开始，每人一张，发两轮；Heads-up 因而从大盲开始。

## 行动顺序

- Preflop：大盲顺时针下一个可行动玩家先行动；Heads-up 时按钮/小盲先行动。
- Flop/Turn/River：按钮顺时针下一个可行动玩家先行动；Heads-up 时大盲先行动。
- 跳过已弃牌、已全押、坐出和无筹码玩家。
- 大盲在翻牌前即使已投入完整大盲，只要仍可行动，就保留“选项”，可以 Check 或 Raise。

## 下注量

```text
to_call       = max(0, current_bet - committed_round)
max_target    = committed_round + stack
min_bet_to    = big_blind
```

最小加注目标：

```text
current_bet == 0                                  -> big_blind
current_bet < big_blind 且 last_full_raise_to == 0 -> big_blind
其他                                               -> current_bet + last_full_raise_size
```

- 本轮第一个下注至少为大盲。
- 加注目标必须大于 `current_bet`，且不超过 `max_target`。
- 非全押下注/加注必须达到最小目标；全押可以不足最小加注。

## 短全押与补足

- 玩家 `stack <= to_call`：只能弃牌或跟注全押，不能加注。
- 玩家有加注权但 `max_target < min_raise_to`：可以全押，这是“短加注”，不重开加注权。
- 玩家本轮第一个下注若是全押且小于大盲，`current_bet` 取该全押额。
- 后续玩家可以：
  - 跟注该全押额；
  - 补足到大盲或加注到更高金额。目标达到大盲即视为一次完整下注，`last_full_raise_size = big_blind`，`last_full_raise_to = 新 current_bet`；
  - 之后的加注增量按大盲计算。
- 完整加注后，除加注者外，其他可行动玩家的 `acted_since_full_raise` 清零，重新获得加注权。
- 单次短全押不重开加注权；已经行动过的玩家只能跟注或弃牌。
- 尚未行动过的玩家面对短全押仍可加注。
- 多次短全押的累积增量达到 `last_full_raise_to + last_full_raise_size` 时，视为一次完整加注，重新开放加注权；新 `last_full_raise_size = 新 current_bet - last_full_raise_to`。

## 无人可应

若除当前玩家外没有其他可行动玩家：

- 不生成下注、加注或加注型全押候选。
- 只生成弃牌、跟注（若有跟注额）或过牌（若无需跟注）。
- 若当前玩家已匹配 `current_bet`，本轮直接结束，不要求无意义行动。

## 街道推进与全押 Runout

- 本轮结束条件：不存在仍需行动的可行动玩家；只剩一个可行动玩家时，只有其未匹配 `current_bet` 才继续要求行动。
- 未弃牌玩家只剩 1 人：立即 `UncontestedWin`，不发剩余公共牌、不摊牌、不公开底牌。
- 可行动玩家不超过 1 人：进入 Runout，发完剩余公共牌后摊牌。
- 正常进入下一条街时：重置 `current_bet = 0`、`committed_round = 0`、`last_full_raise_size = big_blind`、`last_full_raise_to = 0`，并重置 `needs_action` / `acted_since_full_raise`。
- Runout 开始时即公开所有未弃牌玩家的底牌；摊牌时公开所有未弃牌玩家底牌。
- 发牌顺序：Flop 烧 1 发 3，Turn 烧 1 发 1，River 烧 1 发 1。

## 结算规则（下一里程碑实现）

底池结算尚未写回 `stack`，但已确定以下规则：

- 按 `committed_hand` 分层构建主池和边池；弃牌玩家的投入仍留在池中，但不参与争夺。
- 最高层若只有一名贡献者，超出次高层的部分作为未跟注注额退回。
- 每层由该层贡献者中未弃牌的玩家比较牌型，最大者获胜。
- 赢家平分该层；不能整除的奇数筹码从按钮顺时针方向开始，每个赢家依次多分 1 个筹码。
- 无抽水；结算前后总筹码守恒。

## 当前未覆盖

- 底池分层、边池、未跟注退回、奇数筹码分配和奖金写回属于下一里程碑。
- 摊牌目前只标记状态并公开底牌，尚未计算赢家。
- `gm rebuy`、断线超时、机器人调度和多手会话由 `poker_svr` / `TableService` 后续实现。
