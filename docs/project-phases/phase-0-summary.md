# Phase 0: VSCode + FleetManager + Ubuntu Setup Guide for the GA_3DBPP Project

**Environment:** Remote Ubuntu 24.04 VM accessed via FleetManager (VSCode remote dev container extension). GCC 13.3.0 compiling natively on Linux. No MSYS2, no MinGW, no Windows PATH configuration needed.

---

## Step 1: Connect to the remote VM and verify GCC

Open VSCode, connect to your remote Ubuntu VM via FleetManager. Open the integrated terminal (`Ctrl+` `` ` ``). Verify GCC is present and supports C++20:

```bash
g++ --version
g++ -std=c++20 -x c++ -E /dev/null -o /dev/null && echo "C++20 supported" || echo "C++20 NOT supported"
```

Expected output: GCC 13.3.0 (or newer) and "C++20 supported".

---

## Step 2: Install build tools via apt

```bash
sudo apt update
sudo apt install -y cmake ninja-build gdb git curl zip unzip tar pkg-config
```

Verify:

```bash
cmake --version   # should be 3.28+
ninja --version   # should be 1.11+
gdb --version     # should be 15.0+
```

The `falcon-sensor.conf` error at the end of apt output is harmless — it's CrowdStrike endpoint security on your work machine and does not affect any tools.

---

## Step 3: Install vcpkg and project libraries

```bash
cd ~
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

Make vcpkg available in every future terminal session:

```bash
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.bashrc
echo 'export PATH="$VCPKG_ROOT:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

Install the three project libraries (vcpkg auto-detects the `x64-linux` triplet on Ubuntu — no manual triplet configuration needed):

```bash
vcpkg install nlohmann-json gtest spdlog
```

Verify all packages installed:

```bash
vcpkg list
```

Expected: `nlohmann-json:x64-linux`, `gtest:x64-linux`, `spdlog:x64-linux` (plus `fmt` and `vcpkg-cmake` as transitive dependencies).

---

## Step 4: Install VSCode extensions

In VSCode, go to Extensions (`Ctrl+Shift+X`) and install:

1. **C/C++** by Microsoft (provides IntelliSense, debugging)
2. **CMake** by twxs (syntax highlighting for CMakeLists.txt)
3. **CMake Tools** by Microsoft (build/debug/test integration)

Restart VSCode after installing CMake Tools.

---

## Step 5: Create the project and directory structure

```bash
cd ~/repos
mkdir -p GA_3DBPP/.vscode
cd GA_3DBPP
mkdir -p src test data docs output visualization build
```

---

## Step 6: Create VSCode configuration files

These files live inside the project and configure CMake, IntelliSense, and debugging only for this workspace. Create all four — if you skip `c_cpp_properties.json`, VSCode auto-generates it with wrong defaults (`gcc` instead of `g++`, `gnu++17` instead of `c++20`, and no vcpkg include path), which causes red squiggles on valid code.

```bash
cat > .vscode/settings.json << 'EOF'
{
    "cmake.generator": "Ninja",
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.configureSettings": {
        "CMAKE_TOOLCHAIN_FILE": "${env:HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    },
    "C_Cpp.default.compilerPath": "/usr/bin/g++",
    "C_Cpp.default.cppStandard": "c++20",
    "C_Cpp.default.intelliSenseMode": "gcc-x64"
}
EOF
```

```bash
cat > .vscode/cmake-kits.json << 'EOF'
[
    {
        "name": "GCC (System)",
        "compilers": {
            "C": "/usr/bin/gcc",
            "CXX": "/usr/bin/g++"
        },
        "preferredGenerator": {
            "name": "Ninja"
        }
    }
]
EOF
```

```bash
cat > .vscode/launch.json << 'EOF'
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug GA_3DBPP",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/GA_3DBPP",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "MIMode": "gdb",
            "setupCommands": [
                {
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ]
        },
        {
            "name": "Debug Tests",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/GA_3DBPPTests",
            "args": ["--gtest_color=yes"],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "MIMode": "gdb",
            "setupCommands": [
                {
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ]
        }
    ]
}
EOF
```

```bash
cat > .vscode/c_cpp_properties.json << 'EOF'
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/**",
                "${env:HOME}/vcpkg/installed/x64-linux/include"
            ],
            "defines": [],
            "compilerPath": "/usr/bin/g++",
            "cStandard": "c17",
            "cppStandard": "c++20",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ],
    "version": 4
}
EOF
```

The `includePath` entry for vcpkg is required so IntelliSense can find headers like `gtest/gtest.h` and `nlohmann/json.hpp`. Without it, IntelliSense shows red squiggles even though the code compiles correctly.

---

## Step 7: Create CMakeLists.txt

```bash
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.20)
project(GA_3DBPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG QUIET)

add_executable(GA_3DBPP
    src/main.cpp
)

target_include_directories(GA_3DBPP PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(GA_3DBPP PRIVATE
    nlohmann_json::nlohmann_json
)

if(spdlog_FOUND)
    target_link_libraries(GA_3DBPP PRIVATE spdlog::spdlog)
endif()

enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(GA_3DBPPTests
    test/test_main.cpp
)

target_include_directories(GA_3DBPPTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(GA_3DBPPTests PRIVATE
    GTest::gtest
    GTest::gtest_main
    nlohmann_json::nlohmann_json
)

if(spdlog_FOUND)
    target_link_libraries(GA_3DBPPTests PRIVATE spdlog::spdlog)
endif()

include(GoogleTest)
gtest_discover_tests(GA_3DBPPTests)
EOF
```

As you add new source files during development, add them to the `add_executable()` calls. For example:

```cmake
add_executable(GA_3DBPP
    src/main.cpp
    src/CSVReader.cpp
    src/SupportChecker.cpp
)
```

Header-only files (like `AABB.h` or `Config.h`) do NOT need to be listed — they're found automatically through `#include`.

---

## Step 8: Create starter source files

```bash
cat > src/main.cpp << 'EOF'
#include <iostream>
#include <nlohmann/json.hpp>

int main() {
    std::cout << "GA_3DBPP v0.1 — build successful!\n";

    nlohmann::json j;
    j["project"] = "GA_3DBPP";
    j["status"] = "compiles";
    std::cout << "JSON test: " << j.dump(2) << "\n";

    return 0;
}
EOF
```

```bash
cat > test/test_main.cpp << 'EOF'
#include <gtest/gtest.h>

TEST(SmokeTest, TrueIsTrue) {
    EXPECT_TRUE(true);
}

TEST(SmokeTest, OnePlusOneIsTwo) {
    EXPECT_EQ(1 + 1, 2);
}
EOF
```

---

## Step 9: Configure, build, and verify

```bash
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/GA_3DBPP
cd build && ctest --output-on-failure && cd ..
```

Expected output:

1. CMake configures without errors, finds GCC 13.3.0, nlohmann_json, GTest
2. Ninja builds both targets (GA_3DBPP and GA_3DBPPTests)
3. Main program prints "GA_3DBPP v0.1 — build successful!" and JSON output
4. Both smoke tests pass (2/2, 100%)

---

## Step 10: Open in VSCode and verify editor integration

Open the project folder in VSCode: File → Open Folder → `~/repos/GA_3DBPP`

Then:

1. `Ctrl+Shift+P` → **"CMake: Select a Kit"** → choose **"GCC (System)"**
2. `Ctrl+Shift+P` → **"CMake: Select Variant"** → choose **"Debug"**
3. Press **F7** to build (should succeed with same output as terminal build)
4. Open the Testing panel (flask icon on left sidebar) — both smoke tests should appear and pass when you click the play button

---

## Step 11: Verify debugging works

1. Open `src/main.cpp` in the editor
2. Click in the left margin of the `std::cout` line to set a red breakpoint dot
3. Press **F5** (or `Ctrl+Shift+P` → "Debug: Start Debugging")
4. Select **"Debug GA_3DBPP"** from the dropdown
5. Execution should pause at your breakpoint; inspect variables in the left panel

---

## Step 12: Switch between Debug and Release builds

For normal development, use **Debug** (set in Step 10). For performance benchmarking:

```bash
# From terminal (one-time setup):
cmake -B build-release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build-release
```

Or from VSCode: `Ctrl+Shift+P` → "CMake: Select Variant" → **"Release"** → F7

Release builds use `-O2` optimization and are 5–20x faster than Debug. Switch back to Debug for development.

---

## Final directory structure

```
~/repos/GA_3DBPP/
├── .vscode/
│   ├── settings.json        # CMake + IntelliSense config
│   ├── cmake-kits.json      # Compiler kit definition
│   └── launch.json          # GDB debug configurations
├── CMakeLists.txt            # Build configuration
├── src/
│   └── main.cpp              # Entry point (starter)
├── test/
│   └── test_main.cpp         # Smoke tests (starter)
├── data/                     # Input CSVs + BR benchmarks
├── docs/                     # Project documentation
├── output/                   # Algorithm output JSON files
├── visualization/            # Three.js viewer (HTML/JS/CSS)
└── build/                    # CMake build directory (generated)
```

---

## Daily development workflow

1. **Edit code** in VSCode with full IntelliSense
2. **Build** with F7
3. **Run** with `Ctrl+Shift+P` → "CMake: Run Without Debugging"
4. **Debug** with F5 (breakpoints, variable inspection)
5. **Test** via the Testing panel (flask icon) or `Ctrl+Shift+P` → "CMake: Run Tests"

---

## Troubleshooting

**IntelliSense red squiggles but code compiles fine:** The most common cause is a missing or auto-generated `c_cpp_properties.json`. Check that `.vscode/c_cpp_properties.json` exists and contains `"compilerPath": "/usr/bin/g++"`, `"cppStandard": "c++20"`, and `"${env:HOME}/vcpkg/installed/x64-linux/include"` in the `includePath` array (see Step 6 for the full file). After saving the file, press `Ctrl+Shift+P` → "C/C++: Reset IntelliSense Database" to force a rescan.

**"Cannot find GTest/nlohmann_json" during CMake configure:** Verify `VCPKG_ROOT` is set: run `echo $VCPKG_ROOT` in the terminal (should print `~/vcpkg`). If blank, run `source ~/.bashrc`.

**Debugger doesn't stop at breakpoints:** Verify you're building in Debug variant, not Release. Check with `Ctrl+Shift+P` → "CMake: Select Variant".

**Tests don't appear in Testing panel:** Make sure CMake Tools extension is installed and the project is configured (`Ctrl+Shift+P` → "CMake: Configure").