#!/usr/bin/env python3
"""Reproducible factory-preset pipeline.
  1. build recipes (type x character x style) with a fixed seed
  2. render every preset through the real engine (build/rg_render)
  3. loudness-normalise (writes `vol`), verify: no NaN, not silent, peak <= -1 dBFS, no stuck tail
  4. write presets/factory/*.rgp and presets/factory.json (embedded in the plugin)
Run:  python3 tools/gen_presets.py   (needs build/rg_render:  g++ -std=c++20 -O2 tools/render_cli.cpp -o build/rg_render)"""
import json, os, random, subprocess, sys, tempfile, re
sys.path.insert(0, os.path.dirname(__file__))
from params_def import *

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RENDER = os.path.join(ROOT, 'build', 'rg_render')
WT = os.path.join(ROOT, 'data', 'wavetables')
rnd = random.Random(351367)
DEF = {p[0]: p[5] for p in LAYER}
CH = {p[0]: p[6] for p in LAYER if isinstance(p[6], list)}
def idx(pid, name): return CH[pid].index(name)
T = lambda name: TABLES.index(name)

CHARACTERS = ['BLASTER-8', 'MONO-VOLT', 'FIVE-STAR', 'CHROME-FM', 'NEON-ROM', 'PULP-TUBE', 'XENO']
STYLES = ['Dark', 'Warm', 'Glitchy', 'Wide', 'Vintage', 'Evolving', 'Bright', 'Soft', 'Punchy', 'Distorted', 'Clean', 'Airy']
GENRES = ['Trap', 'Drill', 'R&B', 'Phonk', 'Synthwave', 'Cinematic', 'Hyperpop', 'Lo-Fi']

def mod(slot, src, dst, amt): return {f'm{slot}_src': idx(f'm{slot}_src', src), f'm{slot}_dst': idx(f'm{slot}_dst', dst), f'm{slot}_amt': amt}
def U(a, b): return rnd.uniform(a, b)
def pick(*xs): return rnd.choice(xs)

# ------------------------------------------------------------------ type recipes -> (params, test_note, dur, rel)
def r808():
    p = dict(o1_table=T(pick('Sub Heavy', 'Sine-Saw', 'Sub Heavy')), o1_pos=U(0, 0.35), o1_oct=-1, sub_lvl=U(0.5, 0.9), sub_wave=0, voice=1, glide=pick(0, 0, U(0.03, 0.12)),
             f_type=1, f_cut=U(500, 1800), f_res=U(0.05, 0.2), f_key=0.4, a_atk=0.001, a_dec=U(0.9, 3.5), a_sus=0, a_rel=U(0.15, 0.5), e_dec=U(0.03, 0.09), e_sus=0,
             charge=U(0.1, 0.5), oct=-1, f_drive=U(0, 0.3))
    p.update(mod(1, 'Mod Env', 'Pitch', U(0.06, 0.22))); return p, 36, 1.6, 0.6
def rbass():
    p = dict(o1_table=T(pick('Sine-Saw', 'Pulse Morph', 'Digital Grit', 'Metal Pluck')), o1_pos=U(0.4, 1), o2_lvl=U(0, 0.5), o2_table=T('Sub Heavy'), o2_oct=-1, o2_pos=U(0, 0.5), sub_lvl=U(0.2, 0.6),
             voice=pick(1, 1, 2), glide=U(0.0, 0.08), f_type=1, f_cut=U(250, 900), f_res=U(0.15, 0.45), f_env=U(0.2, 0.6), f_key=0.5, e_dec=U(0.12, 0.35), e_sus=U(0, 0.2),
             a_atk=0.002, a_dec=0.4, a_sus=U(0.6, 1), a_rel=U(0.08, 0.2), oct=-1, f_drive=U(0, 0.4), charge=U(0, 0.3))
    return p, 40, 1.2, 0.5
def rlead():
    p = dict(o1_table=T(pick('Sine-Saw', 'Pulse Morph', 'Digital Grit', 'Hollow Reed', 'Chrome FM')), o1_pos=U(0.4, 1), o2_lvl=U(0.2, 0.6), o2_table=T(pick('Pulse Morph', 'Sine-Saw')), o2_fine=U(5, 18), o2_pos=U(0.2, 0.9),
             uni_n=int(pick(1, 2, 3, 5)), uni_det=U(0.1, 0.4), voice=pick(1, 2, 0), glide=U(0, 0.1), f_type=1, f_cut=U(1800, 7000), f_res=U(0.1, 0.35), f_env=U(0.1, 0.4), f_key=0.6,
             e_dec=U(0.2, 0.6), e_sus=U(0.2, 0.6), a_atk=0.004, a_dec=0.3, a_sus=U(0.7, 1), a_rel=U(0.15, 0.35), delay_mix=U(0.1, 0.3), delay_fb=0.4, verb_mix=U(0.05, 0.25), l1_rate=U(4.5, 6.5), maxv=8)
    p.update(mod(1, 'LFO 1', 'Pitch', 0.0)); p.update(mod(2, 'Mod Wheel', 'Pitch', 0.012)); p.update(mod(3, 'Mod Wheel', 'Cutoff', 0.25)); return p, 64, 1.2, 0.8
def rpad():
    p = dict(o1_table=T(pick('String Ens', 'Warm Organ', 'Sine-Saw', 'TRILL Dark Bed', 'TRILL Air', 'Hollow Reed', 'Pulse Morph')), o1_pos=U(0.1, 0.8), o2_lvl=U(0.4, 0.8), o2_table=T(pick('String Ens', 'Pulse Morph', 'TRILL Glass', 'Vox Formant')),
             o2_pos=U(0, 1), o2_fine=U(-12, 12), uni_n=int(pick(3, 4, 5, 6)), uni_det=U(0.2, 0.5), uni_wid=U(0.6, 1), f_type=1, f_cut=U(900, 4500), f_res=U(0.05, 0.25), f_env=U(0, 0.25), f_key=0.4,
             a_atk=U(0.25, 1.4), a_dec=1.0, a_sus=U(0.7, 1), a_rel=U(1.0, 3.2), e_atk=U(0.5, 1.5), e_dec=U(1, 3), e_sus=U(0.3, 0.8), l1_rate=U(0.15, 0.6), l2_rate=U(0.1, 0.35),
             chorus=U(0.2, 0.5), verb_mix=U(0.2, 0.45), verb_size=U(0.5, 0.9), maxv=12)
    p.update(mod(1, 'LFO 1', 'Osc1 Pos', U(0.1, 0.35))); p.update(mod(2, 'LFO 2', 'Osc2 Pos', U(0.1, 0.3))); p.update(mod(3, 'Mod Wheel', 'Cutoff', 0.3)); return p, 58, 3.0, 3.5
def rkeys():
    kind = pick('fm', 'organ', 'tine')
    p = dict(f_type=1, f_cut=U(2500, 9000), f_res=0.08, f_key=0.5, a_atk=0.003, a_dec=U(0.6, 2.0), a_sus=U(0.0, 0.35), a_rel=U(0.25, 0.8), e_dec=U(0.1, 0.4), e_sus=0, f_env=U(0.1, 0.3), chorus=U(0.1, 0.4), verb_mix=U(0.08, 0.25), maxv=10, uni_n=1)
    if kind == 'fm': p.update(o1_table=T('Sine-Saw'), o1_pos=0.05, o2_table=T('Chrome FM'), o2_lvl=0.0, o2_oct=pick(0, 1), o2_pos=U(0.1, 0.5), fm=U(0.15, 0.5), a_sus=0.1)
    elif kind == 'organ': p.update(o1_table=T('Warm Organ'), o1_pos=U(0.1, 0.9), o2_table=T('Hollow Reed'), o2_lvl=U(0.2, 0.5), o2_oct=1, a_sus=U(0.8, 1), a_dec=0.1, a_rel=0.12, f_drive=0.2)
    else: p.update(o1_table=T('Glass Bell'), o1_pos=U(0.1, 0.6), o2_table=T('Sine-Saw'), o2_pos=0.0, o2_lvl=U(0.3, 0.5), sub_lvl=0.2)
    p.update(mod(1, 'Velocity', 'Cutoff', U(0.15, 0.4))); p.update(mod(2, 'Velocity', 'Osc1 Pos', U(0.1, 0.3))); return p, 60, 1.3, 0.8
def rpluck():
    p = dict(o1_table=T(pick('Metal Pluck', 'Sine-Saw', 'Pulse Morph', 'Digital Grit', 'Glass Bell', 'TRILL Glass')), o1_pos=U(0.2, 1), o2_lvl=U(0, 0.5), o2_table=T('Pulse Morph'), o2_fine=U(4, 14),
             f_type=1, f_cut=U(500, 2500), f_res=U(0.1, 0.35), f_env=U(0.4, 0.8), f_key=0.5, e_atk=0.001, e_dec=U(0.08, 0.3), e_sus=0, a_atk=0.001, a_dec=U(0.12, 0.5), a_sus=0, a_rel=U(0.1, 0.3),
             delay_mix=U(0.05, 0.3), delay_fb=0.35, verb_mix=U(0.05, 0.2), uni_n=int(pick(1, 2, 3)), uni_det=0.2, maxv=8)
    return p, 67, 0.9, 0.6
def rbell():
    p = dict(o1_table=T(pick('Glass Bell', 'Chrome FM', 'Metal Pluck', 'TRILL Glass')), o1_pos=U(0.1, 0.7), o2_table=T('Chrome FM'), o2_lvl=U(0.2, 0.5), o2_oct=pick(1, 2), o2_pos=U(0.2, 0.8), fm=U(0.2, 0.7),
             f_type=1, f_cut=U(5000, 14000), f_res=0.05, f_key=0.8, a_atk=0.001, a_dec=U(1.2, 3.5), a_sus=0, a_rel=U(0.6, 1.8), e_dec=U(0.3, 1.0), e_sus=0, chorus=U(0, 0.3), delay_mix=U(0.1, 0.3), delay_fb=0.4, verb_mix=U(0.2, 0.45), verb_size=0.7, maxv=10)
    p.update(mod(1, 'Mod Env', 'FM Amount', U(0.1, 0.4))); p.update(mod(2, 'Velocity', 'FM Amount', 0.2)); return p, 72, 1.5, 1.8
def rbrass():
    p = dict(o1_table=T('Brass Stack'), o1_pos=U(0.2, 0.7), o2_lvl=U(0.5, 0.8), o2_table=T(pick('Brass Stack', 'Sine-Saw')), o2_fine=U(5, 12), o2_pos=U(0.4, 0.9), uni_n=int(pick(2, 3)), uni_det=U(0.1, 0.25),
             f_type=1, f_cut=U(500, 1200), f_res=U(0.05, 0.2), f_env=U(0.5, 0.9), f_key=0.3, e_atk=U(0.04, 0.12), e_dec=U(0.2, 0.5), e_sus=U(0.35, 0.6), a_atk=U(0.02, 0.08), a_dec=0.3, a_sus=U(0.7, 1), a_rel=U(0.12, 0.3),
             chorus=U(0.1, 0.3), verb_mix=U(0.08, 0.2), maxv=10)
    p.update(mod(1, 'Velocity', 'Cutoff', 0.3)); p.update(mod(2, 'Mod Wheel', 'Osc1 Pos', 0.3)); return p, 57, 1.5, 0.6
def rstrings():
    p = dict(o1_table=T('String Ens'), o1_pos=U(0.1, 0.9), o2_lvl=U(0.5, 0.8), o2_table=T(pick('String Ens', 'Sine-Saw', 'Pulse Morph')), o2_pos=U(0.2, 1), o2_fine=U(-10, 10), uni_n=int(pick(4, 5, 6)), uni_det=U(0.2, 0.45), uni_wid=U(0.7, 1),
             f_type=1, f_cut=U(1800, 6000), f_res=0.08, f_env=U(0.0, 0.2), f_key=0.5, a_atk=U(0.15, 0.7), a_dec=1.0, a_sus=U(0.8, 1), a_rel=U(0.5, 1.6), chorus=U(0.3, 0.6), verb_mix=U(0.15, 0.35), verb_size=0.7, l1_rate=U(4, 5.5), maxv=12)
    p.update(mod(1, 'LFO 2', 'Osc1 Pos', U(0.05, 0.2))); p.update(mod(2, 'Mod Wheel', 'Cutoff', 0.3)); return p, 55, 2.5, 1.5
def rarp():
    p = dict(o1_table=T(pick('Pulse Morph', 'Digital Grit', 'Metal Pluck', 'Sine-Saw', 'TRILL Current', 'TRILL Glass')), o1_pos=U(0.2, 1), o2_lvl=U(0, 0.5), o2_table=T('Pulse Morph'), o2_fine=U(3, 12),
             f_type=1, f_cut=U(900, 4000), f_res=U(0.12, 0.35), f_env=U(0.3, 0.7), f_key=0.5, e_dec=U(0.08, 0.25), e_sus=0, a_atk=0.001, a_dec=U(0.15, 0.4), a_sus=0, a_rel=U(0.1, 0.3),
             arp_on=1, arp_rate=pick(1, 3, 3, 4, 5), arp_mode=pick(0, 0, 1, 2, 3), arp_oct=pick(1, 2, 3), arp_gate=U(0.35, 0.8), delay_mix=U(0.15, 0.35), delay_fb=0.4, verb_mix=U(0.1, 0.25), maxv=8, uni_n=int(pick(1, 2)))
    return p, 60, 2.0, 0.8
def rvox():
    p = dict(o1_table=T('Vox Formant'), o1_pos=U(0.0, 0.8), o2_lvl=U(0.3, 0.7), o2_table=T(pick('Vox Formant', 'Hollow Reed', 'TRILL Dark Bed')), o2_pos=U(0.2, 1), o2_fine=U(-8, 8), uni_n=int(pick(2, 3, 4)), uni_det=U(0.1, 0.3),
             f_type=1, f_cut=U(2500, 7000), f_res=0.1, f_key=0.4, a_atk=U(0.05, 0.4), a_dec=0.6, a_sus=U(0.7, 1), a_rel=U(0.4, 1.4), l1_rate=U(0.2, 0.7), l2_rate=U(4.5, 6), chorus=U(0.2, 0.5), verb_mix=U(0.2, 0.4), verb_size=0.7, maxv=10)
    p.update(mod(1, 'LFO 1', 'Osc1 Pos', U(0.1, 0.4))); p.update(mod(2, 'Mod Wheel', 'Osc1 Pos', 0.5)); p.update(mod(3, 'LFO 2', 'Pitch', 0.01)); return p, 57, 2.0, 1.5
def rfx():
    kind = pick('zap', 'riser', 'impact', 'drone', 'noise')
    p = dict(o1_table=T(pick('Digital Grit', 'Chrome FM', 'TRILL Uzi', 'TRILL Air', 'Metal Pluck')), o1_pos=U(0.2, 1), o2_lvl=U(0.2, 0.6), o2_table=T(pick('Chrome FM', 'Digital Grit')), o2_pos=U(0.1, 1), o2_fine=U(-30, 30),
             o1_warp=U(0.2, 0.9), o1_warpmode=pick(1, 2, 3, 4), f_type=pick(1, 3, 2), f_cut=U(1500, 8000), f_res=U(0.2, 0.6), a_atk=0.002, delay_mix=U(0.15, 0.4), delay_fb=0.5, verb_mix=U(0.2, 0.5), verb_size=0.8, maxv=6, uni_n=int(pick(1, 3)))
    if kind == 'zap': p.update(a_dec=U(0.15, 0.5), a_sus=0, a_rel=0.2, e_dec=U(0.08, 0.25), e_sus=0); p.update(mod(1, 'Mod Env', 'Pitch', -U(0.6, 1.0)), **mod(2, 'Mod Env', 'Cutoff', U(0.3, 0.8)))
    elif kind == 'riser': p.update(a_atk=U(1.5, 3), a_dec=1, a_sus=1, a_rel=0.6, e_atk=U(1.5, 3.5), e_dec=1, e_sus=1, noise_lvl=U(0.2, 0.5)); p.update(mod(1, 'Mod Env', 'Pitch', U(0.3, 0.7)), **mod(2, 'Mod Env', 'Cutoff', U(0.5, 0.9)))
    elif kind == 'impact': p.update(a_dec=U(1.5, 3.5), a_sus=0, a_rel=1.0, e_dec=U(0.1, 0.3), e_sus=0, noise_lvl=U(0.1, 0.3), f_type=1, f_cut=U(300, 1500), charge=U(0.3, 0.7), o1_oct=-1); p.update(mod(1, 'Mod Env', 'Pitch', U(0.15, 0.4)))
    elif kind == 'drone': p.update(a_atk=U(0.6, 1.8), a_dec=1, a_sus=1, a_rel=U(1.5, 3), l1_rate=U(0.1, 0.5), uni_n=int(pick(3, 5)), uni_det=U(0.3, 0.6), uni_wid=1); p.update(mod(1, 'LFO 1', 'Osc1 Pos', 0.4), **mod(2, 'LFO 1', 'Cutoff', 0.25))
    else: p.update(a_atk=U(0.01, 0.4), a_dec=U(1, 2), a_sus=U(0.3, 0.8), a_rel=U(1, 2), noise_lvl=U(0.4, 0.9), o1_lvl=0.3, f_type=3, f_cut=U(800, 5000), f_res=0.4, l1_rate=U(0.3, 2)); p.update(mod(1, 'LFO 1', 'Cutoff', 0.5))
    return p, 60, 2.2, 2.2

RECIPES = {'808': r808, 'Bass': rbass, 'Lead': rlead, 'Pad': rpad, 'Keys': rkeys, 'Pluck': rpluck, 'Bell': rbell, 'Brass': rbrass, 'Strings': rstrings, 'Arp': rarp, 'Vox': rvox, 'FX': rfx}
TYPE_ORDER = ['808', 'Bass', 'Lead', 'Pad', 'Keys', 'Pluck', 'Bell', 'Brass', 'Strings', 'Arp', 'Vox', 'FX']
TYPE_CHAR = {'808': ['XENO', 'MONO-VOLT', 'BLASTER-8'], 'Bass': ['MONO-VOLT', 'BLASTER-8', 'XENO', 'PULP-TUBE'], 'Lead': ['MONO-VOLT', 'XENO', 'NEON-ROM', 'BLASTER-8'], 'Pad': ['PULP-TUBE', 'FIVE-STAR', 'NEON-ROM', 'BLASTER-8', 'XENO'],
             'Keys': ['CHROME-FM', 'PULP-TUBE', 'NEON-ROM'], 'Pluck': ['CHROME-FM', 'XENO', 'NEON-ROM', 'BLASTER-8'], 'Bell': ['CHROME-FM', 'NEON-ROM', 'XENO'], 'Brass': ['FIVE-STAR', 'BLASTER-8', 'PULP-TUBE'],
             'Strings': ['BLASTER-8', 'FIVE-STAR', 'NEON-ROM'], 'Arp': ['XENO', 'NEON-ROM', 'BLASTER-8', 'CHROME-FM'], 'Vox': ['NEON-ROM', 'PULP-TUBE', 'XENO'], 'FX': ['XENO', 'NEON-ROM', 'CHROME-FM']}

def char_overlay(p, ch, typ):
    if ch == 'BLASTER-8': p['chorus'] = max(p.get('chorus', 0), U(0.25, 0.5)); p['f_type'] = 1; p['o2_fine'] = p.get('o2_fine', 0) or U(-9, 9); p['uni_n'] = max(p.get('uni_n', 1), 2 if typ not in ('808',) else 1)
    elif ch == 'MONO-VOLT' and typ not in ('Pad', 'Strings', 'Bell', 'Keys'): p['voice'] = 1; p['sub_lvl'] = max(p.get('sub_lvl', 0), 0.25); p['f_res'] = max(p.get('f_res', 0.1), U(0.25, 0.45)); p['glide'] = p.get('glide', 0) or U(0.02, 0.07)
    elif ch == 'FIVE-STAR': p['uni_n'] = max(p.get('uni_n', 1), 4); p['uni_det'] = max(p.get('uni_det', 0.2), 0.3); p['chorus'] = max(p.get('chorus', 0), 0.35); p['uni_wid'] = 0.85
    elif ch == 'CHROME-FM': p['fm'] = max(p.get('fm', 0), U(0.15, 0.45)); p['o2_table'] = T(pick('Chrome FM', 'Glass Bell')); p['o2_oct'] = pick(0, 1); p['o2_pos'] = U(0.1, 0.6)
    elif ch == 'NEON-ROM': p['o1_warp'] = max(p.get('o1_warp', 0), U(0.15, 0.6)); p['o1_warpmode'] = p.get('o1_warpmode') or pick(1, 2, 3, 4); p['charge'] = max(p.get('charge', 0), U(0.05, 0.25)); p['delay_mix'] = max(p.get('delay_mix', 0), U(0.05, 0.2))
    elif ch == 'PULP-TUBE': p['f_drive'] = max(p.get('f_drive', 0), U(0.2, 0.45)); p['charge'] = max(p.get('charge', 0), U(0.08, 0.25)); p['chorus'] = max(p.get('chorus', 0), 0.25); p['o2_table'] = T(pick('Warm Organ', 'Hollow Reed')) if p.get('o2_lvl', 0) > 0 else p.get('o2_table', 3)
    elif ch == 'XENO': p['o1_table'] = T(pick('TRILL Dark Bed', 'TRILL Glass', 'TRILL Air', 'TRILL Current', 'TRILL Uzi', 'Digital Grit')); p['o1_warp'] = max(p.get('o1_warp', 0), U(0.1, 0.5)); p['o1_warpmode'] = p.get('o1_warpmode') or pick(1, 2, 4); p['l2_rate'] = U(0.1, 3)
    return p

def style_overlay(p, styles):
    for s in styles:
        if s == 'Dark': p['f_cut'] = p.get('f_cut', 8000) * U(0.4, 0.65); p['verb_mix'] = p.get('verb_mix', 0) + 0.05
        elif s == 'Warm': p['f_cut'] = p.get('f_cut', 8000) * U(0.6, 0.85); p['f_drive'] = max(p.get('f_drive', 0), 0.15)
        elif s == 'Glitchy': p['delay_mix'] = max(p.get('delay_mix', 0), 0.25); p['delay_time'] = pick(0.125, 0.1875, 0.25); p['delay_fb'] = 0.55; p['o1_warp'] = max(p.get('o1_warp', 0), 0.4); p['o1_warpmode'] = p.get('o1_warpmode') or 4
        elif s == 'Wide': p['uni_n'] = max(p.get('uni_n', 1), 3); p['uni_wid'] = 1.0; p['chorus'] = max(p.get('chorus', 0), 0.4)
        elif s == 'Vintage': p['chorus'] = max(p.get('chorus', 0), 0.3); p['f_drive'] = max(p.get('f_drive', 0), 0.2); p['f_cut'] = p.get('f_cut', 8000) * 0.8
        elif s == 'Evolving': p['l1_rate'] = U(0.1, 0.4); p.update({k: v for k, v in mod(6, 'LFO 1', 'Osc1 Pos', U(0.2, 0.5)).items()}); p.update(mod(7, 'LFO 2', 'Osc2 Pos', U(0.15, 0.4)))
        elif s == 'Bright': p['f_cut'] = min(20000, p.get('f_cut', 8000) * U(1.4, 2.0)); p['o1_pos'] = min(1, p.get('o1_pos', 0.5) + 0.15)
        elif s == 'Soft': p['f_cut'] = p.get('f_cut', 8000) * 0.7; p['a_atk'] = max(p.get('a_atk', 0.005), 0.02) * 1.5; p['f_res'] = min(p.get('f_res', 0.1), 0.1)
        elif s == 'Punchy': p['a_atk'] = 0.001; p['f_env'] = max(p.get('f_env', 0), 0.35); p['e_dec'] = min(p.get('e_dec', 0.3), 0.2)
        elif s == 'Distorted': p['charge'] = max(p.get('charge', 0), U(0.35, 0.7)); p['f_drive'] = max(p.get('f_drive', 0), 0.4)
        elif s == 'Airy': p['verb_mix'] = max(p.get('verb_mix', 0), 0.3); p['noise_lvl'] = max(p.get('noise_lvl', 0), 0.04); p['f_cut'] = min(20000, p.get('f_cut', 8000) * 1.3)
    return p

NOUNS = {'808': ['Glide 808', 'Sub Cannon', 'Thump 808', 'Boom Tank', 'Rumble Core', 'Slide 808', 'Low Orbit', 'Ground Zero', 'Blast Sub', 'Heavy Round', 'Vault 808', 'Gravity'],
         'Bass': ['Bass', 'Drill Bass', 'Growl Bass', 'Rubber Bass', 'Saw Bass', 'Pulse Bass', 'Reactor Bass', 'Bunker Bass', 'Sub Bass', 'Dirt Bass'],
         'Lead': ['Lead', 'Slide Lead', 'Beam Lead', 'Screamer', 'Whistle Lead', 'Solo', 'Scope Lead', 'Laser Lead', 'Sync Lead', 'Hook'],
         'Pad': ['Pad', 'Bed', 'Drift Pad', 'Haze', 'Atmosphere', 'Wash', 'Nebula', 'Aurora', 'Canopy', 'Tide Pad'],
         'Keys': ['Keys', 'Tine Keys', 'Organ', 'EP', 'Glass Keys', 'Rhodes-ish Keys', 'Clav Keys', 'Soft Keys', 'Chime Keys', 'Wurl Keys'],
         'Pluck': ['Pluck', 'Pick', 'Harp Pluck', 'Marimba Pluck', 'Spark Pluck', 'Stab', 'Mallet', 'Kalimba', 'Chirp', 'Zap Pluck'],
         'Bell': ['Bell', 'Chime', 'Music Box', 'Glass Bell', 'Thump Bell', 'Tubular', 'Crystal', 'Ping', 'Gong Bell', 'Icicle'],
         'Brass': ['Brass', 'Horns', 'Stab Brass', 'Swell Brass', 'Fanfare', 'Section', 'Bold Brass', 'Trumpet-ish', 'Power Brass', 'Blare'],
         'Strings': ['Strings', 'Ensemble', 'Violins', 'Orchestra Bed', 'Legato Strings', 'Tremolo Strings', 'Cello-ish', 'Chamber', 'Sweep Strings', 'Staccato'],
         'Arp': ['Arp', 'Chop Arp', 'Sequence', 'Run', 'Gate Arp', 'Pattern', 'Cascade', 'Ripple', 'Climb', 'Stutter'],
         'Vox': ['Choir', 'Vox', 'Ahh', 'Formant', 'Vowel Pad', 'Whisper', 'Breath', 'Chant', 'Voices', 'Oooh'],
         'FX': ['Zap FX', 'Riser', 'Impact', 'Drone', 'Sweep', 'Plasma', 'Ion FX', 'Noise Wash', 'Downlifter', 'Alarm']}
ADJ = ['Neon', 'Dark', 'Plasma', 'Chrome', 'Velvet', 'Rusty', 'Cosmic', 'Atomic', 'Toxic', 'Hollow', 'Electric', 'Frozen', 'Golden', 'Crimson', 'Lunar', 'Solar', 'Static', 'Phantom', 'Rocket',
       'Retro', 'Orbit', 'Saturn', 'Vapor', 'Brass-Knuckle', 'Midnight', 'Radium', 'Turbo', 'Cobalt', 'Ghost', 'Fusion', 'Quantum', 'Rogue', 'Sugar', 'Smoke', 'Pulse', 'Titan', 'Glitch', 'Silk', 'Jet', 'Mars', 'Venus',
       'Gamma', 'Delta', 'Sigma', 'Ion', 'Vortex', 'Nova', 'Hyper', 'Cyber', 'Astro', 'Zero-G', 'Blaster', 'Warp', 'Lazer', 'Sonic', 'Amber', 'Onyx', 'Ruby', 'Jade', 'Cinder', 'Marble']
KEYS = ['Cm', 'C#m', 'Dm', 'Ebm', 'Em', 'Fm', 'F#m', 'Gm', 'G#m', 'Am', 'Bbm', 'Bm']

def finalize(p):
    out = {}
    for k, v in p.items():
        if k not in DEF: raise KeyError(k)
        dv = DEF[k]
        if isinstance(v, float):
            lo = next(q for q in LAYER if q[0] == k); v = max(lo[3], min(lo[4], v)); v = round(v, 4)
        if abs(v - dv) > 1e-6: out[k] = v if not isinstance(v, bool) else int(v)
    return out

def build_singles():
    out = []; used = set()
    per = {t: (45 if t not in ('Pad', 'Bell') else 46) for t in TYPE_ORDER}
    for typ in TYPE_ORDER:
        for i in range(per[typ]):
            ch = TYPE_CHAR[typ][i % len(TYPE_CHAR[typ])]
            styles = rnd.sample(STYLES, rnd.choice([1, 2, 2, 3]))
            p, note, dur, rel = RECIPES[typ]()
            p = style_overlay(char_overlay(p, ch, typ), styles)
            for _ in range(50):
                name = f"{rnd.choice(ADJ)} {rnd.choice(NOUNS[typ])}"
                if name not in used: break
            else: name = f"{rnd.choice(ADJ)} {rnd.choice(NOUNS[typ])} {rnd.randint(2, 99)}"
            used.add(name)
            genre = rnd.sample(GENRES, rnd.choice([1, 2]))
            tags = [s for s in styles]
            if p.get('arp_on'): tags.append('Synced')
            if p.get('voice', 0) >= 1 and typ in ('Bass', 'Lead', '808'): tags.append('Mono')
            if p.get('glide', 0) > 0.02: tags.append('Glide')
            out.append(dict(name=name, type=typ, character=ch, tags=tags, style=styles, genre=genre, author='Trill Tron', desc='', a=p, note=note, dur=dur, rel=rel))
    return out

def build_stem_dna():
    """Hand-authored presets that mirror the producer's beats (351 = Bb minor, 48 = Uzi loop)."""
    L = []
    def add(name, typ, ch, styles, genre, desc, key, bpm, p, note, dur, rel, macros=None, tags=None):
        d = dict(name=name, type=typ, character=ch, tags=(tags or styles) + ['Stem DNA'], style=styles, genre=genre, author='Trill Tron', desc=desc, key=key, bpm=bpm, a=p, note=note, dur=dur, rel=rel)
        if macros: d['macros'] = macros
        L.append(d)
    p, n, du, r = rpad(); p.update(o1_table=T('TRILL Dark Bed'), o1_pos=0.38, o2_table=T('String Ens'), o2_lvl=0.45, f_cut=2200, a_atk=0.18, a_rel=1.2, chorus=0.45, verb_mix=0.5, f_drive=0.2, uni_n=4)
    add('Bb Minor Bed PD', 'Pad', 'PULP-TUBE', ['Dark', 'Warm'], ['Trap'], 'Warm organ bed in Bb minor from beat 351. Mod wheel opens the filter; Motion macro drifts the table.', 'Bbm', 140, p, 58, du, r, [0.62, 0.45, 0.5, 0.5, 0.2, 0.4, 0.5, 0.5])
    p, n, du, r = rlead(); p.update(o1_table=T('TRILL Current'), voice=1, glide=0.06, f_cut=3800, uni_n=2, o2_lvl=0.3); add('Slide Lead 351', 'Lead', 'MONO-VOLT', ['Glide'], ['R&B'], 'Mono glide lead from the 351 hook.', 'Bbm', 140, p, 70, du, r, tags=['Mono', 'Glide', 'R&B'])
    p, n, du, r = rbell(); p.update(o1_table=T('TRILL Glass'), o1_pos=0.35); add('Thump Bell 367', 'Bell', 'CHROME-FM', ['Clean'], ['Drill'], 'Glassy bell with a pitch thump, from beat 367.', 'Bbm', 140, p, 72, du, r)
    p, n, du, r = rarp(); p.update(o1_table=T('TRILL Uzi'), arp_rate=3, arp_mode=0, arp_oct=2); add('Chop Arp 137', 'Arp', 'XENO', ['Glitchy'], ['Drill'], 'Chopped arp built from the Uzi loop cycles.', '', 137, p, 60, du, r, tags=['Glitchy', 'Synced', 'Drill'])
    p, n, du, r = rbass(); p.update(o1_table=T('TRILL Dark Bed'), o1_oct=0, voice=1, f_cut=700, f_res=0.3); add('Phrygian Drill BS', 'Bass', 'MONO-VOLT', ['Dark'], ['Drill'], 'Dark octave bass for Phrygian drill lines.', 'Bbm', 140, p, 34, du, r, tags=['Dark', 'Octave', 'Drill'])
    p, n, du, r = r808(); p.update(o1_table=T('Sub Heavy'), glide=0.08, charge=0.55); add('Glide 808 Charge', '808', 'XENO', ['Distorted'], ['Trap'], 'Gliding 808 with Charge saturation.', 'Bbm', 140, p, 34, du, r, [0.5, 0.5, 0.5, 0.5, 0.7, 0.5, 0.5, 0.5], tags=['Distorted', 'Glide', 'Trap'])
    p, n, du, r = rfx(); p.update(o1_table=T('TRILL Uzi'), a_atk=0.002, a_dec=0.5, a_sus=0); add('Plasma Zap FX', 'FX', 'XENO', ['Clean'], ['Hyperpop'], 'One-shot sci-fi zap.', '', 0, p, 60, du, r, tags=['Sci-Fi', 'One-shot'])
    p, n, du, r = rkeys(); p.update(o1_table=T('TRILL Glass'), o1_pos=0.5); add('Neon Glass Keys', 'Keys', 'CHROME-FM', ['Bright'], ['R&B'], 'Bright glassy keys from the 351 insert.', 'Bbm', 140, p, 60, du, r)
    p, n, du, r = rstrings(); add('Blaster Strings', 'Strings', 'BLASTER-8', ['Wide', 'Vintage'], ['Cinematic'], 'Wide vintage string wash.', '', 0, p, 55, du, r)
    p, n, du, r = rbrass(); add('Five-Star Brass', 'Brass', 'FIVE-STAR', ['Punchy', 'Vintage'], ['Synthwave'], 'Punchy section brass.', '', 0, p, 57, du, r)
    p, n, du, r = rvox(); add('Dark Choir Vox', 'Vox', 'NEON-ROM', ['Dark'], ['Cinematic'], 'Dark formant choir.', '', 0, p, 57, du, r)
    p, n, du, r = rpluck(); p.update(o1_table=T('Glass Bell'), a_dec=0.8, f_cut=6000); add('Music Box Pluck', 'Pluck', 'CHROME-FM', ['Soft'], ['Lo-Fi'], 'Soft music-box pluck.', '', 0, p, 72, du, r, tags=['Soft', 'Lo-Fi'])
    p, n, du, r = rfx(); p.update(o1_table=T('TRILL Air'), a_atk=2.2, a_sus=1, e_atk=2.5, e_sus=1, noise_lvl=0.3); add('Ion Riser FX', 'FX', 'XENO', ['Clean'], ['Hyperpop'], 'Synced riser built from the 351 air insert.', '', 0, p, 60, 3.5, 1.5, tags=['Riser', 'Synced'])
    p, n, du, r = rlead(); p.update(o1_table=T('Hollow Reed'), a_atk=0.06, l1_rate=5.5); add('Breath Whistle LD', 'Lead', 'XENO', ['Airy'], ['Trap'], 'Breathy whistle lead with vibrato.', '', 0, p, 74, du, r, tags=['Airy', 'Vibrato', 'Trap'])
    return L

def build_multis(singles):
    out = []; bytype = {t: [s for s in singles if s['type'] == t] for t in TYPE_ORDER}
    combos = [('808', 'Bell', 'split', 'Boom + Chime'), ('Bass', 'Lead', 'split', 'Low + Hook'), ('Pad', 'Pluck', 'layer', 'Bed + Pluck'), ('Strings', 'Brass', 'layer', 'Orchestra Stack'),
              ('Keys', 'Pad', 'velocity', 'Touch Keys'), ('Pad', 'Vox', 'layer', 'Choir Bed'), ('Bass', 'Arp', 'split', 'Bass + Arp'), ('Keys', 'Bell', 'layer', 'Glass Keys Stack')]
    for i in range(48):
        ta, tb, mode, label = combos[i % len(combos)]
        A, B = rnd.choice(bytype[ta]), rnd.choice(bytype[tb]); a, b = dict(A['a']), dict(B['a'])
        g = {}
        if mode == 'split': g = dict(mode=3, split=rnd.choice([48, 52, 55, 60]), xfade=rnd.choice([0, 2, 3])); a['keyhi'] = 127; b['keylo'] = 0
        elif mode == 'layer': g = dict(mode=2, blend=round(U(0.35, 0.65), 2))
        else: g = dict(mode=4, vsplit=rnd.choice([84, 96, 104]), vfade=rnd.choice([8, 12, 20]))
        name = f"{rnd.choice(ADJ)} {label}"
        if any(o['name'] == name for o in out): name += f" {rnd.randint(2, 9)}"
        st = list(dict.fromkeys(A['style'][:1] + B['style'][:1]))
        out.append(dict(name=name, type=ta if mode != 'layer' else tb, character=A['character'], tags=st + ['Multi', mode.capitalize()], style=st, genre=A['genre'], author='Trill Tron',
                        desc=f"{mode.capitalize()} patch: Layer A {ta} + Layer B {tb}.", a=a, b=b, **{'global': g}, note=48 if mode == 'split' else 60, dur=2.0, rel=4.0, multi=True))
    return out

# ------------------------------------------------------------------ render / normalise
def write_patch(path, pr, multi, glob):
    with open(path, 'w') as f:
        for k, v in pr['a'].items(): f.write(f'A {k} {v}\n')
        if multi:
            for k, v in pr['b'].items(): f.write(f'B {k} {v}\n')
        else: f.write('G mode 0\n')
        for k, v in glob.items(): f.write(f'G {k} {v}\n')
def render(pr, extra_vol=None):
    d = tempfile.mkdtemp(); path = os.path.join(d, 'p.txt'); multi = pr.get('multi', False)
    glob = dict(pr.get('global', {}))
    a = dict(pr['a'])
    if extra_vol is not None:
        a['vol'] = extra_vol
    q = dict(pr); q['a'] = a
    write_patch(path, q, multi, glob)
    notes = str(pr['note']) if not multi else f"{pr['note']},{pr['note'] + 24}"
    if multi and glob.get('mode') == 4: notes = f"{pr['note']}"
    o = subprocess.run([RENDER, WT, path, '-', notes, str(pr['dur']), str(pr['rel']), '105'], capture_output=True, text=True).stdout
    return json.loads(o)

TARGET_RMS = -20.0
def normalise(pr):
    pr['a'] = finalize(pr['a']);
    if 'b' in pr: pr['b'] = finalize(pr['b'])
    # layer B shares loudness via its own vol: scale both layers equally
    vol = 0.0
    for _ in range(10):
        pa = dict(pr); pa['a'] = dict(pr['a'], vol=vol)
        if 'b' in pr: pa['b'] = dict(pr['b'], vol=vol)
        glob = dict(pr.get('global', {}))
        d = tempfile.mkdtemp(); path = os.path.join(d, 'p.txt'); write_patch(path, pa, 'b' in pr, glob)
        notes = str(pr['note']) if 'b' not in pr else f"{pr['note']},{pr['note'] + 7}"
        s = json.loads(subprocess.run([RENDER, WT, path, '-', notes, str(pr['dur']), str(pr['rel']), '105'], capture_output=True, text=True).stdout)
        if s['nan'] or s['rms_db'] < -70: return False, s
        delta = max(TARGET_RMS - s['rms_db'], -6.0 - s['peak_db'])   # short sounds: aim peak ~ -6 dBFS
        delta = min(delta, 1.6 * (-1.0 - s['peak_db']) if s['peak_db'] > -1.0 else -1.0 - s['peak_db'])
        if abs(delta) < 0.3: break
        vol = max(-40, min(12, vol + delta))
    pr['a']['vol'] = round(vol, 2)
    if 'b' in pr: pr['b']['vol'] = round(vol, 2)
    pr['_stats'] = s
    ok = s['nan'] == 0 and s['peak_db'] <= -0.9 and s['rms_db'] > -45 and s['tail_db'] < -36 and s['tail_db'] - s['rms_db'] < -15
    return ok, s

def main():
    if not os.path.exists(RENDER): sys.exit('build rg_render first')
    singles = build_singles(); stem = build_stem_dna(); allp = stem + singles
    multis = build_multis(singles)
    allp += multis
    # unique names
    seen = {}
    for p in allp:
        n = p['name']; seen[n] = seen.get(n, 0) + 1
        if seen[n] > 1: p['name'] = f"{n} {seen[n]}"
    good, bad = [], []
    for i, p in enumerate(allp):
        ok, s = normalise(p)
        (good if ok else bad).append(p)
        if not ok: print('REJECT', p['name'], s, file=sys.stderr)
        if i % 100 == 0: print(f'{i}/{len(allp)}', file=sys.stderr)
    # replace rejects with re-rolled siblings so the library stays >= 500
    out_dir = os.path.join(ROOT, 'presets', 'factory'); os.makedirs(out_dir, exist_ok=True)
    for f in os.listdir(out_dir): os.remove(os.path.join(out_dir, f))
    factory = []
    for p in good:
        obj = dict(format=1, name=p['name'], author=p['author'], type=p['type'], character=p['character'], tags=p['tags'], style=p['style'], genre=p['genre'],
                   key=p.get('key', ''), bpm=p.get('bpm', 0), desc=p['desc'] or f"{p['character']} {p['type']} - {', '.join(p['style'])}.", a=p['a'])
        if 'b' in p: obj['b'] = p['b']; obj['global'] = p['global']
        if 'macros' in p: obj['macros'] = p['macros']
        factory.append(obj)
        with open(os.path.join(out_dir, re.sub(r'[^A-Za-z0-9 _.-]', '', p['name']) + '.rgp'), 'w') as f: json.dump(obj, f, indent=1)
    with open(os.path.join(ROOT, 'presets', 'factory.json'), 'w') as f: json.dump(factory, f, separators=(',', ':'))
    stats = {}
    for o in factory: stats[o['type']] = stats.get(o['type'], 0) + 1
    print(f'{len(factory)} presets written, {len(bad)} rejected', stats)
    rms = [p['_stats']['rms_db'] for p in good]; print('rms range', min(rms), max(rms))
if __name__ == '__main__': main()
