#!/usr/bin/env bash
# 仅构建静态 libmodbus.a（不生成 libmodbus_backend.so）
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TP_DIR="${ROOT_DIR}/third_party"
LIBMODBUS_VER="3.1.11"
LIBMODBUS_ARCHIVE="libmodbus-${LIBMODBUS_VER}.tar.gz"
LIBMODBUS_SRC_DIR="${TP_DIR}/libmodbus-${LIBMODBUS_VER}"
LIBMODBUS_PREFIX="${TP_DIR}/libmodbus-local"

mkdir -p "${TP_DIR}"

if [[ ! -d "${LIBMODBUS_SRC_DIR}" ]]; then
  if [[ -f "${TP_DIR}/${LIBMODBUS_ARCHIVE}" ]]; then
    tar xf "${TP_DIR}/${LIBMODBUS_ARCHIVE}" -C "${TP_DIR}"
  else
    echo "[download] libmodbus ${LIBMODBUS_VER}..."
    curl -L --fail -o "${TP_DIR}/${LIBMODBUS_ARCHIVE}" \
      "https://github.com/stephane/libmodbus/releases/download/v${LIBMODBUS_VER}/${LIBMODBUS_ARCHIVE}"
    tar xf "${TP_DIR}/${LIBMODBUS_ARCHIVE}" -C "${TP_DIR}"
  fi
fi

if [[ ! -f "${LIBMODBUS_PREFIX}/lib/libmodbus.a" ]]; then
  echo "[build] static libmodbus -> ${LIBMODBUS_PREFIX}"
  pushd "${LIBMODBUS_SRC_DIR}" >/dev/null
  ./configure --prefix="${LIBMODBUS_PREFIX}" --enable-static --disable-shared
  make -C src -j"$(nproc)"
  make -C src install
  popd >/dev/null
else
  echo "[skip] already built: ${LIBMODBUS_PREFIX}/lib/libmodbus.a"
fi

echo "[verify]"
file "${LIBMODBUS_PREFIX}/lib/libmodbus.a"
nm "${LIBMODBUS_PREFIX}/lib/libmodbus.a" | rg "modbus_(new_tcp|connect|read_registers|write_register)" | head || true
echo "Done: static libmodbus ready for qmake link."
