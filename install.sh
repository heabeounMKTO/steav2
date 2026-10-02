#!/usr/bin/env bash
# Builds steav2 in Release and installs:
#   steav, steav2-lsp          -> $PREFIX/bin          (default ~/.local)
#   syntax / ftplugin / lua    -> $VIM_DIR             (default ~/.config/lvim,
#                                                       or ~/.config/nvim if there's no lvim)
#
# usage: ./install.sh [--prefix DIR] [--vim-dir DIR] [--no-editor]
set -euo pipefail

root="$(cd "$(dirname "$0")" && pwd)"
prefix="$HOME/.local"
if [ -d "$HOME/.config/lvim" ]; then vim_dir="$HOME/.config/lvim"; else vim_dir="$HOME/.config/nvim"; fi
editor=1

while [ $# -gt 0 ]; do
  case "$1" in
    --prefix) prefix="$2"; shift 2 ;;
    --vim-dir) vim_dir="$2"; shift 2 ;;
    --no-editor) editor=0; shift ;;
    -h|--help) sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown option: $1 (see --help)" >&2; exit 64 ;;
  esac
done

build="$root/build-release"
echo "==> building (Release) in $build"
cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_STEAV_EXAMPLES=ON -DBUILD_STEAV_LSP=ON >/dev/null
cmake --build "$build" --parallel >/dev/null

echo "==> installing steav + steav2-lsp to $prefix/bin"
mkdir -p "$prefix/bin"
install -m 755 "$build/steav" "$prefix/bin/steav"
install -m 755 "$build/steav2-lsp" "$prefix/bin/steav2-lsp"

if [ $editor -eq 1 ]; then
  echo "==> installing editor files to $vim_dir"
  mkdir -p "$vim_dir/syntax" "$vim_dir/ftplugin" "$vim_dir/lua"
  cp "$root/editor/nvim/syntax/steav2.vim" "$vim_dir/syntax/"
  cp "$root/editor/nvim/ftplugin/steav2.vim" "$vim_dir/ftplugin/"
  cp "$root/editor/nvim/steav2.lua" "$vim_dir/lua/steav2.lua"
fi

case ":$PATH:" in
  *":$prefix/bin:"*) ;;
  *) echo; echo "!! $prefix/bin isn't on your PATH, add it so your editor can find steav2-lsp" ;;
esac

if [ $editor -eq 1 ]; then
  config="$vim_dir/init.lua"
  [ -f "$vim_dir/config.lua" ] && config="$vim_dir/config.lua"
  echo
  echo "Last step, once: add this line to $config"
  echo
  echo '    require("steav2")'
  echo
  if grep -q 'sts = "steavscript"' "$config" 2>/dev/null; then
    echo "!! $config still maps .sts to the old steavscript + sts-lsp."
    echo "   Remove (or comment out) that block, both claim .sts and the last one wins."
    echo
  fi
  echo "Then open a .sts file, :LspInfo should list steav2-lsp."
fi
echo "done."
