
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

### 日志系统 实现总结

**新增文件**：

| 文件 | 说明 |
|------|------|
| [include/core/Logger.h](include/core/Logger.h) | Logger 类声明 |
| [src/core/Logger.cpp](src/core/Logger.cpp) | Logger 实现 |
| [test/test_logger.cpp](test/test_logger.cpp) | 9 个测试用例 |

**核心数据结构**：

| 结构体/枚举 | 说明 |
|-------------|------|
| `enum class LogLevel { Debug, Info, Warn, Error }` | 日志级别（兼容 Windows ERROR 宏） |
| `struct LogEntry { level, category, message, formatted, timestamp }` | 单条日志记录 |

**Logger 类方法**：

| 方法 | 功能 |
|------|------|
| `Init(logDir, level, maxSizeMB)` | 初始化，创建日志目录/文件 |
| `Log(level, category, message)` | 线程安全写入（内存 + 文件） |
| `Debug/Info/Warn/Error(cat, msg)` | 便捷方法，自动填入级别 |
| `GetRecent(count)` | 获取最近 N 条日志（用于 GUI 渲染） |
| `GetByCategory(category, count)` | 按分类筛选日志 |
| `GetByLevel(minLevel, count)` | 按最小级别筛选 |
| `SetLevel(level)` / `GetLevel()` | 运行时调整日志级别 |
| `Flush()` | 强制刷盘 |

**App 集成变动**：3 个 `vector<string>` 日志向量 + `logMutex_` 替换为单个 `Logger` 实例，`RenderLogPanel` 新增分类筛选 Combo。

**日志文件格式**：`[YYYY-MM-DD HH:MM:SS] [LEVEL] [Category] message`  
**文件滚动**：按天 (`RemoteMonitor_YYYYMMDD.log`) + 超过 10MB 自动切分序号后缀

**新增测试**：

| 测试 | 用例数 | 覆盖内容 |
|------|--------|----------|
| [test/test_logger.cpp](test/test_logger.cpp) | 9 | 初始化、读写、级别过滤、分类筛选、环形缓冲、多线程安全、格式化、SetLevel、Flush |

---

### 监听指令 Flag 设计 实现总结

**新增数据结构**（[include/core/Config.h](include/core/Config.h) + [src/gui/App.h](src/gui/App.h)）：

| 结构体/枚举 | 位置 | 说明 |
|-------------|------|------|
| `enum class ListenMode { AI, User }` | App.h | 两个模式：看AI干活 / 等用户指令 |
| `enum class UserModeTrigger { Idle, Manual }` | App.h | User 模式进入原因：idle自动 / 手动切换 |
| `ListenModeConfig` | Config.h | 默认模式 + autoSwitchOnIdle + idleKeepAliveSec |

**核心方法**（[src/gui/App.cpp](src/gui/App.cpp)）：

| 方法 | 功能 |
|------|------|
| `SetListenMode(mode, trigger)` | 统一模式切换：AI→StopComm+StartMonitor / User→StopMonitor+StartComm |
| `OnCommandsExecuted(hasModeCmd)` | 命令执行后决策：Idle触发→自动回AI / Manual→留在User且重启监听 |

**五条规则**：

| 规则 | 行为 |
|------|------|
| AI idle 超时 | SetListenMode(User, Idle) |
| idle触发User → 指令执行完成（无mode指令） | 自动切回 AI |
| 指令含 CMD:mode:user | 留在 User，trigger=Manual |
| Manual触发User → 指令执行完成（无mode指令） | 继续留在 User，重启监听 |
| 指令含 CMD:mode:ai | 切回 AI |

**CMD 扩展**：新增 `CMD:mode:ai` 和 `CMD:mode:user` 两种模式控制指令，可与其他 CMD 指令混合使用。

**GUI 面板**：MonitorPanel 顶部新增模式指示器（绿色AI / 橙色User）+ trigger 标签 + 切换按钮。

**修改文件清单**：

| 文件 | 修改内容 |
|------|----------|
| [src/gui/App.h](src/gui/App.h) | 新增 ListenMode/UserModeTrigger 枚举 + AppState 字段 + SetListenMode/OnCommandsExecuted 方法 |
| [src/gui/App.cpp](src/gui/App.cpp) | 实现 SetListenMode/OnCommandsExecuted；MonitorThread idle 分支调用；ExecuteCommand 处理 mode 指令；ExecuteCommands 末尾调用 OnCommandsExecuted；RenderMonitorPanel/RenderCommPanel 模式 UI |
| [include/core/Config.h](include/core/Config.h) | 新增 ListenModeConfig 结构体 |
| [config/config.json](config/config.json) | 新增 listenMode 配置段 |

---

### 异常守护进程 (CrashGuard) 实现总结

**新增文件**：

| 文件 | 说明 |
|------|------|
| [include/core/CrashGuard.h](include/core/CrashGuard.h) | CrashGuard 类声明（纯静态） |
| [src/core/CrashGuard.cpp](src/core/CrashGuard.cpp) | CrashGuard 实现 |

**捕获的异常类型**：

| 类型 | Windows API | 场景 |
|------|-------------|------|
| SEH 结构化异常 | `SetUnhandledExceptionFilter` | 访问违例(0xC0000005)、栈溢出、除零等 |
| CRT abort() | `signal(SIGABRT)` | `abort()` 调用 → "Debug Error!" 对话框 |
| 纯虚函数调用 | `_set_purecall_handler` | 抽象基类析构时调用纯虚函数 |
| CRT 无效参数 | `_set_invalid_parameter_handler` | `printf(nullptr)` 等 CRT 参数校验失败 |
| abort 对话框屏蔽 | `_set_abort_behavior` | 抑制 "abort() has been called" 系统对话框 |

**CrashGuard 类方法**（全部静态）：

| 方法 | 功能 |
|------|------|
| `Install(logProvider, dumpDir)` | 安装所有异常处理器，创建 crash dump 目录 |
| `SetLogProvider(logProvider)` | 注册日志回调（Logger 创建后调用），crash 时写入最近 100 条日志 |
| `GetDumpDir()` | 获取 crash dump 目录路径 |
| `TestCrash()` | 生成测试报告（不触发实际崩溃） |

**崩溃现场保存内容**：

| 产物 | 文件 | 内容 |
|------|------|------|
| Minidump | `logs/crash_YYYYMMDD_HHMMSS.dmp` | 进程内存快照（Visual Studio/WinDbg 可分析） |
| 文本报告 | `logs/crash_YYYYMMDD_HHMMSS.txt` | 时间戳、异常类型、异常地址、故障线程ID、寄存器上下文（x86/x64）、最近 100 条应用日志 |

**集成点**（[src/gui/main_gui.cpp](src/gui/main_gui.cpp)）：

1. `WinMain` 第 1 行：`CrashGuard::Install(nullptr, "logs")` — 最早安装，捕获全过程异常
2. `App` 创建后：`CrashGuard::SetLogProvider(lambda)` — 注册 Logger 回调

**修改文件清单**：

| 文件 | 修改内容 |
|------|----------|
| [include/core/CrashGuard.h](include/core/CrashGuard.h) | 新增 CrashGuard 类 |
| [src/core/CrashGuard.cpp](src/core/CrashGuard.cpp) | 新增 CrashGuard 实现 |
| [src/gui/main_gui.cpp](src/gui/main_gui.cpp) | 添加 CrashGuard 安装 + Logger 回调注册 |
| [src/gui/App.h](src/gui/App.h) | 新增 `GetLogger()` 公开方法 |
| [CMakeLists.txt](CMakeLists.txt) | gui_app 链接 CrashGuard.cpp + dbghelp.lib |

---

### 线程崩溃 Bug 修复 — `std::thread` 赋值 joinable 线程导致 `std::abort()`

**Bug 描述**：程序在 MonitorThread 检测到 "NO CHANGE 10s" 后反复崩溃，CrashGuard 报告 `abort() called (CRT abort signal)`。两次崩溃（[crash_20260616_235925](logs/crash_20260616_235925.txt)、[crash_20260617_105400](logs/crash_20260617_105400.txt)）日志模式完全一致。

**根因**：C++ 标准规定，对 joinable 的 `std::thread` 对象通过 `operator=` 赋新值时，会调用 `std::terminate()` → `std::abort()`。

竞态触发路径：

```
MonitorThread (正在运行)
  ↓
SendToComm("AI stopped...") → SetListenMode(User) → StartComm()
  ↓  CommThread 启动，MonitorThread 仍在运行
  ↓
CheckAndExecuteStrategies() / Sleep()
  ↓
CommThread 检测到聊天变化 → ExecuteCommands()
  ↓ OnCommandsExecuted(Idle触发) → SetListenMode(AI)
  ↓
StartMonitor()
  ↓  monitorThread_ 仍是 joinable (MonitorThread 尚未退出!)
  ↓  monitorThread_ = std::thread(...)  → 💥 std::terminate() → abort()
```

**修复**（[src/gui/App.cpp](src/gui/App.cpp) 最小改动，仅 `StartMonitor()` + `StartComm()`）：

| 函数 | 修复前 | 修复后 |
|------|--------|--------|
| `StartMonitor()` | 直接 `monitorThread_ = std::thread(...)` | 先检查 `joinable()`：同线程则 `detach()`，异线程则 `join()` |
| `StartComm()` | 仅 `joinable()` → `join()`（同线程调用会死锁） | 增加同线程判断：同线程 `detach()`，异线程 `join()` |

修复后两函数的改动逻辑一致：

```cpp
if (thread_.joinable()) {
    if (thread_.get_id() == std::this_thread::get_id()) {
        thread_.detach();   // 不能 join 自己（会死锁）
    } else {
        stopFlag_ = true;    // 通知旧线程退出
        thread_.join();      // 等待旧线程退出
    }
}
// 然后安全地创建新线程
thread_ = std::thread(...);
```

**修改文件清单**：

| 文件 | 修改内容 |
|------|----------|
| [src/gui/App.cpp](src/gui/App.cpp) | `StartMonitor()` 新增 joinable 检查（6行）；`StartComm()` 新增同线程检查（4行） |

---
