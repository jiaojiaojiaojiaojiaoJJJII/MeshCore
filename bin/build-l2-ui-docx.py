#!/usr/bin/env python3
"""Build a self-contained Unicode DOCX locally, with fixed-width CJK tables."""
from pathlib import Path
from html.parser import HTMLParser
import datetime,hashlib,json,re,struct,zipfile
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/l2-debug/doc-export'
NS={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main','r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships','wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing','a':'http://schemas.openxmlformats.org/drawingml/2006/main','pic':'http://schemas.openxmlformats.org/drawingml/2006/picture'}
for k,v in NS.items():ET.register_namespace(k,v)
def q(n):p,l=n.split(':');return '{'+NS[p]+'}'+l
def E(parent,name,attrs=None,text=None):
    node=ET.SubElement(parent,q(name),{q(k) if ':' in k else k:str(v) for k,v in (attrs or {}).items()})
    if text is not None:node.text=str(text)
    return node
def xml(node):return ET.tostring(node,encoding='utf-8',xml_declaration=True)
class Node:
    def __init__(self,tag,attrs=()):self.tag=tag;self.attrs=dict(attrs);self.children=[]
    def text(self):return ''.join(c if isinstance(c,str) else c.text() for c in self.children)
class Tree(HTMLParser):
    def __init__(self):super().__init__(convert_charrefs=True);self.root=Node('root');self.stack=[self.root]
    def handle_starttag(self,tag,attrs):
        n=Node(tag,attrs);self.stack[-1].children.append(n)
        if tag not in ['meta','col','br','img','hr','link','input']:self.stack.append(n)
    def handle_endtag(self,tag):
        for i in range(len(self.stack)-1,0,-1):
            if self.stack[i].tag==tag:self.stack=self.stack[:i];break
    def handle_data(self,data):self.stack[-1].children.append(data)
def descend(node,tag):
    result=[]
    for c in node.children:
        if not isinstance(c,str):
            if c.tag==tag:result.append(c)
            result+=descend(c,tag)
    return result
class Builder:
    def __init__(self):
        self.doc=ET.Element(q('w:document'));self.body=E(self.doc,'w:body');self.media=[];self.bookmarks=[];self.marker=0;self.table_count=0
    def run(self,parent,text='',bold=False,size=None,color=None):
        run=E(parent,'w:r');rp=E(run,'w:rPr')
        E(rp,'w:rFonts',{'w:ascii':'Arial','w:hAnsi':'Arial','w:eastAsia':'Microsoft YaHei','w:cs':'Arial'})
        E(rp,'w:lang',{'w:val':'en-US','w:eastAsia':'zh-CN'})
        if bold:E(rp,'w:b');E(rp,'w:bCs')
        if size:E(rp,'w:sz',{'w:val':size});E(rp,'w:szCs',{'w:val':size})
        if color:E(rp,'w:color',{'w:val':color})
        if text:
            t=E(run,'w:t',text=text);t.set('{http://www.w3.org/XML/1998/namespace}space','preserve')
        return run
    def attach_bookmarks(self,p):
        for name in self.bookmarks:
            self.marker+=1
            E(p,'w:bookmarkStart',{'w:id':self.marker,'w:name':re.sub('[^A-Za-z0-9_]','_',name)})
            E(p,'w:bookmarkEnd',{'w:id':self.marker})
        self.bookmarks=[]
    def inline(self,parent,node,bold=False,size=None,color=None):
        if isinstance(node,str):self.run(parent,node,bold,size,color);return
        if node.tag=='br':E(self.run(parent),'w:br');return
        if node.tag=='img':self.image(parent,node);return
        if node.tag=='a' and node.attrs.get('name'):
            self.bookmarks.append(node.attrs['name']);return
        if node.tag=='a' and node.attrs.get('href','').startswith('#'):
            link=E(parent,'w:hyperlink',{'w:anchor':re.sub('[^A-Za-z0-9_]','_',node.attrs['href'][1:])})
            for c in node.children:self.inline(link,c,bold,size,'245B43')
            return
        for c in node.children:self.inline(parent,c,bold or node.tag in ['b','strong'],size,color)
    def paragraph(self,node,parent=None,style='Normal',size=None,bold=False):
        parent=self.body if parent is None else parent
        p=E(parent,'w:p');pp=E(p,'w:pPr');E(pp,'w:pStyle',{'w:val':style})
        if node.tag=='p' and node.attrs.get('class')=='image':
            E(pp,'w:jc',{'w:val':'center'});E(pp,'w:keepNext')
        if node.tag=='h3' and re.match(r'^\d{2}\s',node.text()):E(pp,'w:pageBreakBefore')
        self.attach_bookmarks(p)
        for c in node.children:self.inline(p,c,bold,size)
        return p
    def image(self,p,node):
        from urllib.parse import urlparse,unquote
        path=unquote(urlparse(node.attrs['src']).path)
        if re.match(r'^/[A-Za-z]:',path):path=path[1:]
        path=Path(path);raw=path.read_bytes();pw,ph=struct.unpack('>II',raw[16:24])
        self.media.append((f'image{len(self.media)+1}.png',raw));idx=len(self.media)
        cx=2286000;cy=round(cx*ph/pw)
        draw=E(self.run(p),'w:drawing');inline=E(draw,'wp:inline',{'distT':0,'distB':0,'distL':0,'distR':0})
        E(inline,'wp:extent',{'cx':cx,'cy':cy});E(inline,'wp:effectExtent',{'l':0,'t':0,'r':0,'b':0})
        E(inline,'wp:docPr',{'id':idx,'name':'Picture '+str(idx),'descr':node.attrs.get('alt','')})
        locks=E(inline,'wp:cNvGraphicFramePr');E(locks,'a:graphicFrameLocks',{'noChangeAspect':1})
        graphic=E(inline,'a:graphic');data=E(graphic,'a:graphicData',{'uri':NS['pic']});pic=E(data,'pic:pic')
        nv=E(pic,'pic:nvPicPr');E(nv,'pic:cNvPr',{'id':0,'name':self.media[-1][0],'descr':node.attrs.get('alt','')});E(nv,'pic:cNvPicPr')
        fill=E(pic,'pic:blipFill');E(fill,'a:blip',{'r:embed':'rIdImage'+str(idx)});E(E(fill,'a:stretch'),'a:fillRect')
        sp=E(pic,'pic:spPr');transform=E(sp,'a:xfrm');E(transform,'a:off',{'x':0,'y':0});E(transform,'a:ext',{'cx':cx,'cy':cy});E(E(sp,'a:prstGeom',{'prst':'rect'}),'a:avLst')
    def table(self,node):
        self.table_count+=1;rows=descend(node,'tr');n=len([c for c in rows[0].children if not isinstance(c,str) and c.tag in ['td','th']])
        weights={2:[23,77],3:[20,31,49],4:[17,23,30,30],5:[14,8,18,14,46]}[n]
        total=15718;widths=[round(total*x/100) for x in weights];widths[-1]+=total-sum(widths)
        table=E(self.body,'w:tbl');pr=E(table,'w:tblPr');E(pr,'w:tblW',{'w:w':total,'w:type':'dxa'});E(pr,'w:tblLayout',{'w:type':'fixed'})
        borders=E(pr,'w:tblBorders')
        for edge in ['top','left','bottom','right','insideH','insideV']:E(borders,'w:'+edge,{'w:val':'single','w:sz':4,'w:color':'AAB6BB'})
        margins=E(pr,'w:tblCellMar')
        for edge,val in [('top',60),('bottom',60),('left',80),('right',80)]:E(margins,'w:'+edge,{'w:w':val,'w:type':'dxa'})
        grid=E(table,'w:tblGrid')
        for width in widths:E(grid,'w:gridCol',{'w:w':width})
        for ri,row in enumerate(rows):
            tr=E(table,'w:tr');trp=E(tr,'w:trPr');E(trp,'w:cantSplit')
            if ri==0:E(trp,'w:tblHeader')
            cells=[c for c in row.children if not isinstance(c,str) and c.tag in ['td','th']];assert len(cells)==n
            for ci,cell in enumerate(cells):
                tc=E(tr,'w:tc');tcp=E(tc,'w:tcPr');E(tcp,'w:tcW',{'w:w':widths[ci],'w:type':'dxa'});E(tcp,'w:vAlign',{'w:val':'top'})
                if ri==0:E(tcp,'w:shd',{'w:fill':'DCE8E1','w:val':'clear'})
                p=E(tc,'w:p');pp=E(p,'w:pPr');E(pp,'w:pStyle',{'w:val':'TableText'})
                for c in cell.children:self.inline(p,c,ri==0 or (n==5 and ci==0),18,'153C2B' if ri==0 else None)
        spacer=E(self.body,'w:p');E(E(spacer,'w:pPr'),'w:spacing',{'w:after':40,'w:before':0})
    def blocks(self,node):
        if isinstance(node,str):return
        if node.tag=='table':self.table(node);return
        if node.tag=='a' and node.attrs.get('name'):self.bookmarks.append(node.attrs['name']);return
        if node.tag in ['p','h1','h2','h3','h4','li']:
            if node.tag=='li':node.children.insert(0,'• ')
            style={'h1':'Title','h2':'Heading1','h3':'Heading2','h4':'Heading3'}.get(node.tag,'Note' if node.attrs.get('class')=='note' else 'Normal')
            self.paragraph(node,style=style);return
        for c in node.children:self.blocks(c)

def styles():
    root=ET.Element(q('w:styles'));defaults=E(root,'w:docDefaults');rp=E(E(defaults,'w:rPrDefault'),'w:rPr')
    E(rp,'w:rFonts',{'w:ascii':'Arial','w:hAnsi':'Arial','w:eastAsia':'Microsoft YaHei'});E(rp,'w:sz',{'w:val':20});E(rp,'w:lang',{'w:val':'en-US','w:eastAsia':'zh-CN'})
    for ident,name,size,bold,color,outline in [('Normal','Normal',20,False,'152127',None),('Title','Title',44,True,'203E30',None),('Heading1','heading 1',32,True,'203E30',0),('Heading2','heading 2',28,True,'203E30',1),('Heading3','heading 3',24,True,'203E30',2),('TableText','Table Text',18,False,'152127',None),('Note','Note',18,False,'526167',None)]:
        st=E(root,'w:style',{'w:type':'paragraph','w:styleId':ident});E(st,'w:name',{'w:val':name})
        if ident=='Normal':st.set(q('w:default'),'1')
        else:E(st,'w:basedOn',{'w:val':'Normal'})
        pp=E(st,'w:pPr');E(pp,'w:spacing',{'w:after':40 if ident=='TableText' else 80,'w:line':240,'w:lineRule':'auto'})
        if outline is not None:E(pp,'w:outlineLvl',{'w:val':outline});E(pp,'w:keepNext');E(pp,'w:keepLines')
        rp=E(st,'w:rPr');E(rp,'w:sz',{'w:val':size});E(rp,'w:szCs',{'w:val':size});E(rp,'w:color',{'w:val':color})
        if bold:E(rp,'w:b');E(rp,'w:bCs')
    return root

def main():
    tree=Tree();tree.feed((OUT/'word-source.html').read_text(encoding='utf-8-sig'));body=descend(tree.root,'body')[0]
    b=Builder();b.blocks(body)
    sect=E(b.body,'w:sectPr');E(sect,'w:footerReference',{'w:type':'default','r:id':'rIdFooter'})
    E(sect,'w:pgSz',{'w:w':16838,'w:h':11906,'w:orient':'landscape'});E(sect,'w:pgMar',{'w:top':560,'w:right':560,'w:bottom':560,'w:left':560,'w:header':240,'w:footer':240,'w:gutter':0})
    footer=ET.Element(q('w:ftr'));p=E(footer,'w:p');E(E(p,'w:pPr'),'w:jc',{'w:val':'center'});b.run(p,'第 ',size=18);field=E(p,'w:fldSimple',{'w:instr':' PAGE '});b.run(field,'1',size=18);b.run(p,' 页',size=18)
    settings=ET.Element(q('w:settings'));E(settings,'w:zoom',{'w:percent':90});E(settings,'w:defaultTabStop',{'w:val':420});E(settings,'w:updateFields',{'w:val':'true'})
    fonts=ET.Element(q('w:fonts'))
    for name in ['Arial','Microsoft YaHei']:
        f=E(fonts,'w:font',{'w:name':name});E(f,'w:family',{'w:val':'swiss'})
    relns='http://schemas.openxmlformats.org/package/2006/relationships'
    relationships=ET.Element('Relationships',{'xmlns':relns})
    for idx,(name,_) in enumerate(b.media,1):ET.SubElement(relationships,'Relationship',{'Id':'rIdImage'+str(idx),'Type':NS['r']+'/image','Target':'media/'+name})
    for ident,kind,target in [('rIdStyles','styles','styles.xml'),('rIdSettings','settings','settings.xml'),('rIdFonts','fontTable','fontTable.xml'),('rIdFooter','footer','footer1.xml')]:ET.SubElement(relationships,'Relationship',{'Id':ident,'Type':NS['r']+'/'+kind,'Target':target})
    package=ET.Element('Relationships',{'xmlns':relns});ET.SubElement(package,'Relationship',{'Id':'rIdOfficeDocument','Type':NS['r']+'/officeDocument','Target':'word/document.xml'})
    types=ET.Element('Types',{'xmlns':'http://schemas.openxmlformats.org/package/2006/content-types'})
    for ext,mime in [('rels','application/vnd.openxmlformats-package.relationships+xml'),('xml','application/xml'),('png','image/png')]:ET.SubElement(types,'Default',{'Extension':ext,'ContentType':mime})
    for part,mime in [('document','document.main'),('styles','styles'),('settings','settings'),('fontTable','fontTable'),('footer1','footer')]:ET.SubElement(types,'Override',{'PartName':'/word/'+part+'.xml','ContentType':'application/vnd.openxmlformats-officedocument.wordprocessingml.'+mime+'+xml'})
    output=ROOT/'docs/L2-UI-Requirements-0.5.1.docx'
    with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
        for name,node in [('[Content_Types].xml',types),('_rels/.rels',package),('word/document.xml',b.doc),('word/styles.xml',styles()),('word/settings.xml',settings),('word/fontTable.xml',fonts),('word/footer1.xml',footer),('word/_rels/document.xml.rels',relationships)]:z.writestr(name,xml(node))
        for name,data in b.media:z.writestr('word/media/'+name,data)
    result={'file':str(output),'tables':b.table_count,'images':len(b.media),'sha256':hashlib.sha256(output.read_bytes()).hexdigest()}
    (OUT/'build.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(json.dumps(result,ensure_ascii=False))
if __name__=='__main__':main()
