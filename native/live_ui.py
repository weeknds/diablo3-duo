#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Native lower-screen character sheet, equipment ledger and exploration view."""
import copy
import json
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
MODULE = 'modules/android-arm64-v8a/01001B300B9BE000.so'
MODULE_HASH = '42827d0926b6b5c3b4b4a4b1cad6308164d7e946d803127b20f7c97134e9a9e3'
BUILD_ID = '2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000'
VERSION = '0.2.2-dev'
ASSETS = ROOT / 'assets'
C = {'bg':'11110E', 'panel':'191813', 'selected':'242117', 'rule':'49412E',
     'gold':'D7B574', 'text':'F1EBDD', 'muted':'BDB4A0'}


def color(name):
    return '#FF' + C.get(name, name)


def text_width(text, scale):
    metrics = (ASSETS / 'duo-sans.mfnt').read_bytes()
    cap = struct.unpack_from('<I', metrics, 0x1C)[0]
    first = struct.unpack_from('<I', metrics, 0x28)[0]
    advance = sum(struct.unpack_from('<H', metrics, first + (ord(c) - 31) * 14 + 12)[0]
                  for c in text)
    return math.ceil(advance * scale * 5 / cap)


def rect(x, y, w, h, bg='panel', stroke=None, tap=None):
    widget = {'type':'rect', 'rect':[x,y,w,h], 'bg':color(bg),
              'color':color(stroke) if stroke else 0, 'frame':1 if stroke else 0}
    if tap:
        widget['on_tap'] = tap
    return widget


def label(text, x, y, scale=5, ink='text', width=1100, ident=None, min_scale=None):
    widget = {'type':'label', 'rect':[x,y,0,0], 'text':text, 'text_scale':scale,
              'color':color(ink), 'wrap_width':min(width,1240-x), 'max_lines':1,
              'fit_text':True, 'text_min_scale':min_scale or scale}
    if ident:
        widget['id'] = ident
    return widget


def image(name, x, y, w, h, tint=None):
    widget = {'type':'image', 'rect':[x,y,w,h], 'src':f'file:assets/{name}.png'}
    if tint:
        widget['tint'] = color(tint)
    return widget


def pair(widget, key):
    offline = copy.deepcopy(widget)
    offline['id'] += '_offline'
    offline['need_bind'] = '!module_ready'
    live = copy.deepcopy(widget)
    live.update(bind_text=key, need_bind='module_ready', hide_bind='module_error', hide_eq=1)
    return [offline, live]


def live(key, ident, x, y, width, scale=5, ink='text', fallback='Unavailable', minimum=4):
    return pair(label(fallback,x,y,scale,ink,width,ident,min_scale=minimum),key)


def button(title, action, x, y, w, h=72, icon=None):
    result = [rect(x,y,w,h,'selected','rule',action)]
    tw = text_width(title,4)
    if icon:
        left = x + (w-tw-44)//2
        result += [image('icon-'+icon,left,y+(h-30)//2,30,30,'gold')]
        left += 44
    else:
        left = x+(w-tw)//2
    result += [label(title,left,y+(h-20)//2,4,'gold',w-32)]
    return result


def chrome(page):
    result = [image('icon-skills',40,22,34,34,'gold'), image('brand',94,27,500,41),
              label('LIVE PREVIEW',1030,33,3,'muted',168,'preview_badge'),
              rect(40,77,1160,1,'rule'), rect(0,982,1240,98,'bg'),
              rect(0,982,1240,1,'rule')]
    for i,(key,title,icon) in enumerate((('character','Character','character'),
                                        ('map','Map','map'),('combat','Skills','skills'))):
        x = i*413
        active = page == key or (page == 'equipment' and key == 'character')
        gx = x+(413-46-text_width(title,5))//2
        result += [rect(x,984,414 if i==2 else 413,96,'selected' if active else 'bg',tap='open_'+key),
                   image('icon-'+icon,gx,1015,32,32,'gold' if active else 'muted'),
                   label(title,gx+46,1019,5,'gold' if active else 'muted',270)]
        if active:
            result += [rect(x+44,981,325,3,'gold')]
    return result


def status(key='details.status', ident='connection', fallback='Waiting for the companion to connect.'):
    widgets = live(key,ident,44,947,1152,3,'muted',fallback,3)
    widgets[0].update(hide_bind='module_error',hide_eq=1)
    error = label('Companion unavailable. Check the game version and reload.',44,947,
                  3,'muted',1152,ident+'_error')
    error['need_bind'] = 'module_error'
    return widgets+[error]


def character():
    widgets = chrome('character')
    title = image('class-0',44,118,820,66)
    title.update(bind='details.class_index', src_names=[f'file:assets/class-{i}.png' for i in range(8)],
                 need_bind='module_ready',hide_bind='module_error',hide_eq=1)
    title_offline = image('class-0',44,118,820,66)
    title_offline.update(need_bind='!module_ready',hide_bind='module_error',hide_eq=1)
    title_error = image('class-0',44,118,820,66)
    title_error['need_bind'] = 'module_error'
    widgets += [title_offline,title_error,title]
    widgets += live('details.level','level',46,198,260,4)
    widgets += live('details.paragon','paragon',326,198,470,4,'muted')
    widgets += button('Equipment','open_equipment',940,121,256,74,'gear')
    widgets += [rect(44,258,1152,124,'panel','rule')]
    for i,(name,field) in enumerate((('Strength',4),('Dexterity',5),('Intelligence',6),('Vitality',7))):
        x = 72+i*288
        widgets += [label(name,x,282,4,'muted',236)]
        widgets += live(f'stats.{field}.value','attribute_'+str(field),x,321,232,7,minimum=3)
        if i<3:
            widgets += [rect(x+259,282,1,76,'rule')]
    widgets += [image('section-combat',44,421,540,41),rect(44,474,1152,432,'panel','rule'),
                rect(620,494,1,392,'rule')]
    stats = [('Attack speed',0,'aps','sword'),('Armor',2,'armor','shield'),
             ('Critical chance',8,'critical',None),('Cooldown reduction',1,'cdr',None),
             ('Resource cost reduction',9,'cost',None),('Movement bonus',3,'movement','movement')]
    for i,(title,field,ident,icon) in enumerate(stats):
        x,y = 76+i%2*576,497+i//2*144
        widgets += [label(title,x,y,4,'muted',490)]
        widgets += live(f'stats.{field}.value',ident,x,y+41,510,9,minimum=4)
        if icon:
            widgets += [image('icon-'+icon,x+470,y+2,28,28,'gold')]
        if i<4:
            widgets += [rect(x,y+123,512,1,'rule')]
    return widgets+status()


# Reader order follows equipped-slot keys 1..13. The native left-hand slot
# is the main-hand weapon; keep the familiar Main hand / Off-hand display order.
GEAR = [('Head','helm'),('Torso','gear'),('Main hand','sword'),('Off-hand','shield'),
        ('Hands','hands'),('Waist','belt'),('Feet','movement'),('Shoulders','shoulders'),
        ('Legs','legs'),('Bracers','bracers'),('Right ring','ring'),('Left ring','ring'),('Neck','neck')]
GEAR_SLOT_INDICES = (0,1,3,2,4,5,6,7,8,9,10,11,12)


def equipment():
    widgets = chrome('equipment')+[image('title-equipment',44,115,820,60),
                                  label('Tap a slot to inspect its equipped item',46,182,4,'muted')]
    widgets += button('Character','open_character',956,121,240,74,'character')
    for row,(title,icon) in enumerate(GEAR):
        i = GEAR_SLOT_INDICES[row]
        x,y = 44+(row%2)*588,230+(row//2)*80
        widgets += [rect(x,y,564,70,'panel','rule','inspect_'+str(i)),image('icon-'+icon,x+16,y+14,42,42,'gold'),
                    label(title,x+80,y+10,3,'muted',444)]
        widgets += live(f'gear.{i}.name','gear_'+str(i),x+80,y+35,460,4,minimum=4)
    widgets += [rect(44,810,1152,116,'selected','rule')]
    widgets += live('gear.selected_slot','selected_slot',66,827,1108,3,'gold','Select an equipment slot',3)
    widgets += live('gear.selected_name','selected_name',66,856,1108,6,'text','Item inspection',4)
    widgets += live('gear.inspect_note','inspect_note',66,903,1108,3,'muted','Tap an equipped slot to see its base item name.',3)
    return widgets+status()


def skills():
    widgets = chrome('combat')+[image('title-skills',44,115,820,60),
                               label('Your equipped skills and selected runes',46,190,4,'muted')]
    for i in range(6):
        x,y = 44+(i%2)*588,266+(i//2)*202
        widgets += [rect(x,y,564,170,'panel','rule'),rect(x+22,y+25,44,44,'selected','rule'),
                    label(str(i+1),x+36,y+38,4,'gold',24)]
        widgets += live(f'skills.{i}.name','skill_'+str(i),x+90,y+30,448,5,minimum=4)
        widgets += [rect(x+90,y+82,444,1,'rule')]
        widgets += live(f'skills.{i}.rune','rune_'+str(i),x+90,y+112,448,4,'muted',minimum=3)
    widgets += [label('Changing your loadout in the game updates this view.',46,896,3,'muted')]
    return widgets+status()


def exploration():
    widgets = chrome('map')+[image('title-map',44,115,820,60)]
    widgets += button('Next marker','poi_next',940,121,256,74,'pin')
    widgets += live('map.description','map_description',88,200,1108,4,'muted',
                    'Enter the world to begin exploring.')
    widgets += [rect(44,246,1152,580,'panel','rule')]
    # A real map widget keeps terrain cached while its camera and markers move.
    widgets += [{'type':'map','id':'exploration','area':'exploration',
                 'rect':[44,246,1152,580],'bg':'#FF141410',
                 'image_bind':'map.image','need_bind':'map.ready',
                 'hide_bind':'module_error','hide_eq':1,
                 'marker_x_bind':'map.player.x','marker_y_bind':'map.player.y',
                 'marker_src':'file:assets/icon-player.png','marker_size':[28,28],
                 'marker_anchor':[0.5,0.5],'marker_scale':1,
                 'view_rect_x0_bind':'map.view.x0','view_rect_x1_bind':'map.view.x1',
                 'view_rect_y0_bind':'map.view.y0','view_rect_y1_bind':'map.view.y1',
                 'view_rect_pad':0,'pan_zoom':True,'min_zoom':0.25,'max_zoom':8,
                 'on_marker_tap':'poi_select','marker_tap_groups':['poi'],'marker_hit_px':32}]
    # Preload outside the opaque map (the runtime culls covered map widgets).
    # A normal opaque rectangle covers these 1px workers without suppressing them.
    preloader=image('map-background',44,85,1,1)
    preloader.update(src_bind='map.pending',need_bind='map.available')
    widgets += [preloader,image('icon-player',45,85,1,1),image('icon-map-pin',46,85,1,1),
                *[image('icon-poi-'+kind,47+i,85,1,1)
                  for i,kind in enumerate(('quest','portal','waypoint','shrine','pylon','goblin'))],
                rect(44,85,9,1,'bg')]
    direction=image('icon-direction',46,196,28,28)
    direction.update(rotate_bind='map.focus.angle',pivot=[14,14],
                     need_bind='map.focus.direction',hide_bind='module_error',hide_eq=1)
    widgets += [direction]
    empty=label('Exploration data unavailable',399,505,5,'muted',630)
    empty.update(need_bind='!map.ready',bind_text='map.waiting',hide_bind='module_error',hide_eq=1)
    widgets += [empty]
    for title,action,x in (('+','map_zoom_in',1110),('-', 'map_zoom_out',1018)):
        widgets += button(title,action,x,842,78,70)
    widgets += button('Center on you','map_recenter',44,842,296,70,'recenter')
    widgets += button('Pin location','map_pin',362,842,286,70,'pin')
    widgets += button('Clear pin','map_clear_pin',670,842,246,70)
    return widgets+status('map.status','map_status',
                          'Explored areas appear as you move through the world.')


def make_manifest():
    actions = {f'open_{key}':{'kind':'page','page':key} for key in ('character','equipment','combat','map')}
    actions.update({f'inspect_{i}':{'kind':'module','action':'inspect_equipment','argument':i}
                    for i in range(len(GEAR))})
    actions.update({name:{'kind':'module','action':name,'argument':0}
                    for name in ('map_zoom_in','map_zoom_out','map_recenter','map_pin','map_clear_pin','poi_next')})
    actions['map_recenter']={'kind':'view_reset','view':'exploration'}
    actions['poi_select']={'kind':'module','action':'poi_select','argument':'$payload'}
    actions['map_sync_view']={'kind':'view_reset','view':'exploration','enabled_bind':'map.reset_pending'}
    actions['map_sync_selection']={'kind':'map_select','group':'poi','value':'$map.poi.selected_slot',
                                   'enabled_bind':'map.selection_pending'}
    return {'format':1,'name':'Diablo III Duo - Live Preview','title_id':'01001B300B9BE000',
            'min_runtime':18,'canvas_w':1240,'canvas_h':1080,'background':color('bg'),
            'flags':{'gpu_composite':True},
            'nav':False,'requires_module':True,'module_tick_hidden':False,
            'font':'file:assets/duo-sans.mfnt','font_atlas':'file:assets/duo-sans.png',
            'module':{'abi':1,'build_ids':[BUILD_ID],
                      'libraries':{'android-arm64-v8a':{'path':MODULE,'sha256':MODULE_HASH}}},
            'map':{'areas':{'exploration':{
                'min':[0,0],'max':[2304*64,1160*64],
                'image':'file:assets/map-background.png','no_pin':False,'clamp_view':False,
                'dynamic_markers':[{'group':'poi','count':64,'x':'map.poi.{i}.x','y':'map.poi.{i}.y',
                    'kind':'map.poi.{i}.kind','hide_when_kind':0,'icon_src_bind':'map.poi.{i}.icon',
                    'show_bind':'map.ready','size':34,'selected_size':46,'anchor':[0.5,0.5]},
                    {'count':1,'x':'map.pin.x','y':'map.pin.y',
                    'icon_src_bind':'map.pin.image','show_bind':'map.pinned',
                    'size':32,'anchor':[0.5,0.9]}]}},'style':{'opacity':1.0}},
            'enforce':[{'action':'map_sync_view','every_ms':1},{'action':'map_sync_selection','every_ms':1}],
            'enforce_gate':{'point':'map.action_pending','max':1},
            'actions':actions,
            'pages':[{'id':key,'title':title,'widgets':build()}
                     for key,title,build in (('character','Character',character),('equipment','Equipment',equipment),
                                             ('combat','Skills',skills),('map','Exploration',exploration))]}
