# - Find STEP Class Library binaries and libraries
#
# The following variables are set:
#
# SCL_INCLUDE_DIR
# SCL_EXPRESS_EXECUTABLE
# SCL_SYMLINK_EXECUTABLE
# SCL_EXPPP_EXECUTABLE
# SCL_FEDEX_OS_EXECUTABLE
# SCL_FEDEX_IDL_EXECUTABLE
# SCL_FEDEX_PLUS_EXECUTABLE
# SCL_EXPPP_LIB
# SCL_CORE_LIB
# SCL_UTILS_LIB
# SCL_DAI_LIB
# SCL_EDITOR_LIB

FIND_PROGRAM(SCL_EXPRESS_EXECUTABLE express  DOC "path to the SCL express executable")
FIND_PROGRAM(SCL_SYMLINK_EXECUTABLE symlink DOC "path to the SCL symlink executable")
FIND_PROGRAM(SCL_EXPPP_EXECUTABLE exppp DOC "path to the SCL exppp executable")
FIND_PROGRAM(SCL_FEDEX_OS_EXECUTABLE fedex_os DOC "path to the SCL fedex_os executable")
FIND_PROGRAM(SCL_FEDEX_IDL_EXECUTABLE fedex_idl DOC "path to the SCL fedex_idl executable")
FIND_PROGRAM(SCL_FEDEX_PLUS_EXECUTABLE fedex_plus  DOC "path to the SCL fedex_plus executable")

FIND_LIBRARY(SCL_EXPPP_LIB NAMES exppp)
FIND_LIBRARY(SCL_CORE_LIB NAMES stepcore)
FIND_LIBRARY(SCL_DAI_LIB NAMES stepdai)
FIND_LIBRARY(SCL_UTILS_LIB NAMES steputils)
FIND_LIBRARY(SCL_EDITOR_LIB NAMES stepeditor)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SCL DEFAULT_MSG SCL_CORE_LIB SCL_DAI_LIB SCL_UTILS_LIB SCL_FEDEX_PLUS_EXECUTABLE SCL_EXPRESS_EXECUTABLE)

MARK_AS_ADVANCED(SCL_CORE_LIB SCL_DAI_LIB SCL_EDITOR_LIB SCL_EXPPP_LIB SCL_UTILS_LIB)
MARK_AS_ADVANCED(SCL_EXPPP_EXECUTABLE SCL_EXPRESS_EXECUTABLE SCL_FEDEX_IDL_EXECUTABLE SCL_FEDEX_OS_EXECUTABLE SCL_FEDEX_PLUS_EXECUTABLE SCL_SYMLINK_EXECUTABLE)
