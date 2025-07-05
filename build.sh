#!/bin/bash

# Script pentru clean și build PQCWallet
# Utilizare: ./build.sh [clean|rebuild|debug|release]

PROJECT_DIR="/home/simedruf/Projects/PQCWallet"
BUILD_DIR="$PROJECT_DIR/build"

# Funcția de ajutor
show_help() {
    echo "Utilizare: ./build.sh [opțiune]"
    echo ""
    echo "Opțiuni:"
    echo "  clean      - Curăță build-ul"
    echo "  build      - Build normal"
    echo "  rebuild    - Clean + Build"
    echo "  debug      - Build în modul debug"
    echo "  release    - Build optimizat pentru release"
    echo "  help       - Afișează acest mesaj"
    echo ""
    echo "Fără parametri: build normal"
}

# Funcția de clean
clean_build() {
    echo "🧹 Curăț build-ul..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"/*
        echo "✅ Build curățat cu succes!"
    else
        echo "⚠️  Directorul build nu există"
    fi
}

# Funcția de build
build_project() {
    local build_type="${1:-Release}"
    
    echo "🔨 Compilez aplicația..."
    echo "📁 Directory: $PROJECT_DIR"
    echo "🎯 Build Type: $build_type"
    
    # Creează directorul build dacă nu există
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure cu CMake
    echo "⚙️  Configurez cu CMake..."
    if cmake -DCMAKE_BUILD_TYPE="$build_type" ..; then
        echo "✅ Configurare CMake reușită!"
    else
        echo "❌ Eroare la configurarea CMake!"
        exit 1
    fi
    
    # Build cu Make
    echo "🔧 Compilez cu Make..."
    if make -j$(nproc); then
        echo "✅ Compilare reușită!"
        echo "📦 Executabil creat: $BUILD_DIR/PQCWallet"
    else
        echo "❌ Eroare la compilare!"
        exit 1
    fi
}

# Funcția de rebuild
rebuild_project() {
    clean_build
    build_project "$1"
}

# Main
case "${1:-build}" in
    "clean")
        clean_build
        ;;
    "build"|"")
        build_project "Release"
        ;;
    "rebuild")
        rebuild_project "Release"
        ;;
    "debug")
        rebuild_project "Debug"
        ;;
    "release")
        rebuild_project "Release"
        ;;
    "help"|"-h"|"--help")
        show_help
        ;;
    *)
        echo "❌ Opțiune necunoscută: $1"
        show_help
        exit 1
        ;;
esac

echo ""
echo "🎉 Proces completat!"
echo "🚀 Pentru a rula aplicația: cd $PROJECT_DIR && ./run.sh"
