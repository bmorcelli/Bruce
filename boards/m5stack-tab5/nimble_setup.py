from __future__ import annotations

from pathlib import Path
import re

try:
    Import("env")
except Exception:  # Running outside PlatformIO/SCons
    env = None


NIMCONFIG_BLOCK = """\
#ifdef CONFIG_IDF_TARGET_ESP32P4
# undef NIMBLE_CFG_CONTROLLER
# define NIMBLE_CFG_CONTROLLER 0
# undef CONFIG_BT_CONTROLLER_ENABLED
# ifndef CONFIG_NIMBLE_CPP_IDF
#  define CONFIG_NIMBLE_CPP_IDF (1)
# endif
#endif
"""


INCLUDE_REGEX = re.compile(r'^\s*#\s*include\s+[<"]esp_bt\.h[>"]')
IF_DIRECTIVE = re.compile(r'^\s*#\s*(if|ifdef|ifndef)\b')
ENDIF_DIRECTIVE = re.compile(r'^\s*#\s*endif\b')
TARGET_IFNDEF = re.compile(r'^\s*#\s*ifndef\s+CONFIG_IDF_TARGET_ESP32P4\b')
P4_FILE_GUARD_BEGIN = "#if 0 /* TAB5_NIMBLE_PATCH */\n"
P4_FILE_GUARD_END = "#endif /* TAB5_NIMBLE_PATCH */\n"
OLD_P4_FILE_GUARD_BEGIN = "#if !defined(CONFIG_IDF_TARGET_ESP32P4) /* TAB5_NIMBLE_PATCH */\n"


def _repo_root() -> Path:
    if env is not None:
        return Path(env.subst("$PROJECT_DIR")).resolve()
    if "__file__" in globals():
        return Path(__file__).resolve().parents[2]
    return Path.cwd().resolve()


def _ensure_nimconfig(nimconfig_path: Path) -> None:
    if not nimconfig_path.exists():
        return

    text = nimconfig_path.read_text(encoding="utf-8", errors="ignore")
    if "CONFIG_IDF_TARGET_ESP32P4" in text and "NIMBLE_CFG_CONTROLLER 0" in text:
        return

    insert_at = text.rfind("\n#endif")
    if insert_at == -1:
        insert_at = text.rfind("#endif")
    if insert_at == -1:
        new_text = text.rstrip() + "\n\n" + NIMCONFIG_BLOCK + "\n"
    else:
        new_text = (
            text[:insert_at].rstrip()
            + "\n\n"
            + NIMCONFIG_BLOCK
            + "\n"
            + text[insert_at:].lstrip()
        )

    if new_text != text:
        nimconfig_path.write_text(new_text, encoding="utf-8")


def _wrap_esp_bt_includes(lib_root: Path) -> None:
    if not lib_root.exists():
        return

    for path in lib_root.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() not in {".h", ".hpp", ".c", ".cpp", ".cc", ".ino"}:
            continue

        text = path.read_text(encoding="utf-8", errors="ignore")
        if "esp_bt.h" not in text:
            continue

        lines = text.splitlines(keepends=True)
        stack = []
        changed = False
        out_lines = []

        i = 0
        while i < len(lines):
            line = lines[i]
            stripped = line.strip()

            if TARGET_IFNDEF.match(line):
                stack.append(True)
            elif IF_DIRECTIVE.match(line):
                stack.append(False)
            elif ENDIF_DIRECTIVE.match(line):
                if stack:
                    stack.pop()

            if INCLUDE_REGEX.match(line):
                if any(stack):
                    out_lines.append(line)
                else:
                    out_lines.append("#ifndef CONFIG_IDF_TARGET_ESP32P4\n")
                    out_lines.append(line if line.endswith("\n") else line + "\n")
                    out_lines.append("#endif\n")
                    changed = True
            else:
                out_lines.append(line)

            i += 1

        if changed:
            new_text = "".join(out_lines)
            if new_text != text:
                path.write_text(new_text, encoding="utf-8")


def _guard_file_for_esp32p4(path: Path) -> None:
    if not path.exists():
        return

    text = path.read_text(encoding="utf-8", errors="ignore")
    if text.startswith(OLD_P4_FILE_GUARD_BEGIN):
        text = text.replace(OLD_P4_FILE_GUARD_BEGIN, P4_FILE_GUARD_BEGIN, 1)
        path.write_text(text, encoding="utf-8")
        return

    if text.startswith(P4_FILE_GUARD_BEGIN):
        return

    if "TAB5_NIMBLE_PATCH" in text:
        return

    new_text = P4_FILE_GUARD_BEGIN + text
    if not new_text.endswith("\n"):
        new_text += "\n"
    new_text += P4_FILE_GUARD_END
    path.write_text(new_text, encoding="utf-8")


def _disable_conflicting_esp32p4_sources(lib_root: Path) -> None:
    files = (
        lib_root / "src" / "nimble" / "nimble" / "transport" / "esp_ipc" / "src" / "hci_esp_ipc.c",
        lib_root / "src" / "nimble" / "porting" / "npl" / "freertos" / "src" / "npl_os_freertos.c",
    )
    for file_path in files:
        _guard_file_for_esp32p4(file_path)


def main() -> None:
    root = _repo_root()
    lib_root = root / ".pio" / "libdeps" / "m5stack-tab5" / "NimBLE-Arduino"
    _ensure_nimconfig(lib_root / "src" / "nimconfig.h")
    _wrap_esp_bt_includes(lib_root)
    _disable_conflicting_esp32p4_sources(lib_root)


def _patch_nimble(*_args, **_kwargs) -> None:
    main()


if env is not None:
    # Run now (when possible) and again right before linking to catch first-build installs.
    _patch_nimble()
    env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", _patch_nimble)
elif __name__ == "__main__":
    _patch_nimble()
