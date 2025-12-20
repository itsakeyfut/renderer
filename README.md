# 自作レンダラー

Vulkanベースの自作レンダラー

## 成果物
### Phase 1
#### Hello Triangle
![Hello Triangle](screenshots/phase-1/hello_triangle.png)

### Phase 2
#### Mario Mario
![Mario Mario](screenshots/phase-2/mario.png)

#### Rabbit Maid
![Rabbit Maid](screenshots/phase-2/rabbit_maid.png)

### Bunny
![Bunny](screenshots/phase-2/bunny.png)

### Neko Mimi
![Neko Mimi](screenshots/phase-2/nekomimi.png)

#### Butterfly Areana Of Valor
![Butterfly Areana Of Valor](screenshots/phase-2/butterfly_arena_of_valor.png)

#### A Contortionist Dancer
![A Contortionist Dancer](screenshots/phase-2/dancer.png)

#### Fortnite Back to The Future Time Machine
![Fortnite Back to The Future Time Machine](screenshots/phase-2/fortnite_back_to_the_future_time_machine.png)

### Phasee 3
#### Mario Mario
![Mario Mario](screenshots/phase-3/mario.png)

#### Rabbit Maid
![Rabbit Maid](screenshots/phase-3/rabbit_maid.png)

### Bunny
![Bunny](screenshots/phase-3/bunny.png)

### Neko Mimi
![Neko Mimi](screenshots/phase-3/nekomimi.png)

#### Butterfly Areana Of Valor
![Butterfly Areana Of Valor](screenshots/phase-3/butterfly_arena_of_valor.png)

#### A Contortionist Dancer
![A Contortionist Dancer](screenshots/phase-3/dancer.png)

#### Fortnite Back to The Future Time Machine
![Fortnite Back to The Future Time Machine](screenshots/phase-3/fortnite_back_to_the_future_time_machine.png)

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

## アプリケーション実行

```bash
# Windowsの場合
./build/VulkanRenderer.exe

# または
build/VulkanRenderer.exe
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
