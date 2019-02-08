#!/usr/bin/python3
import pycpsw
import yaml_cpp
import threading

class FixupRoot(pycpsw.YamlFixup):
  def __init__(self):
    pycpsw.YamlFixup.__init__(self)

  def __call__(self, root, top):
    for nod in root["children"]:
      if nod[0].Scalar() == "mmio":
        nod[0].set( "MMIO" )
        break

class AsyncCB(pycpsw.AsyncIO):
  def __init__(self):
    pycpsw.AsyncIO.__init__(self)
    self._sem = threading.Semaphore(value=0)

  def callback(self, *args):
    self._res = args[0]
    if len(args) > 1:
      self._sta = args[1]
    else:
      self._sta = None
    self._sem.release()

  def getVal(self, sv):
    sv.getValAsync( self )
    self._sem.acquire()
    return self._res

r=pycpsw.Path.loadYamlFile("O.linux-x86_64/cpsw_netio_tst_7.yaml", rootName="root", yamlIncDirName="O.linux-x86_64", yamlFixup=FixupRoot() )

try:
  tst = r.findByName("MMIO/srvm/arr-32-0-0-leNOTFOUND")
  raise RuntimeError("Test Failed")
except pycpsw.NotFoundError:
  pass

tst = r.findByName("MMIO/srvm/arr-32-0-0-le")

if tst.empty() or tst.size() != 3:
  raise RuntimeError("Test Failed")

sv=pycpsw.ScalVal_RO.create(tst)
y = sv.getVal()

tst1 = tst.clone()

if tst1.toString() != tst1.toString():
  raise RuntimeError("Test Failed")

if tst1.up().getName() != tst.tail().getName():
  raise RuntimeError("Test Failed")

if tst1.toString() != "/MMIO/srvm":
  raise RuntimeError("Test Failed")

tst1.up()
tst1.up()
if not tst1.empty():
  raise RuntimeError("Test Failed")

tst1.findByName("MMIO/srvm")

empty = tst.clone()
empty.clear()

if not empty.empty():
  raise RuntimeError("Test Failed")

# Following must not crash
empty.up()
empty.parent()
empty.tail()
empty.clear()
empty.size()
empty.origin()
empty.getNelms()
empty.concat(empty)
empty.intersect(empty)
empty.isIntersecting(empty)
empty.explore(pycpsw.PathVisitor())
try:
  empty.getTailFrom()
  raise RuntimeError("Test Failed")
except pycpsw.InvalidPathError:
  pass
try:
  empty.getTailTo()
  raise RuntimeError("Test Failed")
except pycpsw.InvalidPathError:
  pass
try:
  empty.findByName("Foo")
except pycpsw.NotFoundError:
  pass

tst.origin().findByName("MMIO")
if tst.parent().getName() != "srvm":
  raise RuntimeError("Test Failed")

if tst.getNelms() != 2048:
  raise RuntimeError("Test Failed")

if empty.getNelms() != 1:
  raise RuntimeError("Test Failed")

tst1 = tst.clone()
tst1.up()
tst2 = tst.parent().findByName( tst.tail().getName() )
if not tst1.verifyAtTail( tst2 ) or tst.verifyAtTail( tst2 ):
  raise RuntimeError("Test Failed")

try:
  tst1.append( tst )
  raise RuntimeError("Test Failed")
except pycpsw.InvalidPathError:
  pass

if tst1.concat(tst2).toString() != tst.toString():
  raise RuntimeError("Test Failed")

tst1.append(tst2)

if tst1.toString() != tst.toString():
  raise RuntimeError("Test Failed")

tst2 = tst.parent().findByName( tst.tail().getName() + "[10-20]" )
tst1.up()
tst1.append( tst2 )

if not tst.isIntersecting(tst1) or not tst1.isIntersecting(tst):
  raise RuntimeError("Test Failed")

if tst.isIntersecting(tst2) or tst2.isIntersecting(tst):
  raise RuntimeError("Test Failed")

tst2=tst.intersect(tst1)

if tst2.toString() != "/MMIO/srvm/arr-32-0-0-le[10-20]":
  raise RuntimeError("Test Failed")

if tst2.getTailFrom() != 10 or tst2.getTailTo() != 20:
  raise RuntimeError("Test Failed")

class Explorer(pycpsw.PathVisitor):

  def __init__(self, comp, skip = None):
    pycpsw.PathVisitor.__init__(self)
    self.reset(comp, skip)

  def reset(self, comp, skip = None):
    self.comp  = comp
    self.skip  = skip
    self.gotit = False

  def found(self):
    return self.gotit

  def visitPre(self, here):
    #print(here.toString())
    return here.toString() != self.skip

  def visitPost(self, here):
    if here.toString() == self.comp:
      self.gotit = True

evis = Explorer( tst.toString() )

if evis.found():
  raise RuntimeError("Test Failed")

r.findByName("MMIO").explore( evis )

if not evis.found():
  raise RuntimeError("Test Failed")

evis.reset( tst.toString(), "/MMIO/srvm" )

if evis.found():
  raise RuntimeError("Test Failed")

tst.loadConfigFromYamlString("5555")
tst2.loadConfigFromYamlString("1111")

sv=pycpsw.ScalVal_RO.create(tst)

acb = AsyncCB()
sv.getValAsync(acb)

acb = AsyncCB()

for y in [sv.getVal(), acb.getVal( sv )]:
  i = 0
  for x in y:
    if i < 10 or i > 20:
      if x != 5555:
        raise RuntimeError("Test Failed")
    else:
      if x != 1111:
        raise RuntimeError("Test Failed")
    i = i + 1
  if i != 2048:
    raise RuntimeError("Test Failed")

print("TEST PASSED")
