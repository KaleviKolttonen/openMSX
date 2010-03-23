# $Id$

from components import EmulationCore, GLRenderer, Laserdisc, iterComponents

class Configuration(object):

	def __init__(self, requiredComponents, optionalComponents, linkStatic):
		self.__requiredComponents = requiredComponents
		self.__optionalComponents = optionalComponents
		self.__linkStatic = linkStatic

	def iterRequiredComponents(self):
		return iter(self.__requiredComponents)

	def iterOptionalComponents(self):
		return iter(self.__optionalComponents)

	def iterDesiredComponents(self):
		return iter(self.__requiredComponents | self.__optionalComponents)

	def linkStatic(self):
		'''Returns True iff static linking should be used for non-system libs.
		'''
		return self.__linkStatic

def getConfiguration(name):
	if name == 'SYS_DYN':
		requiredComponents = set((EmulationCore, ))
		optionalComponents = set(iterComponents()) - requiredComponents
		linkStatic = False
	elif name == '3RD_STA':
		requiredComponents = set((EmulationCore, GLRenderer))
		optionalComponents = set((Laserdisc, ))
		linkStatic = True
	elif name == '3RD_STA_MIN':
		requiredComponents = set((EmulationCore, ))
		optionalComponents = set()
		linkStatic = True
	else:
		raise ValueError('No configuration named "%s"' % name)
	return Configuration(requiredComponents, optionalComponents, linkStatic)
