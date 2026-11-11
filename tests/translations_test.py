#!/usr/bin/env python3
from pathlib import Path
import collections,re,xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[1]
ts=ET.parse(root/'translations/rnet_de_DE.ts').getroot()
assert ts.get('sourcelanguage')=='en_US' and ts.get('language')=='de_DE'
seen=set();total=0
for context in ts.findall('context'):
    for message in context.findall('message'):
        source=message.findtext('source');translation=message.find('translation')
        assert translation is not None and translation.get('type') not in ('unfinished','vanished','obsolete'),source
        text=''.join(translation.itertext());assert text,source
        assert collections.Counter(re.findall(r'%[1-9]\d*',source))==collections.Counter(re.findall(r'%[1-9]\d*',text)),source
        key=(context.findtext('name'),source);assert key not in seen,key
        seen.add(key);total+=1
assert ('RNet','&Read from Controller') in seen
assert ('RNet','Factory') in seen
assert ('RNet','Language') in seen
print(f'PASS: {total} complete German translations; US English source; placeholders preserved')
