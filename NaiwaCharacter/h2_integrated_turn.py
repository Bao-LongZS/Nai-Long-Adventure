"""Low-degree final front curves, specified by shape and real endpoint tangents.

The two free Bernstein angle coefficients fit two broad *final positions*.
Integrating these angles replaces the old narrow circle plus a displaced lobe.
Each radial column uses its own frozen endpoints; all columns are subsequently
fit together by the periodic surface basis and final constrained surface solve.
"""
import math
import numpy as np


def integrated_front_curves(grid, zs, front_weights, lower_inset=.020,
                            underface_inset=.010, terminal_radius=.055,
                            lateral_turn=False):
    za, zm, anchor = 2.22, 2.400440216064, 2.34
    zz = np.linspace(za, zm, 801)
    t = (zz-za)/(zm-za)
    basis = np.column_stack([math.comb(5,k)*t**k*(1-t)**(5-k) for k in range(6)])
    curves = grid[:,:,1].copy()
    reports = []
    upper_ends=np.full(grid.shape[1],zm)
    for j, weight in enumerate(front_weights):
        if weight < 1e-5:
            continue
        source = grid[:,j,1]
        slopes = np.gradient(source, zs)
        if lateral_turn:
            x=abs(float(np.interp(2.40,zs,grid[:,j,0])))
            lateral=np.clip((x-.060)/.145,0.,1.)
            lateral=lateral**3*(10+lateral*(-15+6*lateral))
            # A feature-adjacent return near the center blends continuously
            # into longer cheek arcs, instead of ending every front column at
            # one horizontal mouth-height shelf.
            zm=2.390+.055*lateral
            anchor=2.340+.018*lateral
            zz=np.linspace(za,zm,801);t=(zz-za)/(zm-za)
            basis=np.column_stack([math.comb(5,k)*t**k*(1-t)**(5-k) for k in range(6)])
            upper_ends[j]=zm
        beta0 = np.arctan(-np.interp(za,zs,slopes))
        beta5 = np.arctan(-np.interp(zm,zs,slopes))
        final_beta4=beta5+(zm-za)*weight/(5*terminal_radius)
        if lateral_turn:
            take=np.abs(zs-zm)<.012
            cc=np.polyfit(zs[take]-zm,source[take],3)
            slope=float(np.interp(zm,zs,slopes))
            beta_derivative=np.clip(-2*cc[-3]/(1+slope*slope),-25.,25.)
            final_beta4=beta5-(zm-za)*beta_derivative/5
        coeff = np.array([beta0,beta0,1.,2.6,
                          final_beta4,beta5])
        goals = np.array([np.interp(za,zs,source)+lower_inset*weight,
                          np.interp(anchor,zs,source)+(underface_inset+.004*float(lateral_turn))*weight])
        mouth = np.interp(zm,zs,source)
        def evaluate(x):
            c=coeff.copy();c[2:4]=x
            beta=basis@c
            # Singular tangents are rejected, not hidden by clipping them.
            if np.max(np.abs(beta)) >= np.radians(79.):
                return None
            slope=np.tan(beta)
            increments=(slope[:-1]+slope[1:])*.5*np.diff(zz)
            yy=mouth+np.r_[np.cumsum(increments[::-1])[::-1],0.]
            return np.array([yy[0],np.interp(anchor,zz,yy)]),beta,yy
        x=coeff[2:4].copy()
        if evaluate(x) is None:
            x=np.array([beta0,beta5])
        for iteration in range(80):
            values,beta,yy=evaluate(x)
            error=values-goals
            if np.max(np.abs(error))<1e-9:
                break
            jac=np.column_stack([(evaluate(x+np.eye(2)[k]*1e-5)[0]-values)/1e-5
                                  for k in range(2)])
            step=np.linalg.solve(jac,error)
            accepted=False
            for damping in (1.,.5,.25,.125,.0625,.03125):
                trial=x-step*damping
                result=evaluate(trial)
                if result is not None and np.linalg.norm(result[0]-goals)<np.linalg.norm(error):
                    x=trial;accepted=True;break
            if not accepted:
                break
        values,beta,yy=evaluate(x)
        residual=float(np.max(np.abs(values-goals)))
        if residual>5e-5:
            # Weak side columns need not force a nearly vertical front tangent.
            # Their source remains the observation in the joint periodic fit.
            reports.append({'column':j,'weight':float(weight),'used':False,
                            'maximum_position_residual':residual})
            continue
        delta=yy-np.interp(zz,zs,source)
        curves[:,j]=source+np.interp(zs,zz,delta,left=float(delta[0]),right=0.)
        reports.append({'column':j,'weight':float(weight),'used':True,
                        'upper_Z':zm,'underface_anchor_Z':anchor,
                        'maximum_position_residual':residual,
                        'maximum_actual_angle_degrees':float(np.degrees(beta).max()),
                        'minimum_actual_angle_degrees':float(np.degrees(beta).min()),
                        'angle_controls_degrees':np.degrees(np.r_[coeff[:2],x,coeff[4:]]).tolist(),
                        'delta_Y_range':[float(delta.min()),float(delta.max())]})
    return curves, {'enabled':True,'definition':'integrated quintic Bernstein angle field; final positions plus real endpoint tangents',
                    'lower_Z':za,'mouth_Z':zm,'underface_anchor_Z':anchor,
                    'lower_inset':lower_inset,'underface_inset':underface_inset,
                    'terminal_radius_prior':terminal_radius,'lateral_feature_endpoints':lateral_turn,
                    'upper_end_Z':upper_ends.tolist(),'angle_degrees_of_freedom_per_column':6,
                    'free_coefficients_per_column':2,'columns':reports}
