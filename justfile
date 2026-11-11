# iac dev commands. Recipes are a line or two of cmake; anything longer is a
# Node script in scripts/.

set windows-shell := ["cmd.exe", "/c"]

build_dir := "build"
build_type := "Release"
prefix := home_directory() / ".local"
binary := if os() == "windows" { build_dir + "\\iac.exe" } else { build_dir + "/iac" }

# Ninja drives cl directly on Windows, so it needs the Visual Studio environment.
msvc := if os() == "windows" { "powershell -NoProfile -ExecutionPolicy Bypass -File scripts/with-msvc.ps1" } else { "" }

# Show the recipe list by default.
default:
    @just --list

[doc('Configure + build iac; extra arguments go to the configure step')]
build *cmake_args:
    {{msvc}} cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE={{build_type}} {{cmake_args}}
    {{msvc}} cmake --build {{build_dir}} --target iac

[doc('Build everything, then run every test')]
test: build
    {{msvc}} cmake --build {{build_dir}}
    ctest --test-dir {{build_dir}} --output-on-failure

[doc('Build, then run iac with the given arguments')]
run *args: build
    {{binary}} {{args}}

[doc('Build, then install iac into <prefix>/bin (default ~/.local/bin)')]
install prefix=prefix: build
    cmake --install {{build_dir}} --prefix "{{prefix}}"

[doc('Move branch-pinned dependencies to their branch heads and rewrite lockstep.lock')]
update-deps:
    {{msvc}} cmake -S . -B {{build_dir}} -G Ninja -DLOCKSTEP_MODE=update
    {{msvc}} cmake -S . -B {{build_dir}} -G Ninja -DLOCKSTEP_MODE=locked

[doc('clang-format every tracked source in place')]
[unix]
format:
    git ls-files '*.h' '*.cpp' '*.mm' | xargs clang-format -i

[doc('Delete the build directory')]
clean:
    cmake -E rm -rf {{build_dir}}
