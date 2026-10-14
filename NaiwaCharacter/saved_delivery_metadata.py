"""Read-only snapshots of saved object links, materials and action curves."""
import bpy

def scalar(value):
    if isinstance(value,(str,bool,int,float)) or value is None:return value
    if isinstance(value,bpy.types.ID):return value.name
    try:return [scalar(v) for v in value]
    except TypeError:return str(value)

def settings(item):
    """Snapshot saved RNA settings without construction flags or data pointers."""
    out={}
    for prop in item.bl_rna.properties:
        if prop.identifier in ('rna_type','name','type','execution_time','is_active','session_uid') or prop.type=='COLLECTION':continue
        if prop.type=='POINTER':
            value=getattr(item,prop.identifier,None)
            if value is None or isinstance(value,bpy.types.ID):out[prop.identifier]=scalar(value)
        elif prop.type in ('BOOLEAN','INT','FLOAT','STRING','ENUM'):
            try:out[prop.identifier]=scalar(getattr(item,prop.identifier))
            except (AttributeError,TypeError):pass
    return out

def relationships(obj):
    return {'type':obj.type,'parent':obj.parent.name if obj.parent else None,
        'parent_type':obj.parent_type,'parent_bone':obj.parent_bone,
        'parent_inverse':list(map(list,obj.matrix_parent_inverse)),
        'material_slots':[(s.name,s.link) for s in obj.material_slots] if obj.type=='MESH' else [],
        'modifiers':[(m.name,m.type,settings(m)) for m in obj.modifiers],
        'constraints':[(c.name,c.type,settings(c)) for c in obj.constraints]}

def material_snapshot(material):
    out={'settings':settings(material),'nodes':[],'links':[]}
    if material.node_tree:
        for node in material.node_tree.nodes:
            out['nodes'].append({'name':node.name,'type':node.bl_idname,'settings':settings(node),
                'inputs':[(s.identifier,scalar(s.default_value)) for s in node.inputs if hasattr(s,'default_value')]})
        out['links']=[(l.from_node.name,l.from_socket.identifier,l.to_node.name,l.to_socket.identifier)
                      for l in material.node_tree.links]
    return out

def curve_snapshot(curve):
    return {'path':curve.data_path,'index':curve.array_index,'settings':settings(curve),
        'keyframes':[{'settings':settings(k)} for k in curve.keyframe_points],
        'samples':[list(p.co) for p in curve.sampled_points],
        'modifiers':[(m.type,settings(m)) for m in curve.modifiers]}

def actions_snapshot():
    out={}
    for action in bpy.data.actions:
        curves=[]
        if hasattr(action,'fcurves'):curves.extend(action.fcurves)
        for layer in getattr(action,'layers',[]):
            for strip in layer.strips:
                for bag in getattr(strip,'channelbags',[]):curves.extend(bag.fcurves)
        out[action.name]={'range':list(action.frame_range),
            'curves':[curve_snapshot(c) for c in curves]}
    return out

def preserved_links():
    return {'objects':{o.name:relationships(o) for o in bpy.data.objects if o.type in ('MESH','ARMATURE')},
        'materials':{m.name:material_snapshot(m) for m in bpy.data.materials},
        'actions':actions_snapshot()}

def differences(a,b,path=''):
    if isinstance(a,dict) and isinstance(b,dict):
        out=[]
        for key in sorted(set(a)|set(b)):
            out.extend(differences(a.get(key),b.get(key),path+'/'+str(key)))
        return out
    if isinstance(a,(list,tuple)) and isinstance(b,(list,tuple)) and len(a)==len(b):
        out=[]
        for i,(x,y) in enumerate(zip(a,b)):out.extend(differences(x,y,path+'/'+str(i)))
        return out
    return [] if a==b else [{'path':path,'source':a,'candidate':b}]

