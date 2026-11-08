"""Single source of truth for RAYGUN parameters. Run tools/gen_params.py to emit Source/ParamTable.h"""
TABLES=['Sine-Saw','Pulse Morph','Warm Organ','Glass Bell','Chrome FM','Vox Formant','Brass Stack','String Ens',
        'Digital Grit','Hollow Reed','Metal Pluck','Sub Heavy',
        'TRILL Dark Bed','TRILL Glass','TRILL Air','TRILL Current','TRILL Uzi']
TABLE_IDS=['sine_saw','pulse_morph','warm_organ','glass_bell','chrome_fm','vox_formant','brass_stack','string_ens',
        'digital_grit','hollow_reed','metal_pluck','sub_heavy',
        'TRILL_DARK_BED','TRILL_GLASS','TRILL_AIR','TRILL_CURRENT','TRILL_UZI']
WARP=['Off','Sync','Bend','Mirror','Fold']
SUB=['Sine','Tri','Square']
FILT=['LP 12','LP 24','HP 12','BP','Notch','Off']
LFO=['Sine','Tri','Saw','Square','S&H']
VOICE=['Poly','Mono','Legato']
ARPR=['1/4','1/8','1/8T','1/16','1/16T','1/32']
ARPM=['Up','Down','UpDown','Random','AsPlayed']
SRC=['None','LFO 1','LFO 2','Mod Env','Velocity','Mod Wheel','Key Track','Aftertouch','Random',
     'Macro 1','Macro 2','Macro 3','Macro 4','Macro 5','Macro 6','Macro 7','Macro 8']
DST=['None','Osc1 Pos','Osc2 Pos','Osc1 Pitch','Osc2 Pitch','Pitch','Cutoff','Resonance','Drive','Osc1 Level','Osc2 Level',
     'Unison Detune','Osc1 Warp','FM Amount','Amp','Pan','Osc2 Warp','Noise Level','Sub Level']
MODE=['A','B','A+B','Split','Velocity']
QUAL=['Eco','Clean','Hot']
MACROS=['Bright','Chorus','Attack','Release','Charge','Motion','Space','Drift']
# (id,name,kind,min,max,default,skew/choices)   kind: f=float i=int c=choice b=bool
def L():
    P=[]
    for o in (1,2):
        P+= [(f'o{o}_table',f'Osc{o} Table','c',0,len(TABLES)-1,0 if o==1 else 3,TABLES),
             (f'o{o}_pos',f'Osc{o} Position','f',0,1,0,None),
             (f'o{o}_oct',f'Osc{o} Octave','i',-3,3,0,None),
             (f'o{o}_semi',f'Osc{o} Semi','i',-12,12,0,None),
             (f'o{o}_fine',f'Osc{o} Fine','f',-100,100,0,None),
             (f'o{o}_lvl',f'Osc{o} Level','f',0,1,0.8 if o==1 else 0.0,None),
             (f'o{o}_warp',f'Osc{o} Warp','f',0,1,0,None),
             (f'o{o}_warpmode',f'Osc{o} Warp Mode','c',0,len(WARP)-1,0,WARP)]
    P+=[('sub_lvl','Sub Level','f',0,1,0,None),('sub_wave','Sub Wave','c',0,2,0,SUB),('noise_lvl','Noise Level','f',0,1,0,None),
        ('fm','FM Osc2>Osc1','f',0,1,0,None),
        ('uni_n','Unison Voices','i',1,7,1,None),('uni_det','Unison Detune','f',0,1,0.2,None),('uni_wid','Unison Width','f',0,1,0.5,None),
        ('f_type','Filter Type','c',0,5,1,FILT),('f_cut','Cutoff','f',20,20000,8000,'log'),('f_res','Resonance','f',0,1,0.1,None),
        ('f_drive','Filter Drive','f',0,1,0,None),('f_key','Filter Keytrack','f',0,1,0.3,None),('f_env','Filter Env Amount','f',-1,1,0,None),
        ('a_atk','Amp Attack','f',0.001,10,0.005,'log'),('a_dec','Amp Decay','f',0.005,10,0.3,'log'),('a_sus','Amp Sustain','f',0,1,0.8,None),('a_rel','Amp Release','f',0.005,10,0.2,'log'),
        ('e_atk','Env2 Attack','f',0.001,10,0.005,'log'),('e_dec','Env2 Decay','f',0.005,10,0.4,'log'),('e_sus','Env2 Sustain','f',0,1,0.0,None),('e_rel','Env2 Release','f',0.005,10,0.3,'log'),
        ('l1_rate','LFO1 Rate','f',0.05,30,2,'log'),('l1_shape','LFO1 Shape','c',0,4,0,LFO),
        ('l2_rate','LFO2 Rate','f',0.05,30,0.5,'log'),('l2_shape','LFO2 Shape','c',0,4,1,LFO),
        ('glide','Glide','f',0,2,0,'sq'),('voice','Voice Mode','c',0,2,0,VOICE),('maxv','Max Voices','i',1,16,8,None),('bend','Bend Range','i',0,24,2,None),
        ('chorus','Chorus','f',0,1,0,None),('delay_mix','Delay Mix','f',0,1,0,None),('delay_time','Delay Time','f',0.02,1,0.3,'log'),('delay_fb','Delay Feedback','f',0,0.95,0.35,None),
        ('verb_mix','Reverb Mix','f',0,1,0,None),('verb_size','Reverb Size','f',0,1,0.5,None),('charge','Charge','f',0,1,0,None),
        ('vol','Layer Volume dB','f',-60,12,0,None),('pan','Layer Pan','f',-1,1,0,None),('oct','Layer Octave','i',-3,3,0,None),
        ('keylo','Key Low','i',0,127,0,None),('keyhi','Key High','i',0,127,127,None),('midich','MIDI Channel','i',0,16,0,None),
        ('arp_on','Arp On','b',0,1,0,None),('arp_rate','Arp Rate','c',0,5,3,ARPR),('arp_mode','Arp Mode','c',0,4,0,ARPM),('arp_oct','Arp Octaves','i',1,3,1,None),('arp_gate','Arp Gate','f',0.1,1,0.6,None)]
    for m in range(1,9):
        P+=[(f'm{m}_src',f'Mod {m} Source','c',0,len(SRC)-1,0,SRC),(f'm{m}_dst',f'Mod {m} Dest','c',0,len(DST)-1,0,DST),(f'm{m}_amt',f'Mod {m} Amount','f',-1,1,0,None)]
    return P
G=[('mode','Layer Mode','c',0,4,3,MODE),('split','Split Point','i',0,127,48,None),('xfade','Split Crossfade','i',0,12,3,None),
   ('blend','A-B Blend','f',0,1,0.5,None),('vsplit','Velocity Split','i',1,127,96,None),('vfade','Velocity Fade','i',0,40,12,None),
   ('master','Master dB','f',-40,6,-3,None),('quality','Quality','c',0,2,1,QUAL)]
G+=[(f'macro{i+1}',f'Macro {i+1} {MACROS[i]}','f',0,1,0.5,None) for i in range(8)]
LAYER=L()
