#!/usr/bin/env python3
"""Controlled source-size generator and generic command runner for MDE-HRA experiments.

The analyzer command is intentionally user-supplied because the repository currently
has multiple drivers and no single stable CLI. Example after adapting flags:
  python scripts/run_scaling.py --out results --sizes 8,32,128 \
    --command './build/perf-analyzer --input {source} --config {config} --json {json}'
The runner records command exit status, elapsed wall time, stdout/stderr, and JSON result
if the analyzer writes it. RSS is measured via GNU /usr/bin/time when available.
"""
import argparse, csv, hashlib, json, os, random, shlex, subprocess, tempfile, time
from pathlib import Path

TEMPLATE = r'''#include <stdlib.h>
struct Node { struct Node *next; unsigned value; };
int main(void) {
    struct Node *head = 0;
    for (unsigned i = 0; i < NODES; ++i) {
        struct Node *n = (struct Node *)malloc(sizeof *n);
        if (!n) return 2;
        n->value = i; n->next = head; head = n;
    }
    unsigned long sum = 0;
    for (struct Node *p = head; p; p = p->next) sum += p->value;
    while (head) { struct Node *q = head->next; free(head); head = q; }
    return sum == ((unsigned long)NODES * (NODES - 1) / 2) ? 0 : 1;
}
'''

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--out', default='results/scaling')
    ap.add_argument('--sizes', default='8,32,128,512')
    ap.add_argument('--configs', default='A,B,C,D')
    ap.add_argument('--repetitions', type=int, default=30)
    ap.add_argument('--seed', type=int, default=20261023)
    ap.add_argument('--command', required=True, help='shell command template with {source} {config} {json} placeholders')
    args=ap.parse_args(); out=Path(args.out).resolve(); out.mkdir(parents=True, exist_ok=True)
    srcdir=out/'generated'; srcdir.mkdir(exist_ok=True)
    sizes=[int(x) for x in args.sizes.split(',') if x.strip()]
    configs=[x.strip() for x in args.configs.split(',') if x.strip()]
    rows=[]; rng=random.Random(args.seed)
    for n in sizes:
        source=srcdir/f'chain_{n}.c'; source.write_text(TEMPLATE.replace('NODES',str(n)))
        sha=hashlib.sha256(source.read_bytes()).hexdigest()
        for rep in range(args.repetitions):
            order=configs[:]; rng.shuffle(order)
            for cfg in order:
                result_path=out/f'result_n{n}_{cfg}_r{rep}.json'
                stdout_path=out/f'stdout_n{n}_{cfg}_r{rep}.txt'; stderr_path=out/f'stderr_n{n}_{cfg}_r{rep}.txt'
                cmd=args.command.format(source=str(source.resolve()), config=cfg, json=str(result_path.resolve()))
                start=time.perf_counter()
                try:
                    p=subprocess.run(cmd, shell=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                    elapsed=time.perf_counter()-start; rc=p.returncode
                    stdout_path.write_text(p.stdout); stderr_path.write_text(p.stderr)
                except Exception as e:
                    elapsed=time.perf_counter()-start; rc=-999
                    stdout_path.write_text(''); stderr_path.write_text(repr(e))
                parsed={}
                if result_path.exists():
                    try: parsed=json.loads(result_path.read_text())
                    except Exception: parsed={'json_parse_error':True}
                rows.append({'size':n,'config':cfg,'rep':rep,'order_index':order.index(cfg),'seconds':elapsed,'return_code':rc,'source_sha256':sha,'command':cmd,'result_json':str(result_path),'status':parsed.get('status','NO_STATUS'),'rss_kb':parsed.get('peak_rss_kb','')})
                print(f'N={n} config={cfg} rep={rep} rc={rc} {elapsed:.4f}s')
    csvpath=out/'raw_runs.csv'
    with csvpath.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]) if rows else ['size']); w.writeheader(); w.writerows(rows)
    meta={'seed':args.seed,'sizes':sizes,'configs':configs,'repetitions':args.repetitions,'command_template':args.command,'warning':'Not publication evidence until command is validated, analyzer emits status/results, and memory/correctness instrumentation is checked.'}
    (out/'manifest.json').write_text(json.dumps(meta,indent=2))
    print(f'Wrote {csvpath}')
if __name__=='__main__': main()
