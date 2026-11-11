# iac dev commands. Recipes are a line or two of cmake; anything longer is a
# Node script in scripts/.

set windows-shell := ["cmd.exe", "/c"]

build_dir := "build"
build_type := "Release"
prefix := home_directory() / ".local"

# Show the recipe list by default.
default:
    @just --list

[doc('Configure + build iac')]
build:
    cmake -B {{build_dir}} -S . -DCMAKE_BUILD_TYPE={{build_type}}
    cmake --build {{build_dir}} --config {{build_type}} --target iac -j

[doc('Build, then install iac into <prefix>/bin (default ~/.local/bin)')]
install prefix=prefix: build
    cmake --install {{build_dir}} --config {{build_type}} --prefix "{{prefix}}"

[doc('clang-format every tracked source in place')]
[unix]
format:
    git ls-files '*.h' '*.cpp' '*.mm' | xargs clang-format -i
