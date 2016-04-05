# Process this file with autoconf to produce a configure script.
AC_INIT(RcppBoostCompute, 0.1, msuchard@ucla.edu)
AC_CONFIG_MACRO_DIR([m4])

# find R home and set CC/CFLAGS
: ${R_HOME=`R RHOME`}
if test -z "${R_HOME}"; then
  echo "could not determine R_HOME"
  exit 1
fi

# platform check
RHOST=`"${R_HOME}/bin/Rscript" -e 'cat(Sys.info()[["sysname"]])'`

# Check for which host we are on and setup a few things
# specifically based on the host
HAVE_OPENCL=""
case $RHOST in
  Darwin* )
        # Do something specific for mac
        OPENCL_LIB="-framework OpenCL";
        HAVE_OPENCL="-DHAVE_OPENCL"
        ;;
  Linux*)
        # Do something specific for linux
        OPENCL_LIB="-lOpenCL";
        AC_CHECK_LIB(OpenCL, clFinish, 
        	[HAVE_OPENCL="-DHAVE_OPENCL";
        	 OPENCL_LIB="-lOpenCL"], 
        	[HAVE_OPENCL="";
        	 OPENCL_LIB=""])
        ;;
   *BSD*)
        # Do something specific for BSD
        AC_MSG_ERROR([BSD is not currently supported])
        ;;
    *)
        #Default Case
        AC_MSG_ERROR([Your platform is not currently supported])
        ;;
esac

AC_SUBST(OPENCL_LIB)
AC_SUBST(HAVE_OPENCL)

AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT
