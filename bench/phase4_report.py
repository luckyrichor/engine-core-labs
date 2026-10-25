"""Audit S8 runs and present only comparisons using bench.verdict outputs."""
import argparse
import csv
import json
from pathlib import Path


def report(root, plots=False):
    root=Path(root);groups=[];raw=[];comparisons=[]
    for directory in sorted(root.iterdir()):
        if not (directory/'raw.csv').exists():continue
        groups.append(directory.name)
        with (directory/'raw.csv').open() as stream:
            raw.extend(dict(row,group=directory.name) for row in csv.DictReader(stream))
        for suffix in ('', '-pinned'):
            with (directory/f'summary{suffix}.csv').open() as stream:
                for row in csv.DictReader(stream):
                    if row['metric']!='throughput' or '@aa' in row['impl']:continue
                    pinned=row['impl'].endswith('-pinned')
                    if suffix and not pinned:continue
                    kind='padding-pinned' if suffix else ('affinity-packed' if row['impl']=='packed-pinned' else 'padding-unbound')
                    if not suffix and pinned and row['impl']!='packed-pinned':continue
                    if row['impl'] in ('packed','packed-pinned') and kind!='affinity-packed':continue
                    comparisons.append(dict(group=directory.name,comparison=kind,**row))
    invalid=[r for r in raw if r['valid']!='1']
    hashes={json.loads((root/group/'device.json').read_text())['source']['sha256'] for group in groups}
    audit=dict(groups=groups,raw_rows=len(raw),invalid_rows=len(invalid),
        executions=len({(r['group'],r['threads'],r['round'],r['impl']) for r in raw}),
        stub_rows=sum(r['stub']!='0' for r in raw),source_hashes=sorted(hashes),
        invalid_reasons=sorted({r['invalid_reason'] for r in invalid}),
        minimum_valid_throughput_rounds=min(int(r['rounds_valid']) for r in comparisons))
    (root/'audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    with (root/'comparisons.csv').open('w') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(comparisons[0]));writer.writeheader();writer.writerows(comparisons)
    text='# S8 result comparisons\n\nAll significance classifications come from bench.verdict. Pinning and each layout change are paired in the same round; combined changes are not presented as a one-factor effect. Windows unbound still retains the P-core eligible mask.\n\n'
    text+=f"Audit: {audit['executions']} measured processes, {len(raw)} metric rows, invalid={len(invalid)}, minimum valid throughput rounds={audit['minimum_valid_throughput_rounds']}.\n\n"
    text+='| case | comparison | threads | impl | paired median [min,max] | A/A range | verdict | flags |\n|---|---|---|---|---|---|---|---|\n'
    for r in comparisons:
        if int(r['threads']) not in (1,4):continue
        ratio=f"{float(r['ratio_median']):.3f} [{float(r['ratio_min']):.3f},{float(r['ratio_max']):.3f}]" if r['ratio_median'] else ''
        noise=f"[{float(r['aa_min']):.3f},{float(r['aa_max']):.3f}]" if r['aa_min'] else ''
        text+=f"| {r['group']} | {r['comparison']} | {r['threads']} | {r['impl']} | {ratio} | {noise} | {r['verdict']} | {r['flags']} |\n"
    text+='\nAll thread counts are retained in comparisons.csv. CPU migrations are a Linux software event over the controlled interval, include producer/main, and are not cache/coherence hardware events. Layout bytes are selected objects/active cells, not RSS.\n'
    (root/'RESULTS.md').write_text(text)
    if plots:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        fig,axes=plt.subplots(1,3,figsize=(13,4))
        for axis,case in zip(axes,('positive','cold','stats')):
            for comparison in ('padding-unbound','padding-pinned','affinity-packed'):
                rows=sorted([r for r in comparisons if r['group']==case and r['comparison']==comparison],key=lambda r:int(r['threads']))
                if rows:axis.plot([int(r['threads']) for r in rows],[float(r['ratio_median']) for r in rows],'o-',label=comparison)
            axis.axhline(1,color='black',linewidth=.8);axis.set_xscale('log',base=2);axis.set_xlabel('workers');axis.set_title(case);axis.grid(alpha=.25)
        axes[0].set_ylabel('paired throughput / matching packed baseline')
        axes[-1].legend(fontsize=7);fig.suptitle('S8: per-machine controlled comparisons; significance in CSV')
        fig.tight_layout();fig.savefig(root/'s8-layout.svg');plt.close(fig)
        (root/'plot-version.json').write_text(json.dumps({'matplotlib':matplotlib.__version__})+'\n')
    print(json.dumps(audit,indent=2))


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('directory',type=Path);parser.add_argument('--plots',action='store_true');args=parser.parse_args();report(args.directory,args.plots)
if __name__=='__main__':main()
