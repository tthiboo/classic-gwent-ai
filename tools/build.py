#!/usr/bin/env python3
"""Build the source-only export; never depends on a private binary or artwork."""
from pathlib import Path
import argparse, os, subprocess
ROOT=Path(__file__).resolve().parents[1]
TESTS=['opening_discipline_tests','response_adaptation_tests','verified_lessons_tests','character_choice_tests','preparation_tests']
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--tests',action='store_true');args=parser.parse_args()
    for source,target in [('classic_bridge','classicbot'),('verified_reviewer','verified_reviewer')]+([(t,'check-'+t) for t in TESTS] if args.tests else []):
        subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O2','-pthread',str(ROOT/'engine'/(source+'.cpp')),'-o',str(ROOT/'engine'/target)],check=True)
        print('Built '+target,flush=True)
