---
name: hebrew-rtl
description: >
  Use this skill whenever the user requests a document, presentation, report, or any written output in Hebrew (עברית).
  Trigger on any of the following: the user writes their request in Hebrew, mentions "בעברית", asks for a מסמך, דוח, מצגת, סיכום, מכתב, or any other deliverable and the content is in Hebrew.
  This skill ensures correct RTL (right-to-left) layout, Hebrew-compatible fonts, proper punctuation placement per Hebrew keyboard conventions, and correct use of hyphens and dashes.
  Always use this skill when producing .docx, .pptx, .pdf, or HTML/web content in Hebrew — even if the user does not explicitly mention RTL or formatting. Do NOT skip this skill just because the user's request was brief.
---

# Hebrew RTL Document Skill

---

## Core Principles

### 1. Direction & Alignment
- All Hebrew text: RTL, right-aligned
- Mixed Hebrew-English: Hebrew is primary; English words flow inline via BiDi
- **Tables, info-boxes, timelines, grids**: always start from the RIGHT — first column/item on the right, last on the left
- Lists: bullets on the right side

### 2. Font
- **Arial** — most reliable for docx/pptx
- **Heebo** — preferred for HTML/web
- Always set `w:hint="cs"` (complex script) on font runs in docx

### 3. Punctuation
- Hyphen: `-` (U+002D) — **NEVER** em-dash `—` or en-dash `–`
- Colon `:`, semicolon `;`, comma `,` — standard
- Quotation marks: `"..."` — not curly quotes

### 4. Numbers and Dates
- Numbers stay LTR within RTL text (BiDi automatic)
- Dates: `DD.MM.YYYY` or `15 במרץ 2025`

---

## Word (.docx) — THE ONLY RELIABLE APPROACH

**Always use the bundled template `hebrew_template.docx` and the helper script `create_doc.py`.**

The template is a real Hebrew Word file with correct `settings.xml` and `styles.xml`.
Never use docx-js, python-docx, or any other library — they do not produce correct RTL.

### Step 1: Copy the template from the skill directory

```python
import os, sys
# The skill directory is available as SKILL_DIR
TEMPLATE = os.path.join(SKILL_DIR, 'hebrew_template.docx')
```

### Step 2: Use the helper functions from create_doc.py

```python
exec(open(os.path.join(SKILL_DIR, 'create_doc.py')).read())

# Build paragraphs
paragraphs = [
    heb_para('כותרת המסמך', bold=True, size=36, color='1F497D', space_after=80),
    heb_para('תת-כותרת', size=24, color='444444', space_after=160),
    heb_empty(),
    heb_para('כותרת פרק', bold=True, size=28, color='1F497D', space_before=160, space_after=80),
    heb_para('טקסט גוף רגיל כאן.', size=24, space_after=160),
    heb_bullet('פריט ראשון ברשימה'),
    heb_bullet('פריט שני ברשימה'),
]

# Build the document
build_hebrew_docx(paragraphs, '/mnt/user-data/outputs/output.docx', TEMPLATE)
```

### RTL XML pattern (reference):
```xml
<!-- Paragraph: ONLY <w:bidi/> — no attributes, no <w:jc w:val="right"/> -->
<w:p>
  <w:pPr>
    <w:bidi/>
    <w:rPr><w:rtl/></w:rPr>
  </w:pPr>
  <w:r>
    <w:rPr>
      <w:rFonts w:hint="cs"/>
      <w:rtl/>
    </w:rPr>
    <w:t xml:space="preserve">הטקסט כאן</w:t>
  </w:r>
</w:p>
```

### What does NOT work:
- ❌ `<w:bidi w:val="1"/>` — must be `<w:bidi/>` with no attribute
- ❌ `<w:jc w:val="right"/>` — not needed, bidi handles alignment
- ❌ docx-js or python-docx — insufficient RTL support

### RTL Bullets — add numbering.xml if needed:
```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:numbering xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:abstractNum w:abstractNumId="1">
    <w:multiLevelType w:val="hybridMultilevel"/>
    <w:lvl w:ilvl="0">
      <w:start w:val="1"/>
      <w:numFmt w:val="bullet"/>
      <w:lvlText w:val="&#x2022;"/>
      <w:lvlJc w:val="right"/>
      <w:pPr><w:bidi/><w:ind w:right="720" w:hanging="360"/></w:pPr>
      <w:rPr><w:rFonts w:hint="cs"/><w:rtl/></w:rPr>
    </w:lvl>
  </w:abstractNum>
  <w:num w:numId="1"><w:abstractNumId w:val="1"/></w:num>
</w:numbering>
```

### Tables in Hebrew docx:
- Add `<w:bidiVisual/>` inside `<w:tblPr>` — reverses column order visually
- First column in XML = rightmost column on screen

---

## PowerPoint (.pptx)

### Text boxes:
```javascript
slide.addText("הטקסט כאן", {
  fontFace: "Arial", fontSize: 24,
  align: "right", rtlMode: true
});
```

### REQUIRED post-processing for bullets/tables/timelines:
```python
import zipfile, os, re

def fix_rtl_pptx(src, dst):
    os.makedirs('/tmp/pptx_fix', exist_ok=True)
    with zipfile.ZipFile(src, 'r') as z:
        z.extractall('/tmp/pptx_fix')
    slides_dir = '/tmp/pptx_fix/ppt/slides'
    for fname in os.listdir(slides_dir):
        if fname.endswith('.xml') and fname.startswith('slide'):
            fpath = os.path.join(slides_dir, fname)
            content = open(fpath, encoding='utf-8').read()
            content = fix_slide(content)
            open(fpath, 'w', encoding='utf-8').write(content)
    with zipfile.ZipFile(dst, 'w', zipfile.ZIP_DEFLATED) as zout:
        for root, dirs, files in os.walk('/tmp/pptx_fix'):
            for file in files:
                fp = os.path.join(root, file)
                zout.write(fp, os.path.relpath(fp, '/tmp/pptx_fix'))

def fix_slide(content):
    content = re.sub(r'rtlCol="0"', 'rtlCol="1"', content)
    def fix_para_with_bullet(m):
        para = m.group(0)
        if 'buChar' not in para and 'buAutoNum' not in para:
            return para
        def fix_pPr(pm):
            ppr = pm.group(0)
            if 'rtl="1"' not in ppr:
                ppr = ppr.replace('<a:pPr ', '<a:pPr rtl="1" ')
            ppr = re.sub(r'marL="(\d+)"', lambda x: f'marR="{x.group(1)}"', ppr)
            ppr = re.sub(r' indent="-(\d+)"', lambda x: f' indent="{x.group(1)}"', ppr)
            return ppr
        para = re.sub(r'<a:pPr[^/]*/>', fix_pPr, para)
        return para
    content = re.sub(r'<a:p>.*?</a:p>', fix_para_with_bullet, content, flags=re.DOTALL)
    def fix_run_lang(m):
        run = m.group(0)
        if re.search(r'[\u0590-\u05FF]', run):
            run = re.sub(r'lang="en-US"', 'lang="he-IL"', run)
        return run
    content = re.sub(r'<a:r>.*?</a:r>', fix_run_lang, content, flags=re.DOTALL)
    return content
```

### Timelines and grids:
- First item (earliest/most important) on the RIGHT
- Build items right-to-left with decreasing x positions

---

## HTML / PDF

```html
<!DOCTYPE html>
<html lang="he" dir="rtl">
<head>
  <meta charset="UTF-8">
  <link href="https://fonts.googleapis.com/css2?family=Heebo:wght@400;700&display=swap" rel="stylesheet">
  <style>
    body { font-family: 'Heebo', Arial, sans-serif; direction: rtl; text-align: right; }
    ul, ol { padding-right: 1.5em; padding-left: 0; }
    table { direction: rtl; }
    th, td { text-align: right; }
  </style>
</head>
```

---

## Checklist

**docx:**
- [ ] Use `hebrew_template.docx` from skill directory as base
- [ ] Use `create_doc.py` helper functions
- [ ] `<w:bidi/>` (no attributes) on every paragraph
- [ ] `<w:rFonts w:hint="cs"/>` + `<w:rtl/>` on every run
- [ ] Tables: `<w:bidiVisual/>` in tblPr
- [ ] Bullets: numbering.xml with `<w:lvlJc w:val="right"/>`

**pptx:**
- [ ] `align: "right"` + `rtlMode: true` on all text boxes
- [ ] Run `fix_rtl_pptx()` post-processing on every pptx
- [ ] Tables/grids/timelines: first item on the RIGHT

**All formats:**
- [ ] No em-dash `—` — only regular hyphen `-`
- [ ] Font is Hebrew-compatible (Arial for docx/pptx, Heebo for HTML)
