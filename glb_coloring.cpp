// glb_coloring_cleaned.cpp
// -----------------------------------------------------------------------------
// [Description] GLB Model Viewer with "Azure Glazed" Material Effect
// [描述] GLB 模型查看器，包含“青琉璃”材质效果及鼠标交互功能
// -----------------------------------------------------------------------------

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>

// =============================================================================
// GLOBAL INTERACTION VARIABLES / 全局交互变量
// =============================================================================

// Camera distance from the origin (Initial: 5.0)
// 摄像机距离原点的距离 (初始值: 5.0)
float cameraDistance = 5.0f;

// Mouse input state variables
// 鼠标输入状态变量
bool firstMouse = true;       // Is this the first mouse input? / 是否首次鼠标输入
bool isDragging = false;      // Is the left mouse button held down? / 是否正在按住左键拖拽
float lastX = 400.0f;         // Last X position of cursor / 上一次光标X坐标
float lastY = 300.0f;         // Last Y position of cursor / 上一次光标Y坐标

// Model rotation angles (Euler angles in degrees)
// 模型旋转角度 (欧拉角，单位：度)
float rotX = 0.0f; // Pitch (Rotation around X-axis) / 俯仰角 (绕X轴旋转)
float rotY = 0.0f; // Yaw (Rotation around Y-axis) / 偏航角 (绕Y轴旋转)

// =============================================================================
// FUNCTION PROTOTYPES / 函数原型声明
// =============================================================================
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// =============================================================================
// DATA STRUCTURES / 数据结构
// =============================================================================
struct PrimitiveObject {
    GLuint vao;             // Vertex Array Object ID / 顶点数组对象ID
    GLsizei count;          // Index count / 索引数量
    glm::vec3 baseColor;    // Base color of the material / 材质基础色
    float roughness;        // Roughness factor / 粗糙度
    float metallic;         // Metallic factor / 金属度
    float transmission;     // Transmission factor (Transparency) / 透射率 (透明度)
};

// =============================================================================
// SHADERS / 着色器源码
// =============================================================================

// Vertex Shader: Transforms vertex positions and normals
// 顶点着色器：转换顶点位置和法线
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 vWorldPos;
out vec3 vNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // Calculate position in world space
    // 计算世界空间中的位置
    vWorldPos = vec3(model * vec4(aPos, 1.0));
    
    // Calculate normal matrix to handle non-uniform scaling
    // 计算法线矩阵以处理非均匀缩放
    vNormal = mat3(transpose(inverse(model))) * aNormal; 
    
    gl_Position = projection * view * vec4(vWorldPos, 1.0);
}
)";

// Fragment Shader: Implements the "Azure Glazed" effect
// 片段着色器：实现“青琉璃”材质效果
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 vWorldPos;
in vec3 vNormal;

// Material Uniforms / 材质统一变量
uniform vec3 u_BaseColor;
uniform float u_Roughness;
uniform float u_Transmission; 

// Scene Uniforms / 场景统一变量
uniform vec3 u_LightPos;
uniform vec3 u_ViewPos;

void main() {
    // 1. Normal Correction for Double-Sided Rendering
    // 1. 双面渲染的法线修正
    vec3 N_raw = normalize(vNormal);
    vec3 N = gl_FrontFacing ? N_raw : -N_raw;

    vec3 L = normalize(u_LightPos - vWorldPos);
    vec3 V = normalize(u_ViewPos - vWorldPos);
    vec3 H = normalize(L + V);
    
    // Avoid division by zero or negative dot products
    // 避免除零或负点积
    float NdotV = max(dot(N, V), 0.001);

    // 2. Fresnel Effect (Schlick's approximation)
    // 2. 菲涅尔效应 (Schlick 近似)
    float F0 = 0.04; 
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 4.0);

    // 3. Enhanced Specular Highlight
    // 3. 增强的高光反射
    float specPower = (1.0 - u_Roughness) * 128.0; 
    float spec = pow(max(dot(N, H), 0.0), specPower);
    vec3 specular = vec3(spec) * fresnel * 4.0; 

    // 4. Diffuse & Fake Subsurface Scattering (SSS)
    // 4. 漫反射与伪次表面散射
    float diff = max(dot(N, L), 0.0);
    vec3 ambient = u_BaseColor * 0.55; 
    vec3 diffuse = diff * u_BaseColor * 1.8;

    // 5. Alpha/Transparency Calculation
    // 5. 透明度计算
    // Adjust alpha based on Fresnel to simulate glass edges being more opaque
    // 基于菲涅尔调整Alpha值，模拟玻璃边缘更不透明的效果
    float alpha = clamp((1.0 - u_Transmission) + fresnel * 0.5, 0.3, 0.95);

    // 6. Final Composition
    // 6. 最终合成
    vec3 result = ambient + diffuse + specular;
    
    // Simple Tone Mapping (Reinhard-ish)
    // 简单的色调映射
    result = result / (result + vec3(1.0));

    FragColor = vec4(result, alpha);
}
)";

// =============================================================================
// UTILITY FUNCTIONS / 工具函数
// =============================================================================

/**
 * Creates and links the shader program.
 * 创建并链接着色器程序。
 * * @return GLuint The ID of the compiled shader program / 编译好的着色器程序ID
 */
GLuint createShaderProgram() {
    // Compile Vertex Shader / 编译顶点着色器
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "VERTEX SHADER ERROR:\n" << infoLog << std::endl;
    }

    // Compile Fragment Shader / 编译片段着色器
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "FRAGMENT SHADER ERROR:\n" << infoLog << std::endl;
    }

    // Link Program / 链接程序
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Cleanup / 清理
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

/**
 * Loads a GLB model and extracts mesh data.
 * 加载GLB模型并提取网格数据。
 * * @param filename Path to the .glb file / .glb 文件路径
 * @param outPrimitives Vector to store loaded primitives / 用于存储加载图元的向量
 * @return true if successful, false otherwise / 成功返回true，否则false
 */
bool loadGLBModel(const std::string& filename, std::vector<PrimitiveObject>& outPrimitives) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    if (!warn.empty()) std::cout << "Warn: " << warn << std::endl;
    if (!err.empty()) std::cerr << "Err: " << err << std::endl;
    if (!ret) return false;

    // Iterate over meshes and primitives / 遍历网格和图元
    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            PrimitiveObject primObj;

            // 1. Get Position Attributes / 获取位置属性
            const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
            const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];
            const float* positions = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

            // 2. Get Normal Attributes / 获取法线属性
            auto itNorm = primitive.attributes.find("NORMAL");
            if (itNorm != primitive.attributes.end()) {
                const tinygltf::Accessor& normAccessor = model.accessors[itNorm->second];
                const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
                const tinygltf::Buffer& normBuffer = model.buffers[normView.buffer];
                const float* normals = reinterpret_cast<const float*>(&normBuffer.data[normView.byteOffset + normAccessor.byteOffset]);

                // Setup OpenGL Buffers / 设置OpenGL缓冲区
                glGenVertexArrays(1, &primObj.vao);
                glBindVertexArray(primObj.vao);

                GLuint vbo[2];
                glGenBuffers(2, vbo);

                // Bind Positions (Location 0) / 绑定位置 (Location 0)
                glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
                glBufferData(GL_ARRAY_BUFFER, posAccessor.count * 3 * sizeof(float), positions, GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
                glEnableVertexAttribArray(0);

                // Bind Normals (Location 1) / 绑定法线 (Location 1)
                glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
                glBufferData(GL_ARRAY_BUFFER, normAccessor.count * 3 * sizeof(float), normals, GL_STATIC_DRAW);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
                glEnableVertexAttribArray(1);

                // 3. Get Indices / 获取索引
                const tinygltf::Accessor& idxAccessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& idxView = model.bufferViews[idxAccessor.bufferView];
                const tinygltf::Buffer& idxBuffer = model.buffers[idxView.buffer];

                GLuint ebo;
                glGenBuffers(1, &ebo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxView.byteLength, &idxBuffer.data[idxView.byteOffset + idxAccessor.byteOffset], GL_STATIC_DRAW);

                primObj.count = (GLsizei)idxAccessor.count;
                primObj.baseColor = glm::vec3(1.0f); // Default placeholder / 默认占位符

                glBindVertexArray(0);
                outPrimitives.push_back(primObj);
            }
        }
    }
    return true;
}

// =============================================================================
// MAIN FUNCTION / 主函数
// =============================================================================
int main() {
    // 1. Initialize GLFW / 初始化 GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "GLB Viewer - Mouse Rotate", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // 2. Register Callbacks / 注册回调函数
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // 3. Initialize GLAD / 初始化 GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // 4. Global OpenGL State / 全局 OpenGL 状态
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // Enable Face Culling / 开启面剔除
    glEnable(GL_BLEND);     // Enable Blending for Transparency / 开启混合以支持透明
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint shaderProgram = createShaderProgram();
    std::vector<PrimitiveObject> sceneObjects;

    // Load Model (Ensure "model.glb" exists) / 加载模型 (确保 "model.glb" 存在)
    if (!loadGLBModel("model.glb", sceneObjects)) {
        std::cout << "Failed to load model!" << std::endl;
        // Don't exit immediately to allow viewing the window, but essentially failed
        return -1;
    }

    // -------------------------------------------------------------------------
    // RENDER LOOP / 渲染循环
    // -------------------------------------------------------------------------
    while (!glfwWindowShouldClose(window)) {
        // Clear screen with dark background / 深色背景清屏
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // A. Update Camera (Based on scroll) / 更新摄像机 (基于滚轮)
        glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, cameraDistance);
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // B. Update Model Matrix (Based on mouse drag) / 更新模型矩阵 (基于鼠标拖拽)
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, glm::radians(rotX), glm::vec3(1.0f, 0.0f, 0.0f)); // X-axis rotation / X轴旋转
        model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f)); // Y-axis rotation / Y轴旋转

        // C. Update Projection Matrix / 更新投影矩阵
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 3000.0f);

        // Upload Matrices to Shader / 上传矩阵到着色器
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // D. Update Lighting / 更新光照
        // Headlight mode: Light follows camera / 头灯模式：光随摄像机动
        glUniform3f(glGetUniformLocation(shaderProgram, "u_LightPos"), cameraPos.x, cameraPos.y, cameraPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram, "u_ViewPos"), cameraPos.x, cameraPos.y, cameraPos.z);

        // E. Draw Objects / 绘制物体
        // Disable depth writing for transparency correctness in complex shapes
        // 关闭深度写入以保证复杂形状的透明度正确性
        glDepthMask(GL_FALSE);

        // --- Pass 1: Draw Back Faces Only / 第一遍：只绘制背面 ---
        glCullFace(GL_FRONT); // Cull front faces -> only back faces remain / 剔除正面 -> 只留背面

        for (const auto& obj : sceneObjects) {
            glBindVertexArray(obj.vao);
            // Set Material Properties / 设置材质属性
            glUniform3f(glGetUniformLocation(shaderProgram, "u_BaseColor"), 0.3f, 0.72f, 0.65f);
            glUniform1f(glGetUniformLocation(shaderProgram, "u_Roughness"), 0.15f);
            glUniform1f(glGetUniformLocation(shaderProgram, "u_Transmission"), 0.8f);
            glDrawElements(GL_TRIANGLES, obj.count, GL_UNSIGNED_INT, 0);
        }

        // --- Pass 2: Draw Front Faces Only / 第二遍：只绘制正面 ---
        glCullFace(GL_BACK); // Cull back faces -> standard rendering / 剔除背面 -> 标准渲染

        for (const auto& obj : sceneObjects) {
            glBindVertexArray(obj.vao);
            // Re-set uniforms (state machine safety) / 重新设置统一变量 (状态机安全)
            glUniform3f(glGetUniformLocation(shaderProgram, "u_BaseColor"), 0.3f, 0.72f, 0.65f);
            glUniform1f(glGetUniformLocation(shaderProgram, "u_Roughness"), 0.15f);
            glUniform1f(glGetUniformLocation(shaderProgram, "u_Transmission"), 0.8f);
            glDrawElements(GL_TRIANGLES, obj.count, GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// =============================================================================
// CALLBACK IMPLEMENTATIONS / 回调函数实现
// =============================================================================

// Handle window resize
// 处理窗口大小调整
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// Handle mouse button clicks
// 处理鼠标按键点击
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            isDragging = true;
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            lastX = (float)xpos;
            lastY = (float)ypos;
        }
        else if (action == GLFW_RELEASE) {
            isDragging = false;
        }
    }
}

// Handle mouse movement for rotation
// 处理鼠标移动以进行旋转
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    if (isDragging) {
        float xoffset = (float)xpos - lastX;
        float yoffset = (float)ypos - lastY;

        lastX = (float)xpos;
        lastY = (float)ypos;

        float sensitivity = 0.5f;
        rotY += xoffset * sensitivity; // Yaw / 偏航
        rotX += yoffset * sensitivity; // Pitch / 俯仰
    }
}

// Handle mouse scroll for zoom
// 处理鼠标滚轮以进行缩放
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    cameraDistance -= (float)yoffset;
    // Clamp distance / 限制距离范围
    if (cameraDistance < 1.0f) cameraDistance = 1.0f;
    if (cameraDistance > 50.0f) cameraDistance = 50.0f;
}