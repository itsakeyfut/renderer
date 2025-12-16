# 自作レンダラー

Vulkanベースの自作レンダラー

## 必要環境

- CMake 3.20以上
- C++20対応コンパイラ（MSVC, GCC, Clang）

## ビルド

```bash
# 構成（Releaseビルド）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# ビルド
cmake --build build
```

## テスト実行

```bash
# 全テスト実行
ctest --test-dir build

# 詳細出力
ctest --test-dir build -V

# 失敗時のみ出力
ctest --test-dir build --output-on-failure

# 名前でフィルタ
ctest --test-dir build -R LogTest
```
