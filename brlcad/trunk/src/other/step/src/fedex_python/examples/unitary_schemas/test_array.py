# This file was generated by fedex_python.  You probably don't want to edit
# it since your modifications will be lost if fedex_plus is used to
# regenerate it.
import sys

from SCL.SCLBase import *
from SCL.SimpleDataTypes import *
from SCL.ConstructedDataTypes import *
from SCL.AggregationDataTypes import *
from SCL.TypeChecker import check_type
from SCL.Builtin import *
from SCL.Rules import *

schema_name = 'test_array'

schema_scope = sys.modules[__name__]


####################
 # ENTITY point #
####################
class point(BaseEntityClass):
	'''Entity point definition.

	:param coords
	:type coords:ARRAY(1,3,'REAL', scope = schema_scope)
	'''
	def __init__( self , coords, ):
		self.coords = coords

	@apply
	def coords():
		def fget( self ):
			return self._coords
		def fset( self, value ):
		# Mandatory argument
			if value==None:
				raise AssertionError('Argument coords is mantatory and can not be set to None')
			if not check_type(value,ARRAY(1,3,'REAL', scope = schema_scope)):
				self._coords = ARRAY(value)
			else:
				self._coords = value
		return property(**locals())
