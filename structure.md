# RemoteMonitor — 项目文件架构

> AI 审阅速览文档：多进程 + 插件架构的 Windows 远程 AI 终端监控与自动化工具。
> C++17，CMake 构建，OpenCV 图像处理，ImGui + DirectX 11 GUI，Win32 API 模拟操作。

---

## 1. 项目总览

```
remoteVscodeClaude/
├── CMakeLists.txt              # 构建脚本
├── README.md                   # 项目说明
├── TODO                        # 待办/Bug 跟踪
├── LICENSE                     # Mulan PSL v2 许可证
├── structure.md                # ← 本文件
├── start_gui.cmd               # Windows 一键启动脚本
├── .gitignore
├── config/
│   └── config.json             # 运行时配置
├── include/                    # 公共头文件（接口定义）
│   ├── action/                 #   操作插件接口
│   ├── comm/                   #   通信/OCR 接口
│   ├── core/                   #   核心框架接口
│   └── monitor/                #   屏幕监控接口
├── src/                        # 源代码实现
│   ├── action/                 #   操作插件实现
│   ├── comm/                   #   通信/OCR 插件实现
│   ├── core/                   #   主控进程 + 框架实现
│   ├── gui/                    #   GUI 应用（ImGui）
│   └── monitor/                #   屏幕监控插件实现
├── test/                       # 单元测试
├── third_party/imgui/          # Dear ImGui 库
├── pic/                        # 演示图片/GIF
├── tem/                        # 临时参考材料
├── build/                      # CMake 构建输出（gitignore）
└── build2/                     # 备用构建目录（gitignore）
```

**构建产物**（CMake 定义的目标）：

| 目标 | 类型 | 输出位置 | 说明 |
|------|------|----------|------|
| `controller` | EXE | `build/bin/Debug/` | 主控进程（命令行） |
| `gui_app` | WIN32 EXE | `build/bin/Debug/` | GUI 应用 |
| `monitor_plugin` | DLL | `build/plugins/Debug/` | 屏幕监控插件 |
| `comm_plugin` | DLL | `build/plugins/Debug/` | 通信/OCR 插件 |
| `action_plugin` | DLL | `build/plugins/Debug/` | 操作执行插件 |
| `test_*` | EXE | `build/bin/Debug/` | 各单元测试 |

---

## 2. 根目录文件

### [CMakeLists.txt](CMakeLists.txt)

项目构建入口。**关键信息**：
- C++17 标准
- 依赖：OpenCV 4.x（可选，未找到则仅编译测试）、DirectX 11
- 编译产物分两路：`controller` 命令行主控（加载 DLL 插件），`gui_app` GUI 应用（将插件逻辑内联，避免 DLL 依赖）
- 测试框架：自定义轻量框架，8 个测试可执行文件

### [config/config.json](config/config.json)

JSON 配置文件。**结构定义** (`include/core/Config.h:12-17`)：
```json
{
    "pollIntervalMs": 1000,       // 轮询间隔(ms)
    "pluginDir": "./plugins",      // DLL 插件目录
    "commTarget": "",              // QQ/微信窗口标题
    "monitorRegions": [{
        "id": "ai_terminal_1",     // 区域唯一标识
        "x": 100, "y": 100,        // 左上角坐标
        "width": 800, "height": 600,
        "threshold": 0.05          // 变化检测阈值
    }]
}
```

### [start_gui.cmd](start_gui.cmd)

Windows 批处理启动脚本。将 OpenCV DLL 目录临时加入 `PATH`，运行 `gui_app.exe`。

### [TODO](TODO)

待办事项清单：  
- 已完成：图片回传、坐标标注点  
- 未完成：自定义处理策略（自动点 yes）、VLM 视觉识别、日志系统  
- Bug：`CMD:type:text` 无法输入中文（已通过剪贴板 Unicode 修复标记 `[V]`）

### [imgui.ini](imgui.ini)

ImGui 窗口布局持久化文件（运行时自动生成）。

---

## 3. `include/` — 公共头文件（接口层）

### 3.1 `include/core/` — 核心框架接口

#### [IPlugin.h](include/core/IPlugin.h)

**插件生命周期接口**。所有插件 DLL 必须实现。

```cpp
struct PluginConfig { string configPath; };

class IPlugin {
    virtual bool Init(const PluginConfig& config) = 0;  // 输入：配置 → 输出：bool
    virtual void Shutdown() = 0;                         // 无输入输出
    virtual string GetName() const = 0;                  // → 插件名
    virtual string GetVersion() const = 0;               // → 版本号
};
```

**DLL 导出约定**：每个插件 DLL 导出两个 C 函数：
- `CreatePlugin()` → `IPlugin*`
- `DestroyPlugin(IPlugin*)`

宏 `EXPORT_PLUGIN(ClassName)` 自动生成上述导出函数。

#### [EventBus.h](include/core/EventBus.h)

**事件总线**（发布/订阅模式），模块间解耦。

```cpp
enum class EventType { REGION_CHANGED, MESSAGE_RECEIVED, COMMAND_RECEIVED, SCREENSHOT_READY };
struct Event { EventType type; string source; any data; };
using EventCallback = function<void(const Event&)>;

class EventBus {
    void Subscribe(EventType type, EventCallback callback);  // 订阅事件
    void Publish(const Event& event);                        // 发布事件（同步广播）
};
// 线程安全：内部有 mutex
```

#### [Config.h](include/core/Config.h)

**配置管理**，JSON 配置的加载/保存。

```cpp
struct RegionConfig { string id; int x,y,width,height; double threshold; };
struct AppConfig {
    vector<RegionConfig> monitorRegions;
    string commTarget;      // 通信窗口标题
    int pollIntervalMs;     // 监控轮询间隔
    string pluginDir;       // 插件 DLL 目录
};

class Config {
    bool Load(const string& path);   // 从 JSON 文件加载 → bool
    bool Save(const string& path);   // 保存到 JSON 文件 → bool
    const AppConfig& Get() const;    // 只读访问
    AppConfig& GetMutable();         // 可写访问
};
```

#### [IPC.h](include/core/IPC.h)

**命名管道进程间通信**（Windows Named Pipe）。

```cpp
class IPC {
    bool CreatePipeServer(const string& pipeName);   // 创建管道服务端 → bool
    bool ConnectToPipe(const string& pipeName);      // 客户端连接 → bool
    bool Send(const string& data);                   // 发送数据（最大4096字节）→ bool
    string Receive();                                // 接收数据 → string（最多4096字节）
    void Close();                                    // 关闭管道
};
```

#### [Json.h](include/core/Json.h)

**自实现 JSON 解析器**（零依赖，不依赖 nlohmann/json）。

```cpp
namespace json {
    struct Value { variant<nullptr_t, bool, double, string, Array, Object> data; };
    using Object = unordered_map<string, Value>;
    using Array = vector<Value>;

    class Parser {
        Value Parse(const string& input);       // 字符串 → Value
        Value ParseFile(const string& path);    // 文件 → Value（抛异常）
    };
    string Serialize(const Value& v);           // Value → JSON 字符串
}
```

#### [PluginManager.h](include/core/PluginManager.h)

**DLL 插件加载/卸载管理**。

```cpp
struct LoadedPlugin { HMODULE handle; IPlugin* instance; string path; };

class PluginManager {
    bool LoadPlugin(const string& dllPath);       // LoadLibrary + GetProcAddress
    void UnloadAll();                             // 销毁所有插件，FreeLibrary
    IPlugin* GetPlugin(const string& name);       // 按名称查找插件
    const vector<LoadedPlugin>& GetAll() const;   // 获取所有已加载插件
};
```

### 3.2 `include/action/`

#### [IActionExecutor.h](include/action/IActionExecutor.h)

**操作执行接口**（Win32 API 模拟鼠标/键盘）。

```cpp
class IActionExecutor {
    virtual void Click(int x, int y) = 0;              // 左键点击（屏幕绝对坐标）
    virtual void TypeText(const string& text) = 0;     // 输入文本（逐字符 keybd_event）
    virtual void KeyPress(WORD vkCode) = 0;            // 单键按下/释放
    virtual void MoveMouse(int x, int y) = 0;          // 移动光标
};
```

### 3.3 `include/comm/`

#### [ICommunicator.h](include/comm/ICommunicator.h)

**通信接口**（通过 QQ/微信窗口收发消息）。

```cpp
struct Message { string sender; string content; uint64_t timestamp; };

class ICommunicator {
    virtual bool SendImage(const string& imagePath) = 0;   // 发送图片到聊天窗口
    virtual bool SendText(const string& text) = 0;         // 发送文本到聊天窗口
    virtual vector<Message> ReadMessages() = 0;            // 读取新消息
};
```

#### [IOCR.h](include/comm/IOCR.h)

**OCR 识别接口**。

```cpp
class IOCR {
    virtual string Recognize(const cv::Mat& image) = 0;  // 输入：OpenCV 图像 → 输出：识别文本
};
```

### 3.4 `include/monitor/`

#### [IScreenCapture.h](include/monitor/IScreenCapture.h)

**屏幕截图接口**。

```cpp
struct Rect { int x, y, width, height; };

class IScreenCapture {
    virtual cv::Mat CaptureRegion(const Rect& rect) = 0;  // 截取屏幕区域 → BGR 图像
    virtual cv::Mat CaptureWindow(HWND hwnd) = 0;         // 截取窗口 → BGR 图像
};
```

#### [IRegionMonitor.h](include/monitor/IRegionMonitor.h)

**区域变化监控接口**（OpenCV 帧差法）。

```cpp
class IRegionMonitor {
    virtual void AddRegion(const string& id, const Rect&, double threshold) = 0;
    virtual void RemoveRegion(const string& id) = 0;
    virtual bool CheckChange(const string& id) = 0;     // 与上一帧对比 → bool
    virtual cv::Mat GetSnapshot(const string& id) = 0;  // 获取当前区域截图
};
```

---

## 4. `src/` — 源代码实现

### 4.1 `src/core/` — 主控进程

#### [main.cpp](src/core/main.cpp)

**controller.exe 入口**（命令行主控进程）。

**执行流程**：
1. 加载 `config/config.json`（或命令行参数指定的路径）
2. 创建 EventBus
3. 从 `pluginDir` 目录加载所有 `.dll` 插件
4. 依次调用每个插件的 `Init()`
5. 进入死循环（按 `pollIntervalMs` 间隔 sleep），等待 `Ctrl+C`
6. 收到退出信号 → 调用各插件 `Shutdown()` → `UnloadAll()`

**输入**：命令行 `argv[1]` = 配置文件路径（可选，默认 `config/config.json`）  
**输出**：控制台日志

#### [PluginManager.cpp](src/core/PluginManager.cpp)

`PluginManager` 实现。`LoadLibraryA` → `GetProcAddress("CreatePlugin")` → 调用工厂函数。卸载时 `GetProcAddress("DestroyPlugin")` → `FreeLibrary`。

#### [EventBus.cpp](src/core/EventBus.cpp)

`EventBus` 实现。`Subscribe` 追加回调到 `subscribers_` map；`Publish` 遍历对应 `EventType` 的所有回调并同步调用。

#### [Config.cpp](src/core/Config.cpp)

`Config` 实现。使用自实现的 `json::Parser` 解析配置文件。异常时返回默认值。`Save` 手动拼接 JSON 字符串。

#### [IPC.cpp](src/core/IPC.cpp)

`IPC` 实现。服务端用 `CreateNamedPipeA` + `ConnectNamedPipe`；客户端用 `CreateFileA` 打开管道。`Send`/`Receive` 使用 `WriteFile`/`ReadFile`，缓冲区大小 4096 字节。

### 4.2 `src/action/`

#### [Win32Action.cpp](src/action/Win32Action.cpp)

**action_plugin.dll 的实现**。同时继承 `IActionExecutor` 和 `IPlugin`。

- `Click(x,y)`: `SetCursorPos` + `mouse_event(LEFTDOWN/LEFTUP)`
- `TypeText(text)`: 逐字符 `VkKeyScanA` → `keybd_event`（不支持中文）
- `KeyPress(vkCode)`: `keybd_event` 按下+释放
- `MoveMouse(x,y)`: `SetCursorPos`
- 底部 `EXPORT_PLUGIN(Win32Action)` 宏生成 DLL 导出

### 4.3 `src/comm/`

#### [WeChatComm.cpp](src/comm/WeChatComm.cpp)

**comm_plugin.dll 的实现**。同时继承 `ICommunicator` 和 `IPlugin`。

- `SendImage(path)`: 查找目标窗口 → 置前台 → 将文件路径写入剪贴板 `CF_HDROP` → `Ctrl+V` → `Enter`
- `SendText(text)`: 查找目标窗口 → 置前台 → 文本写入剪贴板 `CF_TEXT` → `Ctrl+V` → `Enter`
- `ReadMessages()`: 截图聊天区域 → OCR 识别 → 逐行解析 → 返回**增量**新消息（通过 `lastMessageCount_` 去重）
- 依赖注入：`SetOCR()`, `SetCapture()`, `SetChatRegion()` 设置合作对象

#### [OCREngine.cpp](src/comm/OCREngine.cpp)

**OCR 引擎实现**。实现 `IOCR` 接口（非插件，通过依赖注入使用）。

- `Recognize(image)`: cv::Mat → 保存临时 PNG → PowerShell 调用 `Windows.Media.Ocr.OcrEngine` → 返回识别文本
- 实现方式：`_popen("powershell -Command ...")` 执行脚本

### 4.4 `src/monitor/`

#### [MonitorPlugin.cpp](src/monitor/MonitorPlugin.cpp)

**monitor_plugin.dll 的实现**。包含三个内部类：

1. **`ScreenCapture`** (implements `IScreenCapture`)
   - `CaptureRegion`: `GetDC(nullptr)` → `CreateCompatibleDC` → `BitBlt` → `GetDIBits` → cv::Mat (BGRA→BGR)
   - `CaptureWindow`: `GetWindowRect` → 委托给 `CaptureRegion`

2. **`RegionMonitor`** (implements `IRegionMonitor`)
   - `AddRegion`: 添加监控区域到 map
   - `CheckChange`: `CaptureRegion` → `absdiff` 与上一帧对比 → 灰度二值化(>30) → 计算变化像素比例 → 与阈值比较
   - `GetSnapshot`: 返回当前区域截图

3. **`MonitorPlugin`** (implements `IPlugin`)
   - 持有 `ScreenCapture` 和 `RegionMonitor` 实例
   - 对外暴露 `GetCapture()` 和 `GetMonitor()` 供依赖注入

### 4.5 `src/gui/` — GUI 应用

#### [main_gui.cpp](src/gui/main_gui.cpp)

**gui_app.exe 入口**（WinMain）。

- 创建 Win32 窗口（1280x720，"RemoteMonitor"）
- 初始化 DirectX 11 设备和交换链
- 初始化 ImGui（加载微软雅黑中文字体）
- 主循环：PeekMessage → ImGui 帧渲染 → Present
- `WndProc` 处理窗口事件（WM_SIZE 重建渲染目标，WM_DESTROY 退出）

#### [App.h](src/gui/App.h)

**App 类声明**。核心数据结构：

```cpp
struct AppState {
    Rect monitorRegion;       // 监控区域（屏幕坐标）
    Rect commInputRegion;     // 通信输入框区域（粘贴位置）
    Rect commChatRegion;      // 聊天内容区域（读取位置）
    POINT commClickPoint;     // 聊天消息点击位置（选中文本用）
    float threshold;          // 变化阈值，默认 0.05
    int pollIntervalMs;       // 轮询间隔，默认 1000ms
    int noChangeTimeoutSec;   // 无变化超时触发告警，默认 10s
    bool monitorRunning;      // 监控线程运行状态
    bool commRunning;         // 通信监听线程运行状态
};

struct Command { string action; string params; };
// action 支持: click/dclick/rclick/move/type/key/hotkey/wait/scroll/stop
```

**App 类方法清单**：

| 方法 | 可见性 | 功能 |
|------|--------|------|
| `App(device)` | public | 构造，保存 D3D11 设备指针 |
| `~App()` | public | 析构，停止线程，释放纹理 |
| `Render()` | public | 每帧渲染（三面板布局） |
| `SelectRegion(outRect, hwnd)` | public | 全屏半透明遮罩区域选择 |
| `RenderMonitorPanel()` | private | 左面板：选区域、调参数、启停监控 |
| `RenderCommPanel()` | private | 右面板：选输入框/聊天区域、启停监听 |
| `RenderLogPanel()` | private | 底面板：操作日志 |
| `StartMonitor()/StopMonitor()` | private | 启停监控线程 |
| `StartComm()/StopComm()` | private | 启停通信线程 |
| `MonitorThread()` | private | 监控线程体：截图→帧差→超时告警 |
| `CommThread()` | private | 通信线程体：截图→检测变化→读取回复→执行命令 |
| `SendToComm(text)` | private | 剪贴板粘贴文本到输入框→Enter 发送 |
| `SendImageToComm(image)` | private | 剪贴板粘贴图片(CF_DIB)→Enter 发送 |
| `ReadChatByClipboard()` | private | 三击选中→Ctrl+C→读剪贴板 |
| `ReadChatAtPosition(x,y)` | private | 在指定坐标三击→Ctrl+C→读 CF_UNICODETEXT |
| `ParseCommand(text)` | private | 解析单条 "CMD:action:params" |
| `ParseCommands(text)` | private | 解析分号分隔的多条命令 |
| `ExecuteCommand(cmd)` | private | 执行单条命令 |
| `ExecuteCommands(text)` | private | 解析并执行多条命令（每条间隔 1s） |
| `UpdateTexture(srv, img)` | private | cv::Mat → D3D11 Texture2D |
| `FlushPendingFrames()` | private | 线程安全地将后台帧同步到渲染线程 |

#### [App.cpp](src/gui/App.cpp)

**App 实现**（约 865 行，核心逻辑所在）。关键函数详解：

**`CaptureScreen(Rect)`** (匿名命名空间)：独立的屏幕截取实现，与 `ScreenCapture` 类逻辑相同，避免 GUI 程序链接插件 DLL。

**`SelectRegion(Rect&, HWND)`**：创建全屏透明 overlay 窗口（`WS_EX_TOPMOST | WS_EX_LAYERED`，80/255 透明度），用户拖拽选择区域，鼠标释放后通过指针返回 `Rect`。

**`MonitorThread()`**：
1. 循环截图 `monitorRegion`
2. 帧差法检测变化：`absdiff` → 灰度 → `countNonZero(gray > 30) / total` → 与 `threshold` 比较
3. 有变化：更新 `lastChangeTime`，重置告警标志
4. 无变化超时（>= `noChangeTimeoutSec`）：发送当前截图 + 标注网格图（每 100px 画线，每 25px 画点）+ 文本通知
5. 截图通过 `frameMutex_` 传递到主线程渲染

**`CommThread()`**：
1. 循环截图 `commChatRegion`
2. 检测到画面变化时，在 `commClickPoint` 三击选中文本 → `Ctrl+C` → 读剪贴板 `CF_UNICODETEXT`（支持中文）
3. 去重后调用 `ExecuteCommands(text)` 解析并执行
4. 命令来源格式：`CMD:click:100,200;type:继续;key:enter`

**命令协议**（`CMD:` 前缀，分号分隔）：  

| 命令 | 格式 | 说明 |
|------|------|------|
| `click` | `CMD:click:x,y` | 左键单击（坐标相对于 monitorRegion 左上角） |
| `dclick` | `CMD:dclick:x,y` | 双击 |
| `rclick` | `CMD:rclick:x,y` | 右键单击 |
| `move` | `CMD:move:x,y` | 移动光标 |
| `type` | `CMD:type:text` | 输入文本（通过剪贴板 CF_UNICODETEXT 粘贴，支持中文） |
| `key` | `CMD:key:name` | 按键（enter/tab/esc/space/backspace/delete/up/down/left/right/home/end/pgup/pgdn） |
| `hotkey` | `CMD:hotkey:ctrl+c` | 组合键（ctrl/alt/shift/win + key，`+` 连接） |
| `wait` | `CMD:wait:ms` | 等待毫秒（最大 10000ms） |
| `scroll` | `CMD:scroll:x,y,delta` | 滚轮（delta > 0 向上） |
| `stop` | `CMD:stop` | 停止监控 |

**数据流**：
```
手机端 → QQ/微信 → CommThread 截图OCR → ParseCommands → ExecuteCommand(s)
                                                         ↓
MonitorThread → 检测AI idle → SendImageToComm → QQ/微信 → 手机端
                                                         ↓
                                   ExecuteCommand后截图回传(反馈闭环)
```

---

## 5. `test/` — 测试

**测试框架**：[`TestFramework.h`](test/TestFramework.h) — 轻量自实现框架。

```cpp
TEST(name) { ... }                    // 定义测试用例（自动注册）
ASSERT_TRUE(expr) / ASSERT_FALSE(expr)
ASSERT_EQ(a, b)
RunAllTests() → int                   // 返回 0=全部通过，1=有失败
```

| 测试文件 | 测试目标 | 依赖 |
|----------|----------|------|
| `test_config.cpp` | `Config` 加载/保存 JSON | 无 |
| `test_json.cpp` | `json::Parser` 解析/序列化 | 无 |
| `test_eventbus.cpp` | `EventBus` 发布/订阅 | 无 |
| `test_ipc.cpp` | `IPC` 命名管道通信 | 无 |
| `test_action.cpp` | `IActionExecutor` 接口 | 无 |
| `test_parse_cmd.cpp` | `CMD:action:params` 命令解析 | 无 |
| `test_monitor.cpp` | `IScreenCapture`/`IRegionMonitor` | OpenCV |
| `test_comm.cpp` | `ICommunicator`/`IOCR` | OpenCV |

---

## 6. `third_party/` — 第三方库

### [Dear ImGui](third_party/imgui/) (v1.x)

即时模式 GUI 库。直接嵌入源码，非子模块。

| 文件 | 说明 |
|------|------|
| `imgui.cpp` + `imgui.h` | 核心库 |
| `imgui_draw.cpp` | 绘图 |
| `imgui_widgets.cpp` | 控件 |
| `imgui_tables.cpp` | 表格 |
| `imgui_internal.h` | 内部 API |
| `imstb_*.h` | 字体/文本/纹理辅助 |
| `backends/imgui_impl_win32.cpp/.h` | Win32 窗口后端 |
| `backends/imgui_impl_dx11.cpp/.h` | DirectX 11 渲染后端 |

---

## 7. 架构数据流图

```
┌─────────────────────────────────────────────────────────────────┐
│                        gui_app.exe (Win32 + ImGui + D3D11)       │
│                                                                   │
│  ┌──────────────┐   ┌──────────────┐   ┌────────────────────┐   │
│  │ MonitorThread │   │  CommThread  │   │   Main UI Thread   │   │
│  │               │   │              │   │                    │   │
│  │ BitBlt截图    │   │ BitBlt截图   │   │ ImGui 三面板布局    │   │
│  │ 帧差法检测    │   │ 变化检测     │   │ - 监控面板         │   │
│  │ 超时→告警     │   │ 三击复制     │   │ - 通信面板         │   │
│  │ 发送截图/文本 │   │ OCR/剪贴板   │   │ - 日志面板         │   │
│  │               │   │ 解析CMD命令   │   │ 区域选择 Overlay   │   │
│  └──────┬────────┘   └──────┬───────┘   └────────────────────┘   │
│         │                   │                                     │
│         └───────┬───────────┘                                     │
│                 │ 共享: AppState + mutex 保护的日志/帧缓冲          │
└─────────────────┼─────────────────────────────────────────────────┘
                  │
         Win32 API 自动化
         (mouse_event / keybd_event / SetCursorPos / Clipboard)
                  │
         ┌────────▼────────┐
         │  QQ / 微信 窗口   │
         │  (第三方通信APP)  │
         └─────────────────┘
                  │
         ┌────────▼────────┐
         │     手机端       │
         │  (接收截图+发送命令) │
         └─────────────────┘


┌─────────────────────────────────────────────┐
│  controller.exe (命令行，插件架构)             │
│                                              │
│  Config → PluginManager → EventBus           │
│              │                                │
│     ┌────────┼────────┬──────────┐           │
│     ▼        ▼        ▼          ▼           │
│  monitor  comm   action       (future)       │
│  _plugin  _plugin _plugin                    │
│  .dll     .dll    .dll                       │
│                                              │
│  进程间通信: IPC (Named Pipe)                  │
└─────────────────────────────────────────────┘
```

---

## 8. 关键约定

### 坐标系统
- **`Rect`**: 屏幕绝对坐标（x, y 为左上角）
- **CMD 命令中的 xy**: 相对于 `monitorRegion` 左上角的偏移量
- **`ExecuteCommand` 内部**: `SetCursorPos(monitorRegion.x + cmd_x, monitorRegion.y + cmd_y)`

### 线程模型
- GUI 主线程：ImGui 渲染 + 用户交互
- `MonitorThread`: 后台截图对比，通过 `frameMutex_` 传递帧数据
- `CommThread`: 后台监听回复，通过 `sendMutex_` 与发送逻辑协调
- `suppressNextRead_` 标志：发送消息后跳过一轮读取，避免读取到自己刚发的消息

### 剪贴板使用
- **发送图片**: `CF_DIB` 格式（BMP DIB，垂直翻转）
- **发送文本**: `CF_TEXT`（ANSI）或 `CF_UNICODETEXT`（UTF-16，支持中文）
- **读取文本**: `CF_UNICODETEXT` → `WideCharToMultiByte(CP_UTF8)`，支持中文

### 构建注意事项
- OpenCV DLL 必须在 PATH 中才能运行
- GUI 应用将插件逻辑内联（不依赖 DLL），控制器才使用 DLL 插件模式
- 测试框架为自实现，不依赖 GoogleTest
- JSON 解析为自实现，不依赖 nlohmann/json

---

## 9. 快速导航

| 想了解... | 看这里 |
|-----------|--------|
| 项目是什么 | [README.md](README.md) |
| 怎么构建 | [CMakeLists.txt](CMakeLists.txt#L76-L98) 底部注释 |
| 有哪些接口 | [include/](include/) 目录 |
| 命令协议格式 | [App.cpp:668-683](src/gui/App.cpp#L668-L683) |
| GUI 核心逻辑 | [App.cpp](src/gui/App.cpp) — `MonitorThread` + `CommThread` + `ExecuteCommand` |
| 截图实现 | [MonitorPlugin.cpp](src/monitor/MonitorPlugin.cpp) — `ScreenCapture::CaptureRegion` |
| OCR 实现 | [OCREngine.cpp](src/comm/OCREngine.cpp) |
| 操作模拟实现 | [Win32Action.cpp](src/action/Win32Action.cpp) |
| 配置格式 | [config.json](config/config.json) + [Config.h](include/core/Config.h) |
| 当前待办/Bug | [TODO](TODO) |
