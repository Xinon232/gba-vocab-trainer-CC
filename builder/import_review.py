"""Conservative import review. Source rows never change; removals are exclusions."""
from dataclasses import dataclass
import re
import io
import zipfile
from pathlib import Path
import dictionary_builder as pack

MAX_ARCHIVE = 64 * 1024 * 1024
MAX_TEXT = 64 * 1024 * 1024
MAX_MEMBERS = 256
MAX_ROWS = 250000

class MemberSelectionRequired(ValueError):
    def __init__(self, candidates):
        super().__init__('Select one text member from the archive')
        self.candidates = candidates

def bounded_read(stream, limit):
    chunks, size = [], 0
    while True:
        chunk = stream.read(min(65536, limit - size + 1))
        if not chunk: return b''.join(chunks)
        size += len(chunk)
        if size > limit: raise ValueError('Input exceeds the configured size bound')
        chunks.append(chunk)

def load_source(path, member=None, *, max_archive=MAX_ARCHIVE, max_text=MAX_TEXT):
    path = Path(path)
    with path.open('rb') as stream:
        raw = bounded_read(stream, max_archive if path.suffix.lower() == '.zip' else max_text)
    if path.suffix.lower() == '.zip' or raw.startswith((b'PK\x03\x04', b'PK\x05\x06', b'PK\x07\x08')):
        if len(raw) > max_archive: raise ValueError('Archive exceeds size bound')
        try:
            with zipfile.ZipFile(io.BytesIO(raw)) as archive:
                infos = archive.infolist()
                if len(infos) > MAX_MEMBERS: raise ValueError('Too many ZIP members')
                candidates = [i for i in infos if not i.is_dir() and Path(i.filename).suffix.lower() in ('.txt', '.tsv')]
                names = [i.filename for i in candidates]
                if not names: raise ValueError('Archive contains no TXT/TSV members')
                if len(set(names)) != len(names): raise ValueError('Duplicate ZIP member names are ambiguous')
                if member is None:
                    if len(names) != 1: raise MemberSelectionRequired(names)
                    member = names[0]
                if member not in names: raise ValueError('Selected ZIP member is not a text candidate')
                info = candidates[names.index(member)]
                if info.flag_bits & 1: raise ValueError('Encrypted ZIP members are not supported')
                if info.file_size > max_text or info.compress_size > max_archive: raise ValueError('ZIP member exceeds size bound')
                if info.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED): raise ValueError('Unsupported ZIP compression (use stored or deflated)')
                # Never extract paths, even absolute or traversal-looking member names.
                with archive.open(info) as stream: text_bytes = bounded_read(stream, max_text)
        except (zipfile.BadZipFile, RuntimeError, NotImplementedError, EOFError) as error:
            raise ValueError('Cannot read ZIP: ' + str(error)) from error
    else:
        text_bytes = raw
    model = parse_text(text_bytes.decode('utf-8-sig'))
    model.source = path.resolve()
    model.member = member
    if not model.defaults: model.defaults = dict(name=path.stem)
    return model

@dataclass(frozen=True)
class Row:
    line: int
    pair: tuple
    metadata: tuple
    issues: tuple

    @property
    def byte_length(self):
        return len('\t'.join(self.pair).encode('utf-8'))

class Review:
    def __init__(self, rows, comments=(), defaults=None):
        self.rows = tuple(rows)
        self.comments = tuple(comments)
        self.defaults = defaults or {}
        self.excluded = set()
        self.history = []
        self.source: Path | None = None
        self.member: str | None = None
        self.format = 'dictionary'

    def exclude(self, indices):
        indices = set(indices)
        if any(not isinstance(i, int) or not 0 <= i < len(self.rows) for i in indices):
            raise ValueError('Invalid row selection')
        added = indices - self.excluded
        if added:
            self.history.append(self.excluded.copy())
            self.excluded.update(added)
        return len(added)

    def exclude_issue(self, issue):
        return self.exclude(i for i, r in enumerate(self.rows) if issue in r.issues)

    def duplicate_indices(self):
        seen, duplicates = set(), []
        for i, row in enumerate(self.rows):
            if i in self.excluded: continue
            if row.pair in seen: duplicates.append(i)
            seen.add(row.pair)
        return duplicates

    def exclude_duplicates(self):
        return self.exclude(self.duplicate_indices())

    def undo(self):
        if not self.history: return False
        self.excluded = self.history.pop()
        return True

    def reset(self):
        if self.excluded:
            self.history.append(self.excluded.copy())
            self.excluded = set()

    @property
    def counts(self):
        active = [r for i, r in enumerate(self.rows) if i not in self.excluded]
        return dict(total=len(self.rows), included=len(active), excluded=len(self.excluded),
                    invalid=sum(any(s != 'duplicate' for s in r.issues) for r in active),
                    oversized=sum('oversized' in r.issues for r in active),
                    duplicates=len(self.duplicate_indices()))

    def included_pairs(self):
        if self.counts['invalid']:
            raise ValueError('Remove included invalid rows before saving')
        pairs = [r.pair for i, r in enumerate(self.rows) if i not in self.excluded]
        if not pairs:
            raise ValueError('Empty dictionary')
        for pair in pairs:
            pack.validate_pair(pair)
        return pairs

def parse_text(text):
    rows, comments, seen = [], [], set()
    for number, line in enumerate(text.lstrip('\ufeff').split('\n'), 1):
        line = line.removesuffix('\r')
        if line.lstrip().startswith('#'):
            comments.append(line)
            continue
        if not line.strip() or all(c.isspace() or c in '=+-_|─━│┌┐└┘┼╔╗╚╝═║' for c in line):
            continue
        if '\t' not in line and re.fullmatch(r'[\s=\[\]_*+-]*(?:box|field)\s*[1-5][\s=\[\]_*+-]*', line, re.I):
            continue
        fields = line.split('\t')
        pair = tuple((fields + [''])[:2])
        issues = []
        if len(fields) < 2 or any(not f.strip() for f in pair): issues.append('missing fields')
        if any(ord(c) < 32 or ord(c) == 127 for f in pair for c in f): issues.append('invalid controls')
        if len('\t'.join(pair).encode('utf-8')) > 191: issues.append('oversized')
        if pair in seen: issues.append('duplicate')
        seen.add(pair)
        if len(rows) >= MAX_ROWS: raise ValueError('Export exceeds 250,000 row limit')
        rows.append(Row(number, pair, tuple(fields[2:]), tuple(issues)))
    if not rows: raise ValueError('Export contains no vocabulary entries')
    header = '\n'.join(comments).lower()
    defaults = {}
    if 'dict.cc' in header and ('german-spanish' in header or 'de-es' in header):
        defaults = dict(name='German-Spanish', front='de', back='es', front_label='German', back_label='Spanish')
    model = Review(rows, comments, defaults)
    model.format = 'dict.cc' if 'dict.cc' in header else 'TSV'
    return model
