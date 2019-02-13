# This file was generated by exp2python.  You probably don't want to edit
# it since your modifications will be lost if exp2python is used to
# regenerate it.
import sys

from SCL.SCLBase import *
from SCL.SimpleDataTypes import *
from SCL.ConstructedDataTypes import *
from SCL.AggregationDataTypes import *
from SCL.TypeChecker import check_type
from SCL.Builtin import *
from SCL.Rules import *

schema_name = 'index_attribute'

schema_scope = sys.modules[__name__]

common_datum_list = LIST(1,None,'datum_reference_element', scope = schema_scope)
# Defined datatype label
class label(STRING):
	def __init__(self,*kargs):
		pass

# SELECT TYPE datum_or_common_datum
datum_or_common_datum = SELECT(
	'common_datum_list',
	'datum',
	scope = schema_scope)

####################
 # ENTITY shape_aspect #
####################
class shape_aspect(BaseEntityClass):
	'''Entity shape_aspect definition.

	:param name
	:type name:label

	:param of_shape
	:type of_shape:product_definition_shape
	'''
	def __init__( self , name,of_shape, ):
		self.name = name
		self.of_shape = of_shape

	@apply
	def name():
		def fget( self ):
			return self._name
		def fset( self, value ):
		# Mandatory argument
			if value==None:
				raise AssertionError('Argument name is mantatory and can not be set to None')
			if not check_type(value,label):
				self._name = label(value)
			else:
				self._name = value
		return property(**locals())

	@apply
	def of_shape():
		def fget( self ):
			return self._of_shape
		def fset( self, value ):
		# Mandatory argument
			if value==None:
				raise AssertionError('Argument of_shape is mantatory and can not be set to None')
			if not check_type(value,product_definition_shape):
				self._of_shape = product_definition_shape(value)
			else:
				self._of_shape = value
		return property(**locals())
	def wr1(self):
		eval_wr1_wr = (SIZEOF(USEDIN(self,'INDEX_ATTRIBUTE.'  +  'ID_ATTRIBUTE.IDENTIFIED_ITEM'))  <=  1)
		if not eval_wr1_wr:
			raise AssertionError('Rule wr1 violated')
		else:
			return eval_wr1_wr


####################
 # ENTITY general_datum_reference #
####################
class general_datum_reference(shape_aspect):
	'''Entity general_datum_reference definition.

	:param base
	:type base:datum_or_common_datum
	'''
	def __init__( self , inherited0__name , inherited1__of_shape , base, ):
		shape_aspect.__init__(self , inherited0__name , inherited1__of_shape , )
		self.base = base

	@apply
	def base():
		def fget( self ):
			return self._base
		def fset( self, value ):
		# Mandatory argument
			if value==None:
				raise AssertionError('Argument base is mantatory and can not be set to None')
			if not check_type(value,datum_or_common_datum):
				self._base = datum_or_common_datum(value)
			else:
				self._base = value
		return property(**locals())
	def wr1(self):
		eval_wr1_wr = (( not ('INDEX_ATTRIBUTE.COMMON_DATUM_LIST'  ==  TYPEOF(self.base)))  or  (self.self.shape_aspect.self.of_shape  ==  self.base[1].self.shape_aspect.self.of_shape))
		if not eval_wr1_wr:
			raise AssertionError('Rule wr1 violated')
		else:
			return eval_wr1_wr


####################
 # ENTITY product_definition_shape #
####################
class product_definition_shape(BaseEntityClass):
	'''Entity product_definition_shape definition.
	'''
	# This class does not define any attribute.
	pass

####################
 # ENTITY datum_reference_element #
####################
class datum_reference_element(general_datum_reference):
	'''Entity datum_reference_element definition.
	'''
	def __init__( self , inherited0__name , inherited1__of_shape , inherited2__base ,  ):
		general_datum_reference.__init__(self , inherited0__name , inherited1__of_shape , inherited2__base , )

####################
 # ENTITY datum #
####################
class datum(shape_aspect):
	'''Entity datum definition.
	'''
	def __init__( self , inherited0__name , inherited1__of_shape ,  ):
		shape_aspect.__init__(self , inherited0__name , inherited1__of_shape , )
