"""
各テストスクリプトが共通で使う，テスト対象のアセンブラをビルドする処理．
どのスクリプトも単独で実行でき，常に最新のソースからビルドしたアセンブラでテストするよう，各スクリプトの最初に呼び出す．
"""

import os
import subprocess

ASSEMBLER_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # アセンブラのソースがあるディレクトリ
ASM2MC = os.path.join(ASSEMBLER_DIR, "asm2mc.exe")                            # ビルドしたアセンブラの実行ファイル
SOURCE = os.path.join(ASSEMBLER_DIR, "asm2mc.cpp")                            # ビルド対象のソース(ヘッダはこのファイルが取り込む)

def build_assembler():
    """アセンブラをビルドする．成功すれば True を返す．"""
    # 警告を有効にして，ソースから実行ファイルを作るコマンドを組み立てる
    cmd = ["g++", "-Wall", "-Wextra", "-std=c++17", "-o", ASM2MC, SOURCE]
    # g++の警告はソース中の日本語コメントを引用するため，Windows既定のcp932ではなくUTF-8で読む
    result = subprocess.run(cmd, capture_output=True, encoding="utf-8", errors="backslashreplace")
    # ビルドに失敗した場合は，原因が分かるようg++のエラー出力を表示する
    if result.returncode != 0:
        print("[BUILD FAIL] アセンブラのビルドに失敗しました")
        print(result.stderr.strip())
        return False
    return True
