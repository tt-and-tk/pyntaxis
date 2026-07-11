"""
test/asm/*.asm を全て変換して test/bin/ へ出力するテストスクリプト。
変換に成功した件数と失敗した件数を報告する。
bin_ans/ に期待値ファイルがなければ FAIL とする。
"""

import difflib
import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASM_DIR = os.path.join(SCRIPT_DIR, "asm")
BIN_DIR = os.path.join(SCRIPT_DIR, "bin")
BIN_ANS_DIR = os.path.join(SCRIPT_DIR, "bin_ans")
ASM2BIN = os.path.join(os.path.dirname(SCRIPT_DIR), "asm2bin.exe")

def main():
    os.makedirs(BIN_DIR, exist_ok=True)

    asm_files = sorted(
        f for f in os.listdir(ASM_DIR) if f.endswith(".asm")
    )

    if not asm_files:
        print("テストケースが見つかりません。")
        sys.exit(1)

    convert_success = []
    convert_fail = []
    compare_pass = []
    compare_fail = []

    for asm_file in asm_files:
        asm_path = os.path.join(ASM_DIR, asm_file)
        sv_name = asm_file.replace(".asm", ".sv")
        sv_path = os.path.join(BIN_DIR, sv_name)
        ans_path = os.path.join(BIN_ANS_DIR, sv_name)

        result = subprocess.run(
            [ASM2BIN, asm_path, "-b", sv_path],
            capture_output=True,
            text=True,
        )

        stdout = result.stdout.strip()
        stderr = result.stderr.strip()
        output = (stdout + "\n" + stderr).strip()

        if result.returncode != 0:
            convert_fail.append((asm_file, output))
            print(f"[FAIL] {asm_file}: {output}")
            continue

        convert_success.append(asm_file)

        # bin_ans/ に期待値ファイルがなければエラー
        if not os.path.exists(ans_path):
            compare_fail.append(asm_file)
            print(f"[FAIL] {asm_file}  (bin_ans/{sv_name} が存在しません)")
            continue

        with open(sv_path, encoding="utf-8") as f:
            actual_lines = f.readlines()
        with open(ans_path, encoding="utf-8") as f:
            expected_lines = f.readlines()

        if actual_lines == expected_lines:
            compare_pass.append(asm_file)
            print(f"[PASS] {asm_file}")
        else:
            compare_fail.append(asm_file)
            diff = difflib.unified_diff(
                expected_lines,
                actual_lines,
                fromfile=f"expected (bin_ans/{sv_name})",
                tofile=f"actual   (bin/{sv_name})",
            )
            print(f"[FAIL] {asm_file}")
            for line in "".join(diff).splitlines():
                print(f"  {line}")

    print()
    print(f"アセンブル成功: {len(convert_success)}件 / 失敗: {len(convert_fail)}件")
    print(f"期待値比較  PASS: {len(compare_pass)}件 / FAIL: {len(compare_fail)}件")

    if convert_fail or compare_fail:
        sys.exit(1)

if __name__ == "__main__":
    main()
