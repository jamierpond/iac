# iac dev command runner. Plain cmake — no wrapper CLI.

set windows-shell := ["cmd.exe", "/c"]

# Show the recipe list by default.
default:
    @just --list

[doc('Configure + build (Release)')]
build:
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build build --config Release -j

[doc('Build + install iac into ~/.local/bin')]
[unix]
install: build
    cmake --install build --config Release --prefix "$HOME/.local"

[doc('Build + install iac into %USERPROFILE%\.local\bin')]
[windows]
install: build
    cmake --install build --config Release --prefix "%USERPROFILE%\.local"

[doc('clang-format all sources in place')]
[unix]
format:
    git ls-files '*.h' '*.cpp' '*.mm' | xargs clang-format -i
