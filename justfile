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

[doc('Coverage is the default for now: build iac with line-coverage counters and install it into <prefix>/bin (default ~/.local/bin), keeping the old and new programs for coverage-report')]
[unix]
install prefix=prefix: coverage-build
    {{line_coverage}} keep --programs "{{prefix}}/bin"
    cmake --install {{coverage_build_dir}} --prefix "{{prefix}}"
    {{line_coverage}} keep --programs "{{prefix}}/bin"

[doc('Line coverage needs clang, so on Windows install is install-without-coverage')]
[windows]
install prefix=prefix: (install-without-coverage prefix)

[doc('Build, then install the normal iac, without line-coverage counters, into <prefix>/bin (default ~/.local/bin)')]
install-without-coverage prefix=prefix: build
    cmake --install {{build_dir}} --prefix "{{prefix}}"

# ---- Line coverage: which lines of iac's own code agents actually run ----

coverage_build_dir := "build-coverage"
coverage_cmake_arguments := "-DPOND_LINE_COVERAGE=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++"
# Quoted so line-coverage.py expands ~ on each machine it runs on, not the shell here.
line_coverage := coverage_build_dir / "PondLineCoverage/line-coverage.py --app iac --program iac --installed-in '~/.local/bin' --own-code-folder Source --not-own-code-folder Tests"

[doc('Build iac with line-coverage counters, in build-coverage')]
[unix]
coverage-build:
    just build_dir={{coverage_build_dir}} build {{coverage_cmake_arguments}}

[doc('Print the code that never ran in any installed coverage build, largest first, and write an HTML per-line view')]
[unix]
coverage-report *arguments:
    {{line_coverage}} report {{arguments}}

[doc("Copy a host's line-coverage counts and its programs here, for coverage-report")]
[unix]
coverage-collect-from-host host *arguments:
    {{line_coverage}} collect {{host}} {{arguments}}

[doc('Delete the line-coverage counts and kept programs here, or on user@host')]
[unix]
coverage-reset host="" *arguments:
    {{line_coverage}} reset {{host}} {{arguments}}

[doc('List the crash reports nobody has marked fixed here and on other machines (list [HOST ...]), or mark one fixed (mark-fixed HOST ID)')]
crash-reports *arguments:
    python3 {{build_dir}}/PondCrashReports/crash-reports.py --app iac {{arguments}}

[doc('Move branch-pinned dependencies to their branch heads and rewrite lockstep.lock')]
update-deps:
    {{msvc}} cmake -S . -B {{build_dir}} -G Ninja -DLOCKSTEP_MODE=update
    {{msvc}} cmake -S . -B {{build_dir}} -G Ninja -DLOCKSTEP_MODE=locked

[doc("Build everything, then run clang-tidy (eacp's checks) over iac's sources; fails on any finding")]
[unix]
tidy: build
    cmake --build {{build_dir}}
    scripts/tidy.sh {{build_dir}}

[doc('clang-format every tracked source in place')]
[unix]
format:
    git ls-files '*.h' '*.cpp' '*.mm' | xargs clang-format -i

[doc('Delete the build directory')]
clean:
    cmake -E rm -rf {{build_dir}}
