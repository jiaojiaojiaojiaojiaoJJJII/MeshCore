#!/usr/bin/env python3
"""Verify every exported Word table cell and embedded picture against the source."""
from pathlib import Path
import hashlib,html,json,re,unicodedata,zipfile,xml.etree.ElementTree as ET
from html.parser import HTMLParser
import pymupdf
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/l2-debug/doc-export'
W='{http://schemas.openxmlformats.org/wordprocessingml/2006/main}'
R='{http://schemas.openxmlformats.org/package/2006/relationships}'
class Plain(HTMLParser):
    def __init__(self):super().__init__(convert_charrefs=True);self.parts=[]
    def handle_data(self,data):self.parts.append(data)

def expected_text(t):
    t=html.escape(t,quote=False).replace('&lt;br&gt;','<br>').replace('&lt;br/&gt;','<br>')
    t=re.sub(r'\*\*(.+?)\*\*',r'<b>\1</b>',t)
    t=re.sub(r'`([^`]+)`',r'\1',t)
    t=re.sub(r'\[([^\]]+)\]\(([^)]+)\)',r'\1',t)
    p=Plain();p.feed(t);return ''.join(p.parts)

def norm(t):return re.sub(r'\s+','',unicodedata.normalize('NFKC',t)).replace('\u00ad','')

def main():
    manifest=json.loads((OUT/'source-manifest.json').read_text(encoding='utf-8'))
    docx=ROOT/'docs/L2-UI-Requirements-0.5.1.docx';doc=ROOT/'docs/L2-UI-Requirements-0.5.1.doc'
    if doc.exists(): assert doc.read_bytes()[:8]==bytes.fromhex('d0cf11e0a1b11ae1'),'not a native binary DOC'
    with zipfile.ZipFile(docx) as z:
        assert z.testzip() is None
        document=ET.fromstring(z.read('word/document.xml'))
        tables=document.findall('.//'+W+'tbl');assert len(tables)==manifest['table_count'],len(tables)
        cells=0
        for n,(actual,source) in enumerate(zip(tables,manifest['tables']),1):
            rows=actual.findall(W+'tr');assert len(rows)==len(source['cells']),(n,'rows')
            assert rows[0].find(W+'trPr/'+W+'tblHeader') is not None,(n,'repeat header')
            for ri,(row,expected) in enumerate(zip(rows,source['cells']),1):
                tcs=row.findall(W+'tc');assert len(tcs)==len(expected),(n,ri,'columns')
                for ci,(tc,e) in enumerate(zip(tcs,expected),1):
                    text=''.join(t.text or '' for t in tc.iter(W+'t'))
                    assert '\ufffd' not in text,(n,ri,ci,'replacement character')
                    assert norm(text)==norm(expected_text(e)),(n,ri,ci,text[:140],expected_text(e)[:140])
                    cells+=1
        media=[name for name in z.namelist() if name.startswith('word/media/') and not name.endswith('/')]
        assert len(media)==manifest['image_count'],('images',len(media))
        external=[]
        for name in z.namelist():
            if name.endswith('.rels'):
                rels=ET.fromstring(z.read(name))
                external.extend(r.attrib for r in rels if r.get('Type','').endswith('/image') and r.get('TargetMode')=='External')
        assert not external,external
        fonts=document.findall('.//'+W+'rFonts')
        assert any(f.get(W+'eastAsia') in ['Microsoft YaHei','微软雅黑'] for f in fonts),'CJK font missing'
        assert len(document.findall('.//'+W+'drawing'))>=manifest['image_count']
    pdf=pymupdf.open(OUT/'L2-UI-Requirements-0.5.1.pdf')
    text='\n'.join(page.get_text() for page in pdf)
    assert '\ufffd' not in text,'PDF replacement characters'
    for t in ['页面组件','占比','色彩','交互方式','功能','锁屏','未读信息','首页']:
        assert t in text,('PDF text missing',t)
    chosen={}
    for n,page in enumerate(pdf):
        t=page.get_text()
        if '12 锁屏' in t and '未读信息卡片区' in t:chosen['locked']=n
        if '01 首页' in t and '大号时钟区' in t:chosen['home']=n
        if '34 正在播放' in t and '音量控制栏' in t:chosen['player']=n
    for label,n in chosen.items():pdf[n].get_pixmap(matrix=pymupdf.Matrix(1.4,1.4),alpha=False).save(OUT/('word-check-'+label+'.png'))
    result={'tables':len(tables),'verified_cells':cells,'embedded_images':len(media),'external_images':0,'pdf_pages':len(pdf),'preview_pages':chosen,'docx_sha256':hashlib.sha256(docx.read_bytes()).hexdigest(),'doc_sha256':hashlib.sha256(doc.read_bytes()).hexdigest() if doc.exists() else None}
    (OUT/'validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
