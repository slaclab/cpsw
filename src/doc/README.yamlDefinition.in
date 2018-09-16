# Describing CPSW Hierarchies with YAML

## Copyright Notice
This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
file found in the top-level directory of this distribution and
[at](https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html).

No part of CPSW, including this file, may be copied, modified, propagated, or
distributed except according to the terms contained in the LICENSE.txt file.

## Introduction

This document describes how a CPSW hierarchy can be defined by writing
a YAML file. Such a YAML file provides a convenient interface to the
CPSW 'builder' API as it does not require writing any C++ code.

The document does not discuss YAML itself. Consult the YAML
[specification](http://www.yaml.org/spec/1.2/spec.html)
for details about the syntax and other features.

## 1. YAML Extensions

CPSW adds a few extensions to YAML:

### 1.1 YAML Include Files and Preprocessor

YAML provides no way to include one YAML file from another one. The YAML
specification (and yaml-cpp implementation) makes it impossible to define
file inclusion within YAML itself.

For this reason and because re-using snippets of YAML was deemed very
desirable a simple *preprocessor* was implemented in CPSW.

The preprocessor recursively parses commented headers in YAML files for
'include' and 'once' directives and assembles a single stream of YAML
which is then forwarded to the YAML parser library (yaml-cpp).

#### 1.1.1 Preprocessor Header Format

The preprocessor scans an initial block of header lines:

        header_block =  { header_line } ;

        header_line  =  '#' , ( include_directive | once_directive | comment ) , ?EOL? ;

i.e., it scans until a line does not start with `#`.

#### 1.1.3 #include Directive

The 'include' directive (a line starting with '`#include `' [notice the blank]):

        whitespace        = ' ' | ?TAB? | ?CR? ;
        nonwhite_char     = ?ANYCHAR? - whitespace ;
        nonbra_char       = nonwhite_char - '<' ;
        nonket_char       = nonwhite_char - '>' ;

        include_directive = 'include ', { whitespace }, include_filename, { whitespace } ;
 
        include_filename  = bracketed_name | nonbracketed_name ;

        nonbracketed_name = nonbra_char, { nonwhite_char } ;
        bracketed_name    = '<', nonket_char, { nonket_char }, '>' ;


directs the preprocessor to (recursively) continue with processing the file
`include_filename` before resuming processing the current file.

`include_filename` currently cannot not contain any whitespace characters
(escaping not implemented at this time) but it may contain path components.

All YAML files are assumed to reside in a single directory which defaults
to the current-working directory but the preprocessor can be pointed to
an alternate location.

#### 1.1.4 #once Directive

If the preprocessor encounters a 'once' directive (a line starting with '`#once `'
[notice the blank])

        once_directive := 'once ' , { whitespace } , once_tag ;
        once_tag       := nonwhite_char, { nonwhite_char } ;

then it reads the `once_tag` (any non-empty string following '`#once `') and
checks if it has encountered the same `once_tag` already earlier. If this is
the case then the preprocessor skips to the end of the file. Otherwise it
records the tag and continues processing.

Thus, the `#once` directive serves as a simple multiple inclusion guard. However,
unlike the familiar `#ifdef`, `#once` always skips to the end of the file.

E.g., if `me.yaml` contains:

        # This file includes itself but does not result in infinite recursion
        #
        # Note that here cannot be whitespace in '#include' nor '#once'
        #
        #once protect_me
        #
        #include me.yaml
  
        a_yaml_node: something

then the preprocessor will emit `a_yaml_node: something` just once.
  

### 1.2 YAML Merge Key --- `YAML_KEY_MERGE`

#### 1.2.1 Purpose of the Merge Key

With the standard YAML 'alias' feature and the preprocessor's '#include'
directive there are powerful ways to re-use pieces of YAML at different
locations.

Often, however, it is desirable to reuse *most* parts of an existing piece
while being able to *override* select parts.

This cannot be achieved with standard YAML. As a result, the 'Merge Key'
for YAML 'maps' has been [proposed:](http://yaml.org/type/merge.html).

#### 1.2.2 The Semantics of the Merge Key 

Assume we have a definition

        default: &default_tag
          setting_1: value_1
          setting_2: value_2
        
which we want to use in our map 'mymap':

        mymap: *default_tag
        
However, what if we want to override (just) `setting_2` in `mymap` with let's
say `my_value_2`.

In standard YAML that would not be possible because a key must be present
at most *once* in a map. Thus we might try:

        mymap: *default_tag
          setting_2: my_value_2
        
but this would not be valid YAML code. (mymap already has the 'default' node
as its value and there is no way to add additional entries to that node. And
even if there were, it would be illegal to use the `setting_2` key again.)

This is where the merge key comes in:

   mymap:
         <<       : *default_tag
         setting_2: my_value_2

Thus 'mymap has now two keys:

 * `<<` with the default node (itself a map) as its value
 * `setting_2` with `my_value_2` as its value

The idea of the merge key defines semantics for key lookup in a map:

        lookup_key:

          if ( map["key"] )
            return map["key"]
          else if ( map["<<"] )
            return map["<<"]["key"]

I.e.: look for the key in the map. If it is not found then check if there
is a merge key and if there is then look for the original key in the
merged node (value of the merge key).

NOTE:
 - the merge key ONLY works for maps, not sequences. I.e., a map must
   hold the merge key and another map must be attached to the merge key.

 - the merge key is not 'natively' supported by YAML. For it to work the
   user-code (CPSW in this case) must obey/implement its semantics during
   lookup operations.

#### 1.2.3 Complex Merging

The idea of merging looks simple enough. However, as described above
it works only across a single level. Imagine our default definition 
looks like this:
          
        default: &default_tag
          nested_map:
            setting_1: value_1
            setting_2: value_2

I.e., the default map contains nested maps and we want to override
a setting located at a deeper level.

        mymap:
          <<       : *default_tag
          setting_2: my_value_2

no longer works, since `setting_2` is not at the same hierarchy level
as `setting_2` inside `nested_map`.

What we mean is
            
        mymap:
          <<       : *default_tag
          nested_map:
            setting_2: my_value_2

but in this case the simple lookup algorithm presented above no longer
works (`setting_2` is not found in `mymap["nested_map"]` but there is also
no merge key in `mymap["nested_map"]`).

As a result, the merging algorithm implemented by CPSW is much more
sophisticated and actually supports the notation just presented in the
example.

If a key is not found then CPSW uses a backtracking algorithm to
climb out of the map searching for merged nodes at each 'ancestor'
level and then descending into merged nodes recursively looking
for the desired key.

Note that this complex mergin considerably increases the complexity
for lookup if a key is not found/present at all.

## 2. CPSW Building Blocks

This section describes the building blocks made available by
the CPSW framework which can be used to assemble hierarchies 
in YAML.

CPSW is extensible and user-defined classes which derive from
the core classes may add additional YAML properties.

### 2.1 Basic Node

Every CPSW node described in YAML must be an entry in a map
and is itself a YAML map. The key of the CPSW Node will become
its name in the hierarchy:

        myname:
          property: value
        
will define a CPSW node called 'myname' and it's map contains
a key 'property'. The exact properties which are supported
and/or expected by a particular node depend in its underlying
c++ class. All core classes shall be described in the following
sections.

#### 2.1.1 `YAML_KEY_class` Property

Every node *must* define a map entry with the key `YAML_KEY_class`.
Its value must correspond to one of the classes registered with
CPSW. CPSW will then dispatch interpreting the YAML node data
to the factory method of this class which takes care of constructing
the underlying CPSW object.

Thus, the basic node is defined by

        myname:
          YAML_KEY_class: <className>

The value of this key may also be a sequence of class names.
In this case, CPSW tries to locate a class in the specified
order. E.g., 

        myname:
          YAML_KEY_class:
            - myClass
            - MMIODev

CPSW would then try to locate 'myClass' first and if that fails
'MMIODev' would be used as a fallback.

The typical use case is a device for which a driver is initially
not available. At least its registers could be made available by
MMIODev.

##### 2.1.1.1 Dynamic Class Loading

CPSW implements dynamic loading of classes. If it doesn't find
a class in its registry it tries to load "<className>.so".

A properly written class contains initialization code which would
register the class with CPSW so that the ensuing second attempt
to find the class will succeed.

#### 2.1.2 `YAML_KEY_instantiate` Property

Every node also supports an optional map entry with the key
`YAML_KEY_instantiate` which has a boolean value (defaults
to 'true'). When set to 'false' then CPSW will skip instantiation
of this node and any potential children.

This is useful (especially in combination with the merge key)
to disable select nodes.

### 2.2 'Field' Class

The lowest-level class which can actually be instantiated
is a 'Field'. 'Field' implements the 'Stream' user API.

It supports the following properties (in addition to 'Basic Node'):

        myField:
            # class of this node
          YAML_KEY_class:       Field      # *mandatory*

            # brief description of the purpose and semantics of this Field
          YAML_KEY_description: <string>   # optional

            # suggested polling interval (not used by CPSW)
          YAML_KEY_pollSecs:    <double>   # optional

            # size (in bytes) of this field. Note that some subclasses
            # don't accept a size of zero (default).
          YAML_KEY_size:        <int>      # optional

            # hint to software whether this Field can be cached.
            # Legal values are:
            #   NOT_CACHEABLE
            #   WT_CACHEABLE  (write-through cacheable)
            #   WB_CACHEABLE  (write-back    cacheable)
            # If a Field is WB_CACHEABLE then the software may
            # e.g., combine transactions scheduled for this field
            # with others and/or reorder them.
          YAML_KEY_cacheable:   <Cacheable> # optional

            # definition of this property allows you to establish an
            # order in which Fields are dumped/saved to a configuration
            # file (ignored if a configuration 'template' is used).
            # Consult 'README.configData' for more information.
          YAML_KEY_configPrio:  <int>       # optional

The default value for `YAML_KEY_cacheable` is <unknown> for
leaves and `WT_CACHEABLE` for containers. During the build 
process <unknown> settings are resolved by using the parent
container's value. This makes it easier to manage a default
(via the container) and override select Field(s) only.

### 2.3 'Dev' Class

The Dev is the base class for all containers. It is derived from 
Field and inherits all of its properties (but the default value
for `YAML_KEY_cacheable` is `WB_CACHEABLE`).

The Dev adds support for a map of children. The `YAML_KEY_class`
entry must be 'Dev' or a subclass thereof.

        myDev:
          YAML_KEY_class:       Dev     # *mandatory*
          YAML_KEY_children:            # optional
            <child_entry>
              YAML_KEY_at:              # *mandatory*
                <child_address_entry>   # *mandatory*
            <child_entry>
              YAML_KEY_at:              # *mandatory*
                <child_address_entry>   # *mandatory*
            ...

After the `YAML_KEY_children` key all children are listed
(as maps themselves).

Note that the Dev class also requires for each child
a map with *addressing information* to be attached to
the child. The properties in the `<child_address_entry>`
are interpreted and used by the *Dev* class and not
by the child itself. However, the information is specific
to each child and is needed for communication between
the Dev and its children.

E.g., in the case of a conventional memory-mapped device
the addressing information contains an 'offset'. In
other cases it may be a 'i2c' address or a device filename
etc.

Each Subclass of Dev supports a distinct subclass or set
of subclasses of 'Address' which are usually implemented
together with the container class.

#### 2.3.1 The 'Address' Class

'Address' is the base class for addressing information
which defines the connection of a child to its container.

The basic Address supports the following YAML properties
(which must be defined in a map under the `YAML_KEY_at` key
inside the child definition):

          # How many replicas of the child are to be
          # attached. This allows for the definition
          # of arrays.
          # The default value is '1'.
          # Note that not all subclasses of Address
          # may support values other than 1.
        YAML_KEY_nelms:    <int>        # optional

          # Associate a byte order with this address.
          # The default is 'unknown' in which case the
          # build process will propagate the container's
          # byte order here.
          #
          # Legal values are
          #  "BE"   (big-endian)
          #  "LE"   (little-endian)
          #  "UNKNOWN"
        YAML_KEY_byteOrder: <ByteOrder> # optional

The byte order may not make sense for all subclasses
of 'Address' and is best left to be resolved by the
container. It may occasionally required to be overridden,
e.g., if select registers in a memory-mapped device
appear swapped.

Here is an example for a little-endian memory map
which has an 'abnormal' register presented in big-endian:

        register:  &aregister
          class:     IntField
          size:             4
          sizeBits:        32

        myDevice:
          class:      MMIODev
          byteOrder:  LE
          size:       0x10
          children:

            <<: *normal_children # assume to be defined elsewhere

            abnormal_register:
              <<: *aregister
              at:
                offset:    12
                byteOrder: BE

Regarding the `YAML_KEY_nelms` key it is important to realize
that the definition of arrays of elements does *not* rest in
the definition of the element itself but with the 'address'
where the element is attached. (Similar to the 'C' language:
an array is usually an 'array of element-types' and not an
'array-of-element type'.

### 2.4 The 'SequenceCommand' Class

The SequenceCommand class is derived from Field and is a
'leaf' element.

It implements the user-API 'Command' interface and can
execute sequences of
  - delays
  - writing values to 'ScalVal' interfaces

In addition to Field's properties SequenceCommand supports
`YAML_KEY_sequence`. It's value must be a YAML sequence of
maps:

        mySequence:
          YAML_KEY_class:  SequenceCommand      # *mandatory*
            YAML_KEY_sequence:
              - YAML_KEY_entry: <string>
                YAML_KEY_value: <number>
              - YAML_KEY_entry: <string>
                YAML_KEY_value: <number>
              ...

The `YAML_KEY_entry` value strings identify a cpsw Path (starting
at the sequence node's parent) to which the numbers are 
written in the sequence defined by the list.

E.g., 
             - YAML_KEY_entry: sibling
               YAML_KEY_value: 1234
             - YAML_KEY_entry: usleep
               YAML_KEY_value: 10000
             - YAML_KEY_entry: ../aunt/cousin
               YAML_KEY_value: 5678

would write 1234 to 'sibling' which resides in the same
container as the SequenceCommand itself and 5678 to
'cousin' which resides in another container 'aunt'
which is a sibling of the SequenceCommand's parent.

The 'usleep' Path is treated special and will cause
the sequence to delay for the specified number of
microseconds. Note that this shadows any sibling
named 'usleep'.

### 2.5 The 'IntField' Class

IntField is derived from Field and is a 'leaf' class. It implements
the `ScalVal`, `ScalVal_RO`, `DoubleVal` and `DoubleVal_RO` user-APIs.

IntField supports signed or unsigned integer numerical values of
arbitrary length in bits (up to 64). IntField also supports arbitrary
alignment within a byte, i.e., the bits that make up an IntField
need not be byte-aligned (but the must be contiguous).

In addition, IntField supports single- or double-precision IEEE-754
floating-point numbers. Such numbers must have a length of exactly
32 or 64 bits and marked with the `YAML_KEY_encoding` property
set to `IEEE_754`. Bit-shift and endianness-conversion etc. are
handled the in the same way as for integers ('enum' mappings as
described below are not supported for floating-point entities).

Note that if an entity has IEEE_754 encoding then it may only
be accessed via the `DoubleVal`/`DoubleVal_RO` interfaces.

Thus, IntFields are ideal to represent registers or bits thereof.

Optionally, a list of 'enum' mappings can be associated with an IntField
which allows mapping of numerical values to strings and back
(e.g., a bit can be mapped to 'True'/'False').

IntField also takes care of byte-swapping numbers to host byte order
and it can optionally word-swap (suitably aligned) sub-words to
handle more obscure cases.

Depending on an IntField's signed-ness numbers are sign-extended to
the size requested by the user.

IntField can be configured to a 'read-only' mode (statically defined by
YAML - cannot be switched later) in which only `ScalVal_RO` and `DoubleVal_RO`
are supported and values cannot be changed.

An optional 'encoding' attribute can be defined for IntField.

This e.g., allows the user-application to properly decode numbers into
strings if the underlying entity actually represents a character.
The 'encoding' attribute is also used for registers containing
floating-point numbers.

        myIntField:
          YAML_KEY_class: IntField           # *mandatory*

            # Whether to interpret the underlying bits
            # as a signed or unsigned number. Signed
            # numbers are sign-extended to the size
            # requested by the user. Defaults to 'false'.
          YAML_KEY_isSigned: <bool>          # optional

            # Size of the IntField in bits. The Field's
            # corresponding size in bytes is automatically
            # computed (YAML setting ignored).
            # Defaults to 32.
            # I.e.,: size = (sizeBits + lsBit + 7) / 8
          YAML_KEY_sizeBits: <int>           # optional

            # Alignment of the least-significant bit
            # within a byte. Legal values are 0..7.
            #
            # A number written is left-shifted by
            # this amount before being (merged and)
            # written to hardware. It is right-shifted
            # after being extracted from hardware and
            # before being handed to the user.
            # Defaults to 0.
          YAML_KEY_lsBit   : <int>           # optional

            # Access-mode of this IntField
            #  - "RW" (read-write; default)
            #  - "RO" (read-only)
            #  - "WO" (write-only -- *currently not supported*)
          YAML_KEY_mode    : <string>        # optional

            # Swap words of specified length (in bytes;
            # the IntField's bit-size *must* be a multiple
            # of the word-length!)
            # 
            # Defaults to 0.
          YAML_KEY_wordSwap: <int>           # optional

            # If this IntField represents a character or a
			# floating-point number etc., then the encoding
            # used by this IntField can be specified.
            # An automatically generated GUI, for example,
            # could use the encoding to detect strings and
            # display them correctly.
            #
            # Legal values include "NONE", "ASCII", "UTF_8",
            # "ISO_8859_1" among others (consult the API header
            # for a complete list).
			# Floating-point numbers use e.g., "IEEE_754".
            #
            # Defaults to NONE.
          YAML_KEY_encoding: <int>           # optional

            # The base in which values are saved to a configuration
            # file. Legal values are 10 (decimal) or 16 (hex). 
            # The only effect of this parameter is increasing
            # readability of saved configuration files.
            #
            # Defaults to 16.
          YAML_KEY_configBase: <int>         # optional

The `YAML_KEY_wordSwap` parameter can e.g., be used to handle
the case of a 64-bit word in 'mixed format':

Assume a 32-bit system with little-endian byte-addressing
where the most significant word is stored at a lower address,
followed by the least significant word:

   address
             0  high_word
             4  low_word

If the words are stored in little-endian format then the
quad '0x0807060504030201' would *normally* be stored (by
increasing address):

          0x01, 0x02, 0x03, 0x04, 0x04, 0x06, 0x07, 0x08

If, however, (as it sometimes happens in 'real-world' devices)
the above 'mixed format' is used then the data would be
stored:

          0x04, 0x05, 0x06, 0x07, 0x01, 0x02, 0x03, 0x04

In this case an IntField with `YAML_KEY_wordSwap` set
to 4 would read the correct value.

### 2.6 The 'ConstIntField' Class

ConstIntField is derived from IntField and is another 'leaf' class.
It implements `ScalVal_RO` and `DoubleVal_RO` user-APIs.

The purpose of ConstIntField is providing run-time access to
`constant` values which are defined in YAML. ConstIntField
does never access real hardware.

ConstIntField can supply string-, double- or integer values.
It inherits all properties from IntField but overrides some
of them (`YAML_KEY_sizeBits`, `YAML_KEY_lsBit`, `YAML_KEY_byteOrder`, `YAML_KEY_wordSwap`, `YAML_KEY_mode`).

The type of the value retrieved from YAML is defined by the
`YAML_KEY_encoding` property.

If `YAML_KEY_encoding` is set to `ASCII` then the value is interpreted
as a string; otherwise, if `YAML_KEY_encoding` equals `IEEE_754` then
it is interpreted as a `double` else as a 64-bit integer
(honoring `YAML_KEY_isSigned`).

A 'Enum' menu with a single entry which maps a string to
the numerical value is always attached to ConstIntField 
(except if IEEE_754 encoding is used) but it is not
necessary to define such a menu in YAML.

E.g., in the case of

        aString:
          YAML_KEY_class:    ConstIntField     # *mandatory*

            # inherited from IntField
          YAML_KEY_encoding: ASCII
            # The constant value
          YAML_KEY_value:    "Hello"           # *mandatory*

        aDouble:
          YAML_KEY_class:    ConstIntField     # *mandatory*
            # set to 'IEEE_754' if the value is to be interpreted
            # as a double.
          YAML_KEY_encoding: IEEE_754
          YAML_KEY_value:    3.141             # *mandatory*

`aString`'s menu will map the value 0 to the string "Hello"
and `aDouble`'s menu will map the value 3 to the string "3.141".

### 2.7 The 'MMIODev' Class

MMIODev is a subclass of Dev and implements a memory-mapped
container.

The only parameter added to Dev is `YAML_KEY_byteOrder` which
is just for convenience since it allows the user to define
a byte-order for the entire container which sets the default
for all of its children (each child's Address may override
this setting individually for the specific child).

        myMMIODev:
          YAML_KEY_class: MMIODev               # *mandatory*
 
            # NOTE: An MMIODev must specify a non-zero size
          YAML_KEY_size:  <int>

            # Set the default ByteOrder for the container.
            # As explained under 'Address' - if the ByteOrder
            # is unknown (default) then this MMIODev inherits
            # from the Address where it is attached.
            #
            # Note: At *some* level the byte-order which
            #       will be used by Fields for which this
            #       is relevant *must* be defined. Either
            #       by the Field itself or any of its parents
            #       in the hierarchy.
            # Legal values are
            #
            #  "BE"   (big-endian)
            #  "LE"   (little-endian)
            #  "UNKNOWN"
 
          YAML_KEY_byteOrder: <ByteOrder>

#### 2.7.1 MMIOAddress

When a Field is attached to an MMIODev then there are two parameters
in addition to `YAML_KEY_nelms` and `YAML_KEY_byteOrder` (which are
inherited from Address):

          # The byte offset from the MMIODev's base address
          # where the child resides (if the child spans more than one
          # byte then this means the offset of the byte with the
          # lowest byte-address ).
          # 
          # Default: <none>
        YAML_KEY_offset: <int>                   # *see note*

          # The 'stride' specifies by how many bytes the elements
          # of an array are separated/spaced.
          # The default (0) assumes 'dense' packing i.e., the stride
          # is automatically set to the size of an array element.
        YAML_KEY_stride: <int>                   # *optional*

Note that when the offset is omitted then an ordinary 'Address'
is used which essentially bypasses the MMIODev and forwards
all read/write operations to the parent.

#### 2.7.2 MMIODev and IntField Example

Assume a (memory-mapped) device presents an ASCII string
of 40 characters where each character is aligned on a
32-bit boundary. The string starts at offset 0x10.

        myMMIODev:
          YAML_KEY_class    :  MMIODev
          YAML_KEY_byteOrder:  LE
          YAML_KEY_size     :  0x1000
          YAML_KEY_children :
            myString:
              YAML_KEY_class:    IntField
              YAML_KEY_sizeBits: 8
              YAML_KEY_encoding: ASCII
              YAML_KEY_at:
                YAML_KEY_offset: 0x10
                YAML_KEY_stride:    4
                YAML_KEY_nelms:    40
           
### 2.8 The 'NetIODev' Class

NetIODev is -- for most applications -- the 'root' device which
handles all communication with a remote endpoint.

The main property supported by NetIODev is the IP address
of the peer:

        myNetIODev:
          YAML_KEY_class : NetIODev

            # CPSW does attempt to resolve DNS names.
            # IP addresses may be provided in 'numbers-
            # and-dot' notation or as names.

            # Note: omitting the peer's IP lets
            # CPSW pick 'INADDR_ANY' which on some systems
            # causes communication with the local host
            # (which may be useful for testing only).
          YAML_KEY_ipAddr:     <string>     # *optional*

            # Connect to 'ipAddr' via an 'rssi bridge'
            # proxy. This property defines the address
            # of the proxy (you must make sure such a
            # bridge is actually running!)
          YAML_KEY_rssiBridge: <string>

            # Connect to a TCP destination (either
            # a 'native' TCP destination or an 'rssi
            # bridge') via a SOCKS proxy (the most
            # common case being an SSH connection
            # with SOCKS support - see open-ssh's
            # '-D' option for details).
          YAML_KEY_socksProxy: <string>

Thus, for each peer with a different IP address a separate NetIODev 
instance is required. These could be attached to a dummy root Dev:

        myRoot:
          YAML_KEY_class:      Dev
          YAML_KEY_children:
            peer_1:
              YAML_KEY_class:  NetIODev
              YAML_KEY_ipAddr: 10.0.0.1
              YAML_KEY_at: # empty 'at' -- no addressing info needed
            peer_2:
              YAML_KEY_class:  NetIODev
              YAML_KEY_ipAddr: 10.0.0.2
              YAML_KEY_at: # empty 'at' -- no addressing info needed

In addition to the peer's address the following settings help
with tunneling/proxying connections over TCP and even SSH:

        myRoot:
          YAML_KEY_class:       NetIODev
          YAML_KEY_rssiBridge: <rssi_bridge_ip_or_name>
          YAML_KEY_socksProxy: <socks_proxy_ip_or_name>[:<socks_proxy_port>]

If the `YAML_KEY_rssiBridge` property is set then it points to a computer
with direct connectivity to the target. On that computer a properly configured
`rssi_bridge` must be running. Such a bridge serves as a proxy for RSSI/UDP
communication:

        CPSW <-- TCP connection --> rssi_bridge <-- RSSI/UDP connection --> target

If `rssiBridge` is defined in YAML then all `UDP` modules are automatically
converted into `TCP`.

In addition to `YAML_KEY_rssiBridge` the `YAML_KEY_socksProxy` property
may be used for tunneling TCP connection(s) over SSH. Typically, an
SSH connection is started with the `-D` option ("dynamic port forwarding")
letting the SOCKS protocol (which is supported by CPSW) taking care of
the rest. Alternatively to `YAML_KEY_socksProxy`, the environment-variable
`SOCKS_PROXY` may also be used for defining a SOCKS proxy.

It is often convenient to start an `rssi_bridge` via ssh:

        ssh -D1080 -tt user@host <path>/rssi_bridge -a <target_ip> -p 8198 -u 8197 <other ports>

In most cases `YAML_KEY_socksProxy` is simply set to `localhost`.

Note that the SOCKS proxy is ignored if no TCP is defined in the YAML
(explicitly or implicitly due to `YAML_KEY_rssiBridge`).


#### 2.8.1 NetIODev Address Objects

The true power of NetIODev lies in the Address objects it implements.
These are the communication endpoints where children are attached.

Each endpoint instantiates a protocol stack or a part thereof
which is configured with a variety of parameters that shall be listed below.

Note that the arrangement of the stack ('stacking order') is configured
automatically and not subject to user choices.

The various protocol modules are presented in the order in which they
are usually stacked:

        SRP_VirtualChannel -- (De)/Multiplexer of SRP transactions based on a tag
        SRP                -- 'Slac Register Protocol'. Encodes I/O Operations
        TDEST              -- (De)/Multiplexer based on TDEST tag
        Fragmentation      -- AKA 'Packetizer'; assembles/disassembles
                              larger frames from/into MTU-sized ones.
        RSSI               -- Reliability and flow-control
        UDP                -- UDP communication layer communicating with a single
                              port of the peer.

All modules above UDP are optional but some depend on each other:
The `TDEST` (De)Muxer requires the Packetizer to be present and `SRP_VC`
requires SRP.

Of course, the setup of these layers must match the peer's configuration
exactly.

Each protocol module is assigned its own map entry in the `YAML_KEY_at`
map of the attachee. Since these entries are YAML maps the order of
the protocol modules in YAML is arbitrary (the decision was made
deliberately to use maps so that merge keys work across all levels).

        YAML_KEY_UDP:

            # The UDP port of the peer
            # NOTE: a port number of 0 *disables* UDP
            #
            # Default: 8192
          YAML_KEY_port:           <int>

            # Depth of the queue up to the next module
            # zero (default) picks a suitable value.
          YAML_KEY_outQueueDepth:  <int>

            # Number of RX threads to spawn for handling
            # zero (default) picks a suitable value.
          YAML_KEY_numRxThreads:   <int>
            #
            # Peers which do not implement ARP rely
            # on being contacted at regular intervals
            # so that they can record our UDP/IP/MAC
            # address.
            # A special thread periodically sends a short
            # message to the peer for this purpose.
            # This parameter defines the frequency of
            # this polling operation.
            #
            # A value of 0 disables polling altogether.
            # A value less than zero lets CPSW pick
            # a suitable default.
            #
            # Default: -1
          YAML_KEY_pollSecs:       <int>

            # Priority of the UDP RX threads. A number
            # bigger than zero must be a valid pthread
            # priority and tries to engage a real-time
            # scheduler. If this fails, CPSW falls back
            # to the default scheduler.
            #
            # Default: 0
          YAML_KEY_threadPriority: <int>

            # The presence of this key indicates
            # that RSSI shall be used. Its absence
            # that no RSSI is to be configured.
        YAML_KEY_RSSI:

            # Priority of the RSSI thread. A number
            # bigger than zero must be a valid pthread
            # priority and tries to engage a real-time
            # scheduler. If this fails, CPSW falls back
            # to the default scheduler.
            #
            # Default: 0
          YAML_KEY_threadPriority: <int>

            # The presence of this key indicates
            # that 'depack' shall be used. Its absence
            # that no 'depack' is to be configured.
            # Obviously, configuring any parameter
            # for the depacketizer enables it as well.
            # The depacketizer is also enabled
            # automatically if the TDESTMux module
            # is enabled.
        YAML_KEY_depack:

            # Packetizer protocol version.
            # Either DEPACKETIZER_V0 or
            #        DEPACKETIZER_V2
            #
            # Note that  DEPACKETIZER_V2 which
            # supports TDEST interleaving is actually
            # implemented by the TDESTMux module, i.e.,
            # all parameters defined for the depacketizer
            # (such as threadPriority, queue sizes etc.)
            # are ignored; the TDESTMux configuration
            # is used instead!
            #
            # Default: DEPACKETIZER_V0
          YAML_KEY_protocolVersion:<DepackProtoVersion>

            # The output queue depth can in special
            # cases be tuned - if this module
            # is at the top of the stack and the
            # application requires more buffering
            # capacity inside CPSW.
          YAML_KEY_outQueueDepth:  <int>

            # For expert use only
          YAML_KEY_ldFrameWinSize: <int>

            # For expert use only
          YAML_KEY_ldFragWinSize:  <int>

            # Priority of the depacketizer thread. A number
            # bigger than zero must be a valid pthread
            # priority and tries to engage a real-time
            # scheduler. If this fails, CPSW falls back
            # to the default scheduler.
            #
            # Default: 0
          YAML_KEY_threadPriority: <int>

            # The TDEST (De)Muxer is enabled
            # by the presence of this key.
        YAML_KEY_TDESTMux:

            # TDEST to which the child connects
            # a value between 0 and 255;
            # Defaults to 0
          YAML_KEY_TDEST:          <int>

            # Whether to strip the Packetizer header
            # and footer prior to handing data up
            # (or adding header/footer in the opposite
            # direction).
            # The default (when this key is not present)
            # does not strip the header except if SRP
            # is layered on top.
          YAML_KEY_stripHeader:    <bool>

            # The output queue depth can in special
            # cases be tuned - if this module
            # is at the top of the stack and the
            # application requires more buffering
            # capacity inside CPSW.
          YAML_KEY_outQueueDepth:  <int>

            # The input queue depth can in special
            # cases be tuned - if this module
            # is at the top of the stack and the
            # application requires more buffering
            # capacity inside CPSW.
            #
            # Note: this parameter is only relevant
            # if the TDESTMux is operating in DEPACKETIZER_V2
            # mode. It is ignored otherwise.
          YAML_KEY_inpQueueDepth:  <int>

            # Priority of the demultiplexer thread. A number
            # bigger than zero must be a valid pthread
            # priority and tries to engage a real-time
            # scheduler. If this fails, CPSW falls back
            # to the default scheduler.
            #
            # Default: 0
          YAML_KEY_threadPriority: <int>

            # The presence of the SRP module is defined
            # by the value of YAML_KEY_protocolVersion.
            # NOTE: the default is SRP_UDP_V2 which enables
            #       SRP.
        YAML_KEY_SRP:

            # SRP Protocol version. To disable set explicitly
            # to SRP_UDP_NONE.
            # Legal values:
            #  SRP_UDP_NONE   No SRP
            #  SRP_UDP_V1     (early version in network-byte order
            #                  with extra header word)
            #  SRP_UDP_V2     first version in wider use
            #  SRP_UDP_V3     Current version
            # Consult respective confluence documentation for
            # details about SRP versions.
          YAML_KEY_protocolVersion:<SRPProtoVersion>

            # How long to wait for a SRP transaction to
            # complete (in micro-seconds)
            # zero (default) picks a suitable value
          YAML_KEY_timeoutUS:      <int>

            # Whether to automatically adjust the 
            # retransmission timeout based on statistics.
            # Should be disabled if RSSI or TDESTMux
            # are being used (large frames to a different
            # TDEST going through the packetizer may stall
            # SRP)
            # Default: yes unless TDESTMux is configured
          YAML_KEY_dynTimeout:     <bool>
            
            # How many times to retry a failed SRP transaction
          YAML_KEY_retryCount:     <int>

            # The default write mode (might be overridden by
            # for individual operations if the API offers such
            # a feature). POSTED or SYNCHRONOUS (defaults to
            # POSTED).
          YAML_KEY_defaultWriteMode: <WriteMode>

            # The presence of this key enables the
            # SRP VirtualChannel (De)Muxer.
        YAML_KEY_SRPMux:

            # Virtual Channel number (see note) to use.
            # A number in the range 0..127.
            # Default: 0
          YAML_KEY_virtualChannel: <int>

            # Depth of the queue where asynchronous
            # replies are posted.
            # zero (default) picks a suitable value.
          YAML_KEY_outQueueDepth:  <int>

            # Priority of the demultiplexer thread. A number
            # bigger than zero must be a valid pthread
            # priority and tries to engage a real-time
            # scheduler. If this fails, CPSW falls back
            # to the default scheduler.
            #
            # Default: 0
          YAML_KEY_threadPriority: <int>

##### 2.8.1.1 TCP Module
A TCP protocol module is also available. It is intended to 
substitute UDP and RSSI. This module is useful when contacting
a server implemented in software (as opposed to firmware).
In software, TCP is readily available and provides a reliable
data stream without the need for RSSI.

The TCP module supports the following YAML properties:

        YAML_KEY_TCP:

            # The TCP port of the peer
            # NOTE: a port number of 0 *disables* TCP
            #
            # Default: 8192
          YAML_KEY_port:           <int>

            # Depth of the queue up to the next module
            # zero (default) picks a suitable value.
          YAML_KEY_outQueueDepth:  <int>

            # Priority of the TCP RX thread. A number
            # bigger than zero must be a valid pthread
            # priority and tries to engage a real-time
            # scheduler. If this fails, CPSW falls back
            # to the default scheduler.
            #
            # Default: 0
          YAML_KEY_threadPriority: <int>


#### 2.8.2 Protocol Multiplexing

There are two layers of protocol-multiplexers available
in the stack: messages may be routed based on TDEST
information and in addition, SRP transactions may be 
multiplexed by virtual channel:

Items in brackets [] represent protocol modules, (*)
are endpoints where children of NetIODev (itself in {})
are attached.

    {   NetIODev Class                    }
                    [       UDP       ]
                    [      RSSI       ]
                    [      depack     ]
                    [ TDEST Mux-Demux ]
                    /        |        \
                   /         |         \
            TDEST_0       TDEST_1       TDEST_2
               |             |                |
              (*)           (*)            [ SRP ]
             Field         Field              |
                                        [ SRP VC Mux]
                                         /    |    \
                                       VC0   VC1   VC2
                                        |     |     |
                                       (*)   (*)   (*)
                                   MMIODev MMIODev MMIODev
    
The diagram shows a configuration with a `TDEST` multiplexer
which has three different `TDEST`s configured. `TDEST_0`
and `TDEST_1` are used as raw (non-SRP) endpoints, e.g., to
attach a 'Stream'ing interface (implemented by 'Field').

`TDEST_2` has an SRP module layered on top, followed by
a `SRP_VC` multiplexer which has three endpoints (`VC0`, `VC1`
and `VC2`) configured.

In total, there are 5 children of the NetIODev.

The SRP VC multiplexer uses some bits in the SRP transaction
ID for routing transactions. Its operation is entirely
transparent to firmware and no extra firmware features are
used.

The benefit is the following: all SRP transactions to a single
VirtualChannel are strictly synchronous in order to guarantee
a defined order in which transactions are executed.

However, if the user has multiple independent subdevices then
these could all be attached to different VC channels and then
be accessed in parallel.

From the above it is obvious that children of NetIODev can
share parts of the protocol stack. It is necessary, however,
that the common parts are configured identically. E.g., it
is not possible to have two children talking to the same
UDP port with one using RSSI and the other child not using RSSI.

Here is the YAML file describing the above configuration:

        commonConfig: &commonConfig
          YAML_KEY_UDP:
            YAML_KEY_port: 8192
          YAML_KEY_RSSI: ~
          YAML_KEY_TDESTMux: ~

        srpConfig: &srpConfig
          YAML_KEY_MERGE: *commonConfig
          YAML_KEY_SRP:
            YAML_KEY_protocolVersion: SRP_UDP_V3
          YAML_KEY_TDESTMux:
            YAML_KEY_TDEST: 2
          YAML_KEY_SRPMux: ~

        root:
          YAML_KEY_class:   NetIODev
          YAML_KEY_children:
            stream0:
              YAML_KEY_class: Field
              YAML_KEY_at:
                YAML_KEY_MERGE: *commonConfig
                YAML_KEY_TDESTMux:
                  YAML_KEY_TDEST:  0
            stream1:
              YAML_KEY_class: Field
              YAML_KEY_at:
                YAML_KEY_MERGE: *commonConfig
                YAML_KEY_TDESTMux:
                  YAML_KEY_TDEST:  1
            mmio0:
              YAML_KEY_class: MMIODev
              YAML_KEY_size:  0x1000
              YAML_KEY_at:
                YAML_KEY_MERGE: *srpConfig
                YAML_KEY_SRPMux:
                  YAML_KEY_virtualChannel: 0
            mmio1:
              YAML_KEY_class: MMIODev
              YAML_KEY_size:  0x1000
              YAML_KEY_at:
                YAML_KEY_MERGE: *srpConfig
                YAML_KEY_SRPMux:
                  YAML_KEY_virtualChannel: 1

            ... (mmio2 not shownN)
