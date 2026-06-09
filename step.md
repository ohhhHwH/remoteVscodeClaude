
### 自定义处理策略 实现总结

**新增数据结构**（[include/core/Config.h](include/core/Config.h) + [src/gui/App.h](src/gui/App.h)）：

| 结构体 | 位置 | 说明 |
|--------|------|------|
| `ActionStepConfig` | Config.h | JSON 配置中的单步动作 |
| `StrategyConfig` | Config.h | JSON 配置中的策略定义 |
| `StrategyAction` | App.h | 运行时动作步骤（type/params/delayMs） |
| `Strategy` | App.h | 运行时策略（含 lastExecutedAtSec 防重复） |

**核心方法**（[src/gui/App.cpp](src/gui/App.cpp)）：

| 方法 | 功能 |
|------|------|
| `LoadStrategiesFromFile()` | 从 config.json 加载策略到 strategies_ |
| `ExecuteActionStep(step)` | 统一动作执行引擎，支持 9 种类型（click/dclick/rclick/move/type/key/hotkey/wait/scroll） |
| `ExecuteStrategy(s)` | 遍历策略动作列表顺序执行，每步间 delayMs 延迟 |
| `CheckAndExecuteStrategies(elapsedSec)` | idle 时遍历策略，检查 idleTimeoutSec 触发条件，防止同周期重复触发 |

**MonitorThread 集成**：在 idle 告警分支末尾调用 `CheckAndExecuteStrategies(elapsedSec)`。

---

### 自动点击 Yes 实现总结

**新增配置**（[include/core/Config.h](include/core/Config.h) `YesDetectConfig`）：

| 字段 | 默认值 | 说明 |
|------|--------|------|
| `enabled` | false | 开关 |
| `matchThreshold` | 0.7 | 模板匹配置信度阈值 |
| `templateImage` | "" | 模板图片路径（如 pic/yes.png） |
| `clickOffsetX/Y` | 0 | 点击偏移量 |

**核心方法**（[src/gui/App.cpp](src/gui/App.cpp)）：

| 方法 | 功能 |
|------|------|
| `LoadYesDetectFromFile()` | 加载 yesDetect 配置 + 缓存模板图片到 yesTemplate_ |
| `DetectYesButton(screenshot)` | 多尺度（0.7x~1.3x）`cv::matchTemplate(TM_CCOEFF_NORMED)` → 返回匹配中心坐标或 (-1,-1) |
| `ClickYesIfFound()` | 截图 monitorRegion → DetectYesButton → 屏幕坐标转换 → `mouse_event` 点击 |

**MonitorThread 集成**：每次截图循环结束时，若 `autoClickYes && yesTemplate_` 非空则调用 `ClickYesIfFound()`。

**GUI 控制**：`RenderMonitorPanel()` 新增 "Auto-click Yes" 复选框 + 策略列表折叠面板。

---

### 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| [include/core/Config.h](include/core/Config.h) | 新增 ActionStepConfig / StrategyConfig / YesDetectConfig |
| [src/core/Config.cpp](src/core/Config.cpp) | JSON 解析/序列化 strategies 和 yesDetect 段 |
| [src/gui/App.h](src/gui/App.h) | 新增 StrategyAction / Strategy 结构体 + 6 个方法 |
| [src/gui/App.cpp](src/gui/App.cpp) | ~150 行新代码：策略执行 + Yes 检测 + MonitorThread 集成 |
| [config/config.json](config/config.json) | 新增 strategies 和 yesDetect 示例配置 |
| [CMakeLists.txt](CMakeLists.txt) | gui_app 链接 Config.cpp；新增 2 个测试目标 |

### 新增测试

| 测试 | 用例数 | 覆盖内容 |
|------|--------|----------|
| [test_strategy_parse.cpp](test/test_strategy_parse.cpp) | 7 | 策略 JSON 解析、空策略、缺失段、多策略、yesDetect、默认值、全动作类型 |
| [test_yes_detect.cpp](test/test_yes_detect.cpp) | 8 | 精确/放大/缩小匹配、无按钮、高阈值、多尺度一致性、空输入、pic/yes.png 加载 |

---
