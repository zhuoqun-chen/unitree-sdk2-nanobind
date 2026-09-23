#!/usr/bin/env bash
# Build both variant wheels into dist/:
#   real:  unitree_sdk2_bind-<ver>        links the unitree_sdk2 submodule + bundled DDS
#   stub:  unitree_sdk2_bind-<ver>+stub   hardware-free, synthetic data (USE_STUB_SDK=ON)
# The +stub local-version label keeps the two filenames distinct; the wheels are
# still mutually exclusive at install time (same import package).
# Distributable manylinux wheels come from CI (.github/workflows/wheels.yml).
# The real build needs the submodule: git submodule update --init
set -euo pipefail
cd "$(dirname "$0")/.."

rm -rf dist

# Real (default: USE_STUB_SDK=OFF, links third_party/unitree_sdk2).
uv build --wheel -o dist

# Stub: append +stub to the version so the filename differs, build hardware-free.
cp pyproject.toml pyproject.toml.bak
trap 'mv -f pyproject.toml.bak pyproject.toml 2>/dev/null || true' EXIT
sed -i 's/^version = "\(.*\)"$/version = "\1+stub"/' pyproject.toml
uv build --wheel -o dist -C cmake.define.USE_STUB_SDK=ON
mv -f pyproject.toml.bak pyproject.toml
trap - EXIT

echo
echo "Built:"
ls -1 dist/
