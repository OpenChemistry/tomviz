from tomviz import  state

state.load(open('/home/cjh/tmp/recon1.tvsm').read())
gop = state.pipelines[0].datasource.operators[1]
gop.sigma
gop.sigma = 5

from tomviz.state import modules
m = modules.Outline()
state.pipelines[0].datasource.add_module(m)

state.sync()
