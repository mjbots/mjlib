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

#pragma once

#include <memory>

#include <Eigen/Core>

namespace mjlib {
namespace imgui {

/// This implements a platform agnostic imgui setup.  On linux it uses
/// opengl, and on Windows it uses DirectX 12.
class ImguiApplication {
 public:
  struct Options {
    bool persist_settings = true;

    int width = 0;
    int height = 0;
    std::string title;

    // GL specific options.
    int gl_version_major = 4;
    int gl_version_minor = 0;
    std::string glsl_version = "#version 400";

    Options() {}
  };

  ImguiApplication(const Options&);
  ~ImguiApplication();

  void PollEvents();
  void SwapBuffers();
  void NewFrame();
  void Render();

  bool should_close();

  void set_pos(const Eigen::Vector2i&);
  void set_clear_color(const Eigen::Vector4f&);

  Eigen::Vector2i pos();
  Eigen::Vector2i size();
  Eigen::Vector2i framebuffer_size();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}
}
