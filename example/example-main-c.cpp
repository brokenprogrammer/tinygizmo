// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include <iostream>
#include <chrono>

#include "util.hpp"
#include "gl-api.hpp"
#include "teapot.h"

// Include the C wrapper instead of the C++ API
#include "../src/tiny-gizmo-c.h"

using namespace minalg;

static inline uint64_t get_local_time_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

const linalg::aliases::float4x4 identity4x4 = { { 1, 0, 0, 0 },{ 0, 1, 0, 0 },{ 0, 0, 1, 0 },{ 0, 0, 0, 1 } };

constexpr const char gizmo_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    layout(location = 1) in vec3 normal;
    layout(location = 2) in vec4 color;
    out vec4 v_color;
    out vec3 v_world, v_normal;
    uniform mat4 u_mvp;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
        v_color = color;
        v_world = vertex;
        v_normal = normal;
    }
)";

constexpr const char gizmo_frag[] = R"(#version 330
    in vec4 v_color;
    in vec3 v_world, v_normal;
    out vec4 f_color;
    uniform vec3 u_eye;
    void main()
    {
        vec3 light = vec3(1) * max(dot(v_normal, normalize(u_eye - v_world)), 0.50) + 0.25;
        f_color = v_color * vec4(light, 1);
    }
)";

constexpr const char lit_vert[] = R"(#version 330
    uniform mat4 u_modelMatrix;
    uniform mat4 u_viewProj;

    layout(location = 0) in vec3 inPosition;
    layout(location = 1) in vec3 inNormal;

    out vec3 v_position, v_normal;

    void main()
    {
        vec4 worldPos = u_modelMatrix * vec4(inPosition, 1);
        v_position = worldPos.xyz;
        v_normal = normalize((u_modelMatrix * vec4(inNormal,0)).xyz);
        gl_Position = u_viewProj * worldPos;
    }
)";

constexpr const char lit_frag[] = R"(#version 330
    uniform vec3 u_diffuse = vec3(1, 1, 1);
    uniform vec3 u_eye;

    in vec3 v_position;
    in vec3 v_normal;

    out vec4 f_color;
    
    vec3 compute_lighting(vec3 eyeDir, vec3 position, vec3 color)
    {
        vec3 light = vec3(0, 0, 0);
        vec3 lightDir = normalize(position - v_position);
        light += color * u_diffuse * max(dot(v_normal, lightDir), 0);
        vec3 halfDir = normalize(lightDir + eyeDir);
        light += color * u_diffuse * pow(max(dot(v_normal, halfDir), 0), 128);
        return light;
    }

    void main()
    {
        vec3 eyeDir = vec3(0, 1, -2);
        vec3 light = vec3(0, 0, 0);
        light += compute_lighting(eyeDir, vec3(+3, 1, 0), vec3(235.0/255.0, 43.0/255.0, 211.0/255.0));
        light += compute_lighting(eyeDir, vec3(-3, 1, 0), vec3(43.0/255.0, 236.0/255.0, 234.0/255.0));
        f_color = vec4(light + vec3(0.5, 0.5, 0.5), 1.0);
    }
)";

//////////////////////////
//   Main Application   //
//////////////////////////

// Define a C++ geometry_mesh struct for compatibility with OpenGL rendering
struct geometry_mesh
{
    std::vector<TG_GeometryVertex> vertices;
    std::vector<TG_UInt3> triangles;
};

geometry_mesh make_teapot()
{
    geometry_mesh mesh;
    for (int i = 0; i < 4974; i += 6)
    {
        TG_GeometryVertex v;
        v.position = {teapot_vertices[i + 0], teapot_vertices[i + 1], teapot_vertices[i + 2]};
        v.normal = {teapot_vertices[i + 3], teapot_vertices[i + 4], teapot_vertices[i + 5]};
        v.color = {1.0f, 1.0f, 1.0f, 1.0f}; // Default white color
        mesh.vertices.push_back(v);
    }
    for (int i = 0; i < 4680; i += 3)
    {
        mesh.triangles.push_back({(uint32_t)teapot_triangles[i + 0], 
                                  (uint32_t)teapot_triangles[i + 1], 
                                  (uint32_t)teapot_triangles[i + 2]});
    }
    return mesh;
}

void draw_mesh(GlShader& shader, GlMesh& mesh, const linalg::aliases::float3 eye, 
               const linalg::aliases::float4x4& viewProj, const linalg::aliases::float4x4& model)
{
    linalg::aliases::float4x4 modelViewProjectionMatrix = mul(viewProj, model);
    shader.bind();
    shader.uniform("u_mvp", modelViewProjectionMatrix);
    shader.uniform("u_eye", eye);
    mesh.draw_elements();
    shader.unbind();
}

void draw_lit_mesh(GlShader& shader, GlMesh& mesh, const linalg::aliases::float3 eye, 
                  const linalg::aliases::float4x4& viewProj, const linalg::aliases::float4x4& model)
{
    shader.bind();
    shader.uniform("u_viewProj", viewProj);
    shader.uniform("u_modelMatrix", model);
    shader.uniform("u_eye", eye);
    mesh.draw_elements();
    shader.unbind();
}

void upload_mesh(const geometry_mesh& cpu, GlMesh& gpu)
{
    gpu.set_vertices(reinterpret_cast<const std::vector<linalg::aliases::float3>&>(cpu.vertices), GL_DYNAMIC_DRAW);
    gpu.set_attribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(TG_GeometryVertex), (GLvoid*)offsetof(TG_GeometryVertex, position));
    gpu.set_attribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(TG_GeometryVertex), (GLvoid*)offsetof(TG_GeometryVertex, normal));
    gpu.set_attribute(2, 4, GL_FLOAT, GL_FALSE, sizeof(TG_GeometryVertex), (GLvoid*)offsetof(TG_GeometryVertex, color));
    gpu.set_elements(reinterpret_cast<const std::vector<linalg::aliases::uint3>&>(cpu.triangles), GL_DYNAMIC_DRAW);
}

// Callback function for rendering gizmo geometry
void render_gizmo_mesh(TG_GeometryMesh mesh, void* user_data)
{
    if (!mesh) return;
    
    // Unpack the user data
    struct CallbackData {
        GlMesh* gizmoMesh;
        GlShader* wireframeShader;
        linalg::aliases::float3 camPosition;
        linalg::aliases::float4x4 viewProjMatrix;
    };
    
    auto data = static_cast<CallbackData*>(user_data);
    
    // Create a local geometry mesh and copy the data
    geometry_mesh localMesh;
    
    uint32_t vertexCount = TG_GetGeometryMeshVertexCount(mesh);
    uint32_t triangleCount = TG_GetGeometryMeshTriangleCount(mesh);
    
    if (vertexCount > 0) {
        TG_GeometryVertex* vertices = TG_GetGeometryMeshVertices(mesh);
        localMesh.vertices.assign(vertices, vertices + vertexCount);
    }
    
    if (triangleCount > 0) {
        TG_UInt3* triangles = TG_GetGeometryMeshTriangles(mesh);
        localMesh.triangles.assign(triangles, triangles + triangleCount);
    }
    
    // Upload and render the mesh
    upload_mesh(localMesh, *(data->gizmoMesh));
    draw_mesh(*(data->wireframeShader), *(data->gizmoMesh), data->camPosition, 
              data->viewProjMatrix, identity4x4);
}

// Helper function to convert a rigid_transform to a 4x4 matrix
linalg::aliases::float4x4 get_transform_matrix(TG_RigidTransform transform)
{
    TG_Float3 position;
    TG_Float4 orientation;
    TG_Float3 scale;
    
    TG_GetRigidTransformPosition(transform, &position);
    TG_GetRigidTransformOrientation(transform, &orientation);
    TG_GetRigidTransformScale(transform, &scale);
    
    // Convert to minalg types for matrix calculation
    minalg::float3 pos = {position.x, position.y, position.z};
    minalg::float4 orient = {orientation.x, orientation.y, orientation.z, orientation.w};
    minalg::float3 scl = {scale.x, scale.y, scale.z};
    
    // Calculate matrix using minalg functions
    minalg::float3 x_axis = qxdir(orient) * scl.x;
    minalg::float3 y_axis = qydir(orient) * scl.y;
    minalg::float3 z_axis = qzdir(orient) * scl.z;
    
    return linalg::aliases::float4x4{
        {x_axis.x, x_axis.y, x_axis.z, 0},
        {y_axis.x, y_axis.y, y_axis.z, 0},
        {z_axis.x, z_axis.y, z_axis.z, 0},
        {pos.x, pos.y, pos.z, 1}
    };
}

std::unique_ptr<Window> win;

int main(int argc, char* argv[])
{
    bool ml = 0, mr = 0, bf = 0, bl = 0, bb = 0, br = 0;

    camera cam = {};
    cam.yfov = 1.0f;
    cam.near_clip = 0.01f;
    cam.far_clip = 32.0f;
    cam.position = {0, 1.5f, 4};

    // Create C API gizmo application state and context
    TG_GizmoApplicationState gizmo_state = {};
    TG_GizmoContext gizmo_ctx = TG_CreateGizmoContext();

    try
    {
        win.reset(new Window(1280, 800, "tiny-gizmo-example-app-c-wrapper"));
        glfwSwapInterval(1);
    }
    catch (const std::exception& e)
    {
        std::cout << "Caught GLFW window exception: " << e.what() << std::endl;
    }

    auto windowSize = win->get_window_size();

    GlShader wireframeShader, litShader;
    GlMesh gizmoEditorMesh, teapotMesh;

    wireframeShader = GlShader(gizmo_vert, gizmo_frag);
    litShader = GlShader(lit_vert, lit_frag);

    geometry_mesh teapot = make_teapot();
    upload_mesh(teapot, teapotMesh);

    // Set up data for the render callback
    struct CallbackData {
        GlMesh* gizmoMesh;
        GlShader* wireframeShader;
        linalg::aliases::float3 camPosition;
        linalg::aliases::float4x4 viewProjMatrix;
    } callbackData;
    
    callbackData.gizmoMesh = &gizmoEditorMesh;
    callbackData.wireframeShader = &wireframeShader;

    // Set render callback for the gizmo context
    TG_SetGizmoContextRenderCallback(gizmo_ctx, render_gizmo_mesh, &callbackData);

    win->on_key = [&](int key, int action, int mods)
    {
        if (key == GLFW_KEY_LEFT_CONTROL) gizmo_state.hotkey_ctrl = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_L) gizmo_state.hotkey_local = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_T) gizmo_state.hotkey_translate = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_R) gizmo_state.hotkey_rotate = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_S) gizmo_state.hotkey_scale = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_W) bf = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_A) bl = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_S) bb = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_D) br = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_ESCAPE) win->close();
    };

    win->on_mouse_button = [&](int button, int action, int mods)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT) gizmo_state.mouse_left = (action != GLFW_RELEASE);
        if (button == GLFW_MOUSE_BUTTON_LEFT) ml = (action != GLFW_RELEASE);
        if (button == GLFW_MOUSE_BUTTON_RIGHT) mr = (action != GLFW_RELEASE);
    };

    minalg::float2 lastCursor;
    win->on_cursor_pos = [&](linalg::aliases::float2 position)
    {
        auto deltaCursorMotion = minalg::float2(position.x, position.y) - lastCursor;
        if (mr)
        {
            cam.yaw -= deltaCursorMotion.x * 0.01f;
            cam.pitch -= deltaCursorMotion.y * 0.01f;
        }
        lastCursor = minalg::float2(position.x, position.y);
    };

    // Create rigid transforms using the C API
    TG_RigidTransform xform_a = TG_CreateRigidTransform();
    TG_Float3 position_a = {-2, 0, 0};
    TG_SetRigidTransformPosition(xform_a, &position_a);
    
    TG_RigidTransform xform_b = TG_CreateRigidTransform();
    TG_Float3 position_b = {2, 0, 0};
    TG_SetRigidTransformPosition(xform_b, &position_b);

    // For tracking changes
    TG_Float3 last_position_a = position_a;
    TG_Float4 last_orientation_a = {0, 0, 0, 1};
    TG_Float3 last_scale_a = {1, 1, 1};

    auto t0 = std::chrono::high_resolution_clock::now();
    while (!win->should_close())
    {
        glfwPollEvents();

        auto t1 = std::chrono::high_resolution_clock::now();
        float timestep = std::chrono::duration<float>(t1 - t0).count();
        t0 = t1;

        if (mr)
        {
            const linalg::aliases::float4 orientation = cam.get_orientation();
            linalg::aliases::float3 move;
            if (bf) move -= qzdir(orientation);
            if (bl) move -= qxdir(orientation);
            if (bb) move += qzdir(orientation);
            if (br) move += qxdir(orientation);
            if (length2(move) > 0) cam.position += normalize(move) * (timestep * 10);
        }

        glViewport(0, 0, windowSize.x, windowSize.y);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.725f, 0.725f, 0.725f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto cameraOrientation = cam.get_orientation();

        const auto rayDir = get_ray_from_pixel({lastCursor.x, lastCursor.y}, {0, 0, windowSize.x, windowSize.y}, cam).direction;

        // Update gizmo state from app parameters using C API types
        gizmo_state.viewport_size = {(float)windowSize.x, (float)windowSize.y};
        gizmo_state.cam.near_clip = cam.near_clip;
        gizmo_state.cam.far_clip = cam.far_clip;
        gizmo_state.cam.yfov = cam.yfov;
        gizmo_state.cam.position = {cam.position.x, cam.position.y, cam.position.z};
        gizmo_state.cam.orientation = {cameraOrientation.x, cameraOrientation.y, cameraOrientation.z, cameraOrientation.w};
        gizmo_state.ray_origin = {cam.position.x, cam.position.y, cam.position.z};
        gizmo_state.ray_direction = {rayDir.x, rayDir.y, rayDir.z};
        //gizmo_state.screenspace_scale = 80.f; // optional flag to draw the gizmos at a constant screen-space scale

        // Update callback data for this frame
        callbackData.camPosition = cam.position;
        callbackData.viewProjMatrix = cam.get_viewproj_matrix((float)windowSize.x / (float)windowSize.y);

        glDisable(GL_CULL_FACE);
        
        // Get matrix from transform A and render teapot
        auto teapotModelMatrix_a = get_transform_matrix(xform_a);
        draw_lit_mesh(litShader, teapotMesh, cam.position, 
                      cam.get_viewproj_matrix((float)windowSize.x / (float)windowSize.y), 
                      teapotModelMatrix_a);

        // Get matrix from transform B and render teapot
        auto teapotModelMatrix_b = get_transform_matrix(xform_b);
        draw_lit_mesh(litShader, teapotMesh, cam.position, 
                      cam.get_viewproj_matrix((float)windowSize.x / (float)windowSize.y), 
                      teapotModelMatrix_b);

        glClear(GL_DEPTH_BUFFER_BIT);

        // Update gizmo context with current application state
        TG_UpdateGizmoContext(gizmo_ctx, &gizmo_state);

        // Interact with gizmo A
        if (TG_TransformGizmo(gizmo_ctx, "first-example-gizmo", xform_a))
        {
            std::cout << get_local_time_ns() << " - " << "First Gizmo Hovered..." << std::endl;
            
            // Check if the transform has changed
            TG_Float3 current_position;
            TG_Float4 current_orientation;
            TG_Float3 current_scale;
            
            TG_GetRigidTransformPosition(xform_a, &current_position);
            TG_GetRigidTransformOrientation(xform_a, &current_orientation);
            TG_GetRigidTransformScale(xform_a, &current_scale);
            
            bool changed = false;
            changed |= (current_position.x != last_position_a.x || 
                        current_position.y != last_position_a.y || 
                        current_position.z != last_position_a.z);
                        
            changed |= (current_orientation.x != last_orientation_a.x || 
                        current_orientation.y != last_orientation_a.y || 
                        current_orientation.z != last_orientation_a.z ||
                        current_orientation.w != last_orientation_a.w);
                        
            changed |= (current_scale.x != last_scale_a.x || 
                        current_scale.y != last_scale_a.y || 
                        current_scale.z != last_scale_a.z);
                        
            if (changed) {
                std::cout << get_local_time_ns() << " - " << "First Gizmo Changed..." << std::endl;
                
                // Update last values
                last_position_a = current_position;
                last_orientation_a = current_orientation;
                last_scale_a = current_scale;
            }
        }

        // Interact with gizmo B
        TG_TransformGizmo(gizmo_ctx, "second-example-gizmo", xform_b);
        
        // Draw gizmos
        TG_DrawGizmoContext(gizmo_ctx);

        gl_check_error(__FILE__, __LINE__);

        win->swap_buffers();
    }
    
    // Clean up resources
    TG_DestroyRigidTransform(xform_a);
    TG_DestroyRigidTransform(xform_b);
    TG_DestroyGizmoContext(gizmo_ctx);
    
    return EXIT_SUCCESS;
}