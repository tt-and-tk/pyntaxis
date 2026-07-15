---
name: issue-resolve
description: GitHub issueに対応する。1issue=1回の実行。「issue #Nに対応して」で起動。
---

# 概要

GitHub issueへの対応 (調査・ブランチ作成・修正・PR作成) を行う．粒度は1issue=1回の実行．対応が複数リポジトリにまたがる場合の進め方は手順4冒頭を参照．

# 手順

## 1. issue内容の確認

```
gh issue view <番号> --repo <owner>/<repo>
```

## 2. 調査

原因調査を行い，影響範囲 (対象リポジトリ・対象ファイル) を特定する．issue本文中に推測が書かれている場合は，事実かどうかをここで検証する．

## 3. 修正方針の承認

調査結果と修正方針をユーザーに提示し，承認を得る．**これが実装前の最後の確認ポイント**であり，承認後は手順4を人間の追加確認なしに連続して実行する．  
ここでは，特に聞かれない限り具体的なソース内容を提示する必要はない．ソースのうち，どの仕事をする部分をどのように修正するかを提示できれば十分．  

## 4. リポジトリごとの実装〜PR作成

影響リポジトリごとに以下を行う．**複数リポジトリに影響する場合，1つのセッションで複数リポジトリを串刺しに編集しない．**このリポジトリでの対応が完了したら，他の影響リポジトリについては，対象リポジトリと該当issue番号を明示した上で，そのリポジトリのプロジェクトで本スキルを実行するようユーザーに依頼する(このセッションから直接他リポジトリを操作しない)．issue自体は1つのままでよく，実装の実行単位だけをリポジトリごとに分ける．

### 4.1 作業場所の準備

`EnterWorktree`(name: `fix/issue-<番号>-<内容を表す短い語句>`)で専用の作業ディレクトリとブランチを作成してから作業する．`EnterWorktree`はブランチ作成まで一体で行うため，**この場合は直後の`git checkout -b`によるブランチ作成コードブロックを実行しない**．

**ただしVivadoプロジェクトを含むリポジトリの場合は対象外とする．** XPRファイルの絶対パス依存やキャッシュ生成コストがあるため，worktreeでの作業は行わず，従来通り単一ディレクトリで直列に作業する(以下のコードブロックでブランチ作成のみ行う)．

```
git checkout <デフォルトブランチ>
git pull
git checkout -b fix/issue-<番号>-<内容を表す短い語句>
```

### 4.2 修正

手順3で承認された方針に基づいて修正を行う．

### 4.3 コミット・push・PR作成

```
git add <変更したファイル>
git commit -m "<コミットメッセージ>"
git push -u origin fix/issue-<番号>-<内容を表す短い語句>
gh pr create --repo <owner>/<repo> --title "<タイトル>" --body "<本文>" [--draft]
```

PR作成時の`--body`に，closeキーワード (`Closes owner/repo#番号`) またはリンクのみ (`Related to owner/repo#番号`) を含める．closeキーワードは1issueにつき1箇所のPRのみに付与する (issueが存在するリポジトリのPR，またはユーザーが指定したPR)．それ以外のリポジトリのPRは`Related to owner/repo#番号`のみを記載する．  

**`--draft`は，closeキーワードを持つPRにのみ付ける．** マージするとissueが閉じるPRであることを一目で分かるようにするための運用．closeキーワードを持たない(`Related to`のみの)PRはissueを閉じないため，通常のPR(Ready)として作成する．  

### 4.4 自動レビューループ

`claude -p`によるレビューを行い，修正すべきと判断した指摘がなくなるまで，人間の確認なしに修正・再レビューを繰り返す．変更内容のうち説明が必要な箇所は，PRへのコメントまたはコミットメッセージで補足する．  

レビューはgit diffの確認だけでなく，変更対象ファイルのソース全体も改めて確認し，既存不具合・新規不具合の有無をチェックする対応とする．`claude -p`には対象issue番号・リポジトリを渡し，必要なら`claude -p`自身が`gh issue view`でissue詳細を取得できるようにする．

**`--allowedTools`だけでは不十分**．このリポジトリの`.claude/settings.json`が許可しているツール(`Edit`・`Write`・`git commit`・`git push`・`gh pr create`等)は，`--allowedTools`に含めなくても素通りしてしまう(settings.jsonの許可は`--allowedTools`と加算的に効くため)．レビュー用の`claude -p`には，settings.jsonが許可している項目のうち意図的に残す3つ(`Bash(git status:*)`・`Bash(git diff:*)`・`Bash(gh issue view:*)`)を除く全てを`--disallowedTools`で明示的に禁止すること．`claude -p`自身の再帰呼び出しも禁止対象に含める．

- 今回の変更で新たに持ち込んだ不具合は，このissue対応の中で修正する
- 今回の変更とは無関係に以前から存在する不具合を見つけた場合は，このissue対応では修正せず，`claude -p`自身に`issue-create`スキルを起動して別issueを起票してもらう．`claude -p`が起動できるスキルは`issue-create`のみに限定し，`issue-resolve`自身を含む他のスキルは呼び出せないようにする．`issue-create`は起動元プロンプトに非対話的な自動レビューループからの起動である旨が明示されていないと承認待ちの挙動になるため，`claude -p`に渡すプロンプト文言には「`issue-resolve`の自動レビューループから非対話的に起動している (人間は介在しない)．不具合を見つけた場合は承認を待たずそのまま`issue-create`スキルで起票してよい．深い検証は不要で，「〜の可能性がある」程度の疑いでも起票してよい」という趣旨を必ず含める

一回のレビュー内容のうち，指摘事項一つずつについてコミットを行う．プッシュについてはまとめて行ってよい．  
レビュー指摘への対応方針は，末尾の「レビュー指摘への向き合い方」を参照(AIレビュー・人間レビューの両方に共通する原則)．  
レビュー結果の修正が他のプロジェクトにも及ぶ場合は，同じissueに対応する設定のブランチを切ってから対応する．  

```
claude -p --allowedTools "Read,Grep,Glob,Skill(issue-create),Bash(git status:*),Bash(git diff:*),Bash(gh issue view:*)" --disallowedTools "Edit,Write,EnterWorktree,ExitWorktree,Bash(git add:*),Bash(git commit:*),Bash(git push:*),Bash(git checkout:*),Bash(git pull),Bash(git branch -d:*),Bash(git branch --show-current),Bash(git branch),Bash(git log:*),Bash(git show:*),Bash(git remote -v),Bash(gh pr create:*),Bash(gh api repos/*/pulls/*/comments),Bash(gh api repos/*/pulls/*/comments:*),Bash(gh api repos/*/pulls/*/comments/*/replies:*),Bash(gh api repos/*/issues/*/comments),Bash(gh api repos/*/issues/*/comments:*),Bash(gh issue create:*),Bash(claude -p:*)" -- "issue #<番号>(<owner>/<repo>)の対応について，git diffとソース全体のレビューを依頼する文言" > review.md
```
```
git add <変更したファイル>
git commit -m "<コミットメッセージ>"
git push
```

### 4.5 人間レビューへの対応

Draftの解除はユーザーが手動で行う (AIは行わない)．  

ユーザーがPRにレビューコメントを追加した場合は取得して確認し，指摘があれば修正してコミット・pushする．対応方針は「レビュー指摘への向き合い方」(末尾)を参照．

```
gh api repos/<owner>/<repo>/pulls/<PR番号>/comments
gh api repos/<owner>/<repo>/issues/<PR番号>/comments
```
```
git add <変更したファイル>
git commit -m "<コミットメッセージ>"
git push
```

人間レビューへの対応が終わったら，**4.4の自動レビューループを再度実行する**．人間からの指摘で行った修正が新たな不具合を持ち込んでいないか，`claude -p`によるレビューで確認する．

## 5. マージ

マージはユーザー自身が行う (共有状態を変更する操作のため，スキルは代行しない)．  

## 6. マージ後の後始末

マージ完了の報告を受けたら，issueがクローズされたことを確認する．

```
gh issue view <番号> --repo <owner>/<repo>
```

**Vivadoプロジェクトを含むリポジトリの場合**(4.1でworktreeを使わなかった場合)，ローカルを最新化してローカルブランチを削除する．

```
git checkout <デフォルトブランチ>
git pull
git branch -d fix/issue-<番号>-<内容を表す短い語句>
```

**それ以外の場合**(4.1で`EnterWorktree`を使った場合)，上記の代わりに`ExitWorktree`(`remove`)で作業ディレクトリとブランチをまとめて削除する(元のディレクトリに自動的に戻るため，`git checkout`は不要)．

# レビュー指摘への向き合い方

AIレビュー(`claude -p`)・人間レビューのいずれであっても，指摘を鵜呑みにしてそのまま従うのではなく，**本当に対応すべきか，このまま変更しない方が正しいのではないかを都度考えてから，対応する・しないを判断する．**

- 対応する必要がないと判断した指摘は，対応しなくてよい
- 指摘に対して反論する，あるいは意図的に対応しない場合も認められる
- ただし，指摘を無視した場合・反論した場合は，その理由を必ずPRコメントまたはコミットメッセージまたはその両方に残す(黙って無視しない)

# 注意

- 人間の確認が必須なのは，手順3(修正方針の承認)と，破壊的操作(force push，reset --hard等)のみ．それ以外(ブランチ作成・実装・コミット・push・PR作成・レビューループ・レビューコメント対応)は連続して自動実行する
- 1issueに対して複数のPRがある場合，closeキーワードを持つPRは1つだけにする
- 影響リポジトリが不明な場合は，ユーザーに確認する
- マージは決して行わない
