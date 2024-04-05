#pragma once

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <functional>
#include <imgui.h>
#include <latch>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>
#include <windows.h>

class imgui_overlay {
public:
  imgui_overlay() = delete;

  imgui_overlay(const imgui_overlay &) = delete;

  imgui_overlay(imgui_overlay &&) = delete;

  ~imgui_overlay() {
    if (!_running)
      return;
    
    _running = false;
    if (_overlay_thread.joinable())
      _overlay_thread.join();
  }

  void register_callback(std::function<void()> callback) {
    auto lock = std::lock_guard(_overlay_mutex);
    _draw_callbacks.emplace_back(callback);
  }

  bool is_running() const noexcept { return _running; }

  bool stop() {
    if (!_running)
      return false;

    _running = false;
    if (_overlay_thread.joinable())
      _overlay_thread.join();

    return true;
  }

  static std::unique_ptr<imgui_overlay>
  attach_to_hwnd(HWND hwnd) {
    std::latch latch(1);
    bool initialized = false;
    auto overlay = std::unique_ptr<imgui_overlay>(
        new imgui_overlay(hwnd));

    overlay
        ->_overlay_thread = std::jthread([_overlay = overlay.get(), &latch,
                                          &initialized]() {
      if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        latch.count_down();
        return;
      }

      SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

      const Uint32 flags =
          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
          SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS |
          SDL_WINDOW_TRANSPARENT;

      RECT target_rect;
      GetWindowRect(_overlay->_target_hwnd, &target_rect);
      auto width = target_rect.right - target_rect.left;
      auto height = (target_rect.bottom - target_rect.top);

      _overlay->_overlay_window = SDL_CreateWindow(
          "Dear ImGui SDL3+SDL_Renderer example", width,
          height, flags);

      if (_overlay->_overlay_window == nullptr) {
        latch.count_down();
        return;
      }

      HWND overlay_hwnd = (HWND)SDL_GetProperty(
          SDL_GetWindowProperties(_overlay->_overlay_window),
          SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

      SetWindowLong(
          overlay_hwnd, GWL_EXSTYLE,
          GetWindowLong(overlay_hwnd, GWL_EXSTYLE) |
              WS_EX_LAYERED);

      SetLayeredWindowAttributes(overlay_hwnd, RGB(0, 0, 0),
                                 0, LWA_COLORKEY);

      SetWindowPos(overlay_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE);

      _overlay->_overlay_renderer = SDL_CreateRenderer(
          _overlay->_overlay_window, nullptr,
          SDL_RENDERER_PRESENTVSYNC);

      if (_overlay->_overlay_renderer == nullptr) {
        latch.count_down();
        return;
      }

      SDL_SetRenderDrawBlendMode(_overlay->_overlay_renderer,
                                 SDL_BLENDMODE_BLEND);

      SDL_SetWindowPosition(_overlay->_overlay_window,
                            target_rect.left,
                            target_rect.top);

      SDL_ShowWindow(_overlay->_overlay_window);

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
              _overlay->_overlay_window,
              _overlay->_overlay_renderer)) {
        latch.count_down();
        return;
      }

      if (!ImGui_ImplSDLRenderer3_Init(
              _overlay->_overlay_renderer)) {
        latch.count_down();
        return;
      }

      initialized = true;
      _overlay->_running = true;
      latch.count_down();
      SDL_SetWindowAlwaysOnTop(
                _overlay->_overlay_window, SDL_TRUE);

      while (_overlay->_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
          ImGui_ImplSDL3_ProcessEvent(&event);
          if (event.type == SDL_EVENT_QUIT)
            _overlay->_running = false;
          if (event.type ==
                  SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
              event.window.windowID ==
                  SDL_GetWindowID(_overlay->_overlay_window))
            _overlay->_running = false;
        }

        // Resize overlay window to match game window
        GetWindowRect(_overlay->_target_hwnd, &target_rect);
        width = target_rect.right - target_rect.left;
        height = target_rect.bottom - target_rect.top;

        SDL_SetWindowPosition(_overlay->_overlay_window,
                              target_rect.left,
                              target_rect.top);

        SDL_SetWindowSize(_overlay->_overlay_window, width,
                          height);

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        std::vector<std::function<void()>> callbacks;
        {
          auto lock =
              std::lock_guard(_overlay->_overlay_mutex);

          callbacks = _overlay->_draw_callbacks;
        }

        for (const auto &callback : callbacks) {
          callback();
        }

        ImGui::Render();

        SDL_SetRenderDrawColor(_overlay->_overlay_renderer,
                               0, 0, 0, 0);
        SDL_RenderClear(_overlay->_overlay_renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(
            ImGui::GetDrawData());
        SDL_RenderPresent(_overlay->_overlay_renderer);

        SDL_Delay(16);
      }

      ImGui_ImplSDLRenderer3_Shutdown();
      ImGui_ImplSDL3_Shutdown();
      ImGui::DestroyContext();

      SDL_DestroyRenderer(_overlay->_overlay_renderer);
      SDL_DestroyWindow(_overlay->_overlay_window);
      SDL_Quit();
    });

    latch.wait();
    return initialized ? std::move(overlay) : nullptr;
  }

  static std::unique_ptr<imgui_overlay>
  attach_to_pid(DWORD pid, std::string_view class_name,
                std::string_view child_class_name = "") {
    struct window_data {
      DWORD pid;
      HWND hwnd;
      std::string_view class_name;
      std::string_view child_class_name;
    } data{.pid = pid,
           .hwnd = NULL,
           .class_name = class_name,
           .child_class_name = child_class_name};

    EnumWindows(
        [](HWND hwnd, LPARAM lparam) -> BOOL CALLBACK {
          window_data *data =
              reinterpret_cast<window_data *>(lparam);

          DWORD pid;
          GetWindowThreadProcessId(hwnd, &pid);
          if (pid != data->pid)
            return TRUE;

          if (data->class_name.empty()) {
            data->hwnd = hwnd;
            return FALSE;
          }

          char class_name[256];
          GetClassNameA(hwnd, class_name, 256);
          if (data->class_name != class_name)
            return TRUE;

          if (data->child_class_name.empty()) {
            data->hwnd = hwnd;
            return FALSE;
          }

          struct search_data {
            std::string_view class_name;
            HWND hwnd;
          } child_data = {.class_name =
                              data->child_class_name,
                          .hwnd = NULL};

          EnumChildWindows(
              hwnd,
              [](HWND hwnd,
                 LPARAM lparam) -> BOOL CALLBACK {
                search_data *data =
                    reinterpret_cast<search_data *>(lparam);
                char class_name[256];
                GetClassNameA(hwnd, class_name, 256);
                if (data->class_name != class_name)
                  return TRUE;
                
                data->hwnd = hwnd;
                return FALSE;
              },
              (LPARAM)&child_data);

          if (child_data.hwnd != NULL) {
            data->hwnd = child_data.hwnd;
            return FALSE;
          }

          return TRUE;
        },
        (LPARAM)&data);

    if (data.hwnd == NULL)
      return nullptr;
    
    char window_name[256];
    GetClassNameA(data.hwnd, window_name, 256);
    if (window_name[0] == '\0')
      return nullptr;
    else 
      std::cout << "Window name: " << window_name << std::endl;
    return attach_to_hwnd(data.hwnd);
  }

private:
  HWND _target_hwnd;
  std::jthread _overlay_thread;
  SDL_Window *_overlay_window;
  SDL_Renderer *_overlay_renderer;
  std::mutex _overlay_mutex;
  std::vector<std::function<void()>> _draw_callbacks;
  std::atomic_bool _running = false;

  imgui_overlay(HWND target_hwnd)
      : _target_hwnd(target_hwnd) {}
};