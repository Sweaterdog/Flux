#!/bin/bash
# ===========================================================================
# Flux Language Installer
#
# Builds Flux from source and installs it system-wide so you can run
# `flux run <file.flux>` from anywhere.
#
# Usage:
#   ./install.sh              Build and install to /usr/local/bin
#   ./install.sh --prefix=/opt  Install to custom prefix
#   ./install.sh --uninstall    Remove installed binary
# ===========================================================================

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

# Defaults
PREFIX="/usr/local"
UNINSTALL=false
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --prefix=*)
            PREFIX="${arg#--prefix=}"
            ;;
        --uninstall)
            UNINSTALL=true
            ;;
        --help|-h)
            echo "Flux Language Installer"
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --prefix=<path>  Install to custom prefix (default: /usr/local)"
            echo "  --uninstall      Remove installed Flux binary"
            echo "  --help, -h       Show this help"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $arg${RESET}"
            echo "Use --help for usage information."
            exit 1
            ;;
    esac
done

INSTALL_DIR="$PREFIX/bin"

# ---- Uninstall ----
if $UNINSTALL; then
    echo -e "${CYAN}Uninstalling Flux...${RESET}"
    if [[ -f "$INSTALL_DIR/flux" ]]; then
        if [[ -w "$INSTALL_DIR" ]]; then
            rm -f "$INSTALL_DIR/flux"
        else
            sudo rm -f "$INSTALL_DIR/flux"
        fi
        echo -e "${GREEN}Flux has been removed from $INSTALL_DIR${RESET}"
    else
        echo -e "${YELLOW}Flux is not installed at $INSTALL_DIR${RESET}"
    fi
    exit 0
fi

# ---- Build ----
echo -e "${BOLD}${CYAN}═══════════════════════════════════════════════${RESET}"
echo -e "${BOLD}${CYAN}         Flux Language Installer               ${RESET}"
echo -e "${BOLD}${CYAN}═══════════════════════════════════════════════${RESET}"
echo ""

# Check for g++
if ! command -v g++ &> /dev/null; then
    echo -e "${RED}Error: g++ is required but not installed.${RESET}"
    echo "Install it with: sudo apt install g++ (Debian/Ubuntu)"
    echo "                 sudo dnf install gcc-c++ (Fedora)"
    echo "                 sudo pacman -S gcc (Arch)"
    exit 1
fi

# Check for make
if ! command -v make &> /dev/null; then
    echo -e "${RED}Error: make is required but not installed.${RESET}"
    echo "Install it with: sudo apt install make"
    exit 1
fi

echo -e "${CYAN}[1/3] Building Flux...${RESET}"
cd "$SCRIPT_DIR"
make release 2>&1 | tail -5

if [[ ! -f "$SCRIPT_DIR/build/flux" ]]; then
    echo -e "${RED}Build failed. See errors above.${RESET}"
    exit 1
fi

echo -e "${GREEN}Build successful.${RESET}"
echo ""

# ---- Install ----
echo -e "${CYAN}[2/3] Installing to $INSTALL_DIR...${RESET}"

# Create install directory if needed
if [[ ! -d "$INSTALL_DIR" ]]; then
    if [[ -w "$(dirname "$INSTALL_DIR")" ]]; then
        mkdir -p "$INSTALL_DIR"
    else
        echo -e "${YELLOW}Need sudo to create $INSTALL_DIR${RESET}"
        sudo mkdir -p "$INSTALL_DIR"
    fi
fi

# Copy the binary
if [[ -w "$INSTALL_DIR" ]]; then
    cp "$SCRIPT_DIR/build/flux" "$INSTALL_DIR/flux"
    chmod +x "$INSTALL_DIR/flux"
else
    echo -e "${YELLOW}Need sudo to install to $INSTALL_DIR${RESET}"
    sudo cp "$SCRIPT_DIR/build/flux" "$INSTALL_DIR/flux"
    sudo chmod +x "$INSTALL_DIR/flux"
fi

echo -e "${GREEN}Installed flux to $INSTALL_DIR/flux${RESET}"
echo ""

# ---- Verify ----
echo -e "${CYAN}[3/3] Verifying installation...${RESET}"

if command -v flux &> /dev/null; then
    INSTALLED_PATH="$(command -v flux)"
    echo -e "${GREEN}Success! flux is available at: $INSTALLED_PATH${RESET}"
    echo ""
    echo -e "${BOLD}Usage:${RESET}"
    echo "  flux run <file.flux>                  Run a Flux program"
    echo "  flux compile <file.flux> -o output    Compile to native binary"
    echo "  flux                                  Start the REPL"
else
    echo -e "${YELLOW}Warning: $INSTALL_DIR is not in your PATH.${RESET}"
    echo ""
    echo "Add it by running:"
    echo "  export PATH=\"$INSTALL_DIR:\$PATH\""
    echo ""
    echo "To make it permanent, add that line to your ~/.bashrc or ~/.zshrc"
fi

echo ""
echo -e "${BOLD}${CYAN}═══════════════════════════════════════════════${RESET}"
echo -e "${GREEN}  Flux has been installed successfully!${RESET}"
echo -e "${BOLD}${CYAN}═══════════════════════════════════════════════${RESET}"
