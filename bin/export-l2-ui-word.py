#!/usr/bin/env python3
"""Prepare a local Unicode HTML intermediate for native Word export."""
from pathlib import Path
import hashlib,html,json,re,shutil
import pymupdf
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/l2-ui-requirements.md'
OUT=ROOT/'out/l2-debug/doc-export'
IMAGES=OUT/'images'

def inline(text):
    value=html.escape(text,quote=False)
    value=value.replace('&lt;br&gt;','<br>').replace('&lt;br/&gt;','<br>')
    value=re.sub(r'\*\*(.+?)\*\*',r'<b>\1</b>',value)
    value=re.sub(r'`([^`]+)`',r'<span>\1</span>',value)
    def link(m):
        label,target=m.groups()
        return '<a href="'+html.escape(target,quote=True)+'">'+label+'</a>' if target.startswith('#') else label
    return re.sub(r'\[([^\]]+)\]\(([^)]+)\)',link,value)

def split_row(line):
    return [part.strip() for part in line.strip().strip('|').split('|')]

def main():
    IMAGES.mkdir(parents=True,exist_ok=True)
    source=SOURCE.read_text(encoding='utf-8');lines=source.splitlines();blocks=[];images=[];tables=[]
    i=0;list_open=False
    while i<len(lines):
        line=lines[i].strip()
        if not line:
            if list_open:blocks.append('</ul>');list_open=False
            i+=1;continue
        if line.startswith('```'):
            fence=line[3:];content=[];i+=1
            while i<len(lines) and not lines[i].strip().startswith('```'):
                content.append(lines[i]);i+=1
            i+=1
            if fence=='mermaid':
                blocks.append('<div class="flows"><p><b>页面跳转关系</b></p>')
                for step in content:
                    if '-->' in step:
                        blocks.append('<p>'+inline(step.strip().replace('-->',' → '))+'</p>')
                blocks.append('</div>')
            else:blocks.append('<p>'+inline(' '.join(content))+'</p>')
            continue
        image=re.fullmatch(r'!\[([^\]]*)\]\(([^)]+)\)',line)
        if image:
            alt,target=image.groups();p=(SOURCE.parent/target).resolve();dest=IMAGES/(p.stem+'.png')
            if p.suffix.lower()=='.svg':
                d=pymupdf.open(stream=p.read_bytes(),filetype='svg')
                d[0].get_pixmap(matrix=pymupdf.Matrix(3,3),alpha=False).save(dest);d.close()
            else:shutil.copyfile(p,dest)
            images.append({'source':target,'file':str(dest.relative_to(OUT)),'alt':alt,'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()})
            blocks.append('<p class="image"><img src="'+dest.as_uri()+'" width="240" alt="'+html.escape(alt,quote=True)+'"></p>')
            i+=1;continue
        if line.startswith('<a id='):
            anchor=re.search('id="([^"]+)"',line)
            if anchor:blocks.append('<a name="'+anchor[1]+'"></a>')
            i+=1;continue
        if line.startswith('|') and i+1<len(lines) and re.match(r'^\|[\s:|\-]+\|$',lines[i+1].strip()):
            headers=split_row(line);rows=[];i+=2
            while i<len(lines) and lines[i].strip().startswith('|'):
                row=split_row(lines[i]);assert len(row)==len(headers),(len(row),headers,row)
                rows.append(row);i+=1
            widths={2:[23,77],3:[20,31,49],4:[17,23,30,30],5:[14,8,18,14,46]}.get(len(headers),[100/len(headers)]*len(headers))
            blocks.append('<table class="spec" width="100%" cellspacing="0" cellpadding="5" style="table-layout:fixed"><colgroup>'+''.join('<col style="width:'+str(w)+'%">' for w in widths)+'</colgroup><thead><tr>'+''.join('<th>'+inline(c)+'</th>' for c in headers)+'</tr></thead><tbody>')
            for row in rows:blocks.append('<tr>'+''.join('<td>'+inline(c)+'</td>' for c in row)+'</tr>')
            blocks.append('</tbody></table><p class="after-table"></p>')
            tables.append({'headers':headers,'rows':len(rows),'cells':[headers]+rows})
            continue
        heading=re.match(r'^(#{1,6})\s+(.*)$',line)
        if heading:
            level,title=len(heading[1]),heading[2]
            page=bool(level==3 and re.match(r'^\d\d\s',title))
            blocks.append('<h'+str(level)+(' style="page-break-before:always"' if page else '')+'>'+inline(title)+'</h'+str(level)+'>');i+=1;continue
        if line.startswith('- '):
            if not list_open:blocks.append('<ul>');list_open=True
            blocks.append('<li>'+inline(line[2:])+'</li>');i+=1;continue
        if line.startswith('>'):
            if line[1:].strip():blocks.append('<p class="note">'+inline(line[1:].strip())+'</p>')
            i+=1;continue
        blocks.append('<p>'+inline(line)+'</p>');i+=1
    if list_open:blocks.append('</ul>')
    css='''
    @page {size:A4 landscape;margin:10mm;}
    body {font-family:Arial,"Microsoft YaHei",SimSun;font-size:10pt;color:#152127;line-height:1.3;}
    h1 {font-size:23pt;color:#203e30;} h2 {font-size:16pt;color:#203e30;margin-top:16pt;}
    h3 {font-size:13pt;color:#203e30;margin-top:12pt;} h1,h2,h3 {page-break-after:avoid;}
    p {margin:4pt 0;} a {color:#245b43;text-decoration:none;}
    .note {font-size:9pt;color:#526167;} .image {text-align:center;margin:6pt 0;}
    table.spec {border-collapse:collapse;margin:6pt 0;width:100%;}
    .spec td,.spec th {border:0.6pt solid #aab6bb;padding:5pt;vertical-align:top;word-break:normal;font-size:9pt;font-family:Arial,"Microsoft YaHei",SimSun;}
    .spec th {background:#dce8e1;font-weight:bold;color:#153c2b;}
    tr {page-break-inside:avoid;} thead {display:table-header-group;}
    .flows p {font-size:10pt;} .after-table {font-size:2pt;line-height:2pt;}
    '''
    content='<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta http-equiv="Content-Type" content="text/html; charset=utf-8"><title>L2 UI 产品讨论稿</title><style>'+css+'</style></head><body>'+''.join(blocks)+'</body></html>'
    (OUT/'word-source.html').write_text(content,encoding='utf-8-sig')
    metadata={'source':str(SOURCE.relative_to(ROOT)),'source_sha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),'image_count':len(images),'table_count':len(tables),'images':images,'tables':tables}
    (OUT/'source-manifest.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'html':str(OUT/'word-source.html'),'images':len(images),'tables':len(tables)},ensure_ascii=False))
if __name__=='__main__':main()
