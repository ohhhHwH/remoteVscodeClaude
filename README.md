
# 初版设想
开发一个app在平板/手机上运行，监听电脑上的事件，并模拟点击/输入操作，实现各个ai终端的监工。

分为三个部分：
电脑端基于OpenCV的事件监控，基于win32 API的模拟点击/输入等操作，
通信，
手机端的app开发。


## 电脑端

设置忽略区和关注区，当发现关注区一定时间内没有变化时（说明AI不工作了），触发事件，发送给手机端。

监听微信、QQ等APP的消息通知，当有新消息时，触发事件，在电脑上执行相应的操作。

## 通信

直接通过现成的APP（如微信、QQ等）发送消息给手机端。
电脑通过监听这些消息来触发相应的操作。

如何获取这些消息？可以通过一些第三方库来监听这些APP的消息通知，或者通过一些系统级的API来获取这些消息.


## 手机端

现成的APP（如微信、QQ等）


# 整体设计

## 架构

多进程 + 插件架构，C++17实现。

```
主控进程 (Controller)
├── 屏幕监控插件 (ScreenMonitor)    - OpenCV截图对比，检测自定义区域变化
├── 通信插件 (Communicator)         - QQ/微信窗口OCR读取 + 模拟输入发送
├── 操作插件 (ActionExecutor)       - Win32 API模拟点击/输入
└── 配置管理 (ConfigManager)        - JSON配置文件
```

进程间通信：Windows命名管道 (Named Pipe)

## 核心接口

| 接口 | 职责 |
|------|------|
| IPlugin | 插件生命周期管理，DLL动态加载 |
| EventBus | 事件发布/订阅，模块间解耦 |
| IScreenCapture | Win32 BitBlt截图 |
| IRegionMonitor | OpenCV区域变化检测 |
| ICommunicator | 通过QQ/微信聊天窗口收发消息 |
| IOCR | OCR文字识别（Windows OCR API） |
| IActionExecutor | 模拟鼠标点击/键盘输入 |
| IPC | 命名管道进程间通信 |

## 工作流程

1. 主控进程启动，加载配置和插件
2. 屏幕监控插件定时截图自定义区域，OpenCV对比检测变化
3. 检测到变化时，通过EventBus发布事件
4. 通信插件收到事件，截图发送到QQ/微信聊天窗口
5. 通信插件定时OCR识别聊天窗口，读取手机端发来的指令
6. 操作插件执行指令（点击、输入等）


## 依赖

- OpenCV 4.x
- Win32 API
- Windows OCR API
- nlohmann/json（配置解析）


## 构建

PowerShell:
```powershell
# 配置
& "C:/Program1/MS/MStool2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" -S . -B build -DOpenCV_DIR="C:/Program Files/Opencv/opencv/build"

# 编译全部
& "C:/Program1/MS/MStool2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build build --config Debug

# 只编译GUI
& "C:/Program1/MS/MStool2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build build --config Debug --target gui_app
```

CMD:
```cmd
:: 配置
"C:/Program1/MS/MStool2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" -S . -B build -DOpenCV_DIR="C:/Program Files/Opencv/opencv/build"

:: 编译全部
"C:/Program1/MS/MStool2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build build --config Debug

:: 只编译GUI
"C:/Program1/MS/MStool2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build build --config Debug --target gui_app
```

## 运行

PowerShell:
```powershell
# 需要OpenCV DLL在PATH中
$env:PATH += ";C:\Program Files\Opencv\opencv\build\x64\vc16\bin"

# 运行GUI
build\bin\Debug\gui_app.exe

```

CMD:
```cmd
:: 需要OpenCV DLL在PATH中
set PATH=%PATH%;C:\Program Files\Opencv\opencv\build\x64\vc16\bin

:: 运行GUI
build\bin\Debug\gui_app.exe
```


# CMD

CMD:click:x,y	左键点击	坐标相对于monitor区域
CMD:dclick:x,y	双击	
CMD:rclick:x,y	右键点击	
CMD:move:x,y	移动光标	
CMD:type:text	输入文字	
CMD:key:name	按键	enter/tab/esc/space/backspace/delete/up/down/left/right/home/end
CMD:hotkey:mod+key	组合键	如 ctrl+c, alt+f4, ctrl+shift+s
CMD:wait:ms	等待	最大10000ms
CMD:scroll:x,y,delta	滚轮	delta正=上，负=下
CMD:stop	停止监控	

```
CMD:click:300,880;type:继续根todo完成下一个阶段的任务;key:enter
```