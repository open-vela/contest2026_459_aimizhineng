---
name: "word-report-generator"
description: "Generates structured Word (.docx) reports with headings, tables, code blocks and note boxes using python-docx. Invoke when user asks to generate a Word document, report, or summary in .docx format."
---

# Word Report Generator

This skill generates well-formatted Word (.docx) documents using the `python-docx` library. It is designed for technical reports, project documentation, porting reports, and knowledge summaries.

## When to Invoke

- User asks to generate a Word document / .docx file
- User asks for a "report", "summary", "documentation" in Word format
- User wants to convert markdown content to a structured Word document

## Prerequisites

- `python-docx` must be installed: `pip install python-docx`
- Python 3 available on the system

## Core Implementation Pattern

### 1. Imports and Helpers

```python
from docx import Document
from docx.shared import Pt, RGBColor, Cm
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement
import datetime

def set_cell_bg(cell, color):
    tc = cell._tc.get_or_add_tcPr()
    shd = OxmlElement('w:shd')
    shd.set(qn('w:val'), 'clear')
    shd.set(qn('w:color'), 'auto')
    shd.set(qn('w:fill'), color)
    tc.append(shd)

def H(doc, text, level=1, color=RGBColor(0x1F,0x49,0x7D)):
    """Add heading with optional color."""
    h = doc.add_heading(text, level=level)
    if color:
        for r in h.runs:
            r.font.color.rgb = color
    return h

def Code(doc, text):
    """Add a code block with gray background and Consolas font."""
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Pt(12)
    p.paragraph_format.space_before = Pt(4)
    p.paragraph_format.space_after = Pt(4)
    run = p.add_run(text)
    run.font.name = 'Consolas'
    run.font.size = Pt(9)
    run._element.rPr.rFonts.set(qn('w:eastAsia'), 'Consolas')
    pPr = p._p.get_or_add_pPr()
    shd = OxmlElement('w:shd')
    shd.set(qn('w:val'), 'clear')
    shd.set(qn('w:fill'), 'F5F5F5')
    pPr.append(shd)
    return p

def Note(doc, text, color='FFF3CD'):
    """Add a colored note box (yellow by default)."""
    t = doc.add_table(rows=1, cols=1)
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    cell = t.cell(0,0)
    set_cell_bg(cell, color)
    p = cell.paragraphs[0]
    run = p.add_run(text)
    run.font.size = Pt(10)
    run.font.name = '微软雅黑'
    run._element.rPr.rFonts.set(qn('w:eastAsia'), '微软雅黑')

def Tbl(doc, rows):
    """Add a table with header row bolded."""
    t = doc.add_table(rows=len(rows), cols=len(rows[0]))
    t.style = 'Light Grid Accent 1'
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, rd in enumerate(rows):
        for j, ct in enumerate(rd):
            c = t.cell(i,j)
            c.text = str(ct)
            for p in c.paragraphs:
                for r in p.runs:
                    r.font.size = Pt(10)
                    r.font.name = '微软雅黑'
                    r._element.rPr.rFonts.set(qn('w:eastAsia'), '微软雅黑')
                    if i == 0:
                        r.font.bold = True
    return t
```

### 2. Document Setup

```python
doc = Document()
s = doc.styles['Normal']
s.font.name = '微软雅黑'
s.font.size = Pt(11)
s._element.rPr.rFonts.set(qn('w:eastAsia'), '微软雅黑')

for sec in doc.sections:
    sec.top_margin = Cm(2.5)
    sec.bottom_margin = Cm(2.5)
    sec.left_margin = Cm(2.5)
    sec.right_margin = Cm(2.5)
```

### 3. Cover Page

```python
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_before = Pt(120)
run = p.add_run('报告标题')
run.font.size = Pt(28)
run.font.bold = True
run.font.color.rgb = RGBColor(0x1F, 0x49, 0x7D)
run.font.name = '微软雅黑'
run._element.rPr.rFonts.set(qn('w:eastAsia'), '微软雅黑')
```

### 4. Table of Contents

Generate TOC manually (no field codes):
```python
H(doc, '目录', level=1)
toc = ['一、章节一', '二、章节二']
for item in toc:
    p = doc.add_paragraph(item)
    p.paragraph_format.left_indent = Pt(18 if not item.startswith('  ') else 36)
```

### 5. Save

```python
doc.save(r'E:\path\to\output.docx')
```

## Critical Pitfalls (Must Avoid)

### Pitfall 1: Empty triple-quote code blocks
**Wrong:**
```python
Code(doc, ''
'''.strip())

Code(doc, '''
actual code
'''.strip())
```
**Right:** Delete the empty `Code(doc, '')` lines entirely.

### Pitfall 2: Chinese quotation marks inside Python strings
**Wrong:** `doc.add_paragraph('他说"你好"')` — Python 3.14 may misparse.
**Right:** Use escaped double quotes inside single-quoted strings: `'他说\"你好\"'` or use Chinese book marks `「」`.

### Pitfall 3: BOM in Python files
When editing files on Windows, ensure UTF-8 without BOM:
```python
[System.IO.File]::WriteAllText(path, content, (New-Object System.Text.UTF8Encoding $false))
```

### Pitfall 4: Nested Pt() / Cm() calls
**Wrong:** `p.paragraph_format.left_indent = Pt(18 if cond else Pt(36))`
**Right:** `p.paragraph_format.left_indent = Pt(18 if cond else 36)`

### Pitfall 5: Cm() EMU overflow
`Cm()` returns EMU values that can overflow. Use `Pt()` for indents under ~20pt.

## Workflow

1. Read user requirements (report topic, sections, audience)
2. Check `python-docx` is installed: `pip install python-docx`
3. Write the Python generation script using the helper functions above
4. **Always** run `python -c "import ast; ast.parse(open(path, encoding='utf-8').read())"` to check syntax BEFORE running
5. Run the script: `python script.py`
6. Verify output file exists with `Get-Item`
7. Delete the temporary generation script

## Style Conventions

- **Font**: 微软雅黑 for body, Consolas for code
- **Heading color**: RGB(0x1F, 0x49, 0x7D) (dark blue)
- **Code background**: #F5F5F5 (light gray)
- **Note box background**: #FFF3CD (light yellow)
- **Table style**: Light Grid Accent 1
- **Margins**: 2.5cm all sides
- **Body font size**: 11pt, code 9pt, table 10pt
- **Section breaks**: Use `doc.add_page_break()` between major chapters
