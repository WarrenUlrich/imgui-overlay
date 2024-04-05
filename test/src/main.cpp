#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>

#include <imgui_overlay.hpp>

DWORD get_process_id_by_name(
    const std::string &process_name) {
  DWORD process_id = 0;
  HANDLE snapshot =
      CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

  if (snapshot != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32 process_entry;
    process_entry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &process_entry)) {
      do {
        std::string current_process_name =
            process_entry.szExeFile;
        if (current_process_name == process_name) {
          process_id = process_entry.th32ProcessID;
          break;
        }
      } while (Process32Next(snapshot, &process_entry));
    }

    CloseHandle(snapshot);
  }

  return process_id;
}

std::vector<HWND> get_process_windows(DWORD process_id) {
  struct search_data {
    DWORD process_id;
    std::vector<HWND> windows;
  } data = {process_id, {}};

  EnumWindows(
      [](HWND hwnd, LPARAM lparam) -> BOOL CALLBACK {
        search_data *data =
            reinterpret_cast<search_data *>(lparam);
        DWORD window_process_id;
        GetWindowThreadProcessId(hwnd, &window_process_id);
        if (window_process_id == data->process_id) {
          data->windows.emplace_back(hwnd);
        }
        return TRUE;
      },
      (LPARAM)&data);

  return data.windows;
}

std::vector<HWND> get_child_windows(HWND parent) {
  struct search_data {
    std::vector<HWND> windows;
  } data = {{}};

  EnumChildWindows(
      parent,
      [](HWND hwnd, LPARAM lparam) -> BOOL CALLBACK {
        search_data *data =
            reinterpret_cast<search_data *>(lparam);
        data->windows.emplace_back(hwnd);
        return TRUE;
      },
      (LPARAM)&data);

  return data.windows;
}

int main(int argc, char **args) {
  DWORD process_id =
      get_process_id_by_name("JagexLauncher.exe");
  if (process_id != 0) {
    std::cout << "Process ID: " << process_id << std::endl;
  } else {
    std::cout << "Process not found" << std::endl;
  }

  std::vector<HWND> windows =
      get_process_windows(process_id);
  HWND found = NULL;
  for (auto window : get_process_windows(process_id)) {
    char window_name[256];
    GetWindowTextA(window, window_name, 256);
    if (strcmp(window_name, "Old School RuneScape") != 0)
      continue;

    for (auto child : get_child_windows(window)) {
      char class_name[256];
      GetClassNameA(child, class_name, 256);
      if (strcmp(class_name, "SunAwtCanvas") != 0) {
        continue;
      }

      found = child;
      break;
    }
  }

  if (found == NULL) {
    std::cout << "Window not found.\n";
    return 1;
  }

  std::cout << "Window found: " << found << std::endl;

  // auto overlay = imgui_overlay(found);
  // overlay.register_callback([]() {
  //   ImGui::Begin("Hello, world!");
  //   ImGui::Text("This is some useful text.");
  //   ImGui::End();
  // });

  // if (!overlay.initialize()) {
  //   std::cout << "Failed to initialize overlay.\n";
  //   return 1;
  // }

  // while(overlay.is_running()) {
  //   Sleep(16);
  // }
  
  return 0;
}