# TODO

---

### 已完成 ✅

- [x] CMD:type:text 中文输入 — 通过剪贴板 `CF_UNICODETEXT` + `Ctrl+V` 粘贴实现
- [x] 告警后回传标注网格截图 — 每 100px 画线标注坐标，每 25px 加点
- [x] 命令执行后自动回传结果截图 — `ExecuteCommands()` 末尾调用 `SendImageToComm()`
- [x] **自定义处理策略** — 检测到 AI idle 后自动执行预设动作序列
- [x] **自动点击 Yes 按钮** — 多尺度模板匹配检测并点击 Yes/确认 按钮

---

## - TODO VLM 视觉判断识别 — 接入视觉语言模型分析截图状态
    - 依赖项：网络请求能力（WinHTTP / libcurl）、VLM API（Claude/OpenAI/Gemini）、
      图片编码为 base64（已有 OpenCV imencode）、配置文件 API Key 存储

    功能说明：
        当前变化检测使用像素帧差法（cv::absdiff），只能判断"画面是否变化"，无法理解
        "AI 在做什么/卡住了没/进度几何"。引入 VLM 可以：
        1. 截图 → base64 → 发送到 VLM API → 获取结构化状态分析
        2. 输出：状态（working/stuck/idle/done）、建议动作、进度百分比
        3. 结合自定义策略：VLM 输出作为策略触发条件

    需要修改的文件：
        [src/gui/App.h](src/gui/App.h)
            - AppState 新增 vlmEnabled / vlmApiKey / vlmApiEndpoint / vlmModel / vlmPrompt
            - 新增方法 VLMResult AnalyzeWithVLM(const cv::Mat& screenshot)
            - 新增方法 string ImgToBase64(const cv::Mat& img)

        [src/gui/App.cpp](src/gui/App.cpp)
            - MonitorThread() 循环中：当 VLM 启用时，每隔 N 个轮询周期调用 AnalyzeWithVLM()
            - AnalyzeWithVLM() 实现：imencode → base64 → WinHTTP POST → 解析结构化 JSON
            - RenderMonitorPanel() 新增 VLM 配置 UI

        **新增文件**（建议）：
        [src/core/VLMClient.h] + [src/core/VLMClient.cpp] — VLM HTTP 客户端封装

    需要实现的功能 API：
        ```cpp
        struct VLMResult {
            string status;       // "working" | "idle" | "error" | "done" | "unknown"
            string suggestion;   // "click_continue" | "type_enter" | "none"
            string detail;       // 详细描述
            float confidence;    // 置信度 0.0~1.0
        };
        string ImgToBase64(const cv::Mat& img);
        VLMResult AnalyzeWithVLM(const cv::Mat& screenshot);
        ```

    需要生成的测试：
        [test/test_vlm_client.cpp] — base64 编码、JSON 请求构造、响应解析

---

## - TODO 日志系统 — 文件日志记录 + 日志级别 + 滚动
    - 依赖项：文件 I/O（C++ fstream）、时间戳（chrono）、现有的内存日志（已有 logMutex_ + vector<string>）

    功能说明：
        当前日志仅保存在内存 vector<string> 中，GUI 关闭即丢失。需要实现：
        1. 日志写入文件（logs/RemoteMonitor_YYYYMMDD.log）
        2. 日志级别：DEBUG < INFO < WARN < ERROR
        3. 内存日志 + 文件日志双写
        4. 按天滚动或按文件大小滚动（超过 10MB 自动切分）
        5. 日志格式：[2026-06-07 21:30:15] [INFO] [Monitor] Change detected: 15%
        6. 在 GUI 的 Log 面板增加分类筛选下拉框

    需要修改的文件：
        [src/gui/App.h](src/gui/App.h)
            - 将 monitorLog_/commLog_/actionLog_ 替换为 Logger 实例
        [src/gui/App.cpp](src/gui/App.cpp)
            - 所有 push_back 改为 Log(category, msg) 调用
            - RenderLogPanel() 增加分类筛选

        **新增文件**（建议）：
        [src/core/Logger.h] + [src/core/Logger.cpp] — 通用日志模块

    需要实现的功能 API：
        ```cpp
        enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };
        class Logger {
            bool Init(const string& logDir, LogLevel level, int maxSizeMB);
            void Log(LogLevel level, const string& category, const string& message);
            void Debug/Info/Warn/Error(const string& category, const string& msg);
            vector<string> GetRecent(int count = 50) const;
            vector<string> GetByCategory(const string& category) const;
            void SetLevel(LogLevel level);
        };
        ```

    需要生成的测试：
        [test/test_logger.cpp] — 日志写入、滚动、多线程安全、级别过滤

## TODO - 异常情况的处理，其他位置有弹窗