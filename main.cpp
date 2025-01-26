#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <sstream>

// Vertex Shader (updated for lighting)
const char* vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 mvp;
uniform mat4 model;

out vec3 FragPos;
out vec3 Normal;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = mvp * vec4(aPos, 1.0);
}
)glsl";

// Fragment Shader (updated for lighting)
const char* fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 objectColor;
uniform vec3 lightDir;
uniform vec3 lightColor;

void main() {
    // Ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDirNorm = normalize(-lightDir);
    float diff = max(dot(norm, lightDirNorm), 0.0);
    vec3 diffuse = diff * lightColor;

    // Combine
    vec3 result = (ambient + diffuse) * objectColor;
    FragColor = vec4(result, 1.0);
}
)glsl";

// Outline Shader (unchanged)
const char* outlineFragmentShader = R"glsl(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
)glsl";

struct Mesh {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<unsigned int> indices;
    std::vector<unsigned int> edgeIndices;
};

Mesh loadOBJ(const char* path) {
    Mesh mesh;
    std::ifstream file(path);
    std::string line;
    std::vector<glm::vec3> tempVertices;
    std::vector<glm::vec3> tempNormals;

    while (std::getline(file, line)) {
        if (line.substr(0, 2) == "v ") {
            std::istringstream ss(line.substr(2));
            glm::vec3 vertex;
            ss >> vertex.x >> vertex.y >> vertex.z;
            tempVertices.push_back(vertex);
        }
        else if (line.substr(0, 3) == "vn ") {
            std::istringstream ss(line.substr(3));
            glm::vec3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            tempNormals.push_back(normal);
        }
        else if (line.substr(0, 2) == "f ") {
            std::istringstream ss(line.substr(2));
            std::string token;
            std::vector<unsigned int> faceVerts, faceNormals;

            while (ss >> token) {
                size_t pos1 = token.find('/');
                size_t pos2 = token.find('/', pos1 + 1);
                unsigned int v = std::stoul(token.substr(0, pos1)) - 1;
                unsigned int n = std::stoul(token.substr(pos2 + 1)) - 1;
                faceVerts.push_back(v);
                faceNormals.push_back(n);
            }

            // Triangulate face
            for (size_t i = 1; i < faceVerts.size() - 1; i++) {
                mesh.indices.push_back(faceVerts[0]);
                mesh.indices.push_back(faceVerts[i]);
                mesh.indices.push_back(faceVerts[i + 1]);

                // Add normals
                for (int j = 0; j < 3; j++) {
                    glm::vec3 normal = tempNormals[faceNormals[j]];
                    mesh.normals.push_back(normal.x);
                    mesh.normals.push_back(normal.y);
                    mesh.normals.push_back(normal.z);
                }
            }
        }
    }

    // Add vertices to mesh
    for (auto& vertex : tempVertices) {
        mesh.vertices.push_back(vertex.x);
        mesh.vertices.push_back(vertex.y);
        mesh.vertices.push_back(vertex.z);
    }

    return mesh;
}

unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(id, 512, nullptr, infoLog);
        std::cerr << "Shader error:\n" << infoLog << std::endl;
    }
    return id;
}

unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    unsigned int program = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program linking error:\n" << infoLog << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Red Teapot with Lighting", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Load teapot data
    Mesh mesh = loadOBJ("teapot.obj");

    // Create and bind buffers
    unsigned int VAO, VBO, EBO, NBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glGenBuffers(1, &NBO);

    glBindVertexArray(VAO);

    // Vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(float), mesh.vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal buffer
    glBindBuffer(GL_ARRAY_BUFFER, NBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.normals.size() * sizeof(float), mesh.normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    // Element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int), mesh.indices.data(), GL_STATIC_DRAW);

    // Create shaders
    unsigned int mainShader = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    unsigned int outlineShader = createShaderProgram(vertexShaderSource, outlineFragmentShader);

    glEnable(GL_DEPTH_TEST);

    // Lighting setup
    glm::vec3 lightDir(-0.2f, -1.0f, -0.3f); // Directional light
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);  // White light
    glm::vec3 objectColor(1.0f, 0.0f, 0.0f); // Blue teapot

    // Rotation and zoom variables
    float angleY = 0.0f;
    float angleZ = 0.0f;
    float rotationSpeed = 2.0f;
    float zoomSpeed = 10.0f;
    float cameraDistance = 5.196f; // Initial distance (sqrt(3²+3²+3²))
    glm::vec3 cameraDir = glm::normalize(glm::vec3(1.0f)); // Original direction (3,3,3) normalized
    float lastFrameTime = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrameTime;
        lastFrameTime = currentFrame;

        // Input handling
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Rotation controls
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            angleY += rotationSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            angleY -= rotationSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            angleZ += rotationSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            angleZ -= rotationSpeed * deltaTime;

        // Zoom controls
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            cameraDistance -= zoomSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            cameraDistance += zoomSpeed * deltaTime;

        // Clamp camera distance
        cameraDistance = glm::clamp(cameraDistance, 1.5f, 40.0f);

        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Calculate camera position
        glm::vec3 eye = cameraDir * cameraDistance;

        // Create transformation matrices
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(
            eye,
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, angleY, glm::vec3(0.0f, 1.0f, 0.0f)); // Y-axis
        model = glm::rotate(model, angleZ, glm::vec3(0.0f, 0.0f, 1.0f)); // Z-axis

        glm::mat4 mvp = projection * view * model;

        // Draw main teapot
        glUseProgram(mainShader);
        glUniformMatrix4fv(glGetUniformLocation(mainShader, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(glGetUniformLocation(mainShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(mainShader, "objectColor"), 1, &objectColor[0]);
        glUniform3fv(glGetUniformLocation(mainShader, "lightDir"), 1, &lightDir[0]);
        glUniform3fv(glGetUniformLocation(mainShader, "lightColor"), 1, &lightColor[0]);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);

        // Draw outline
        //glUseProgram(outlineShader);
        //glUniformMatrix4fv(glGetUniformLocation(outlineShader, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
        //glLineWidth(3.0f);
        //glDrawElements(GL_LINES, mesh.edgeIndices.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteBuffers(1, &NBO);
    glDeleteProgram(mainShader);
    glDeleteProgram(outlineShader);

    glfwTerminate();
    return 0;
}