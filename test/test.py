"""
test/asm/*.pt を全て変換して test/sv/ へ出力するテストスクリプト．
アセンブラをビルドしてから変換する．
変換に成功した件数と失敗した件数を報告する．
sv_ans/ に期待値ファイルがなければ FAIL とする．
"""

import difflib
import os
import subprocess
import sys

from asm_build import ASM2MC, build_assembler

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASM_DIR = os.path.join(SCRIPT_DIR, "asm")
SV_DIR = os.path.join(SCRIPT_DIR, "sv")
SV_ANS_DIR = os.path.join(SCRIPT_DIR, "sv_ans")

def main():
    # まずアセンブラをビルドする
    if not build_assembler():
        sys.exit(1)

    os.makedirs(SV_DIR, exist_ok=True)

    asm_files = sorted(
        f for f in os.listdir(ASM_DIR) if f.endswith(".pt")
    )

    if not asm_files:
        print("テストケースが見つかりません．")
        sys.exit(1)

    convert_success = []
    convert_fail = []
    compare_pass = []
    compare_fail = []

    for asm_file in asm_files:
        asm_path = os.path.join(ASM_DIR, asm_file)
        sv_name = os.path.splitext(asm_file)[0] + ".sv"
        sv_path = os.path.join(SV_DIR, sv_name)
        ans_path = os.path.join(SV_ANS_DIR, sv_name)

        # 出力は入力由来の日本語を含みうるため，Windows既定のcp932ではなくUTF-8で読む
        result = subprocess.run(
            [ASM2MC, asm_path, "-sv", sv_path],
            capture_output=True,
            encoding="utf-8",
            errors="backslashreplace",
        )

        stdout = result.stdout.strip()
        stderr = result.stderr.strip()
        output = (stdout + "\n" + stderr).strip()

        if result.returncode != 0:
            convert_fail.append((asm_file, output))
            print(f"[FAIL] {asm_file}: {output}")
            continue

        convert_success.append(asm_file)

        # sv_ans/ に期待値ファイルがなければエラー
        if not os.path.exists(ans_path):
            compare_fail.append(asm_file)
            print(f"[FAIL] {asm_file}  (sv_ans/{sv_name} が存在しません)")
            continue

        # 改行コードの違いも検出するため，newline=""で改行コードを変換せずに読む
        with open(sv_path, encoding="utf-8", newline="") as f:
            actual_lines = f.readlines()
        with open(ans_path, encoding="utf-8", newline="") as f:
            expected_lines = f.readlines()

        if actual_lines == expected_lines:
            compare_pass.append(asm_file)
            print(f"[PASS] {asm_file}")
        else:
            compare_fail.append(asm_file)
            # 改行コードだけの違いも差分に見えるよう，CRを\rと表記する(表示時のsplitlines()ではCRLFごと取り除かれるため)
            diff = difflib.unified_diff(
                [line.replace("\r", "\\r") for line in expected_lines],
                [line.replace("\r", "\\r") for line in actual_lines],
                fromfile=f"expected (sv_ans/{sv_name})",
                tofile=f"actual   (sv/{sv_name})",
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
