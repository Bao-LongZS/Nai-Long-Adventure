from pathlib import Path
out=Path(__file__).resolve().parent
path=out/'neck_extension_sculpt.py'
text=path.read_text(encoding='utf-8-sig')
marker='def attachment_height(angle):'
insert='''def neck_length_fraction(t,angle):
    # A broader derivative plateau distributes the added length smoothly.
    u=2*t-1
    fraction=(u-0.4*u**5+u**9/9+32/45)/(64/45)
    # Offset front/side/back curvature bands without changing either boundary.
    return fraction+0.08*math.sin(angle)*64*t**3*(1-t)**3

'''
text=text.replace(marker,insert+marker)
text=text.replace('p.z+=HEAD_RAISE*ease(NECK_BASE_Z,HEAD_BASE_Z,old_z)','p.z+=HEAD_RAISE*neck_length_fraction(t,angle)')
path.write_text(text,encoding='utf-8')
