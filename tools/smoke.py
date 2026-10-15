#!/usr/bin/env python3
"""An isolated protocol smoke check with no writable gameplay memory."""
from pathlib import Path
import argparse, json, subprocess
ROOT=Path(__file__).resolve().parents[1]
TESTS=['opening_discipline_tests','response_adaptation_tests','verified_lessons_tests','character_choice_tests','preparation_tests']
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--tests',action='store_true');args=parser.parse_args()
    p=subprocess.run([str(ROOT/'engine/classicbot'),str(ROOT/'engine/decks.txt')],input='version\nnew 123 0 1 0 -1 1 0 -\n',text=True,capture_output=True,check=True)
    version,game=[json.loads(x) for x in p.stdout.splitlines()]
    assert version['ok'] and version['engineBuild']=='20261015-opening-render-1'
    assert game['ok'] and len(game['players'][0]['hand'])==10 and not game['ready']
    print('PASS: current build, legal preparation and ten-card starting hand; no player memory used.')
    if args.tests:
        for test in TESTS:subprocess.run([str(ROOT/'engine'/('check-'+test))],cwd=ROOT/'engine',check=True)
