# CPSW "Configuration" Data

## Copyright Notice
This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
file found in the top-level directory of this distribution and
[at](https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html).

No part of CPSW, including this file, may be copied, modified, propagated, or
distributed except according to the terms contained in the LICENSE.txt file.

## Serializing Configuration Data

The current run-time state of a CPSW hierarchy or sub-hierarchy can be
saved to a serialized YAML file and reloaded from such a file.

Note that the format of the YAML configuration files is *different* from
the YAML which is used to define a CPSW device hierarchy.

This README documents the YAML configuration file format.

The YAML format was chosen in a way that is simple and yet enforces
a strict order in which configuration items are processed. It therefore
only contains sequences and maps with a single entry.

Entries in the CPSW hierarchy are identified by sequences of single-entry
maps. The keys of these maps are CPSW path segments and the values are
again sequences of single-entry maps.

This structure defines an order which can be traversed in a defined
way. E.g.:

        - a:
          - b:
          - c:
            - d:
          - e:
        - f:

is processed in the order


        a
        a/b
        a/c
        a/c/d
        a/e
        f

i.e., indented path elements are appended to higher-level elements. Higher-level
nodes are visited before recursing to a deeper level.

Note that individual path elements may contain multiple levels of a CPSW path.

The same sequence could have been coded in YAML, e.g., as:

        - a:
        - a/b:
          - c:
          - c/d:
          - e:
        - f:
 
In some cases it may be desirable to 'collapse' multiple path elements into
a single YAML key. Note that YAML anchors and references work as expected.

HOWEVER: YAML merge key notation is not supported as this would violate the
'single-key' rule. Also, due to the extensive use of sequences merging 
(which is most useful in the context of maps) is not very attractive.

The presence of actual configuration data/values is indicated by a 

        !<value> 

YAML tag. Thus, if the above leaf nodes `a/b`, `a/c/d`, `a/e` and `a/f` could have
properties attached to them which store the actual configuration values:

        - a:
          - b: !<value>
              key:       mapped value
              otherkey:  other mapped value
          - c:
            - d: !<value>  scalar value
          - e: !<value>
            -            sequenced value 1
            -            sequenced value 2
            -            sequenced value 3
        - f:  !<value>
           key: value

The YAML value-tagged nodes are interpreted by the CPSW objects which are 
present in the CPSW hierarchy at the location identified by the path
constructed as described above.

### Array Handling

CPSW (multi-dimensional) arrays are fully supported and work as expected.
Here is an example assuming a hierarchy of a `top` container which holds
an array of two subdevices which hold arrays of 4 registers each:

A CPSW path which covers all registers in all subdevices is (in explicit notation):

        top/subdev[0-1]/register[0-3]

this is identical to `top/subdev/register` since omitted indices imply
the full range.

Thus, all registers may be written by the following config file:

        - top/subdev/register: !<value>
              - subdev0_reg0_value
              - subdev0_reg1_value
              - subdev0_reg2_value
              - subdev0_reg3_value
              - subdev1_reg0_value
              - subdev1_reg1_value
              - subdev1_reg2_value
              - subdev1_reg3_value

It is important to realize that the sequence of values is *not* part of
the sequential processing as guaranteed by the loading procedure.
As explained above, the entire node tagged `!<value>` is passed to the
underlying object. While the sequence defines the exact association
between elements in the sequence and corresponding registers there is
no timely relation defined. This is because - for efficiency reasons -
the entire array is passed through all the transport layers and
caches etc. and there is no defined (timely) order in which elements of an
array written to the destination hardware.

If it is e.g., of importance that reg3 is written *after* all the other
registers are written then this could be described by a different
configuration file:

        - top/subdev:
          - reg[0-2]: !<value>
              - subdev0_reg0_value
              - subdev0_reg1_value
              - subdev0_reg2_value
              - subdev1_reg0_value
              - subdev1_reg1_value
              - subdev1_reg2_value
          - reg[3]: !<value>
              - subdev0_reg3_value
              - subdev1_reg3_value

Now CPSW ensures that registers 0..2 are written first and registers 3
only afterwards.

## Saving Configurations

An existing configuration file may be used as a 'program' or 'template'
for dumping a current configuration. The CPSW entries in the hierarchy
are visited in the order as defined by the 'template' file, current
values are retrieved and stored to a new configuration file.

Alternatively, a configuration may be dumped without a 'template'.
In this case the ``configPrio`` property of each entry in the hierarchy
is relevant:

 1. Entries with a `configPrio` or zero are ignored, i.e., their configuration
        is not dumped.
 2. Children of a container are dumped in increasing order of `configPrio`, 
        i.e., children with `configPrio == -1` are dumped prior to dumping `configPrio == +1`.
        Children with `configPrio == 0` (and all of their descendants) are ignored.

There is no defined order among children with the same `configPrio`.

Thus, the `configPrio` helps creating a configuration 'template' for the
first time. The `configPrio` is defined (usually by the firmware engineer) in the YAML 
file which is used for building the hierarchy (i.e., NOT in the configuration
file itself). It is a property of each object in the CPSW hierarchy.
        
The default `configPrio` is specific to each CPSW class:

 - +1 for IntFields (scalar values) in read/write mode
 - +1 for containers and subclasses thereof
 -  0/zero for other elements (including commands and read-only IntFields)

Because the `configPrio` is only relevant when a 'template' file is not used
the default priority can be overruled by listing the desired element
in a template file. Thus, if a particular command is to be executed when
a configuration is loaded or a read-only value should be saved then the
corresponding elements can simply be listed in a config file (a read-only
element can never loaded back, however).

## Configuration of Container Devices

So far the assumption has been that only leaf elements save/restore configuration
data.

It shall be shown here that the file format (and the code, actually) also
supports properties of a container device:

        - top:
          - container: !<value>
                 - mykey: something for the container proper
          - container:
              - child: !<value>
                   key: something for the child
                 
Even though `container` may have children there is indeed a sequence
with a single-entry map '`- mykey:`' the presence of the `!<value> tag`
makes it clear that no recursion into `mykey` should be attempted.

Any desired ordering of such properties can obviously be achieved.

No currently implemented container makes use of this feature yet but it
is very likely that it is required to support e.g., drivers which
are derived from MMIO etc.
