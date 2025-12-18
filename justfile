# Update local main branch
new:
    git checkout main && git fetch && git pull origin main

# Run debug mode
run:
    ./build/VulkanRenderer.exe

# Run release mode
run-release:
    ./build-release/VulkanRenderer.exe

# Build the project (debug)
build:
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

# Build the project (release)
build-release:
    cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release

# Clean build artifacts
clean:
    rm -rf build/*

clean-release:
    rm -rf build-release/*

# Run tests
test:
    ctest --test-dir build --output-on-failure

test-verbose:
    ctest --test-dir build -V

test-filter name:
    ctest --test-dir build -R {{name}} --output-on-failure

# Format source files
format:
    find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
    find tests -name "*.cpp" | xargs clang-format -i

format-check:
    find src -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror
    find tests -name "*.cpp" | xargs clang-format --dry-run --Werror

# Lint source files
lint:
    find src -name "*.cpp" | xargs clang-tidy -- -Isrc
    find tests -name "*.cpp" | xargs clang-tidy -- -Isrc
