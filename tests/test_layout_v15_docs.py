from pathlib import Path
r=Path(__file__).resolve().parents[1]
h=(r/'src/home_screen.cpp').read_text()
for text in ['gbavocab v1.6.0-pre.1','Up: abc    Right: hij','Down: nop  Left: tuw','Up: def    Right: klm','Down: qrs  Left: xyz','Hold Right, tap R twice: g.','Hold Left, tap R twice: v.']:
    assert text in h, text
m=(r/'docs/full-controls.md').read_text()
for text in ['gbavocab v1.6.0-pre.1','Up: a / b / c','Right: h / i / j','Down: n / o / p','Left: t / u / w','L + Up: d / e / f','L + Right: k / l / m','L + Down: q / r / s','L + Left: x / y / z','keep Right held and tap R twice','keep Left held and tap R twice','sample file.txt']:
    assert text in m,text
print('PASS V1.5 title, help, complete manual layout and standalone sample instructions')
