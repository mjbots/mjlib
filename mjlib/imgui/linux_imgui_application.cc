// Copyright 2020 Josh Pieper, jjp@pobox.com.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mjlib/imgui/imgui_application.h"

#include <GL/gl3w.h>

// GL must be included *before* we include glfw
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_opengl3.h>

#include <fmt/format.h>

#include "mjlib/base/fail.h"
#include "mjlib/imgui/gl.h"

namespace mjlib {
namespace imgui {

namespace {
void glfw_error_callback(int error, const char* description) {
  mjlib::base::Fail(fmt::format("glfw error {}: {}\n", error, description));
}
}

class ImguiApplication::Impl {
 public:
  Impl(const Options& options) {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
      mjlib::base::Fail("glfw init failed");
    }

    // GL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, options.gl_version_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, options.gl_version_minor);

    window_ = glfwCreateWindow(options.width, options.height,
                               options.title.c_str(), nullptr, nullptr);

    if (window_ == nullptr) {
      mjlib::base::Fail("Could not create glfw window");
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    const bool err = gl3wInit() != 0;
    if (err) {
      mjlib::base::Fail("Could not initialize gl3w");
    }

    MJ_TRACE_GL_ERROR();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    if (!options.persist_settings) {
      io.IniFilename = nullptr;
    }

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(options.glsl_version.c_str());
  }

  ~Impl() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window_);
    glfwTerminate();
  }

  GLFWwindow* window_ = nullptr;
  Eigen::Vector4f clear_color_{0.45f, 0.55f, 0.60f, 1.0f};
};

ImguiApplication::ImguiApplication(const Options& options)
    : impl_(std::make_unique<Impl>(options)) {}

ImguiApplication::~ImguiApplication() {}

void ImguiApplication::PollEvents() {
  MJ_TRACE_GL_ERROR();
  glfwPollEvents();
  MJ_TRACE_GL_ERROR();
}

void ImguiApplication::SwapBuffers() {
  MJ_TRACE_GL_ERROR();
  glfwSwapBuffers(impl_->window_);
  MJ_TRACE_GL_ERROR();
}

void ImguiApplication::NewFrame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  glClearColor(impl_->clear_color_(0),
               impl_->clear_color_(1),
               impl_->clear_color_(2),
               impl_->clear_color_(3));
  glClear(GL_COLOR_BUFFER_BIT);
}

void ImguiApplication::Render() {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool ImguiApplication::should_close() {
  return glfwWindowShouldClose(impl_->window_);
}

void ImguiApplication::set_pos(const Eigen::Vector2i& pos) {
  glfwSetWindowPos(impl_->window_, pos.x(), pos.y());
}

void ImguiApplication::set_clear_color(const Eigen::Vector4f& color) {
  impl_->clear_color_ = color;
}

Eigen::Vector2i ImguiApplication::pos() {
  int result_w = 0;
  int result_h = 0;
  glfwGetWindowPos(impl_->window_, &result_w, &result_h);
  return { result_w, result_h };
}

Eigen::Vector2i ImguiApplication::size() {
  int result_w = 0;
  int result_h = 0;
  glfwGetWindowSize(impl_->window_, &result_w, &result_h);
  return { result_w, result_h };
}

Eigen::Vector2i ImguiApplication::framebuffer_size() {
  int result_w = 0;
  int result_h = 0;
  glfwGetFramebufferSize(impl_->window_, &result_w, &result_h);
  return { result_w, result_h };
}

}
}
