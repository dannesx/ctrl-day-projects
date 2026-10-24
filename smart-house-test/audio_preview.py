"""In-memory preview of audio controls and uploads; does not emulate I2S sound."""
from email.parser import BytesParser
from email.policy import default
import struct
from urllib.parse import parse_qs

NAMES=('inicio','tudo_on','tudo_off','superior_on','superior_off','central_on','central_off',
       'terreo_on','terreo_off','manual','ciclo','neve','halloween','natal','matrix')


def valid_wav(raw):
    if len(raw)<44 or raw[:4]!=b'RIFF' or raw[8:12]!=b'WAVE' or struct.unpack_from('<I',raw,4)[0]!=len(raw)-8:return False
    offset=12;fmt=False;data=False
    while offset<len(raw):
        if len(raw)-offset<8:return False
        tag=raw[offset:offset+4];size=struct.unpack_from('<I',raw,offset+4)[0];offset+=8
        if size>len(raw)-offset:return False
        if tag==b'fmt ':
            if fmt or size<16 or struct.unpack_from('<HHIIHH',raw,offset)!=(1,1,16000,32000,2,16):return False
            fmt=True
        elif tag==b'data':
            if data or not size or size%2:return False
            data=True
        offset+=size+(size%2)
        if offset>len(raw):return False
    return fmt and data


class AudioPreview:
    def __init__(self):
        self.files={}
        self.state={'storage_ready':False,'ready':True,'volume':25,'muted':False,'playing':-1,
                    'files':[{'name':n+'.wav','available':False,'size':0} for n in NAMES],
                    'preview':True,'last_announcement':None,'announcement_count':0}

    def control(self,data):
        assert 'volume' in data or 'muted' in data
        volume=self.state['volume'];muted=self.state['muted']
        if 'volume' in data:
            assert data['volume'].isascii() and data['volume'].isdigit()
            volume=int(data['volume']);assert 0<=volume<=100
        if 'muted' in data:
            assert data['muted'] in ('true','false');muted=data['muted']=='true'
        self.state.update(volume=volume,muted=muted)

    def announce(self,action,data,before,after):
        clip=None
        if action=='mode' and before['mode']!=after['mode']:
            clip={'hsl_cycle':'ciclo','snow':'neve','christmas':'natal'}.get(after['mode'],after['mode'])
        elif action=='all' and any(a['on']!=b['on'] for a,b in zip(before['floors'],after['floors'])):
            clip='tudo_'+data['value']
        elif action=='floor' and 'on' in data:
            i=next(i for i,f in enumerate(after['floors']) if f['id']==data['id'])
            if before['floors'][i]['on']!=after['floors'][i]['on']:
                clip=('superior','central','terreo')[i]+('_on' if after['floors'][i]['on'] else '_off')
        if clip and clip+'.wav' in self.files and not self.state['muted'] and self.state['volume']:
            self.state['last_announcement']=clip
            self.state['announcement_count']+=1

    def request(self,path,headers,stream):
        length=int(headers.get('Content-Length',0))
        if length>132096:return 413,'Limite de tamanho excedido'
        raw=stream.read(length)
        if headers.get('Origin') not in (None,'http://'+headers.get('Host','')):return 403,'Origem invalida'
        if path=='/audio/initialize':
            if self.state['storage_ready'] or parse_qs(raw.decode()).get('confirm')!=['ERASE_AUDIO_DATA']:return 400,'Preparacao nao permitida'
            self.state['storage_ready']=True
            return 200,'Memoria preparada (simulacao em RAM)'
        if not self.state['storage_ready']:return 409,'Prepare a memoria primeiro'
        msg=BytesParser(policy=default).parsebytes(('Content-Type: '+headers.get('Content-Type','')+'\r\n\r\n').encode()+raw)
        parts=list(msg.iter_parts())
        if len(parts)!=1:return 400,'Envie um arquivo por requisicao'
        part=parts[0];name=part.get_filename();payload=part.get_payload(decode=True)
        if part.get_param('name',header='content-disposition')!='audio' or name not in [n+'.wav' for n in NAMES]:return 400,'Nome invalido'
        if not isinstance(payload,bytes):return 400,'WAV ausente'
        if len(payload)>128000 or sum(len(v) for k,v in self.files.items() if k!=name)+len(payload)>1200000:return 413,'Limite de tamanho excedido'
        if not valid_wav(payload):return 400,'WAV invalido'
        self.files[name]=payload
        for f in self.state['files']:
            if f['name']==name:f.update(available=True,size=len(payload))
        return 200,'Audio salvo (simulacao em RAM)'
