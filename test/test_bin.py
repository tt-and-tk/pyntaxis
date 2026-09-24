"""
実行ファイルの出力(-bin)を確認するテストスクリプト．
- 正常系: test/asm_bin/*.pt を全て実行ファイルに変換して test/bin/ へ出力し，test/bin_ans/ の期待値とバイト単位で比較する．
  期待値のファイル名は，出力と同じく.ptを除いた名前(拡張子なし)とする．
- 異常系: test/asm_bin_err/*.pt を全て実行ファイルに変換し，構文エラーになることを確認する．
- 引数の誤り: -svとの同時指定・Qosmosの実行ファイル名として使えない名前・値の無い-binなどが，引数エラーになり出力ファイルを作らないことを確認する．
"""

import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASM_BIN_DIR = os.path.join(SCRIPT_DIR, "asm_bin")
ASM_BIN_ERR_DIR = os.path.join(SCRIPT_DIR, "asm_bin_err")
BIN_DIR = os.path.join(SCRIPT_DIR, "bin")
BIN_ANS_DIR = os.path.join(SCRIPT_DIR, "bin_ans")
ASM2SV = os.path.join(os.path.dirname(SCRIPT_DIR), "asm2sv.exe")

INSTRUCTION_SIZE = 8  # 実行ファイルでの1命令のバイト数


def run(args):
    """アセンブラを実行し，終了コードと出力(標準出力と標準エラー出力)を返す．"""
    # 出力は入力由来の日本語を含みうるため，Windows既定のcp932ではなくUTF-8で読む
    result = subprocess.run(
        [ASM2SV] + args,
        capture_output=True,
        encoding="utf-8",
        errors="backslashreplace",
    )
    output = (result.stdout.strip() + "\n" + result.stderr.strip()).strip()
    return result.returncode, output


def list_asm(directory):
    """ディレクトリ内の.ptファイル名を名前順に返す．"""
    return sorted(f for f in os.listdir(directory) if f.endswith(".pt"))


def instructions_of(data):
    """実行ファイルの内容を，1命令ずつ64ビットの機械語の16進表記にして返す(差分表示用)．"""
    return [
        f"{int.from_bytes(data[i:i + INSTRUCTION_SIZE], 'little'):016x}"
        for i in range(0, len(data), INSTRUCTION_SIZE)
    ]


def check_normal():
    """正常系を確認し，失敗した件数を返す．"""
    fail = 0

    for asm_file in list_asm(ASM_BIN_DIR):
        name = asm_file[:-len(".pt")]
        bin_path = os.path.join(BIN_DIR, name)
        ans_path = os.path.join(BIN_ANS_DIR, name)

        # 前回の出力が残っていると，出力されなかったことに気付けないため消しておく
        if os.path.exists(bin_path):
            os.remove(bin_path)

        returncode, output = run([os.path.join(ASM_BIN_DIR, asm_file), "-bin", bin_path])
        if returncode != 0:
            fail += 1
            print(f"[FAIL] {asm_file}: {output}")
            continue

        # bin_ans/ に期待値ファイルがなければエラー
        if not os.path.exists(ans_path):
            fail += 1
            print(f"[FAIL] {asm_file}  (bin_ans/{name} が存在しません)")
            continue

        with open(bin_path, "rb") as f:
            actual = f.read()
        with open(ans_path, "rb") as f:
            expected = f.read()

        if actual == expected:
            print(f"[PASS] {asm_file}")
            continue

        # 食い違った命令を，命令の番号と64ビットの機械語で示す
        fail += 1
        print(f"[FAIL] {asm_file}")
        actual_instructions = instructions_of(actual)
        expected_instructions = instructions_of(expected)
        for index in range(max(len(actual_instructions), len(expected_instructions))):
            a = actual_instructions[index] if index < len(actual_instructions) else "(なし)"
            e = expected_instructions[index] if index < len(expected_instructions) else "(なし)"
            if a != e:
                print(f"  index {index}: expected {e} / actual {a}")

    return fail


def check_error():
    """異常系を確認し，失敗した件数を返す．"""
    fail = 0

    for asm_file in list_asm(ASM_BIN_ERR_DIR):
        bin_path = os.path.join(BIN_DIR, "ERR" + asm_file[:-len(".pt")])

        # 前回の出力が残っていると，出力しなかったことを確かめられないため消しておく
        if os.path.exists(bin_path):
            os.remove(bin_path)

        returncode, output = run([os.path.join(ASM_BIN_ERR_DIR, asm_file), "-bin", bin_path])

        # 終了コード1 かつ 構文エラーのメッセージが含まれ，出力ファイルを作っていなければエラー検出成功
        if returncode == 1 and "asm syntax error" in output and not os.path.exists(bin_path):
            print(f"[OK]   {asm_file}: エラー検出 ({output})")
        else:
            fail += 1
            print(f"[FAIL] {asm_file}: エラーが検出されなかった (returncode={returncode}, output={output!r})")

    return fail


def check_args():
    """引数の誤りと実行ファイル名の長さの上限を確認し，失敗した件数を返す．"""
    fail = 0
    asm_path = os.path.join(ASM_BIN_DIR, list_asm(ASM_BIN_DIR)[0])

    # (説明, 出力先の引数, 作られてはならないファイル)
    cases = [
        ("-svと-binの同時指定", ["-sv", os.path.join(BIN_DIR, "BOTH.sv"), "-bin", os.path.join(BIN_DIR, "BOTH")],
         [os.path.join(BIN_DIR, "BOTH.sv"), os.path.join(BIN_DIR, "BOTH")]),
        ("拡張子付きの実行ファイル名", ["-bin", os.path.join(BIN_DIR, "EXT.BIN")], [os.path.join(BIN_DIR, "EXT.BIN")]),
        ("ファイル名の部分が空の実行ファイル名", ["-bin", BIN_DIR + os.sep], []),
        ("8文字を超える実行ファイル名", ["-bin", os.path.join(BIN_DIR, "LONGNAME9")], [os.path.join(BIN_DIR, "LONGNAME9")]),
        ("短い名前に使えない記号を含む実行ファイル名", ["-bin", os.path.join(BIN_DIR, "A+B")], [os.path.join(BIN_DIR, "A+B")]),
        ("値の無い-bin", ["-bin"], [asm_path[:-len(".pt")] + ".sv"]),
        ("空の-bin", ["-bin", ""], [asm_path[:-len(".pt")] + ".sv"]),
    ]

    for description, args, outputs in cases:
        # 前回の出力が残っていると，出力しなかったことを確かめられないため消しておく
        for path in outputs:
            if os.path.exists(path):
                os.remove(path)

        returncode, output = run([asm_path] + args)

        # 終了コード1 かつ 引数エラーのメッセージが含まれ，出力ファイルを作っていなければエラー検出成功
        if returncode == 1 and "args fail" in output and not any(os.path.exists(p) for p in outputs):
            print(f"[OK]   {description}: エラー検出")
        else:
            fail += 1
            print(f"[FAIL] {description}: エラーが検出されなかった (returncode={returncode}, output={output!r})")

    # 名前の長さの上限ちょうど(8文字)の実行ファイル名は受け付けて出力する
    name8_path = os.path.join(BIN_DIR, "LONGNAM8")
    if os.path.exists(name8_path):
        os.remove(name8_path)
    returncode, output = run([asm_path, "-bin", name8_path])
    if returncode == 0 and os.path.exists(name8_path):
        print("[OK]   8文字の実行ファイル名: 出力")
    else:
        fail += 1
        print(f"[FAIL] 8文字の実行ファイル名: 出力されなかった (returncode={returncode}, output={output!r})")

    return fail


def main():
    os.makedirs(BIN_DIR, exist_ok=True)

    fail = check_normal() + check_error() + check_args()

    print()
    print(f"失敗: {fail}件")

    if fail:
        sys.exit(1)


if __name__ == "__main__":
    main()
