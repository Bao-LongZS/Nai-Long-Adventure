from pathlib import Path
out=Path(__file__).resolve().parent
path=out/'neck_extension_sculpt.py'
code=path.read_text(encoding='utf-8-sig')
assert 'polar_row' not in code
code=code.replace('import bpy\nfrom mathutils','import bpy\nimport math\nfrom mathutils')
helper="""def quintic(z, a, b, v0, d0, s0, v1, d1, s1):
    h=b-a
    t=(z-a)/h
    c0=v0; c1=h*d0; c2=0.5*h*h*s0
    dv=v1-c0-c1-c2; dd=h*d1-c1-2*c2; ds=h*h*s1-2*c2
    c3=10*dv-4*dd+0.5*ds
    c4=-15*dv+7*dd-ds
    c5=6*dv-3*dd+0.5*ds
    return c0+c1*t+c2*t*t+c3*t**3+c4*t**4+c5*t**5


"""
code=code.replace('def extend_neck(body, shell=None, move_features=True):',helper+'def extend_neck(body, shell=None, move_features=True):')
angular="""        # Fit the complete perimeter and its tangents at both frozen ends.
        angular_count=128
        jet_eps=0.012
        def polar_row(source_z):
            rx,f,r=section(obj,source_z)
            cy=(f+r)/2; depth=(r-f)/2
            origin=Vector((0,cy,source_z))
            points=[]
            for j in range(angular_count):
                angle=2*math.pi*j/angular_count
                direction=Vector((rx*math.cos(angle),depth*math.sin(angle),0)).normalized()
                hit,point,_,_=obj.ray_cast(origin,direction)
                assert hit, ('Polar section missed',source_z,j)
                points.append((point.x,point.y))
            return points
        low_rows=[polar_row(NECK_BASE_Z+d) for d in (-jet_eps,0,jet_eps)]
        high_rows=[polar_row(HEAD_BASE_Z+d) for d in (-jet_eps,0,jet_eps)]
        def ellipse_at(z,angle):
            w,f,r=target(z)
            return (w*math.cos(angle),(f+r)/2+(r-f)/2*math.sin(angle))
        low_jets=[]; high_jets=[]
        for j in range(angular_count):
            angle=2*math.pi*j/angular_count
            t0=ellipse_at(NECK_BASE_Z,angle)
            t1=ellipse_at(NECK_BASE_Z+0.002,angle)
            t2=ellipse_at(NECK_BASE_Z+0.004,angle)
            u0=ellipse_at(NECK_TOP_Z,angle)
            u1=ellipse_at(NECK_TOP_Z-0.002,angle)
            u2=ellipse_at(NECK_TOP_Z-0.004,angle)
            lj=[]; hj=[]
            for k in range(2):
                a0=low_rows[1][j][k]
                ad=(low_rows[2][j][k]-low_rows[0][j][k])/(2*jet_eps)
                ads=(low_rows[2][j][k]-2*a0+low_rows[0][j][k])/(jet_eps**2)
                td=(-3*t0[k]+4*t1[k]-t2[k])/0.004
                tds=(t0[k]-2*t1[k]+t2[k])/0.002**2
                lj.append((a0-t0[k],ad-td,max(-12,min(12,ads-tds))))
                a0=high_rows[1][j][k]
                ad=(high_rows[2][j][k]-high_rows[0][j][k])/(2*jet_eps)
                ads=(high_rows[2][j][k]-2*a0+high_rows[0][j][k])/(jet_eps**2)
                td=(3*u0[k]-4*u1[k]+u2[k])/0.004
                tds=(u0[k]-2*u1[k]+u2[k])/0.002**2
                hj.append((a0-u0[k],ad-td,max(-12,min(12,ads-tds))))
            low_jets.append(lj); high_jets.append(hj)
        def angular_value(jets,angle,k):
            index=(angle%(2*math.pi))/(2*math.pi)*angular_count
            lo=int(index)%angular_count; t=index-int(index)
            return tuple(jets[lo][k][q]*(1-t)+jets[(lo+1)%angular_count][k][q]*t for q in range(3))
        def polar_target(z,angle):
            point=list(ellipse_at(z,angle))
            if z<NECK_BASE_Z+0.075:
                for k in range(2):
                    v,d,s=angular_value(low_jets,angle,k)
                    point[k]+=quintic(z,NECK_BASE_Z,NECK_BASE_Z+0.075,v,d,s,0,0,0)
            if z>NECK_TOP_Z-0.095:
                for k in range(2):
                    v,d,s=angular_value(high_jets,angle,k)
                    point[k]+=quintic(z,NECK_TOP_Z-0.095,NECK_TOP_Z,0,0,0,v,d,s)
            return point

"""
code=code.replace('        rigid_count = 0\n        neck_count = 0',angular+'        rigid_count = 0\n        neck_count = 0')
old="""            new_width, new_front, new_rear = target(z)
            p.x *= new_width/width
            p.y = (new_front+new_rear)/2+(p.y-(front+rear)/2)*(new_rear-new_front)/(rear-front)
            p.z = z"""
new="""            angle=math.atan2((p.y-(front+rear)/2)/((rear-front)/2),p.x/width)
            p.x,p.y=polar_target(z,angle)
            p.z=z"""
assert old in code
code=code.replace(old,new)
path.write_text(code,encoding='utf-8')
print('POLAR_NECK_BOUNDARY_REFINEMENT_READY')

