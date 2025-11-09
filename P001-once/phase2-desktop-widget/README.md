下面是新的阶段文档草案，可以直接放入
`Project001-Once/phase2-desktop-widget/README.md`
作为 **Phase 2 — Desktop Widget** 的完整 PRD。

---

# Project001 · Once

## Phase 2 — Desktop Widget

> “让时间常驻桌面。”

---

### 一、背景与动机

在 Phase 1 的 web 原型中，`Once` 已经实现了核心体验：
按下一次开始计时、再次按下结束，随后给出偏差与 sweet spot 评分。
它满足了“行为-反馈-反思”的闭环，但仍需浏览器承载。
用户在日常桌面操作时，启动网页显得繁琐，也无法始终停留在视线范围内。

**Phase 2** 的目标是：
让 Once 成为一个**跨平台、可置顶、低占用的桌面小组件**，
既保留 Phase 1 的温度与风格，又融入操作系统的节奏。

---

### 二、目标定义

**目标一句话：**

> 构建一个跨平台的桌面 Stopwatch Widget，
> 在 Windows /macOS /Linux 上均可运行、置顶显示，
> 支持 Start / Stop 快捷键与软键盘输入目标时间。

**关键体验指标：**

1. 启动时间 ≤ 1 秒。
2. CPU 占用 < 2 % （静止状态）。
3. 内存 < 50 MB。
4. 全局快捷键响应 < 100 ms。

---

### 三、产品结构

#### 3.1 主窗口

* 无边框、可拖动、圆角矩形。
* 背景透明，模仿 Phase 1 的 LCD 外观与橙色背光。
* 支持 **always-on-top** 开关。
* 拖动区域：窗口上半部；按钮区设置 `no-drag`。
* 宽 ≈ 360 px × 高 ≈ 160 px。

#### 3.2 显示区域

* 与 web 版保持一致：`MM:SS.d` 显示。
* 顶部三灯仍为“小时进位指示”。
* sweet spot 评分在停表瞬间淡入，再淡出。

#### 3.3 交互控件

| 动作           | 方式                          | 说明         |
| ------------ | --------------------------- | ---------- |
| Start / Stop | 点击主按钮 或 全局快捷键 `Alt + Space` | 启动 / 结束 计时 |
| 设置目标         | 右侧 「目标」 按钮 → 弹出虚拟键盘 模态框     | 仅数字与小数点输入  |
| 置顶开关         | 托盘菜单 → 「保持置顶」               | 勾选状态同步至窗口  |
| 退出           | 托盘菜单 → 「退出 Once」            | 结束进程       |
| 快捷键设置        | 预留 settings.json 修改接口       | 后续扩展       |

#### 3.4 托盘菜单

* 「打开 Once」
* 「保持置顶」 (checkbox)
* 「开机自启」 (checkbox)
* 「退出」

#### 3.5 数据持久化

* 最近 5 次 计时结果 与 目标值保存至本地 JSON。
* 存放路径 `appDir()/data/once_state.json`。
* 数据结构示例：

  ```json
  { "lastLap": 12.34, "target": 10.00, "history": [11.9, 10.3, 12.0, 9.8, 12.3] }
  ```

---

### 四、技术实现

#### 4.1 框架

* **Tauri v2** （Rust + WebView）为核心。
* 前端直接复用 Phase 1 的 HTML/CSS/JS。
* 构建脚本：

  ```bash
  bun build phase1-web-prototype --out-dir src-tauri/dist
  cargo tauri build
  ```

#### 4.2 窗口与系统接口

| 功能         | API / 模块                                                           |
| ---------- | ------------------------------------------------------------------ |
| 无边框 + 透明窗口 | `tauri::WindowBuilder` 配置 `transparent: true` `decorations: false` |
| 置顶         | `window.set_always_on_top(true)`                                   |
| 托盘         | `tauri::SystemTray`                                                |
| 全局快捷键      | `tauri::global_shortcut`                                           |
| 文件读写       | `tauri::api::path::app_dir` + `fs`                                 |
| 本地通知（可选）   | `tauri-plugin-notification`                                        |

#### 4.3 安全与性能

* 禁用 HTTP 请求与远程内容。
* WebView 加载本地资源 (`asset:` 协议)。
* 构建产物 ≤ 10 MB。

---

### 五、时间计划

| 阶段     | 内容                         | 预期时长 |
| ------ | -------------------------- | ---- |
| Week 1 | 初始化 Tauri 项目，迁移 Phase 1 前端 | 2 天  |
| Week 2 | 实现窗口拖动、置顶、托盘               | 2 天  |
| Week 3 | 全局快捷键 + 虚拟键盘模态框            | 3 天  |
| Week 4 | 数据持久化、打包测试                 | 3 天  |
| Week 5 | 视觉微调、文档与 Quickstart        | 1 天  |

---

### 六、交付物

1. 可执行程序 （`Once-win.exe` / `Once-mac.app` / `Once-linux.AppImage`）。
2. `README.md` + `Quickstart.pdf` 一页复现说明。
3. Tauri 源代码与配置文件。
4. 更新后的 favicon 与应用图标。
5. 版本标签： `v0.2.0-desktop-alpha`。

### 七、验收标准

* 双击即启动，1 秒内显示主窗口。
* Start/Stop 逻辑无延迟，评分正常。
* 关闭窗口后托盘驻留；退出时清理干净。
* 任一系统下窗口置顶功能可开关。
* 目标键盘输入、分数提示与 Phase 1 体验一致。

