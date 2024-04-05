#pragma once

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <functional>
#include <imgui.h>
#include <latch>
#include <mutex>
#include <thread>
#include <vector>
#include <windows.h>

class imgui_overlay {
public:
  imgui_overlay(HWND target_hwnd)
      : _target_hwnd(target_hwnd) {}

  bool initialize() noexcept {
    std::latch latch(1);
    bool initialized = false;
    _overlay_thread = std::jthread([&]() {
      if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        latch.count_down();
        return;
      }

      SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

      Uint32 flags =
          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
          SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS |
          SDL_WINDOW_TRANSPARENT | SDL_WINDOW_ALWAYS_ON_TOP;

      RECT target_rect;
      GetWindowRect(_target_hwnd, &target_rect);
      auto width = target_rect.right - target_rect.left;
      auto height = (target_rect.bottom - target_rect.top);

      _overlay_window = SDL_CreateWindow(
          "Dear ImGui SDL3+SDL_Renderer example", width,
          height, flags);

      if (_overlay_window == nullptr) {
        latch.count_down();
        return;
      }

      HWND overlay_hwnd = (HWND)SDL_GetProperty(
          SDL_GetWindowProperties(_overlay_window),
          SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

      SetWindowLong(
          overlay_hwnd, GWL_EXSTYLE,
          GetWindowLong(overlay_hwnd, GWL_EXSTYLE) |
              WS_EX_LAYERED);

      SetLayeredWindowAttributes(overlay_hwnd, RGB(0, 0, 0),
                                 0, LWA_COLORKEY);

      _overlay_renderer =
          SDL_CreateRenderer(_overlay_window, nullptr,
                             SDL_RENDERER_PRESENTVSYNC);

      if (_overlay_renderer == nullptr) {
        latch.count_down();
        return;
      }

      SDL_SetRenderDrawBlendMode(_overlay_renderer,
                                 SDL_BLENDMODE_BLEND);

      SDL_SetWindowPosition(_overlay_window,
                            target_rect.left,
                            target_rect.top);

      SDL_ShowWindow(_overlay_window);

      SDL_GL_SetSwapInterval(1);

      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      ImGuiIO &io = ImGui::GetIO();
      (void)io;
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

      // Setup Dear ImGui style
      ImGui::StyleColorsDark();

      // Setup Platform/Renderer backends
      if (!ImGui_ImplSDL3_InitForSDLRenderer(
              _overlay_window, _overlay_renderer)) {
        latch.count_down();
        return;
      }

      if (!ImGui_ImplSDLRenderer3_Init(_overlay_renderer)) {
        latch.count_down();
        return;
      }

      initialized = true;
      _running = true;
      latch.count_down();

      while (_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
          ImGui_ImplSDL3_ProcessEvent(&event);
          if (event.type == SDL_EVENT_QUIT)
            _running = false;
          if (event.type ==
                  SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
              event.window.windowID ==
                  SDL_GetWindowID(_overlay_window))
            _running = false;
        }

        // Resize overlay window to match game window
        GetWindowRect(_target_hwnd, &target_rect);
        width = target_rect.right - target_rect.left;
        height = target_rect.bottom - target_rect.top;

        SDL_SetWindowPosition(_overlay_window,
                              target_rect.left,
                              target_rect.top);

        SDL_SetWindowSize(_overlay_window, width, height);

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        std::vector<std::function<void()>> callbacks;
        {
          auto lock = std::lock_guard(_overlay_mutex);

          callbacks = _draw_callbacks;
        }

        for (const auto &callback : callbacks) {
          callback();
        }

        ImGui::Render();

        SDL_SetRenderDrawColor(_overlay_renderer, 0, 0, 0,
                               0);
        SDL_RenderClear(_overlay_renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(
            ImGui::GetDrawData());
        SDL_RenderPresent(_overlay_renderer);

        SDL_Delay(16);
      }

      ImGui_ImplSDLRenderer3_Shutdown();
      ImGui_ImplSDL3_Shutdown();
      ImGui::DestroyContext();

      SDL_DestroyRenderer(_overlay_renderer);
      SDL_DestroyWindow(_overlay_window);
      SDL_Quit();
    });

    latch.wait();
    return initialized;
  }


  void register_callback(std::function<void()> callback) {
    auto lock = std::lock_guard(_overlay_mutex);
    _draw_callbacks.emplace_back(callback);
  }

  bool is_running() const noexcept {
    return _running;
  }

private:
  HWND _target_hwnd;
  std::jthread _overlay_thread;
  SDL_Window *_overlay_window;
  SDL_Renderer *_overlay_renderer;
  std::mutex _overlay_mutex;
  std::vector<std::function<void()>> _draw_callbacks;
  std::atomic_bool _running = false;
};