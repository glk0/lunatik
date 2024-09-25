#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2024 jperon
# SPDX-License-Identifier: MIT OR GPL-2.0-only

LUNATIK_DIR="/opt/lunatik"
XDP_DIR="/opt/xdp-tools"
KERNEL_RELEASE=$(uname -r)
# This ugly hack is needed because Ubuntu has a weird kernel package versionning.
KERNEL_VERSION=$( (grep -o '[0-9.]*$' /proc/version_signature 2>/dev/null) || (uname -r | grep -o '^[0-9.]*') )
KERNEL_VERSION_MAJOR=$(echo "${KERNEL_VERSION}" | grep -o '^[0-9]*')
CPU_CORES=$( grep -m1 'cpu cores' /proc/cpuinfo | grep -o '[0-9]*$')

echo "Checking git, linux-headers-${KERNEL_RELEASE}, linux-tools-generic, lua5.4 and pahole are installed..."
dpkg --get-selections | grep 'git\s' | grep install &&\
dpkg --get-selections | grep "linux-headers-${KERNEL_RELEASE}\s" | grep install &&\
dpkg --get-selections | grep 'linux-tools-generic\s' | grep install &&\
dpkg --get-selections | grep 'lua5.4\s' | grep install &&\
dpkg --get-selections | grep 'pahole\s' | grep install || exit 1
cp /sys/kernel/btf/vmlinux "/usr/lib/modules/${KERNEL_RELEASE}/build/" || exit 1

echo "Compiling and installing resolve_btfids from kernel sources"
cd /usr/local/src &&\
wget -O- "https://cdn.kernel.org/pub/linux/kernel/v${KERNEL_VERSION_MAJOR}.x/linux-${KERNEL_VERSION}.tar.xz" | tar xJf - &&\
cd "linux-${KERNEL_VERSION}"/tools/bpf/resolve_btfids/ &&\
make -j"${CPU_CORES}" &&\
mkdir -p "/usr/src/linux-headers-${KERNEL_RELEASE}/tools/bpf/resolve_btfids/" &&\
cp resolve_btfids /usr/src/linux-headers-`uname -r`/tools/bpf/resolve_btfids/ || exit 1

echo "Compiling and installing Lunatik"
if [ -d "${LUNATIK_DIR}" ]; then
  cd "${LUNATIK_DIR}" && git pull --ff-only
  make clean
else
  git clone --recurse-submodules https://github.com/luainkernel/lunatik "${LUNATIK_DIR}"
  cd "${LUNATIK_DIR}"
fi
make -j"${CPU_CORES}" && make install && rm -r /usr/local/src/"linux-${KERNEL_VERSION}" || exit 1

echo "Compiling and installing xdp-loader" &&\
if [ -d "${XDP_DIR}" ]; then
  cd "${XDP_DIR}" && git pull --ff-only
  make clean
else
  git clone --recurse-submodules https://github.com/xdp-project/xdp-tools "${XDP_DIR}"
fi
cd "${XDP_DIR}"/lib/libbpf/src && make && sudo DESTDIR=/ make install &&\
cd ../../../ && make clean && make -j"${CPU_CORES}" libxdp &&\
cd xdp-loader && make && sudo make install
