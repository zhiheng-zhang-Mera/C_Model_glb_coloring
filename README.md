# GLB Viewer - Azure Glazed Edition (青琉璃渲染器)

## 简介 (Introduction)
这是一个基于 OpenGL (C++) 的轻量级 GLB 模型查看器。它不仅能加载 `.glb` 格式的 3D 模型，还通过自定义 Shader 实现了独特的“青琉璃” (Azure Glazed) 材质效果。该效果结合了菲涅尔反射 (Fresnel)、伪次表面散射 (Fake SSS) 和基于物理的透明度计算，呈现出类似玉石或琉璃的半透明质感。

## 主要特性 (Features)
* **GLB 模型加载**: 使用 `tinygltf` 库解析二进制 glTF 文件。
* **青琉璃材质**: 硬编码的高级材质效果，模拟半透明玉石质感。
* **交互控制**: 
    * 鼠标左键拖拽：旋转模型 (360度)。
    * 鼠标滚轮：缩放视角 (推近/拉远)。
* **双面渲染**: 包含两遍渲染流程 (Two-Pass Rendering)，正确处理透明物体的背面和正面遮挡关系。
* **头灯光照**: 光源始终跟随摄像机视角，确保模型细节清晰可见。

## 依赖库 (Dependencies)
本项目依赖以下开源库（请确保环境已配置）：
* **OpenGL 3.3+**: 核心图形库。
* **GLFW**: 用于创建窗口和处理输入。
* **GLAD**: 用于加载 OpenGL 函数指针。
* **GLM**: 数学库 (矩阵、向量运算)。
* **TinyGLTF**: (Header-only) 用于加载 GLB 模型 (需包含 `stb_image.h` 和 `stb_image_write.h`)。

## 目录结构 (Directory Structure)
```text
.
├── glb_coloring.cpp      # 主程序源代码
├── tiny_gltf.h           # TinyGLTF 头文件
├── json.hpp              # (TinyGLTF 依赖)
├── stb_image.h           # (TinyGLTF 依赖)
├── stb_image_write.h     # (TinyGLTF 依赖)
├── model.glb             # 目标模型文件 (必须存在)
└── README.md             
