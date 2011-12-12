from fabric.api import *
from string import Template
import os

def fill_in_template(template_name,**arguments):
	source=open(os.path.join(env.localroot,'deploy','templates',template_name))
	binding=env.copy()
	binding.update(arguments)
	result=Template(source.read()).substitute(binding)
	destname=os.path.join(env.localroot,'deploy','.jobscripts',"%s.sh"%binding['name'])
	target=open(destname,'w')
	target.write(result)
	return destname