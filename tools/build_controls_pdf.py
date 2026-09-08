#!/usr/bin/env python3
"""Build the current-controls PDF from docs/full-controls.md.

Requires reportlab and PyMuPDF (fitz). Example:
  python3 tools/build_controls_pdf.py --qa-dir ../gbavocab-pdf-evidence
Only writes the requested PDF and optionally prefixed QA evidence outside source.
"""
from __future__ import annotations

import argparse
import hashlib
import html
import json
from pathlib import Path
import re
from typing import Any

from reportlab import rl_config
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import SimpleDocTemplate, Paragraph, PageBreak

ROOT = Path(__file__).resolve().parents[1]
AUDIT_SOURCES = (
    'src/main.cpp', 'src/home_screen.cpp', 'src/entry_screen.cpp',
    'src/entry_editor.cpp', 'src/entry_render.cpp', 'src/writer_core.cpp',
    'src/writer_layout.cpp', 'src/state.cpp', 'src/vocab_file_io.cpp',
    'src/vocab.cpp', 'src/render.cpp', 'include/writer_core.h',
    'include/entry_editor.h', 'include/entry_shortcuts.h', 'include/state.h',
    'include/vocab_file_io.h', 'include/home_screen.h', 'include/writer_layout.h',
    'Makefile', 'src/entry_font.c', 'src/entry_font_pack.s',
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inline(text: str) -> str:
    text = html.escape(text)
    text = re.sub(r'`([^`]+)`', r'<font name="Mono">\1</font>', text)
    text = re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', text)
    return text


def build(source: Path, output: Path, qa: Path | None) -> dict:
    rl_config.invariant = 1
    fonts = Path('/usr/share/fonts/truetype/dejavu')
    for name, filename in [('Body', 'DejaVuSans.ttf'), ('BodyBold', 'DejaVuSans-Bold.ttf'),
                           ('Mono', 'DejaVuSansMono.ttf')]:
        pdfmetrics.registerFont(TTFont(name, str(fonts / filename)))
    pdfmetrics.registerFontFamily('Body', normal='Body', bold='BodyBold', italic='Body', boldItalic='BodyBold')
    base: dict[str, Any] = dict(fontName='Body', fontSize=10.2, leading=14, textColor=colors.HexColor('#202b36'), alignment=TA_LEFT)
    styles = {
        'body': ParagraphStyle('body', **base, spaceAfter=7),
        'bullet': ParagraphStyle('bullet', **base, leftIndent=10, firstLineIndent=-8, spaceAfter=3),
        'title': ParagraphStyle('title', fontName='BodyBold', fontSize=22, leading=27, spaceAfter=12, textColor=colors.HexColor('#133d59')),
        'h2': ParagraphStyle('h2', fontName='BodyBold', fontSize=16, leading=21, spaceBefore=5, spaceAfter=10, keepWithNext=True, textColor=colors.HexColor('#133d59')),
        'h3': ParagraphStyle('h3', fontName='BodyBold', fontSize=11.5, leading=16, spaceBefore=6, spaceAfter=6, keepWithNext=True, textColor=colors.HexColor('#133d59')),
    }
    story = []
    for block in source.read_text().strip().split('\n\n'):
        if block == '<!-- PAGEBREAK -->':
            story.append(PageBreak())
        elif block.startswith('# '):
            story.append(Paragraph(inline(block[2:]), styles['title']))
        elif block.startswith('## '):
            story.append(Paragraph(inline(block[3:]), styles['h2']))
        elif block.startswith('### '):
            story.append(Paragraph(inline(block[4:]), styles['h3']))
        elif block.startswith('- '):
            for line in block.splitlines():
                if not line.startswith('- '):
                    raise ValueError('Unexpected multiline list item')
                story.append(Paragraph('• ' + inline(line[2:]), styles['bullet']))
        else:
            story.append(Paragraph(inline(block.replace('\n', ' ')), styles['body']))
    output.parent.mkdir(parents=True, exist_ok=True)
    doc = SimpleDocTemplate(str(output), pagesize=A4, rightMargin=42, leftMargin=42,
                            topMargin=35, bottomMargin=38, title='gbavocab — Full controls',
                            author='Halim Jarrar', subject='Complete current controls, typing and TXT persistence')

    def footer(canvas, document):
        canvas.saveState()
        canvas.setStrokeColor(colors.HexColor('#c8d7e1'))
        canvas.line(42, 29, A4[0]-42, 29)
        canvas.setFont('Body', 8)
        canvas.setFillColor(colors.HexColor('#4d6475'))
        canvas.drawString(42, 17, 'gbavocab V1.0  |  Halim Jarrar')
        canvas.drawRightString(A4[0]-42, 17, str(document.page))
        canvas.restoreState()

    doc.build(story, onFirstPage=footer, onLaterPages=footer)
    import pymupdf
    pdf = pymupdf.open(output)
    expected = source.read_text().count('<!-- PAGEBREAK -->') + 1
    texts = [page.get_text() for page in pdf]
    report = {'output': str(output.resolve()), 'sha256': sha(output), 'pages': len(pdf),
              'expected_pages': expected, 'metadata': pdf.metadata,
              'body_font_pt': 10.2, 'leading_pt': 14, 'render_dpi': 150,
              'page_text_lengths': [len(t) for t in texts], 'bounds_errors': [],
              'sources': {p: sha(ROOT / p) for p in AUDIT_SOURCES},
              'documents': {p: sha(ROOT / p) for p in ('README.md', 'docs/entry-editor.md', 'docs/full-controls.md', 'tools/build_controls_pdf.py')}}
    for i, page in enumerate(pdf):
        for block in page.get_text('blocks'):
            if block[0] < 35 or block[1] < 25 or block[2] > A4[0]-35 or block[3] > A4[1]-10:
                # Footer is intentionally below the main content frame.
                if not (block[1] > A4[1]-30):
                    report['bounds_errors'].append({'page': i+1, 'block': block[:4]})
    if qa:
        qa.mkdir(parents=True, exist_ok=True)
        for i, page in enumerate(pdf):
            page.get_pixmap(dpi=150).save(qa / f'page-{i+1:02d}.png')
        (qa / 'pdf-text.txt').write_text('\n\n'.join(f'PAGE {i+1}\n{t}' for i, t in enumerate(texts)))
        (qa / 'build-report.json').write_text(json.dumps(report, indent=2, ensure_ascii=False)+'\n')
        for p in AUDIT_SOURCES:
            destination = qa / 'audit-sources' / p
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes((ROOT / p).read_bytes())
    assert len(pdf) == expected, f'Pagination overflow: {len(pdf)} pages, expected {expected}'
    assert not report['bounds_errors'], report['bounds_errors']
    assert pdf.metadata['author'] == 'Halim Jarrar'
    assert all(len(t)>500 for t in texts), 'Unexpected near-empty page'
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT/'docs/full-controls.md')
    parser.add_argument('--output', type=Path, default=ROOT.parent/'gbavocab-full-controls.pdf')
    parser.add_argument('--qa-dir', type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args.source, args.output, args.qa_dir), indent=2, ensure_ascii=False))


if __name__ == '__main__':
    main()
