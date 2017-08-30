if [ -z "$CPSWVERS" ] ; then
	CPSWVERS=R3.5.branch
fi
if [ -z $BOOSTVERS ] ; then
	BOOSTVERS=1.63.0
fi
if [ -z $YAMLCPPVERS ] ; then
	YAMLCPPVERS=0.5.3
fi
if [ -z $PYVERS ] ; then
	PYVERS=2.7.9
fi
if [ -z "$ARCH" ] ; then
  MACH=`uname -m`
  VERS=`uname -v`
  REL=`uname -r`
  if echo $VERS | grep -q PREEMPT ; then
    if  echo $REL | grep -q '4[.]8[.]11' ; then
      ARCH=buildroot-2016.11.1-
    elif echo $REL | grep -q '3[.]18[.]11' ; then
      ARCH=buildroot-2015.02-
    else
      echo "Unable to determine buildroot version"
    fi
  elif echo $REL | grep -q el6 ; then
    ARCH=rhel6-
  else
    echo "Unable to determine OS type"
  fi
  ARCH=$ARCH$MACH
fi
if [ -z $ARCH ] ; then
  echo "ARCH not set; not modifying environment"
else
  echo "Setting environment for $ARCH"
  export LD_LIBRARY_PATH=/afs/slac/g/lcls/package/boost/$BOOSTVERS/$ARCH/lib:/afs/slac/g/lcls/package/yaml-cpp/yaml-cpp-$YAMLCPPVERS/$ARCH/lib:/afs/slac/g/lcls/package/cpsw/framework/$CPSWVERS/$ARCH/lib:/afs/slac/g/lcls/package/python/python$PYVERS/$ARCH/lib
  export PYTHONPATH=/afs/slac/g/lcls/package/cpsw/framework/$CPSWVERS/$ARCH/bin
  export PATH=/afs/slac/g/lcls/package/cpsw/framework/$CPSWVERS/$ARCH/bin:$PATH
  if [ -d /afs/slac/g/lcls/package/python/python$PYVERS/$ARCH/bin ] ; then
    PATH=/afs/slac/g/lcls/package/python/python$PYVERS/$ARCH/bin:$PATH
  fi
fi
