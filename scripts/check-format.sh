#!/usr/bin/env bash
set -euo pipefail

project_root="$(git -C "$(dirname -- "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"

mapfile -d '' source_files < <(
    git -C "$project_root" ls-files -z -- \
        '*.c' '*.cc' '*.cpp' '*.cxx' \
        '*.h' '*.hh' '*.hpp' '*.ipp' '*.inl'
)
existing_source_files=()
for file in "${source_files[@]}"; do
    [[ -f "$project_root/$file" ]] && existing_source_files+=("$project_root/$file")
done
clang-format --dry-run --Werror "${existing_source_files[@]}"

while IFS= read -r -d '' file; do
    cmake-format --check "$project_root/$file"
done < <(
    git -C "$project_root" ls-files -z -- \
        'CMakeLists.txt' 'CMakeLists.txt.in' '*.cmake' '*.cmake.in'
)

git -C "$project_root" diff --check
