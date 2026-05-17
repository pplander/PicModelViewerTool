# PicModelViewerTool

基于 **Qt 6 + OpenSceneGraph + Assimp** 的桌面三维模型查看与编辑工具，面向工程模型的浏览、属性编辑、批量转换与可视化前处理。

---

## 主要功能

### 模型加载与浏览
- 通过 Assimp 解析 40+ 主流模型格式（FBX / OBJ / STL / DAE / 3DS / PLY / GLTF 等），由 `AssimpToOSGConverter` 转为 OSG 场景图渲染。
- OSG `osgViewer` 嵌入 Qt `QOpenGLWidget`，支持轨迹球操作、拾取选择、自定义投影裁剪面。
- 多种显示模式：实体 / 线框 / 实体+线框 / 点云。
- 欢迎页 `WelcomeWidget` 展示最近文件与拖拽入口。

### 模型信息与场景树
- `ModelInfoDock`：顶点 / 三角形 / 包围盒 / 材质 / 纹理 / 加载耗时统计。
- `SceneTreeDock`：场景图层级树，节点选中与可见性切换，与 3D 视图双向联动。

### 节点编辑器（NodeEditorDock）
节点级 Tab 化属性编辑，未选中节点时自动降级到场景根：
- **Transform**：平移 / 旋转 / 缩放，自动包装非 Transform 节点。
- **Material**：环境光 / 漫反射 / 镜面 / 自发光 / 透明度 / 预设。
- **Texture**：贴图加载、UV 缩放（缺失 UV 时自动生成 planar UV，并以内联提示告知用户）。
- **Mesh**：网格简化（osgUtil::Simplifier 子树驱动）+ 法线重算（SmoothingVisitor）。
- **Scene**：背景色、光照、相机参数等场景级开关。
- 全部操作通过 `EditCommand.h` 的命令模式接入 `QUndoStack`，支持撤销 / 重做。

### 前处理特效（PreProcessDock + PreProcessManager）
按枚举分类的可视化效果集合：
- 模型炸开（子物体级 / 顶点级，幅度自适应模型尺度，可逆动画）。
- 离散模式选择、参数实时调节，状态由 PreProcessManager 集中持有。

### 批量格式转换（BatchConvertDialog）
- 多文件选择 → 目标格式选择 → 一次性转换。
- 复用 `ModelConverter` 与中文路径安全处理逻辑。

### 多语言
- 内置中 / 英文翻译（`translations/ModelViewer_zh_CN.ts` / `ModelViewer_en.ts`），运行时切换。
- 切换后 DockWidget 通过 `retranslateUi()` 同步刷新（含动态构建的子控件）。

---

## 技术栈

| 组件 | 版本 / 路径（开发环境） |
|---|---|
| Qt | 6.10.1 (msvc2022_64) — Core / Gui / Widgets / OpenGL / OpenGLWidgets / Xml / Svg / LinguistTools |
| OpenSceneGraph | 3.6.5 — `E:/Program/osg/osg` |
| Assimp | 6.0.4 (vc143-mt) — `E:/Program/assimp/assimp-6.0.4` |
| 编译器 | MSVC 2022 (v143) |
| C++ 标准 | C++17 |
| 构建系统 | CMake ≥ 3.20，多配置生成器（Visual Studio 17 2022） |

---

## 目录结构

```
PicModelViewerTool/
├── CMakeLists.txt
├── README.md
├── resources/                 # 图标 / QSS / qrc
├── translations/              # ts / qm 翻译资源
└── src/
    ├── main.cpp
    ├── MainWindow.{h,cpp}             # 主窗口、菜单、工具栏、Dock 装配
    ├── OSGWidget.{h,cpp}              # OSG 渲染视图、显示模式、拾取
    ├── ModelLoader.{h,cpp}            # 异步加载 + 中文路径处理
    ├── AssimpToOSGConverter.{h,cpp}   # Assimp → OSG 转换
    ├── ModelConverter.{h,cpp}         # 单文件格式转换
    ├── BatchConvertDialog.{h,cpp}     # 批量转换对话框
    ├── ModelInfoDock.{h,cpp}          # 模型信息面板
    ├── SceneTreeDock.{h,cpp}          # 场景树面板
    ├── NodeEditorDock.{h,cpp}         # 节点编辑器（Transform/Material/Texture/Mesh/Scene）
    ├── EditCommand.h                  # 撤销/重做命令集合
    ├── PreProcessManager.{h,cpp}      # 前处理特效后端
    ├── PreProcessDock.{h,cpp}         # 前处理特效面板
    ├── WelcomeWidget.{h,cpp}          # 欢迎页
    └── I18nManager.{h,cpp}            # 多语言切换
```

---

## 构建与运行

### 依赖准备
按 `CMakeLists.txt` 顶部路径变量配置本机依赖（或修改下列变量为本机实际位置）：

```cmake
set(Qt6_DIR    "D:/qt/6.10.1/msvc2022_64/lib/cmake/Qt6")
set(OSG_DIR    "E:/Program/osg/osg")
set(ASSIMP_DIR "E:/Program/assimp/assimp-6.0.4")
```

### 构建（Release）
项目仅维护 Release 配置。Visual Studio 多配置生成器示例：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

构建产物：`build/bin/Release/PicModelViewerTool.exe`

构建过程会自动：
- 调用 `lupdate` / `lrelease` 生成 `.qm` 翻译文件；
- 复制 OSG dll、`osgPlugins-3.6.5` 插件目录、Assimp dll 到输出目录。

### 运行
直接运行 `build/bin/Release/PicModelViewerTool.exe`，可通过命令行参数或拖拽打开模型文件。

---

## 撤销 / 重做

所有编辑性操作通过命令对象进入 `QUndoStack`：

| 命令 | 触发位置 |
|---|---|
| `TransformCommand` | NodeEditorDock → Transform |
| `MaterialCommand` | NodeEditorDock → Material |
| `TextureCommand` | NodeEditorDock → Texture |
| `SimplifyCommand` / `RecomputeNormalsCommand` | NodeEditorDock → Mesh |
| `SceneStateCommand` | NodeEditorDock → Scene |

工具栏左上角的弧形箭头按钮即 Undo / Redo。

---

## 开发约定

- **构建配置**：仅使用 Release，所有 `cmake --build` 必须显式 `--config Release`。
- **新增 UI 字符串**：用 `tr(...)` 包裹，最后 `lupdate -no-obsolete` 同步两份 `.ts`，再 `lrelease`。
- **DockWidget 国际化**：动态构建的子控件需在 `retranslateUi()` 中重建标签；如布局复杂可整体重建 Dock。
- **OSG 节点修改**：避免让模型节点 multi-parent；如需 overlay 走 `DEEP_COPY_NODES` 浅克隆，且按"懒构建 / 用完即拆"策略管理生命周期。

---

## License

本项目当前未声明开源许可证，使用前请联系作者确认授权范围。
