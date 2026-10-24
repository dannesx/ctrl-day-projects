"""Convert named recordings with FFmpeg to the house's PCM WAV format."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
from audio_preview import NAMES, valid_wav


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path,help='Folder of recordings named inicio, tudo_on, etc.')
    parser.add_argument('destination',type=Path,help='Separate folder for converted WAV files')
    parser.add_argument('--ffmpeg',default='ffmpeg',help='FFmpeg executable path')
    args=parser.parse_args()
    if args.source.resolve()==args.destination.resolve():parser.error('Use a separate destination folder.')
    if not args.source.is_dir():parser.error('Source folder does not exist.')
    ffmpeg=shutil.which(args.ffmpeg)
    if not ffmpeg:parser.error('FFmpeg not found. Install it or pass --ffmpeg with its executable path.')
    sources={}
    for path in args.source.iterdir():
        if path.is_file() and path.stem in NAMES:
            if path.stem in sources:parser.error('Duplicate recording: '+path.stem)
            sources[path.stem]=path
    if not sources:parser.error('No recognized recordings. See AUDIO.md for names.')
    with tempfile.TemporaryDirectory(prefix='house-audio-') as temporary:
        converted=[]
        for name,source in sources.items():
            output=Path(temporary)/(name+'.wav')
            subprocess.run([ffmpeg,'-nostdin','-v','error','-i',str(source),'-vn','-ac','1','-ar','16000',
                            '-c:a','pcm_s16le','-map_metadata','-1','-fflags','+bitexact','-flags:a','+bitexact',str(output)],check=True)
            raw=output.read_bytes()
            if len(raw)>128000 or not valid_wav(raw):raise ValueError(name+': invalid WAV or longer than approximately 4 seconds; shorten the recording.')
            converted.append(output)
        total=sum(p.stat().st_size for p in converted)
        if args.destination.exists():
            total+=sum(p.stat().st_size for p in args.destination.glob('*.wav') if p.stem in NAMES and p.stem not in sources)
        if total>1200000:raise ValueError('Combined audio exceeds 1,200,000 bytes. Shorten recordings.')
        args.destination.mkdir(parents=True,exist_ok=True)
        for path in converted:shutil.copyfile(path,args.destination/path.name)
    print(f'Prepared {len(sources)} recordings; combined size {total:,} bytes.')
    missing=[name+'.wav' for name in NAMES if not (args.destination/(name+'.wav')).exists()]
    if missing:print('Still missing: '+', '.join(missing))


if __name__=='__main__':main()
