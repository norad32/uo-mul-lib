#!/usr/bin/env bash
set -euo pipefail

project_root="$(git -C "$(dirname -- "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
mapfile -d '' project_files < <(git -C "$project_root" ls-files --cached --others --exclude-standard -z)
cmake_files=()

for file in "${project_files[@]}"; do
    if git -C "$project_root" check-ignore --no-index -q -- "$file"; then
        continue
    fi

    case "${file##*/}" in
        CMakeLists.txt|CMakeLists.txt.in|*.cmake|*.cmake.in)
            if [[ -f "$project_root/$file" ]]; then
                cmake_files+=("$project_root/$file")
            fi
            ;;
    esac
done

for file in "${cmake_files[@]}"; do
    relative_file="${file#"$project_root"/}"
    printf 'Formatting %s... ' "$relative_file"
    if cmake-format --check "$file" >/dev/null 2>&1; then
        printf '%s\n' "already formatted"
    else
        cmake-format -i "$file"
        printf '%s\n' "formatted"
    fi
done
