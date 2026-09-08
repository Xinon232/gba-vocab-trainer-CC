#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
EVIDENCE=${1:-/home/halim/gba-suite-release/gbavocab-fat-native}
mkdir -p "$EVIDENCE"
RUN=$(mktemp -d "$EVIDENCE/run-XXXXXX")
printf 'Evidence: %s\n' "$RUN"
exec > >(tee "$RUN/run.log") 2>&1
set -x
for tool in g++ gcc mkfs.fat fsck.fat; do command -v "$tool"; done
mkdir -p "$RUN/source/src" "$RUN/source/tests"
cp -a "$ROOT/include" "$RUN/source/"
for s in vocab_file_io.cpp vocab.cpp writer_core.cpp ff.c ffunicode.c; do cp "$ROOT/src/$s" "$RUN/source/src/"; done
cp "$ROOT/tests/test_native_deferred.cpp" "$RUN/source/tests/"
cp "$ROOT/tests/run_native_deferred.sh" "$RUN/runner.sh"
(cd "$RUN/source"; sha256sum src/* include/*.h include/fatfs/* tests/*) > "$RUN/source.sha256"
CFLAGS=(-O2 -g -ffunction-sections -fdata-sections -I"$RUN/source/include" -I"$RUN/source/include/fatfs")
gcc "${CFLAGS[@]}" -c "$RUN/source/src/ff.c" -o "$RUN/ff.o"
gcc "${CFLAGS[@]}" -c "$RUN/source/src/ffunicode.c" -o "$RUN/ffunicode.o"
g++ -std=c++17 "${CFLAGS[@]}" -DVOCAB_HOST_FATFS -DVOCAB_NO_DEMOS -DVOCAB_ROOT_DIRECTORY "$RUN/source/src/vocab_file_io.cpp" "$RUN/source/src/vocab.cpp" "$RUN/source/src/writer_core.cpp" "$RUN/source/tests/test_native_deferred.cpp" "$RUN/ff.o" "$RUN/ffunicode.o" -Wl,--gc-sections -o "$RUN/native-deferred"
truncate -s 16M "$RUN/base.img"
mkfs.fat -F 16 "$RUN/base.img"
python3 -c 'import sys; f=open(sys.argv[1],"wb"); f.write(b"\r\n\r\n".join(b"\r\n".join(("word%d\ttranslation%d\textra"%(i,i)).encode() for i in range(b*16,(b+1)*16)) for b in range(5))); f.close()' "$RUN/cards.txt"
mmd -i "$RUN/base.img" ::/gbavocab
mcopy -i "$RUN/base.img" "$RUN/cards.txt" ::/gbavocab/cards.txt
set +x
printf 'case\ttest_exit\tfsck_exit\n' > "$RUN/results.tsv"
run_case() {
 local name=$1 mode=$2 nth=$3 rc=0 fc=0
 cp "$RUN/base.img" "$RUN/$name.img"
 "$RUN/native-deferred" "$RUN/$name.img" "$mode" "$nth" > "$RUN/$name.log" 2>&1 || rc=$?
 sha256sum "$RUN/$name.img" > "$RUN/$name.before-fsck.sha256"
 fsck.fat -n "$RUN/$name.img" > "$RUN/$name.fsck.log" 2>&1 || fc=$?
 sha256sum "$RUN/$name.img" > "$RUN/$name.after-fsck.sha256"
 cmp "$RUN/$name.before-fsck.sha256" "$RUN/$name.after-fsck.sha256" || fc=99
 printf '%s\t%s\t%s\n' "$name" "$rc" "$fc" | tee -a "$RUN/results.tsv"
}
run_case baseline none 0
if python3 -c 'import csv,sys; r=list(csv.DictReader(open(sys.argv[1]),delimiter="\t"));sys.exit(int(r[-1]["test_exit"]))' "$RUN/results.tsv"; then
 read -r NR NW < <(python3 -c 'import re,sys; s=open(sys.argv[1]).read(); m=re.search(r"SAVE .* reads=(\d+) writes=(\d+)",s);print(*m.groups())' "$RUN/baseline.log")
 for ((n=1;n<=NR;n++)); do run_case "read-$n" read "$n"; done
 for ((n=1;n<=NW;n++)); do run_case "write-$n" write "$n"; done
fi
python3 - "$ROOT" "$RUN" <<'PY'
import pathlib,hashlib,sys,json,csv
root,run=map(pathlib.Path,sys.argv[1:]); source=run/'source'
changed=[]
for p in source.rglob('*'):
 if p.is_file() and (root/p.relative_to(source)).exists():
  if p.read_bytes()!=(root/p.relative_to(source)).read_bytes():changed.append(str(p.relative_to(source)))
rows=list(csv.DictReader((run/'results.tsv').open(),delimiter='\t'))
summary={'cases':len(rows),'test_failures':[r['case'] for r in rows if r['test_exit']!='0'],'fsck_nonzero':[r['case'] for r in rows if r['fsck_exit']!='0'],'production_changed':changed}
(run/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
PY
printf 'Evidence complete: %s\n' "$RUN"
python3 -c 'import json,sys;s=json.load(open(sys.argv[1]));sys.exit(bool(s["test_failures"] or s["fsck_nonzero"] or s["production_changed"]))' "$RUN/summary.json"
