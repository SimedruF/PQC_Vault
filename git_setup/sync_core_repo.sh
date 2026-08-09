#!/usr/bin/env bash

set -Eeuo pipefail

readonly SCRIPT_NAME="$(basename -- "$0")"
readonly DEFAULT_TARGET="${HOME:?HOME is not set}/PQCWallet-Core"

APPLY_CHANGES=false
TARGET_INPUT="$DEFAULT_TARGET"

usage() {
    cat <<EOF
Utilizare:
  $SCRIPT_NAME [--target DIRECTOR] [--apply]

Sincronizeaza fisierele publicabile ale PQCWallet intr-un repository Git separat.

Optiuni:
  --target DIRECTOR  Repository-ul destinatie (implicit: $DEFAULT_TARGET)
  --apply            Aplica modificarile. Fara aceasta optiune ruleaza doar simularea.
  -h, --help         Afiseaza acest mesaj.

Exemple:
  $SCRIPT_NAME
  $SCRIPT_NAME --apply
  $SCRIPT_NAME --target /cale/catre/PQCWallet-Core --apply

Scriptul nu executa git add, commit sau push.
EOF
}

fail() {
    printf 'Eroare: %s\n' "$*" >&2
    exit 1
}

while (($# > 0)); do
    case "$1" in
        --apply)
            APPLY_CHANGES=true
            shift
            ;;
        --target)
            (($# >= 2)) || fail "--target necesita un director."
            TARGET_INPUT="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "optiune necunoscuta: $1"
            ;;
    esac
done

for required_command in git realpath rsync; do
    command -v "$required_command" >/dev/null 2>&1 || \
        fail "comanda '$required_command' nu este instalata."
done

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly SOURCE_DIR="$(realpath -- "$SCRIPT_DIR/..")"
readonly TARGET_DIR="$(realpath -m -- "$TARGET_INPUT")"
readonly RESOLVED_HOME="$(realpath -- "${HOME:?HOME is not set}")"

[[ -d "$SOURCE_DIR" ]] || fail "directorul sursa nu exista: $SOURCE_DIR"
[[ -d "$TARGET_DIR" ]] || fail "directorul destinatie nu exista: $TARGET_DIR"
[[ ! -L "$TARGET_INPUT" ]] || fail "destinatia nu poate fi o legatura simbolica: $TARGET_INPUT"
[[ "$TARGET_DIR" != "/" ]] || fail "destinatia nu poate fi directorul radacina."
[[ "$TARGET_DIR" != "$RESOLVED_HOME" ]] || fail "destinatia nu poate fi directorul HOME."
[[ "$TARGET_DIR" != "$SOURCE_DIR" ]] || fail "sursa si destinatia trebuie sa fie diferite."

git -C "$TARGET_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1 || \
    fail "destinatia nu este un repository Git: $TARGET_DIR"

readonly GIT_ROOT="$(realpath -- "$(git -C "$TARGET_DIR" rev-parse --show-toplevel)")"
[[ "$GIT_ROOT" == "$TARGET_DIR" ]] || \
    fail "destinatia trebuie sa fie radacina repository-ului Git: $GIT_ROOT"

if [[ -n "$(git -C "$TARGET_DIR" status --porcelain)" ]]; then
    if [[ "$APPLY_CHANGES" == true ]]; then
        fail "repository-ul destinatie are deja modificari. Fa commit/stash sau revino la simulare."
    fi
    printf 'Atentie: repository-ul destinatie are deja modificari locale.\n\n'
fi

readonly -a ROOT_FILES=(
    ".gitignore"
    "README.md"
    "CMakeLists.txt"
    "CMakePresets.json"
    "build.sh"
    "run.sh"
    "create_desktop_shortcut.sh"
    "download_fonts.sh"
    "generate_icon.sh"
)

readonly -a MANAGED_DIRECTORIES=(
    "src"
    "assets"
    "test_files"
    "fuzz"
    "fonts"
    "documentation"
    "windows"
    ".github"
    "third_party"
)

readonly -a COMMON_EXCLUDES=(
    "--exclude=.git/"
    "--exclude=build/"
    "--exclude=.build/"
    "--exclude=users/"
    "--exclude=archives/"
    "--exclude=extracted/"
    "--exclude=imgui.ini"
    "--exclude=*.enc"
    "--exclude=*.pqc"
    "--exclude=*.pem"
    "--exclude=*.key"
)

RSYNC_OPTIONS=(--archive --checksum --delete --itemize-changes --human-readable)
if [[ "$APPLY_CHANGES" == false ]]; then
    RSYNC_OPTIONS+=(--dry-run)
fi

validate_managed_path() {
    local relative_path="$1"
    local source_path="$SOURCE_DIR/$relative_path"
    local target_path="$TARGET_DIR/$relative_path"

    [[ -d "$source_path" ]] || fail "directorul sursa lipseste: $source_path"
    [[ ! -L "$source_path" ]] || fail "directorul sursa este o legatura simbolica: $source_path"
    [[ ! -L "$target_path" ]] || fail "directorul destinatie este o legatura simbolica: $target_path"

    if find "$source_path" -type l -print -quit | grep -q .; then
        fail "directorul sursa contine legaturi simbolice si nu va fi sincronizat: $source_path"
    fi

    if [[ -d "$target_path" ]] && find "$target_path" -type l -print -quit | grep -q .; then
        fail "directorul destinatie contine legaturi simbolice: $target_path"
    fi

    if [[ -d "$target_path" ]] && \
        find "$target_path" -mindepth 1 -name .git -print -quit | grep -q .; then
        fail "directorul destinatie contine un repository Git imbricat: $target_path"
    fi
}

sync_file() {
    local source_relative="$1"
    local target_relative="${2:-$1}"
    local source_path="$SOURCE_DIR/$source_relative"
    local target_path="$TARGET_DIR/$target_relative"

    [[ -f "$source_path" ]] || fail "fisierul sursa lipseste: $source_path"
    [[ ! -L "$source_path" ]] || fail "fisierul sursa este o legatura simbolica: $source_path"
    [[ ! -L "$target_path" ]] || fail "fisierul destinatie este o legatura simbolica: $target_path"

    rsync "${RSYNC_OPTIONS[@]}" "$source_path" "$target_path"
}

sync_directory() {
    local relative_path="$1"
    validate_managed_path "$relative_path"

    rsync \
        "${RSYNC_OPTIONS[@]}" \
        "${COMMON_EXCLUDES[@]}" \
        "$SOURCE_DIR/$relative_path/" \
        "$TARGET_DIR/$relative_path/"
}

printf 'Sursa:      %s\n' "$SOURCE_DIR"
printf 'Destinatie: %s\n' "$TARGET_DIR"
if [[ "$APPLY_CHANGES" == true ]]; then
    printf 'Mod:         APLICARE\n\n'
else
    printf 'Mod:         SIMULARE (nu se modifica fisiere)\n\n'
fi

for root_file in "${ROOT_FILES[@]}"; do
    sync_file "$root_file"
done

# Pastreaza compatibilitatea cu comenzile Windows documentate la radacina.
sync_file "windows/setup_windows.bat" "setup_windows.bat"
sync_file "windows/create_desktop_shortcut_windows.bat" \
    "create_desktop_shortcut_windows.bat"

# Publica utilitarul de sincronizare fara vechiul script de initializare.
validate_managed_path "git_setup"
rsync \
    "${RSYNC_OPTIONS[@]}" \
    --delete-excluded \
    --include=/sync_core_repo.sh \
    --exclude='*' \
    "$SOURCE_DIR/git_setup/" \
    "$TARGET_DIR/git_setup/"

for managed_directory in "${MANAGED_DIRECTORIES[@]}"; do
    sync_directory "$managed_directory"
done

# Copiaza strict fisierele ImGui utilizate de CMake (core + GLFW + OpenGL3).
validate_managed_path "imgui"
rsync \
    "${RSYNC_OPTIONS[@]}" \
    --delete-excluded \
    --include=/backends/ \
    --include=/backends/imgui_impl_glfw.cpp \
    --include=/backends/imgui_impl_glfw.h \
    --include=/backends/imgui_impl_opengl3.cpp \
    --include=/backends/imgui_impl_opengl3.h \
    --include=/backends/imgui_impl_opengl3_loader.h \
    --include=/LICENSE.txt \
    --include=/imconfig.h \
    --include=/imgui.cpp \
    --include=/imgui.h \
    --include=/imgui_demo.cpp \
    --include=/imgui_draw.cpp \
    --include=/imgui_internal.h \
    --include=/imgui_tables.cpp \
    --include=/imgui_widgets.cpp \
    --include=/imstb_rectpack.h \
    --include=/imstb_textedit.h \
    --include=/imstb_truetype.h \
    --exclude='*' \
    "$SOURCE_DIR/imgui/" \
    "$TARGET_DIR/imgui/"

printf '\n'
if [[ "$APPLY_CHANGES" == false ]]; then
    printf 'Simulare terminata. Verifica lista de mai sus.\n'
    printf 'Pentru aplicare: %s --target %q --apply\n' "$SCRIPT_NAME" "$TARGET_DIR"
else
    printf 'Sincronizare terminata. Nu s-a executat niciun commit sau push.\n\n'
    git -C "$TARGET_DIR" status --short --branch
    printf '\nVerifica modificarile cu:\n'
    printf '  git -C %q diff --check\n' "$TARGET_DIR"
    printf '  git -C %q diff\n' "$TARGET_DIR"
fi

printf '\nNota: liboqs nu este copiat; ramane o dependenta externa a proiectului.\n'
