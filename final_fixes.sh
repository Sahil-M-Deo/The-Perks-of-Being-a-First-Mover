#!/usr/bin/env bash
set -e
[ -f 02.tex ] || { echo "Run this from the repo root (where 02.tex is)"; exit 1; }
python3 - <<'EOF'
s = open('02.tex').read()
s = s.replace('increase with faster time controls', 'increase with slower time controls')
s = s.replace(r'\usepackage[sc]{mathpazo}', r'\usepackage{mathptmx}')
old = r'\hspace{1em}Sahil & \url{https://github.com/Sahil-Deo-IITB} \quad earliest commit: November 3, 2026 \\'
new = (r'\hspace{1em}Sahil & \url{https://github.com/Sahil-M-Deo} \quad earliest commit: October 17, 2026 \\' + '\n'
       r' & \url{https://github.com/Sahil-Deo-IITB} \quad earliest commit: November 3, 2026 \\')
if 'Sahil-M-Deo' not in s:
    assert old in s, 'Sahil row not found'
    s = s.replace(old, new)
s = s.replace(r'\quad earliest commit: ', r'\quad ')
sess = r'\item \url{https://claude.ai/code/session_01PXyFhprEmSRAuFAiQ7o4je}'
anchor = r'\item \url{https://claude.ai/share/1bb9222d-6e23-4b2d-8803-6cdff0f0e31c}'
if sess not in s:
    s = s.replace(anchor, anchor + '\n' + sess)
india = r'''\subsection*{India Focus}
Chess originated in India as \emph{chaturanga}, and India today has one of the world's largest
and fastest-growing chess communities, much of it playing online on platforms like Lichess.
Lichess records carry no nationality, so our sample is global, and the results describe the
online rating pools Indian players compete in.

'''
if 'India Focus' not in s:
    s = s.replace(r'\subsection*{Challenges}', india + r'\subsection*{Challenges}', 1)
s = s.replace('\\newpage\n\n\\subsection*{Results}', '\\subsection*{Results}', 1)
s = s.replace('\\subsection*{Does the advantage vary with time control? (H3)}\n', '')
open('02.tex', 'w').write(s)
r = open('README.md').read()
r = r.replace('All of it is explained with worked examples in `reports/final/Theory.pdf`.',
              'The method is explained in Section 2 of `02.pdf`.')
r = r.replace('../../reports/final/report_numbers.tex', '../../report_numbers.tex')
open('README.md', 'w').write(r)
print('Edits done')
EOF
git rm --cached -q --ignore-unmatch 02.fdb_latexmk 02.fls 02.synctex.gz
git rm -r --cached -q --ignore-unmatch scripts/__pycache__
grep -q '__pycache__' .gitignore || printf '\n# build leftovers\n*.fdb_latexmk\n*.fls\n*.synctex.gz\n__pycache__/\n*.pyc\n' >> .gitignore
pdflatex -interaction=nonstopmode 02.tex > /dev/null || true
pdflatex -interaction=nonstopmode 02.tex > /dev/null || true
grep -m3 '^!' 02.log || echo "LaTeX: no errors"
pdfinfo 02.pdf | grep Pages
