"""
Helper script: creates a Hebrew RTL docx from the bundled template.

KEY INSIGHT from reverse-engineering real Hebrew Word documents:
- Word does NOT use RLM characters for punctuation
- Word puts ALL text including punctuation in a SINGLE run with <w:rtl/>
- Splitting text into multiple runs causes punctuation to misalign
- The fix: never split text, always use one run per paragraph
"""
import zipfile, os, re, sys, shutil

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TEMPLATE_PATH = os.path.join(SCRIPT_DIR, 'hebrew_template.docx')

def build_hebrew_docx(paragraphs_xml, output_path, template_path=None):
    tpl = template_path or TEMPLATE_PATH
    work_dir = '/tmp/heb_work'
    if os.path.exists(work_dir):
        shutil.rmtree(work_dir)
    os.makedirs(work_dir)
    with zipfile.ZipFile(tpl, 'r') as z:
        z.extractall(work_dir)
    orig = open(os.path.join(work_dir, 'word/document.xml'), encoding='utf-8').read()
    ns_part = orig[:orig.index('<w:body>')]
    sectPr = re.search(r'<w:sectPr.*?</w:sectPr>', orig, re.DOTALL).group()
    body = '\n'.join(paragraphs_xml)
    new_doc = f'{ns_part}<w:body>{body}\n{sectPr}</w:body></w:document>'
    open(os.path.join(work_dir, 'word/document.xml'), 'w', encoding='utf-8').write(new_doc)
    if 'numId' in body and not os.path.exists(os.path.join(work_dir, 'word/numbering.xml')):
        _add_numbering(work_dir)
    with zipfile.ZipFile(output_path, 'w', zipfile.ZIP_DEFLATED) as zout:
        for root, dirs, files in os.walk(work_dir):
            for file in files:
                fp = os.path.join(root, file)
                zout.write(fp, os.path.relpath(fp, work_dir))

def _add_numbering(work_dir):
    numbering = '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:numbering xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:abstractNum w:abstractNumId="1">
    <w:multiLevelType w:val="hybridMultilevel"/>
    <w:lvl w:ilvl="0">
      <w:start w:val="1"/><w:numFmt w:val="bullet"/>
      <w:lvlText w:val="&#x2022;"/>
      <w:lvlJc w:val="right"/>
      <w:pPr><w:bidi/><w:ind w:right="720" w:hanging="360"/></w:pPr>
      <w:rPr><w:rFonts w:hint="cs"/><w:rtl/></w:rPr>
    </w:lvl>
  </w:abstractNum>
  <w:num w:numId="1"><w:abstractNumId w:val="1"/></w:num>
</w:numbering>'''
    open(os.path.join(work_dir, 'word/numbering.xml'), 'w', encoding='utf-8').write(numbering)
    rels_path = os.path.join(work_dir, 'word/_rels/document.xml.rels')
    rels = open(rels_path).read()
    if 'numbering' not in rels:
        rels = rels.replace('</Relationships>',
            '<Relationship Id="rId99" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering" Target="numbering.xml"/></Relationships>')
        open(rels_path, 'w').write(rels)
    ct_path = os.path.join(work_dir, '[Content_Types].xml')
    ct = open(ct_path).read()
    if 'numbering' not in ct:
        ct = ct.replace('</Types>',
            '<Override PartName="/word/numbering.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml"/></Types>')
        open(ct_path, 'w').write(ct)

def _make_rPr(bold=False, size=24, color=None):
    """Build run properties — always include cs font and rtl."""
    rPr = '<w:rFonts w:hint="cs"/>'
    if bold: rPr += '<w:b/><w:bCs/>'
    if size: rPr += f'<w:sz w:val="{size}"/><w:szCs w:val="{size}"/>'
    if color: rPr += f'<w:color w:val="{color}"/>'
    rPr += '<w:rtl/>'
    return rPr

def _make_run(text, bold=False, size=24, color=None):
    """
    Single run containing ALL text including punctuation.
    Never split punctuation into a separate run — that causes misalignment.
    """
    rPr = _make_rPr(bold, size, color)
    t_attr = ' xml:space="preserve"' if ' ' in text else ''
    return f'<w:r><w:rPr>{rPr}</w:rPr><w:t{t_attr}>{text}</w:t></w:r>'

def heb_para(text, bold=False, size=24, color=None, space_before=None, space_after=None):
    """Standard Hebrew RTL paragraph. Keep all text in one run."""
    spacing = ''
    if space_before or space_after:
        sp = '<w:spacing'
        if space_before: sp += f' w:before="{space_before}"'
        if space_after: sp += f' w:after="{space_after}"'
        spacing = sp + '/>'
    run = _make_run(text, bold, size, color)
    return (f'<w:p><w:pPr><w:bidi/>{spacing}<w:rPr><w:rtl/></w:rPr></w:pPr>'
            f'{run}</w:p>')

def heb_bullet(text, size=24, space_after=80):
    """Hebrew RTL bullet point."""
    run = _make_run(text, size=size)
    return (f'<w:p><w:pPr>'
            f'<w:numPr><w:ilvl w:val="0"/><w:numId w:val="1"/></w:numPr>'
            f'<w:bidi/><w:spacing w:after="{space_after}"/>'
            f'<w:rPr><w:rtl/></w:rPr></w:pPr>'
            f'{run}</w:p>')

def heb_table(rows_data, col_widths=None, header_color='1F497D'):
    """
    RTL table. First item in each row = rightmost column.
    rows_data: list of lists, first row = header.
    """
    n_cols = len(rows_data[0])
    if not col_widths:
        w = 8640 // n_cols
        col_widths = [w] * n_cols
    total_w = sum(col_widths)
    col_widths_xml = ''.join(f'<w:gridCol w:w="{w}"/>' for w in col_widths)

    rows_xml = ''
    for i, row in enumerate(rows_data):
        is_header = (i == 0)
        cells_xml = ''
        for j, cell_text in enumerate(row):
            bg = header_color if is_header else None
            shading = f'<w:shd w:val="clear" w:color="auto" w:fill="{bg}"/>' if bg else ''
            text_color = 'FFFFFF' if is_header else '000000'
            w_width = col_widths[j] if j < len(col_widths) else col_widths[-1]
            run = _make_run(cell_text, bold=is_header, size=24, color=text_color)
            cells_xml += (
                f'<w:tc><w:tcPr><w:tcW w:w="{w_width}" w:type="dxa"/>{shading}</w:tcPr>'
                f'<w:p><w:pPr><w:bidi/><w:rPr><w:rtl/></w:rPr></w:pPr>'
                f'{run}</w:p></w:tc>'
            )
        rows_xml += f'<w:tr>{cells_xml}</w:tr>'

    return (
        f'<w:tbl>'
        f'<w:tblPr>'
        f'<w:tblW w:w="{total_w}" w:type="dxa"/>'
        f'<w:bidiVisual/>'
        f'<w:tblBorders>'
        f'<w:top w:val="single" w:sz="4" w:color="CCCCCC"/>'
        f'<w:left w:val="single" w:sz="4" w:color="CCCCCC"/>'
        f'<w:bottom w:val="single" w:sz="4" w:color="CCCCCC"/>'
        f'<w:right w:val="single" w:sz="4" w:color="CCCCCC"/>'
        f'<w:insideH w:val="single" w:sz="4" w:color="CCCCCC"/>'
        f'<w:insideV w:val="single" w:sz="4" w:color="CCCCCC"/>'
        f'</w:tblBorders>'
        f'</w:tblPr>'
        f'<w:tblGrid>{col_widths_xml}</w:tblGrid>'
        f'{rows_xml}'
        f'</w:tbl>'
    )

def heb_empty():
    return '<w:p><w:pPr><w:bidi/><w:rPr><w:rtl/></w:rPr></w:pPr></w:p>'
