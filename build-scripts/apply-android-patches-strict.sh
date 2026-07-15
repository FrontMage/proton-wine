#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
STATUS_FILE="${PATCH_STATUS_PATH:-${ROOT_DIR}/patch-status.tsv}"

patches=(
  dlls_advapi32_advapi.c.patch
  dlls_amd_ags_x64_unixlib.c.patch
  dlls_dnsapi_libresolv.c.patch
  dlls_dnsapi_record.c.patch
  dlls_midimap_Makefile.in.patch
  dlls_midimap_midimap.c.patch
  dlls_nsiproxy.sys_nsi_common.h.patch
  dlls_nsiproxy.sys_ip.c.patch
  dlls_nsiproxy.sys_ndis.c.patch
  dlls_ntdll_Makefile.in.patch
  dlls_ntdll_unix_fsync.c.patch
  dlls_ntdll_unix_loader.c.patch
  dlls_ntdll_unix_server.c.patch
  dlls_ntdll_unix_sync.c.patch
  dlls_ntdll_unix_virtual.c.patch
  dlls_ntdll_unix_signal_x86_64.c.patch
  dlls_opengl32_unix_wgl.c.patch
  dlls_user32_Makefile.in.patch
  dlls_win32u_clipboard.c.patch
  dlls_winebus.sys_bus_sdl.c.patch
  dlls_winepulse.drv_pulse.c.patch
  dlls_winex11.drv_bitblt.c.patch
  dlls_winex11.drv_keyboard.c.patch
  dlls_winex11.drv_mouse.c.patch
  dlls_winex11.drv_opengl.c.patch
  dlls_winex11.drv_window.c.patch
  dlls_winex11.drv_x11drv.h.patch
  dlls_winex11.drv_x11drv_main.c.patch
  dlls_wow64_syscall.c.patch
  loader_preloader.c.patch
  programs_explorer_desktop.c.patch
  programs_wineboot_wineboot.c.patch
  programs_winebrowser_Makefile.in.patch
  programs_winebrowser_main.c.patch
  programs_winemenubuilder_winemenubuilder.c.patch
  server_Makefile.in.patch
  server_fsync.c.patch
  server_inproc_sync.c.patch
  server_main.c.patch
  server_thread.c.patch
  server_unicode.c.patch
  dlls_ntdll_unix_esync.c.patch
  dlls_ntdll_unix_esync.h.patch
  server_esync.c.patch
  server_esync.h.patch
)

cd "${ROOT_DIR}"
printf 'patch\tcheck\tapply\n' > "${STATUS_FILE}"

for patch_name in "${patches[@]}"; do
  patch_path="android/patches/${patch_name}"
  if ! git apply --check "${patch_path}"; then
    printf '%s\tfailed\tnot-run\n' "${patch_name}" >> "${STATUS_FILE}"
    echo "Android patch check failed: ${patch_name}" >&2
    exit 1
  fi
  printf '%s\tok\t' "${patch_name}" >> "${STATUS_FILE}"
  if ! git apply "${patch_path}"; then
    printf 'failed\n' >> "${STATUS_FILE}"
    echo "Android patch application failed: ${patch_name}" >&2
    exit 1
  fi
  printf 'ok\n' >> "${STATUS_FILE}"
done

printf 'all\tok\tok\n' >> "${STATUS_FILE}"
