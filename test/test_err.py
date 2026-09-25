"""
test/asm_err/*.pt を全て変換し，期待したエラーが出ることを確認するテストスクリプト．
- 各ファイルの1行目に「; expect: <期待するメッセージ>」の形で期待するエラーメッセージを書く．
- 終了コードが1で，かつ出力が期待するメッセージと完全に一致することを「成功(エラー検出)」とする．
  (終了コードが非0なだけでは，例外で異常終了した場合も成功とみなしてしまうため)
  (構文エラーのメッセージが出たかだけでは，確かめたい誤りとは別の誤りで失敗した場合も成功とみなしてしまうため)
- 1行目に期待するメッセージがなければ「失敗」とする．
- エラーが出なかった場合や，期待と異なるエラーが出た場合は「失敗」として報告する．
"""

import os
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASM_ERR_DIR = os.path.join(SCRIPT_DIR, "asm_err")
ASM2MC = os.path.join(os.path.dirname(SCRIPT_DIR), "asm2mc.exe")
EXPECT_PREFIX = "; expect: "  # 期待するエラーメッセージを書く1行目の接頭辞

def read_expected(asm_path):
    """入力ファイルの1行目に書かれた期待するエラーメッセージを返す．書かれていなければ None を返す．"""
    # 期待するメッセージは入力由来の日本語を含みうるため，UTF-8で読む
    with open(asm_path, encoding="utf-8") as f:
        first_line = f.readline().rstrip("\r\n")
    if not first_line.startswith(EXPECT_PREFIX):
        return None
    return first_line[len(EXPECT_PREFIX):].strip()

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

    detected = []    # エラー検出成功(アセンブラが期待どおりのエラーを返した)
    failed = []      # 失敗(エラー未検出・期待と異なるエラー・期待するメッセージの記載なし)

    with tempfile.TemporaryDirectory() as tmpdir:
        for asm_file in asm_files:
            asm_path = os.path.join(ASM_ERR_DIR, asm_file)
            sv_path = os.path.join(tmpdir, os.path.splitext(asm_file)[0] + ".sv")

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

            # 1行目に期待するメッセージがなければエラー
            expected = read_expected(asm_path)
            if expected is None:
                failed.append((asm_file, output))
                print(f"[FAIL] {asm_file}: 1行目に期待するメッセージ({EXPECT_PREFIX}...)がありません (output={output!r})")
                continue

            # 終了コード1 かつ 出力が期待値と一致していればエラー検出成功
            if result.returncode == 1 and output == expected:
                detected.append((asm_file, output))
                print(f"[OK]   {asm_file}: エラー検出 ({output})")
            else:
                failed.append((asm_file, output))
                print(f"[FAIL] {asm_file}: 期待したエラーが検出されなかった (returncode={result.returncode})")
                print(f"  expected: {expected!r}")
                print(f"  actual:   {output!r}")

    print()
    print(f"成功(エラー検出): {len(detected)} 件 / 失敗: {len(failed)} 件")

    if failed:
        sys.exit(1)

if __name__ == "__main__":
    main()
