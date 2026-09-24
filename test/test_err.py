"""
test/asm_err/*.pt を全て変換し，エラーが出ることを確認するテストスクリプト。
- 終了コードが1で、かつ構文エラーのメッセージ(asm syntax error)が出力されることを「成功（エラー検出）」とする。
  (終了コードが非0なだけでは、例外で異常終了した場合も成功とみなしてしまうため)
- エラーが出なかった場合は「失敗（エラー未検出）」として報告する。
"""

import os
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASM_ERR_DIR = os.path.join(SCRIPT_DIR, "asm_err")
ASM2SV = os.path.join(os.path.dirname(SCRIPT_DIR), "asm2sv.exe")

def main():
    if not os.path.isdir(ASM_ERR_DIR):
        print(f"asm_err ディレクトリが見つかりません: {ASM_ERR_DIR}")
        sys.exit(1)

    asm_files = sorted(
        f for f in os.listdir(ASM_ERR_DIR) if f.endswith(".pt")
    )

    if not asm_files:
        print("異常系テストケースが見つかりません。")
        sys.exit(1)

    detected = []    # エラー検出成功（アセンブラが正しくエラーを返した）
    undetected = []  # エラー未検出（アセンブラが誤って正常終了した）

    with tempfile.TemporaryDirectory() as tmpdir:
        for asm_file in asm_files:
            asm_path = os.path.join(ASM_ERR_DIR, asm_file)
            sv_path = os.path.join(tmpdir, os.path.splitext(asm_file)[0] + ".sv")

            # 出力は入力由来の日本語を含みうるため，Windows既定のcp932ではなくUTF-8で読む
            result = subprocess.run(
                [ASM2SV, asm_path, "-sv", sv_path],
                capture_output=True,
                encoding="utf-8",
                errors="backslashreplace",
            )

            stdout = result.stdout.strip()
            stderr = result.stderr.strip()
            output = (stdout + "\n" + stderr).strip()

            # 終了コード1 かつ 構文エラーのメッセージが含まれていればエラー検出成功
            error_detected = result.returncode == 1 and "asm syntax error" in output

            if error_detected:
                detected.append((asm_file, output))
                print(f"[OK]   {asm_file}: エラー検出 ({output})")
            else:
                undetected.append((asm_file, output))
                print(f"[FAIL] {asm_file}: エラーが検出されなかった (returncode={result.returncode}, output={output!r})")

    print()
    print(f"成功（エラー検出）: {len(detected)} 件 / 失敗（エラー未検出）: {len(undetected)} 件")

    if undetected:
        sys.exit(1)

if __name__ == "__main__":
    main()
